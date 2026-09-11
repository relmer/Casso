# Implementation Plan: Physical Game Controllers

**Branch**: `034-game-controllers` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/034-game-controllers/spec.md` (GH #97)

## Summary

Xbox-class controllers are read through XInput and every other controller through DirectInput 8, on a dedicated controller thread that wakes on DirectInput's own state-change events and polls XInput at a period measured from the real controllers (R13), handles hot-plug through HID device notifications, and applies input only while Casso is active. Both APIs sit behind one `IControllerBackend` seam that delivers a normalized sample; everything the spec asks for (calibration, deadzone, control mapping, profiles, selection and fallback, press-to-assign) is pure logic in `CassoEmuCore/Controllers/`, driven in tests by a scripted fake backend.

A new `GamePortInputMixer` becomes the single writer of PDL0/PDL1/PB0/PB1. Today the keyboard, Alt and mouse sources overwrite each other, and a controller on another thread could not satisfy FR-014 against that; the existing writers are migrated onto the mixer first.

Two findings change delivery order and need the owner's attention:

1. **Focus and sample rate need a hardware check** (research R2, R13). Background input was dropped in favor of XInput, so the check now answers two narrower questions: whether XInput keeps delivering while the Settings sheet is the active Casso window, and what report rate each controller actually delivers.
2. **032 also replaces the Machine menu's command table**, not only the toolbar (R11). The controller and profile menu entries therefore move behind 032 along with the toolbar pickers. Until then the feature is complete through automatic selection (FR-032) and the Controllers page, which gains a per-machine controller and active-profile choice. This narrows FR-008/FR-028's "Machine menu" for the pre-032 slices; the spec says the menu is part of the input selector.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145

**Primary Dependencies**: Windows SDK only: XInput 1.4 (`xinput.lib`), DirectInput 8 (`dinput8.lib`, `dxguid.lib`), HID (`hid.lib`) for serial numbers, `RegisterDeviceNotification`. Dxui for the Controllers page; after 032, `DxuiCommand` / `DxuiDropdownItem` / `DxuiToolbar`.

**Storage**: `GlobalUserPrefs` JSON (new `controllers` section) and the per-machine `$cassoUiPrefs` block ([contracts/prefs-schema.md](contracts/prefs-schema.md))

**Testing**: Microsoft C++ Unit Test Framework; new `UnitTest/ControllerTests/` with `FakeControllerBackend` and `RecordingGamePortSink`; existing `InMemoryFileSystem` for prefs

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only; x64 Debug and Release are the test bar)

**Project Type**: Desktop application (emulator)

**Performance Goals**: DirectInput devices read on their own change events; XInput polled at the measured packet rate (R13; provisional 8 ms, unmeasured); a change reaches the game port within one displayed frame (SC-002); no sink writes while input is unchanged; no measurable cost with no controller selected (SC-007: the thread idles on its message wait and rechecks empty XInput slots once per second)

**Constraints**: no redistributables; no real device access in unit tests; no undocumented API on a required path (`XInputGetCapabilitiesEx` is optional with fallback, R6); input applies only while Casso is active, and Xbox-class controllers always use XInput (FR-033)

**Scale/Scope**: up to 4 XInput slots plus any number of DirectInput devices; one controller drives the game port at a time

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How |
|---|---|---|
| I. Code Quality | Pass | EHM on every failable path, including the backend's per-device read retry. Decoders and evaluators are short pure functions. Helpers are class statics. |
| II. Testing Discipline, Test Isolation | Pass | Only `Win32ControllerBackend` touches devices and it holds no rule worth asserting: POV decoding, range normalization and XInput bit mapping are pure decoders tested with synthetic `DIJOYSTATE2`/`XINPUT_STATE`. Everything else runs against `FakeControllerBackend`. Degraded operation is observable: a failed read reports disconnected, never a healthy rest sample (FR-015). |
| III. UX Consistency | Pass | Settings page follows the sheet's Apply/Cancel; notices reuse the existing overlay; no CLI change. |
| IV. Performance | Pass | Change-only sink writes; empty-slot backoff; no allocation in the sample loop (fixed-size sample). |
| V. Simplicity | Pass with note | The mixer adds a class, justified by FR-014 (research R9). The dedicated thread is needed because the UI frame hook stops while the machine is idle and in modal loops (R3). |
| VI. Thin Executable, Testable Core | Pass | All code in `CassoEmuCore` (and Dxui for nothing new before 032). `Casso.exe` unchanged. The Win32 backend lives in core like `Win32HostCapsLock`. |
| Dependencies | Pass | Windows SDK only; no allowlist change. |

**Post-design re-check**: Pass. The contracts keep the device boundary to one seam with a fake, the mixer is pure with a recording sink, and prefs use the existing in-memory file system. No violations to track.

## Project Structure

### Documentation (this feature)

```text
specs/034-game-controllers/
├── spec.md
├── plan.md              # This file
├── research.md          # Phase 0
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── controller-backend.md
│   ├── game-port-mixer.md
│   └── prefs-schema.md
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks, not created here
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Controllers/                       # new: pure logic
│   ├── ControllerTypes.h              # keys, ControlId, ControllerSample, ControllerDeviceInfo
│   ├── ControlLabels.h/.cpp           # display labels per control and model kind
│   ├── DirectInputSampleDecoder.h/.cpp
│   ├── XInputSampleDecoder.h/.cpp
│   ├── ControllerCalibration.h/.cpp   # automatic learning + user calibration
│   ├── DeadzoneShaper.h/.cpp          # radial/axial, rescale to full range
│   ├── ControlMapping.h/.cpp          # bindings + DefaultMapping
│   ├── MappingEvaluator.h/.cpp        # sample -> GamePortContribution
│   ├── ControllerProfileStore.h/.cpp  # models, profiles, calibration, JSON
│   ├── ControllerSelectionPolicy.h/.cpp
│   ├── ControlCapture.h/.cpp          # press-to-assign, ignores pre-held controls
│   ├── GamePortInputMixer.h/.cpp
│   └── ControllerInputService.h/.cpp  # thread body: backend + policy + evaluator -> mixer
├── Seams/
│   ├── IControllerBackend.h           # new
│   └── Win32ControllerBackend.h/.cpp  # new
├── Config/
│   ├── GlobalUserPrefs.h/.cpp         # + controllers section
│   └── MachineInputPrefs.h/.cpp       # + controller, controllerProfile
├── Shell/
│   ├── MachineGamePortSink.h/.cpp     # new: IGamePortSink over MachineRefs
│   ├── ControllerInputThread.h/.cpp   # new: owns the thread and message-only window
│   ├── EmulatorShell.h                # own service, mixer, thread
│   ├── EmulatorShellPrefs.cpp         # adopt/persist controller keys
│   ├── EmulatorShellPresent.cpp       # generalized transient notice
│   ├── EmulatorShellDialogs.cpp       # OpenSettings (page)
│   └── Window/EmulatorWindowInput.cpp # existing writers -> mixer; selection interplay
└── Ui/
    ├── Settings/
    │   ├── ControllersPage.h/.cpp     # new DxuiPropertyPage
    │   ├── ControllersPageState.h/.cpp# new: pure page model (edits, capture, pending apply)
    │   ├── SettingsSheet.h/.cpp       # + page
    │   └── SettingsApplyController.h/.cpp # + controllers baseline/dirty/commit/revert
    └── Chrome/                        # after 032 only
        ├── EmulatorCommands.h/.cpp    # controller + profile commands (032's table)
        └── InputClusterEntry.h/.cpp   # controller rows in the picker, status decoration

UnitTest/
└── ControllerTests/                   # new
    ├── FakeControllerBackend.h
    ├── RecordingGamePortSink.h
    ├── DirectInputSampleDecoderTests.cpp
    ├── XInputSampleDecoderTests.cpp
    ├── CalibrationTests.cpp
    ├── DeadzoneShaperTests.cpp
    ├── MappingEvaluatorTests.cpp
    ├── ControllerProfileStoreTests.cpp
    ├── ControllerSelectionPolicyTests.cpp
    ├── ControlCaptureTests.cpp
    ├── GamePortInputMixerTests.cpp
    ├── ControllerInputServiceTests.cpp
    └── ControllersPageStateTests.cpp
```

**Structure Decision**: a new `CassoEmuCore/Controllers/` folder for the pure logic, the device seam beside the existing seams, shell wiring in `Shell/`, and the page beside the other Settings pages. Chrome changes wait for 032 and land in the files 032 creates.

## Delivery Slices

Each slice leaves the build green and is committed on its own (constitution: commit per phase).

| # | Slice | Depends on | Stories | Notes |
|---|---|---|---|---|
| 0 | **Hardware check**: throwaway probe (not committed) reading XInput and DirectInput from a worker thread; measures report rate per controller and whether XInput delivers while a second top-level window of the process is active | none | gate | Records R2 and R13 outcomes |
| 1 | **Mixer**: `GamePortInputMixer`, `MachineGamePortSink`, migrate every existing writer | none | FR-014 | Pure refactor for existing behavior; existing input tests unchanged |
| 2 | **Backend + decoders**: seam, Win32 backend, decoders, controller thread, hot-plug | 0 | FR-001, FR-002, FR-015 | |
| 3 | **Play (MVP)**: default mapping, deadzone, evaluator, service, automatic selection, disconnect fallback, notice, per-machine persistence | 1, 2 | US1, US2 (auto), US3, FR-032, FR-033 | Usable end to end with no UI |
| 4 | **Calibration** | 3 | US4 | |
| 5 | **Controllers page**: selection, live readings, mapping edit, capture, deadzone, calibrate, Apply/Cancel, open-to-page | 3, 4 | US2 (manual), US5 | |
| 6 | **Profiles**: store, create/rename/delete/reset, active profile per machine, rate response, Paddles template, PB2 target | 5 | US6, FR-020, FR-021a | |
| 7 | **Menu + toolbar** on 032's command table, dropdown and input cluster | 6, 032 on master | FR-008, FR-028, FR-031, SC-010 | Merge master first |
| 8 | **Polish**: CHANGELOG, README, full gates | 7 | | |

Slices 0-6 can merge to master before 032 if the owner wants the feature early; slice 7 then follows as a second merge.

## Complexity Tracking

No constitution violations.
