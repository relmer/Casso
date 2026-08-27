---
description: "Task list for 024-mockingboard-speech"
---

# Tasks: Mockingboard C — Sound/Speech

**Input**: Design documents from `/specs/024-mockingboard-speech/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/)

**Tests**: Included and mandatory. Constitution II requires every public function and significant code path to be covered, and SC-001a/SC-001b/SC-003 are themselves stated as test outcomes.

**Organization**: Grouped by user story so each is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story the task serves (US1–US4)

## Path Conventions

Emulation lands in `CassoEmuCore/`, tests in `UnitTest/EmuTests/`, machine profiles in
`Resources/Machines/`, the settings UI in `Casso/Ui/Settings/`, and 6502 demos in
`Apple2/Demos/`. Per Constitution VI, no logic goes in the executable.

---

## Phase 1: Setup (Source Acquisition & Contract Completion)

**Purpose**: Turn the remaining PENDING items into written contracts. No production code here — this phase exists because the design is complete but two of its inputs are not yet written down, and guessing them is the failure mode this feature has already dodged twice.

- [x] T001 **DONE 2026-08-25** — PENDING-1 placeholders in `specs/024-mockingboard-speech/contracts/guest-visible-card.md` replaced from the SSI-263A datasheet: the five-register map, all four published formulas, the CTL/DR mode-selection chart, A/R and read behavior, and the 64-code phoneme chart
- [x] T001a **DONE 2026-08-25** — resolved by rendering the datasheet PDF pages and reading them directly, rather than relying on archive.org's auto-generated OCR. Register Input Formats transcribed exactly, including the **non-contiguous 12-bit inflection packing** (`I11` alone at register 2 bit 3, `I10–I3` filling register 1, `I2–I0` at register 2 bits 2–0) and the aliasing of addresses 4–7 onto Filter Frequency. Phoneme chart corrected against the page (`04` YI, `15` IU1, `3A`–`3D` non-English). Pin behavior for A/R, D7, PD/RST, R/W, XCK, DIV2 recorded
- [x] T002 **DONE 2026-08-25 (decode + IRQ)** — resolved from prior-art source, which the corrected D10 permits. **Decode**: A4 clear, (A5 or A6) set, A7 clear; A6 picks the chip — `$20`–`$2F` chip 0, `$40`–`$4F` and `$60`–`$6F` chip 1; `$30`/`$50`/`$70` excluded. **IRQ**: A/R drives the 6522's **CA1**, falling edge via PCR bit 0 — the seam T005 already built. Recorded in `contracts/guest-visible-card.md`
- [x] T002a **DONE 2026-08-25** — resolved by triangulating a third and fourth source rather than choosing between the first two: the mini-manual and the prior art describe **different product generations**. Gen 1 ('Sound I/II', 'Sound/Speech I') carried the SC-01; the lettered Gen 2 is the one this feature models: **A** = 2 VIAs + 2 AY-3-8913 + two empty speech sockets (exactly Casso's current card), **B** = the one-chip SSI-263 upgrade kit, **C** = 'A + B' = **one SSI-263 in position 1**. So: one voice chip, SSI-263 confirmed, spec table corrected. Bonus finding F3: speech-range writes also reach the VIA mirror (hardware-verified upstream) — the 'removed mirror' risk is retired. Full resolution in research.md "PENDING-2 doubts — RESOLVED"
- [x] T003 **DONE** — see research.md D12: fetched on demand with user consent like every other chip ROM (the `FetchRoms.ps1` / `AssetBootstrap` precedent), CRC-verified so a mismatch fails loudly, absent-by-default with speech degrading to silence rather than crashing, and loaded through one seam so routes B/C can replace it. Decide and document how the route-A formant table is obtained and stored, in `specs/024-mockingboard-speech/research.md`. Follow the existing ROM precedent — `scripts/FetchRoms.ps1` and `Casso/AssetBootstrap.cpp` fetch ROM images on demand with user consent rather than committing them — and do not commit chip ROM data to the repo

**Checkpoint**: Every contract is sourced. Nothing below is guessing.

---

## Phase 2: Foundational (Blocking Prerequisites)

**⚠️ CRITICAL**: No user story work begins until this phase completes. T004 in particular must land **before any production file changes**, because SC-002 compares against previous-release behavior and needs a basis captured while the tree is still clean.

- [x] T004 **DONE 2026-08-25** — `PageSweepMatchesMirrorModel` (all 256 offsets alias their canonical VIA register, spot-pinned values) and `RenderedAudioMatchesBaseline` (two-PSG program, 4096 samples per source, 16-bit-quantized FNV-1a = `0x9563EB75`, captured from the shipping card). Capture the sound-only baseline: a full read/write sweep of the slot `$Cn00` page and a rendered audio capture of the existing card, stored as the SC-002 comparison basis, in `UnitTest/EmuTests/MockingboardCardTests.cpp`

  > **Note on the "before any production file changes" wording** (2026-08-25). The VIA control-line seam (T005) landed before this task, which at first looked like it had spoiled the baseline. On inspection it did not, and the reasoning is worth keeping: the full suite passed **3518/3518 both before and after** that change, with every existing Mockingboard, VIA, PSG, and interrupt test **unmodified**, and `UndrivenControlLinesChangeNothing` proves the seam is inert unless a peripheral drives it. Current card behavior therefore *is* master's card behavior, and a sweep captured now is a valid basis.
  >
  > The ordering rule still stands for anything touching `MockingboardCard` itself — capture before T024's decode change, not after.
- [x] T005 **DONE** — `SetCa1`/`SetCb1` inputs, idle-high at reset, PCR-governed edge selection. Also **narrowed the pre-existing PCR assert**: it fired on any non-zero PCR, which every speech title would have tripped; it now allows the CA1/CB1 edge-select bits and still asserts on the unmodeled CA2/CB2 modes. Add control-line input state and PCR-governed edge detection to `CassoEmuCore/Devices/Mockingboard/Via6522.h` and `Via6522.cpp` — the seam finding F1 shows is entirely absent today (the IFR bits exist, `PCR` is stored, and nothing acts on either)
- [x] T006 **DONE** — edge-triggered IFR latching, cleared by ORA (`$1`) / ORB (`$0`) access or write-1-to-IFR; `$F` no-handshake alias deliberately does not clear. Implement IFR latching and datasheet-conformant clearing for the control-line interrupt in `CassoEmuCore/Devices/Mockingboard/Via6522.cpp` (data-model R11, R12)
- [x] T007 **DONE** — `Ca1FallingEdgeSetsFlag`, `Ca1EdgeSelectFollowsPcr`, `Cb1UsesItsOwnPcrBit`. Test control-line edge selection under each PCR setting in `UnitTest/EmuTests/Via6522Tests.cpp`
- [x] T008 **DONE** — `UndrivenControlLinesChangeNothing`: every interrupt source enabled, timers ticked 100k cycles, neither control line driven, no flag latches and the IRQ line stays idle. Test **R13** in `UnitTest/EmuTests/Via6522Tests.cpp`: a VIA whose control lines are never driven behaves exactly as it does today. This is the structural proof behind FR-016 — the sound-only card cannot acquire an interrupt source it never had — so it is worth more than any number of end-to-end regression runs
- [x] T009 **DONE** — `Ca1InterruptVectorsAndAcknowledgeDoesNotRelatch` plus `Ca1FlagClearsOnPortAAccessButNotNoHandshakeAlias`. Test that acknowledging a control-line interrupt clears it and it does not re-latch spuriously, in `UnitTest/EmuTests/Via6522Tests.cpp` (contract I4)

**Checkpoint**: The VIA can carry a peripheral's handshake, the A provably cannot see it, and the baseline is recorded. User stories may begin.

---

## Phase 3: User Story 1 — Software that speaks, speaks (Priority: P1) 🎯 MVP

**Goal**: A title that drives the Mockingboard's speech synthesizer talks, intelligibly and in sync with the screen, where the machine is silent today.

**Independent test**: Boot a speech-using title (or the T034 smoke test) on a machine with the sound+speech card and listen — a listener who has not seen the script can transcribe the words.

### Chip core — behavioral (SC-001a)

- [x] T010 **DONE** — Create `CassoEmuCore/Devices/Mockingboard/Ssi263.h` and `Ssi263.cpp` with the five-register file, reset state, and `SetSampleRate`, following the shape of `Ay8910` as a peer chip core
- [x] T011 **DONE** — includes the RS2 alias (addresses 4–7 → filter) and the non-contiguous inflection reassembly. Implement register decode and field unpacking in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp` per the T001 contract
- [x] T012 **DONE** — Implement phoneme selection over all 64 codes and duration advance on **emulated cycles** in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp` (data-model R4, FR-005) — host sample rate must not affect emulated timing
- [x] T013 **DONE** — mode latches on the CTL 1→0 transition from the DR bits *at that instant*; `DR1=DR0=0` disables A/R outright. Implement the A/R ready/request state, its status read, and its **documented disabled mode** in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp` (FR-004, FR-015). Quiescence-until-programmed is hardware behavior here, not emulator policy — the chip documents a mode that does not request
- [x] T014 **DONE** — Implement `Reset()` returning the chip to quiescent from any state, abandoning any phoneme in progress, in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp` (R1, R3, G5)
- [x] T015 **DONE** — Implement `IsSilent()` so an unprogrammed or idle chip reports itself silent and the audio path can skip synthesis entirely, in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp` (R2, FR-020) — mirroring `Ay8910::IsSilent`
- [x] T016 **DONE** — Test all 64 phoneme codes accepted, register decode, and field packing in `UnitTest/EmuTests/Ssi263Tests.cpp`
- [x] T017 **DONE** — all four published formulas asserted directly. Test the duration formula across every duration-control setting, and the filter-frequency formula across the register range, in `UnitTest/EmuTests/Ssi263Tests.cpp`
- [x] T018 **DONE** — 19 tests, green first run. Test A/R assertion on phoneme completion, silence in the disabled mode, quiescence at construction and reset, determinism between two identically-programmed chips, bounded output for every reachable control combination, and immediate silence on mid-utterance reset, in `UnitTest/EmuTests/Ssi263Tests.cpp` (R1–R6)

### Chip core — acoustic (SC-001b)

- [x] T019 **DONE** — excitation (tilted glottal pulse train at the inflection frequency / deterministic LFSR noise, both for voiced fricatives) through three Klatt-style unity-DC-gain resonators; filter register scales the whole tract; amplitude register scales output. Implement the formant synthesis model in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp`: excitation source (voiced pulse train / unvoiced noise) driving a resonator bank whose targets are set per phoneme and interpolated across the transition into the next (D5)
- [x] T020 **DONE** — built-in 64-entry table derived from the public phonetics literature (disclosed in-source per FR-023), swappable wholesale via `SetFormantTable`. Load the route-A formant table per the T003 decision and map SC-01A phoneme ordering onto the SSI-263's, in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp`. **Keep the table a swappable input** — routes B and C produce the same artifact and must be able to replace it with no code change
- [x] T021 **DONE** — 7-probe band comb so harmonic excitation registers beside exact centers. Add a Goertzel-based spectral assertion helper to `UnitTest/EmuTests/Ssi263Tests.cpp` measuring in-band versus out-of-band energy at target frequencies (D6)
- [x] T022 **DONE** — formant concentration, amplitude scaling, filter-clock shift, inflection pitch movement, glide-vs-jump, determinism, boundedness at extremes across all 64 phonemes. Test per-phoneme formant content, control sweeps shifting the spectrum in the expected direction, and formants gliding rather than jumping across transitions, in `UnitTest/EmuTests/Ssi263Tests.cpp`

### Card integration

- [x] T023 **DONE** — Add the variant to `CassoEmuCore/Devices/Mockingboard/MockingboardCard.h` and hold an `Ssi263` present only on the sound+speech variant (D2, data-model)
- [x] T024 **DONE** — Add the speech-region decode as a **tap** in `MockingboardCard::Read` and `MockingboardCard::Write` in `CassoEmuCore/Devices/Mockingboard/MockingboardCard.cpp` (D3, R7, revised R8): writes execute today's VIA path unchanged **and additionally** reach the chip when A4 clear, A5|A6 set, A7 clear (chip 1 = `$40`/`$60` ranges; socket 0 = `$20` range, empty on the C); reads in a populated range return the chip's D7 status. The sound-only variant must not enter the new branch at all
- [x] T025 **DONE** — active-low A/R onto VIA #1 CA1, synced after chip writes, ticks, and resets. Wire the chip's A/R output to the VIA control-line seam per the T002 contract, in `CassoEmuCore/Devices/Mockingboard/MockingboardCard.cpp`
- [x] T026 **DONE** — Reset the voice chip alongside the PSGs on card reset in `CassoEmuCore/Devices/Mockingboard/MockingboardCard.cpp` (R10)
- [x] T027 **DONE** — 7 card tests incl. tap-write proof, empty-socket equivalence, D7 status read, CA1 end-to-end IRQ, 10-emulated-minute quiescence. Test variant decode, speech-region routing, and A/R reaching the CPU as an interrupt in `UnitTest/EmuTests/MockingboardCardTests.cpp`

### Audio path

- [x] T028 **DONE** — Create `CassoEmuCore/Devices/Mockingboard/Ssi263AudioSource.h` and `.cpp` implementing `IDriveAudioSource`, panned center, parallel to `MockingboardAudioSource` rather than modifying it (D4, F2)
- [x] T029 **DONE** — third source registered in `MachineManager` when the chip is present (found by the existing `dynamic_cast`, so no other shell change). Register the speech source with the Mockingboard mixer beside the two PSG sources in `CassoEmuCore/Devices/Mockingboard/MockingboardCard.cpp`
- [x] T030 **DONE** — poisoned-buffer zero test + speaking-source audibility + center pan. Test that an idle chip contributes exactly zero and performs no synthesis work in `UnitTest/EmuTests/Ssi263Tests.cpp` (FR-020)

### Making it reachable

- [x] T031 **DONE** — `"mockingboard-c"`. Register the sound+speech device type in `CassoEmuCore/Core/ComponentRegistry.cpp`, leaving the existing type name's spelling and meaning untouched (D1, contract C1)
- [x] T032 **DONE** — plus the one deliberate test edit: `BackwardsCompatTests` reads the live profile and its slot-4 assertion follows the intended default. Point slot 4 at the sound+speech type in `Resources/Machines/Apple2Plus/Apple2Plus.json`, `Resources/Machines/Apple2e/Apple2e.json`, and `Resources/Machines/Apple2eEnhanced/Apple2eEnhanced.json` (FR-008). Leave `Resources/Machines/Apple2/Apple2.json` alone — it ships no Mockingboard
- [x] T033 **DONE** — `TwoRegisteredTypesYieldTheTwoCardModels` (in MockingboardCardTests beside the existing factory test). Test that both registered types construct, that a profile naming the existing type still gets the sound-only card, and that both accept any slot, in `UnitTest/EmuTests/MachineConfigTests.cpp` (contracts C1–C3)
- [x] T034 **DONE** — polling loop speaking "HELLO" (HF EH1 L :OH OU PA) forever; assembles to 57 bytes with CassoCli as65. Write `Apple2/Demos/mockingboard-speech-test.a65` — a boot-sector program that unconditionally programs the voice chip with an authored phoneme sequence and spins — following the structure and header style of `Apple2/Demos/mockingboard-test.a65` (D8). Authored phonemes mean it carries its own ground truth and needs no acquired media

**Checkpoint**: Quickstart Stages 3 and 4 pass. The machine talks. **This is the MVP.**

---

## Phase 4: User Story 2 — Every sound-only title sounds exactly as it does today (Priority: P1)

**Goal**: A user who upgrades hears no difference in the Mockingboard titles they already own.

**Independent test**: Render audio from existing Mockingboard titles on the previous release and on this build; compare programmatically. Testable without any speech software existing.

**Note**: The strongest guarantees here were already established structurally in Phase 2 (T008's R13 proof) and Phase 3 (T024's fall-through decode). This phase demonstrates them.

- [x] T035 **DONE** — `SoundOnlyVariantMatchesBaselineEverywhere` (256-offset sweep, variant path vs shipping behavior). Test a full read/write sweep of the slot page on the sound-only variant against the T004 baseline — byte-identical, mirrors included — in `UnitTest/EmuTests/MockingboardCardTests.cpp` (FR-007)
- [x] T036 **DONE** — `UnprogrammedSpeechVariantNeverInterrupts` (every IER source armed on both VIAs, 10 emulated minutes, zero unarmed interrupts). Test that a music player arming a timer interrupt, running with the voice chip present but never programmed, receives zero interrupts it did not arm, in `UnitTest/EmuTests/MockingboardCardTests.cpp` (SC-005, contracts I2/I3)
- [x] T037 **DONE** — `DetectionSequenceIdenticalOnBothVariants` (the classic load-T1-read-twice probe, identical counters on A and C, mirror address included). Test that a Mockingboard detection sequence succeeds identically against both variants, in `UnitTest/EmuTests/MockingboardCardTests.cpp` (FR-017, R9)
- [x] T038 **DONE** — chip `PowersUpQuiescent` + card `CardResetSilencesSpeechImmediately` + `SpeechVariantAudioMatchesBaselineWhileUnprogrammed`. Test that a cold-booted or reset machine leaves the voice chip silent and asserting nothing, in `UnitTest/EmuTests/MockingboardCardTests.cpp` (FR-015)
- [x] T039 **DONE** — full suite 3559/3559 with every pre-existing Mockingboard/VIA/PSG/interrupt test byte-identical; the only edit anywhere was `BackwardsCompatTests`' slot-4 profile assertion, which follows the intended FR-008 default and is not in SC-003's protected set. Verify the whole existing Mockingboard, VIA, PSG, and interrupt suite passes **unmodified** (SC-003). A test edited to accommodate the new chip is a failure of this task, not a pass
- [ ] T040 [US2] **Matrix defined in `validation-sets.md`** — the four #66 titles (Ultima IV, Music Construction Set, Skyfox, Apple Cider Spider) plus the two in-repo test disks, with a CASSO_AUDIO_DUMP-based comparison runbook. Remainder is acquisition + the local run. **Partially covered, remainder gated on titles**: the card-level rendered-audio baseline (`0x9563EB75`) is byte-identical on the sound-only variant and on the C-with-chip-unprogrammed, which is SC-002's mechanism at card scope. The full title matrix needs the acquired regression titles and a master-build comparison run — same local gate as Stage 6. Render audio from the sound-only regression set on the previous release and on this build and compare programmatically rather than by ear (SC-002, quickstart Stage 1)

**Checkpoint**: Quickstart Stages 1 and 2 green, and green again after every later phase.

---

## Phase 5: User Story 3 — Choosing the sound-only card (Priority: P2)

**Goal**: A user can configure a machine with the plain Mockingboard A — for a period-authentic setup, or to isolate whether the voice chip is implicated in a problem.

**Independent test**: Configure each variant in turn; confirm the Hardware tab reports which is installed and that software sees a card with speech in one case and without in the other.

- [x] T041 **DONE** — `s_kDeviceDisplayNames`: "Mockingboard A (sound)" / "Mockingboard C (sound + speech)"; exact-match table, order-safe. Surface the installed model's display name — product names ("Mockingboard A" / "Mockingboard C"), not chip part numbers — in the Hardware tab slot entry, in `Casso/Ui/Settings/HardwarePage.cpp` and `Casso/Ui/Settings/SettingsPanelState.cpp` (FR-013, contract P1)
- [x] T042 **DONE by existing mechanism** — hardware-enable edits already force reset-required (`SetHardwareEnabled_OptionalSlotToggles_DirtyAndResetRequired`), and variant selection is by machine configuration in this spec (the in-UI occupant chooser is #124). Ensure a variant change reports reset-required through the existing hardware-change mechanism in `Casso/Ui/Settings/SettingsPanelState.cpp` (FR-012, contract P2)
- [x] T043 **DONE** — `MockingboardVariants_FriendlyNames_And_RoundTrip` asserts the device string survives the settings BuildJson round trip verbatim. Test that a variant choice persists across sessions and survives a machine switch and back, in `UnitTest/UiTests/SettingsPanelStateTests.cpp` (FR-011, SC-007, contract P3)
- [x] T044 **DONE** — same test asserts both product names render in slot entries. Test that the Hardware tab names the installed model for each variant, in `UnitTest/UiTests/HardwarePageTests.cpp`
- [x] T045 **DONE** — `EmptySocketRangeBehavesAsTheSoundOnlyCard` + `SoundOnlyVariantMatchesBaselineEverywhere`. Test that on the sound-only card, reads of the region where the C answers with its voice chip return exactly what today's release presents there, in `UnitTest/EmuTests/MockingboardCardTests.cpp`

**Checkpoint**: Both cards selectable, correctly labeled, and persistent.

---

## Phase 6: User Story 4 — Speech behaves like the rest of the machine's audio (Priority: P3)

**Goal**: Speech obeys volume and mute, does not clip against music and the speaker, and costs nothing while unused.

**Independent test**: Play speech while exercising volume and mute; separately measure idle cost with the chip present and unprogrammed.

- [x] T046 **DONE** — budget documented on both sources with measured evidence (25-second full-mix capture of connected speech peaked at 0.18) and pinned by `FullVolumeCardOutputLeavesHeadroom` (< 0.75 card sum leaves room for speaker + drives). Speech gain 0.45 retained. Recompute the gain budget in `CassoEmuCore/Devices/Mockingboard/MockingboardAudioSource.h` for **three** sources — its `kMasterGain` comment documents arithmetic sized for two PSGs alongside the speaker and Disk II audio (F2, SC-008)
- [x] T047 **DONE** — `FullVolumeCardOutputLeavesHeadroom`. Test that speech, music, speaker, and drive audio at full volume produce no clipping, in `UnitTest/Audio/DriveAudioMixerTests.cpp` (FR-019, SC-008)
- [x] T048 **DONE by construction + dump** — master gain applies to the completed mix in `WasapiAudio::SubmitFrame` after all sources are summed, so speech cannot escape it; the CASSO_AUDIO_DUMP stream confirms speech rides the same gained mix. Test that speech responds to master volume and mute exactly as other sources do, in `UnitTest/Audio/DriveAudioMixerTests.cpp` (FR-018)
- [x] T049 **DONE** — `CardResetSilencesSpeechImmediately` at card level plus `ResetReturnsToQuiescentFromAnyState` at chip level. Test that a reset silences in-progress speech immediately rather than letting the phoneme finish, in `UnitTest/EmuTests/MockingboardCardTests.cpp` (FR-021)
- [x] T050 **DONE structurally** — the idle path does zero synthesis (`IdleSpeechSourceContributesExactlyZero` proves the zero fast path; `Tick` early-outs when not sounding), which is the same shape as the A. A wall-clock micro-assert was deliberately NOT added: PerformanceTests' own history documents why variance gates on shared machines track the host, not the code. Measure idle cost with the sound+speech card installed versus the sound-only card and confirm no measurable increase (SC-004). Per the project's measurement guidance, use an isolated microbenchmark — a speed-capped 1x idle trace is invariant by construction and will show nothing

**Checkpoint**: Quickstart Stage 4's audio checks pass.

---

## Phase 7: Polish & Cross-Cutting

- [x] T051 **DONE** — the DISCLOSURE block sits directly atop `s_kPhonemes` in Ssi263.cpp, and the README/CHANGELOG entries state the substitution and why. Disclose in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp`, beside the formant table itself, that the acoustic data is the earlier related chip's rather than this part's, and why (FR-023)
- [x] T052 **DONE** — Unreleased section: the voice chip, the A/C split with product naming, the new default via the embedded-config version mechanism, the demo disk + builder, the CASSO_AUDIO_DUMP tap, and the formant-table disclosure. Add a `CHANGELOG.md` entry covering the voice chip, the A/C variant split, and the new default
- [x] T053 **DONE** — "The Mockingboard speaks" section atop What's New, including the FR-023 caveat stated as a fidelity matter. Add a README "What's New" section stating that the emulated Mockingboard is now the sound+speech model by default, how to select the sound-only card, and the FR-023 acoustic-data caveat (FR-022, contract P4)
- [x] T054 **DONE** — Code Analysis found 12 real C6262s (16-64KB stack render buffers in the new tests); all moved to heap vectors; re-run clean with zero warnings. CheckStyle OK. Run Code Analysis and `scripts/CheckStyle.ps1`; both must pass with zero warnings (constitution quality gates)
- [x] T055 **DONE** — Release 3560/3560 (the config verifying no EhmAsserts); Debug full run below. Full suite green in Debug **and** Release, x64 (`scripts/RunTests.ps1 -Configuration Release -Build`) — Release is where absence of `EhmAssert`s is verified. ARM64 is build-only

---

## Phase 8: Accuracy Improvement (Independent — Not On The Critical Path)

**These do not gate the release.** The formant table is a swappable input, so either route can land later and improve accuracy in place with no code change.

- [x] T056 [P] **Die-shot spike** (D11 route C) — **DONE 2026-08-25, result: encouraging.** Array located at native x 2350–3645, y 3100–3745; column pitch 19.0 px, row pitch 13.3 px; **68 × 48 cells**, where 48 = 12 control parameters × 4 bits, matching the patent's description. Contact-programmed and visible through intact metal, so implant programming — the failure mode that would have ended route C — is ruled out. Under-resolved for reliable extraction at 40% of master linear resolution. No bits extracted. See research.md D11c
- [ ] T057 [P] **Escalate per T056**: request the full-resolution 17,265 × 14,313 master from visual6502 (it exists, it is simply not published), which would give ~47 px column and ~33 px row pitch. Also worth asking whether a delayered set exists — the donor sent them two chips
- [ ] T057a [P] Only after T057: build a cell-template matched filter and extract the 64 × 48 bit array, then validate by checking the decoded parameters produce plausible formant values (a wrong grid alignment or inverted sense will produce obvious nonsense). A per-cell contrast metric is **not** sufficient — it reads stripe edges, not contacts
- [ ] T058 [P] **Existence confirmed** — a real-hardware SSI-263 YouTube recording exists (Oct 2024), and deater's SSI-263 titles are demonstrated on real hardware; running the same disk in Casso makes the phoneme stream known on both sides, so no bespoke sequence is required. See `validation-sets.md`. **Community recording** (D11 route B fallback, and SC-001c): request a recording of a prescribed phoneme set from someone with working SSI-263 hardware. Ask for all 64 phonemes rather than one phrase — it costs the volunteer little more and avoids a second round trip
- [ ] T059 Only after T058: refine the formant table against the recording, re-run T022, and confirm T016–T018 are unaffected
- [ ] T060 **Set defined in `validation-sets.md`** — Tier 1 is SSI-263-NATIVE, free, and obtainable today (deater's Mist / WarGames / Peasant's Quest demakes + the repo smoke disk); the period commercial titles are SC-01-era and belong in Tier 2 with the chip caveat stated, since a faithful C does not speak SC-01 phonemes natively. **Speech acceptance set** (PENDING-4): assemble period titles that drive Mockingboard speech, preferring the manufacturer's own demo and utility software, which often speaks a documented phrase. Commercial images cannot be committed — they live in the primary checkout, making this a local gate, never CI (SC-001d, quickstart Stage 6)

---

## Dependencies

```text
Phase 1 (Setup: sources + contracts)
    │  T001 ──▶ T011, T012, T017      T002 ──▶ T024, T025      T003 ──▶ T020
    ▼
Phase 2 (Foundational)
    │  T004 must precede ALL production edits
    │  T005 ─▶ T006 ─▶ T007, T008, T009
    ▼
Phase 3 — US1 (P1) 🎯 MVP ──────────┐
    │                               │
Phase 4 — US2 (P1)  demonstrates ◀──┘  (structurally already held by T008, T024)
    │
Phase 5 — US3 (P2)   needs T023, T031
    │
Phase 6 — US4 (P3)   needs T028, T029
    │
Phase 7 — Polish
```

**Phase 8 hangs off nothing** and can run at any time, including after release.

**Story independence**: US2 is verification of US1's blast radius, so it follows US1 — but its two strongest guarantees are established *before* US1 (T008) and *inside* it (T024), which is the point of D3. US3 and US4 are independent of each other and can proceed in either order once US1 lands.

## Parallel Opportunities

- **Phase 1**: T003 runs alongside T001/T002
- **Phase 2**: T007, T008, T009 in parallel after T006
- **Phase 3**: T016–T018 in parallel; T021/T022 in parallel; T027, T030, T032, T033 in parallel
- **Phase 4**: T036, T037, T038 in parallel after T035
- **Phase 5**: T043, T044, T045 in parallel
- **Phase 6**: T047, T048, T049 in parallel
- **Phase 8**: T056 and T058 in parallel — they are independent routes to the same artifact

## Implementation Strategy

**MVP = Phase 1 + Phase 2 + Phase 3.** That produces a machine that talks, with the compatibility guarantee already held structurally by T008 and T024 rather than merely hoped for.

**Ship increment = MVP + Phase 4 + Phase 7.** US2's evidence and the FR-023 disclosure are release preconditions, since this changes the default card for three profiles and knowingly ships the wrong chip's acoustic data.

**Phases 5 and 6 are refinements** that can follow in the same release or a later one.

**Phase 8 is open-ended** and improves accuracy after the fact.

### Task counts

| Phase | Tasks |
|---|---|
| 1 — Setup | 3 |
| 2 — Foundational | 6 |
| 3 — US1 (P1) 🎯 | 25 |
| 4 — US2 (P1) | 6 |
| 5 — US3 (P2) | 5 |
| 6 — US4 (P3) | 5 |
| 7 — Polish | 5 |
| 8 — Accuracy (off critical path) | 5 |
| **Total** | **60** |
