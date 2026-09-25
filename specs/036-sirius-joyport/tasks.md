---

description: "Task list for 036 Sirius Joyport emulation"
---

# Tasks: Sirius Joyport Emulation

**Input**: Design documents from `specs/036-sirius-joyport/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: Required. Constitution Principle II requires unit tests for all production code, and FR-014 requires every Joyport rule to be testable without a controller or a Joyport. Each test task ends by confirming the test fails with the implementation stubbed or reverted (copilot-instructions, "Verify a new test fails without the fix").

**Organization**: Tasks are grouped by user story so each story can be implemented and validated on its own. US1 and US2 are both P1 and ship together (spec, User Story 2's priority note).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1-US5 from spec.md
- Paths are repository-relative. Every new `.h`/`.cpp` is added to `CassoEmuCore/CassoEmuCore.vcxproj` or `UnitTest/UnitTest.vcxproj` in the same task that creates it.
- Existing functions are referenced by name, not line number.
- Code style: `.github/copilot-instructions.md` (EHM, column alignment, 5/3 blank lines, `////` banners in `.cpp` with new functions spliced ahead of the banner, verb-first function names, no magic numbers, American spelling, no spec or task references in comments). Run `scripts/CheckStyle.ps1 -Mode Staged` before every commit that adds files.
- There is no GitHub issue for this feature, so commit messages carry no `Refs` line.
- Clean-room: never read `repos\gssquared` source. The owner's manual is the reference (research R1).

---

## Phase 1: Setup

**Purpose**: A worktree that builds and runs the suite before anything changes.

- [ ] T001 Run `scripts/FetchRoms.ps1 -Fixtures` if the fixture ROMs are missing, then `scripts/Build.ps1` (x64 Debug) and `scripts/RunTests.ps1` to record the baseline pass count in the session (not in a file); a failing baseline stops the feature until it is understood

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Annunciators, the new types, and a `SiriusJoyport` device wired into the ][, ][+ and //e that answers button and paddle reads when attached. No user-visible change: nothing attaches it yet.

**CRITICAL**: No user story work begins until this phase is complete.

### Annunciators (contracts/joyport-device.md, research R2)

- [ ] T002 In `CassoEmuCore/Machines/Apple2/Common/AppleSoftSwitchBank.h/.cpp`, add `std::atomic<Byte> m_annunciators` (in-class `= 0`), `bool IsAnnunciatorOn (int index) const` for index 0-2, and handle `$C058`-`$C05D` in `Read` (and so in `Write`): `$C058 + 2n` clears bit n, `$C059 + 2n` sets it; add a `PowerCycle (Prng &)` override that clears `m_annunciators` and then calls the virtual `SoftReset()`. `Reset()` and `SoftReset()` do not touch the field. Correct the header comment that claims annunciator handling today
- [ ] T003 In `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.cpp`, route `$C058`-`$C05D` to `AppleSoftSwitchBank::Read` after the existing //c IOU branch (so while `m_mouse->IsIouAccessEnabled()` they still go only to `AccessIouSwitch`), and leave the `$C05E`/`$C05F` DHIRES arms as they are
- [ ] T004 [P] Create `UnitTest/EmuTests/AnnunciatorTests.cpp`: on the ][+ bank and the //e bank, each of `$C058`-`$C05D` by read and by write sets or clears the right bit and no other; `$C05E`/`$C05F` still toggle DHIRES on the //e; on a //c `TestMachine` with IOU access on, `$C058`-`$C05B` reach the mouse (`AppleMouse` DISXY/ENBXY and DISVBL/ENVBL state changes) and AN0-AN2 stay off; `PowerCycle` clears all three; `SoftReset` keeps them. Confirm the tests fail with the `$C058`-`$C05D` handling removed

### Types and tokens (data-model.md)

- [ ] T005 In `CassoEmuCore/Controllers/ControllerTypes.h`, add `enum class GamePortAdapter { None, SiriusJoyport }`, `enum class JoystickSwitch { Up, Down, Left, Right, Fire, Count }`, `struct JoystickSwitches { std::bitset<5> bits; }` with `IsClosed`/`SetClosed` and equality, and `struct JoyportJacks { std::array<JoystickSwitches, 2> jack; }` (`[0]` left jack, player 1, AN0 off; `[1]` right jack, player 2, AN0 on) with equality. Add `JoystickSwitches switches` and `std::optional<JoyportJacks> jacks` to `GamePortContribution`
- [ ] T006 In `CassoEmuCore/Controllers/ControllerTokens.h/.cpp`, add `GamePortAdapterToToken` and `GamePortAdapterFromToken` with tokens `"none"` and `"siriusJoyport"`; an unknown or empty token reads as `None`
- [ ] T007 [P] In `UnitTest/ControllerTests/ControllerTokensTests.cpp`, sweep every `GamePortAdapter` value (loop to a `Count`-style bound or an explicit list checked against the enum) for a round trip; an unknown token and an empty token give `None`. Confirm a test fails with one token removed
- [ ] T008 In `CassoEmuCore/Machines/MachineDefinition.h`, add `bool hasAnnunciators = false` with a one-line comment; in `CassoEmuCore/Machines/MachineDefinitions.cpp` set it true for the ][, ][+, //e and enhanced //e and leave the //c false; extend `UnitTest/EmuTests/MachineDefinitionTests.cpp` with a check per model

### Device model (contracts/joyport-device.md, research R3, R4)

- [ ] T009 Create `CassoEmuCore/Machines/Apple2/Common/SiriusJoyport.h/.cpp` per the contract: `SetAnnunciatorSource`, `SetCycleSource`, `SetAttached`/`IsAttached` (atomic), `SetJackSwitches (size_t jack, JoystickSwitches)` storing into `std::array<std::atomic<Byte>, 2>`, `OnMachineReset()` (stamps `*m_cycleSource`, sets `m_isInResetWindow`), `TryReadButton (int index, Byte & value)` and `IsDrivingPaddles()`. `kReleaseCycles = 500'000` as a public `static constexpr`. The switch selection is the contract's table (index 0 Fire; index 1 Left or Up; index 2 Right or Down by AN1; jack by AN0); closed reads `0x00`, open `0x80`, as named constants. `TryReadButton` returns false when detached or in the window, clearing the window flag on the first read at or past `kReleaseCycles`. `IsDrivingPaddles` is `IsAttached()`
- [ ] T010 [P] Create `UnitTest/EmuTests/SiriusJoyportTests.cpp` using a real `AppleSoftSwitchBank` for the annunciators and a local `uint64_t` for the cycle counter: all 12 (index, AN0, AN1) combinations against a jack pattern where each switch of each jack is distinguishable; detached returns false; `OnMachineReset` opens the window, which still declines at `kReleaseCycles - 1` and answers at `kReleaseCycles`; `SetAttached (true)` while running answers at once; `OnMachineReset` while detached, then attach, still declines until the window ends; `IsDrivingPaddles` true through the window. Confirm tests fail with AN0 and AN1 swapped in the table
- [ ] T011 Add `SiriusJoyport * m_joyport = nullptr` plus `SetJoyport (SiriusJoyport *)` to `CassoEmuCore/Machines/Apple2/Common/AppleGamePort.h/.cpp`: `ReadButton` asks `m_joyport->TryReadButton` first and uses its value when it returns true (the button-read event carries the value returned); `ReadPaddle` returns the "timer running" value (bit 7 set) while `m_joyport->IsDrivingPaddles()`. A null pointer is today's behavior
- [ ] T012 Same in `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eKeyboard.h/.cpp` for `$C061`-`$C063` (on false, the existing Open Apple or hold, Closed Apple or hold, and Shift or //c mouse logic), and in `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h/.cpp` for `ReadPaddle`
- [ ] T013 In `CassoEmuCore/Shell/MachineHost.h/.cpp`, own the Joyport (`SetJoyport (std::unique_ptr<SiriusJoyport>)`, `GetJoyport()` const and non-const, beside the mouse) and call `m_joyport->OnMachineReset()` as the last step of `SoftReset` and of `PowerCycle`, after the CPU's reset, when a Joyport exists. Add `SiriusJoyport * joyport` to `CassoEmuCore/Shell/MachineRefs.h`
- [ ] T014 In `CassoEmuCore/Shell/MachineBuilder.cpp`, when the machine definition has `hasAnnunciators`, create the Joyport, wire `SetAnnunciatorSource` (the `softSwitches` ref on the ][/][+, `iieSoftSwitches` on the //e), `SetCycleSource (cpu->GetCycleCounterPtr())` in `CreateCpu` beside the paddle-timer wiring, `gamePort->SetJoyport` or `iieKeyboard->SetJoyport` plus `iieSoftSwitches->SetJoyport`, hand it to the host, and fill `MachineRefs::joyport`. The //c gets none
- [ ] T015 [P] Extend `UnitTest/EmuTests/MachineBuildTests.cpp`: the ][+, //e and enhanced //e build a Joyport wired to their button device; the //c builds none and `MachineRefs::joyport` is null
- [ ] T016 Create `UnitTest/EmuTests/JoyportMachineTests.cpp` on `TestMachine` (][+ and //e): with a test-attached Joyport past its window and a jack pattern set through `SetJackSwitches`, a guest program `STA $C059 / STA $C05B / LDA $C062` (right jack, up) and the other combinations read the pattern; `$C064`-`$C067` read bit 7 set; detached, the existing expectations of `KeyboardTests` (`OpenAppleReadable_C061` etc.) and `GamePortTests` hold on the same machine. Confirm a test fails with the reading devices' Joyport call removed

### Sink path (contracts/switch-evaluation.md, research R7)

- [ ] T017 Add `JoyportJacks jacks` to `GamePortState` in `CassoEmuCore/Controllers/GamePortInputMixer.h` (equality and change detection include it), and `SiriusJoyport * joyport` to `GamePortTargets` in `CassoEmuCore/Shell/MachineGamePortSink.h`; add `WriteJacks` to `MachineGamePortSink.cpp`, called from `TryApply`, writing each jack whose switches differ from `lastApplied` (all when `lastApplied` is null) and skipping a null `joyport`; fill `joyport` in the targets lambda in `CassoEmuCore/Shell/EmulatorShell.cpp`
- [ ] T018 [P] Extend `UnitTest/ControllerTests/MachineGamePortSinkTests.cpp`: jacks reach a real `SiriusJoyport`, only the changed jack is written, a null Joyport (the //c) is skipped without failing the write. Confirm a test fails with `WriteJacks` stubbed
- [ ] T019 Build x64 Debug, run `scripts/RunTests.ps1`, then commit: `feat(joyport): Annunciators and the Joyport device model`

**Checkpoint**: the machines record AN0-AN2, and a test-attached Joyport answers button and paddle reads on the ][+ and //e. Nothing in the UI attaches it.

---

## Phase 3: User Story 1 - Play a Joyport game with one controller (Priority: P1) MVP

**Goal**: a single controller, or the arrow keys, drives the Joyport's switches on both jacks, and the picker attaches it (not yet saved).

**Independent Test**: quickstart V1 and V2 on the readout disk; SC-001 for the left jack in single-source mode.

### Tests for User Story 1

- [ ] T020 [P] [US1] Extend `UnitTest/ControllerTests/MappingEvaluatorTests.cpp`: an Absolute stick at 0.49 and 0.51 of the shaped deflection on each of the four directions (open, then closed); a diagonal closes one horizontal and one vertical; a digital pair closes at once and both held closes neither; a Rate binding closes while deflected and opens when released although its paddle byte stays put; an inverted axis swaps the switches; PB0 bindings drive Fire and PB1/PB2 bindings do not. Confirm tests fail with switches computed from the paddle byte
- [ ] T021 [P] [US1] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: in single-source mode the merged contribution's `jacks` carry the selection's switches on both jacks; with no driver they are all open. Confirm a test fails with the right jack left open
- [ ] T022 [P] [US1] Extend `UnitTest/ControllerTests/GamePortInputMixerTests.cpp` per the contract's owner table: Controller owner uses the Controller source's jacks; ArrowKeys owner derives Left/Up from 0 and Right/Down from 255, Fire from FireKeys PB0, on both jacks; MousePaddle owner leaves them open; None owner carries only FireKeys fire; AppleModifierKeys never closes a switch. Confirm a test fails with AppleModifierKeys included
- [ ] T023 [P] [US1] Extend `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp`: with `SetJoyportFns` offered, a checkable **Sirius Joyport** row sits after the source rows and before the separator above Multiplayer, checked exactly when `isOn` returns true, and its dispatch calls the toggle once; not offered, no row

### Implementation for User Story 1

- [ ] T024 [US1] In `CassoEmuCore/Controllers/MappingEvaluator.h/.cpp`, add `static constexpr float kSwitchThreshold = 0.5f` and fill `GamePortContribution::switches` in `Evaluate` from the shaped PDL0 and PDL1 values inside `EvaluatePair` (after deadzone, calibration and inversion, before `ToAxisPaddle`, for Rate bindings too), using the winning binding per axis: `<= -kSwitchThreshold` closes Left/Up, `>= +kSwitchThreshold` closes Right/Down; Fire from `IsButtonListHeld` over the PB0 bindings
- [ ] T025 [US1] In `CassoEmuCore/Controllers/ControllerInputService.cpp`, after `BuildMergedLocked`, set `merged.jacks` in single-source mode to the selection driver's `logical->switches` on both jacks, all open with no driver (a helper `BuildJacksLocked`, declared in the `.h`)
- [ ] T026 [US1] In `CassoEmuCore/Controllers/GamePortInputMixer.cpp`, fill `GamePortState::jacks` in `ComputeTargetLocked` per the owner table in `contracts/switch-evaluation.md` (a static helper per source kind keeps the function short)
- [ ] T027 [US1] In `CassoEmuCore/Shell/EmulatorShell.h` and a shell `.cpp` beside the mouse toggle, add `SetGamePortAdapter (GamePortAdapter)` and `GetGamePortAdapter()`: store the value, call `SetAttached` on the live machine's Joyport under the shared lifetime lock, and call `SyncSelectorState`. Not persisted yet (US4)
- [ ] T028 [US1] In `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp`, add `SetJoyportFns (isOn, isOffered, toggle)` and the checkable **Sirius Joyport** `DxuiCommand` in `GetPaddlePickerItems` at the position T023 tests; wire it in `CassoEmuCore/Shell/Window/EmulatorWindow.cpp` beside `SetMouseModeFns`, with `isOffered` from the running machine's `hasAnnunciators` and the toggle flipping `SetGamePortAdapter`
- [ ] T029 [US1] Create `Disks/Casso/JoyportTest.bas` in the style of `Disks/Casso/JoystickTest.bas` (uppercase only, for the ][+): for each jack (AN0 off, on) and each AN1 state, `POKE` the annunciators and `PEEK(49249)`-`PEEK(49251)`, and print a two-column table of UP, DOWN, LEFT, RIGHT and FIRE reading CLOSED or OPEN, redrawn in a loop with a Ctrl-C note; build `Disks/Casso/JoyportTest.dsk` with the three `CassoCli disk` commands in `quickstart.md`, and boot it once on the //e to confirm it runs
- [ ] T030 [US1] Build, run `scripts/RunTests.ps1`, run quickstart V1 and V2 on the //e and the ][+ with a real controller (ask the user to hold the directions if no controller input can be scripted), record the results in a new `specs/036-sirius-joyport/validation.md`, then commit: `feat(joyport): Drive the Joyport's switches from one controller`

**Checkpoint**: a Joyport game is playable with one controller from a picker click. Do not ship without US2: on the //e a Ctrl-Reset still needs the reset window's validation.

---

## Phase 4: User Story 2 - The //e keeps working with the Joyport attached (Priority: P1)

**Goal**: resets and power-on behave normally with the Joyport attached, and Open Apple through reset still reboots.

**Independent Test**: quickstart V3; SC-003.

### Tests for User Story 2

- [ ] T031 [P] [US2] Extend `UnitTest/EmuTests/JoyportMachineTests.cpp` on a //e `TestMachine` with the Joyport attached and every switch open: 20 `SoftReset`s and 20 `PowerCycle`s each end on the normal reset path, with no self-test and no reboot (detect as `OpenAppleResetTests.cpp` does, by where the firmware ends up after its `$C061`/`$C062` reads); with `HoldAppleKeysThroughReset (true, false)`, a `SoftReset` reboots as without a Joyport; after the window, `SetOpenApple (true)` does not change `$C061`. Confirm the first test fails with `OnMachineReset` removed from `MachineHost`
- [ ] T032 [P] [US2] In the same file, a held Fire through a //e reset does not change the reset result (switches written before `SoftReset`, window still declines), and a machine switch (a fresh `TestMachine` built and power-cycled with the Joyport attached) opens the window

### Implementation for User Story 2

- [ ] T033 [US2] Confirm `MachineHost::SoftReset`/`PowerCycle` (T013) cover the Ctrl-Reset, power-cycle and machine-switch entry points in `CassoEmuCore/Shell/MachineManager.cpp`, and that `SetGamePortAdapter` on a machine switch is applied to the new Joyport before `PowerCycle` runs (read the value from the shell in the build step, not after); fix any path the tests show missing
- [ ] T034 [US2] Build, run the suite, run quickstart V3 on the //e (20 Ctrl-Resets, 20 power cycles, one Open Apple reset), record in `validation.md`, then commit: `feat(joyport): Release the Joyport's lines after every reset`

**Checkpoint**: US1 and US2 together are the shippable MVP.

---

## Phase 5: User Story 4 - Turning it on and off (Priority: P2)

**Goal**: the setting is saved per machine, shown on the Machine tab, and not offered on the //c. It comes before US3 because it finishes the picker row US1 started.

**Independent Test**: quickstart V5 and V9; SC-007.

### Tests for User Story 4

- [ ] T035 [P] [US4] Extend `UnitTest/UiTests/HardwarePageTests.cpp`: `BuildNodes` with `supportsGamePortAdapter` appends a **Game port** group with **None** and **Sirius Joyport**, exactly one checked per `adapter`; unsupported (the //c) has no group
- [ ] T036 [P] [US4] Extend `UnitTest/UiTests/SettingsPanelStateTests.cpp`: `gamePortAdapter` round-trips through `ExtractUiPrefs`/`BuildJson`, marks the state dirty, is pushed to `RecordingSink::ApplyGamePortAdapter` on `Apply` and never queues a reset (template: `MouseConnected_DefaultsOnRoundTripsNoReset`); `ObserveLiveGamePortAdapter` with a changed live value while the entry is untouched leaves the state not dirty and makes `BuildJson` write the live value; with a pending user edit, the edit survives the observation. Confirm a test fails with `ArePrefsEqual` ignoring the field
- [ ] T037 [P] [US4] Extend `UnitTest/UiTests/ChromeCommandRoutingTests.cpp` with `IDM_GAMEPORT_ADAPTER_NONE` and `IDM_GAMEPORT_ADAPTER_JOYPORT`: unique, routed to the UI thread
- [ ] T038 [P] [US4] Extend `UnitTest/UiTests/UserConfigStoreTests.cpp`: `"none"` is omitted by `SaveDelta`; `"siriusJoyport"` is kept per machine and does not leak to another machine

### Implementation for User Story 4

- [ ] T039 [US4] Add `IDM_GAMEPORT_ADAPTER_NONE` and `IDM_GAMEPORT_ADAPTER_JOYPORT` to `CassoEmuCore/resource.h`, route them in `WindowCommandManager::GetCommandRoute` to a handler in `CassoEmuCore/Shell/WindowCommandManager.cpp` that calls `EmulatorShell::SetGamePortAdapter`
- [ ] T040 [US4] Persist from `SetGamePortAdapter` through `DiskSettings::WriteSavedUiPrefs` (`gamePortAdapter` token; never written for a machine without `hasAnnunciators`); add the `"none"` default to `UserConfigStore::BuildUiPrefsDefaults` in `CassoEmuCore/Config/UserConfigStore.cpp`
- [ ] T041 [US4] Read the pref at cold boot in `EmulatorShell::ApplyPersistedChromePrefs` (`CassoEmuCore/Shell/EmulatorShellPrefs.cpp`) and on machine switch in `MachineManager::SwitchMachine` beside `mouseConnected`, applying it to the new machine's Joyport before `PowerCycle`
- [ ] T042 [US4] In `CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp`: `SettingsUiPrefs::gamePortAdapter` (default `None`) in `ExtractUiPrefs`, `BuildJson`, `ArePrefsEqual`; `SetGamePortAdapter`; `ISettingsApplySink::ApplyGamePortAdapter` called from `Apply` with no `QueueMachineReset`; `ObserveLiveGamePortAdapter (GamePortAdapter live)` returning whether a rebuild is needed, per `contracts/prefs-and-ui.md`
- [ ] T043 [US4] Implement `ApplyGamePortAdapter` in `CassoEmuCore/Ui/Settings/SettingsApplyAdapter.h/.cpp` by posting the matching IDM as `WM_COMMAND`, and add it to the test `RecordingSink`
- [ ] T044 [US4] In `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp`, add the `supportsGamePortAdapter` and `adapter` parameters to `BuildNodes`, pass them from `Rebuild`, and route the two labels in `SetOnToggle` to `SetGamePortAdapter`, ignoring a toggle that would leave neither checked
- [ ] T045 [US4] In `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`'s `OnDialogTick`, call `ObserveLiveGamePortAdapter` with the shell's live value and rebuild the Machine tab when it returns true
- [ ] T046 [US4] Build, run the suite, run quickstart V5 and V9, record in `validation.md`, then commit: `feat(joyport): Save the Joyport per machine and show it on the Machine tab`

**Checkpoint**: attach and detach from either place, saved per machine, never offered on the //c.

---

## Phase 6: User Story 3 - Two players at once (Priority: P2)

**Goal**: multiplayer slot 1 drives the left jack and slot 2 the right, whatever their paddle targets.

**Independent Test**: quickstart V4; SC-001 for both jacks, SC-005.

### Tests for User Story 3

- [ ] T047 [P] [US3] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp` using `SetUpTwoPlayers`: player 2's fire closes the right jack's Fire and not the left's (US3 scenario 1); both players moving at once each close only their own jack; slot targets swapped (slot 1 on Joystick1, slot 2 on Paddle0) leave slot 1 on the left jack; player 2 disconnected opens the right jack and leaves the left unaffected; multiplayer configured with no slot connected falls back to the single-source jacks. Confirm a test fails with the jacks taken from the merged buttons

### Implementation for User Story 3

- [ ] T048 [US3] Extend `BuildJacksLocked` in `CassoEmuCore/Controllers/ControllerInputService.cpp` for live multiplayer: slot 1's driver on the left jack and slot 2's on the right, from each driver's pre-merge `logical->switches`, all open for an absent or disconnected slot
- [ ] T049 [US3] Build, run the suite, run quickstart V4 with two controllers, record in `validation.md`, then commit: `feat(joyport): Give each multiplayer slot its own Joyport jack`

**Checkpoint**: two-player Joyport games work.

---

## Phase 7: User Story 5 - See the switches on the Controllers page (Priority: P2)

**Goal**: five live switch lights and the jack caption on the Controllers page while the Joyport is attached.

**Independent Test**: quickstart V8.

### Tests for User Story 5

- [ ] T050 [P] [US5] Extend `UnitTest/ControllerTests/ControllersPageStateTests.cpp`: `GetJoyportJack()` is `Both` in single-source mode, `Left` for player 1's controller and `Right` for player 2's in multiplayer, and follows Editing when it moves; `ComputeLiveReading` carries `switches` for the page's pending mapping. Confirm a test fails with `Both` returned always

### Implementation for User Story 5

- [ ] T051 [US5] Add `GetJoyportJack()` (an enum `JoyportJack { Left, Right, Both }` nested in `ControllersPageState`) to `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp`
- [ ] T052 [US5] In `CassoEmuCore/Ui/Settings/ControllersPage.h/.cpp`, add `SetJoyportAttachedFn`, five `ButtonLightView`s (Up, Down, Left, Right, Fire), a **Joyport** heading and a jack caption ("Left jack", "Right jack", "Both jacks"); in `Layout`, show them and hide the stick, the button lights and their headings while attached; in `Poll`, light them from `reading.switches` and `Relayout` when the attach state changes. Wire the function in `CassoEmuCore/Ui/Settings/SettingsSheet.cpp` to `EmulatorShell::GetGamePortAdapter`
- [ ] T053 [US5] Build, run the suite, run quickstart V8, capture a screenshot of the page with the Joyport attached (DPI-aware `PrintWindow` of the sheet), record in `validation.md`, then commit: `feat(joyport): Show the Joyport's switches on the Controllers page`

**Checkpoint**: all five stories done.

---

## Phase 8: Polish and Cross-Cutting Concerns

- [ ] T054 Run quickstart V6 (Wavy Navy on the ][+), V7 (Boulder Dash on the //e) and V10 (detached, `JoystickTest.dsk`), with the user supplying the game disks; restore the machine's `disk1Path` afterward; record in `validation.md`
- [ ] T055 [P] Add the CHANGELOG `[Unreleased]` entry (terse, user-visible effect only) and a README headline for the Joyport, and show both to the user for approval before any push
- [ ] T056 Run the merge gate: `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for x64 Debug and Release, `scripts/RunTests.ps1` Debug and Release (full suite, not filtered), `scripts/CheckStyle.ps1 -Mode Tree`, and `rg -n '\w \(\)'` over the new code; ARM64 build only
- [ ] T057 Reconcile `spec.md`, `plan.md` and `tasks.md` with what was built, then commit: `docs(spec): Validation results (036-sirius-joyport)`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none.
- **Foundational (Phase 2)**: after Setup; blocks every story.
- **US1 (Phase 3)**: after Foundational.
- **US2 (Phase 4)**: after US1 (its tests attach through the same device, and it validates the reset path US1 exposed). Ships with US1.
- **US4 (Phase 5)**: after US1 (extends the picker row and `SetGamePortAdapter`).
- **US3 (Phase 6)**: after US1; independent of US2 and US4.
- **US5 (Phase 7)**: after US3 (the jack caption covers multiplayer) and US4 (the page reads the setting).
- **Polish (Phase 8)**: after the stories to ship.

### Within Each Story

- Tests first; confirm they fail with the implementation stubbed.
- Pure logic in `CassoEmuCore/Controllers/` and the device before shell wiring; shell wiring before chrome.
- Commit at each phase end.

### Parallel Opportunities

- T004, T007, T010, T015, T018 in Phase 2 (separate test files).
- T020-T023 (US1 tests), T031-T032 (US2), T035-T038 (US4).
- US3 beside US2 and US4 once US1 is done.

---

## Parallel Example: User Story 1

```text
Task: "Extend MappingEvaluatorTests.cpp (T020)"
Task: "Extend ControllerInputServiceTests.cpp (T021)"
Task: "Extend GamePortInputMixerTests.cpp (T022)"
Task: "Extend PaddleSourceRowsTests.cpp (T023)"
```

## Parallel Example: User Story 4

```text
Task: "Extend HardwarePageTests.cpp (T035)"
Task: "Extend SettingsPanelStateTests.cpp (T036)"
Task: "Extend ChromeCommandRoutingTests.cpp (T037)"
Task: "Extend UserConfigStoreTests.cpp (T038)"
```

---

## Implementation Strategy

### MVP (User Stories 1 and 2)

1. Phase 2: annunciators and the device.
2. Phase 3: one controller on both jacks, attached from the picker.
3. Phase 4: the reset window.
4. Stop and validate: quickstart V1-V3.

### Incremental Delivery

1. US4: saved per machine, Machine tab.
2. US3: two players.
3. US5: the Controllers page.

Each phase ends with a commit and leaves the build and suite green.

---

## Notes

- Never run `Get-Process Casso | Stop-Process`; stop only the PID you launched.
- Every agent launch of Casso passes `--title <worktree name>` and runs minimized unless the user asked for it.
- `.specify/feature.json` stays out of commits.
- No Claude attribution in commit messages (CheckStyle CS0008).
