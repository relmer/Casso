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

### D5 — Formant synthesis

**Decision**: Model the chip as what it is — a formant synthesizer: an excitation
source (voiced pulse train / unvoiced noise) driving a bank of resonators whose
center frequencies and bandwidths are set per phoneme and interpolated across the
transition into the next one.

**Rationale**: This is the only model that reproduces the chip's actual behavior
under the controls software manipulates. A sampled-phoneme lookup would produce
recognizable speech but would not respond correctly to the inflection and rate
registers, failing FR-003 and SC-001b.

**Externally confirmed** by D10: the public description of this chip family's
synthesis is formant filters set to per-phoneme frequencies, applied to a
vocal-cord approximator, with transitions performed by sliding the filters from
one phoneme's frequencies toward the next. That is this model, arrived at
independently by the project that reverse-engineered the part from die
photographs.

**Constraint**: The per-phoneme parameter tables are *not* in the datasheet. See
PENDING-1b and D9.

### D6 — Two verification layers, because the datasheet only covers one

**Decision**: Split chip verification in two, because the two halves have
different sources of truth (see PENDING-1-RESOLVED):

1. **Behavioral conformance** — register decode, the full 64-code phoneme set,
   the published duration and filter-frequency formulas, amplitude and rate
   response, and A/R assert/disable timing. Asserted directly against datasheet
   formulas. No fixtures, no listener, permanent regression gate.
2. **Acoustic fidelity** — that a phoneme's rendered output carries the formant
   content it should. Verified by spectral assertion (Goertzel evaluation at the
   target bins, in-band versus out-of-band energy) against targets that come from
   the sources in D9, *not* from the datasheet.

**Rationale**: The original single criterion assumed the datasheet published
per-phoneme formant frequencies. It does not (PENDING-1-RESOLVED). Layer 1 is
now *better* grounded than originally specified — it tests published formulas
rather than an assumed table — and layer 2 is honestly separated from it, with
its own sourcing problem stated rather than hidden.

**Alternatives considered**: Golden PCM files (brittle across any resampling or
gain change, and large in-tree); automated speech recognition scoring (an ASR
engine's behavior on 1982-vintage formant synthesis is itself unvalidated — it
would be measuring the recognizer, not the chip).

### D9 — Formant targets from the patent literature first, then tuned against a recording

**Decision**: Source per-phoneme formant targets in this order:

1. **The Votrax patents and the designer's own published paper.** Richard Gagnon
   and Duane Houck hold speech-synthesizer patents covering this chip lineage
   (US 4,527,274 "Voice synthesizer" and US 4,829,573 "Speech synthesizer" are
   the candidates to read first), and Gagnon published *"Votrax Real Time
   Hardware for Phoneme Synthesis of Speech"*, ICASSP-78, pp. 175–178. Patents
   and conference papers are public documents describing the actual device —
   the same category of source as a datasheet, and the strongest available.
2. **Published acoustic phonetics** — Peterson & Barney vowel formant data,
   Klatt synthesizer parameters — to fill any phoneme the patents do not
   parameterize. Public literature, immediately available, defensible.
3. **Spectral measurement of a reference recording** to tune the result
   (PENDING-3).

**Rationale**: The user's direction was published phonetics first, then tune
against a recording. The patent literature slots in *ahead* of general phonetics
because it describes this chip rather than speech in general — a strictly better
source in the same legal category, discovered while surveying prior art (D10).

**This makes the architecture in D5 externally confirmed** rather than inferred:
the public description of how this chip family works is formant filters set to
per-phoneme frequencies, applied to a vocal-cord approximator, with transitions
performed by sliding the filters from one phoneme's frequencies toward the next.
That is D5, and it is what the emulator that reverse-engineered this chip family
from die photographs concluded independently.

### D10 — Prior art surveyed at the level of approach, never source

**Decision**: Record what other emulators do, from their public documentation,
release notes, and issue trackers. **Do not read their source.**

**Findings**:

| Emulator | Approach |
|---|---|
| **AppleWin** (GPL-2) | Sampled phoneme playback at a fixed rate. Its own documentation describes the SSI-263 emulation as basic, with **no inflection and no filter support**. It also reuses SSI-263 phonemes to approximate the SC-01, which its notes acknowledge is rough, the two chips having 64 phonemes in different orders that do not map 1:1 |
| **MAME** (GPL-2) | Full synthesis for the SC-01, debuted in 0.181, built by reverse-engineering **die photographs plus the patent** — timing circuit, transition circuit, glottal generator, and noise source. Described as near-perfect digitally, with the analog section still imperfect (plosives) |

**What this tells us**:

- **The approach in D5 is the right one and is more ambitious than the reference
  Apple II emulator.** AppleWin took the sampled route that D5 rejected, and its
  own docs name the resulting limitation — no inflection, no filters — which is
  exactly the FR-003 capability this feature promises.
- **The patent is a legitimate, public route to the parameters**, and MAME's work
  is the existence proof that it is sufficient. That is what D9 acts on.
- **Die photographs of the SSI-263P are publicly published** by visual6502, an
  independent primary source available to us directly.

**Licensing boundary — this is the important part.** Both emulators are GPL-2.
The constitution forbids copyleft dependencies and FR-002 forbids copying. The
strongest clean-room posture is *not having looked*: **do not open MAME's
`votrax.cpp` or AppleWin's speech sources at any point.** The patents, the
datasheet, the ICASSP paper, and the die shots are all independently available
and are what this feature is built from.

A specific trap worth naming: AppleWin's phoneme samples are plausibly recorded
from real hardware, which makes them tempting as acoustic ground truth. They are
GPL-licensed assets in a GPL repository, and deriving formant tables from them
would be derivation from a copyleft work. **Use a recording we make or obtain
independently** (PENDING-3), not theirs.

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

### PENDING-1 — RESOLVED (2026-08-25), and it changed the plan

The SSI-263A datasheet is freely available, including as OCR'd text. It also
turns out that **the SSI-263A and the Votrax SC-02 are the same part**, so SC-02
documentation applies interchangeably and widens sourcing.

**What the datasheet supplies:**

| Item | Content |
|---|---|
| Register map | **Five registers**, selected by RS2–RS0: Duration/Phoneme (DR1–DR0 + P5–P0), Inflection, Rate/Inflection, Control/Articulation/Amplitude, Filter Frequency |
| Phoneme set | **Complete — 64 codes, `$00`–`$3F`**, with phonetic symbols and example words |
| Duration | A formula, not a table: `Phoneme Duration = Frame Duration × (4 − D)` |
| Filter | `Filter Frequency = XCK / [2 × (256 − FF)]` |
| A/R pin | Open-collector; goes **high→low once the phoneme is generated**; documented as usable as an interrupt request, and **disableable** via CTL and the DR1–DR0 mode bits |

The A/R row is a gift for FR-015: the chip has a *documented* mode in which it
does not request at all, so quiescence-until-programmed is hardware behavior we
reproduce rather than emulator policy we invent.

**What the datasheet does NOT supply, at all: per-phoneme formant frequencies or
filter coefficients.** It describes "five cascaded programmable low pass filter
sections" and publishes no numeric parameters for them; they live in on-chip ROM.
This is not a bad scan or the wrong document — the data was never published.

**Consequence**: the original SC-001a ("assert energy at the formant frequencies
named in the datasheet") was unsatisfiable as written, because the datasheet
names none. Superseded by D6's two layers and D9's sourcing order. The
behavioral layer is now better grounded than originally specified, since it tests
published formulas rather than an assumed table.

**Sources**: SSI-263A datasheet — `archive.org/details/ssi-263-a` (OCR text
available); Votrax SC-02 / SSI-263A datasheet — bitsavers, `federalScrewWorks/`.

### PENDING-1b — Per-phoneme formant targets

**Needed**: Formant center frequencies and bandwidths per phoneme, plus
transition behavior, for the D5 synthesis model.

**Resolution path**: D9 — patents and the designer's ICASSP-78 paper first,
published acoustic phonetics to fill gaps, then tuned against a measured
reference. **Not blocked on any single document**, which is the point of the
ordering: work can start from the literature and improve, rather than waiting.

**Explicitly excluded**: MAME's and AppleWin's source and assets (D10).

### PENDING-2 — Where the chip is decoded in the slot page, and how its request line is wired

**Needed**: The address range within `$Cn00` that the board decodes to the speech
chip, and which VIA control line (and which VIA) the request output drives.

**Why it cannot be inferred**: This is a property of the *board*, not the chip —
the same chip on a different card would be wired differently. It determines
exactly which mirror the C removes (the Overview's first named compatibility
risk) and which IFR bit software sees.

**Primary source**: Sweet Micro Systems Mockingboard schematics (Apple II
Documentation Project).

### PENDING-3 — A reference recording — PROMOTED to the critical path

**Was**: a nice-to-have gating one human judgement. **Now**: the tuning target
for D9 step 3, and therefore the acoustic ground truth for the synthesis itself.
PENDING-1's formant gap is what promoted it.

**Needed**: A recording of known phonemes produced by real hardware — a
Mockingboard C, a Phasor, or a bare SSI-263A/SC-02 on a bench — clean enough to
extract formant frequencies from by spectral analysis.

**Resolution path**: The Apple II community is the realistic source. A request on
comp.sys.apple2, Applefritter, or the ReActiveMicro community — who still sell
the SSI-263 as a replacement part, so working hardware is in circulation — is
more likely to succeed than hunting for an existing archived capture. Ask for a
specific authored phoneme sequence rather than "some speech", so the recording is
directly comparable to our own output.

**Do NOT use** AppleWin's phoneme samples as the reference, however convenient:
they are GPL-licensed assets, and deriving formant tables from them is derivation
from a copyleft work (D10).

**Blocks**: D9 step 3 (tuning), SC-001b's final targets, and SC-001c. **Does not block** starting the
synthesizer from the literature — that is exactly why D9 is ordered as it is.

### PENDING-4 — The speech acceptance set

**Needed**: Period titles that drive Mockingboard speech, with obtainable images.

**Guidance**: Prefer the manufacturer's own demo and utility software, which was
written to exhibit the chip and often speaks a documented phrase — supplying a
transcript that game software does not. Candidate game titles should be verified
against a Mockingboard software list rather than recalled; whether a given title
uses speech or only music is exactly the kind of detail that is misremembered.

**Blocks**: SC-001d and User Story 1 sign-off only. Development proceeds without
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
| D5 | Formant synthesis model | Decided; externally confirmed by D10 |
| D6 | Two verification layers: behavioral + acoustic | Decided (revised) |
| D7 | Cycle-driven timing, host-rate rendering | Decided |
| D8 | Boot-sector speech smoke test, early | Decided |
| D9 | Formant sources: patents → phonetics → measurement | Decided |
| D10 | Prior art surveyed by approach only; never their source | Decided |
| P1 | Register model, phoneme set, timing formulas, A/R | ✅ **RESOLVED** — datasheet in hand |
| P1b | Per-phoneme formant targets | Open — worked via D9, blocks nothing |
| P2 | Board decode range + request-line wiring | **PENDING** — schematics |
| P3 | Reference recording | **PENDING** — promoted to critical path |
| P4 | Speech acceptance set | PENDING — blocks sign-off only |

**No NEEDS CLARIFICATION remains in the spec.** PENDING-2 is the only remaining
item that blocks code, and only step 4 of the sequencing.

**The lesson from PENDING-1 is worth keeping.** It was written as "acquire the
datasheet and transcribe its tables", and acquiring it revealed the tables do not
exist. Had the parameter tables been guessed instead — they were guessable, and a
guess would have looked entirely plausible — the error would have survived review
and surfaced only as bad speech against real software, with no obvious cause.
The remaining PENDING items are held to the same standard: sourced, or stated as
unsourced. Never inferred and presented as fact.
