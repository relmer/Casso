---
description: "Disk II Audio — actionable, dependency-ordered tasks"
---

# Tasks: Disk II Audio

**Input**: Design documents from `/specs/005-disk-ii-audio/`
**Prerequisites**: spec.md, research.md, plan.md
**Constitution**: `.specify/memory/constitution.md`

**Tests**: Required throughout. Every production-code task that has
observable behavior has a paired test task. Tests use in-memory sample
buffers and a mock `IDriveAudioSink` — no host filesystem reads, no audio
device, per constitution §II.

**Organization**: Phases follow `research.md` §6.3's recommended
implementation order, expanded for the stereo, multi-source, options-dialog,
and door-event scope. Each phase ends with an explicit `[GATE]` task that
re-asserts pre-existing-tests-still-green and speaker-only behavior is
unchanged (SC-006 / FR-011).

## Format: `[ID] [P?] [GATE?] Description`

- **[P]** — parallel-safe within its phase (different files, no shared-edit conflict)
- **[GATE]** — phase-completion gate task; not parallel
- Every task cites the FR(s) it satisfies and the file(s) it touches
- Every production-code task is paired with (or precedes) a test task

## Constitution rules — apply to every code task

1. Every `.cpp` includes `"Pch.h"` as the **first** include; no angle-bracket includes anywhere except inside `Pch.h`; project headers use quoted includes
2. Function comments live in `.cpp` (80-`/` delimiters), not in the header
3. Function/operator spacing: `func()`, `func (a, b)`, `if (...)`, `for (...)`
4. All locals at top of scope, column-aligned (type / `*`-`&` column / name / `=` / value)
5. No non-trivial calls inside macro arguments — store result first
6. ≤ 50 lines per function (100 absolute); ≤ 2 indent levels beyond EHM (3 max)
7. Exactly 5 blank lines between top-level constructs; exactly 3 between top-of-function declarations and first statement
8. EHM pattern (`HRESULT hr = S_OK; … Error: … return hr;`) on every fallible function; never bare `goto Error`
9. No magic numbers (except 0/1/-1/nullptr/sizeof) — see `plan.md` for the named-constants list
10. File names: PascalCase, no underscores (`DriveAudioMixer.cpp`, not `drive_audio_mixer.cpp`); asset filenames likewise (`MotorLoop.wav`, not `motor_loop.wav`)

---

## Phase 0: Contracts & Declarations

**Purpose**: Stand up the interfaces and class declarations. No behavior yet.

- [X] T001 [P] Create `CassoEmuCore/Audio/` directory and add a placeholder
      `README.md` documenting that this directory hosts the drive-audio
      subsystem (mixer + per-drive sources). (FR-001..FR-005, FR-013..FR-016)
- [X] T002 [P] Create `CassoEmuCore/Audio/IDriveAudioSink.h` declaring the
      abstract notification interface: `OnMotorStart`, `OnMotorStop`,
      `OnHeadStep(int newQt)`, `OnHeadBump()`, `OnDiskInserted()`,
      `OnDiskEjected()`. Header-only, no implementation, no STL includes
      beyond what's in `Pch.h`. (FR-001..FR-004, FR-013, FR-014, FR-016, NFR-002)
- [X] T003 [P] Create `CassoEmuCore/Audio/IDriveAudioSource.h` declaring the
      abstract per-drive source interface: `GeneratePCM(float* outMono, uint32_t n)`,
      `PanLeft()`, `PanRight()`, `SetPan(float left, float right)`. Inherits
      from `IDriveAudioSink`. Header-only. (FR-012, FR-016)
- [X] T004 [P] Create `CassoEmuCore/Audio/DriveAudioMixer.h` per
      `plan.md` §"Architecture" — declares the class with
      `RegisterSource(IDriveAudioSource*)`, `UnregisterSource`,
      `SetEnabled`/`IsEnabled`, `GeneratePCM(float* stereoOut, uint32_t n)`,
      `Tick(uint64_t currentCycle)`. Owns no sources directly; just a
      `std::vector<IDriveAudioSource*>` registry. In-class member
      initialization: `bool m_enabled = true;` (FR-006 default-on).
      Named constants: `kSpeakerCenter = 0.7071f` (= √0.5), `kDrivePanOffset
      = 0.3927f` (= π/8 radians). No function comments in header. (FR-006,
      FR-008, FR-010, FR-012, FR-016)
- [X] T005 [P] Create `CassoEmuCore/Audio/DiskIIAudioSource.h` declaring the
      concrete Disk II source (`public IDriveAudioSource`). All member fields
      with in-class initialization; named constants as `static constexpr`
      (`kMotorVolume = 0.25f`, `kHeadVolume = 0.30f`, `kDoorVolume = 0.30f`,
      `kSeekThresholdCycles = 16368`, `kHeadIdleCycles = 51150`). Sample-buffer
      members for motor/head/door. (FR-001..FR-005, FR-013, FR-014)
- [X] T006 Add `CassoEmuCore/Audio/{IDriveAudioSink,IDriveAudioSource,DriveAudioMixer,DiskIIAudioSource}.h`
      to the `CassoEmuCore` project's vcxproj file under the existing header
      item-group. Build must succeed (declarations only, no `.cpp` yet).
- [X] T007 [GATE] Run `scripts\Build.ps1` and the full unit-test suite;
      verify no regressions from header-only additions.

---

## Phase 1: DiskIIAudioSource Skeleton (Silent, Stateful)

**Purpose**: Implement `DiskIIAudioSource` with infallible event hooks and a
silent `GeneratePCM` so wiring downstream can be built and tested without
sample assets.

- [X] T010 Create `CassoEmuCore/Audio/DiskIIAudioSource.cpp`. Implement
      constructor / destructor, `SetPan` / `PanLeft` / `PanRight`, and the
      six event-hook methods (`OnMotorStart`, `OnMotorStop`, `OnHeadStep`,
      `OnHeadBump`, `OnDiskInserted`, `OnDiskEjected`). The hooks update
      internal state only (set `m_motorRunning`, reset `m_headPos` to 0 and
      switch `m_headBuf` between step/stop pointers, reset `m_doorPos` and
      switch `m_doorBuf` between open/close pointers). `OnHeadStep` records
      `m_lastStepCycle` via a future `Tick(currentCycle)` hook (Phase 6).
      (FR-001..FR-004, FR-013, FR-014)
- [X] T011 Implement `DiskIIAudioSource::GeneratePCM(float* out, uint32_t n)`
      as a silent implementation: `memset` the buffer to 0 and return. Keeps
      the contract live for downstream wiring. (FR-010)
- [X] T012 [P] Create `UnitTest/Audio/DiskIIAudioSourceStateTests.cpp`. Tests:
      - `OnMotorStart_setsRunningTrue`
      - `OnMotorStop_setsRunningFalse`
      - `OnHeadStep_resetsHeadPos_and_pointsAtStepBuffer`
      - `OnHeadBump_pointsAtStopBuffer_distinctFromStep`
      - `OnDiskInserted_pointsAtCloseBuffer_resetsDoorPos`
      - `OnDiskEjected_pointsAtOpenBuffer_resetsDoorPos`
      - `SetPan_storesValuesAndReturnsThem`
      All tests use a directly-instantiated source with empty buffers (no
      I/O). (FR-001..FR-004, FR-012, FR-013, FR-014, NFR-002)
- [X] T013 [GATE] Build + run full test suite; new tests pass, no
      regressions.

---

## Phase 2: DriveAudioMixer Skeleton (Stereo Pan + Mix)

**Purpose**: Stand up the mixer with source registration and the
stereo pan-and-mix loop. Sources still silent at this point; tests verify
mixing math against synthetic source output via a mock source.

- [X] T020 Create `CassoEmuCore/Audio/DriveAudioMixer.cpp`. Implement
      constructor / destructor, `RegisterSource` / `UnregisterSource` (push
      back / erase from `m_sources` vector), `SetEnabled` / `IsEnabled`.
      (FR-008, FR-010, FR-016)
- [X] T021 Implement `DriveAudioMixer::GeneratePCM(float* stereoOut, uint32_t n)`:
      - `memset` stereo buffer to 0
      - if not enabled, return
      - allocate (or reuse a class-member) scratch mono buffer of size n
      - for each registered source: call `source->GeneratePCM(scratch, n)`;
        then loop i in [0, n) accumulating `stereoOut[2*i] += scratch[i] * source->PanLeft()`
        and `stereoOut[2*i+1] += scratch[i] * source->PanRight()`
      - Per-channel clamp is the WASAPI integrator's job (called after
        speaker is summed in), not the mixer's. (FR-010, FR-012)
- [X] T022 Remove the motor-hum dedup mechanic per FR-008's revised
      stance: both drives' motor sources play independently with their
      equal-power pan. No suppression logic. Document the rationale in a
      code comment referencing FR-008 (linear sum bounded by per-drive
      panning + phase-incoherence). (FR-008)
- [X] T023 [P] Create `UnitTest/Audio/DriveAudioMixerTests.cpp`. Use an
      in-test `MockDriveAudioSource` that emits a known PCM pattern. Tests:
      - `NoSources_outputsSilence`
      - `OneSource_centerPan_appearsEqualOnBothChannels`
      - `OneSource_leftPan_appearsMostlyOnLeft`
      - `TwoSources_oppositePans_appearOnOppositeChannels`
      - `SetEnabledFalse_outputsSilenceRegardlessOfSources`
      - `TwoSourcesBothMotorRunning_eachContributesIndependentlyWithItsPan`  *(FR-008)*
      (FR-008, FR-010, FR-012, FR-016)
- [X] T024 [GATE] Build + tests green; speaker tests unchanged.

---

## Phase 3: DiskIIController Wiring

**Purpose**: Add the sink hookup and notification call sites in the
disk controller (motor / head / mount / eject).

- [X] T030 In `CassoEmuCore/Devices/DiskIIController.h`: add forward
      declaration `class IDriveAudioSink;`, add private member
      `IDriveAudioSink * m_audioSink = nullptr;` aligned with existing
      member-declaration column alignment, and add public method
      `void SetAudioSink (IDriveAudioSink * sink);`. (FR-001..FR-004, FR-013, FR-014)
- [X] T031 In `CassoEmuCore/Devices/DiskIIController.cpp`: implement
      `SetAudioSink`. Add notification call sites per `research.md` §5.3:
      - In `HandleSwitch()` case `0x9`, immediately after `m_motorOn = true`,
        fire `OnMotorStart()`. (FR-001)
      - In `Tick()`, at the spindown-timer-expires branch immediately after
        `m_motorOn = false`, fire `OnMotorStop()`. (FR-002)
      - In `HandlePhase()`, immediately after `m_quarterTrack += qtDelta`
        (post-clamp), if `qtDelta != 0`: compute `prevQt = m_quarterTrack
        - qtDelta`; if that crossed a stop, fire `OnHeadBump()`; else
        fire `OnHeadStep(m_quarterTrack)`. (FR-003, FR-004)
      All call sites guard against `m_audioSink == nullptr`.
- [X] T032 In `CassoEmuCore/Devices/DiskIIController.cpp`: add `MountImage`
      / `EjectImage` (or extend the existing mount/eject path) to fire
      `OnDiskInserted()` / `OnDiskEjected()` on the sink. If mount/eject is
      driven from the shell rather than the controller, plumb the call there
      instead and note the choice in a code comment. (FR-013, FR-014)
- [X] T033 [P] Create `UnitTest/Devices/DiskIIControllerAudioTests.cpp`.
      Use a recording mock implementing `IDriveAudioSink` that captures an
      ordered log of events. Tests:
      - `MotorOnSoftSwitch_firesOnMotorStart_exactlyOnce`
      - `MotorOffThenSpindownTick_firesOnMotorStop`
      - `MotorOffThenMotorOnWithinSpindown_doesNotFireOnMotorStop`  *(FR-001 critical: spindown gotcha)*
      - `PhaseChange_noMovement_firesNothing`  *(FR-003 critical: zero-qtDelta path)*
      - `PhaseChange_oneQuarterStep_firesOnHeadStep_withCorrectQt`
      - `PhaseChange_pastTrack0_firesOnHeadBump_notOnHeadStep`  *(FR-004 critical)*
      - `PhaseChange_pastMaxTrack_firesOnHeadBump`
      - `MountImage_firesOnDiskInserted`  *(FR-013)*
      - `EjectImage_firesOnDiskEjected`  *(FR-014)*
      - `MountImage_overExistingDisk_firesEjectThenInsert`  *(FR-013)*
      All tests run against a real `DiskIIController`; no real disk audio
      device. (FR-001..FR-004, FR-013, FR-014)
- [X] T034 [GATE] Build + run full test suite. Speaker-only existing tests
      still pass. New controller-audio tests pass. No DOS 3.3 boot regression
      via existing disk-fixture integration tests.

---

## Phase 4: Sample Loading / Synthesis

**Purpose**: Make `DiskIIAudioSource` own real sample buffers. Real-recording
preference is documented per NFR-005; either path satisfies the spec.

- [X] T040 Decide and document (in `plan.md` §"Sample Sourcing" addendum or
      a short `sample-sourcing.md` note): bundled WAV vs procedural synthesis
      for v1. Per NFR-005, real recordings are preferred when permissively
      licensed; synthesis is acceptable fallback. Implementation tasks
      T041/T042 fork on this decision.
- [X] T041 **If bundled-WAV path**: Implement
      `DiskIIAudioSource::LoadSamples(const wchar_t* dirPath, uint32_t targetSampleRate)`.
      Use `IMFSourceReader` to decode `MotorLoop.wav`, `HeadStep.wav`,
      `HeadStop.wav`, `DoorOpen.wav`, `DoorClose.wav` into mono float32
      buffers, resampling to `targetSampleRate`. EHM pattern. Any individual
      file failure: log one warning line, leave that buffer empty (graceful
      degradation per FR-009). Return `S_OK` if *any* sample loaded. Add
      `Assets/Sounds/DiskII/` to the repo (empty or with placeholder
      synthesis, with a `README.md` describing expected files, preferred
      sourcing per NFR-005, and licensing requirements per NFR-004).
      (FR-009, FR-013, FR-014, NFR-003, NFR-004, NFR-005)
- [ ] T042 **If procedural path**: Implement
      `DiskIIAudioSource::SynthesizeSamples(uint32_t targetSampleRate)`.
      Generate motor loop, head step, head stop, door open, door close per
      `plan.md` §"Sample Sourcing". Buffers owned as `std::vector<float>`
      members. Infallible. Aim for mechanical realism (NFR-005). (FR-001,
      FR-004, FR-013, FR-014, NFR-004, NFR-005)
- [X] T043 [GATE] Build + tests. New tests added in Phase 5 will exercise
      the buffers; for this phase a smoke test that `LoadSamples`/
      `SynthesizeSamples` returns success and populates buffers (or graceful
      empties) is sufficient.

---

## Phase 5: Mixer Playback (Motor, Head, Door)

**Purpose**: Actually emit PCM for each event type.

- [X] T050 Implement `DiskIIAudioSource::MixMotor(float* out, uint32_t n)`:
      while `m_motorRunning`, read from `m_motorBuf` starting at `m_motorPos`,
      wrap at `m_motorLen`, add `sample * kMotorVolume` to each `out[i]`.
      (FR-001)
- [X] T051 Implement `DiskIIAudioSource::MixHead(float* out, uint32_t n)`:
      while `m_headPos < m_headLen` and `m_headBuf != nullptr`, read sample,
      add `sample * kHeadVolume` to `out[i]`, advance `m_headPos`. (FR-003,
      FR-004)
- [X] T052 Implement `DiskIIAudioSource::MixDoor(float* out, uint32_t n)`:
      same shape as `MixHead` but for door open/close buffer with
      `kDoorVolume`. (FR-013, FR-014)
- [X] T053 Update `DiskIIAudioSource::GeneratePCM`: clear buffer; call
      `MixMotor`, `MixHead`, `MixDoor` in sequence. (FR-010, FR-013, FR-014)
- [X] T054 [P] Create or extend `UnitTest/Audio/DiskIIAudioSourcePlaybackTests.cpp`.
      Tests use synthetic in-memory buffers (motor = all-1.0, head = ramp
      0→1 over 10 samples, door = single impulse). Verify:
      - `MotorRunning_outputContainsScaledMotorSamples`
      - `MotorRunning_wrapsAtBufferEnd`
      - `MotorNotRunning_motorContributionIsZero`
      - `OnHeadStep_then_GeneratePCM_outputsScaledStepSampleOnce_thenZero`
      - `OnHeadBump_outputsScaledStopSample_notStepSample`
      - `OnDiskInserted_outputsScaledCloseSample`  *(FR-013)*
      - `OnDiskEjected_outputsScaledOpenSample`  *(FR-014)*
      - `MotorPlusHeadPlusDoor_simultaneouslyMixedAdditively`
      - `MissingMotorBuffer_emptyBuffer_outputsSilenceForMotor`  *(FR-009)*
      - `MissingHeadBuffer_OnHeadStep_outputsSilence`  *(FR-009)*
      - `MissingDoorBuffer_OnDiskInserted_outputsSilence`  *(FR-009)*
      (FR-001, FR-003, FR-004, FR-009, FR-013, FR-014)
- [X] T055 [GATE] Build + test. All Phase-5 tests pass. Speaker-only tests
      unchanged.

---

## Phase 6: Step-vs-Seek Discrimination

**Purpose**: FR-005 — collapse rapid step bursts into a single continuous
gesture.

- [X] T060 Add a `DiskIIAudioSource::Tick(uint64_t currentCycle)` method
      called once per audio frame from `DriveAudioMixer::GeneratePCM` (which
      receives the current CPU cycle from `WasapiAudio::SubmitFrame`).
      Document choice in code comment vs. the alternative of extending
      `IDriveAudioSink::OnHeadStep` to carry the cycle.
- [X] T061 Implement the seek-mode state machine in
      `DiskIIAudioSource::OnHeadStep`:
      - If `(currentCycle - m_lastStepCycle) < kSeekThresholdCycles` and
        `m_lastStepCycle != 0`: set `m_seekMode = true`; do **not** reset
        `m_headPos` (let current sample continue) — or, if the previous
        one-shot has already finished, keep seek state active by an
        equivalent mechanism.
      - Else: clear `m_seekMode`; reset `m_headPos = 0` (fresh step one-shot).
      - In both branches: update `m_lastStepCycle = currentCycle`.
- [X] T062 Implement seek-mode auto-clear in `Tick`: if `m_seekMode &&
      (currentCycle - m_lastStepCycle) > kHeadIdleCycles`, clear `m_seekMode`.
- [X] T063 Update `MixHead` for seek-mode behavior per FR-005 (continuous
      buzz, not click-click-click). Acceptance is auditory; the test below
      verifies the structural invariant.
- [X] T064 [P] Tests in `UnitTest/Audio/DiskIIAudioSourceSeekTests.cpp`:
      - `FourStepsWithin16ms_doesNotResetHeadPosFourTimes`
      - `TwoStepsApartBy30ms_treatsBothAsSingleClicks`
      - `SeekModeIdleTimeout_after50ms_clearsSeekMode`
      (FR-005, SC-005)
- [X] T065 [GATE] Build + test. FR-005 tests pass. Listen to a DOS 3.3 boot
      fixture if a manual harness is available — verify recalibration sounds
      like a buzz, not click-click-click. (Manual step, document outcome.)

---

## Phase 7: WASAPI Stereo Integration

**Purpose**: Mix drive PCM (stereo) into the speaker buffer (centered)
inside `SubmitFrame`. Negotiate stereo from WASAPI; downmix to mono only if
the device demands it.

- [X] T070 Extend `Casso/WasapiAudio.cpp` initialization: request stereo
      (`nChannels = 2`) in `desiredFormat`. Fall back to the device mix
      format if rejected; record `m_channels` (1 or 2) and `m_sampleRate`.
      (FR-010)
- [X] T071 Extend `Casso/WasapiAudio.h`: add optional parameter
      `DriveAudioMixer* driveMixer = nullptr` to `SubmitFrame`. Forward-declare
      `DriveAudioMixer` to avoid including the full header in the public
      header. (FR-010)
- [X] T072 In `Casso/WasapiAudio.cpp`: inside `SubmitFrame`, after speaker
      mono PCM is generated:
      - Build a stereo speaker scratch (`speakerL = speakerR = mono`)
      - If `driveMixer != nullptr`: call
        `driveMixer->Tick(currentCycle)` and
        `driveMixer->GeneratePCM(diskStereoBuf.data(), numSamples)` (size
        `2 * numSamples`)
      - Additively mix: `outL[i] = clamp(speakerL[i] + diskStereoBuf[2*i], -1, +1)`;
        `outR[i] = clamp(speakerR[i] + diskStereoBuf[2*i+1], -1, +1)`
      - If `m_channels == 1`: downmix `(outL[i] + outR[i]) * 0.5` per sample
        into `m_pendingSamples`. Else write interleaved stereo. (FR-010, FR-011)
- [X] T073 [P] Refactor the per-channel additive-mix step into a free helper
      `MixDriveIntoSpeakerStereo(speakerStereo, driveStereo, n)` and test in
      `UnitTest/Audio/WasapiAudioMixingTests.cpp`:
      - `MixDriveIntoSpeaker_speakerOnly_driveNull_unchanged`  *(FR-011)*
      - `MixDriveIntoSpeaker_driveAndSpeaker_sumsThenClampsPerChannel`  *(FR-010)*
      - `MixDriveIntoSpeaker_overflowAbovePlusOne_clampsToPlusOne`
      - `MixDriveIntoSpeaker_underflowBelowMinusOne_clampsToMinusOne`
      - `MonoDownmix_stereoToMono_averagesChannels`
- [X] T074 [GATE] Build + run **all** tests, including the existing
      speaker-pipeline tests. Speaker-only output remains correct (mono input
      → equal L and R → equal mono on mono device, sums to expected
      amplitude on stereo device). SC-006 / FR-011 preserved.

---

## Phase 8: EmulatorShell Wiring

**Purpose**: Compose the mixer and per-drive sources into the shell and
connect everything.

- [X] T080 In `Casso/EmulatorShell.h`: add `DriveAudioMixer m_driveAudioMixer;`
      and a small container of `DiskIIAudioSource` instances (one per
      attached Disk II drive — typically up to 2 per controller and up to 2
      controllers in v1 but treat as N). Column-align with existing members.
      Include `CassoEmuCore/Audio/DriveAudioMixer.h` and `DiskIIAudioSource.h`.
- [X] T081 In `Casso/EmulatorShell.cpp`:
      - During machine construction, for each Disk II controller in
        `MachineConfig`, allocate one `DiskIIAudioSource` per drive on that
        controller. Set per-drive equal-power pan using `kDrivePanOffset`:
        single-drive profiles → centered (`SetPanAngle(π/4)`); two-drive
        profiles → Drive 1 left-biased (`SetPanAngle(π/4 + kDrivePanOffset)`),
        Drive 2 right-biased (`SetPanAngle(π/4 - kDrivePanOffset)`). Call
        `m_driveAudioMixer.RegisterSource(...)` and
        `controller->SetAudioSink(&source)`. (FR-008, FR-012, FR-015)
      - In `ExecuteCpuSlices()` at the existing `SubmitFrame` call site,
        pass `&m_driveAudioMixer` as the new argument. (FR-010)
      - If Phase-4 chose bundled-WAV: call
        `source.LoadSamples(...)` once per source in `Initialize()` after
        `m_wasapiAudio.Initialize()` (so the WASAPI sample rate is known).
        If procedural: call `source.SynthesizeSamples(rate)`. EHM around
        `LoadSamples`; ignore failure (graceful degradation per FR-009).
      - Hook the existing disk mount/eject UI paths to call
        `OnDiskInserted()` / `OnDiskEjected()` on the affected drive's source
        (or its controller's `m_audioSink`). Honor FR-013's cold-boot
        exception: see T082b. (FR-013, FR-014)
- [X] T082 Verify machine-agnostic wiring: profiles with zero Disk II
      controllers (cassette-only ][, or future cassette/non-disk profiles)
      MUST NOT crash, MUST NOT allocate per-drive sources, and MUST cause
      `DriveAudioMixer::GeneratePCM` to emit silence. (FR-015, FR-016)
- [X] T082b Implement the FR-013 **cold-boot mount suppression**.
      `EmulatorShell` (or its mount path) MUST track a "startup phase"
      flag that begins `true` and transitions to `false` after machine
      initialization completes and the main message loop begins delivering
      user input. Any mount performed while the flag is `true` (command-line
      arguments, last-session restoration, autoload) MUST NOT fire
      `OnDiskInserted()`. Mounts after the flag transitions to `false` fire
      normally. Eject events always fire (no cold-boot eject case in
      practice). (FR-013)
- [X] T082c [P] Tests in `UnitTest/Devices/DiskIIControllerColdBootTests.cpp`
      (or extending T033's test file). Tests use a recording sink and drive
      the controller through a shell-stubbed "startup phase" toggle:
      - `ColdBootMount_doesNotFireOnDiskInserted`  *(FR-013)*
      - `PostStartupMount_firesOnDiskInserted`  *(FR-013)*
      - `ColdBootEject_firesOnDiskEjected`  *(FR-014, edge case)*
      (FR-013, FR-014)
- [ ] T083 [P] Smoke-test task (manual): cold-boot a DOS 3.3 fixture, listen.
      Verify motor hum + step clicks + at least one track-0 bump are audible
      during DOS RWTS recalibration. Verify two-drive profile: Drive 1
      activity sounds left-leaning, Drive 2 right-leaning. Verify
      mount/eject produces the door sounds. Document outcome in PR. (SC-001,
      SC-002, SC-007, SC-008)
- [X] T084 [GATE] Build + test full suite. No regressions. Existing disk
      I/O tests pass (audio is decoupled from disk semantics).

---

## Phase 9: View → Options... → Drive Audio Toggle

**Purpose**: FR-006, FR-007 — runtime toggleable drive audio, default on,
in a new Options dialog.

- [X] T090 Create `Casso/OptionsDialog.h` and `Casso/OptionsDialog.cpp`
      hosting a small modal dialog with at least a "Drive Audio" checkbox
      (initial state from the mixer's current `IsEnabled()`). Dialog
      resource template lives in `Casso/Casso.rc`. Apply on OK (FR-006).
- [X] T091 In `Casso/MenuSystem.h`: declare `IDM_VIEW_OPTIONS` alongside
      existing View-menu item ids, preserving column alignment.
- [X] T092 In `Casso/MenuSystem.cpp`: append an "Options..." item to the
      View popup at an appropriate position.
- [X] T093 In `Casso/EmulatorShell.cpp`: in the WM_COMMAND / menu dispatch
      handler, on `IDM_VIEW_OPTIONS` open `OptionsDialog` modally. On OK,
      call `m_driveAudioMixer.SetEnabled(checked)`. (FR-006, FR-007)
- [ ] T094 [P] Manual test task: launch emulator, mount disk, open
      **View → Options...**, verify "Drive Audio" is checked by default;
      uncheck and apply mid-load, verify drive sounds cease within ~50 ms
      while a speaker beep remains audible; re-check, verify drive sounds
      resume on next disk activity. (SC-003)
- [X] T095 [GATE] Build + full test suite green. CHANGELOG entry drafted
      (`feat(audio): Disk II mechanical sounds with stereo placement, mount/eject sounds, and Options dialog`).

---

## Phase 10: Graceful Asset Degradation Verification

**Purpose**: FR-009 — explicit verification that missing assets don't crash
or popup.

- [ ] T100 If bundled-WAV path was chosen in Phase 4: rename
      `Assets/Sounds/DiskII/` locally, launch the emulator, cold-boot a
      disk fixture. Verify:
      - Process reaches the main window without modal popups
      - Logs contain at most 5 warning lines (one per missing sample)
      - **View → Options... → Drive Audio** still toggles without error
      - Speaker is fully audible
      Restore the directory after the test. Document outcome in PR.
      (FR-009, SC-004)
- [X] T101 If procedural path was chosen: this phase is automatically
      satisfied (no external assets). Mark T100 as N/A and document.
- [X] T102 [GATE] PR readiness checklist: SC-001..SC-008 all ticked or
      explicitly waived with rationale.

---

## Phase 11: Polish

- [X] T110 [P] Run `scripts\Build.ps1 -RunCodeAnalysis`; address all new
      warnings in drive-audio code.
- [X] T111 [P] `rg -n '\w \(\)' CassoEmuCore/Audio Casso/MenuSystem.cpp Casso/WasapiAudio.cpp Casso/EmulatorShell.cpp Casso/OptionsDialog.cpp` — zero hits on lines authored in this feature.
- [X] T112 Update `CHANGELOG.md` (current version, no [Unreleased] section
      per project convention) with the `feat(audio)` entry. Mention motor
      hum, head clicks, track-0 bump, disk insert/eject, stereo per-drive
      placement, View → Options dialog, graceful-degradation behavior, and
      generic drive-audio abstraction (for future drive types).
- [X] T113 Update `README.md`: add Disk II audio + Options dialog to the
      feature list / roadmap as appropriate. Note that the abstraction is
      ready for future drive types.
- [X] T114 [GATE] Final full-suite test pass + code-analysis pass +
      constitution sweep. Run extended validation suites (Dormann + Harte)
      only if significant CPU/assembler changes were made (this feature
      should not affect them; document if skipped). Ready for merge with
      `--no-ff`.

---

## Phase 12: Directory Restructure + `.gitignore` Whitelist

**Purpose**: Migrate from top-level `ROMs/` to per-machine
`Machines/<Name>/<Name>.rom` and per-device `Devices/DiskII/`. Drop
`Assets/Sounds/DiskII/` in favor of `Devices/DiskII/<Mechanism>/`.
Replace blacklist `.gitignore` rules with a whitelist-JSON-only rule.

- [ ] T120 Create the new directory layout: `Machines/Apple2/`,
      `Machines/Apple2Plus/`, `Machines/Apple2e/`, `Machines/Apple2eEnhanced/`,
      `Devices/DiskII/`, `Devices/DiskII/Alps/`, `Devices/DiskII/Shugart/`.
      Move each `Machines/*.json` into its eponymous subdir (e.g.,
      `Machines/Apple2e.json` → `Machines/Apple2e/Apple2e.json`).
- [ ] T121 Update `.gitignore`: remove the `ROMs/` line and add a
      whitelist-JSON-only rule scoped to `Machines/**` and `Devices/**`:
      ```
      Machines/**
      !Machines/
      !Machines/**/
      !Machines/**/*.json

      Devices/**
      !Devices/
      !Devices/**/
      !Devices/**/*.json
      ```
      Keep `/Disks/Apple/dos33-master.dsk`. Verify `git status` shows no
      new untracked files inside the migrated directories.
- [ ] T122 Update `Casso/AssetBootstrap.cpp`'s `s_kRomCatalog` so each
      ROM's local path is `Machines/<MachineName>/<RomName>` for
      machine-specific ROMs, and `Devices/DiskII/<RomName>` for the
      Disk II controller ROMs (`Disk2.rom`, `Disk2_13Sector.rom`). Update
      `GetRomDirectory` semantics accordingly (it now returns multiple
      paths or a resolver function rather than a single dir).
- [ ] T123 Update `CassoEmuCore/Core/PathResolver` (or its callers) to
      know about the new per-machine ROM locations. Backward-compat
      search SHOULD also check a top-level `ROMs/` directory so existing
      user installs with ROMs in `ROMs/` continue to work (with a one-time
      migration log line on first launch).
- [ ] T124 Update `Casso/Main.cpp`'s `configRelPath` computation:
      `Machines/<Name>/<Name>.json` instead of `Machines/<Name>.json`.
      Update `AssetBootstrap::EnsureMachineConfigs` similarly.
- [ ] T125 Move (`git mv`) any existing `Assets/Sounds/DiskII/README.md`
      into `Devices/DiskII/README.md`. Keep `Assets/` for screenshots
      (unchanged).
- [ ] T126 Update `DiskIIAudioSource::LoadSamples` signature: take a
      mechanism subdirectory name (`L"Alps"` or `L"Shugart"`) and the
      `Devices/DiskII/` base path. Per-file precedence per FR-019:
      `Devices/DiskII/<filename>.wav` → `Devices/DiskII/<Mechanism>/<filename>.wav`
      → silent.
- [ ] T127 [P] Update `UnitTest/EmuTests/AssetBootstrapTests.cpp` and
      `UnitTest/EmuTests/MachineConfigTests.cpp` for the new path
      conventions. Tests use in-memory resolvers / temp dirs (per
      constitution §II — no real filesystem dependency on the new layout).
- [ ] T128 [GATE] Build + full test suite green. Existing installations
      (with ROMs in old `ROMs/`) still boot via backward-compat search.

---

## Phase 13: Bootstrap Fetch (stb_vorbis + OGG decode + consent)

**Purpose**: Implement FR-017, FR-018, NFR-006. First-run consent dialog
that fetches both Alps and Shugart OGGs from OpenEmulator's GitHub repo,
decodes in memory with stb_vorbis, resamples to the WASAPI device rate,
and writes WAVs to the per-mechanism subdirectories. No `.ogg` files on
disk.

- [ ] T130 Add `stb_vorbis.c` to the repo at
      `CassoEmuCore/External/stb_vorbis.c` (single header / single impl,
      public domain / MIT). Wrap include with
      `#define STB_VORBIS_NO_PUSHDATA_API` (we only need pulldata) and
      `#define STB_VORBIS_NO_STDIO` (we decode from memory only). Add to
      `CassoEmuCore.vcxproj`. (FR-018, NFR-006)
- [ ] T131 Add `s_kDiskAudioCatalog[]` to `Casso/AssetBootstrap.cpp`
      mirroring `s_kRomCatalog[]`. Entries: 5 Shugart sounds and 3 Alps
      sounds with upstream OGG basenames, target WAV filenames, and
      mechanism subdir. URL prefix:
      `/openemulator/libemulation/master/res/sounds/<Mechanism>/`
      with host `raw.githubusercontent.com`. (FR-017, FR-018)
- [ ] T132 Add `AssetBootstrap::CheckAndFetchDiskAudio(HINSTANCE,
      const wstring & machineName, HWND, const fs::path & devicesDir,
      string & outError)` mirroring `CheckAndFetchRoms`. Detection:
      checks for the presence of at least one WAV in each mechanism's
      subdir. If both subdirs are populated, no prompt. If either is
      empty, surfaces the FR-017 consent dialog. (FR-017)
- [ ] T133 Implement the consent dialog resource in `Casso/Casso.rc`
      (or build a runtime DLGTEMPLATE per the existing OptionsDialog
      pattern). Body MUST include: explicit GPL-3 disclosure, recipient-
      obligation note, link to OpenEmulator's `COPYING` file, link to
      the GPL-3 text. Buttons: Download / Skip / Don't ask again this
      session. (FR-017)
- [ ] T134 Implement the in-memory fetch + decode pipeline in
      `AssetBootstrap::FetchAndDecodeOgg(LPCWSTR urlPath, vector<float> &
      outPcm, uint32_t & outSampleRate, uint32_t targetSampleRate,
      string & outError)`:
      - WinHTTP GET → `vector<uint8_t> oggBytes` in memory.
      - `stb_vorbis_open_memory(oggBytes.data(), oggBytes.size(), ...)`.
      - `stb_vorbis_get_samples_short_interleaved(...)` → int16 PCM.
      - Downmix to mono if stereo; convert int16 → float32 (`/ 32768.0f`).
      - Resample to `targetSampleRate` (linear interp is acceptable —
        drive noise is broadband and not pitch-critical, per A-001
        rationale).
      - Discard `oggBytes`.
      EHM pattern. (FR-018, NFR-006)
- [ ] T135 Implement `AssetBootstrap::WritePcmAsWav(const fs::path &
      outPath, const vector<float> & pcm, uint32_t sampleRate,
      string & outError)`. Write a standard 16-bit PCM WAV (since that's
      what the existing `IMFSourceReader`-based `LoadSamples` will read).
      EHM pattern. (FR-018)
- [ ] T136 Wire `CheckAndFetchDiskAudio` into `Casso/Main.cpp` startup,
      right after `CheckAndFetchRoms` and only when the machine config
      contains a Disk II controller (reuse `AssetBootstrap::HasDiskController`).
      Errors do not block startup (FR-009); they log and continue. (FR-017)
- [ ] T137 [P] Tests in `UnitTest/EmuTests/DiskAudioFetchTests.cpp`:
      - `FetchAndDecodeOgg_validOggBytes_producesFloatPcm` (use a tiny
        embedded test OGG, not real OpenEmulator content)
      - `FetchAndDecodeOgg_invalidBytes_returnsFailureNoCrash`
      - `WritePcmAsWav_roundTripsThroughLoadSamples_preservesAmplitude`
      - `CheckAndFetchDiskAudio_bothMechanismsPresent_doesNotPrompt`
        (use in-memory file system mock if available, otherwise tempdir)
      - `CheckAndFetchDiskAudio_mechanismMissing_returnsConsentNeeded`
      Tests do NOT hit the network; the WinHTTP call site is mocked or
      separately tested at integration level. (Constitution §II.)
- [ ] T138 [GATE] Build + full test suite. Manual integration test:
      delete `Devices/DiskII/Alps/` and `Devices/DiskII/Shugart/`,
      launch the emulator with a //e profile, accept the consent dialog,
      verify both subdirs are populated with WAVs and zero `.ogg` files
      remain (NFR-006).

---

## Phase 14: Mechanism Dropdown in Options Dialog

**Purpose**: Implement the FR-006 mechanism selection (Shugart default,
runtime-switchable to Alps without restart). Implement SC-010.

- [ ] T140 Extend `Casso/OptionsDialog.{h,cpp}` to add a "Disk II
      mechanism" dropdown (combobox) with two entries: "Shugart SA400"
      (default) and "Alps 2124A". Wire to a new
      `m_driveAudioMixer.SetMechanism(L"Shugart" | L"Alps")` or
      equivalent.
- [ ] T141 Add `DriveAudioMixer::SetMechanism(const wstring & mechanism)`
      that iterates registered sources and re-invokes
      `LoadSamples(devicesDir, mechanism, deviceSampleRate)` on each.
      State change MUST take effect within one audio frame.
- [ ] T142 In `Casso/EmulatorShell.cpp`'s Options-dialog OK handler:
      detect mechanism change and call `m_driveAudioMixer.SetMechanism(...)`.
      No restart / no disk remount required. (FR-006)
- [ ] T143 [P] Tests in `UnitTest/Audio/DriveAudioMixerMechanismTests.cpp`:
      - `SetMechanism_callsLoadSamplesOnAllRegisteredSources`
      - `SetMechanism_alpsToShugart_changesActiveBufferSet`  (mock loader)
      - `SetMechanism_invalidName_returnsFailureNoStateChange`
      (FR-006, SC-010)
- [ ] T144 Manual smoke test: launch emulator, mount disk, open Options,
      flip mechanism dropdown Shugart→Alps→Shugart, verify audio changes
      audibly (Shugart has door sounds, Alps does not). (SC-010)
- [ ] T145 [GATE] Build + full test suite green. CHANGELOG entry extended
      with the mechanism dropdown and bootstrap consent dialog details.

---

## Dependency Summary

```
Phase 0 (contracts: sink, source, mixer, disk-ii-source)
   └─► Phase 1 (DiskIIAudioSource skeleton)
          ├─► Phase 2 (DriveAudioMixer skeleton)         ─┐
          └─► Phase 3 (controller wiring)                 │
                 ├─► Phase 4 (sample loading/synthesis)   │
                 │     └─► Phase 5 (mixer playback)       │
                 │            └─► Phase 6 (step/seek)     │
                 └────────────────────────────────────────┤
                                                          ▼
                                        Phase 7 (WASAPI stereo)
                                              └─► Phase 8 (shell wire)
                                                     └─► Phase 9 (Options dialog)
                                                            └─► Phase 10 (graceful)
                                                                   └─► Phase 11 (polish)
```

Phases 2/3 and 4/5 can proceed in parallel within their dependency cone.

## FR / SC Coverage Matrix

| Req     | Tasks                                  |
|---------|----------------------------------------|
| FR-001  | T010, T031, T033, T050, T054, T081      |
| FR-002  | T010, T031, T033                        |
| FR-003  | T010, T031, T033, T051, T054            |
| FR-004  | T010, T031, T033, T051, T054            |
| FR-005  | T060, T061, T062, T063, T064, T065      |
| FR-006  | T004, T010, T090, T092, T093, T094, T140, T141, T142, T143 |
| FR-007  | T010, T093, T094                        |
| FR-008  | T022, T023, T081                        |
| FR-009  | T041, T054, T100, T136                  |
| FR-010  | T021, T053, T071, T072, T073            |
| FR-011  | T072, T073, T074                        |
| FR-012  | T021, T023, T081, T083                  |
| FR-013  | T010, T032, T033, T041, T052, T054, T081, T082b, T082c|
| FR-014  | T010, T032, T033, T041, T052, T054, T081, T082c|
| FR-015  | T080, T081, T082                        |
| FR-016  | T002, T003, T004, T005, T030, T082      |
| FR-017  | T131, T132, T133, T136, T138            |
| FR-018  | T130, T131, T134, T135, T137            |
| FR-019  | T126, T127                              |
| NFR-001 | T072, T074                              |
| NFR-002 | T002, T003, T010                        |
| NFR-003 | T041                                    |
| NFR-004 | T041, T042, T121, T133                  |
| NFR-005 | T040, T041, T042                        |
| NFR-006 | T130, T134, T138                        |
| SC-001  | T083                                    |
| SC-002  | T083                                    |
| SC-003  | T094                                    |
| SC-004  | T100, T136                              |
| SC-005  | T064, T065                              |
| SC-006  | T074 (plus every `[GATE]` task: T007, T013, T024, T034, T055, T065, T074, T084, T095, T114, T128, T138, T145) |
| SC-007  | T023, T083                              |
| SC-008  | T054, T083                              |
| SC-009  | T134, T138                              |
| SC-010  | T141, T143, T144                        |
