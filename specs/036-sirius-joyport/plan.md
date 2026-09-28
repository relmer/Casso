# Implementation Plan: Sirius Joyport Emulation

**Branch**: `036-sirius-joyport` | **Date**: 2026-09-24, updated 2026-09-27 (GH #156) | **Spec**: [spec.md](spec.md)

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

### 2026-09-27 update (GH #156)

The 2026-09-24 design above shipped in 1.28.0. GH #156 changed where the
Joyport is turned on, how profiles relate to it and which controller drives
which jack. The paragraphs above stay as the record of what was built; this
update plans the difference.

- **A global Apple / Atari setting** replaces the per-machine adapter
  (R14). It keeps the `"none"` / `"siriusJoyport"` tokens, moves to
  `GlobalUserPrefs`, and adopts the launched machine's saved value once. The
  //c reads it as off without changing it, through one predicate every
  consumer calls (R16).
- **Where it is turned on** (R15, R19, R23): the picker row, relabeled
  **Joyport (Atari mode)**, and a Joyport section on the Controllers page
  drawn as the unit's own vertical switch, using a new `OnDirection` option on
  `DxuiToggle`. Both apply at once. The Machine tab's Game port group and the
  sheet's OK-applied path for it are removed.
- **Profiles belong to a mode** (R17): a `mode` field on each profile, fixed
  at creation and saved as `"profileMode"`; lists show only the mode's
  profiles, built-in first; creation offers the same starting points in both
  modes; reset restores the mode's built-in. This reverses the part of the
  already-built `3f0c4620` and `c00c2c7a` that let any profile be picked in
  either mode. (Superseded 2026-09-27: profiles have three kinds, Joystick,
  Paddle and Joyport, each with its own built-in, starting points and chosen
  profile per controller; see R25.)
- **Player modes with the Joyport** (2026-09-27, R26): each player has a
  Joystick or Paddle mode (spec 034). While the Joyport is in effect both
  players play as Atari sticks with their Joyport-kind choice, and the mode
  controls in the picker and on the Controllers page are disabled and show
  Joystick.
- **Controllers page labels** (2026-09-27, R19, R23): "Apple (rear)" above the
  switch, "Atari (front)" below it, "Joyport" to its left; the heading above
  the switch lights reads "Atari joystick: left jack", ": right jack" or
  ": both jacks". The Multiplayer checkbox is gone from the page.
- **The Joyport profile's DirectInput second stick** (R18): Z/Rz, else Rx/Ry,
  on a DirectInput gamepad only, using `ControllerFormFactor`.
- **Jacks from spec 034's players** (R21): one player drives both jacks; two
  split left and right; a leaver's jack is held; Same as left keeps Player 1
  on both. **Labels** (R22) come from one helper that spec 034's picker and
  notices call.
- **Never analog and Atari at once** (R20): the mechanism already exists; the
  picker now leaves mouse-as-paddle out while the Joyport is on. Since
  2026-09-27 the mouse is offered only in Paddle mode, so the mouse and the
  Joyport are never on together; the keys stay offered in either mode.
- **Deferred**: the Controller Select switch (FR-016) is not decided by the
  owner and is not planned (R24). Casso keeps it at Center.

(Superseded 2026-09-28: the global Apple / Atari setting, the switch on the
Controllers page, the relabeled picker row, `JoyportSetting`, `JoyportLabels`
and the Joyport labels on the players all go; see the update below. The
Controller Select switch is closed as not needed.)

### 2026-09-28 update: a Joyport mode for each player (GH #156)

The owner replaced the global Apple / Atari switch with per-player Joyport
modes (spec Clarifications 2026-09-28; research R27). Spec 034's plan carries
the picker, service and mixer side of the same change.

- **Modes**: `PlayerMode` gains `JoyportLeft`, `JoyportRight` and
  `SameAsPlayer1`, in the order `Joystick`, `JoyportLeft`, `JoyportRight`,
  `Paddle`, `SameAsPlayer1`, saved as `"joystick"`, `"joyportLeft"`,
  `"joyportRight"`, `"paddle"` and `"sameAsPlayer1"`. A Player 2 with no saved
  mode is Same as Player 1.
- **`PlayerModeRules`** (new, pure, `CassoEmuCore/Controllers/`) replaces
  `JoyportSetting` and `JoyportLabels`. It resolves Same as Player 1; reports
  whether the Joyport is on (any player's resolved mode is a jack, on a machine
  that has a Joyport); which jack a mode drives; whether a mode is taken by the
  other player; whether a player's buttons are cut; whether the paddle inputs
  are connected; whether the keys and the mouse are offered; the mode list with
  checked and enabled flags; the mode labels; the assignment notice text; and
  the one-time migration from the old `gamePortAdapter` value.
- **Jacks**: `JoyportJackRules` assigns each jack from the players' resolved
  modes and their states (`Driving`, `Held`, `Idle`); `isPlayer2Disabled` is
  gone. A lone Joyport player drives both jacks while the other jack is free.
- **Service and shell**: `ControllerInputService::SetJoyportAvailable` takes
  whether the machine has a Joyport, and the service derives whether it is on
  from the entries. After every entry change the shell attaches the machine's
  Joyport and connects or disconnects its paddle inputs from
  `PlayerModeRules::ArePaddlesConnected` (`SiriusJoyport::SetPaddlesConnected`),
  through `EmulatorShell::ApplyJoyportToMachine` via `SyncJoyport` on the UI
  thread; `MigrateJoyportAtLaunch` runs the migration. These replace
  `ApplyGamePortAdapterToMachine` and `ResolveGamePortAdapterAtLaunch`. Each
  controller's profile kind
  follows its own player's resolved mode.
- **Buttons cut**: while either player is on a jack, a player on Joystick or
  Paddle drives its paddle inputs as a lone player would and no button line.
- **Removed**: `EmulatorShell::SetGamePortAdapter` / `GetGamePortAdapter`,
  `IDM_GAMEPORT_ADAPTER_JOYPORT`, the picker's Joyport row, the Controllers
  page's switch section, `JoyportSetting`, `JoyportLabels` and their tests.
  `DxuiToggle` keeps its vertical orientation and label-visibility API as
  library code.
- **Controllers page**: each player's row gets the mode list, with a taken jack
  disabled through new per-item enabled flags on `DxuiComboBox`; a player whose
  buttons are cut has its button binding rows disabled and a warning notice
  under its row.
- **FR-016** is closed as not needed; Phase 15's tasks are marked dropped.

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

**Constraints**: clean-room (the [owner's manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf) only; GSSquared's source is not
read); the //c's `$C058`-`$C05F` IOU behavior and the //e's DHIRES unchanged;
detached behavior identical to today (FR-013, SC-006)

**Scale/Scope**: two jacks of five switches; four machine models; three
annunciators

**2026-09-27 additions**:

- **Dependencies**: spec 034's player slots, Automatic, player submenus and
  notice stack (its FR-008, FR-040 to FR-044), being planned at the same time.
  Only the hookups wait on it (R21, R22); the rules and their tests do not.
  Dxui's `DxuiToggle` gains an orientation option.
- **Storage**: global `gamePortAdapter` in `UserPrefs.json`; per profile
  `"profileMode"` in the `controllers` section. The per-machine key is read
  once for adoption and no longer written.
- **Testing**: as above, plus `GlobalUserPrefsTests`,
  `ControllerProfileStoreTests`, `ControllersPageStateTests`, a new
  `JoyportJackRulesTests`, `JoyportLabelsTests`, `JoyportSettingTests` and
  `DxuiToggleTests`. The scenario suite runs at the gate because the Joyport
  is guest-visible.
- **Unknowns**: none left for planning. FR-016 is an owner decision, not a
  planning unknown, and is deferred (R24). Spec items found underspecified
  are listed under Open Items below; none blocks the plan.

**2026-09-28 changes**:

- **Storage**: the players' modes in the global `controllers.players`
  (spec 034 contracts/prefs-schema.md). The global `gamePortAdapter` is read
  once by the migration and then written as `"none"` as its marker; the
  per-machine key is read only when the global one was never set.
- **Testing**: a new `PlayerModeRulesTests`; `JoyportJackRulesTests`,
  `ControllerInputServiceTests`, `PaddleSourceRowsTests`,
  `ControllersPageStateTests`, `ControllersPageLayoutTests`,
  `SiriusJoyportTests` and `DxuiComboBoxTests` extended.
  `JoyportSettingTests` and `JoyportLabelsTests` are deleted with their
  classes.

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

### 2026-09-27 check

| Principle | Status | How |
|---|---|---|
| I. Code Quality | Pass | New logic is three small static-only classes (`JoyportSetting`, `JoyportJackRules`, `JoyportLabels`) and a field on `ControllerProfile`. The prefs read and write keep EHM; the rules cannot fail. |
| II. Testing Discipline, Test Isolation | Pass | Every new rule is a pure function with a table-driven test: adoption and `IsInEffect`, jack assignment, labels, profile mode on load, create and reset, the second-stick choice, toggle geometry. Prefs through `InMemoryFileSystem`; controllers through `FakeControllerBackend`. The rework of the two built commits rewrites `ChosenProfile_IsRememberedForEachMode` to the new rule rather than deleting it. |
| III. UX Consistency | Pass | Sentence-case labels ("Joyport (Atari mode)", "Atari mode", "Same as left"; since 2026-09-27 "Apple (rear)", "Atari (front)", "Atari joystick: left jack", and "same as left" in lower case after a colon). One setting, two places that show and change it, as the mouse toggle does. No CLI change. |
| IV. Performance | Pass | No new thread or timer. The jack assignment runs on the existing controller tick. |
| V. Simplicity | Pass | The setting reuses the adapter enum and tokens. Removing the Machine tab path deletes more Settings plumbing than the Controllers page section adds. The Controller Select switch is not built (YAGNI until decided). |
| VI. Thin Executable, Testable Core | Pass | All code in `CassoEmuCore` and `Dxui`; nothing in `Casso.exe`. The two shell call sites for adoption stay one-line forwarders to `JoyportSetting`. |
| Dependencies | Pass | None added. |

**Post-design re-check (2026-09-27)**: Pass. Two items are outside planning
and are not gate failures: FR-016 carries an open clarification marker on
purpose (R24), and the hookups of R21 and R22 wait on spec 034's player work.

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
│   ├── GamePortInputMixer.h/.cpp           # jacks by axis owner
│   └── InputModeRules.h/.cpp               # fire keys without Alt while attached
├── Config/
│   ├── MachineInputPrefs.h/.cpp            # read and build the gamePortAdapter entry
│   └── UserConfigStore.cpp                 # gamePortAdapter default
├── Shell/
│   ├── MachineHost.h/.cpp                  # own the Joyport; OnMachineReset after the CPU
│   ├── MachineBuilder.cpp                  # build and wire it
│   ├── MachineGamePortSink.h/.cpp          # WriteJacks (the Joyport comes from MachineHost::GetJoyport)
│   ├── MachineManager.cpp                  # adopt the pref on machine switch
│   ├── EmulatorShell.h, EmulatorShellPrefs.cpp          # Set/Get/ApplyLive/Adopt/Persist GamePortAdapter, cold boot
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
├── ControllerTests/                        # extend: evaluator, service, mixer, sink, picker rows, input-mode rules, page state
└── UiTests/                                # extend: HardwarePage, SettingsPanelState, command routing, MachineInputPrefs
```

**Structure Decision**: the device model sits beside the other Apple II common
devices, the switch logic extends the Controllers classes spec 034 created, and
the UI changes stay in the files that already own each surface. Nothing new
goes in an executable.

#### 2026-09-27 additions

```text
Dxui/Widgets/
└── DxuiToggle.h/.cpp                       # OnDirection (Right, Up, Down); ComputeTrackAndThumb

CassoEmuCore/
├── Config/
│   ├── GlobalUserPrefs.h/.cpp              # + gamePortAdapter (global token)
│   └── MachineInputPrefs.h/.cpp            # ReadGamePortAdapter kept for adoption only
├── Controllers/
│   ├── JoyportSetting.h/.cpp               # new: IsInEffect, IsMousePaddleOffered, ResolveAtLaunch
│   ├── JoyportJackRules.h/.cpp             # new: AssignJacks from the players
│   ├── JoyportLabels.h/.cpp                # new: FR-019 strings
│   ├── ControlMapping.h/.cpp               # MakeJoyport + formFactor; FindSecondStick
│   ├── ControllerProfileStore.h/.cpp       # ControllerProfile::mode, "profileMode", GetProfileNames(mode),
│   │                                       #   ProfileSource::JoyportMapping, reset to the mode's built-in
│   └── ControllerInputService.h/.cpp       # mode-checked choices; AddJoyportSwitches by AssignJacks
├── Shell/
│   ├── EmulatorShell.h, EmulatorShellPrefs.cpp          # global setting, adoption, IsJoyportInEffect
│   ├── MachineManager.cpp                  # apply the global setting on machine switch
│   ├── WindowCommandManager.cpp            # IDM_GAMEPORT_ADAPTER_* to SetGamePortAdapter
│   └── Window/EmulatorWindow.cpp, EmulatorWindowInput.cpp  # picker fns, profile sections per mode
└── Ui/
    ├── Chrome/EmulatorCommands.h/.cpp      # "Joyport (Atari mode)"; per-mode profile sections
    └── Settings/
        ├── HardwarePage.h/.cpp             # Game port group removed
        ├── SettingsPanelState.h/.cpp       # gamePortAdapter plumbing removed
        ├── SettingsApplyAdapter.h/.cpp     # ApplyGamePortAdapter removed
        ├── SettingsSheet.cpp               # observation removed; SetJoyportFns wiring
        ├── ControllersPage.h/.cpp          # Joyport section with the vertical toggle
        └── ControllersPageState.h/.cpp     # SetProfileMode reloads; lists per mode; create/reset by mode

(2026-09-27) ControllerProfileStore gains ProfileMode::Paddle, ProfileSource::PaddleMapping,
the Paddles built-in (ControllerProfileKind::Paddles, DefaultMapping::MakePaddles) and
paddleActiveProfiles; PlayerSlotPolicy::GetEffectiveMode reports Joystick for both
players while the Joyport is in effect; ControllersPageState::SetJoyportInEffect
replaces SetProfileMode, and the page drops the Multiplayer checkbox.

(2026-09-28) PlayerModeRules.h/.cpp is new in Controllers/ and replaces
JoyportSetting.h/.cpp and JoyportLabels.h/.cpp, which are deleted with their
tests; JoyportJackRules takes resolved modes (ReducePlayers); SiriusJoyport's
paddle inputs are connected or disconnected through SetPaddlesConnected; DxuiComboBox gains per-item enabled flags; the Controllers
page's switch section, the picker's Joyport row and IDM_GAMEPORT_ADAPTER_JOYPORT
are removed.

UnitTest/
├── Dxui/DxuiToggleTests.cpp                # new
├── ControllerTests/
│   ├── JoyportSettingTests.cpp             # new
│   ├── JoyportJackRulesTests.cpp           # new
│   ├── JoyportLabelsTests.cpp              # new
│   └── (extend) ControllerProfileStoreTests, ControllerInputServiceTests, ControllersPageStateTests, PaddleSourceRowsTests
└── UiTests/                                # (extend) GlobalUserPrefsTests, HardwarePageTests, SettingsPanelStateTests, MachineInputPrefsTests
```

## Delivery Slices

Each slice leaves the build green and is committed on its own (constitution:
commit per phase).

| Phase | Slice | Depends on | Covers |
|---|---|---|---|
| 1 | **Setup**: baseline build and suite | none | none |
| 2 | **Foundation**: annunciators; types and tokens; `SiriusJoyport` with its reset window; the two reading devices and the paddles; `MachineHost` ownership and reset ordering; the sink's jack writes. Attached by tests only | 1 | FR-003, FR-004, FR-009 (paddles), FR-010, FR-011, FR-013 |
| 3 | **US1 play (MVP)**: evaluator switches, single-source jacks, mixer, fire keys without Alt while attached, the picker row (not saved yet), the readout disk | 2 | US1, FR-005-008 (single source), FR-010 (keys), SC-001, SC-002 |
| 4 | **US2 resets**: reset tests on the //e, entry-point check, V3 on the running app | 3 | US2, SC-003 |
| 5 | **US4 setting**: pref, IDM pair, Machine tab group, live observation while Settings is open | 3 | US4, FR-001, FR-002, FR-012, SC-007 |
| 6 | **US3 two players**: multiplayer jacks, disconnect | 3 | US3, FR-008 (multiplayer), SC-005 |
| 7 | **US5 page**: switch lights, jack caption | 5, 6 | US5, FR-015 |
| 8 | **Polish**: quickstart V6, V7, V10, CHANGELOG, README, the full gate | all | SC-004, SC-006 |

### 2026-09-27 slices

| Phase | Slice | Depends on | Covers |
|---|---|---|---|
| 9 | **Profile modes**: `ControllerProfile::mode` and its storage, lists per mode, create and reset by mode, `ProfileSource::JoyportMapping`, mode-checked choices; rework of `3f0c4620` and `c00c2c7a` | 8 | US7 sc. 2, 5; FR-018, FR-020 |
| 10 | **Joyport profile second stick**: `formFactor` through the built-in mapping path, `FindSecondStick` | 9 | US7 sc. 1, 3; FR-017 |
| 11 | **Global setting**: `GlobalUserPrefs::gamePortAdapter`, adoption, `IsInEffect`, the //c, machine switch, command routing | 8 | US4 sc. 3, 4; FR-002, SC-007 |
| 12 | **Where it is turned on**: `DxuiToggle` orientation, the Controllers page section and in-place list swap, picker row relabel, mouse-as-paddle hidden, Machine tab and sheet plumbing removed | 9, 11 | US4 sc. 1, 2; FR-001, FR-009, FR-012 |
| 13 | **Jacks and labels from spec 034's players**: `JoyportJackRules`, `JoyportLabels`, and their hookups | 11; spec 034's player work for the hookups | US3; FR-008, FR-019, SC-009 |
| 14 | **Validation and gate**: quickstart V11-V20, CHANGELOG and README for approval, the full gate | 9-13 | SC-007, SC-009, SC-010 |
| (15) | **Optional, blocked on the FR-016 decision**: the Controller Select switch (dropped 2026-09-28: FR-016 closed as not needed) | owner decision | US6, FR-016, SC-008 |

### 2026-09-28 slices

| Phase | Slice | Depends on | Covers |
|---|---|---|---|
| 18 | **A Joyport mode for each player**: `PlayerModeRules`, jacks from resolved modes, buttons cut, paddle inputs connected, per-player profile kind, migration, picker and Controllers page mode lists with taken jacks disabled, the warning notice, removal of the switch, the row, the command, `JoyportSetting` and `JoyportLabels` (built with spec 034's Phase 20) | 13, 17 | US3, US4 sc. 6-10; FR-001, FR-002, FR-008, FR-009, FR-012, FR-019-FR-022, SC-011 |

## Open Items (2026-09-27)

Found while planning; recorded, not resolved here.

- **FR-016**: the owner's decision on the Controller Select switch (R24).
  (Resolved 2026-09-28: closed as not needed.)
- **Mouse-as-paddle already chosen** when the Joyport is turned on: FR-009
  hides the entry, but the spec does not state what Player 1 plays meanwhile.
  The plan keeps today's behavior (the mixer's MousePaddle owner leaves every
  switch open) and keeps the entry saved, so turning the Joyport off restores
  it. (Resolved 2026-09-27: the mouse is offered only in Paddle mode, and
  while the Joyport is in effect both players are in Joystick mode, so the
  mouse and the Joyport are never on together.)
- **Arrow keys as Player 1 beside a controller as Player 2** (R21): whether
  spec 034's players allow that pairing decides whether the mixer must compose
  the jacks from two sources. The plan supports it; if 034 rules it out, the
  composition reduces to today's owner table. (2026-09-28: allowed; the
  mixer's keys-to-jacks fallback follows the jacks the arrow-keys contribution
  marks, `keyJacks`, so the keys drive Player 1's jack or jacks.)
- **A profile saved by this build read by an older build**: the older build
  ignores `"profileMode"` and lists a Joyport-mode profile as an ordinary one.
  Accepted; nothing is lost. Since 2026-09-27 every profile carries
  `"profileMode"` (`"joystick"`, `"paddle"` or `"joyport"`), and a profile
  saved without one is classified on read (R25).
- **Joyport profile already saved**: an existing saved Joyport profile keeps
  its mapping, so a DirectInput gamepad user gets the second stick only after
  Reset profile (R18). The spec's "keeping its mapping" rule covers only a
  user profile called Joyport; this reads it as covering the saved built-in
  too.

## Complexity Tracking

No constitution violations.
