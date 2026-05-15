---
description: "Disk II Audio — actionable, dependency-ordered tasks"
---

# Tasks: Disk II Audio

**Input**: Design documents from `/specs/005-disk-ii-audio/`
**Prerequisites**: spec.md, research.md, plan.md
**Constitution**: `.specify/memory/constitution.md`

**Tests**: Required throughout. Every production-code task that has
observable behavior has a paired test task. Tests use in-memory sample
buffers and a mock `IDeskAudioSink` — no host filesystem reads, no audio
device, per constitution §II.

**Organization**: Phases follow `research.md` §6.3's recommended
implementation order. Each phase ends with an explicit `[GATE]` task that
re-asserts pre-existing-tests-still-green and speaker-only behavior is
unchanged (SC-006 / FR-011).

## Format: `[ID] [P?] [GATE?] Description`

- **[P]** — parallel-safe within its phase (different files, no shared-edit conflict)
- **[GATE]** — phase-completion gate task; not parallel
- Every task cites the FR(s) it satisfies and the file(s) it touches
- Every production-code task is paired with (or precedes) a test task

## Constitution rules — apply to every code task

1. Function comments live in `.cpp` (80-`/` delimiters), not in the header
2. Function/operator spacing: `func()`, `func (a, b)`, `if (...)`, `for (...)`
3. All locals at top of scope, column-aligned (type / `*`-`&` column / name / `=` / value)
4. No non-trivial calls inside macro arguments — store result first
5. ≤ 50 lines per function (100 absolute); ≤ 2 indent levels beyond EHM (3 max)
6. Exactly 5 blank lines between top-level constructs; exactly 3 between top-of-function declarations and first statement
7. EHM pattern (`HRESULT hr = S_OK; … Error: … return hr;`) on every fallible function
8. No magic numbers (except 0/1/-1/nullptr/sizeof) — see `plan.md` for the named-constants list

---

## Phase 0: Contracts & Declarations

**Purpose**: Stand up the interface and class declarations. No behavior yet.

- [ ] T001 [P] Create `CassoEmuCore/Audio/` directory and add a placeholder
      `README.md` documenting that this directory hosts the disk-audio
      subsystem. (FR-001..FR-005)
- [ ] T002 [P] Create `CassoEmuCore/Audio/IDiskAudioSink.h` declaring the
      abstract interface (`OnMotorStart`, `OnMotorStop`,
      `OnHeadStep(int newQt)`, `OnHeadBump`). Header-only, no
      implementation, no STL includes beyond what's in `Pch.h`.
      (FR-001..FR-004, NFR-002)
- [ ] T003 [P] Create `CassoEmuCore/Audio/DiskAudioMixer.h` per
      `plan.md` §"DiskAudioMixer Interface" — declares the class
      (`public IDiskAudioSink`), all member fields with in-class
      initialization, and the named constants
      (`kMotorVolume`, `kHeadVolume`, `kSeekThresholdCycles`,
      `kStepDebounceCycles`, `kHeadIdleCycles`) as `static constexpr`.
      No function comments in the header. (FR-001..FR-010)
- [ ] T004 Add `CassoEmuCore/Audio/DiskAudioMixer.h` and
      `IDiskAudioSink.h` to the `CassoEmuCore` project's vcxproj file
      under the existing header item-group. Build must succeed
      (declarations only, no `.cpp` yet).
- [ ] T005 [GATE] Run `scripts\Build.ps1` and the full unit-test suite;
      verify no regressions from header-only additions.

---

## Phase 1: Mixer Skeleton (Silent, Stateful)

**Purpose**: Implement `DiskAudioMixer` with infallible event hooks and a
silent `GeneratePCM` so the rest of the wiring can be built and tested
without depending on sample assets.

- [ ] T010 Create `CassoEmuCore/Audio/DiskAudioMixer.cpp`. Implement
      constructor / destructor, `SetEnabled` / `IsEnabled`, and the four
      event-hook methods (`OnMotorStart`, `OnMotorStop`, `OnHeadStep`,
      `OnHeadBump`). The hooks update internal state only (set
      `m_motorRunning`, reset `m_headPos` to 0, switch `m_headBuf` between
      step/stop buffer pointers). `OnHeadStep` records `m_lastStepCycle`
      (cycle source: take a `uint64_t currentCycle` parameter — extend
      `IDiskAudioSink` if needed). (FR-001..FR-004)
- [ ] T011 Implement `DiskAudioMixer::GeneratePCM(float* out, uint32_t n)`
      as a silent implementation: `memset` the buffer to 0 and return.
      This keeps the public contract live for Phase 5 wiring. (FR-010)
- [ ] T012 [P] Create `UnitTest/Audio/DiskAudioMixerStateTests.cpp` (new
      file). Tests:
      - `OnMotorStart_setsRunningTrue`
      - `OnMotorStop_setsRunningFalse`
      - `OnHeadStep_resetsHeadPos_and_pointsAtStepBuffer`
      - `OnHeadBump_pointsAtStopBuffer_distinctFromStep`
      - `SetEnabled_false_thenGeneratePCM_outputIsSilent`
      All tests use directly-instantiated mixer with empty buffers (no
      I/O). (FR-001..FR-007, NFR-002)
- [ ] T013 [GATE] Build + run full test suite; new tests pass, no
      regressions.

---

## Phase 2: DiskIIController Wiring

**Purpose**: Add the sink hookup and four notification call sites in the
disk controller.

- [ ] T020 In `CassoEmuCore/Devices/DiskIIController.h`: add forward
      declaration `class IDiskAudioSink;`, add private member
      `IDiskAudioSink * m_audioSink = nullptr;` aligned with existing
      member-declaration column alignment, and add public method
      `void SetAudioSink (IDiskAudioSink * sink);`. (FR-001..FR-004)
- [ ] T021 In `CassoEmuCore/Devices/DiskIIController.cpp`: implement
      `SetAudioSink`. Add the four notification call sites per
      `research.md` §5.3:
      - In `HandleSwitch()` case `0x9`, immediately after `m_motorOn =
        true`, fire `OnMotorStart()`. (FR-001)
      - In `Tick()`, at the spindown-timer-expires branch immediately
        after `m_motorOn = false`, fire `OnMotorStop()`. (FR-002)
      - In `HandlePhase()`, immediately after `m_quarterTrack += qtDelta`
        (post-clamp), if `qtDelta != 0`: compute `prevQt = m_quarterTrack
        - qtDelta`; if that crossed a stop, fire `OnHeadBump()`; else
        fire `OnHeadStep(m_quarterTrack)`. (FR-003, FR-004)
      All four call sites guard against `m_audioSink == nullptr`.
- [ ] T022 [P] Create `UnitTest/Devices/DiskIIControllerAudioTests.cpp`.
      Use a recording mock implementing `IDiskAudioSink` that captures an
      ordered log of events. Tests:
      - `MotorOnSoftSwitch_firesOnMotorStart_exactlyOnce`
      - `MotorOffThenSpindownTick_firesOnMotorStop`
      - `MotorOffThenMotorOnWithinSpindown_doesNotFireOnMotorStop`  *(FR-001 critical: tests the spindown-gotcha)*
      - `PhaseChange_noMovement_firesNothing`  *(FR-003 critical: zero-qtDelta path)*
      - `PhaseChange_oneQuarterStep_firesOnHeadStep_withCorrectQt`
      - `PhaseChange_pastTrack0_firesOnHeadBump_notOnHeadStep`  *(FR-004 critical)*
      - `PhaseChange_pastMaxTrack_firesOnHeadBump`
      All tests run against a real `DiskIIController` driven by direct
      soft-switch invocation; no disk image required.
      (FR-001, FR-002, FR-003, FR-004)
- [ ] T023 [GATE] Build + run full test suite. Speaker-only existing
      tests still pass. New controller-audio tests pass. Verify no
      DOS 3.3 boot regression via existing disk-fixture integration tests.

---

## Phase 3: Mixer Playback (Sample-Backed PCM)

**Purpose**: Make the mixer actually emit PCM. The choice between bundled
WAV decode and procedural synthesis is made here; either path satisfies
the spec.

- [ ] T030 Decide and document (in `plan.md` §"Sample Sourcing" addendum
      or a new `sample-sourcing.md` note): bundled WAV vs procedural
      synthesis. Implementation tasks T031/T032 fork on this decision.
- [ ] T031 **If bundled-WAV path chosen**: Implement
      `DiskAudioMixer::LoadSamples(const wchar_t* dirPath, uint32_t
      targetSampleRate)`. Use `IMFSourceReader` to decode
      `motor_loop.wav`, `head_step.wav`, `head_stop.wav` into mono
      float32 buffers, resampling to `targetSampleRate`. EHM pattern.
      Any individual file failure: log one warning line, leave that
      buffer empty (graceful degradation per FR-009). Return `S_OK` if
      *any* sample loaded; only return failure HR if all three failed
      AND the caller wants to know. Add `Assets/Sounds/DiskII/` to the
      repo (empty, with a `.gitkeep` and a `README.md` describing
      expected files and licensing requirements per NFR-004). (FR-009, NFR-003, NFR-004)
- [ ] T032 **If procedural path chosen**: Implement
      `DiskAudioMixer::SynthesizeSamples(uint32_t targetSampleRate)`.
      Generate motor loop (low-frequency oscillator + noise + envelope),
      head step (impulse + short decay), head stop (impulse + longer
      decay). Buffers owned as `std::vector<float>` members. Infallible
      (no host I/O). (FR-001, FR-004, NFR-004)
- [ ] T033 Implement `DiskAudioMixer::MixMotor(float* out, uint32_t n)`:
      while `m_motorRunning`, read from `m_motorBuf` starting at
      `m_motorPos`, wrap at `m_motorLen`, add `sample * kMotorVolume` to
      each `out[i]`. (FR-001)
- [ ] T034 Implement `DiskAudioMixer::MixHead(float* out, uint32_t n)`:
      while `m_headPos < m_headLen` and `m_headBuf != nullptr`, read
      sample, add `sample * kHeadVolume` to `out[i]`, advance
      `m_headPos`. (FR-003, FR-004)
- [ ] T035 Update `GeneratePCM`: clear buffer; if `!m_enabled`, return.
      Else call `MixMotor` then `MixHead`. Per-sample clamp is the
      *caller's* responsibility (WasapiAudio) since speaker is added on
      top. (FR-006, FR-007, FR-010)
- [ ] T036 [P] Extend `DiskAudioMixerStateTests.cpp` (or create
      `DiskAudioMixerPlaybackTests.cpp`). Tests use synthetic in-memory
      sample buffers (e.g., motor = all-1.0, head = ramp 0→1 over 10
      samples). Verify:
      - `MotorRunning_outputContainsScaledMotorSamples`
      - `MotorRunning_wrapsAtBufferEnd`
      - `MotorNotRunning_motorContributionIsZero`
      - `OnHeadStep_then_GeneratePCM_outputsScaledStepSampleOnce_thenZero`
      - `OnHeadBump_outputsScaledStopSample_notStepSample`
      - `MissingMotorBuffer_emptyBuffer_outputsSilenceForMotor`  *(FR-009)*
      - `MissingHeadBuffer_OnHeadStep_outputsSilence`  *(FR-009)*
      (FR-001, FR-003, FR-004, FR-009)
- [ ] T037 [GATE] Build + test. All Phase-3 tests pass. Speaker-only
      tests unchanged.

---

## Phase 4: Step-vs-Seek Discrimination

**Purpose**: FR-005 — collapse rapid step bursts into a single continuous
gesture.

- [ ] T040 Add `uint64_t m_currentCycle` plumbing: the mixer needs the
      current CPU cycle count at each `OnHeadStep`. Either (a) extend
      `IDiskAudioSink::OnHeadStep` to take `uint64_t cycle`, or (b) add a
      `DiskAudioMixer::Tick(uint64_t currentCycle)` called once per
      audio frame from `WasapiAudio::SubmitFrame()`. Option (b)
      preferred: cleaner interface for unit tests, and the cycle delta
      from the previous step is sufficient. Document choice in code
      comment.
- [ ] T041 Implement the seek-mode state machine in
      `DiskAudioMixer::OnHeadStep`:
      - If `(currentCycle - m_lastStepCycle) < kSeekThresholdCycles` and
        `m_lastStepCycle != 0`: set `m_seekMode = true`; do **not**
        reset `m_headPos` (let current sample continue) — or, if the
        previous one-shot has already finished, keep the seek state
        active by some equivalent mechanism.
      - Else: clear `m_seekMode`; reset `m_headPos = 0` (fresh step
        one-shot).
      - In both branches: update `m_lastStepCycle = currentCycle`.
- [ ] T042 Implement seek-mode auto-clear in `GeneratePCM` (or `Tick`):
      if `m_seekMode && (currentCycle - m_lastStepCycle) >
      kHeadIdleCycles`, clear `m_seekMode`.
- [ ] T043 Update `MixHead` for seek-mode behavior: while
      `m_seekMode`, continue producing audio that maps onto a perceived
      continuous buzz (either holding the step sample's looping body
      between steps, or by emitting a synthesized seek tone — implementer
      choice). The acceptance criterion is auditory (FR-005); the test
      below verifies the structural invariant.
- [ ] T044 [P] Tests in `DiskAudioMixerSeekTests.cpp`:
      - `FourStepsWithin16ms_doesNotResetHeadPosFourTimes`
        — drive `OnHeadStep` four times with `currentCycle` deltas of
        4000 cycles; verify `m_seekMode == true` after the 2nd step and
        that head one-shots are not restarted from 0 on every step.
      - `TwoStepsApartBy30ms_treatsBothAsSingleClicks`
        — deltas of 31000 cycles; verify each step is a distinct one-shot
        (head_pos resets to 0 each time, seek mode stays false).
      - `SeekModeIdleTimeout_after50ms_clearsSeekMode`
      (FR-005, SC-005)
- [ ] T045 [GATE] Build + test. FR-005 tests pass. Listen to a DOS 3.3
      boot fixture if a manual harness is available — verify the
      recalibration sounds like a buzz, not click-click-click. (Manual
      step, document outcome in PR.)

---

## Phase 5: WASAPI Integration

**Purpose**: Mix disk PCM into the speaker buffer inside `SubmitFrame`.

- [ ] T050 Extend `Casso/WasapiAudio.h`: add optional parameter
      `DiskAudioMixer* diskMixer = nullptr` to `SubmitFrame`. Forward-declare
      `DiskAudioMixer` to avoid including the full header in the public
      header. (FR-010)
- [ ] T051 In `Casso/WasapiAudio.cpp`: inside `SubmitFrame` after speaker
      PCM is generated into the existing scratch buffer (`m_tempBuf` per
      research §5.3), if `diskMixer != nullptr` call
      `diskMixer->GeneratePCM(diskBuf.data(), numSamples)` into a
      file-scope static `std::vector<float>` (resize as needed); then
      additively mix `m_tempBuf[i] = clamp(m_tempBuf[i] + diskBuf[i],
      -1.0f, +1.0f)`. (FR-010, FR-011)
- [ ] T052 If Phase-4 chose `Tick(currentCycle)` (option b): call it once
      at the top of `SubmitFrame` with the current cycle counter
      retrieved from the shell or controller.
- [ ] T053 [P] Tests — extend or create
      `UnitTest/Audio/WasapiAudioMixingTests.cpp` (note: full WASAPI
      isn't exercised; we test the mixing math in isolation by
      refactoring the additive-mix step into a free helper
      `MixDiskIntoSpeaker(speaker, disk, n)` and testing that). Tests:
      - `MixDiskIntoSpeaker_speakerOnly_diskNull_unchanged`  *(FR-011)*
      - `MixDiskIntoSpeaker_diskAndSpeaker_sumsThenClamps`  *(FR-010)*
      - `MixDiskIntoSpeaker_overflowAbovePlusOne_clampsToPlusOne`
      - `MixDiskIntoSpeaker_underflowBelowMinusOne_clampsToMinusOne`
- [ ] T054 [GATE] Build + run **all** tests, including the existing
      speaker-pipeline tests. Pre-existing speaker tests must produce
      bit-identical output to before the diff (SC-006, FR-011). If
      audio-output goldens exist (per the //e fidelity feature),
      verify those are unchanged.

---

## Phase 6: EmulatorShell Wiring

**Purpose**: Compose the mixer into the shell and connect it to the
controller and WASAPI.

- [ ] T060 In `Casso/EmulatorShell.h`: add `DiskAudioMixer m_diskAudioMixer;`
      member, properly column-aligned with existing members. Include
      `CassoEmuCore/Audio/DiskAudioMixer.h`.
- [ ] T061 In `Casso/EmulatorShell.cpp`:
      - During machine construction (after the disk controller is
        created), call `m_diskController->SetAudioSink (&m_diskAudioMixer);`.
        Do this for **each** disk controller in the machine config
        (FR-008 — both drives in a 2-drive //e share the same mixer
        instance).
      - In `ExecuteCpuSlices()` at the existing `SubmitFrame` call site
        (~line 2480 per research), pass `&m_diskAudioMixer` as the new
        argument.
      - If Phase-3 chose bundled-WAV: call
        `m_diskAudioMixer.LoadSamples(...)` once in `Initialize()` after
        `m_wasapiAudio.Initialize()` so the WASAPI sample rate is
        known. If procedural: call `m_diskAudioMixer.SynthesizeSamples(rate)`.
        EHM around `LoadSamples`; ignore failure (graceful degradation).
- [ ] T062 [P] Smoke-test task: run the emulator manually (or via the
      existing headless harness if it can produce audio), cold-boot a
      DOS 3.3 fixture, listen / capture audio. Verify motor hum + step
      clicks + at least one track-0 bump are audible during DOS RWTS
      recalibration. Document outcome in PR. (SC-001, SC-002)
- [ ] T063 [GATE] Build + test full suite. No regressions. Existing disk
      I/O tests pass (the audio sink is decoupled from disk semantics).

---

## Phase 7: View → Disk Audio Menu Toggle

**Purpose**: FR-006, FR-007 — runtime toggleable disk audio, default on.

- [ ] T070 In `Casso/MenuSystem.h`: declare a new menu-item id
      `IDM_VIEW_DISKAUDIO` alongside the existing View-menu item ids,
      preserving column alignment.
- [ ] T071 In `Casso/MenuSystem.cpp`: append a check-style item "Disk Audio"
      to the View popup at the position appropriate to the existing
      grouping. Initial state: checked. (FR-006)
- [ ] T072 In `Casso/EmulatorShell.cpp`: in the WM_COMMAND / menu dispatch
      handler, on `IDM_VIEW_DISKAUDIO` toggle the menu item's check state
      and call `m_diskAudioMixer.SetEnabled(checked)`. (FR-006, FR-007)
- [ ] T073 [P] Manual test task: launch emulator, mount disk, verify
      **View → Disk Audio** is checked by default; uncheck mid-load,
      verify disk sounds cease within ~50 ms while a speaker beep
      remains audible; re-check, verify disk sounds resume on next
      disk activity. (SC-003)
- [ ] T074 [GATE] Build + full test suite green. CHANGELOG entry drafted
      (`feat(audio): add Disk II mechanical sounds (motor, step, bump) with View menu toggle`).

---

## Phase 8: Graceful Asset Degradation Verification

**Purpose**: FR-009 — explicit verification that missing assets don't crash
or popup.

- [ ] T080 If bundled-WAV path was chosen in Phase 3: rename
      `Assets/Sounds/DiskII/` locally, launch the emulator, cold-boot a
      disk fixture. Verify:
      - Process reaches the main window without modal popups
      - Logs contain at most 3 warning lines (one per missing sample)
      - **View → Disk Audio** still toggles without error
      - Speaker is fully audible
      Restore the directory after the test. Document outcome in PR.
      (FR-009, SC-004)
- [ ] T081 If procedural path was chosen: this phase is automatically
      satisfied (no external assets). Mark T080 as N/A and document.
- [ ] T082 [GATE] PR readiness checklist: SC-001..SC-006 all
      ticked or explicitly waived with rationale.

---

## Phase 9: Polish

- [ ] T090 [P] Run `scripts\Build.ps1 -RunCodeAnalysis`; address all new
      warnings in disk-audio code.
- [ ] T091 [P] `rg -n '\w \(\)' CassoEmuCore/Audio Casso/MenuSystem.cpp Casso/WasapiAudio.cpp Casso/EmulatorShell.cpp` — zero hits on lines authored in this feature.
- [ ] T092 Update `CHANGELOG.md` under `## [Unreleased]` with the
      `feat(audio)` entry. Mention motor hum, head clicks, track-0 bump,
      View menu toggle, and the graceful-degradation behavior.
- [ ] T093 Update `README.md`: add Disk II audio to the feature list /
      roadmap as appropriate.
- [ ] T094 [GATE] Final full-suite test pass + code-analysis pass +
      constitution sweep. Ready for merge with `--no-ff`.

---

## Dependency Summary

```
Phase 0 (contracts)
   └─► Phase 1 (mixer skeleton)
          └─► Phase 2 (controller wiring)        ─┐
          └─► Phase 3 (playback)                  │
                 └─► Phase 4 (step/seek)          │
                        └─► Phase 5 (WASAPI mix)  │
                               └─► Phase 6 (shell wire) ◄┘
                                      └─► Phase 7 (menu)
                                             └─► Phase 8 (graceful)
                                                    └─► Phase 9 (polish)
```

Phases 2 and 3 can proceed in parallel after Phase 1 (different files: the
controller wiring touches `Devices/DiskIIController.*`; playback is
self-contained in `Audio/DiskAudioMixer.cpp`). They re-converge at Phase 5.

## FR / SC Coverage Matrix

| Req     | Tasks                            |
|---------|----------------------------------|
| FR-001  | T010, T021, T022, T033, T036, T060 |
| FR-002  | T010, T021, T022                  |
| FR-003  | T010, T021, T022, T034, T036      |
| FR-004  | T010, T021, T022, T034, T036      |
| FR-005  | T041, T042, T043, T044, T045      |
| FR-006  | T010, T071, T072, T073            |
| FR-007  | T010, T072, T073                  |
| FR-008  | T061                              |
| FR-009  | T031, T036, T080                  |
| FR-010  | T035, T050, T051, T053            |
| FR-011  | T051, T053, T054                  |
| NFR-001 | T051, T054                        |
| NFR-002 | T002, T010                        |
| NFR-003 | T031                              |
| NFR-004 | T031, T032                        |
| SC-001  | T062                              |
| SC-002  | T062                              |
| SC-003  | T073                              |
| SC-004  | T080                              |
| SC-005  | T044, T045                        |
| SC-006  | T054                              |
