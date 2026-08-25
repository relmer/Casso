# Contract: What guest software sees

**Feature**: `024-mockingboard-speech` | **Issue**: #123

The consumer of this contract is 6502 code running inside the emulator. It is the
contract that matters most in the feature, because every compatibility
requirement (FR-007, FR-014, FR-016, FR-017) is a statement about it.

Entries marked **PENDING-2** await the board schematics; **PENDING-1** awaits the
chip datasheet. Their presence is fixed, their addresses and encodings are not.

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

## Voice chip register interface (**PENDING-1**)

Shape that can be relied on for structuring code; encodings come from the
datasheet.

| Function | Direction | Notes |
|---|---|---|
| Phoneme + duration | Write | Selects what sounds and for how long |
| Inflection | Write | Pitch contour |
| Rate | Write | Speaking rate |
| Amplitude / transition | Write | Loudness, and how formants glide between phonemes |
| Filter frequency | Write | Spectral shaping |
| Request / ready status | Read | Appears as a high-order bit |

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
