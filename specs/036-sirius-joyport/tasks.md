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
- **[Story]**: US1-US7 from spec.md (US6, the Controller Select switch, only in the optional Phase 15)
- Paths are repository-relative. Every new `.h`/`.cpp` is added to `CassoEmuCore/CassoEmuCore.vcxproj` or `UnitTest/UnitTest.vcxproj` in the same task that creates it.
- Existing functions are referenced by name, not line number.
- Code style: `.github/copilot-instructions.md` (EHM, column alignment, 5/3 blank lines, `////` banners in `.cpp` with new functions spliced ahead of the banner, verb-first function names, no magic numbers, American spelling, no spec or task references in comments). Run `scripts/CheckStyle.ps1 -Mode Staged` before every commit that adds files.
- There is no GitHub issue for this feature, so commit messages carry no `Refs` line. (Phases 9-15 carry `GH #156` in the commit body, as `3f0c4620` and `c00c2c7a` do.)
- Clean-room: never read `repos\gssquared` source. The [owner's manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf) is the reference (research R1).

---

## Phase 1: Setup

**Purpose**: A worktree that builds and runs the suite before anything changes.

- [X] T001 Run `scripts/FetchRoms.ps1 -Fixtures` if the fixture ROMs are missing, then `scripts/Build.ps1` (x64 Debug) and `scripts/RunTests.ps1` to record the baseline pass count in the session (not in a file); a failing baseline stops the feature until it is understood

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Annunciators, the new types, and a `SiriusJoyport` device wired into the ][, ][+ and //e that answers button and paddle reads when attached. No user-visible change: nothing attaches it yet.

**CRITICAL**: No user story work begins until this phase is complete.

### Annunciators (contracts/joyport-device.md, research R2)

- [X] T002 In `CassoEmuCore/Machines/Apple2/Common/AppleSoftSwitchBank.h/.cpp`, add `std::atomic<Byte> m_annunciators` (in-class `= 0`), `bool IsAnnunciatorOn (int index) const` for index 0-2, and handle `$C058`-`$C05D` in `Read` (and so in `Write`): `$C058 + 2n` clears bit n, `$C059 + 2n` sets it; add a `PowerCycle (Prng &)` override that clears `m_annunciators` and then calls the virtual `SoftReset()`. `Reset()` and `SoftReset()` do not touch the field. Correct the header comment that claims annunciator handling today
- [X] T003 In `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.cpp`, route `$C058`-`$C05D` to `AppleSoftSwitchBank::Read` after the existing //c IOU branch (so while `m_mouse->IsIouAccessEnabled()` they still go only to `AccessIouSwitch`), and leave the `$C05E`/`$C05F` DHIRES arms as they are
- [X] T004 [P] Create `UnitTest/EmuTests/AnnunciatorTests.cpp`: on the ][+ bank and the //e bank, each of `$C058`-`$C05D` by read and by write sets or clears the right bit and no other; `$C05E`/`$C05F` still toggle DHIRES on the //e; on a //c `TestMachine` with IOU access on, `$C058`-`$C05B` reach the mouse (`AppleMouse` DISXY/ENBXY and DISVBL/ENVBL state changes) and AN0-AN2 stay off; `PowerCycle` clears all three; `SoftReset` keeps them. Confirm the tests fail with the `$C058`-`$C05D` handling removed

### Types and tokens (data-model.md)

- [X] T005 In `CassoEmuCore/Controllers/ControllerTypes.h`, add `enum class GamePortAdapter { None, SiriusJoyport }`, `enum class JoystickSwitch { Up, Down, Left, Right, Fire, Count }`, `struct JoystickSwitches { std::bitset<5> bits; }` with `IsClosed`/`SetClosed` and equality, and `struct JoyportJacks { std::array<JoystickSwitches, 2> jack; }` (`[0]` left jack, player 1, AN0 off; `[1]` right jack, player 2, AN0 on) with equality. Add `JoystickSwitches switches` and `std::optional<JoyportJacks> jacks` to `GamePortContribution`
- [X] T006 In `CassoEmuCore/Controllers/ControllerTokens.h/.cpp`, add `GamePortAdapterToToken` and `GamePortAdapterFromToken` with tokens `"none"` and `"siriusJoyport"`; an unknown or empty token reads as `None`
- [X] T007 [P] In `UnitTest/ControllerTests/ControllerTokensTests.cpp`, sweep every `GamePortAdapter` value (loop to a `Count`-style bound or an explicit list checked against the enum) for a round trip; an unknown token and an empty token give `None`. Confirm a test fails with one token removed
- [X] T008 In `CassoEmuCore/Machines/MachineDefinition.h`, add `bool hasAnnunciators = false` with a one-line comment; in `CassoEmuCore/Machines/MachineDefinitions.cpp` set it true for the ][, ][+, //e and enhanced //e and leave the //c false; extend `UnitTest/EmuTests/MachineDefinitionTests.cpp` with a check per model

### Device model (contracts/joyport-device.md, research R3, R4)

- [X] T009 Create `CassoEmuCore/Machines/Apple2/Common/SiriusJoyport.h/.cpp` per the contract: `SetAnnunciatorSource`, `SetCycleSource`, `SetAttached`/`IsAttached` (atomic), `SetJackSwitches (size_t jack, JoystickSwitches)` storing into `std::array<std::atomic<Byte>, 2>`, `OnMachineReset()` (stamps `*m_cycleSource`, sets `m_isInResetWindow`), `TryReadButton (int index, Byte & value)` and `IsDrivingPaddles()`. `kReleaseCycles = 500'000` as a public `static constexpr`. The switch selection is the contract's table (index 0 Fire; index 1 Left or Up; index 2 Right or Down by AN1; jack by AN0); closed reads `0x00`, open `0x80`, as named constants. `TryReadButton` returns false when detached or in the window, which is computed on each read from the stamp. `IsDrivingPaddles` is `IsAttached()`
- [X] T010 [P] Create `UnitTest/EmuTests/SiriusJoyportTests.cpp` using a real `AppleSoftSwitchBank` for the annunciators and a local `uint64_t` for the cycle counter: all 12 (index, AN0, AN1) combinations against a jack pattern where each switch of each jack is distinguishable; detached returns false; `OnMachineReset` opens the window, which still declines at `kReleaseCycles - 1` and answers at `kReleaseCycles`; `SetAttached (true)` while running answers at once; `OnMachineReset` while detached, then attach, still declines until the window ends; `IsDrivingPaddles` true through the window. Confirm tests fail with AN0 and AN1 swapped in the table
- [X] T011 Add `SiriusJoyport * m_joyport = nullptr` plus `SetJoyport (SiriusJoyport *)` to `CassoEmuCore/Machines/Apple2/Common/AppleGamePort.h/.cpp`: `ReadButton` asks `m_joyport->TryReadButton` first and uses its value when it returns true (the button-read event carries the value returned); `ReadPaddle` returns the "timer running" value (bit 7 set) while `m_joyport->IsDrivingPaddles()`. A null pointer is today's behavior
- [X] T012 Same in `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eKeyboard.h/.cpp` for `$C061`-`$C063` (on false, the existing Open Apple or hold, Closed Apple or hold, and Shift or //c mouse logic), and in `CassoEmuCore/Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h/.cpp` for `ReadPaddle`
- [X] T013 In `CassoEmuCore/Shell/MachineHost.h/.cpp`, own the Joyport (`SetJoyport (std::unique_ptr<SiriusJoyport>)`, `GetJoyport()` const and non-const, beside the mouse) and call `m_joyport->OnMachineReset()` as the last step of `SoftReset` and of `PowerCycle`, after the CPU's reset, when a Joyport exists. (no `MachineRefs` entry: it holds only pointers into the owned device list)
- [X] T014 In `CassoEmuCore/Shell/MachineBuilder.cpp`, when the machine definition has `hasAnnunciators`, create the Joyport, wire `SetAnnunciatorSource` (the `softSwitches` ref on the ][/][+, `iieSoftSwitches` on the //e), `SetCycleSource (cpu->GetCycleCounterPtr())` in `CreateCpu` beside the paddle-timer wiring, `gamePort->SetJoyport` or `iieKeyboard->SetJoyport` plus `iieSoftSwitches->SetJoyport`, hand it to the host (`MachineBuilder::WireJoyport`, after `CreateCpu`, using `GetCycleCounterPtr`). The //c gets none
- [X] T015 [P] Extend `UnitTest/EmuTests/MachineBuildTests.cpp`: the ][+, //e and enhanced //e build a Joyport wired to their button device; the //c builds none (`GetJoyport()` is null)
- [X] T016 Create `UnitTest/EmuTests/JoyportMachineTests.cpp` on `TestMachine` (][+ and //e): with a test-attached Joyport past its window and a jack pattern set through `SetJackSwitches`, a guest program `STA $C059 / STA $C05B / LDA $C062` (right jack, up) and the other combinations read the pattern; `$C064`-`$C067` read bit 7 set; detached, the existing expectations of `KeyboardTests` (`OpenAppleReadable_C061` etc.) and `GamePortTests` hold on the same machine. Confirm a test fails with the reading devices' Joyport call removed

### Sink path (contracts/switch-evaluation.md, research R7)

- [X] T017 Add `JoyportJacks jacks` to `GamePortState` in `CassoEmuCore/Controllers/GamePortInputMixer.h` (equality and change detection include it), and `SiriusJoyport * joyport` to `GamePortTargets` in `CassoEmuCore/Shell/MachineGamePortSink.h`; add `WriteJacks` to `MachineGamePortSink.cpp`, called from `TryApply`, writing each jack whose switches differ from `lastApplied` (all when `lastApplied` is null) and skipping a null `joyport`; fill `joyport` in the targets lambda in `CassoEmuCore/Shell/EmulatorShell.cpp`
- [X] T018 [P] Extend `UnitTest/ControllerTests/MachineGamePortSinkTests.cpp`: jacks reach a real `SiriusJoyport`, only the changed jack is written, a null Joyport (the //c) is skipped without failing the write. Confirm a test fails with `WriteJacks` stubbed
- [X] T019 Build x64 Debug and x64 Release (constitution Quality Gate 1), run `scripts/RunTests.ps1`, then commit: `feat(joyport): Annunciators and the Joyport device model`

**Checkpoint**: the machines record AN0-AN2, and a test-attached Joyport answers button and paddle reads on the ][+ and //e. Nothing in the UI attaches it.

---

## Phase 3: User Story 1 - Play a Joyport game with one controller (Priority: P1) MVP

**Goal**: a single controller, or the arrow keys, drives the Joyport's switches on both jacks, and the picker attaches it (not yet saved).

**Independent Test**: quickstart V1 and V2 on the readout disk; SC-001 for the left jack in single-source mode.

### Tests for User Story 1

- [X] T020 [P] [US1] Extend `UnitTest/ControllerTests/MappingEvaluatorTests.cpp`: an Absolute stick at 0.49 and 0.51 of the shaped deflection on each of the four directions (open, then closed); a diagonal closes one horizontal and one vertical; a digital pair closes at once and both held closes neither; a Rate binding closes while deflected and opens when released although its paddle byte stays put; an inverted axis swaps the switches; PB0 bindings drive Fire and PB1/PB2 bindings do not. Confirm tests fail with switches computed from the paddle byte
- [X] T021 [P] [US1] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: in single-source mode the merged contribution's `jacks` carry the selection's switches on both jacks; with no driver they are all open. Confirm a test fails with the right jack left open
- [X] T022 [P] [US1] Extend `UnitTest/ControllerTests/GamePortInputMixerTests.cpp` per the contract's owner table: Controller owner uses the Controller source's jacks; ArrowKeys owner derives Left/Up from 0 and Right/Down from 255, Fire from FireKeys PB0, on both jacks; MousePaddle owner and None owner leave them all open, even with a FireKeys contribution present; AppleModifierKeys never closes a switch. Confirm a test fails with AppleModifierKeys included
- [X] T023 [P] [US1] Extend `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp`: with `SetJoyportFns` offered, a checkable **Sirius Joyport** row sits after the source rows and before the separator above Multiplayer, checked exactly when `isOn` returns true, and its dispatch calls the toggle once; not offered, no row
- [X] T024 [P] [US1] Extend `UnitTest/ControllerTests/InputModeRulesTests.cpp` for `InputModeRules::GetFireKeyButtons`: detached, PB0 from X or left Alt and PB1 from Z or right Alt, as today; attached, PB0 from X only and PB1 from Z only, so neither Alt key (the //e's Open Apple and Closed Apple) closes Fire (FR-010). Confirm a test fails with the Alt keys kept while attached

### Implementation for User Story 1

- [X] T025 [US1] In `CassoEmuCore/Controllers/MappingEvaluator.h/.cpp`, add `static constexpr float kSwitchThreshold = 0.5f` and fill `GamePortContribution::switches` in `Evaluate` from the shaped PDL0 and PDL1 values inside `EvaluatePair` (after deadzone, calibration and inversion, before `ToAxisPaddle`, for Rate bindings too), using the winning binding per axis: `<= -kSwitchThreshold` closes Left/Up, `>= +kSwitchThreshold` closes Right/Down; Fire from `IsButtonListHeld` over the PB0 bindings
- [X] T026 [US1] In `CassoEmuCore/Controllers/ControllerInputService.cpp`, after `BuildMergedLocked`, set `merged.jacks` in single-source mode to the selection driver's `logical->switches` on both jacks, all open with no driver (done as `AddJoyportSwitches`, a static helper called for each driver inside `BuildMergedLocked`, which also covers the multiplayer rule of T051)
- [X] T027 [US1] In `CassoEmuCore/Controllers/GamePortInputMixer.cpp`, fill `GamePortState::jacks` in `ComputeTargetLocked` per the owner table in `contracts/switch-evaluation.md` (a static helper per source kind keeps the function short)
- [X] T028 [US1] In `CassoEmuCore/Shell/EmulatorShell.h` and a shell `.cpp` beside the mouse toggle, add `SetGamePortAdapter (GamePortAdapter)` and `GetGamePortAdapter()`: store the value, call `SetAttached` on the live machine's Joyport under the shared lifetime lock, and call `SyncSelectorState`. Not persisted yet (US4)
- [X] T029 [US1] Add `static std::bitset<2> GetFireKeyButtons (bool xDown, bool zDown, bool leftAltDown, bool rightAltDown, bool isJoyportAttached)` to `CassoEmuCore/Controllers/InputModeRules.h/.cpp`; make `EmulatorShell::UpdateJoystickButtonsFromKeys` in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` read the four keys and submit its result, and call `UpdateJoystickButtonsFromKeys` from `SetGamePortAdapter` while arrows-to-joystick is on, so a held Alt is dropped or restored at the moment of attaching or detaching
- [X] T030 [US1] In `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp`, add `SetJoyportFns (isOn, isOffered, toggle)` and the checkable **Sirius Joyport** `DxuiCommand` in `GetPaddlePickerItems` at the position T023 tests; wire it in `CassoEmuCore/Shell/Window/EmulatorWindow.cpp` beside `SetMouseModeFns`, with `isOffered` from the running machine's `hasAnnunciators` and the toggle flipping `SetGamePortAdapter`
- [X] T031 [US1] Create `Disks/Casso/JoyportTest.bas` in the style of `Disks/Casso/JoystickTest.bas` (uppercase only, for the ][+): for each jack (AN0 off, on) and each AN1 state, `POKE` the annunciators and `PEEK(49249)`-`PEEK(49251)`, and print a two-column table of UP, DOWN, LEFT, RIGHT and FIRE reading CLOSED or OPEN, redrawn in a loop with a Ctrl-C note; build `Disks/Casso/JoyportTest.dsk` with the three `CassoCli disk` commands in `quickstart.md`, and boot it once on the //e to confirm it runs
- [ ] T032 [US1] (Automated part done, including the `GuestVisibleJoyportTests` scenario for SC-001; V1 and V2 with a real controller still to run.) Build x64 Debug and x64 Release, run `scripts/RunTests.ps1`, run quickstart V1 and V2 on the //e and the ][+ with a real controller (ask the user to hold the directions if no controller input can be scripted), record the results in a new `specs/036-sirius-joyport/validation.md`, then commit: `feat(joyport): Drive the Joyport's switches from one controller`

**Checkpoint**: a Joyport game is playable with one controller from a picker click. Do not ship without US2: on the //e a Ctrl-Reset still needs the reset window's validation.

---

## Phase 4: User Story 2 - The //e keeps working with the Joyport attached (Priority: P1)

**Goal**: resets and power-on behave normally with the Joyport attached, and Open Apple through reset still reboots.

**Independent Test**: quickstart V3; SC-003.

### Tests for User Story 2

- [X] T033 [P] [US2] Extend `UnitTest/EmuTests/JoyportMachineTests.cpp` on a //e `TestMachine` with the Joyport attached and every switch open: 20 `SoftReset`s and 20 `PowerCycle`s each end on the normal reset path, with no self-test and no reboot (detect as `OpenAppleResetTests.cpp` does, by where the firmware ends up after its `$C061`/`$C062` reads); with `HoldAppleKeysThroughReset (true, false)`, a `SoftReset` reboots as without a Joyport; after the window, `SetOpenApple (true)` does not change `$C061` and `SetShift (true)` does not change `$C063`. Confirm the first test fails with `OnMachineReset` removed from `MachineHost`
- [X] T034 [P] [US2] In the same file, a held Fire through a //e reset does not change the reset result (switches written before `SoftReset`, window still declines), and a machine switch (a fresh `TestMachine` built and power-cycled with the Joyport attached) opens the window. Then the spec's "machine switch with a controller held" edge case: with Fire and Up held on an attached //e, feed the same `MachineGamePortSink` state to a freshly built ][+ without the Joyport attached, and to a //c; `$C061`-`$C063` read the staged buttons only (nothing stuck closed), and `$C064`-`$C067` time out normally

### Implementation for User Story 2

- [X] T035 [US2] Confirm `MachineHost::SoftReset`/`PowerCycle` (T013) are the only paths the Ctrl-Reset, power-cycle and machine-switch entry points in `CassoEmuCore/Shell/MachineManager.cpp` take, so every reset reaches `OnMachineReset`; fix any path the tests show missing. (Applying the saved adapter to a new machine belongs to T044.)
- [ ] T036 [US2] (Automated part done; V3 in the running app still to run.) Build x64 Debug and x64 Release, run the suite, run quickstart V3 on the //e (20 Ctrl-Resets, 20 power cycles, one Open Apple reset), record in `validation.md`, then commit: `feat(joyport): Release the Joyport's lines after every reset`

**Checkpoint**: US1 and US2 together are the shippable MVP.

---

## Phase 5: User Story 4 - Turning it on and off (Priority: P2)

**Goal**: the setting is saved per machine, shown on the Machine tab, and not offered on the //c. It comes before US3 because it finishes the picker row US1 started.

**Independent Test**: quickstart V5 and V9; SC-007.

### Tests for User Story 4

- [X] T037 [P] [US4] Extend `UnitTest/UiTests/HardwarePageTests.cpp`: `BuildNodes` with `supportsGamePortAdapter` appends a **Game port** group with **None** and **Sirius Joyport**, exactly one checked per `adapter`; unsupported (the //c) has no group
- [X] T038 [P] [US4] Extend `UnitTest/UiTests/SettingsPanelStateTests.cpp`: `gamePortAdapter` round-trips through `ExtractUiPrefs`/`BuildJson`, marks the state dirty, is pushed to `RecordingSink::ApplyGamePortAdapter` on `Apply` and never queues a reset (template: `MouseConnected_DefaultsOnRoundTripsNoReset`); `ObserveLiveGamePortAdapter` with a changed live value while the entry is untouched leaves the state not dirty and makes `BuildJson` write the live value; with a pending user edit, the edit survives the observation. Confirm a test fails with `ArePrefsEqual` ignoring the field
- [X] T039 [P] [US4] Extend `UnitTest/UiTests/ChromeCommandRoutingTests.cpp` with `IDM_GAMEPORT_ADAPTER_NONE` and `IDM_GAMEPORT_ADAPTER_JOYPORT`: unique, routed to the UI thread
- [X] T040 [P] [US4] Extend `UnitTest/UiTests/UserConfigStoreTests.cpp`: `"none"` is omitted by `SaveDelta`; `"siriusJoyport"` is kept per machine and does not leak to another machine
- [X] T041 [P] [US4] Extend `UnitTest/UiTests/MachineInputPrefsTests.cpp` for the helpers T043 adds: `ReadGamePortAdapter` gives `SiriusJoyport` for the token on a machine with annunciators, `None` for a missing or unknown token, and `None` on a machine without annunciators whatever the token; `BuildGamePortAdapterEntry` round-trips through `ReadGamePortAdapter`; written through `UserConfigStore::SpliceUiPrefs` and `SaveDelta` over an `InMemoryFileSystem` (the steps `DiskSettings::WriteSavedUiPrefs` takes, which itself reads the machine's default JSON from disk and so cannot run in a unit test) for the //e and read back through `UserConfigStore::Load` for the //e, the ][+ and the //c, only the //e reads `SiriusJoyport` (SC-007: this is the value the cold-boot and machine-switch paths adopt). Confirm a test fails with the `hasAnnunciators` check removed

### Implementation for User Story 4

- [X] T042 [US4] Add `IDM_GAMEPORT_ADAPTER_NONE` and `IDM_GAMEPORT_ADAPTER_JOYPORT` to `CassoEmuCore/resource.h`, route them in `WindowCommandManager::GetCommandRoute` to a handler in `CassoEmuCore/Shell/WindowCommandManager.cpp` that calls `EmulatorShell::SetGamePortAdapter`
- [X] T043 [US4] In `CassoEmuCore/Config/MachineInputPrefs.h/.cpp`, add `kpszGamePortAdapterKey = "gamePortAdapter"`, `static GamePortAdapter ReadGamePortAdapter (const JsonValue * uiPrefs, bool hasAnnunciators)` and `static std::pair<std::string, JsonValue> BuildGamePortAdapterEntry (GamePortAdapter)`; persist from `SetGamePortAdapter` (`PersistGamePortAdapterForMachine`) through `DiskSettings::WriteSavedUiPrefs` with that entry; the Settings commands use a live-only `ApplyGamePortAdapterLive`, since the sheet saves on its own (never written for a machine without `hasAnnunciators`); add the `"none"` default to `UserConfigStore::BuildUiPrefsDefaults` in `CassoEmuCore/Config/UserConfigStore.cpp`
- [X] T044 [US4] Adopt the pref with `MachineInputPrefs::ReadGamePortAdapter` at cold boot in `EmulatorShell::ApplyPersistedChromePrefs` (`CassoEmuCore/Shell/EmulatorShellPrefs.cpp`) and on machine switch in `MachineManager::SwitchMachine` beside `mouseConnected`, applying it to the new machine's Joyport after `BuildMachineDevices` and before `PowerCycle`; both call sites stay one-line forwarders to the helper, since neither is reachable from a unit test
- [X] T045 [US4] In `CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp`: `SettingsUiPrefs::gamePortAdapter` (default `None`) in `ExtractUiPrefs`, `BuildJson`, `ArePrefsEqual`; `SetGamePortAdapter`; `ISettingsApplySink::ApplyGamePortAdapter` called from `Apply` with no `QueueMachineReset`; `ObserveLiveGamePortAdapter (GamePortAdapter live)` returning whether a rebuild is needed, per `contracts/prefs-and-ui.md`
- [X] T046 [US4] Implement `ApplyGamePortAdapter` in `CassoEmuCore/Ui/Settings/SettingsApplyAdapter.h/.cpp` by posting the matching IDM as `WM_COMMAND`, and add it to the test `RecordingSink`
- [X] T047 [US4] In `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp`, add the `supportsGamePortAdapter` and `adapter` parameters to `BuildNodes`, pass them from `Rebuild`, and route the two labels in `SetOnToggle` to `SetGamePortAdapter`, ignoring a toggle that would leave neither checked
- [X] T048 [US4] In `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`'s `OnDialogTick`, call `ObserveLiveGamePortAdapter` with the shell's live value and rebuild the Machine tab when it returns true
- [ ] T049 [US4] (Automated part done; V5 and V9 in the running app still to run.) Build x64 Debug and x64 Release, run the suite, run quickstart V5 and V9, record in `validation.md`, then commit: `feat(joyport): Save the Joyport per machine and show it on the Machine tab`

**Checkpoint**: attach and detach from either place, saved per machine, never offered on the //c.

---

## Phase 6: User Story 3 - Two players at once (Priority: P2)

**Goal**: multiplayer slot 1 drives the left jack and slot 2 the right, whatever their paddle targets.

**Independent Test**: quickstart V4; SC-001 for both jacks, SC-005.

### Tests for User Story 3

- [X] T050 [P] [US3] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp` using `SetUpTwoPlayers`: player 2's fire closes the right jack's Fire and not the left's (US3 scenario 1); both players moving at once each close only their own jack; slot targets swapped (slot 1 on Joystick1, slot 2 on Paddle0) leave slot 1 on the left jack; player 2 disconnected opens the right jack and leaves the left unaffected; multiplayer configured with no slot connected falls back to the single-source jacks. Confirm a test fails with the jacks taken from the merged buttons

### Implementation for User Story 3

- [X] T051 [US3] (Landed with T026 in `AddJoyportSwitches`.) Extend the jack placement in `CassoEmuCore/Controllers/ControllerInputService.cpp` for live multiplayer: slot 1's driver on the left jack and slot 2's on the right, from each driver's pre-merge `logical->switches`, all open for an absent or disconnected slot
- [ ] T052 [US3] (Automated part done with Phase 3; V4 with two controllers still to run.) Build x64 Debug and x64 Release, run the suite, run quickstart V4 with two controllers, record in `validation.md`, then commit: `feat(joyport): Give each multiplayer slot its own Joyport jack`

**Checkpoint**: two-player Joyport games work.

---

## Phase 7: User Story 5 - See the switches on the Controllers page (Priority: P2)

**Goal**: five live switch lights and the jack caption on the Controllers page while the Joyport is attached.

**Independent Test**: quickstart V8.

### Tests for User Story 5

- [X] T053 [P] [US5] Extend `UnitTest/ControllerTests/ControllersPageStateTests.cpp`: `GetJoyportJack()` is `Both` in single-source mode, `Left` for player 1's controller and `Right` for player 2's in multiplayer, and follows Editing when it moves; `ComputeLiveReading` carries `switches` for the page's pending mapping. Confirm a test fails with `Both` returned always

### Implementation for User Story 5

- [X] T054 [US5] Add `GetJoyportJack()` (a free `enum class JoyportJack { None, Left, Right, Both }` in the same header, and a pure `GetJoyportHeading`) to `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp`
- [X] T055 [US5] In `CassoEmuCore/Ui/Settings/ControllersPage.h/.cpp`, add `SetJoyportAttachedFn` and five `ButtonLightView`s in a cross over the stick's square (Up, Down, Left, Right around Fire), with the Joystick heading reading "Joyport: left jack", "right jack" or "both jacks"; in `Layout`, show them and hide the stick, the button lights and their headings while attached; in `Poll`, light them from `reading.switches` and `Relayout` when the attach state changes. Wire the function in `CassoEmuCore/Ui/Settings/SettingsSheet.cpp` to `EmulatorShell::GetGamePortAdapter`
- [ ] T056 [US5] (Automated part done; V8 and the screenshot still to run, since opening Settings comes up over the user's work.) Build x64 Debug and x64 Release, run the suite, run quickstart V8, capture a screenshot of the page with the Joyport attached (DPI-aware `PrintWindow` of the sheet), record in `validation.md`, then commit: `feat(joyport): Show the Joyport's switches on the Controllers page`

**Checkpoint**: all five stories done.

---

## Phase 8: Polish and Cross-Cutting Concerns

- [ ] T057 Run quickstart V6 (Wavy Navy on the ][+), V7 (Boulder Dash on the //e) and V10 (detached, `JoystickTest.dsk`), with the user supplying the game disks; restore the machine's `disk1Path` afterward; record in `validation.md`, including a note for SC-002 that the switches ride the same `Submit` and flush as the controller buttons whose latency spec 034 measured, so no separate measurement was made
- [ ] T058 [P] Add the CHANGELOG `[Unreleased]` entry (terse, user-visible effect only) and a README headline for the Joyport, and show both to the user for approval before any push
- [X] T059 Run the merge gate: `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for x64 Debug and Release, `scripts/RunTests.ps1` Debug and Release (full suite, not filtered), `scripts/CheckStyle.ps1 -Mode Tree`, and `rg -n '\w \(\)'` over the new code; ARM64 build only
- [X] T060 Reconcile `spec.md`, `plan.md` and `tasks.md` with what was built, then commit: `docs(spec): Validation results (036-sirius-joyport)`

---

## 2026-09-27 design (GH #156)

Phases 9-15 cover the difference between the 1.28.0 design above and the spec's `### Session 2026-09-27` clarifications: plan.md's "2026-09-27 update" and slices 9-15, research R14-R24, and the 2026-09-27 sections of data-model.md and contracts/. Earlier tasks stay as built; where one of them built something this design removes or changes, the task below says so. Each test task ends by confirming the test fails with the implementation stubbed or reverted, and each phase has its own mutation-check task that records its results in `validation.md`.

---

## Phase 9: User Story 7 - Profiles belong to one mode (Priority: P1)

**Goal**: every profile belongs to normal or Joyport mode, fixed at creation; lists show only the mode's profiles with its built-in first; creation offers the same starting points in both modes; reset restores the mode's built-in (FR-018, FR-020, research R17). Reworks the part of `3f0c4620` and `c00c2c7a` that let any profile be picked in either mode.

**Independent Test**: quickstart V17; with the Joyport on, a profile created is listed only while it is on, and the Default cannot be picked.

### Tests for Phase 9

- [X] T061 [P] [US7] In `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp`, add profile-mode storage tests: a user profile created in Joyport mode writes `"profileMode": "joyport"` and reads back `ProfileMode::Joyport`; a normal-mode profile writes no `profileMode` member; a profile with no member ("Profiles saved before this change are normal-mode profiles") reads `Normal`; an unknown value reads `Normal` and the profile is kept, not rejected; the Default reads `Normal` and the Joyport built-in reads `Joyport` whatever the file holds; a user profile called Joyport that becomes the built-in keeps its mapping and becomes Joyport-mode. Confirm the tests fail with `ReadProfile` ignoring the member
- [X] T062 [P] [US7] In the same file, add list, create and reset tests: `ControllerModelSettings::GetProfileNames (ProfileMode)` returns the mode's built-in first, then that mode's user profiles in stored order, and none of the other mode's; `ControllerProfileStore::CreateProfile` with every `ProfileSource` value (sweep the enum: `DefaultMapping`, `JoyportMapping`, `CopyOfProfile` from a profile of each mode, `Paddles`) in each mode stamps the mode in effect and starts from the right mapping; a name used in the other mode is `DuplicateName` ignoring case ("Names stay unique per model across both modes"); `ResetProfile` gives a Joyport-mode user profile the Joyport mapping and a normal-mode one the Default mapping, and each built-in its own. Confirm a test fails with `ResetProfile` always using the Default mapping
- [X] T063 [P] [US7] In `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`, rewrite `ChosenProfile_IsRememberedForEachMode` so it picks a normal-mode user profile without the Joyport and a Joyport-mode user profile with it (it now picks the Joyport built-in in normal mode and the Default in Joyport mode, which FR-020 forbids), keeping its per-mode assertions; add `ChosenProfile_OfTheOtherMode_IsIgnored`: `SetActiveProfile` with the Joyport built-in in normal mode, or the Default in Joyport mode, leaves the choice unchanged and plays the mode's built-in, and an entry loaded from prefs that points at the other mode's profile plays the mode's built-in and is gone from `GetActiveProfiles` after the next save. `UnchosenProfile_IsTheModesBuiltInProfile` stays as it is. Confirm the new test fails with the mode check removed
- [X] T064 [P] [US7] In `UnitTest/ControllerTests/ControllersPageStateTests.cpp`: `GetProfileNames` follows the page's mode; `SetProfileMode` after `Load` swaps the list and the edited profile in place (to the controller's choice for the new mode, or the mode's built-in with none), handling a pending edit as `SelectProfile` does; `CreateProfile` stamps the page's mode; `ResetProfile` restores the mode's built-in; the copy sources list the profiles of both modes. Update the existing tests that expect Default and Joyport in every list. Confirm a test fails with `SetProfileMode` only storing the value
- [X] T065 [P] [US7] In `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp`, update the profile-section tests from `3f0c4620`: a Joyport-mode section lists the Joyport profile first and only Joyport-mode names, with no Default; a normal-mode section lists the Default first and no Joyport profile; Automatic still checks the mode's built-in. Confirm a test fails with `SetProfileSections` adding both built-ins again

### Implementation for Phase 9

- [X] T066 [US7] In `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp`: add `ProfileMode mode = ProfileMode::Normal` to `ControllerProfile`; read and write it as `"profileMode"` with the value `"joyport"`, omitted for `Normal`, absent or unknown reading `Normal`; force the built-ins' modes from their kind in `EnsureBuiltInProfiles` and on load; add `GetProfileNames (ProfileMode) const`; give `AddProfile` and `ControllerProfileStore::CreateProfile` a `ProfileMode` parameter; add `ProfileSource::JoyportMapping`; make `ResetProfile` restore `MakeBuiltInMapping (GetAutomaticKind (profile->mode), ...)` for a user profile. Update the `ProfileMode`, `ControllerProfile` and `MakeBuiltInMapping` header comments to state that each profile belongs to one mode
- [X] T067 [US7] In `CassoEmuCore/Controllers/ControllerInputService.h/.cpp`, resolve a controller's profile only from a choice of the mode in effect (else the mode's built-in), make `SetActiveProfile` leave the choice unchanged for a profile of the other mode, and drop other-mode entries from `activeProfiles` / `joyportActiveProfiles` when the choices are next handed back for saving
- [X] T068 [US7] In `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp`, build the list from `GetProfileNames (m_profileMode)`, make `SetProfileMode` reload the list and the edited profile when called after `Load`, pass the page's mode to `CreateProfile`, and list both modes' profiles as copy sources; in `CassoEmuCore/Ui/Settings/ControllersPage.cpp`, add **Joyport mapping** to the create dialog's starting points beside **Default mapping** and **Paddles** (sentence case)
- [X] T069 [US7] In `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`, fill each `EmulatorCommands::ProfileSection`'s `names` from `GetProfileNames (snapshot.profileMode)`, and in `CassoEmuCore/Ui/Chrome/EmulatorCommands.cpp` `SetProfileSections`, drop the rule that puts Default and Joyport at the top of every section (the names already lead with the mode's built-in), updating its banner comment
- [X] T070 [US7] Mutation checks for Phase 9, each against its test group, results recorded in `specs/036-sirius-joyport/validation.md` under a new "Phase 9" heading: `GetProfileNames` ignoring the mode; `ReadProfile` ignoring `profileMode`; `ResetProfile` always the Default mapping; `SetActiveProfile` accepting the other mode's profile; `SetProfileMode` not reloading
- [X] T071 [US7] Build x64 Debug and Release, run `scripts/RunTests.ps1` (full suite), then commit: `feat(controllers): Give each profile a mode, fixed when it is created` (body: GH #156)

**Checkpoint**: each mode lists and plays only its own profiles.

---

## Phase 10: User Story 7 - The Joyport profile's second stick on a DirectInput gamepad (Priority: P1)

**Goal**: FR-017's DirectInput rule: on a DirectInput gamepad the Joyport profile also steers with Z/Rz, else Rx/Ry; joysticks and wheels unchanged; no trigger ever steers (research R18).

**Independent Test**: quickstart V16.

### Tests for Phase 10

- [X] T072 [P] [US7] In `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp`, beside the built-in profile tests, add `DefaultMapping::FindSecondStick` and `MakeJoyport` tests: a DirectInput gamepad reporting Z and Rz gets Z/Rz; one reporting Z, Rx and Ry but no Rz gets Rx/Ry (an Xbox-class pad read through DirectInput); Rx and Ry only gets Rx/Ry; neither pair gets none; a joystick and a wheel with Z/Rz get none; an XInput controller still gets its right stick; the pair is bound Absolute on PDL0 and PDL1 after the primary stick; for every `ControllerFormFactor` value (sweep the enum) no `ControlKind::Trigger` appears in `pdl0`-`pdl3`. Confirm a test fails with `FindSecondStick` trying Rx/Ry before Z/Rz

### Implementation for Phase 10

- [X] T073 [US7] In `CassoEmuCore/Controllers/ControlMapping.h/.cpp`, add `static std::optional<std::pair<ControlId, ControlId>> FindSecondStick (ControllerFormFactor formFactor, const std::vector<ControlId> & controls)` with the axis indexes as class constants (Z 2, Rx 3, Ry 4, Rz 5, matching `DirectInputSampleDecoder::Decode`'s x, y, z, rx, ry, rz order), give `MakeJoyport` a `ControllerFormFactor formFactor` parameter after the model, bind the pair for a DirectInput gamepad, and rewrite the `MakeJoyport` banner comment to state the rule
- [X] T074 [US7] Pass `ControllerFormFactor` through the built-in mapping path with no default argument, so the compiler finds every caller: `ControllerModelSettings::MakeBuiltInMapping`, `EnsureBuiltInProfiles` and `ResetProfile`, and `ControllerProfileStore::GetBuiltInSettings`, `GetOrCreateModel`, `CreateProfile` and `ResetProfile` in `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp`; the callers in `CassoEmuCore/Controllers/ControllerInputService.cpp` (from `device->formFactor`) and `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp` (add `ControllerFormFactor formFactor` to `ControllerEntry`, filled in `Load` and `UpdateDevices`); update every test call site
- [X] T075 [US7] Mutation checks for Phase 10, recorded in `validation.md`: `FindSecondStick` ignoring the form factor; preferring Rx/Ry; binding a trigger axis
- [X] T076 [US7] Build x64 Debug and Release, run the full suite, then commit: `feat(controllers): Steer with a DirectInput gamepad's second stick in the Joyport profile` (body: GH #156)

**Checkpoint**: FR-017 complete for every kind of controller.

---

## Phase 11: User Story 4 - A global Joyport setting (Priority: P2)

**Goal**: the Joyport setting is global, adopts the launched machine's saved value once, and reads as off on the //c without changing (FR-002, SC-007, research R14, R16).

**Independent Test**: quickstart V12, V13 and V14.

### Tests for Phase 11

- [ ] T077 [P] [US4] Create `UnitTest/ControllerTests/JoyportSettingTests.cpp` (add to `UnitTest/UnitTest.vcxproj`): `IsInEffect` for all four combinations of setting and `hasAnnunciators`; `IsMousePaddleOffered` true only when not in effect; `ResolveAtLaunch`: a set global token (`"none"` or `"siriusJoyport"`) wins and is not adopted whatever the machine block holds; an empty global token with the launched //e's `$cassoUiPrefs.gamePortAdapter` = `"siriusJoyport"` gives `SiriusJoyport`, adopted; empty with no machine key gives `None`, adopted; empty with the //c (`hasAnnunciators` false) and a key present gives `None`, adopted; an unknown global token gives `None`, not adopted. Confirm a test fails with `IsInEffect` ignoring `hasAnnunciators`
- [ ] T078 [P] [US4] Extend `UnitTest/UiTests/GlobalUserPrefsTests.cpp`: `gamePortAdapter` round-trips through `ToJson` / `FromJson`; an absent key loads as empty; a set value is written
- [ ] T079 [P] [US4] Extend `UnitTest/UiTests/MachineInputPrefsTests.cpp` and `UnitTest/UiTests/UserConfigStoreTests.cpp`: `ReadGamePortAdapter` stays as the adoption reader (T041's reading cases stand); a machine block holding `gamePortAdapter` round-trips untouched through `SaveDelta`; no path writes the key any more. Remove the T041 case that persists the key through `SpliceUiPrefs`, since nothing persists it

### Implementation for Phase 11

- [ ] T080 [US4] Create `CassoEmuCore/Controllers/JoyportSetting.h/.cpp` (add to `CassoEmuCore/CassoEmuCore.vcxproj`), static members only, per data-model.md: `IsInEffect`, `IsMousePaddleOffered`, and `ResolveAtLaunch` returning `JoyportLaunchSetting { GamePortAdapter setting; bool isAdopted; }`
- [ ] T081 [US4] In `CassoEmuCore/Config/GlobalUserPrefs.h/.cpp`, add `std::string gamePortAdapter` ("empty == never set"), read and written as the global key `gamePortAdapter`
- [ ] T082 [US4] In `CassoEmuCore/Shell/EmulatorShell.h` and `CassoEmuCore/Shell/EmulatorShellPrefs.cpp`: at cold boot, a one-line forwarder calls `JoyportSetting::ResolveAtLaunch` with `m_globalPrefs.gamePortAdapter` and the launched machine's ui prefs, and saves the global prefs when `isAdopted`; `SetGamePortAdapter` stores the global token and saves with `SaveGlobalPrefs` instead of `PersistGamePortAdapterForMachine`, which is removed; `AdoptGamePortAdapterForMachine` becomes `ApplyGamePortAdapterToMachine()`, setting the Joyport's attached state and `ControllerInputService::SetJoyportAttached` from `IsInEffect`; add `IsJoyportInEffect() const`; `GetGamePortAdapter` returns the setting
- [ ] T083 [US4] In `CassoEmuCore/Shell/MachineManager.cpp`'s `SwitchMachine`, replace the per-machine read with a call to `ApplyGamePortAdapterToMachine` after `BuildMachineDevices` and before `PowerCycle` (one-line forwarder), so the //c reads the Joyport as off and switching back restores it
- [ ] T084 [US4] In `CassoEmuCore/Shell/Window/EmulatorWindow.cpp`, wire the picker row's `isOn` to `IsJoyportInEffect` and `isOffered` to the running machine's `hasAnnunciators`
- [ ] T085 [US4] Mutation checks for Phase 11, recorded in `validation.md`: `IsInEffect` ignoring `hasAnnunciators`; `ResolveAtLaunch` adopting when the global token is set; `GlobalUserPrefs` not writing the key
- [ ] T086 [US4] Build x64 Debug and Release, run the full suite, then commit: `feat(joyport): Make the Joyport setting global` (body: GH #156)

**Checkpoint**: one setting for every machine that can use it; the //c ignores it and leaves it alone.

---

## Phase 12: User Story 4 - The Apple / Atari switch on the Controllers page (Priority: P2)

**Goal**: the Controllers page shows the Joyport as the unit's vertical Apple / Atari switch and swaps the profile list in place; the picker row reads "Joyport (Atari mode)" and hides mouse-as-paddle while on; the Machine tab no longer lists the Joyport (FR-001, FR-009, FR-012, FR-020, research R15, R19, R20, R23).

**Independent Test**: quickstart V11, V17 (the page half) and V20.

### Tests for Phase 12

- [ ] T087 [P] [US4] Create `UnitTest/Dxui/DxuiToggleTests.cpp` (add to `UnitTest/UnitTest.vcxproj`): `ComputeTrackAndThumb` for `OnDirection::Right` matches today's horizontal geometry for both states; `Up` and `Down` give a track taller than wide, with the thumb at the top or bottom end as the checked state requires; the default is `Right`; a click and Space flip it the same in every direction. Confirm a test fails with `Down` laid out as `Up`
- [ ] T088 [P] [US4] In `UnitTest/UiTests/HardwarePageTests.cpp`, replace the Game port group tests (T037) with one asserting that `BuildNodes` lists no Joyport and no Game port group on the ][+, the //e or the //c
- [ ] T089 [P] [US4] In `UnitTest/UiTests/SettingsPanelStateTests.cpp`, remove the `gamePortAdapter` tests (T038) and add one asserting that `BuildJson` never writes `gamePortAdapter`, so OK cannot undo a change made from the picker or the page
- [ ] T090 [P] [US4] In `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp`: the row's label is exactly "Joyport (Atari mode)"; it is absent on the //c even with the setting on; checked exactly when `isOn` returns true; mouse-as-paddle is absent while the Joyport is in effect and present otherwise (retarget to Player 1's submenu when spec 034's picker lands). Confirm a test fails with mouse-as-paddle always offered
- [ ] T091 [P] [US4] In `UnitTest/ControllerTests/ControllersPageStateTests.cpp`, test a static `ControllersPageState::GetJoyportSwitchLabel (bool isAtariMode)`: exactly "Atari mode" and "Apple mode"

### Implementation for Phase 12

- [ ] T092 [US4] In `Dxui/Widgets/DxuiToggle.h/.cpp`, add `enum class OnDirection { Right, Up, Down }`, `SetOnDirection` / `GetOnDirection` (default `Right`) and the pure static `ComputeTrackAndThumb (const RECT & pill, OnDirection, bool checked)`; paint from it, leaving the horizontal pill unchanged; update the class banner comment
- [ ] T093 [US4] In `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp`, remove the Game port group (`BuildGamePortGroup`, `SetGamePortChecks`, `ResolveGamePortToggle`, the `supportsGamePortAdapter` and `gamePortAdapter` parameters of `BuildNodes` and its `SetOnToggle` routing)
- [ ] T094 [US4] Remove the sheet's OK-applied path for the setting: `SettingsUiPrefs::gamePortAdapter`, `SetGamePortAdapter`, `ObserveLiveGamePortAdapter` and `ISettingsApplySink::ApplyGamePortAdapter` in `CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp`; `ApplyGamePortAdapter` in `CassoEmuCore/Ui/Settings/SettingsApplyAdapter.h/.cpp` and the test `RecordingSink`; the `OnDialogTick` observation in `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`. Keep `SettingsMachineInfo::supportsGamePortAdapter`
- [ ] T095 [US4] In `CassoEmuCore/Shell/WindowCommandManager.cpp`, route `IDM_GAMEPORT_ADAPTER_NONE` and `IDM_GAMEPORT_ADAPTER_JOYPORT` to `EmulatorShell::SetGamePortAdapter`, and remove `ApplyGamePortAdapterLive` from `CassoEmuCore/Shell/EmulatorShell.h` and `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`, now that nothing calls it
- [ ] T096 [US4] In `CassoEmuCore/Ui/Settings/ControllersPage.h/.cpp`, replace `SetJoyportAttachedFn` with `SetJoyportFns (isOn, isOffered, set)` and add a **Joyport** section at the top of the page, only when offered: a `DxuiToggle` with `OnDirection::Down`, checked for Atari mode, labeled by `GetJoyportSwitchLabel`. On a change it calls `set`, then `ControllersPageState::SetProfileMode`, then relayouts; `Poll` compares `isOn()` with the toggle and applies a picker change the same way without calling `set`. The stick art (`JoyportSwitchView`) follows the mode as before. In `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`, wire `isOn` to `EmulatorShell::IsJoyportInEffect`, `isOffered` to `supportsGamePortAdapter`, and `set` to posting the matching `IDM_GAMEPORT_ADAPTER_*` as `WM_COMMAND`
- [ ] T097 [US4] In `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp`, label the row **Joyport (Atari mode)** and leave mouse-as-paddle out of the picker while `JoyportSetting::IsMousePaddleOffered` returns false; update the comments that still describe a device on the game socket
- [ ] T098 [US4] Mutation checks for Phase 12, recorded in `validation.md`: `ComputeTrackAndThumb` ignoring the direction; mouse-as-paddle always offered; the row offered on the //c
- [ ] T099 [US4] Build x64 Debug and Release, run the full suite, capture the Controllers page with the Joyport section in both modes (DPI-aware `PrintWindow` of the sheet, Casso launched minimized with `--title`), then commit: `feat(joyport): Turn the Joyport on from an Apple / Atari switch on the Controllers page` (body: GH #156)

**Checkpoint**: the Joyport is turned on from the picker or the Controllers page, and nowhere else.

---

## Phase 13: User Story 3 - Jacks and labels from spec 034's players (Priority: P2)

**Goal**: FR-008's jack rules and FR-019's labels, driven by spec 034's player slots (research R21, R22).

**Depends on spec 034**: T100-T103 do not. T104-T106 need spec 034's `PlayerSlotPolicy`, player submenus and notice stack (034 FR-008, FR-040 to FR-044, its "Players (2026-09-27)" data model) to have landed; do not start them before 034's tasks for those are complete.

**Independent Test**: quickstart V18.

### Tests for Phase 13

- [ ] T100 [P] [US3] Create `UnitTest/ControllerTests/JoyportJackRulesTests.cpp` (add to `UnitTest/UnitTest.vcxproj`): every row of the table in `contracts/switch-evaluation.md`; then a sweep of every `JoyportPlayerState` pair with `isPlayer2Disabled` both ways asserting the invariant that a jack's source is always a Driving player and a Held player's jack is never handed to the other. Confirm a test fails with a Held player treated as Idle
- [ ] T101 [P] [US3] Create `UnitTest/ControllerTests/JoyportLabelsTests.cpp` (add to `UnitTest/UnitTest.vcxproj`): with the Joyport in effect, exactly "Joyport left", "Joyport right", "Same as left", "Joyport right: same as left", "Joyport left and right: description" for a controller playing alone, and "Joyport left: description" / "Joyport right: description" otherwise; with it off, spec 034's "Player 1", "Player 2", "Disabled" and "Player N: description". Confirm a test fails with the Joyport labels returned while off

### Implementation for Phase 13

- [ ] T102 [US3] Create `CassoEmuCore/Controllers/JoyportJackRules.h/.cpp` (add to `CassoEmuCore/CassoEmuCore.vcxproj`) per `contracts/switch-evaluation.md`: `JoyportPlayerState`, `JoyportPlayers`, `JoyportJackSource`, `AssignJacks`
- [ ] T103 [US3] Create `CassoEmuCore/Controllers/JoyportLabels.h/.cpp` (add to `CassoEmuCore/CassoEmuCore.vcxproj`), static members only, returning the strings T101 tests
- [ ] T104 [US3] (Needs spec 034's `PlayerSlotPolicy`.) Tests first in `UnitTest/ControllerTests/ControllerInputServiceTests.cpp` and `UnitTest/ControllerTests/GamePortInputMixerTests.cpp` through `FakeControllerBackend`: US3 scenarios 1, 2, 4, 5 and 6; a leaver's jack reads open while the other player keeps only their own; Player 1 on the arrow keys with Player 2 on a controller splits keys left and controller right. Then add `JoyportJackRules::ReducePlayerState` (034's `Playing`, `Provisional` or the arrow keys are Driving; `Held` is Held; the rest Idle), rewrite `ControllerInputService::AddJoyportSwitches` in `CassoEmuCore/Controllers/ControllerInputService.cpp` to place each player's pre-merge switches by `AssignJacks`, and compose keyboard and controller jacks in `CassoEmuCore/Controllers/GamePortInputMixer.cpp`. Update T050's tests that assume slot 1 left and slot 2 right whenever multiplayer is on. Confirm a test fails with the old placement
- [ ] T105 [US3] (Needs spec 034's player submenus.) In `CassoEmuCore/Ui/Chrome/EmulatorCommands.cpp`, take the player row labels, Player 2's Disabled entry and the Automatic row text from `JoyportLabels` while `IsJoyportInEffect`; tests in `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp` (or the picker test file spec 034 creates) for both states
- [ ] T106 [US3] (Needs spec 034's notice stack.) Make spec 034's `PlayerSlotPolicy::DescribeAssignment` take its text from `JoyportLabels` while the Joyport is in effect, including "Joyport left and right: description" while one controller plays alone; tests beside spec 034's notice tests
- [ ] T107 [US3] Mutation checks for Phase 13, recorded in `validation.md`: `AssignJacks` treating Held as Idle; `AssignJacks` ignoring `isPlayer2Disabled`; `JoyportLabels` returning Joyport labels while off; `AddJoyportSwitches` reverted to slot placement
- [ ] T108 [US3] Build x64 Debug and Release, run the full suite, then commit: `feat(joyport): Assign the Joyport's jacks and labels from the players` (body: GH #156)

**Checkpoint**: one player drives both jacks, two split them, and the picker and notices read Joyport left and Joyport right.

---

## Phase 14: Validation and gate (2026-09-27 design)

- [ ] T109 Run quickstart V11-V20 in the running app (V16 needs a DirectInput gamepad and a flight stick, V18 two controllers, V19 the Bandits disk from the user), restoring the global `gamePortAdapter` and `disk1Path` afterward; record in `validation.md` under a new "2026-09-27 design" heading, including SC-009 (V18) and SC-010 (V19)
- [ ] T110 [P] Draft the CHANGELOG `[Unreleased]` entry (terse, user-visible effect only, `GH #156:` first) and the README headline change, and show both to the user for approval before any push
- [ ] T111 Run the merge gate: `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for x64 Debug and Release; `scripts/RunTests.ps1` Debug and Release, full suite, not filtered; `scripts/RunTests.ps1 -Build -Scenario`, since the Joyport is guest-visible; `git add` the new files (not `.specify/feature.json`), then `scripts/CheckStyle.ps1 -Mode Tree`; ARM64 Debug build only; `rg -n '\w \(\)'` over the new code. Record every result in `validation.md`, including any suite that could not run and why
- [ ] T112 Reconcile `spec.md`, `plan.md`, `research.md` and `tasks.md` with what was built, then commit: `docs(spec): Validation results for the GH #156 design (036-sirius-joyport)`

---

## Phase 15 (optional): User Story 6 - The Controller Select switch -- BLOCKED on the FR-016 decision

**Status**: blocked. FR-016 carries an open clarification marker that only the owner can resolve. Do not start any task in this phase until the owner has decided to emulate the switch; if the decision is no, delete this phase. The tasks record the shape research R24 describes so the decision can be made against it.

- [ ] T113 [P] [US6] (Blocked on FR-016.) Extend `UnitTest/EmuTests/SiriusJoyportTests.cpp`: at Left or Right every read comes from that jack whatever AN0 selects; at Center AN0 chooses; a change takes effect on the next read
- [ ] T114 [US6] (Blocked on FR-016.) Add `enum class JoyportControllerSelect { Left, Center, Right }` and `SetControllerSelect` to `CassoEmuCore/Machines/Apple2/Common/SiriusJoyport.h/.cpp`, read on each `TryReadButton`
- [ ] T115 [US6] (Blocked on FR-016.) Save the position globally as `joyportControllerSelect` in `CassoEmuCore/Config/GlobalUserPrefs.h/.cpp`, default Center, with tests in `UnitTest/UiTests/GlobalUserPrefsTests.cpp`, and apply it at launch and on machine switch beside T082 and T083
- [ ] T116 [US6] (Blocked on FR-016.) Create a three-position Dxui control in the toggle's style in `Dxui/Widgets/` with a geometry test in `UnitTest/Dxui/`, and show it beside the Joyport switch in `CassoEmuCore/Ui/Settings/ControllersPage.h/.cpp`, positions left to right, disabled in Apple mode
- [ ] T117 [US6] (Blocked on FR-016.) Run the manual's test program with two controllers for SC-008 and record it in `validation.md`

---

## Phase 16: User Story 7 - A new profile starts only from its own mode (Priority: P1)

**Goal**: FR-020 as changed on 2026-09-27: the New profile dialog offers only the starting points of the mode in effect (Default mapping, Paddles or a copy in normal mode; Joyport mapping or a copy in Joyport mode), and the profile to copy is chosen from a list of that mode's profiles that leaves out the mode's built-in profile while its mapping is still the built-in mapping. Creating a profile refuses a copy of the other mode's profile. Replaces Phase 9's "every source in both modes" and "copy sources list the profiles of both modes".

**Independent Test**: with the Joyport off, New profile offers Default mapping, Paddles and Copy of, and the copy list holds only normal-mode profiles, the Default only once edited; with it on, Joyport mapping and Copy of, with only Joyport-mode profiles.

### Tests for Phase 16

- [X] T118 [P] [US7] In `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp`, add a test that `ControllerProfileStore::CreateProfile` with `ProfileSource::CopyOfProfile` refuses a profile of the other mode, the other mode's built-in profile included, with `ProfileEditResult::NotFound` and adds nothing; rework `CreateProfile_FromEverySourceInEachMode_StampsTheModeInEffect` so a copy across modes is refused while every other source still stamps the mode in effect. Confirm the new test fails with the mode check removed
- [X] T119 [P] [US7] In `UnitTest/ControllerTests/ControllersPageStateTests.cpp`: `GetCopySourceNames` lists only the page mode's profiles; the mode's built-in profile is left out while its mapping equals the built-in mapping, and listed once edited, a pending edit included; with no controller the list is empty; `CreateProfile` refuses a copy of the other mode's profile with `NotFound` and creates nothing; update `ProfileNames_FollowThePagesMode` and `CreateProfile_BelongsToThePagesModeAndResetsToItsBuiltIn`, which expect both modes' profiles. Confirm a test fails with the built-in profile always listed
- [X] T120 [P] [US7] In `UnitTest/ControllerTests/ControllersPageStateTests.cpp`, test the static `ControllersPageState::GetStartingPoints (ProfileMode mode, bool canCopy)`: normal mode gives `DefaultMapping`, `Paddles`, `CopyOfProfile` in that order; Joyport mode gives `JoyportMapping`, `CopyOfProfile`; `CopyOfProfile` is left out when nothing can be copied; and the static `GetStartingPointLabel (ProfileSource)` gives exactly "Default mapping", "Joyport mapping", "Paddles" and "Copy of" (sweep the enum). Confirm a test fails with the Joyport mapping offered in normal mode

### Implementation for Phase 16

- [X] T121 [US7] In `CassoEmuCore/Controllers/ControllerProfileStore.cpp`, make `CreateProfile` return `ProfileEditResult::NotFound` for a `CopyOfProfile` source whose profile belongs to the other mode (`ControllerModelSettings::IsOfOtherMode`), and update the `ProfileSource` and `CreateProfile` comments in `ControllerProfileStore.h/.cpp`
- [X] T122 [US7] In `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp`, rework `GetCopySourceNames` to the page mode's profiles less an unedited built-in profile, make `CreateProfile` refuse a copy of the other mode's profile, and add the static `GetStartingPoints` and `GetStartingPointLabel`
- [X] T123 [US7] In `CassoEmuCore/Ui/Settings/ProfileDialogOverlay.h/.cpp`, replace the fixed starting-point table with the list `OpenNew` is given; show "Copy of" followed by a `DxuiComboBox` of the copy sources, opening in a popup through the host `ControllersPage::SetPopupHost` passes on; choosing from the list selects Copy of; the list takes a focus stop and its own keys while open; the accept callback gets the chosen copy source. In `CassoEmuCore/Ui/Settings/ControllersPage.cpp`, open the dialog with `GetStartingPoints (mode, !names.empty())` and `GetCopySourceNames`, preselecting the edited profile when it is in the list, and create from the chosen source (sentence case)
- [X] T124 [US7] Mutation checks for Phase 16, recorded in `specs/036-sirius-joyport/validation.md` under a new "Phase 16" heading: `CreateProfile` without the mode check; the built-in profile always in the copy list; the Joyport mapping offered in normal mode
- [X] T125 [US7] Build x64 Debug and Release, run the full Release suite, capture the New profile dialog in both modes if Casso can be run minimized, then commit: `feat(controllers): start a new profile only from its own mode` (body: GH #156)

**Checkpoint**: a new profile starts from a mapping of its own mode or from a copy of one of that mode's profiles.

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

### 2026-09-27 phases

- **Phase 9 (profile modes)**: after Phase 8. Blocks Phase 10 (same files) and Phase 12 (the page's list swap).
- **Phase 10 (second stick)**: after Phase 9.
- **Phase 11 (global setting)**: after Phase 8; independent of Phases 9 and 10.
- **Phase 12 (the switch and the picker)**: after Phases 9 and 11.
- **Phase 13 (jacks and labels)**: T100-T103 after Phase 11; T104-T106 after spec 034's player slots, submenus and notice stack.
- **Phase 14 (validation and gate)**: after Phases 9-13.
- **Phase 15**: blocked on the owner's FR-016 decision; independent of Phases 9-14 if adopted.
- **Phase 16 (new profile starting points)**: after Phase 9; independent of Phases 10-15.

### Within Each Story

- Tests first; confirm they fail with the implementation stubbed.
- Pure logic in `CassoEmuCore/Controllers/` and the device before shell wiring; shell wiring before chrome.
- Commit at each phase end.

### Parallel Opportunities

- T004, T007, T010, T015, T018 in Phase 2 (separate test files).
- T020-T024 (US1 tests), T033-T034 (US2), T037-T041 (US4).
- US3 beside US2 and US4 once US1 is done.
- T061-T065 (Phase 9 tests), T077-T079 (Phase 11), T087-T091 (Phase 12), T100-T101 (Phase 13).
- Phase 11 beside Phases 9 and 10; T100-T103 beside Phase 12.

---

## Parallel Example: User Story 1

```text
Task: "Extend MappingEvaluatorTests.cpp (T020)"
Task: "Extend ControllerInputServiceTests.cpp (T021)"
Task: "Extend GamePortInputMixerTests.cpp (T022)"
Task: "Extend PaddleSourceRowsTests.cpp (T023)"
Task: "Extend InputModeRulesTests.cpp (T024)"
```

## Parallel Example: User Story 4

```text
Task: "Extend HardwarePageTests.cpp (T037)"
Task: "Extend SettingsPanelStateTests.cpp (T038)"
Task: "Extend ChromeCommandRoutingTests.cpp (T039)"
Task: "Extend UserConfigStoreTests.cpp (T040)"
Task: "Extend MachineInputPrefsTests.cpp (T041)"
```

## Parallel Example: Phase 12

```text
Task: "Create DxuiToggleTests.cpp (T087)"
Task: "HardwarePageTests.cpp lists no Joyport (T088)"
Task: "SettingsPanelStateTests.cpp never writes the setting (T089)"
Task: "PaddleSourceRowsTests.cpp row label and mouse-as-paddle (T090)"
Task: "ControllersPageStateTests.cpp switch label (T091)"
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

### 2026-09-27 design

1. Phase 9 then Phase 10: the Joyport profile and profile modes (US7, P1) -- the smallest shippable slice of GH #156, since it is what a first Joyport user meets.
2. Phase 11 then Phase 12: the global setting and the Apple / Atari switch (US4).
3. Phase 13: jacks and labels, finished once spec 034's players land (US3).
4. Phase 14: validation and the gate.
5. Phase 15 only if the owner adopts FR-016.

---

## Notes

- Never run `Get-Process Casso | Stop-Process`; stop only the PID you launched.
- Every agent launch of Casso passes `--title <worktree name>` and runs minimized unless the user asked for it.
- `.specify/feature.json` stays out of commits.
- No Claude attribution in commit messages (CheckStyle CS0008).
