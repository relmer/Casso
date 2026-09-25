# Implementation Plan: Sirius Joyport Emulation

**Branch**: `036-sirius-joyport` | **Date**: 2026-09-24 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/036-sirius-joyport/spec.md`

## Summary

A new `SiriusJoyport` device model, built for every machine with annunciators
(][, ][+, //e, enhanced //e) and owned by `MachineHost` beside the mouse,
answers PB0-PB2 at read time from the current AN0 and AN1 and two jacks of
Atari switches. The two devices that answer the pushbuttons today,
`AppleGamePort` on the ][/][+ and `Apple2eKeyboard` on the //e, ask it first and
fall back to exactly today's behavior when it declines. It declines when
detached and for 500,000 cycles after every reset, which keeps the //e out of
self-test and leaves Open Apple reboots working (research R3, R4).

The annunciators are recorded for the first time: `AppleSoftSwitchBank` stores
AN0-AN2 from `$C058`-`$C05D`, and the //e bank delegates those addresses to it
after the //c's IOU branch (R2).

The switches come from the controller pipeline spec 034 built.
`MappingEvaluator` gains a five-switch result computed from the shaped axis
deflection, not the paddle byte, because Rate bindings report a held position
(R5). `ControllerInputService` takes each player's switches before the merge
collapses players onto one set of lines, and assigns jacks: single source on
both, multiplayer slot 1 left and slot 2 right (R6). The mixer and
`MachineGamePortSink` carry them to the device on the existing flush path (R7).

The per-machine adapter setting is `$cassoUiPrefs.gamePortAdapter`, changed from
a checkable picker row (live) or a Game port group on the Machine tab (on OK),
with neither ever resetting the machine (R8-R11). The Controllers page swaps its
stick and button lights for five switch lights while the Joyport is attached
(R12). A `JoyportTest` readout disk covers SC-001 (R13).

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145

**Primary Dependencies**: none new. Spec 034's controller stack
(`CassoEmuCore/Controllers/`), Dxui's `DxuiTreeView`, `DxuiCommand` and the
Controllers page's `ButtonLightView`.

**Storage**: per-machine `$cassoUiPrefs.gamePortAdapter` in `UserPrefs.json`
([contracts/prefs-and-ui.md](contracts/prefs-and-ui.md))

**Testing**: Microsoft C++ Unit Test Framework. Device tests on `TestMachine`
and the bare devices; controller tests on spec 034's `FakeControllerBackend`
and `RecordingGamePortSink`; Settings tests on `SettingsPanelState` and its
`RecordingSink`

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: Desktop application (emulator)

**Performance Goals**: a switch change reaches the machine within one frame
(SC-002), on the same path controller buttons already take. A Joyport read
costs two atomic loads and a table lookup on the CPU thread; with no Joyport the
read path adds one null-pointer test

**Constraints**: clean-room (the owner's manual only; GSSquared's source is not
read); the //c's `$C058`-`$C05F` IOU behavior and the //e's DHIRES unchanged;
detached behavior identical to today (FR-013, SC-006)

**Scale/Scope**: two jacks of five switches; four machine models; three
annunciators

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How |
|---|---|---|
| I. Code Quality | Pass | The Joyport is a small class with short methods; the switch table is data. EHM wherever something can fail (prefs I/O); the read path cannot fail. |
| II. Testing Discipline, Test Isolation | Pass | Every rule has a test that needs no controller and no real Joyport (FR-014): the device against a fake annunciator source and a plain cycle counter; guest programs on `TestMachine`; switches through `FakeControllerBackend`; prefs through `InMemoryFileSystem`. |
| III. UX Consistency | Pass | The picker row follows the mouse toggle; the Machine tab entry follows the synthetic device rows and the sheet's OK semantics; no CLI change. |
| IV. Performance | Pass | No new thread, tick or timer. The reset window is a cycle stamp checked on read. |
| V. Simplicity | Pass | One new class. The adapter is an enum with two values because the spec's entity is a choice of device, not because more are planned. |
| VI. Thin Executable, Testable Core | Pass | All code in `CassoEmuCore`; nothing in Dxui or `Casso.exe`. |
| Dependencies | Pass | None added. |

**Post-design re-check**: Pass. The one presentation decision the spec leaves
open, how a two-choice setting appears in a checkbox-only tree, is resolved
without a new widget (R10). No violations to track.

## Project Structure

### Documentation (this feature)

```text
specs/036-sirius-joyport/
├── spec.md
├── plan.md              # This file
├── research.md          # Phase 0
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── joyport-device.md
│   ├── switch-evaluation.md
│   └── prefs-and-ui.md
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks, not created here
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Machines/
│   ├── MachineDefinition.h                 # + hasAnnunciators
│   ├── MachineDefinitions.cpp              # set for ][, ][+, //e, enhanced //e
│   └── Apple2/
│       ├── Common/
│       │   ├── SiriusJoyport.h/.cpp        # new
│       │   ├── AppleSoftSwitchBank.h/.cpp  # AN0-AN2, PowerCycle
│       │   └── AppleGamePort.h/.cpp        # ask the Joyport for buttons and paddles
│       └── Apple2e/
│           ├── Apple2eSoftSwitchBank.cpp   # delegate $C058-$C05D; paddles
│           └── Apple2eKeyboard.h/.cpp      # ask the Joyport for $C061-$C063
├── Controllers/
│   ├── ControllerTypes.h                   # GamePortAdapter, JoystickSwitches, JoyportJacks
│   ├── ControllerTokens.h/.cpp             # adapter tokens
│   ├── MappingEvaluator.h/.cpp             # switches from shaped deflection
│   ├── ControllerInputService.cpp          # jacks per player
│   └── GamePortInputMixer.h/.cpp           # jacks by axis owner
├── Config/UserConfigStore.cpp              # gamePortAdapter default
├── Shell/
│   ├── MachineHost.h/.cpp                  # own the Joyport; OnMachineReset after the CPU
│   ├── MachineBuilder.cpp                  # build and wire it
│   ├── MachineRefs.h                       # + joyport
│   ├── MachineGamePortSink.h/.cpp          # WriteJacks
│   ├── MachineManager.cpp                  # adopt the pref on machine switch
│   ├── EmulatorShell.h, EmulatorShellPrefs.cpp          # Set/GetGamePortAdapter, cold boot
│   ├── Window/EmulatorWindow.cpp           # picker fns
│   └── WindowCommandManager.cpp            # IDM routes
├── resource.h                              # IDM_GAMEPORT_ADAPTER_NONE/_JOYPORT
└── Ui/
    ├── Chrome/EmulatorCommands.h/.cpp      # Sirius Joyport row
    └── Settings/
        ├── HardwarePage.h/.cpp             # Game port group
        ├── SettingsPanelState.h/.cpp       # pref, sink, ObserveLiveGamePortAdapter
        ├── SettingsApplyAdapter.h/.cpp     # ApplyGamePortAdapter
        ├── SettingsSheet.cpp               # observe live value; page wiring
        ├── ControllersPage.h/.cpp          # switch lights
        └── ControllersPageState.h/.cpp     # GetJoyportJack

Disks/Casso/
├── JoyportTest.bas                         # new
└── JoyportTest.dsk                         # new

UnitTest/
├── EmuTests/
│   ├── AnnunciatorTests.cpp                # new
│   ├── SiriusJoyportTests.cpp              # new
│   └── JoyportMachineTests.cpp             # new
├── ControllerTests/                        # extend: evaluator, service, mixer, sink, picker rows, page state
└── UiTests/                                # extend: HardwarePage, SettingsPanelState, command routing
```

**Structure Decision**: the device model sits beside the other Apple II common
devices, the switch logic extends the Controllers classes spec 034 created, and
the UI changes stay in the files that already own each surface. Nothing new
goes in an executable.

## Delivery Slices

Each slice leaves the build green and is committed on its own (constitution:
commit per phase).

| Phase | Slice | Depends on | Covers |
|---|---|---|---|
| 1 | **Setup**: baseline build and suite | none | none |
| 2 | **Foundation**: annunciators; types and tokens; `SiriusJoyport` with its reset window; the two reading devices and the paddles; `MachineHost` ownership and reset ordering; the sink's jack writes. Attached by tests only | 1 | FR-003, FR-004, FR-009 (paddles), FR-010, FR-011, FR-013 |
| 3 | **US1 play (MVP)**: evaluator switches, single-source jacks, mixer, the picker row (not saved yet), the readout disk | 2 | US1, FR-005-008 (single source), SC-001, SC-002 |
| 4 | **US2 resets**: reset tests on the //e, entry-point check, V3 on the running app | 3 | US2, SC-003 |
| 5 | **US4 setting**: pref, IDM pair, Machine tab group, live observation while Settings is open | 3 | US4, FR-001, FR-002, FR-012, SC-007 |
| 6 | **US3 two players**: multiplayer jacks, disconnect | 3 | US3, FR-008 (multiplayer), SC-005 |
| 7 | **US5 page**: switch lights, jack caption | 5, 6 | US5, FR-015 |
| 8 | **Polish**: quickstart V6, V7, V10, CHANGELOG, README, the full gate | all | SC-004, SC-006 |

## Complexity Tracking

No constitution violations.
