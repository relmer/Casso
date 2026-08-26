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

### D10 — Read the prior art freely; the line is copying, not looking

**Decision**: Read other emulators' sources, documentation, and issue trackers as
much as is useful. **Do not copy their code into ours.** That is the whole rule.

**On an earlier, wrong version of this decision**: this section previously said
their source should not be opened at all, on the theory that "never looked" is
the strongest posture. That is not the project's rule and it is not a good one —
refusing to read buys no legal protection and forfeits real information. The most
valuable thing in another project's file is often not its code but its
**provenance**, which is exactly what produced the finding below.

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

**Licensing boundary**: both emulators are GPL-2, so their *code* stays out of
ours. Their documentation, their approach, and — critically — the **provenance of
their data** are all fair game and worth mining.

### D10a — Provenance of the formant data, and why it does not help us

Tracing where MAME's formant values come from produced the most consequential
finding in this research, and it is a negative one.

**MAME does not contain the SC-01's formant tables. It reads them from
`sc01a.bin` — a dump of the chip's own internal parameter ROM** (CRC32
`fc416227`), extracted by decapping the part in 2007. The patent describes the
structure this ROM holds: for each of the 64 phonemes, twelve control-signal
parameters, including the formant frequencies and Q values.

**That reframes the licensing question entirely.** A hardware ROM dump is not the
creative work of a program that reads it, and it is not encumbered by that
program's license. Casso already fetches Apple II ROM images as a matter of
course. An MIT-licensed Python port of MAME's implementation exists and takes the
same `sc01a.bin` as an external input, which is consistent with the file being
treated as chip data rather than project code.

**And then the finding that matters**: `sc01a.bin` is the **SC-01A's** ROM. The
SSI-263A/SC-02 is a different part with **different phoneme data, and its
parameter ROM has never been extracted.** MAME and AppleWin both approximate the
SC-02 by mapping its phonemes onto SC-01 data — which their own communities
describe as not producing accurate speech, the two chips having 64 phonemes in
different orders that do not map 1:1.

**There is therefore no upstream source to find, in any license, for the chip
this feature emulates.** The data is still inside the part. What exists publicly:

| Asset | Status |
|---|---|
| SC-01A parameter ROM (`sc01a.bin`) | Extracted 2007, freely circulating — **wrong chip** |
| SSI-263A/SC-02 parameter ROM | **Never extracted** |
| SSI-263P die photographs (visual6502) | Published, high resolution — the raw material, unextracted |
| SSI-263A datasheet + programming guide | Available (PENDING-1) |
| Votrax patents, ICASSP-78 paper | Available — structure, not this part's values |

This is an open problem in the emulation community, not an oversight on our part.
See D11 for what to do about it.

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

### D11 — DECIDED (2026-08-25): ship on A, spike C, keep B as the fallback

**Decision**: Build on **route A**, then run **route C as a bounded spike** to
see whether the parameter ROM is readable from the published die photographs. If
C succeeds it supersedes both others. If it fails, **route B** stays available —
but sourced as a community recording rather than a hardware purchase, since no
SSI-263 hardware is on hand.

**Ordering rationale**: all three routes produce the same artifact — a formant
table — and share all their code, so the route only determines where the numbers
come from and can change late without rework. A ships. C is cheap to *evaluate*
and, if it works, is the definitive answer and a genuine first. B is the
fallback rather than the primary because it has a hardware dependency the project
does not currently satisfy.

**Asset availability — checked 2026-08-25.** The visual6502 page is reachable
(its TLS certificate has expired, so plain HTTP with certificate checking
disabled is required; the content is public static files and no credentials are
involved). It publishes:

| Asset | Size |
|---|---|
| Die shot, 1600 × 1326 JPG | 1.29 MB |
| **Die shot, 7000 × 5803 JPG** | **24.7 MB** — the largest published |
| Stitch map, 7000 × 5803 PNG | 974 kB |
| `SSI_263A_Data_Sheet.pdf`, `_v2.pdf`, `SC_02_Data_Sheet.jpg` | — |
| **`SSI_263A_Programming_Guide.pdf`** | — a *second* document beyond the datasheet; see PENDING-2 |

**Two findings that lower C's odds:**

1. **The full-resolution master is not published.** The stitch is 17,265 × 14,313,
   but the largest available image is 7,000 × 5,803 — 40% linear, about 16% of
   the captured pixels. The master exists (Christian Sattler stitched it) and
   could be requested, but it is not a download.
2. **There is no sign of delayering.** The page offers a single image series at
   20× magnification, with no per-layer variants (no metal / poly / diffusion
   sets) of the kind visual6502 publishes when a die has been delayered. That is
   consistent with an as-is shot with metal intact — which is the case where the
   ROM array is obscured.

Two unknowns still decide it, and neither can be settled without looking at the
images:

1. **Layer state.** If the published shot is top-metal-intact, the ROM array is
   probably obscured — mask ROMs are typically encoded below the metal. If the
   die was delayered, the array may be directly legible.
2. **Programming method.** If the ROM is *implant*-programmed rather than
   contact- or diffusion-programmed, the bits are **optically invisible at any
   resolution** without staining or alternative imaging. This is a known dead end
   in ROM extraction and would end route C regardless of image quality.

**Spike shape**: bounded, and it answers a yes/no. Fetch the 24.7 MB die image,
locate the parameter ROM array (the patent's description — 64 phonemes × 12
control parameters — gives its expected size and regular structure, which is what
makes it findable), and determine whether individual cells are distinguishable.
Stop at that answer. Do not begin transcription inside the spike.

**Escalation, only if the spike is encouraging**: if the array is visible but the
published downsample is too coarse to read cells, ask visual6502 for the
full-resolution master. If metal obscures it, ask whether a delayered set exists
— they received *two* chips from the donor, so a second delayering may be
possible. Both are community requests, not project work, and neither is on the
critical path.

**Value if C succeeds**: correct SSI-263 speech, which no emulator currently has,
plus an extraction worth publishing back to the community that made the die shots
available in the first place.

### D11c — Spike result (2026-08-25): the array is there, and it is legible

Ran against the 7000 × 5803 published image. **Encouraging, and not yet
sufficient.**

**The parameter ROM is located.** A block of strongly periodic texture occupies
native coordinates **x 2350–3645, y 3100–3745** — visually distinct from the
surrounding logic, which is irregular. Autocorrelation gives a **column pitch of
19.0 px** (peaks at 19 and 38) and a **row pitch of ~13.3 px** (peaks at 13, 27,
40, 53 — a fundamental with three harmonics, so the earlier 27 px reading was a
double-period artifact).

**Its geometry matches the expected ROM almost exactly:**

| | Measured | Expected |
|---|---|---|
| Columns | **68** | 64 phonemes (+ spares / dummy columns, which are conventional) |
| Rows | **48** | 12 control parameters × 4 bits = **48** |

The 48 is the persuasive number. The patent describes twelve control-signal
parameters per phoneme, and secondary sources describe the formant parameters as
4-bit — 12 × 4 = 48 rows, measured independently from image periodicity. Treat
this as strong corroboration rather than proof; the mapping of rows to specific
parameters is still unverified.

**The cells are not obscured.** Metal is intact, but the programming features are
visible *through* it — this is contact/via-programmed ROM, where presence or
absence of a contact encodes the bit, and that is exactly the case that survives
an un-delayered shot. Implant programming, the failure mode that would have ended
route C outright, is ruled out.

**But this resolution is marginal.** At 19 × 13.3 px per cell, the programming
features are only a few pixels across. A naive per-cell contrast metric returned
93% of cells as set, which is not plausible ROM content — it was measuring stripe
edges, not contacts. Reliable extraction needs a proper cell template, and
realistically more pixels.

**No bits were extracted, and none should be claimed.** The spike's question was
whether cells are distinguishable; the answer is that they are present and
visible but under-resolved at 40% of the master's linear resolution.

### D11c-census — extraction attempted on the published image (2026-08-26): NOT machine-readable

A full census was attempted -- template-match the oval ring at every grid
cell, classify present/absent. Three passes, each failing more instructively:

1. **Constant-pitch grid census**: produced a beautifully bimodal score
   histogram that turned out to be measuring REGISTRATION, not bits -- the
   giveaway was per-column bit counts forming a smooth spatial gradient
   (1, 8, 33, 43, 47, 48...), which data never does and grid drift always
   does. Rough pitches (19.0/13.3 px) accumulated ~24 px of row error across
   the array, and a search window over half the row pitch let dense cells
   steal neighbors' ovals.
2. **Refined-pitch census** (long-range autocorrelation: 18.46/12.81 px):
   still not data -- columns clustered into all-zero / all-one / all-zero
   bands, the beat between a constant-pitch grid and the true geometry.
   With 203 stitched photographs the true pitch is not constant; stitch
   distortion guarantees any fixed grid walks off somewhere. This also
   corrects D11c's geometry claim: the array measures ~70 x 50 sites at the
   refined pitches, so "68 x 48" was partly the miscalibration flattering
   the 12x4 hypothesis. A 64 x 48 payload inside dummy edge rows/columns
   remains plausible -- as hypothesis, not measurement.
3. **Detection-first** (high-pass + unconstrained ring correlation + local
   maxima -- immune to grid drift): the local-max response histogram is
   UNIMODAL. ~19,700 maxima where ~1,500 true ovals should live: at 7000w,
   photographic grain produces ring-scale responses statistically
   inseparable from the real programming features.

**Conclusion**: the ovals are individually visible to the eye in clear
regions, but the published 7000 x 5803 image cannot be machine-extracted --
grain-to-feature response overlap, compounded by stitch distortion. This
sharpens the T057 ask: the ~2.5x master puts a ring at ~25 x 30 px, where
grain separates trivially, and the master extraction should fit its grid
FROM detections per region rather than assume constant pitch.

### D11c-census-2 — reversed on user challenge (2026-08-26): the image IS machine-detectable

The user pushed back on "not machine-readable" -- the ovals are plainly
visible to the eye -- and the challenge was correct. Pass 3 above had two
methodology flaws: UNNORMALIZED correlation (rewards any high-contrast grain,
not ring shape) and an unrestricted search domain (most of its ~20k maxima
came from places no eye would look). With those fixed -- a template cut from
a REAL oval, true normalized cross-correlation via integral-image statistics
-- detection works: a distinct high-score population separates at NCC > 0.80,
and 1,109 detections at > 0.65 organize into a clean lattice.

What the working detector revealed about the geometry (and why every fixed
grid failed): **the column pitch alternates 20/20/16 px** -- a repeating
three-column group, so the true grid was never constant-pitch. 63 column
bands detected (consistent with 64 plus sparse columns). Rows sit on the
~12-16 px mesh with ~39 populated row-lines detected; the programming
feature is a tall vertical stadium (~8-11 x 15-25 px) spanning about two
mesh rows, with a shorter variant also present.

**Corrected status**: extraction from the published image is PLAUSIBLE, not
impossible -- remaining work is a second (short-oval) template, merged
detection, lattice snapping with the 20/20/16 group structure, and occupancy
classification with contact-sheet audits. The master remains the difference
between "plausible with care" and "trivial": ~2.5x pixels per feature and
far better grain separation.

Method lesson worth keeping: "statistically inseparable" claims about
detection are claims about the DETECTOR, not the data, until the template is
drawn from the data itself and the domain is restricted to where a human
would look.

**Recommendation: escalate (T057).** The full 17,265 × 14,313 master would give
roughly **47 px column and 33 px row pitch** — a different proposition entirely,
and very likely sufficient. The master exists; it is simply not published.
Requesting it is a message, not a project.

### D11-prior — the options as originally framed

D10a means the acoustic-fidelity target is a scope decision, not a research task.
Three routes, and they differ by more than an order of magnitude in effort:

| Route | What it means | Cost | Result |
|---|---|---|---|
| **A — Match the state of the art** | Do what MAME and AppleWin do: drive the synthesis from SC-01A data mapped onto the SSI-263's phoneme ordering | Low. `sc01a.bin` is available and the mapping problem is understood | Speech that works and is recognizable, with the same known inaccuracy every other emulator has |
| **B — Measure real hardware** | Record all 64 phonemes from a real SSI-263 and extract formant targets by spectral analysis | Medium. Needs hardware access and signal-processing work, no decapping | Genuinely accurate for this part. Achievable without novel reverse engineering |
| **C — Extract the ROM from the die** | Read the parameter ROM off the published visual6502 die photographs | High, and speculative | The definitive answer, and a first — nobody has published this |

**B was initially recommended on the assumption hardware could be reached.** It
cannot, currently — see D11 for the resolved ordering. B requires a physical
SSI-263 being driven and recorded; it does not require *us* to own one, since a
community member with a Mockingboard C recording a prescribed phoneme sequence
satisfies it equally, which turns B from a purchase into a request.

### D12 — The formant table is a fetched asset, not repo content (T003)

**Decision**: Treat the route-A parameter data exactly as Casso already treats
every other chip ROM — **fetched on demand with user consent, never committed**.

**Rationale**: `scripts/FetchRoms.ps1` downloads peripheral card ROMs rather than
vendoring them, and `Casso/AssetBootstrap.cpp` fetches ROMs, sample disks, and
Disk II audio on first launch with the user's agreement. The //e Enhanced
firmware is already a managed download-on-demand asset. Chip ROM data is the
same category of thing and gets the same treatment; there is no reason to invent
a second mechanism, and committing a ROM image would be a departure from
established practice rather than a convenience.

**Shape**:

| | |
|---|---|
| Identity | `sc01a.bin`, verified by its CRC32 (`fc416227`) — a checksum mismatch must fail loudly, not degrade silently, because wrong parameter data produces *plausible* wrong speech |
| Storage | Alongside the other managed assets; absent by default |
| Absence | The card must still construct and the sound half must work. Speech degrades to silence with a clear reason surfaced, never a crash or a hang — a user without the asset has a working Mockingboard A in all but name |
| Swappability | Loaded through one seam so routes B and C can replace the source with no code change (D11) |

**Two constraints inherited from elsewhere in this document:**

- **It is the wrong chip's data** (D10a), so FR-023 requires the substitution be
  disclosed both in the release notes and beside the loading code.
- **It is chip data, not another project's code** (D10), which is what makes it
  usable at all. That distinction is the whole reason this route exists, and it
  should be stated where the asset is defined so a later reader does not
  re-litigate it.

**Open at implementation**: which URL the asset is fetched from. The file
circulates widely; picking a stable, appropriately-licensed host is a task for
whoever wires the bootstrap entry, not a decision this document can make
usefully in advance.

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

**Status after D10a**: *no public source exists for this part.* The SC-02/SSI-263
parameter ROM has never been extracted, and every existing emulator approximates
it with the wrong chip's data. This is not a document we have failed to find.

**Resolution path**: D11 — ship on route A (SC-01A data mapped over, as the
other emulators do), then spike route C to see whether the parameter ROM is
legible in the published die photographs. Route B is the fallback if C fails,
sourced as a community recording. D9's literature ordering supplies the model
structure and sanity bounds throughout.

**Not excluded**: reading MAME's and AppleWin's source, per the corrected D10.
Only copying their code is out. `sc01a.bin` itself is chip data rather than
project code, and route A uses it on that basis.

### PENDING-2 — RESOLVED for the decode (2026-08-25), with two premises now in doubt

Reading the prior art directly — permitted and encouraged under the corrected
D10 — supplied both facts the datasheet could not:

- **Decode**: the speech chip is selected when **A4 clear, (A5 or A6) set, A7
  clear**; A6 selects between two chips. That yields `$20`–`$2F` for chip 0 and
  `$40`–`$4F` plus `$60`–`$6F` for chip 1, with `$30`/`$50`/`$70` excluded by the
  A4 term. Recorded in the contract.
- **Interrupt**: **A/R drives the 6522's CA1**, falling edge selected by clearing
  PCR bit 0 — exactly the seam already built, and consistent with the datasheet's
  own description of A/R.

**But cross-checking against the Mockingboard mini-manual put two of this spec's
premises in doubt, and neither should be resolved by picking the more convenient
source.**

#### Doubt 1 — how many voice chips, and how many PSGs

| Source | Says |
|---|---|
| Emulator prior art | Two speech chips per card, one per VIA channel, each separately addressed |
| Mockingboard mini-manual | "Sound/Speech I" = **one** PSG + **one** speech chip; "Sound II" = two PSGs |
| This spec's Overview | Mockingboard C = two VIAs + two PSGs + one voice chip |

Casso's existing card is two VIAs + two PSGs, which matches the mini-manual's
**Sound II**, not its Sound/Speech I. The lettered model names (A/B/C/D) and the
Sound-I/Sound-II/Sound-Speech-I names appear to be **different naming schemes**
that the community routinely conflates — which is very likely why the product
table in this spec's Overview looked tidy and may not be right.

**This does not block the decode**, which is the same either way. It affects how
many `Ssi263` instances the card holds and what the UI calls the models.

#### Doubt 2 — SC-01 or SSI-263

The mini-manual describes Sweet Micro's own speech boards as carrying the
**Votrax SC-01**, not the SSI-263. This spec assumed the SSI-263 throughout, on
the grounds that it is what later boards carry and what surviving software
targets. Both can be true — the SSI-263 arriving on later boards and clones —
but the assumption is now *contested by a period source* rather than merely
unverified, and it is the deepest premise in the feature.

**Recommendation**: implement the decode for both chip positions as found, but
populate only chip 1 initially. That is conservative under either reading of the
model line, needs no rework if the second chip is added later, and keeps the
question open honestly instead of settling it by assumption. The chip-count and
part-number questions want a human decision, not another source.

### PENDING-2 doubts — RESOLVED (2026-08-25): two product generations, not one contradiction

Triangulating three further sources (the Apple II wiki's model breakdown, the
encyclopedia's chip complements, and the mini-manual already in hand) dissolves
both doubts. **The sources never disagreed — they describe different product
generations**, and this spec's original Overview table conflated the two naming
schemes into one, which is exactly why it looked tidy and was wrong.

**Generation 1 (early 1980s, named descriptively) — the mini-manual's world:**

| Product | Complement |
|---|---|
| Sound I | 1 PSG |
| Sound II | 2 PSGs |
| Speech I | 1 **SC-01** |
| Sound/Speech I | 1 PSG + 1 **SC-01** |

**Generation 2 (mid 1980s, lettered) — the generation this feature models:**

| Product | Complement |
|---|---|
| Mockingboard **A** | 2 VIAs + 2 **AY-3-8913** PSGs + **two empty SSI-263 sockets** |
| Mockingboard **B** | The SSI-263 upgrade kit — **one chip**, installed into an A's socket |
| Mockingboard **C** | "A + B = C": the A with **one SSI-263 installed**. One speech chip only |
| Mockingboard **D** | //c external unit: 2 PSGs + 1 SSI-263 |
| Mockingboard M/MS | 2 PSGs + one open speech socket |

**Consequences, each pinned:**

1. **Doubt 1 resolved — one voice chip on the C.** The prior art models both
   *sockets*; the real C ships one chip. Software convention writes `$Cn40`,
   which is position 1's range, so **the C populates position 1** and position 0
   is an empty socket. The "populate only chip 1" recommendation above is now the
   product's own definition, not conservatism.
2. **Doubt 2 resolved — the SSI-263 is correct.** The SC-01 belongs to
   Generation 1 exclusively. The mini-manual was describing Sound/Speech I, a
   different board this feature never claimed to model. Everything transcribed in
   T001 stands.
3. **The spec's Overview table was the error** — it equated A with "Sound I" and
   C with "Sound/Speech I" across generations. Corrected in spec.md. Notably,
   Casso's existing 2-VIA + 2-PSG card **is exactly a Mockingboard A**, so the
   current implementation was already faithful to the lettered line.
4. **Part-number nuance**: the lettered boards carry the **AY-3-8913**, the
   24-pin variant of the 8910 without the I/O ports. Register-compatible for
   everything Casso models; recorded for naming precision, no code change.

### F3 — The "removed mirror" premise was wrong: the speech chip is a tap, not a replacement

The prior art carries a hardware-verified note that speech-range writes **also
reach the first 6522** — confirmed against real hardware. The VIAs on the board
do not decode A4–A6 (which is *why* today's whole-page mirroring in Casso's card
is faithful, not loose), and adding a speech chip does not change that. The chip
is an **additional listener** on its ranges:

- **Writes** in `$40`–`$4F`/`$60`–`$6F` reach **both** VIA #1's mirrored register
  and the speech chip.
- **Reads** in a populated range return the chip's status on **D7** (the only
  line the chip drives). Real software tests D7 with `BMI`/`BPL` and ignores the
  rest.
- An **empty socket** (position 0 on the C, both positions on the A) leaves the
  range behaving exactly as the A does today.

This *retires the spec's first named compatibility risk* — "a lost address
mirror" — in the best possible way: the mirror is not lost. The C differs from
the A only by adding a listener and driving one bit on read. D3's guarantee (the
A executes today's path unchanged) survives intact, and the C's decode becomes
additive rather than substitutive, which is strictly easier to verify.

### PENDING-2-original — Where the chip is decoded in the slot page, and how its request line is wired

**Needed**: The address range within `$Cn00` that the board decodes to the speech
chip, and which VIA control line (and which VIA) the request output drives.

**Why it cannot be inferred**: This is a property of the *board*, not the chip —
the same chip on a different card would be wired differently. It determines
exactly which mirror the C removes (the Overview's first named compatibility
risk) and which IFR bit software sees.

**Primary source**: Sweet Micro Systems Mockingboard schematics (Apple II
Documentation Project); the ReActiveMicro Mockingboard wiki, which hosts
schematics; and the Mockingboard Mini-Manual.

**Additional lead found 2026-08-25**: visual6502 hosts an
`SSI_263A_Programming_Guide.pdf` alongside the datasheet — a separate document
that may cover host-side addressing and interfacing rather than only the chip.
Read it before hunting schematics.

**Empirical alternative, possibly better than the schematic**: disassemble the
speech driver of a title that actually talks and observe which addresses it
writes. That is evidence drawn from the exact population this feature must be
compatible with, which a schematic cannot quite provide.

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

**Scope note**: for SC-001c a single phrase suffices. If D11 falls back to route
B, the same request becomes systematic — all 64 phonemes — because the recording
is then the primary source for the formant tables rather than a tuning pass.
Worth asking for the full set up front either way; it costs the volunteer little
more and removes a second round trip.

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
| D10 | Read prior art freely; never copy their code | Decided (corrected) |
| D10a | Formant data provenance: SC-01A ROM dump, **wrong chip**; SC-02 never extracted | Confirmed |
| D11 | Accuracy: ship on **A**, spike **C**, keep **B** as community-recording fallback | ✅ Decided |
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
