# Phase 0 Research: Mockingboard C — Sound/Speech

**Feature**: `024-mockingboard-speech` | **Issue**: #123 | **Date**: 2026-08-24

This document records the design decisions that can be made now, and isolates the
facts that must come from primary hardware documentation before code is written.
The distinction matters more than usual here: a plausible-looking register map
written from memory would be indistinguishable from a correct one until software
failed against it, so anything datasheet-derived is marked **PENDING** rather
than guessed.

---

## Codebase findings

Two properties of the existing card determine most of the work.

### F1 — The VIA has no control-line input, and PCR is inert

`Via6522` defines the IFR bit assignments for the control lines
(`kIrqCa1`, `kIrqCa2`, `kIrqCb1`, `kIrqCb2`), and `WriteRegister` stores `PCR`,
but **nothing ever acts on either**. There is no `SetCa1`-style entry point, no
edge detection, and no path by which an external device can raise a control-line
interrupt. Port I/O is the only external seam (`SetPortAInput` / `SetPortBInput`).

This is the single largest piece of foundational work in the feature. The voice
chip's ready/request line reaches software *through* a VIA control line, so the
chip cannot signal anything until that seam exists.

It is also reusable work rather than speech-specific plumbing: control-line
handshaking is how the 6522 talks to any peripheral that needs it, so this
benefits future cards built on the same VIA.

### F2 — The audio source adapter is PSG-specific, and the gain budget is sized for two sources

`MockingboardAudioSource` binds to an `Ay8910` concretely (`SetPsg`), so it
cannot carry speech as-is. It implements `IDriveAudioSource`, which is the actual
contract the mixer consumes — a parallel adapter satisfies the same interface
without disturbing the existing one.

Its `kMasterGain = 0.28f` carries a comment stating the value exists because
"two PSGs (three channels each) sum into the stereo bus alongside the speaker and
Disk II audio". A third source changes that arithmetic, and SC-008 (no clipping)
depends on it being revisited rather than inherited.

---

## Decisions

### D1 — Two registered device types, not a variant field on `DeviceConfig`

**Decision**: Register a second card type for the sound+speech model, alongside
the existing sound-only one. Do not add a `variant` field to `DeviceConfig`.

**Rationale**: `DeviceConfig` carries `type`, `slot`, `hasSlot` and nothing else,
and `ComponentRegistry` maps a type name to a factory. Two type names need no
schema change anywhere — not in the config struct, not in the JSON reader, not in
the registry. It is also the *better* shape for GH #124: a per-slot dropdown
listing card types presents "Mockingboard A" and "Mockingboard C" as two ordinary
entries, where a type-plus-variant tuple would need special-case UI.

**Alternatives considered**: A `variant` field would generalize to the Phasor's
mode switch later. Rejected on Constitution V (YAGNI) — the Phasor is explicitly
out of scope, and a second type name is not an obstacle to adding one if it ever
arrives.

### D2 — One card class carrying a variant, two factories

**Decision**: `MockingboardCard` keeps its identity and gains a variant it is
constructed with. Both registered types resolve to it. The speech chip is a
member present only on the sound+speech variant.

**Rationale**: The two cards share their entire sound half — VIAs, PSGs, port
translation, IRQ registration. A subclass or a duplicated class would fork all of
that to add one member. A variant enum keeps the decode in one function where the
A-path and C-path differences are visible side by side, which is exactly what
FR-007 needs to be auditable.

### D3 — The A variant's decode is the fall-through, not a parallel path

**Decision**: On the sound+speech variant, the speech-chip region is intercepted
first and everything else falls through to the existing decode unchanged. The
sound-only variant runs the existing decode with no interception at all.

**Rationale**: FR-007 requires the A to be byte-for-byte what ships today. The
strongest way to guarantee that is for the A to execute the *same code path* it
executes now, with the new behavior added strictly as a prefix on the other
variant. A reviewer can then confirm compatibility by inspection rather than by
exhaustive test.

### D4 — A third audio source, centered

**Decision**: The voice chip gets its own `IDriveAudioSource` adapter, panned
center, registered with the Mockingboard mixer beside the two PSG sources.

**Rationale**: The board's speech output is a single mono signal, not part of
either PSG's channel; hard-panning it would be wrong, and folding it into a PSG
source would couple two independently-silenceable things. A third source also
keeps FR-020 (exactly zero when idle) expressible as a property of one object.

**Consequence**: The gain budget in F2 must be recomputed for three sources.

### D5 — Formant synthesis, with parameters from the datasheet

**Decision**: Model the chip as what it is — a formant synthesizer: an excitation
source (voiced pulse train / unvoiced noise) driving a bank of resonators whose
center frequencies and bandwidths are set per phoneme and interpolated across the
transition into the next one.

**Rationale**: This is the only model that reproduces the chip's actual behavior
under the controls software manipulates. A sampled-phoneme lookup would produce
recognizable speech but would not respond correctly to the inflection and rate
registers, failing FR-003 and SC-001a.

**Constraint**: The per-phoneme parameter tables are the datasheet-derived part.
See PENDING-1.

### D6 — Verify formants by spectral assertion, not by golden audio

**Decision**: SC-001a is satisfied by rendering a phoneme and asserting the
output's energy is concentrated at the expected formant frequencies — a Goertzel
evaluation at the target bins, comparing in-band against out-of-band energy.

**Rationale**: Needs no audio fixtures, no external dependency, and no listener;
it is deterministic and fails loudly when a parameter table is wrong. It follows
the precedent the PSG tests already set — `ToneFrequencyMatchesDatasheetFormula`
asserts a frequency relationship rather than comparing a waveform, and
`OutputIsDeterministic` guards reproducibility separately.

**Alternatives considered**: Golden PCM files (brittle across any resampling or
gain change, and large in-tree); automated speech recognition scoring (an ASR
engine's behavior on 1982-vintage formant synthesis is itself unvalidated — it
would be measuring the recognizer, not the chip).

### D7 — Timing is driven by emulated cycles, sample rate is a rendering detail

**Decision**: Phoneme duration and transition advance on emulated machine time;
the audio path asks the chip for samples at the host rate, exactly as the PSG
works today.

**Rationale**: FR-005 requires an utterance to occupy the same emulated cycles
regardless of host sample rate or emulator speed. `Ay8910` already establishes
this split (`SetSampleRate` plus cycle-driven state), so the speech chip should
not invent a second timing convention.

### D8 — First light is a repo-original boot-sector program

**Decision**: Author `Apple2/Demos/mockingboard-speech-test.a65` early, following
the structure of the existing `mockingboard-test.a65` tone test: unconditionally
program the voice chip with a fixed phoneme sequence, then spin.

**Rationale**: It carries its own ground truth (we chose the phonemes), needs no
disk acquisition, and isolates host-side faults from title-side faults exactly as
its tone counterpart does — that file's header already articulates the pattern:
a steady tone proves the path end-to-end, silence localizes the fault to the
host. It is the artifact that unblocks all development on User Story 1.

---

## PENDING — must come from primary sources before implementation

These are deliberately not answered here. Each is a fact about physical hardware,
and a confident-sounding guess would be worse than an open question because it
would survive review.

### PENDING-1 — The chip's register model and phoneme parameter tables

**Needed**: The register file (count, ordering, and the field packing within
each), the phoneme code set, the encoding of the duration, inflection, rate,
amplitude/transition, and filter-frequency controls, and the formant parameters
per phoneme.

**Shape we can rely on** (sufficient for structuring the code, not for writing
the tables): a small write-mostly register file selecting a phoneme together with
a duration, plus separate controls for inflection, rate, amplitude/transition,
and filter frequency; and a readable status in which the request/ready state
appears as a high-order bit.

**Primary source**: the SSI-263 datasheet (Silicon Systems). **This is the
single highest-value input to the whole feature** — SC-001a is only as good as
the document behind it, and D5's parameter tables come from nowhere else.

**Acceptance for this item**: the datasheet is in hand and its formant/duration
tables are transcribed into the plan's contracts before chip code is written.

### PENDING-2 — Where the chip is decoded in the slot page, and how its request line is wired

**Needed**: The address range within `$Cn00` that the board decodes to the speech
chip, and which VIA control line (and which VIA) the request output drives.

**Why it cannot be inferred**: This is a property of the *board*, not the chip —
the same chip on a different card would be wired differently. It determines
exactly which mirror the C removes (the Overview's first named compatibility
risk) and which IFR bit software sees.

**Primary source**: Sweet Micro Systems Mockingboard schematics (Apple II
Documentation Project).

### PENDING-3 — The reference rendering for SC-001b

**Needed**: One rendering of a known phoneme sequence, not produced by Casso, to
compare intelligibility against.

**Resolution path**: A real-hardware recording is preferable. Failing that,
rendering the same authored sequence through another emulator is behavioral
comparison, not derivation, and stays inside FR-002's clean-room rule — no
third-party source is read or copied. Blocks SC-001b only; blocks no code.

### PENDING-4 — The speech acceptance set

**Needed**: Period titles that drive Mockingboard speech, with obtainable images.

**Guidance**: Prefer the manufacturer's own demo and utility software, which was
written to exhibit the chip and often speaks a documented phrase — supplying a
transcript that game software does not. Candidate game titles should be verified
against a Mockingboard software list rather than recalled; whether a given title
uses speech or only music is exactly the kind of detail that is misremembered.

**Blocks**: SC-001c and User Story 1 sign-off only. Development proceeds without
it via D8.

---

## Research summary

| # | Item | Status |
|---|---|---|
| F1 | VIA control-line seam absent; PCR inert | Confirmed in code |
| F2 | Audio adapter is PSG-bound; gain budget sized for two | Confirmed in code |
| D1 | Two device types, no `DeviceConfig` change | Decided |
| D2 | One card class, variant-constructed, two factories | Decided |
| D3 | C intercepts, A falls through unchanged | Decided |
| D4 | Third audio source, centered; gain recomputed | Decided |
| D5 | Formant synthesis model | Decided; tables PENDING-1 |
| D6 | Spectral assertion for SC-001a | Decided |
| D7 | Cycle-driven timing, host-rate rendering | Decided |
| D8 | Boot-sector speech smoke test, early | Decided |
| P1 | Register model + phoneme tables | **PENDING** — datasheet |
| P2 | Board decode range + request-line wiring | **PENDING** — schematics |
| P3 | Reference rendering | PENDING — blocks SC-001b only |
| P4 | Speech acceptance set | PENDING — blocks sign-off only |

**No NEEDS CLARIFICATION remains in the spec.** PENDING-1 and PENDING-2 are
source-acquisition tasks, not design questions: the design above is complete and
does not change based on their answers — only its tables and constants do.
