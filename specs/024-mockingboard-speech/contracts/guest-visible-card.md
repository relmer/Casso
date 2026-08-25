# Contract: What guest software sees

**Feature**: `024-mockingboard-speech` | **Issue**: #123

The consumer of this contract is 6502 code running inside the emulator. It is the
contract that matters most in the feature, because every compatibility
requirement (FR-007, FR-014, FR-016, FR-017) is a statement about it.

Entries marked **PENDING-2** await the board schematics. **PENDING-1 is resolved**
— the chip's register interface below is transcribed from its datasheet (T001),
with two residual gaps flagged inline where OCR could not recover a graphic.

---

## Slot I/O page, sound-only variant (Mockingboard A)

**Frozen. This table describes what ships today and MUST NOT change.**

| Address within `$Cn00` | Reaches |
|---|---|
| bit 7 clear, low 4 bits select register | VIA #1 / PSG #1 |
| bit 7 set, low 4 bits select register | VIA #2 / PSG #2 |

Address lines A4–A6 are not decoded, so each VIA's 16-register file mirrors
throughout its half of the page. `$Cn40` therefore reads as VIA #1's register 0.

**This mirroring is part of the contract for this variant.** It is not an
implementation artifact to be tidied up: it is what today's software observes,
and FR-007 freezes it.

## Slot I/O page, sound+speech variant (Mockingboard C)

Identical to the above **except** that the region the board decodes to the voice
chip (**PENDING-2**) reaches the chip instead of a VIA mirror.

| Address within `$Cn00` | Reaches |
|---|---|
| The speech region (**PENDING-2**) | Voice chip registers |
| Everything else | Exactly as the sound-only variant |

**The removed mirror is the intended, faithful difference between the two cards**
and the first named compatibility risk in the spec. It is also precisely why the
A variant remains available (User Story 3).

---

## Voice chip register interface

Transcribed from the SSI-263A datasheet (T001). Five eight-bit registers,
selected by a three-bit address `RS2–RS0`.

Read directly off the datasheet's *Register Input Formats* table (page image, not
OCR). `X` = don't care.

| RS2 | RS1 | RS0 | Register | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| LO | LO | LO | Duration/Phoneme (DR/P) | `DR1` | `DR0` | `P5` | `P4` | `P3` | `P2` | `P1` | `P0` |
| LO | LO | HI | Inflection (I) | `I10` | `I9` | `I8` | `I7` | `I6` | `I5` | `I4` | `I3` |
| LO | HI | LO | Rate/Inflection (R/I) | `R3` | `R2` | `R1` | `R0` | `I11` | `I2` | `I1` | `I0` |
| LO | HI | HI | Control/Articulation/Amplitude (C/A/A) | `CTL` | `T2` | `T1` | `T0` | `A3` | `A2` | `A1` | `A0` |
| HI | X | X | Filter Frequency (F) | `F7` | `F6` | `F5` | `F4` | `F3` | `F2` | `F1` | `F0` |

**Two decode consequences that are easy to get wrong:**

1. **The 12-bit inflection value is scattered across two registers, non-contiguously.**
   `I11` sits alone at register 2 bit 3, `I10–I3` occupy all of register 1, and
   `I2–I0` occupy register 2 bits 2–0. Reassembly is therefore:

   ```text
   I = (reg2 & 0x08) << 8   |   reg1 << 3   |   (reg2 & 0x07)
       ^ I11 -> bit 11          ^ I10..I3       ^ I2..I0
   ```

2. **`RS2` high selects Filter Frequency regardless of `RS1`/`RS0`.** Register
   addresses 4, 5, 6, and 7 all alias to the same filter register — there are
   five registers in eight address slots.

Field meanings, from the same page: `DR1,DR0` phoneme duration · `P5–P0` phoneme
select · `I11–I0` inflection target frequency and rate of change · `R3–R0` rate
of speech · `CTL` A/R response mode, in conjunction with `DR1`/`DR0`, also set
directly by `PD/RST` · `T2–T0` rate of movement of formant position for
articulation · `A3–A0` output audio amplitude · `F7–F0` frequency of all vocal
tract filters.

Typical settings the datasheet names: rate `$A`, articulation `5`, amplitude
`$C`, inflection for ~90 Hz, filter clock ~20 kHz.

### Relevant pin behavior

| Pin | | |
|---|---|---|
| 4 | `A/R` | Acknowledge/Request Not — open-collector, changes **high → low after the phoneme is generated**; may be used as an interrupt request for new phoneme data |
| 17 | `D7` | MSB of the bus. **Bidirectional — the inverse of pin 4 when read is high** |
| 18 | `PD/RST` (active low) | Power Down — silences audio and retains DC bias without disturbing register contents. **Disables A/R output** |
| 21 | `R/W` | Write active low to load registers. **Read active high but enables `D7` only** |
| 22 | `XCK` | Clock input, ≈1 or 2 MHz |
| 23 | `DIV2` (active high) | Divide external clock by two, for a ≈2 MHz input |

### Published formulas

All are relative to `XCK`, the external clock (800 kHz–1 MHz nominal with `DIV2`
low, twice that with `DIV2` high; the datasheet suggests a 3.5795 MHz colorburst
crystal divided by two).

```text
Frame Duration      = 4096 × (16 − R) / XCK          R = Rate register value
Phoneme Duration    = Frame Duration × (4 − D)       D = Duration register value
Inflection Frequency = XCK / (8 × (4096 − I))        I = Inflection register value
Filter Frequency    = XCK / (2 × (256 − FF))         FF = Filter register value
```

Typical values the datasheet names: rate `$A`, articulation `5`, inflection for
~90 Hz, filter clock ~20 kHz.

### Mode selection — how A/R is armed and disabled

Modes latch on a `CTL` **1 → 0 transition**, taking the operating mode from the
`DR1`/`DR0` bits at that moment:

| DR1 | DR0 | Mode |
|---|---|---|
| HI | HI | A/R active; phoneme timing; transitioned inflection — *the most commonly used mode* |
| HI | LO | A/R active; phoneme timing; immediate inflection |
| LO | HI | A/R active; frame timing; immediate inflection |
| LO | LO | **Disables the A/R output only**; leaves the previous A/R response otherwise unchanged |

`CTL` = 1 puts the device in **Power Down**: excitation sources and analog
circuits off, register contents retained, **A/R output disabled**. It is set high
by `PD/RST` going low **and on power-up**.

**This discharges FR-015 directly.** Quiescence-until-programmed is not emulator
policy invented for safety — the real part powers up with `CTL` set, silent and
not requesting, and stays that way until software drives `CTL` low. Model that
and the requirement falls out.

### Reading the chip

`D7` becomes an output carrying the **inverted state of A/R** when the device is
read (`R/W` high, `CS1` = 0, `CS0` = 1). **The register address bits are ignored
on read** — every address in the chip's range returns the same status bit. Only
`D7` is driven.

### A/R behavior

Open-collector output. Transitions **high → low once the phoneme has been
generated**, and is documented as usable as an interrupt request for new phoneme
data. The datasheet notes that several milliseconds may elapse between request
and load with no detectable degradation in speech quality — useful latitude for
the emulated timing.

### Phoneme chart — 64 codes

`$00`–`$3F` via `P5–P0`. Datasheet hex codes are shown with the duration bits
zero, so they are the phoneme numbers directly.

Transcribed from the page image. Example words in parentheses.

| | | | |
|---|---|---|---|
| `00` PA *(pause)* | `10` AW *(office)* | `20` L *(lift)* | `30` S *(same)* |
| `01` E *(meet)* | `11` O *(store)* | `21` L1 *(play)* | `31` J *(measure)* |
| `02` E1 *(bent)* | `12` OU *(boat)* | `22` LF *(fall, final)* | `32` SCH *(ship)* |
| `03` Y *(before)* | `13` OO *(look)* | `23` W *(water)* | `33` V *(very)* |
| `04` YI *(year)* | `14` IU *(you)* | `24` B *(bag)* | `34` F *(four)* |
| `05` AY *(please)* | `15` IU1 *(could)* | `25` D *(paid)* | `35` THV *(there)* |
| `06` IE *(any)* | `16` U *(tune)* | `26` KV *(tag, glottal stop)* | `36` TH *(with)* |
| `07` I *(six)* | `17` U1 *(cartoon)* | `27` P *(pen)* | `37` M *(more)* |
| `08` A *(made)* | `18` UH *(wonder)* | `28` T *(tart)* | `38` N *(nine)* |
| `09` AI *(care)* | `19` UH1 *(love)* | `29` K *(kit)* | `39` NG *(rang)* |
| `0A` EH *(nest)* | `1A` UH2 *(what)* | `2A` HV *(hold vocal)* | `3A` :A *(märchen, German)* |
| `0B` EH1 *(belt)* | `1B` UH3 *(nut)* | `2B` HVC *(hold vocal closure)* | `3B` :OH *(löwe, French)* |
| `0C` AE *(dad)* | `1C` ER *(bird)* | `2C` HF *(heart)* | `3C` :U *(fünf, German)* |
| `0D` AE1 *(after)* | `1D` R *(roof)* | `2D` HFC *(hold fricative closure)* | `3D` :UH *(menu, French)* |
| `0E` AH *(got)* | `1E` R1 *(rug)* | `2E` HN *(hold nasal)* | `3E` E2 *(bitte, German)* |
| `0F` AH1 *(father)* | `1F` R2 *(mutter, German)* | `2F` Z *(zero)* | `3F` LB *(lube)* |

`$2A`–`$2E` are **hold** states — vocal, vocal closure, fricative closure, nasal —
rather than sounds. They sustain rather than articulate, which the state machine
must treat differently from an ordinary phoneme.

### Behavioral guarantees, independent of encoding

These hold regardless of what the datasheet says, and are testable as written:

- **G1** — Before any write, reads return the quiescent state and the chip
  asserts no interrupt. *(FR-015)*
- **G2** — Writing a phoneme begins audible output within the same emulated
  instruction stream; software need not enable interrupts or poll to get sound.
  *(Spec edge case: programs speech but never polls)*
- **G3** — The ready indication is observable both by polling the status read and
  as an interrupt, and both report the same underlying state. *(FR-004)*
- **G4** — A phoneme occupies a fixed number of emulated cycles for a given rate
  and duration, independent of host sample rate or emulator speed. *(FR-005)*
- **G5** — Reset returns the chip to G1 from any state, immediately, abandoning
  any phoneme in progress. *(FR-021)*

---

## Interrupt contract

The chip's request output drives a VIA control line (**PENDING-2**: which line,
which VIA), latching an interrupt flag that software reads through the VIA's
interrupt registers.

| Guarantee | |
|---|---|
| **I1** | On the sound-only variant, no control line is ever driven, so no control-line interrupt can occur — the card's only interrupt sources remain its timers. *(FR-016)* |
| **I2** | On the sound+speech variant with the chip unprogrammed, likewise: no control-line interrupt occurs. *(FR-015, SC-005)* |
| **I3** | Software that enables only timer interrupts is never vectored by the voice chip, regardless of chip activity. |
| **I4** | Acknowledging the interrupt in the documented way clears it; it does not re-latch spuriously. |

**I3 is the guarantee that protects existing music players.** A player that
dispatches on "the card interrupted" without testing which source is the failure
mode the spec's Overview names, and I3 is what makes it a non-event.

---

## Detection contract

Whatever sequence identifies a Mockingboard on the sound-only card identifies it
identically on the sound+speech card (FR-017). The C is a superset: it answers
everything the A answers, in the same way.

---

## Test surface

Every guarantee above is stated to be checkable without a listener:

| Guarantee | Verified by |
|---|---|
| A-variant page map frozen | Read/write sweep of the full page on both variants; identical outside the speech region |
| G1, G2, G5, I1, I2 | Direct chip and card unit tests |
| G3 | Poll-versus-interrupt agreement test |
| G4 | Same utterance at two host sample rates and two emulator speeds; equal emulated-cycle span |
| I3 | Timer-only interrupt enable, chip driven hard, assert no vectoring (SC-005) |
| I4 | Acknowledge-and-observe test |
| Detection | Detection sequence replayed against both variants |
