# Phase 1 Data Model: Mockingboard C — Sound/Speech

**Feature**: `024-mockingboard-speech` | **Issue**: #123 | **Date**: 2026-08-24

Entities are described by the state they own and the rules that govern it. Field
names marked **PENDING-1** or **PENDING-2** depend on primary-source facts that
research.md deliberately left open; their *presence* is settled, their *encoding*
is not.

---

## Ssi263 — the voice chip

The new core, peer to `Ay8910` in `CassoEmuCore/Devices/Mockingboard/`.

### State

| Group | Owns | Notes |
|---|---|---|
| Register file | The written control values | Count and packing **PENDING-1** |
| Current phoneme | Which phoneme is sounding, and how far through it | Advanced by emulated cycles (D7) |
| Transition | Interpolation position between the previous phoneme's formant targets and the current one's | The chip's transition control governs rate |
| Synthesis | Excitation phase, resonator filter state | Formant bank per D5 |
| Request | Whether the chip is ready for the next phoneme | Readable as status; also drives a VIA control line (**PENDING-2**) |
| Rendering | Host sample rate | Set like `Ay8910::SetSampleRate`; never affects emulated timing |

### Rules

- **R1 — Quiescent at construction and reset.** After `Reset()`, and before any
  register write, the chip produces exactly zero output and does not assert
  request. This is FR-015 and FR-016, and it is the single most important
  invariant in the feature: it is what makes the C safe as a default.
- **R2 — Silence is a property, not a coincidence.** An idle chip reports itself
  silent so the audio path can skip synthesis entirely (FR-020), in the manner of
  `Ay8910::IsSilent`.
- **R3 — Reset is immediate.** A reset mid-utterance abandons the current phoneme
  rather than finishing it (FR-021).
- **R4 — Emulated time is authoritative.** Phoneme duration and transition
  advance on emulated cycles; host sample rate affects only how many samples that
  span is rendered into (FR-005).
- **R5 — Deterministic.** Two identically-programmed chips render identical
  streams, as `Ay8910`'s `OutputIsDeterministic` already requires of the PSG.
- **R6 — Bounded output.** Every reachable combination of control values yields
  output within the sample range, including combinations no real software writes
  (an Edge Case in the spec).

### State transitions

```text
                  ┌──────────────────────────────────────┐
                  │                                      │
                  ▼                                      │
        ┌───────────────────┐                            │
        │       IDLE        │  no audio, no request      │
        │  (reset state)    │                            │
        └─────────┬─────────┘                            │
                  │ software writes a phoneme            │
                  ▼                                      │
        ┌───────────────────┐                            │
        │    SOUNDING       │  rendering; request low    │
        │  phoneme + dur.   │                            │
        └─────────┬─────────┘                            │
                  │ duration elapses (emulated cycles)   │
                  ▼                                      │
        ┌───────────────────┐                            │
        │     REQUEST       │  ready for next phoneme;   │
        │                   │  request asserted → VIA    │
        └────┬─────────┬────┘                            │
             │         │ no write arrives                │
             │         └─────────────────────────────────┘
             │ software writes the next phoneme            (returns to IDLE
             ▼                                              per PENDING-1's
        (SOUNDING, transitioning from the previous)         idle/pause encoding)

    Reset from ANY state ──────────────────────────────▶ IDLE (R3, immediate)
```

The **REQUEST → IDLE** edge is what makes R1 hold for software that speaks once
and stops: a chip left waiting must not sit asserting an interrupt forever. Its
exact form depends on PENDING-1, since the register model itself encodes how a
final phoneme is terminated.

---

## MockingboardVariant

Which model of card is installed. Two values: sound-only (Mockingboard A) and
sound+speech (Mockingboard C).

- Chosen at card construction and immutable thereafter — changing it is a
  machine-configuration change requiring a reset (FR-012), not a runtime toggle.
- Surfaced to the Hardware tab as a display name (FR-013).
- Reaches the card through two registered type names, not a config field (D1).

---

## MockingboardCard (extended)

Keeps its two VIAs, two PSGs, and two PSG audio sources. Gains:

| Adds | Present when |
|---|---|
| The variant it was constructed with | Always |
| An `Ssi263` | Sound+speech only |
| A speech audio source | Sound+speech only |

### Rules

- **R7 — The A is unchanged.** On the sound-only variant, address decoding,
  interrupt sources, and audio output are exactly what ships today, by executing
  the same path rather than an equivalent one (D3, FR-007).
- **R8 — The chip is a tap, not a replacement** *(revised with T002/F3)*. On the
  sound+speech variant, a write in a populated speech range reaches **both** the
  existing VIA path — executed unchanged — and the voice chip; a read there
  returns the chip's status on D7. Empty sockets leave their ranges exactly as
  the sound-only card. The speech ranges are `$20`–`$2F` (socket 0, empty on the
  C) and `$40`–`$4F`/`$60`–`$6F` (chip 1): A4 clear, A5|A6 set, A7 clear.
- **R9 — Detection is unaffected.** Whatever a detection routine probes on the A,
  it sees on the C (FR-017).
- **R10 — Card reset resets the chip**, alongside the PSGs.

---

## Via6522 (extended) — control-line input

The seam finding F1 requires. The VIA today defines `kIrqCa1`/`kIrqCb1` and
stores `PCR`, but nothing drives or reads them.

### Adds

| Adds | Purpose |
|---|---|
| Control-line input state (CA1/CB1 level) | What an external device drives |
| Edge detection governed by `PCR` | Which transition sets the IFR bit |

### Rules

- **R11 — PCR selects the active edge.** The stored control register stops being
  inert and governs which transition latches the interrupt flag.
- **R12 — The flag clears as the datasheet specifies**, so software that
  acknowledges normally does not see a stuck interrupt.
- **R13 — No control-line input, no behavior change.** A VIA whose control lines
  are never driven behaves exactly as it does today. This is what keeps the
  sound-only card (and every existing VIA test) unaffected by the new seam.

**R13 is the compatibility hinge for the whole feature.** The A variant never
drives a control line, so it cannot acquire an interrupt source it did not have —
which is FR-016 discharged structurally rather than by testing.

---

## Speech audio source

An `IDriveAudioSource` adapter for the voice chip, parallel to
`MockingboardAudioSource` rather than a modification of it (D4, F2).

- Panned center; the board's speech output is mono and belongs to neither PSG.
- Contributes exactly zero while the chip is idle (R2 → FR-020).
- Participates in the master volume and mute path like every other source
  (FR-018), and in the recomputed gain budget (F2 → SC-008).

---

## Entity relationships

```text
  MachineConfig (slots[].device)
        │  one of two registered type names (D1)
        ▼
  MockingboardCard ──── variant ────▶ MockingboardVariant
        │
        ├── Via6522   × 2 ──── control-line input (F1) ◀── request
        ├── Ay8910    × 2                                     │
        ├── MockingboardAudioSource × 2 (hard L / hard R)     │
        │                                                     │
        └── Ssi263 ───────────────────────────────────────────┘
                 │            (sound+speech variant only)
                 └── speech audio source (center)
                                │
                                ▼
                      Mockingboard mixer ──▶ master volume / mute ──▶ output
```
