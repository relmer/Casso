# Feature Specification: Mockingboard C — Sound/Speech

**Feature Branch**: `024-mockingboard-speech`

**Created**: 2026-08-24

**Status**: Draft

**Input**: User description: "Add the SSI-263 voice chip to the existing Mockingboard emulation, and make the sound+speech card the default. Two card variants: the sound-only Mockingboard A we ship today, and a new Mockingboard C. Compatibility with sound-only software is the headline requirement."

## Overview

Casso emulates half a Mockingboard. Version 1.7.0 shipped the sound half — two
6522 VIAs driving two AY-3-8910 PSGs — and GH #66 explicitly deferred the rest:
*"SSI-263 speech synthesizer (Mockingboard B+ / Phasor)"*, listed under **Out of
scope (v1)**. That issue closed. Nothing has tracked the voice chip since.

The consequence is that speech-using software is silent on Casso in a way that
looks like a bug rather than a missing feature. The card answers, the music
plays, and the talking does not happen.

### The voice chip is not a separate card

This matters for how the feature is shaped. Sweet Micro Systems sold the
Mockingboard as a small product line, and speech was a **model distinction**, not
an add-in card in another slot:

| Product | Contents |
|---|---|
| Mockingboard **A** ("Sound I") | 2 VIAs + 2 PSGs. Sound only. |
| Mockingboard **B** | Speech board sold as an upgrade that mated with an A |
| Mockingboard **C** ("Sound/Speech I") | Sound + speech as one product |
| Mockingboard **D** | The //c external unit |

So the faithful model is not "a card plus an accessory" — it is **two cards a
user could buy**, one of which is a superset of the other. This feature therefore
delivers two variants and lets a machine be configured with either.

### Compatibility is the headline requirement, and history is the precedent

Because this feature makes the sound+speech card the **default** for the Apple
][+, //e, and //e Enhanced profiles, every Mockingboard title that works today is
in scope for regression. That is the risk the feature carries and the thing it
must prove.

The precedent is reassuring: Sweet Micro sold the C as a superset of the A into a
market where the A already had a software library, and A-targeting software ran
on speech-equipped boards. Compatibility is not a hope here, it is how the real
product line worked. What we have to earn is that *our* implementation inherits
that property, and the risk concentrates in two places:

1. **A lost address mirror.** The card decodes a slot's I/O page loosely today,
   so the region where a real C answers with its speech chip currently mirrors a
   VIA register file. On the C that mirror is replaced. The sound-only A variant
   must keep the current behavior exactly.
2. **A new interrupt source.** The speech chip's ready/request signal reaches
   software through a VIA control line, which means it can raise a flag in a
   register that sound-only music players already read. A player that dispatches
   on *"the VIA interrupted"* rather than testing the timer bit specifically
   would misbehave if the speech chip ever asserts unbidden.

Both are avoidable, and the requirements below pin them down.

### Relationship to per-slot card configuration

A follow-on feature will make each slot's occupant user-selectable from the
Hardware tab, with default slot assignments modeling where people actually
installed each card. That is where a user will *choose* between the A and the C,
alongside choosing any card for any slot.

**This feature does not depend on it.** Here the variant is selected by machine
configuration, which is enough to build the chip, ship it as the default, and
validate it against real software. The follow-on spec exposes the choice; it does
not enable it. Sequencing the chip first also keeps the interesting emulation
problem away from the settings-persistence work, so each can be validated on its
own terms.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Software that speaks, speaks (Priority: P1)

A user boots a title that drives the Mockingboard's speech synthesizer. Where the
machine is silent today, it now talks — intelligibly, in the characteristic voice
of the hardware, and in sync with what is happening on screen.

**Why this priority**: This is the entire point of the feature. Without it
nothing else here has value, and it is the capability the closed GH #66 deferred.

**Independent Test**: Boot a known speech-using title on a machine with the
sound+speech card and listen. A listener who has not been told the script can
transcribe the spoken words correctly.

**Acceptance Scenarios**:

1. **Given** a machine configured with the sound+speech card, **When** software
   programs the voice chip with a sequence of phonemes, **Then** the emulator
   produces audible speech whose words are correctly transcribable by a listener.
2. **Given** software is mid-utterance, **When** it queries the chip for whether
   it is ready for the next phoneme, **Then** it receives the same answer real
   hardware would give, so its pacing loop advances normally.
3. **Given** software drives speech through interrupts rather than polling,
   **When** the chip becomes ready for the next phoneme, **Then** an interrupt
   reaches the CPU and the software's handler runs.
4. **Given** speech and music are playing at once, **When** both are active,
   **Then** both are audible and neither is drowned out or clipped.

---

### User Story 2 - Every sound-only title sounds exactly as it does today (Priority: P1)

A user who upgrades Casso and boots the Mockingboard music titles they already
own hears no difference whatsoever. Nothing gets quieter, louder, mistimed,
detuned, or hung.

**Why this priority**: Also P1, and non-negotiable, because this feature changes
the **default** card for three machine profiles. Every existing Mockingboard user
is opted in without asking. A regression here is worse than shipping no speech at
all, so this must be demonstrated rather than assumed.

**Independent Test**: Render audio from a set of existing Mockingboard titles on
the previous release and on this one, and compare. Fully testable without any
speech software existing.

**Acceptance Scenarios**:

1. **Given** a title that uses only the sound half, **When** it runs on the
   sound+speech card, **Then** its audio is indistinguishable from the same title
   on the sound-only card.
2. **Given** a music player that arms a timer interrupt and dispatches on the
   card's interrupt flags, **When** it runs for an extended period with the voice
   chip present but never programmed, **Then** it receives no interrupt it did
   not arm, and its tempo is unchanged.
3. **Given** a title that probes the card to detect a Mockingboard, **When** it
   runs its detection routine against the sound+speech card, **Then** detection
   succeeds exactly as it does against the sound-only card.
4. **Given** the machine is cold-booted or reset, **When** no software has
   touched the voice chip, **Then** the chip is silent and asserts no interrupt.

---

### User Story 3 - Choosing the sound-only card (Priority: P2)

A user configures a machine with the plain Mockingboard A instead of the C —
because they are reproducing a period-authentic setup, or because they are
isolating whether the voice chip is implicated in a problem.

**Why this priority**: P2 because the default serves nearly everyone, but the
choice is what makes the two-variant model honest rather than decorative, and it
is the escape hatch if a title ever does turn out to prefer the A.

**Independent Test**: Configure a machine with each variant in turn, confirm the
Hardware tab reports which is installed, and confirm software sees a card with
speech in one case and without in the other.

**Acceptance Scenarios**:

1. **Given** a machine configured with the sound-only card, **When** software
   reads the region where the C answers with its voice chip, **Then** it sees
   exactly what today's release presents there.
2. **Given** a user changes the installed variant, **When** the change is
   applied, **Then** the user is told a machine reset is required, consistent
   with how other hardware changes behave.
3. **Given** a user has chosen a variant, **When** they close and reopen Casso,
   **Then** their choice is still in effect.
4. **Given** either variant is installed, **When** the user opens the Hardware
   tab, **Then** the slot entry names which Mockingboard model is present.

---

### User Story 4 - Speech behaves like the rest of the machine's audio (Priority: P3)

Speech obeys the volume slider and the mute control, does not clip when it is
mixed with music and the built-in speaker, and costs nothing while no software is
using it.

**Why this priority**: P3 because it refines an experience the earlier stories
already deliver, but it is what separates a chip that works from a feature that
feels finished — and the idle cost matters on a machine where the card is now
present by default.

**Independent Test**: Play speech while exercising the volume and mute controls;
separately, measure idle cost with the voice chip present and unprogrammed.

**Acceptance Scenarios**:

1. **Given** speech is playing, **When** the user moves the volume slider or
   mutes, **Then** speech responds exactly as the machine's other audio does.
2. **Given** the voice chip has never been programmed, **When** the emulator
   runs, **Then** the chip contributes no audio and no measurable synthesis work.
3. **Given** speech is mid-utterance, **When** the machine is reset, **Then**
   speech stops immediately rather than finishing the utterance.

---

### Edge Cases

- **Software writes a phoneme while the previous one is still sounding.** The
  chip must behave as hardware does rather than dropping or queueing arbitrarily.
- **Software programs speech but never polls status and never enables the
  interrupt.** Speech must still be produced; nothing may stall waiting for an
  acknowledgment that never comes.
- **A reset arrives mid-utterance** — machine reset, card reset, or a warm boot
  from software. Audio stops immediately and the chip returns to quiescent.
- **Software probes the region where the voice chip lives, expecting the mirror
  that a sound-only card presents there.** On the C it now reaches the voice
  chip. This is faithful to hardware, and it is the specific reason the A variant
  must remain byte-for-byte unchanged.
- **Extreme control values** — rate, inflection, or amplitude at the ends of
  their ranges, including values real software would never write. Output stays
  bounded and never produces a click, a screech, or silence-by-crash.
- **Speech while the emulator is running at 2x or maximum speed.** Speech must
  stay coherent rather than becoming chipmunk-pitched noise or tearing.
- **Speech across an audio-device change** (default device switched, sample rate
  changed) while an utterance is in flight.
- **The emulator is paused mid-utterance**, then resumed.
- **A machine configured with more than one Mockingboard**, once per-slot
  configuration lands — each card's voice chip is independent.

## Requirements *(mandatory)*

### Functional Requirements

#### The voice chip

- **FR-001**: The system MUST emulate the Mockingboard's speech synthesizer such
  that software written for it produces intelligible, correctly-paced speech.
- **FR-002**: The speech synthesizer MUST be implemented clean-room from
  published datasheets. Existing GPL-licensed emulator implementations MUST NOT
  be copied, and MAY be consulted only for behavioral comparison when debugging.
- **FR-003**: The system MUST present the chip's full software-visible control
  surface — phoneme selection, and the inflection, rate, and amplitude controls —
  such that software varying them produces correspondingly varied speech rather
  than a single flat voice.
- **FR-004**: The system MUST expose the chip's ready/request state to software
  both by status read and as an interrupt, matching how the hardware wires that
  signal into the card's existing interrupt path.
- **FR-005**: Speech timing MUST be governed by emulated machine time, so that an
  utterance occupies the same number of emulated cycles regardless of the host's
  audio sample rate or the speed at which the emulator is running.

#### Card variants

- **FR-006**: The system MUST offer two Mockingboard variants: a sound-only card
  (the Mockingboard A) and a sound+speech card (the Mockingboard C).
- **FR-007**: The sound-only variant MUST be behaviorally identical to the card
  Casso ships today — identical address decoding across the whole slot page,
  identical interrupt sources, and identical audio output.
- **FR-008**: The Apple ][+, Apple //e, and Apple //e Enhanced machine profiles
  MUST install the sound+speech card by default.
- **FR-009**: On the sound+speech card, the voice chip MUST be reachable at the
  location real hardware decodes it, distinct from the card's timer/port register
  files.
- **FR-010**: Users MUST be able to configure a machine with the sound-only
  variant instead of the default.
- **FR-011**: A variant selection MUST persist across sessions.
- **FR-012**: Changing the installed variant MUST inform the user that a machine
  reset is required, consistent with existing hardware-configuration changes.
- **FR-013**: The Hardware tab MUST identify which Mockingboard model is
  installed in the slot.

#### Compatibility

- **FR-014**: Software written for a sound-only Mockingboard MUST behave
  identically on the sound+speech card, in audio output and in timing.
- **FR-015**: The voice chip MUST be quiescent at power-on and at every reset —
  producing no audio and asserting no interrupt — until software programs it.
- **FR-016**: The voice chip MUST NOT introduce any interrupt that software did
  not arm.
- **FR-017**: Mockingboard detection routines MUST succeed against the
  sound+speech card exactly as they do against the sound-only card.

#### Audio integration

- **FR-018**: Speech MUST mix into the machine's existing audio output alongside
  the card's music channels, the built-in speaker, and drive audio, and MUST be
  subject to the master volume and mute controls.
- **FR-019**: The combined output of speech, music, speaker, and drives MUST NOT
  clip at peak levels.
- **FR-020**: An idle or unprogrammed voice chip MUST contribute exactly zero to
  the mix and MUST NOT perform measurable synthesis work.
- **FR-021**: A reset MUST silence in-progress speech immediately.

#### Release

- **FR-022**: The release notes and user-facing documentation MUST state that the
  emulated Mockingboard is now the sound+speech model by default, and how to
  select the sound-only card.

### Key Entities

- **Mockingboard variant**: Which model of the card is installed in a slot —
  sound-only or sound+speech. A property of a machine's configuration, persisted
  with it, and changeable only across a machine reset.
- **Voice chip**: The speech synthesizer present on the sound+speech variant.
  Owns the phoneme currently sounding, the inflection/rate/amplitude controls,
  and a ready/request state that software can poll or take as an interrupt.
- **Utterance**: The sequence of phonemes software feeds the chip one at a time,
  paced by the chip's readiness. Has no existence in hardware beyond the current
  phoneme plus the chip's state — the emulator must not "know" a whole sentence.
- **Card audio contribution**: What a card adds to the machine's mix each audio
  slice. For the sound+speech card this is the music channels plus speech;
  exactly zero when nothing on the card is active.

## Success Criteria *(mandatory)*

### Measurable Outcomes

Speech quality is verified at three levels rather than one. A single
intelligibility bar was considered and rejected: the voice chip is an early-1980s
formant synthesizer whose authentic output is genuinely rough, so an absolute
transcription target can be *passed by being better than the hardware* — which is
a fidelity failure the criterion would have rewarded. The three criteria below
separate what a machine can check forever, what a person must judge once, and
what only real software exercises.

- **SC-001a** *(automated, permanent)*: For every phoneme the chip supports, the
  rendered output carries its documented acoustic content — the formant
  frequencies named in the datasheet, for the documented duration — and the rate,
  inflection, and amplitude controls shift that content in the documented
  direction. Verified by assertion in the test suite with no audio fixtures and
  no listener, in the manner the existing tone-frequency and DAC-monotonicity
  checks already verify the music chip.
- **SC-001b** *(human, one-time, comparative)*: Transcription accuracy on Casso's
  speech is within 10 percentage points of transcription accuracy on a reference
  rendering of the same utterance, judged by listeners who have not seen the
  script. Stated as a margin rather than an absolute so that neither unusually
  clear nor unusually rough output can pass by diverging from the hardware.
- **SC-001c** *(per-title)*: In every title of the speech acceptance set, speech
  occurs at the moments the software intends, the software's phoneme-pacing loop
  advances to the end of each utterance, and no title stalls or hangs waiting on
  the chip. Verifiable even where the synthesized voice is hard to make out,
  because it measures the software's interaction with the chip rather than the
  audio.
- **SC-002**: Every title in the sound-only regression set produces audio
  indistinguishable from the previous release, verified by automated comparison
  of rendered output rather than by ear alone.
- **SC-003**: 100% of the existing Mockingboard, VIA, PSG, and interrupt tests
  pass unchanged, with no test modified to accommodate the new chip.
- **SC-004**: A machine idling with the sound+speech card installed and no
  speech software running shows no measurable increase in CPU cost over the same
  machine with the sound-only card.
- **SC-005**: A music player driving the card's timer interrupt runs for at least
  10 emulated minutes with the voice chip present and unprogrammed, receiving
  zero interrupts it did not arm.
- **SC-006**: Speech begins within one video frame of the software requesting it,
  so there is no perceptible lag between an on-screen event and its narration.
- **SC-007**: Switching a machine between the two variants and back leaves the
  machine behaving exactly as it did before the round trip.
- **SC-008**: No combination of speech, music, speaker, and drive audio at full
  volume produces clipping.

## Assumptions

- **The chip modeled is the SSI-263**, the synthesizer on the later
  speech-equipped Mockingboards and the one essentially all surviving Apple II
  speech software targets. The earlier Votrax SC-01 found on the first speech
  boards is out of scope; if a title turns out to require it, that is a separate
  feature and a separate variant.
- **Applied Engineering's Phasor is out of scope.** It carries speech but also a
  mode-switch latch and a different card personality; it is its own feature.
- **Mockingboard D (the //c external unit) remains out of scope**, as it was in
  GH #66. The //c has no slots and this feature does not change that.
- **The Apple ][ (non-plus) profile is unaffected** — it ships no Mockingboard
  today and continues not to.
- **Naming follows the product line**, so users see "Mockingboard A" and
  "Mockingboard C" rather than a chip part number. The card Casso ships today is
  described internally as an A/C; with speech present, the C name becomes
  accurate rather than aspirational.
- **Validation is tiered so that acquiring period software gates as little as
  possible.** A purpose-written boot-sector demo that drives the voice chip with
  an authored phoneme sequence supplies perfect ground truth for SC-001a and
  needs no disks at all; the project already carries a directly analogous tone
  smoke test for the music chip, and the speech equivalent belongs beside it.
  Only SC-001c genuinely requires period titles, and only for sign-off of User
  Story 1.
- **A reference rendering for SC-001b need not come from physical hardware.**
  Comparing the same authored phoneme sequence against another emulator's output
  is behavioral comparison, not derivation, and stays within the clean-room rule
  in FR-002 — no third-party source is read or copied. A recording of real
  hardware is preferable where one can be obtained, but is not a prerequisite.
- **The speech acceptance set will be assembled from period titles known to drive
  Mockingboard speech**, with the manufacturer's own demo and utility software
  preferred where available: it was written to exhibit the chip and often speaks
  a documented phrase, which supplies a transcript that game software does not.
  The set cannot be synthesized from Casso's own output — validating a speech
  synthesizer against speech it generated proves nothing.
- **The sound-only regression set is drawn from the titles already used to
  validate GH #66** — the music and effects titles named in its acceptance
  criteria — so SC-002 compares against known-good prior behavior.
- **Existing card infrastructure is reused rather than rebuilt.** The card's
  timer/port emulation, its interrupt path to the CPU, and its stereo audio path
  are all in place and working; this feature adds a chip to that card and a
  variant selection around it.
- **The variant is selected by machine configuration in this feature.** A
  follow-on feature makes each slot's occupant selectable from the Hardware tab
  with period-typical default slot assignments; the two are sequenced
  independently and neither blocks the other.

## Dependencies

- **GH #66 (closed, shipped in 1.7.0)** delivered the card, its timer/port
  emulation, its interrupt wiring, and its stereo audio path. This feature
  extends that work and explicitly picks up the item that issue deferred.
- **A purpose-written speech smoke test** — a boot-sector program that
  unconditionally programs the voice chip with an authored phoneme sequence, in
  the mold of the existing Mockingboard tone smoke test. This is the artifact
  that unblocks User Story 1 development: it is repo-original, needs no disk
  acquisition, and carries its own ground truth. It should exist early rather
  than as a polish task.
- **Period speech software** — required only for SC-001c and only at User Story 1
  sign-off, not for development. This is the one input the codebase cannot supply
  on its own, and the reason SC-001a and SC-001b are deliberately structured not
  to depend on it. Acquired disk images belong in the primary checkout rather
  than a worktree, as existing test media do.
- **No new third-party dependency is anticipated.** Should one be proposed, the
  constitution's Approved Third-Party Dependencies allowlist governs, and adding
  to it is a constitution amendment.
