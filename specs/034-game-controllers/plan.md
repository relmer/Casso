# Implementation Plan: Physical Game Controllers

**Branch**: `034-game-controllers` (worked on `claude/issue-156-fix-6f63fe` for GH #156) | **Date**: 2026-09-11, updated 2026-09-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/034-game-controllers/spec.md` (GH #97)

## Summary

Xbox-class controllers are read through XInput and every other controller through DirectInput 8, on a dedicated controller thread that wakes on DirectInput's own state-change events and polls XInput at a period measured from the real controllers (R13), handles hot-plug through HID device notifications, and applies input only while Casso is active. Both APIs sit behind one `IControllerBackend` seam that delivers a normalized sample; everything the spec asks for (calibration, deadzone, control mapping, profiles, selection and fallback, press-to-assign) is pure logic in `CassoEmuCore/Controllers/`, driven in tests by a scripted fake backend.

A new `GamePortInputMixer` becomes the single writer of PDL0-PDL3 and PB0-PB2. Today the keyboard, Alt and mouse sources overwrite each other, and a controller on another thread could not satisfy FR-014 against that; the existing writers are migrated onto the mixer first.

The unit of assignment is an **analog axis**, not a controller slot. A machine exposes four axes (][, ][+, //e) or two (//c, whose PDL2/PDL3 lines carry the mouse directions instead), each axis has at most one owner, and a controller claims one axis (a paddle), two (a joystick) or four (FR-034 to FR-038). Two-player play, one-controller four-axis play and the //c's reduced port all fall out of that one rule rather than needing separate cases. This lands late, in phase 9, but it shapes three foundation types from the start: `GamePortContribution` carries four paddles, `ControlMapping` has four axis targets, and `AxisOwner` is per-axis rather than one owner for the pair.

Two findings shape delivery:

1. **A hardware check comes first** (research R2, R4, R13): the XInput packet rate, DirectInput change events, whether wireless Xbox power on and off raises HID notifications, and whether XInput keeps delivering while the Settings sheet is the active Casso window.
2. **032 is on master and merged into this branch** (R11). The Machine menu and the toolbar input control are built on its shipped widgets (`DxuiCommand`, `DxuiPopupMenu` submenus, `DxuiToolbar`, `InputClusterEntry`), which differ from 032's contracts.

### 2026-09-27: two always-present player slots (GH #156)

The spec's Session 2026-09-27 replaces single-source and multiplayer modes with **two global player slots that are always present**, so two controllers play two players with no setup. Phases 12-18 of [tasks.md](tasks.md) build it on top of what Phases 1-11 shipped; research [R16-R25](research.md#2026-09-27-two-always-present-player-slots-gh-156) records the decisions.

- **Who plays** is computed by a new pure `PlayerSlotPolicy` from the two player entries (Automatic, a picked controller, keys/mouse for Player 1, Disabled for Player 2), the attached devices, and two logs the service keeps: the order controllers connected while Casso runs and the order they first gave real input while active (R16). A slot filled by Automatic counts as playing only after input; one controller playing drives PDL0/PDL1/PB0-PB2 as a single controller always has; a leaving player's slot is held so the other keeps only what they had (R17).
- **What a slot drives** follows its target, and the target follows the holder's active profile unless set on the Controllers page; buttons follow the target as the hardware wires them (R18). (Superseded later on 2026-09-27 by R26: each player has a mode, Joystick or Paddle, and the two modes alone set the targets; see below.) `PlayerTargetRules` holds both as pure lookups. The mixer is unchanged; the service composes the one `Controller` contribution differently ([contracts/game-port-mixer.md](contracts/game-port-mixer.md)).
- **Idle controllers are watched** for their first input: XInput every 100 ms by packet number, DirectInput by its existing events, only while a player on Automatic is waiting and Casso is active (R23, [contracts/controller-backend.md](contracts/controller-backend.md)).
- **Persistence moves to global** `controllers.players` and `controllers.lastHolders`, with a one-time adoption of the launched machine's per-machine keys (R19, [contracts/prefs-schema.md](contracts/prefs-schema.md)).
- **The picker** gets a Player 1 row and a Player 2 row with submenus and a profile section at each foot; the Profiles submenu, the Multiplayer row and "N controllers" go (R22). Its label gains " +1" and a middle ellipsis from a new `DxuiElide::Middle` (R24).
- **The Controllers page** shows Player 2's row behind a Multiplayer checkbox that slides the rows below it. (Superseded later on 2026-09-27 by R26: both rows are always shown and the checkbox and its slide are gone.)
- **Notices stack** in a new Dxui control, `DxuiNoticeStack`, with a shared `DxuiSlide` for the menus' open duration and easing; the shell keeps the thread hand-off and the anchor (R20, [contracts/notice-stack.md](contracts/notice-stack.md)). Assignment notices show only when a slot's holder differs from the saved last holder (R21).
### 2026-09-27, later: a Joystick or Paddle mode for each player (GH #156)

Phase 19 of [tasks.md](tasks.md); research [R26](research.md#r26-each-player-has-a-mode-joystick-or-paddle-profiles-have-three-kinds-fr-008-fr-008b-fr-037-fr-038-fr-039-fr-043).

- **Each player has a mode**, `PlayerMode::Joystick` or `PlayerMode::Paddle`, on `PlayerEntry` and saved with it. `PlayerTargetRules::GetModeTarget` and `PlayerSlotPolicy::Evaluate` set each slot's internal target from the two modes (both Joystick while the Joyport is in effect); `GetLoneRoute` gives a lone player its mode's lines. The per-slot target, `GetAutomaticTarget`, `IsPaddleMapping`, `ControllerSelectionPolicy::GetTargetChoices` and `GetAxesForPlayer` are removed.
- **Profiles have three kinds**, `ProfileMode::Joystick`, `Paddle` and `Joyport`, with built-ins Default, Paddles and Joyport, and a chosen profile per controller per kind (`activeProfiles`, `paddleActiveProfiles`, `joyportActiveProfiles`). Each controller plays the profile of its player's kind and re-resolves when the kind changes.
- **The picker** offers Joystick and Paddle in each player's submenu, lists the keys only in Joystick mode and the mouse only in Paddle mode, marks a Paddle row " (paddle)", and reads "(disconnected) +1" while Player 1's slot is held.
- **The Controllers page** always shows both rows, each with an entry drop-down, a Joystick / Paddle drop-down and a note of what the player drives; its profile list is the kind of the controller in Editing.

### Ownership

- **Spec 036** owns the picker's "Joyport (Atari mode)" row, the Joyport labels on the player rows and notices, and which mode each profile belongs to. The hooks it needs (`PlayerSlotPolicy::DescribeAssignment`, the row model, the profile section's list for the mode in effect) are planned here and filled there.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145

**Primary Dependencies**: Windows SDK only: XInput 1.4 (`xinput.lib`), DirectInput 8 (`dinput8.lib`, `dxguid.lib`), HID (`hid.lib`) for serial numbers, `RegisterDeviceNotification`. Dxui for the Controllers page, and 032's `DxuiCommand`, `DxuiPopupMenuItem` and `DxuiToolbar` for the menu and toolbar.

**Storage**: `GlobalUserPrefs` JSON (new `controllers` section) and the per-machine `$cassoUiPrefs` block ([contracts/prefs-schema.md](contracts/prefs-schema.md)). From 2026-09-27 the players' entries and last holders are global too (`controllers.players`, `controllers.lastHolders`); the per-machine `controller` and `multiplayer` keys are read once for adoption and no longer written (R19)

**Testing**: Microsoft C++ Unit Test Framework; new `UnitTest/ControllerTests/` with `FakeControllerBackend` and `RecordingGamePortSink`; existing `InMemoryFileSystem` for prefs. 2026-09-27 logic is driven the same way, with the service's injected clock (`ControllerInputService::SetClock`) and Dxui's pass-in time (`Tick (nowMs)`) standing in for real time, and the animation flag passed in rather than read from `DxuiSystemSettings` inside the tested code, since that singleton reads the real system and the CI runner reports animations off

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only; x64 Debug and Release are the test bar)

**Project Type**: Desktop application (emulator)

**Performance Goals**: DirectInput devices read on their own change events; XInput polled at the measured packet rate (R13; measured 125 packets/s, 8 ms); a change reaches the game port within one displayed frame (SC-002); no sink writes while input is unchanged; no measurable cost with no controller selected (SC-007: with nothing connected the thread waits with no timeout; controllers are found by HID arrival notifications, never by polling empty slots). 2026-09-27: while a player on Automatic waits for a controller and Casso is active, attached controllers that hold no slot are watched for first input at 100 ms (XInput) or on their events (DirectInput), still with no measurable cost (SC-007, R23); a notice slide or expiry keeps frames coming only while it runs

**Constraints**: no redistributables; no real device access in unit tests; no undocumented API on a required path (`XInputGetCapabilitiesEx` is optional with fallback, R6); input applies only while Casso is active, and Xbox-class controllers always use XInput (FR-033)

**Scale/Scope**: up to 4 XInput slots plus any number of DirectInput devices; up to four analog axes, each with at most one owner, so several controllers drive the port at once (FR-034 to FR-038); exactly two player slots (FR-037); any number of stacked notices (FR-044)

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

### Constitution check, 2026-09-27 work

| Principle | Status | How |
|---|---|---|
| I. Code Quality | Pass | `PlayerSlotPolicy`, `PlayerTargetRules`, `DxuiNoticeStack` and `DxuiSlide` are small classes with static helpers; the adoption is one function per source key. EHM on the prefs readers, which report rather than drop a bad value. |
| II. Testing Discipline, Test Isolation | Pass | Every ordering, holding, routing and target rule is a pure function over data, driven by `FakeControllerBackend` and the service's injected clock. The notice stack and slide take the time and the animation flag as arguments, so no test reads the clock or `DxuiSystemSettings`, which reads the real system (and reports animations off on the CI runner). Each new test group carries a mutation check (tasks). |
| III. UX Consistency | Pass | Picker labels in sentence case; notices go through one stack for every caller; the slides use the menus' open duration and easing and follow the system's animation setting. No CLI change. |
| IV. Performance | Pass | The idle watch reads an XInput pad at 10 Hz only while a player on Automatic waits and Casso is active, and an unchanged packet is not decoded (R23); DirectInput adds no reads beyond its own events. The stack requests frames only while a slide runs or until the next expiry. |
| V. Simplicity | Pass with note | Two new core classes and two new Dxui types, each with one purpose. The two logs in the service are the least state that answers both SC-013 and SC-014 (R16). The held state is added only because FR-040 and FR-042 would otherwise disagree (R17). |
| VI. Thin Executable, Testable Core | Pass | Player logic in `CassoEmuCore/Controllers/`, the stack, slide and middle elision in `Dxui/`, which `UnitTest` links. `Casso.exe` unchanged. The shell keeps only the thread hand-off, the anchor and the redraw request for notices (R20). |
| Dependencies | Pass | None added. |

**Post-design re-check (2026-09-27)**: Pass. The mixer interface is unchanged, the backend interface is unchanged, the new Dxui control and the prefs keys each have a contract with its test obligations, and nothing new needs the exe.

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
│   ├── notice-stack.md          # 2026-09-27
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

Added by the 2026-09-27 work (research R16-R25):

```text
CassoEmuCore/
├── Controllers/
│   ├── PlayerSlotPolicy.h/.cpp        # new: entries + devices + logs -> two slots, states, notices
│   ├── PlayerTargetRules.h/.cpp       # new: target from the players' modes (was: from profile), button routes by target
│   ├── ControllerInputService.h/.cpp  # logs, idle watch, slot-based merge; multiplayer mode removed
│   ├── ControllerSelectionPolicy.h/.cpp # automatic selection and replacement removed; Normalize kept (target choices removed later)
│   ├── ControllerProfileStore.h/.cpp  # three profile kinds, active profile per kind
│   └── InputModeRules.h/.cpp          # player row model, mode choices, " +1" label; character cut removed
├── Config/
│   ├── GlobalUserPrefs.h/.cpp         # controllers.players, controllers.lastHolders
│   └── MachineInputPrefs.h/.cpp       # read-only adoption helpers
├── Shell/
│   ├── EmulatorShell.h                # DxuiNoticeStack replaces DxuiTimedInfoBanner
│   ├── EmulatorShellPrefs.cpp         # adoption, global persistence
│   ├── EmulatorShellPresent.cpp       # ShowNotice/SyncNotice over the stack
│   └── Window/EmulatorWindowInput.cpp # player picks, profile sections, Machine menu toggles as Player 1
└── Ui/
    ├── Chrome/EmulatorCommands.h/.cpp # player rows with submenus and profile sections
    └── Settings/
        ├── ControllersPageState.h/.cpp# player entries and modes (Multiplayer checkbox and user-set targets removed)
        └── ControllersPage.h/.cpp     # both player rows, mode drop-downs, notes (checkbox and slide removed)

Dxui/
├── Core/
│   ├── DxuiSlide.h/.cpp               # new: menu-open duration and ease-out, flag passed in
│   ├── DxuiTextElide.h/.cpp           # + DxuiElide::Middle with a kept suffix
│   └── DxuiCommand.h                  # + optional label fit (max width, elide, kept suffix)
└── Widgets/
    ├── DxuiNoticeStack.h/.cpp         # new
    └── DxuiToolbar.cpp                # measure and paint a fitted label

UnitTest/
├── ControllerTests/
│   ├── PlayerSlotPolicyTests.cpp      # new
│   ├── PlayerTargetRulesTests.cpp     # new
│   └── (existing service, rows, page, rules tests extended)
├── Dxui/
│   ├── DxuiNoticeStackTests.cpp       # new
│   ├── DxuiSlideTests.cpp             # new
│   ├── DxuiTextElideTests.cpp         # + middle
│   └── DxuiToolbarTests.cpp           # + fitted label
└── UiTests/
    ├── GlobalUserPrefsTests.cpp       # + players, lastHolders
    └── MachineInputPrefsTests.cpp     # + adoption
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
| 8 | **US6 profiles**: named profiles, Paddles template, active profile per controller unit (per machine until Phase 11), Profiles submenu on the paddle-source picker | 7 | US6, FR-026-030, SC-010 |
| 9 | **US7 two players**: per-machine axis budget, per-axis ownership and displacement, multi-controller assignment and its persistence, assignment UI on the Controllers page, //c reduced to two axes | 7, 8 | US7, FR-034-038, SC-011, SC-012 |
| 10 | **Polish**: measurements, CHANGELOG, README, gates | 9 | SC-002, SC-005, SC-007 |
| 11 | **Active profile per controller**: global per unit, Profiles submenu (built after 1.26.1) | 8 | FR-028, FR-029 |
| 12 | **Notice stack** (2026-09-27): `DxuiSlide`, `DxuiNoticeStack`, shell on the stack | none | FR-044 (stacking), FR-013 |
| 13 | **Player slots**: `PlayerTargetRules`, `PlayerSlotPolicy`, service logs, one-playing rule, held slots, buttons by target, target from profile; removal of the multiplayer mode and of automatic selection turning off the keys | 11 | US7, FR-009, FR-010, FR-032, FR-038-040, FR-042, FR-043, SC-013, SC-014 |
| 14 | **Idle watch** | 13 | FR-042, SC-005, SC-007 |
| 15 | **Global persistence and adoption; changed-holder notice** | 12, 13 | FR-011, FR-037, FR-044, SC-006 |
| 16 | **Picker**: player rows, submenus, profile sections, " +1" label with middle ellipsis; Machine menu toggles as Player 1 | 13, 15 | FR-008, FR-008b, FR-028, FR-041 |
| 17 | **Controllers page**: player entries, Multiplayer checkbox and slide, user-set targets; 034 checks of the built Joyport profile work | 12 (slide), 15 | FR-019, FR-024, FR-026, FR-029, FR-037 |
| 18 | **Validation and gates** | 12-17 | quickstart 16-28, SC-005, SC-007 |
| 19 | **A mode for each player** (2026-09-27, later): Paddle profile kind, player modes and routing, picker mode choices, Controllers page rows; replaces the user-set target and the Multiplayer checkbox of phases 13 and 17 | 13-17 | FR-008, FR-008b, FR-037, FR-039, FR-043, quickstart 29-34 |

## Complexity Tracking

No constitution violations.
