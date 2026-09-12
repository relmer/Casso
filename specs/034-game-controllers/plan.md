# Implementation Plan: Physical Game Controllers

**Branch**: `034-game-controllers` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/034-game-controllers/spec.md` (GH #97)

## Summary

Xbox-class controllers are read through XInput and every other controller through DirectInput 8, on a dedicated controller thread that wakes on DirectInput's own state-change events and polls XInput at a period measured from the real controllers (R13), handles hot-plug through HID device notifications, and applies input only while Casso is active. Both APIs sit behind one `IControllerBackend` seam that delivers a normalized sample; everything the spec asks for (calibration, deadzone, control mapping, profiles, selection and fallback, press-to-assign) is pure logic in `CassoEmuCore/Controllers/`, driven in tests by a scripted fake backend.

A new `GamePortInputMixer` becomes the single writer of PDL0-PDL3 and PB0-PB2. Today the keyboard, Alt and mouse sources overwrite each other, and a controller on another thread could not satisfy FR-014 against that; the existing writers are migrated onto the mixer first.

The unit of assignment is an **analog axis**, not a controller slot. A machine exposes four axes (][, ][+, //e) or two (//c, whose PDL2/PDL3 lines carry the mouse directions instead), each axis has at most one owner, and a controller claims one axis (a paddle), two (a joystick) or four (FR-034 to FR-038). Two-player play, one-controller four-axis play and the //c's reduced port all fall out of that one rule rather than needing separate cases. This lands late, in phase 9, but it shapes three foundation types from the start: `GamePortContribution` carries four paddles, `ControlMapping` has four axis targets, and `AxisOwner` is per-axis rather than one owner for the pair.

Two findings shape delivery:

1. **A hardware check comes first** (research R2, R4, R13): the XInput packet rate, DirectInput change events, whether wireless Xbox power on and off raises HID notifications, and whether XInput keeps delivering while the Settings sheet is the active Casso window.
2. **032 is on master and merged into this branch** (R11). The Machine menu and the toolbar input control are built on its shipped widgets (`DxuiCommand`, `DxuiPopupMenu` submenus, `DxuiToolbar`, `InputClusterEntry`), which differ from 032's contracts.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145

**Primary Dependencies**: Windows SDK only: XInput 1.4 (`xinput.lib`), DirectInput 8 (`dinput8.lib`, `dxguid.lib`), HID (`hid.lib`) for serial numbers, `RegisterDeviceNotification`. Dxui for the Controllers page, and 032's `DxuiCommand`, `DxuiPopupMenuItem` and `DxuiToolbar` for the menu and toolbar.

**Storage**: `GlobalUserPrefs` JSON (new `controllers` section) and the per-machine `$cassoUiPrefs` block ([contracts/prefs-schema.md](contracts/prefs-schema.md))

**Testing**: Microsoft C++ Unit Test Framework; new `UnitTest/ControllerTests/` with `FakeControllerBackend` and `RecordingGamePortSink`; existing `InMemoryFileSystem` for prefs

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only; x64 Debug and Release are the test bar)

**Project Type**: Desktop application (emulator)

**Performance Goals**: DirectInput devices read on their own change events; XInput polled at the measured packet rate (R13; measured 125 packets/s, 8 ms); a change reaches the game port within one displayed frame (SC-002); no sink writes while input is unchanged; no measurable cost with no controller selected (SC-007: with nothing connected the thread waits with no timeout; controllers are found by HID arrival notifications, never by polling empty slots)

**Constraints**: no redistributables; no real device access in unit tests; no undocumented API on a required path (`XInputGetCapabilitiesEx` is optional with fallback, R6); input applies only while Casso is active, and Xbox-class controllers always use XInput (FR-033)

**Scale/Scope**: up to 4 XInput slots plus any number of DirectInput devices; up to four analog axes, each with at most one owner, so several controllers drive the port at once (FR-034 to FR-038)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How |
|---|---|---|
| I. Code Quality | Pass | EHM on every failable path, including the backend's per-device read retry. Decoders and evaluators are short pure functions. Helpers are class statics. |
| II. Testing Discipline, Test Isolation | Pass | Only `Win32ControllerBackend` touches devices and it holds no rule worth asserting: POV decoding, range normalization and XInput bit mapping are pure decoders tested with synthetic `DIJOYSTATE2`/`XINPUT_STATE`. Everything else runs against `FakeControllerBackend`. Degraded operation is observable: a failed read reports disconnected, never a healthy rest sample (FR-015). |
| III. UX Consistency | Pass | Settings page follows the sheet's Apply/Cancel; notices reuse the existing overlay; no CLI change. |
| IV. Performance | Pass | Change-only sink writes; no polling of empty XInput slots, and no timed polling at all while no controller is selected; no allocation in the sample loop (fixed-size sample). |
| V. Simplicity | Pass with note | The mixer adds a class, justified by FR-014 (research R9). The dedicated thread is needed because the UI frame hook stops while the machine is idle and in modal loops (R3). |
| VI. Thin Executable, Testable Core | Pass | All code in `CassoEmuCore`; nothing new in Dxui. `Casso.exe` unchanged. The Win32 backend lives in core like `Win32HostCapsLock`. |
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
│   ├── ControllerTokens.h/.cpp        # token text for keys and ControlId
│   ├── ControlLabels.h/.cpp           # display labels per control and model kind
│   ├── InputModeRules.h/.cpp          # mutual exclusion of arrows, paddle and controller selection
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
│   ├── ControllerInputThread.h/.cpp   # new: owns the thread and wait loop (the backend owns the window)
│   ├── MachineManager.cpp             # attach/detach the sink around machine build and teardown
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
    └── Chrome/
        │                              # (paddle-source and profile rows live in EmulatorCommands)
        ├── EmulatorCommands.h/.cpp    # submenu marker in the menu table; Controller Settings item
        ├── MainMenu.h/.cpp            # unchanged: the Machine menu carries no dynamic rows
        ├── InputMonoGlyphs.h/.cpp     # monoline gamepad, joystick, paddle, keys
        └── EmulatorCommands.h/.cpp    # + paddle-source picker rows and the mouse toggle

UnitTest/
└── ControllerTests/                   # new
    ├── FakeControllerBackend.h
    ├── RecordingGamePortSink.h
    ├── ControllerTokensTests.cpp
    ├── MachineGamePortSinkTests.cpp
    ├── InputModeRulesTests.cpp
    ├── PaddleSourceRowsTests.cpp
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

**Structure Decision**: a new `CassoEmuCore/Controllers/` folder for the pure logic, the device seam beside the existing seams, shell wiring in `Shell/`, the page beside the other Settings pages, and menu and toolbar rows in the chrome files 032 created. Shell and chrome decisions that would otherwise be untestable (input-mode exclusion, notice expiry, deferred menu rebuild) are factored into small pure classes with their own tests.

## Delivery Slices

Each slice matches a phase in [tasks.md](tasks.md), leaves the build green, and is committed on its own (constitution: commit per phase).

| Phase | Slice | Depends on | Covers |
|---|---|---|---|
| 1 | **Hardware check**: throwaway probe, not committed; XInput packet rate, DirectInput change events, wireless power on/off notifications, XInput with a second top-level window active | none | Records R2, R4, R13 |
| 2 | **Foundation**: types and tokens; mixer and `MachineGamePortSink` with every existing writer migrated; seam, decoders, Win32 backend, controller thread | 1 | FR-001, FR-002, FR-014, FR-015, FR-017 |
| 3 | **US1 play (MVP)**: deadzone, default mapping, evaluator, service, activation gate, temporary first-controller selection | 2 | US1, FR-003-006, FR-009, FR-033 |
| 4 | **US2 selection**: selection policy (automatic, adoption), per-machine persistence, input-mode exclusion, notice, and ONE paddle-source picker on the command bar wearing the source that drives (no cascade; the Machine menu keeps its existing toggles) | 3 | US2, FR-008, FR-008b, FR-011, FR-031, FR-032 |
| 5 | **US3 hot-plug**: disconnect release, stand-in by another attached controller then the arrow keys, reconnect, status LED and tooltip | 4 | US3, FR-008a, FR-010, FR-013 |
| 6 | **US4 calibration**: automatic and user calibration per unit, calibration persistence | 3 | US4, FR-007, FR-007a, FR-018, FR-018a |
| 7 | **US5 remapping**: Controllers page, capture, rate response, PB2, Default-profile mapping and deadzone persistence, Controller Settings command | 4, 6 | US5, FR-012, FR-019-025, FR-021a |
| 8 | **US6 profiles**: named profiles, Paddles template, active profile per machine, Controller Profile submenus | 7 | US6, FR-026-030, SC-010 |
| 9 | **US7 two players**: per-machine axis budget, per-axis ownership and displacement, multi-controller assignment and its persistence, assignment UI on the Controllers page, //c reduced to two axes | 7, 8 | US7, FR-034-038, SC-011, SC-012 |
| 10 | **Polish**: measurements, CHANGELOG, README, gates | 9 | SC-002, SC-005, SC-007 |

## Complexity Tracking

No constitution violations.
