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

| Addr | Register | Fields |
|---|---|---|
| 0 | Duration / Phoneme | `DR1,DR0` (bits 7:6) relative phoneme duration; `P5–P0` (bits 5:0) phoneme select |
| 1 | Inflection | `I11–I0` inflection target frequency / immediate pitch |
| 2 | Rate / Inflection | `R3–R0` speech rate; remaining inflection bits |
| 3 | Control / Articulation / Amplitude | `CTL` control bit; `T2–T0` articulation (formant movement rate); `A3–A0` audio amplitude |
| 4 | Filter Frequency | `FF7–FF0` switched-capacitor vocal-tract filter clock |

> **Verify before coding**: the datasheet's *Register Input Formats* table is a
> graphic and did not survive OCR. The **field names and their meanings above are
> certain**; the exact bit positions of the inflection fields split across
> registers 1 and 2 are **not yet recovered** and must be read off the PDF page
> image. Everything else in this section is from running text and is reliable.

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

| | | | | | | |
|---|---|---|---|---|---|---|
| `00` PA *(pause)* | `01` E | `02` EI | `03` Y | `04` IY | `05` AY | `06` IE |
| `07` I | `08` A | `09` AI | `0A` EH | `0B` EH1 | `0C` AE | `0D` AE1 |
| `0E` AH | `0F` AH1 | `10` AW | `11` O | `12` OU | `13` OO | `14` IU |
| `15` I1 | `16` U | `17` U1 | `18` UH | `19` UH1 | `1A` UH2 | `1B` UH3 |
| `1C` ER | `1D` R | `1E` R1 | `1F` R2 | `20` L | `21` L1 | `22` LF |
| `23` W | `24` B | `25` D | `26` KV | `27` P | `28` T | `29` K |
| `2A` HV *(hold vocal)* | `2B` HVC | `2C` HF | `2D` HFC | `2E` HN | `2F` Z | `30` S |
| `31` J | `32` SCH | `33` V | `34` F | `35` THV | `36` TH | `37` M |
| `38` N | `39` NG | `3A` — | `3B` — | `3C` — | `3D` — | `3E` E2 |
| `3F` LB | | | | | | |

Codes `$3A`–`$3D` are non-English phonemes (German and French examples in the
chart) whose symbols OCR'd unreliably; read them off the PDF before use. A few
others were corrected from obvious OCR damage — `04` and `17` in particular —
and the whole chart should be checked against the page image before it becomes
a table in code.

Note `$2A`–`$2E` are *hold* states (vocal, vocal closure, fricative closure,
nasal) rather than sounds, which matters for the state machine: they sustain
rather than articulate.

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
