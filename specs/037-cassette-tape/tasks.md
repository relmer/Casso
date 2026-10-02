# Tasks: Apple II Cassette Tape Support

**Input**: Design documents from `specs/037-cassette-tape/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Requested by the spec's Testing Approach. Write each test before its
implementation, confirm it fails first, and mutate what it covers to confirm it
notices (copilot-instructions, "Degraded Operation Must Be Observable").

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an
  incomplete task)
- **[Story]**: US1 load, US2 fast loading, US3 save, US4 deck controls

## Path Conventions

- Core code: `CassoEmuCore/`. Tests: `UnitTest/`. Nothing goes in `Casso/`.
- Every new `.h`/`.cpp` is added by hand to `CassoEmuCore/CassoEmuCore.vcxproj`
  or `UnitTest/UnitTest.vcxproj`. There are no filters files.
- Every new file: `Pch.h` first, quoted includes only, 5-line top-level
  spacing, `////` banners, EHM single exit. Run `scripts/CheckStyle.ps1 -Mode
  Staged` before committing a new file.

---

## Phase 1: Setup

- [X] T001 Run `scripts/FetchRoms.ps1 -Fixtures` and confirm `UnitTest/Fixtures/Apple2.rom`, `Apple2Plus.rom` and `Apple2e.rom` exist; build x64 Debug through `scripts/Build.ps1` to get a known-green baseline
- [X] T002 Create the `CassoEmuCore/Devices/Tape/` folder and the empty test files listed in plan.md's structure, registering each in the two `.vcxproj` files

---

## Phase 2: Foundational (blocks every story)

**Purpose**: the pure audio and signal layer, plus the test tape encoder every
story's tests depend on.

### Tests

- [X] T003 [P] Write `UnitTest/EmuTests/WavCodecTests.cpp`:
  - parse PCM 8/16/24/32-bit, float 32/64, and `WAVE_FORMAT_EXTENSIBLE`, at 8 kHz and 96 kHz
  - mono, and stereo mixed to mono; stereo with an inverted channel falls back to the louder channel
  - zero-length data is valid
  - truncated or garbage headers fail with `ERROR_INVALID_DATA`
  - writing 16-bit mono and reading it back gives the same samples (contracts/tape-files.md)
- [X] T004 [P] Write `UnitTest/EmuTests/AiffCodecTests.cpp`: AIFF and AIFF-C `NONE`/`sowt`, 8-32-bit, the 80-bit extended sample rate, mono/stereo, malformed headers rejected
- [X] T005 [P] Write `UnitTest/EmuTests/TapeSignalDecoderTests.cpp`:
  - a clean square wave gives exactly its edges, interpolated to under 0.1 sample
  - a 1 kHz sine with DC offset ±0.4 gives no missing or extra transitions
  - a gain ramp from 0.05 to 1.0 decodes
  - noise at about 15 dB SNR gives no chatter
  - silence and low hiss give zero transitions
  - inverted polarity gives the same edge times with the opposite initial level

### Implementation

- [X] T006 [P] Implement `TapeTestEncoder` in `UnitTest/EmuTests/TapeTestEncoder.{h,cpp}`. It generates the Apple II tape signal in memory from a byte array:
  - 770 Hz leader, then a sync half-cycle at 2500 Hz and one at 2000 Hz
  - 1-bit = 1000 Hz cycle, 0-bit = 2000 Hz cycle; trailing XOR checksum (seed $FF)
  - parameters: sample rate, bit depth, leader seconds, speed factor, DC offset, gain ramp, seeded noise SNR, polarity, channels
  - outputs a WAV byte buffer through `WavCodec`
  - Test code only; it is never linked into `CassoEmuCore`
  - This project's own implementation, written with c2t's source (Egan Ford, BSD-3-Clause, https://github.com/datajerk/c2t) as the reference for leader, sync and bit timing only. Copy no c2t code; write it fresh to the project's style and EHM rules. Credit c2t in the file banner. Never build or run c2t itself
- [X] T007 [P] Implement `WavCodec` (read and 16-bit mono write over `std::vector<Byte>`) in `CassoEmuCore/Devices/Tape/WavCodec.{h,cpp}`, until T003 passes
- [X] T008 [P] Implement `AiffCodec` (read) in `CassoEmuCore/Devices/Tape/AiffCodec.{h,cpp}`, until T004 passes
- [X] T009 [P] Implement `TapeSignalDecoder` in `CassoEmuCore/Devices/Tape/TapeSignalDecoder.{h,cpp}` until T005 passes:
  - 20 Hz one-pole high-pass
  - 6 kHz low-pass only when the rate is at least 22 kHz
  - peak envelope with fast attack and about 50 ms release
  - hysteresis at about 15% of the envelope, with an absolute floor
  - linear-interpolated crossings
  - Holds no Apple byte-format knowledge (FR-004); thresholds are named constants
- [X] T010 Define `ITapeAudioDecoder` (decode bytes to mono PCM and sample rate) plus `NullTapeAudioDecoder` in `CassoEmuCore/Devices/Tape/ITapeAudioDecoder.h`, and `FakeTapeAudioDecoder` in `UnitTest/EmuTests/FakeTapeAudioDecoder.h`
- [X] T011 Implement `TapeImageLoader::Load` in `CassoEmuCore/Devices/Tape/TapeImageLoader.{h,cpp}` (the `TapeImage` struct is in `TapeImage.h`; the caller passes the bytes and whether the file is read-only):
  - read the bytes through `IDiskFileIo`
  - detect the format by content (`RIFF`, `FORM`, or MP3 sync/ID3), never by extension
  - decode, keep only the transitions and `lengthSamples`, and drop the PCM
  - set `isWritable` = "WAV and not read-only"
  - fail with `ERROR_INVALID_DATA` and the error text from contracts/tape-files.md
  - tests go in `UnitTest/EmuTests/TapeImageLoaderTests.cpp` (format detection, writable flag, failure leaves no image)

**Checkpoint**: codecs, decoder and encoder are green. Encoder output decodes
to the expected edge count.

---

## Phase 3: User Story 1 - Load a program from a tape recording (P1) MVP

**Goal**: a tape plays into $C060 bit 7 and the real ROM loads it.

**Independent test**: an encoded binary loads through Monitor `R` and BASIC
`LOAD` on each supported model, byte-for-byte.

### Tests

- [X] T012 [P] [US1] Write `UnitTest/EmuTests/CassettePortTests.cpp` per contracts/guest-io.md:
  - on the ][, ][+ and //e, $C060/$C068 bit 7 follows the deck level at the bus cycle of the access
  - bits 0-6 are unchanged from today (floating on the ][/][+, 0 on the //e)
  - bit 7 reads 0 with the deck stopped
  - $C020-$C02F read and write each toggle the output level
  - //e $C061-$C063 buttons still work
  - //c: `HasCassettePort()` is false, there is no `CassettePort` on the bus, and $C060 is still RD80SW
- [X] T013 [P] [US1] Write `UnitTest/EmuTests/TapeDeckTests.cpp`:
  - the data-model.md state transitions
  - position advances only with bus cycles; a paused CPU (no cycles) holds it
  - end of tape stops at the length
  - rewind returns to 0
  - reset or machine change stops and keeps the tape inserted
  - the cursor re-seeks correctly after rewind
- [X] T014 [US1] Write `UnitTest/EmuTests/TapeRomLoadTests.cpp`: the SC-001 real-ROM matrix using `TestMachine (id, Slots::Empty)`, `KeystrokeInjector` and `TapeTestEncoder` with a 256-byte known pattern:
  - Monitor `800.8FFR` on Apple2, Apple2Plus and Apple2e
  - Applesoft `LOAD` on Apple2Plus and Apple2e
  - Integer BASIC `LOAD` on Apple2
  - each asserts memory byte-for-byte and that `TextScreenScraper` shows no `ERR`
  - assert a non-zero case count before looping
- [X] T015 [US1] Add the SC-003 robustness cases to `UnitTest/EmuTests/TapeRomLoadTests.cpp`, on Apple2Plus with Monitor `R`:
  - speed ±3% must pass; ±5% is recorded
  - DC offset, gain ramp, 15 dB SNR noise, and inverted polarity
  - 8 kHz and 96 kHz; 8-bit, 24-bit and float; stereo
  - a tape that ends mid-load gives `ERR` or a stall, never a crash
  - a blank leader between two programs

### Implementation

- [X] T016 [US1] Implement `TapeDeck` in `CassoEmuCore/Devices/Tape/TapeDeck.{h,cpp}`:
  - the transport `TapeTransport {Empty, Stopped, Playing, Recording}`
  - `positionSample`, `playStartCycle`, the cursor, and `lastAccessCycle`
  - atomics mirrored for the UI
  - the narrow `ITapeDeckPort` interface: `ReadInputLevel (cycle)` and `OnOutputToggle (cycle)`
  - sample mapping `(cycle - playStartCycle) * sampleRate / cpuClockHz + playStartSample`, with `cpuClockHz` from the machine timing config; until T013 passes
- [X] T017 [US1] Add `virtual bool HasCassettePort() const` to `CassoEmuCore/Machines/IMachine.h` (true by default) and override it to false in `CassoEmuCore/Machines/Apple2/Apple2c/Apple2c.h`
- [X] T018 [US1] Implement the `CassettePort` device ($C020-$C02F, output flip-flop, input forward target) in `CassoEmuCore/Machines/Apple2/Common/CassettePort.{h,cpp}`. It takes the CPU's `GetBusCyclePtr()` through `SetCpuCycleSource (const uint64_t *)` and an `ITapeDeckPort *`. Register it in `CassoEmuCore/Core/ComponentRegistry.cpp`
- [X] T019 [US1] Add `CassettePort` to `GetInternalDevices()` for the ][/][+ (`Machines/Apple2/Apple2/Apple2.cpp`) and the //e (`Machines/Apple2/Apple2e/Apple2e.cpp`), excluded when `HasCassettePort()` is false. In `CassoEmuCore/Shell/MachineBuilder.cpp`, wire its cycle source and keep a typed ref in `GetRefs()`
- [X] T020 [US1] Widen `AppleGamePort` to start at $C060 and forward $C060/$C068 to `CassettePort`, in `CassoEmuCore/Machines/Apple2/Common/AppleGamePort.{h,cpp}`. Keep $C061-$C070 behavior identical
- [X] T021 [US1] In `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eKeyboard.{h,cpp}`, forward $C020-$C02F and $C060 to `CassettePort` outside //c mode. Route $C068 from `Apple2eSoftSwitchBank` the same way. Keep the //c RD80SW path untouched
- [X] T022 [US1] Give the machine host (`MachineHost::GetTapeDeck`, built in `MachineBuilder::WireCassettePort`) a `TapeDeck` that outlives machine rebuilds, and connect each newly built `CassettePort` to it. Stop the deck on soft reset, power cycle and `MachineManager::SwitchMachine`, in `CassoEmuCore/Shell/TapeManager.{h,cpp}` and `CassoEmuCore/Shell/MachineManager.cpp`; until T012-T015 pass

**Checkpoint**: US1 loads tapes by test harness. There is no UI yet.

---

## Phase 4: User Story 4 - Tape-deck controls (P2, ships with US1)

**Goal**: insert, new blank, eject, play, stop, record, rewind and position from a drive-band widget; tape reinserted at launch.

**Independent test**: insert, play part way, stop, rewind; the position tracks
each step; the //c shows no deck; a restart brings the tape back at 0.

### Tests

- [X] T023 [P] [US4] Write `UnitTest/UiTests/TapeDeckWidgetTests.cpp`, modeled on `DriveWidgetHitTests.cpp` and `DriveWidgetStateTests.cpp`:
  - hit regions for name, rewind, play, stop, record and eject
  - enabled states per contracts/tape-deck-ui.md
  - the `m:ss / m:ss` readout and progress fraction
  - `(empty)` with no tape
  - record disabled for a protected tape
- [X] T024 [P] [US4] Write the restore cases in `UnitTest/UiTests/TapeManagerTests.cpp` with `InMemoryFileSystem` (`TapeManager::RestoreTape`, since the saved-path read itself goes through the on-disk machine JSON):
  - a saved `tapePath` reinserts at position 0, stopped
  - a missing file clears the entry and leaves the deck empty (FR-016)
  - the //c never reinserts
- [ ] T025 [P] [US4] (Not done: the band placement lives in `EmulatorShell::SyncTapeChrome`, which no unit test can drive; the widget's own geometry is covered by TapeDeckWidgetTests.) Extend `UnitTest/EmuTests/ChromeBandLayoutTests.cpp` or `DriveRowLayoutTests.cpp`: the band row includes the tape deck left of the drives on the ][/][+/e, and omits it on the //c

### Implementation

- [X] T026 [US4] Pick play, stop, record, rewind and eject glyphs from a rendered MDL2 sheet (never by guessing codepoints). Done: the sheet has play, record and rewind but no eject, so all five marks are drawn as flat shapes in `TapeDeckWidget::PaintMark` instead, and nothing goes in `UnicodeSymbols.h`
- [X] T027 [US4] Implement the `TapeDeckState` UI snapshot in `CassoEmuCore/Ui/TapeDeckState.h`: path, transport, position and length in seconds, record armed, writable. It is fed from the `TapeDeck` atomics
- [X] T028 [US4] Implement `TapeDeckWidget : IDxuiControl` in `CassoEmuCore/Ui/Chrome/TapeDeckWidget.{h,cpp}`, matching `DriveWidget`:
  - a "TAPE" caption column, a marqueed name row, and a progress rail styled like the head bar
  - the transport buttons, plus `HitTest`, `OnDrop` and `SyncFromState`
  - until T023 passes
- [X] T029 [US4] Lay the widget out in the drive band in `CassoEmuCore/Shell/EmulatorShellChrome.cpp` and `Shell/Layout/DriveRowLayout.h`. Hide it when `HasCassettePort()` is false, in all three visibility sites (`EmulatorShell.cpp:1082-1110`, `EmulatorShellPresent.cpp:912-927`, `EmulatorWindow.cpp:1936-1948`). Register its drop hit rect; until T025 passes
- [X] T030 [US4] Add insert, eject, rewind, play and stop to `TapeManager` (`CassoEmuCore/Shell/TapeManager.{h,cpp}`):
  - the tape picker via `IHostDialogs` (done as a plain file picker opening on the inserted tape's folder; a recent-tapes list is left for later)
  - drag-and-drop insertion through `DxuiDragDropTarget` with a tape-extension filter (not done; see T058)
  - transport commands posted to the CPU thread (command-thread routing)
- [X] T031 [US4] Route widget clicks in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`. Add the tape items (Insert tape..., New blank tape..., Play tape, Stop tape, Rewind tape, Eject tape; sentence case) to the Disk menu through `WindowCommandManager`, disabled on the //c
- [X] T032 [US4] Persist the per-machine `tapePath` through `Config/DiskSettings.{h,cpp}` (beside `disk1Path`). Reinsert at launch and on machine switch via `AutoMountResolver`, rewound to 0 and stopped. Clear stale entries. Until T024 passes
- [X] T033 [US4] Implement `MfTapeAudioDecoder` (MP3 through a Media Foundation source reader over `MFCreateMFByteStreamOnStream`, following `Audio/PrinterAudioSource.cpp`) in `CassoEmuCore/Devices/Tape/MfTapeAudioDecoder.{h,cpp}`. Inject it into `TapeImage` from the shell
- [X] T034 [P] [US4] Write `UnitTest/UiTests/DeskSceneRecorderTests.cpp`, modeled on the existing desk-scene hit and layout tests:
  - a `CassetteRecorder` device is present on the ][/][+/e and absent on the //c
  - hit regions for the record, play, rewind and stop/eject keys, the cassette door and the counter map to the same commands as the flat widget
  - the flat widget is hidden while the 3D scene is active
  - Deviation: the recorder is one hit target, its whole case, which picks a tape like the flat widget's name area; there are no per-key, door or counter regions yet. The tests cover the model load, the layout beside the stack, the hit, and no recorder when the scene is loaded without one (the //c). The flat widget was already hidden by `SyncTapeChrome` whenever the scene is active.
- [X] T035 [US4] Author `Resources/Models/CassetteRecorder/CassetteRecorder.mesh` as an accurate model of the Panasonic RQ-309DS, the recorder Apple recommended by name. First collect reference photos (front, top, sides, back) and its published dimensions, and record the sources in `Resources/Models/CassetteRecorder/README.md`. Model it from those, never from memory: the case proportions, the piano-key row with each key's legend and color, the cassette door and window, the tape counter, the speaker grille, the jacks, the controls and the handle, plus its markings and finish. Sub-meshes are identified by material color, as in `DiskII.mesh`. Judge the result by its overall look on a screenshot, and redesign wrong forms rather than tuning constants
  - Deviation: generated by `scripts/modelgen/cad_rq309ds.py` from the radiomuseum.org dimensions and its top and side photographs (no front or back views were found). It has the case, slope, deck, grille, window, label strip, six keys and handle; key legends, the counter, the jacks and the markings are not modeled. Parts are identified by material name, as the loader now does.
- [X] T036 [US4] Add the `CassetteRecorder` `DeskDeviceKind` with its placement, key-press animation and door, and wire it to `TapeDeckState`, in `CassoEmuCore/Ui/Scene/DeskSceneModel.{h,cpp}`, `DeskSceneLayout.{h,cpp}`, `DeskSceneHitTester.{h,cpp}` and `DeskScene.cpp`. Route its hits through the same `TapeManager` commands as the flat widget. Hide the flat widget in that theme. Until T034 passes
  - Deviation: placement, contact shadow, shadow maps and the click (which opens the tape picker through `HandleTapeClick`) are built; key-press animation, the door and per-key commands are not, and nothing reads `TapeDeckState` yet.
- [ ] T037 [US4] Launch Casso (background, `--title 037-cassette-tape`) and screenshot the deck empty, loaded, playing and stopped, in both the flat theme and the 3D desk scene, and on the //c (absent). Validate visually before handoff

**Checkpoint**: a user can load a tape end to end from the UI at normal speed.

---

## Phase 5: User Story 2 - Fast loading during tape I/O (P2)

**Goal**: loads finish in seconds by default, with the user's speed restored and no audio during the override.

**Independent test**: the same tape with the preference on and off gives
identical memory; host time drops sharply with it on.

### Tests

- [X] T038 [P] [US2] Write `UnitTest/EmuTests/TapeTurboGovernorTests.cpp`:
  - on only when the preference is on, the deck is playing or recording, and `nowCycle - lastAccessCycle <= cpuClockHz / 10`
  - off at end of tape, on stop or eject, and after 100 ms with no access
  - never on with the deck stopped or empty
  - //e $C061 button polling with no tape: off
  - $C060 polling with the deck stopped: off (FR-010, SC-005)
- [X] T039 [P] [US2] Write the CPU override case in `UnitTest/EmuTests/TapeTurboGovernorTests.cpp`: `GetEffectiveSpeedMode()` returns Maximum while the override is set, and the user's mode otherwise; `GetSpeedMode()` never changes
- [X] T040 [P] [US2] Add a settings round trip to `UnitTest/UiTests/SettingsPanelStateTests.cpp`: `fastTapeLoading` defaults to true, and load, save, equality and apply all work

### Implementation

- [X] T041 [US2] Implement `TapeTurboGovernor` in `CassoEmuCore/Devices/Tape/TapeTurboGovernor.{h,cpp}`, until T038 passes
- [X] T042 [US2] Add `std::atomic<bool> m_maximumOverride`, `SetMaximumOverride` and `GetEffectiveSpeedMode()` to `CassoEmuCore/Shell/CpuManager.{h,cpp}`. Switch the three runtime readers to it: the pacing loop (`CpuManager.cpp:496`), `ExecuteCpuSlices` (`EmulatorShellCpuThread.cpp:802`) and `ShouldPublishFrame` (`EmulatorShellPresent.cpp:1166`). Until T039 passes
- [X] T043 [US2] Add a hidden `SetSuppressed (bool)` to `CassoEmuCore/WasapiAudio.{h,cpp}` that submits silence without touching master mute or volume
- [X] T044 [US2] Evaluate the governor once per slice on the CPU thread in `CassoEmuCore/Shell/EmulatorShellCpuThread.cpp`, and drive `SetMaximumOverride` and `SetSuppressed` from it
- [X] T045 [US2] Add the `fastTapeLoading` preference, following the `floppySoundEnabled` template:
  - `SettingsUiPrefs` and its setter, load, save and equality (`Ui/Settings/SettingsPanelState.{h,cpp}`)
  - an `ISettingsApplySink` virtual and `SettingsApplyAdapter`
  - the default in `Config/UserConfigStore.cpp`
  - a "Fast tape loading" toggle (placed on the Disk page, `Ui/Settings/DiskPage.cpp`, beside the drive audio toggle it copies)
  - until T040 passes
- [X] T046 [US2] Implement `TapeAudioSource : IDriveAudioSource` in `CassoEmuCore/Audio/TapeAudioSource.{h,cpp}`. It synthesizes a square wave from the transitions at the deck position while playing and the override is off. Mix it through its own `DriveAudioMixer` (`m_tapeAudioMixer`, a new `SubmitFrame` argument) so neither the drive nor the Mockingboard setting silences it (FR-012)
- [ ] T047 [US2] Manual check per quickstart.md step 2: time a 16 KB load with the preference on (under 10 s host time, SC-004) and off (real time, audible). Record both numbers in the commit message

---

## Phase 6: User Story 3 - Save a program to tape (P3)

**Goal**: Monitor `W` and BASIC `SAVE` with record armed overwrite the tape from the current position.

**Independent test**: save, reload into a cleared machine, compare.

### Tests

- [X] T048 [P] [US3] Write `UnitTest/EmuTests/TapeRecorderTests.cpp`:
  - toggles map to the expected sample edges at the tape's rate, as a square wave at 80% full scale
  - a splice at mid-tape leaves earlier samples untouched and extends past the end
  - the commit writes through `FakeDiskFileIo::ReplaceAtomically` and re-decodes the transitions
  - record is unavailable for MP3, AIFF and read-only tapes
  - a toggle with record not armed is discarded
- [X] T049 [US3] Add the SC-002 round trips to `UnitTest/EmuTests/TapeRomLoadTests.cpp`:
  - Monitor `800.8FFW` onto a blank tape on Apple2Plus and Apple2e, then reinsert into a fresh machine, `800.8FFR`, and compare
  - Applesoft `SAVE`/`LOAD` on Apple2Plus

### Implementation

- [X] T050 [US3] Implement `RecordingCapture` and `TapeRecorder` (render, re-read the WAV through `IDiskFileIo`, splice, write it back, re-decode, drop the PCM) in `CassoEmuCore/Devices/Tape/TapeRecorder.{h,cpp}`. Hook the capture into `TapeDeck::OnOutputToggle` while recording. Commit on stop and on eject. Until T048 passes
- [X] T051 [US3] Implement new blank tape in `TapeManager`:
  - `IHostDialogs::PickFileToSave` (the dialog's own default folder; a remembered tape folder is left for later)
  - write a zero-length 44.1 kHz 16-bit mono WAV atomically, then insert it
  - every backing-out path leaves the deck unchanged
- [X] T052 [US3] Wire record arming in the widget and menu, with the protected badge per contracts/tape-deck-ui.md; until T049 passes
- [ ] T053 [US3] Manual check per quickstart.md step 4, including opening a Casso-written WAV in another Apple II tape tool (SC-002 scenario 2)

---

## Phase 7: Polish & Cross-Cutting

- [ ] T054 Manual quickstart.md steps 1, 3 and 5, including one real Internet Archive tape as WAV and as MP3 (SC-006)
- [X] T055 Search all new code for magic numbers, British spelling, `name` used as a verb in strings or comments, and `\w \(\)`
- [X] T056 Pre-merge gate: full suite Debug and Release x64 (`scripts/RunTests.ps1 -Build`), `scripts/Build.ps1 -RunCodeAnalysis`, ARM64 build, and `scripts/CheckStyle.ps1 -Mode Tree` after `git add -A`
- [ ] T057 After the owner has tested and approved: draft the CHANGELOG `[Unreleased]` entry (`GH #160: ...`) and the README feature line for owner approval before committing

---

### Follow-ons found during implementation

- [ ] T058 [US4] Drag a tape file onto the tape widget to insert it: register the widget's rect with `DxuiDragDropTarget` alongside the drive rects (`EmulatorShell::InstallDragDropTarget`) and accept WAV, AIFF and MP3
- [X] T059 [US4] Recent tapes (done as the shared disk picker and recent list, filtered by extension; the folders of every recent entry are scanned for both kinds): a `GlobalUserPrefs::recentTapes` list (`DiskMru`) and an MRU picker matching the disk picker, plus a remembered folder for new blank tapes
- [ ] T060 [US4] Marquee a tape name too long for the widget's name row, as `DriveWidget::PaintBasenameLabel` does

### After 035-debugger lands on master (merge master into 037 first; do not merge or rebase onto 035 before it ships)

- [ ] T061 Merge master into 037 once 035 has shipped; expect conflicts in the shared shell plumbing (CpuCommandDispatcher, EmulatorShellCpuThread, MachineHost, menus, Dxui)
- [ ] T062 Debugger reads of $C020-$C02F and $C060/$C068 MUST NOT have side effects: give `CassettePort` a peek path through 035's side-effect-free read mechanism, using `TapeDeck::PeekLevel` (no access recorded, no toggle, no recording click, no fast-load trigger)
- [ ] T063 Single-instruction steps (035 FR-140) MUST also silence `m_tapeAudioMixer`
- [ ] T064 Debugger Monitor `R`/`W` with no filename (035 FR-020): print that tape loads go through the guest's own Monitor with the deck playing; keep the host-file meaning when a filename is given
- [ ] T065 Soft-switch operand names (035 FR-112): TAPEOUT for $C020-$C02F (ROMBANK at $C028 on the //c), TAPEIN for $C060 (RD80SW on the //c), TAPEIN mirror for $C068
- [ ] T066 Cassette device panel (035 US9): transport, position/length, input level, output flip-flop, toggle count, last access cycle, fast-load override state
- [ ] T067 Script commands (035 FR-014 family) `tape insert|play|stop|rewind` and a `CassoCli debug --tape` option

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup -> Foundational -> US1 -> {US4, US2, US3} -> Polish
- US4 depends on US1 (the deck and port). US2 depends on US1 and uses US4's preference UI only for T045's checkbox. US3 depends on US1, and on US4 for the blank-tape and record UI.

### User Story Dependencies

- US1: none beyond Foundational. This is the MVP.
- US4: US1. It ships with US1 per the spec.
- US2: US1. It can start in parallel with US4.
- US3: US1 and US4.

### Within Each User Story

Tests first, then the model, then the device and shell wiring, then the UI, then a manual check.

### Parallel Opportunities

- T003-T005 and T006-T009 are separate files, so they can run in parallel.
- T012/T013, T023-T025 and T034, T038-T040 and T048 are test files that can be written in parallel within their phase.
- US2 (T038-T046) and US4 (T023-T037) touch disjoint files except `EmulatorShellCpuThread.cpp`, so coordinate that one file.

## Parallel Example: User Story 1

```text
T012 CassettePortTests.cpp  |  T013 TapeDeckTests.cpp
then T016 TapeDeck -> T017/T018 -> T019-T021 -> T022
```

## Implementation Strategy

### MVP First (User Story 1 Only)

Phases 1-3 give a harness-proven load path through the real ROM. Stop and
validate before any UI.

### Incremental Delivery

Push the branch after each phase. US1 plus US4 is the first user-visible drop,
then US2, then US3. CHANGELOG and README come last, after the owner signs off.

### Parallel Team Strategy

One session owns the bus and deck (US1, US3); another can take the widget (US4)
and fast loading (US2) once US1 lands.

## Notes

- Do not comment on or close GH #160.
- Nothing in `CassoEmuCore` may parse Apple tape bytes. `TapeTestEncoder` is test-only.
- Every task's tests must fail before the implementation and go red under a stubbed implementation.

- [X] T068 Remodel the RQ-309DS from the black-key version in `scripts/modelgen/cad_rq309ds.py`; the first pass reads as lo-fi. Reference photos (eBay listing via cassettedecks.us; full size is the `$_57.JPG` form):
  - top views: https://i.ebayimg.com/00/s/MTYwMFgxMjAw/z/ylEAAOSwkbple-RO/$_57.JPG and https://i.ebayimg.com/00/s/MTYwMFgxMjAw/z/jksAAOSwQrlle-RU/$_57.JPG
  - grille and window close-up: https://i.ebayimg.com/00/s/MTYwMFgxMjAw/z/d-UAAOSws85le-RP/$_57.JPG
  - door open, keys and legend strip: https://i.ebayimg.com/00/s/MTYwMFgxMjAw/z/9ToAAOSwKAxle-RQ/$_57.JPG
  - underside: https://i.ebayimg.com/00/s/MTYwMFgxMjAw/z/Z24AAOSwqj9le-RR/$_57.JPG
  - back end with the tone and volume thumbwheels: https://i.ebayimg.com/00/s/MTYwMFgxMjAw/z/HmwAAOSwqwNle-RS/$_57.JPG
  - What the photos show:
    - The body is black textured plastic with a black frame around the top.
    - The speaker grille is a fine-perforated silver plate covering about the back 40% of the top.
    - The clear smoked cassette door hinges at the grille edge and has "AUTO STOP" and "AC/BATTERY" printed on it.
    - Behind the door, the cassette and transport show through.
    - Below the door, a silver strip carries a black "Panasonic" band with a slotted condenser-mic grille on its left.
    - Under the band is a legend row: RECORD, REW, FF, PLAY, STOP, EJECT.
    - Six black keys sit on the sloped front, each with a dished oval face.
    - A chrome handle wraps the front end.
    - The tone and volume thumbwheels sit at the back end, between two screws.
