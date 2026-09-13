---

description: "Task list for 034 physical game controllers"
---

# Tasks: Physical Game Controllers

**Input**: Design documents from `specs/034-game-controllers/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: Required. Constitution Principle II requires unit tests for all production code, and Principle VI requires every rule to be reachable from `UnitTest`. Each test task ends by confirming the test fails with the implementation stubbed (copilot-instructions, "Verify a new test fails without the fix").

**Organization**: Tasks are grouped by user story so each story can be implemented and validated on its own.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1-US7 from spec.md
- Paths are repository-relative. Every new `.h`/`.cpp` is added to `CassoEmuCore/CassoEmuCore.vcxproj` or `UnitTest/UnitTest.vcxproj` in the same task that creates it (neither project has a `.filters` file).
- Existing functions are referenced by name, not line number; line numbers drift.
- Code style: `.github/copilot-instructions.md` (EHM, column alignment, 5/3 blank lines, `////` banners in `.cpp`, verb-first function names, no magic numbers, American spelling). Run `scripts/CheckStyle.ps1 -Mode Staged` before every commit that adds files.
- Every commit message body includes `Refs #97`.

---

## Phase 1: Setup and Hardware Check

**Purpose**: Resolve the open hardware questions (research R2, R4, R13) before code depends on them.

- [X] T001 Write a throwaway probe console program in the session scratchpad directory (NOT in the repo, never committed) that: (a) calls `XInputGetState` in a tight loop for 10 s per connected slot and prints `dwPacketNumber` changes per second; (b) creates a DirectInput 8 device for each attached game controller whose `DIPROP_GUIDANDPATH` path lacks `IG_`, with `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE` on a message-only window, calls `SetEventNotification`, and prints events per second plus whether `DIDEVCAPS` has `DIDC_POLLEDDEVICE`; (c) registers `RegisterDeviceNotification (GUID_DEVINTERFACE_HID)` and prints each `DBT_DEVICEARRIVAL`/`DBT_DEVICEREMOVECOMPLETE` with its device path; (d) opens a second top-level window and reports whether XInput readings keep changing while that window is active
- [X] T002 Run the probe per `specs/034-game-controllers/quickstart.md` section 2 with the Xbox controller wired, wireless (adapter or Bluetooth), an Xbox 360 receiver if available, and a DirectInput gamepad or joystick; ask the user to move sticks and power controllers on and off at each step
- [X] T003 Record results in `specs/034-game-controllers/research.md` R2, R4 and R13: measured XInput packet rate and the chosen poll period, DirectInput event behavior, which receivers raise HID notifications on controller power on/off (vendor and product IDs of any that do not), the second-window result, and a confirmed Microsoft Learn link for `SetEventNotification`; replace each UNVERIFIED/UNMEASURED marker the probe resolved. Update the spec edge case "Controllers page open in the Settings sheet" in `specs/034-game-controllers/spec.md` to match the second-window result, and if XInput stops there, add a paused-state requirement to FR-023. Delete the probe
- [X] T004 [P] Create `specs/034-game-controllers/validation.md` with sections for the hardware check (T003), each phase's hardware scenario results (T037, T053, T060, T080, T089, T101) and the final walk and measurements (T105)
- [X] T005 Commit: `docs(spec): hardware check results (034-game-controllers)`

**Checkpoint**: Poll period, wake strategy, receiver fallback and Settings-sheet behavior are decided with evidence.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Types, the game-port mixer with every existing writer migrated, the device seam and backend, and the controller thread. No user-visible change.

**CRITICAL**: No user story work begins until this phase is complete.

### Types

- [X] T006 Create `CassoEmuCore/Controllers/ControllerTypes.h` with `ControllerKind`, `ControllerModelKey`, `ControllerUnitKey` (with `source`), `ControllerDeviceInfo`, `ControlKind`, `ControlId`, `ControllerSample` and `GamePortContribution` (`std::optional<std::array<Byte, 2>>` paddle, `std::bitset<3>` buttons) per `specs/034-game-controllers/data-model.md`; plain data, equality operators on keys
- [X] T007 Create `CassoEmuCore/Controllers/ControllerTokens.h/.cpp` (class `ControllerTokens`, static members) converting `ControllerModelKey`, `ControllerUnitKey` and `ControlId` to and from the token forms in data-model.md (`xinput:045e:0b13`, `xinput:generic`, `dinput:044f:b10a/{...}`, `axis:1`, `dpad-left:0`); parsing returns `HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)` on malformed input
- [X] T008 [P] Create `UnitTest/ControllerTests/ControllerTokensTests.cpp`: round trip for every `ControlKind`, both controller kinds, the generic Xbox model, units with and without an id; each malformed form rejected; confirm a test fails with parsing stubbed

### Game port mixer (contracts/game-port-mixer.md)

- [X] T009 Create `CassoEmuCore/Controllers/GamePortInputMixer.h/.cpp` with `GamePortSource` (ArrowKeys, FireKeys, AppleModifierKeys, MousePaddle, Controller), `AxisOwner`, `GamePortState`, `IGamePortSink` (`TryApply (target, lastApplied)` returning `bool`, lastApplied null meaning write every field) and `GamePortInputMixer` (`SetSink`, `SetApplyThread`, `SetAxisOwner`, `Submit`, `ReleaseSource`, `NotifyMachineRebuilt`, `FlushPending`, `HasPendingWrite`, `GetTargetState`) per the contract: buttons OR across sources for PB0-PB2, axes from the owner or center 127, sink called only on change, only on the apply thread and outside the internal mutex; a submission from another thread requests one flush; a refused write stays pending until the next apply-thread call or `NotifyMachineRebuilt`
- [X] T010 [P] Create `UnitTest/ControllerTests/RecordingGamePortSink.h` recording each applied state and able to refuse the next N writes
- [X] T011 Create `UnitTest/ControllerTests/GamePortInputMixerTests.cpp`: keyboard PB0 held plus controller PB0 pressed then released keeps PB0 pressed; owner switch uses the new owner's last contribution immediately; no owner rests at center; no sink call on an unchanged submission; a new sink's first write covers every field; `NotifyMachineRebuilt` rewrites every field; PB2 ORs like PB0/PB1; a refused release is delivered by `FlushPending`, by the next `Submit`, and by `NotifyMachineRebuilt`, with `HasPendingWrite` true only in between; a submission from another thread requests one flush and does not write; confirm tests fail with OR replaced by last-writer and with retry removed
- [X] T012 Create `CassoEmuCore/Shell/MachineGamePortSink.h/.cpp` implementing `IGamePortSink` over a `GamePortTargets` structure (game port, //e soft-switch bank, //e keyboard) looked up on every write under the lifetime lock: ][/][+ paddles and buttons 0-2 to `AppleGamePort::SetPaddle`/`SetButton`; //e and //c paddles to `Apple2eSoftSwitchBank::SetPaddle`, PB0/PB1 to `Apple2eKeyboard::SetOpenApple`/`SetClosedApple`; PB2 to `Apple2eKeyboard::SetShift` unless `HasMouseOnShiftLine()` (the //c; add that accessor to `Apple2eKeyboard.h`); take the lifetime lock with `std::try_to_lock` and return false if unavailable; return true without writing when every target is null (FR-017)
- [X] T013 Create `UnitTest/ControllerTests/MachineGamePortSinkTests.cpp` using real `AppleGamePort`, `Apple2eSoftSwitchBank` and `Apple2eKeyboard` instances constructed as in `UnitTest/EmuTests/GamePortTests.cpp`: ][+ routing of both paddles and PB0-PB2 read back through `$C061`-`$C065`; //e routing with PB2 reading as Shift at `$C063`; //c leaves `$C063` (mouse button) untouched; no game port returns true and writes nothing; a lifetime lock held exclusively by the test returns false; confirm a test fails with PB2 routing removed
- [X] T014 Add `GamePortInputMixer` and `MachineGamePortSink` members to `CassoEmuCore/Shell/EmulatorShell.h`; at startup set the sink (targets read from `m_machine.GetRefs()`) and `SetApplyThread` to the UI thread with a flush request that posts a new `WM_APP_GAMEPORT_FLUSH` (in `CassoEmuCore/Shell/EmulatorShellInternal.h`) whose handler calls `FlushPending`; call `NotifyMachineRebuilt` in `CassoEmuCore/Shell/MachineManager.cpp` right after the exclusive lifetime lock is released at the end of the rebuild
- [X] T015 Migrate `EmulatorShell::UpdateJoystickAxesFromKeys` and `UpdateJoystickButtonsFromKeys` in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` to `Submit (GamePortSource::FireKeys, ...)` and axis contributions for `AxisOwner::ArrowKeys`, preserving the X/Z plus Alt OR and the foreground-only reads
- [X] T016 Migrate `ApplyAppleModifierKeys`, `ReleaseGuestKeys`, `PushPaddlePosition`, `PushPaddleButton` and `StopPaddleCapture` in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` to `Submit`/`ReleaseSource` with `AppleModifierKeys` and `MousePaddle`; set the axis owner from `SetArrowsJoystick`, `SetPointerMapping` and `SetInputMappingMode`; then grep `CassoEmuCore/` for `SetPaddle (`, `SetButton (`, `SetOpenApple (`, `SetClosedApple (` and confirm no caller remains outside device classes and `MachineGamePortSink.cpp`
- [X] T017 Build Debug x64 with `scripts/Build.ps1`; run `scripts/RunTests.ps1 -Filter GamePort`, `-Filter InputEvent`, `-Filter MachineGamePortSink`, and confirm `UnitTest/EmuTests/GamePortTests.cpp` and `InputEventCoalescingTests.cpp` pass unchanged; launch Casso minimized with `--title` (workspace rules), boot a //e and a ][+, and confirm arrows-to-joystick, Open/Solid-Apple, Shift and mouse-to-paddle behave as before
- [X] T018 Commit: `refactor(shell): game-port mixer as the single paddle and button writer`

### Device seam, decoders, backend, thread (contracts/controller-backend.md)

- [X] T019 Create `CassoEmuCore/Seams/IControllerBackend.h` with `IControllerBackend` (`Initialize`, `Shutdown`, `EnumerateDevices`, `ReadSample`, `GetWakeSources`) and `IControllerBackendEvents` (`OnDevicesChanged`) per the contract
- [X] T020 [P] Create `CassoEmuCore/Controllers/DirectInputSampleDecoder.h/.cpp`: static members decoding a `DIJOYSTATE2`-mirroring POD (declared in the header so tests need no DirectInput headers) into `ControllerSample`: axes from [-32768, 32767] to [-1, 1] for present slots, POV hundredths of a degree to D-pad bits (centered when `LOWORD == 0xFFFF`, diagonals set two bits), 128 buttons
- [X] T021 [P] Create `CassoEmuCore/Controllers/XInputSampleDecoder.h/.cpp`: static members decoding an `XINPUT_GAMEPAD`-mirroring POD into `ControllerSample` with the fixed Xbox `ControlId` layout in data-model.md (left stick axis 0/1 with Y oriented so up is negative, right stick axis 3/4, triggers 0/1 from 0-255, A/B/X/Y buttons 0-3, LB/RB 4/5, Back/Start 6/7, LS/RS 8/9, D-pad hat 0), plus a static returning the Xbox control list
- [X] T022 [P] Create `UnitTest/ControllerTests/DirectInputSampleDecoderTests.cpp`: axis extremes and center, POV centered, each cardinal and diagonal, absent slots stay 0, button bits; confirm a test fails with POV decoding stubbed
- [X] T023 [P] Create `UnitTest/ControllerTests/XInputSampleDecoderTests.cpp`: every button bit to its `ControlId`, thumb extremes including -32768, triggers at 0 and 255, D-pad diagonals, Y orientation; confirm a test fails with the bit table altered
- [X] T024 [P] Create `UnitTest/ControllerTests/FakeControllerBackend.h`: scripted devices with `AddDevice`, `RemoveDevice`, `SetSample`, `FailNextRead (HRESULT)`, `RaiseDevicesChanged`, and scripted `GetWakeSources` facts (event handles, polled flag)
- [X] T025 Create `CassoEmuCore/Seams/Win32ControllerBackend.h/.cpp` implementing the contract (the backend owns the message-only window): `#pragma comment (lib, ...)` for `xinput.lib`, `dinput8.lib`, `dxguid.lib`, `hid.lib` in the `.cpp` (convention of `CassoEmuCore/WasapiAudio.cpp`; spacing as in `CassoEmuCore/Devices/Printer/PngCodec.cpp`); add `<Xinput.h>`, `<dinput.h>` (after `#define DIRECTINPUT_VERSION 0x0800`), `<hidsdi.h>` and `<dbt.h>` to `CassoEmuCore/Pch.h`; `Initialize` creates the window, HID notification registration and `DirectInput8Create`, and resolves `XInputGetCapabilitiesEx` ordinal 108 with `GetProcAddress`, tolerating its absence; `EnumerateDevices` lists connected XInput slots (model from ordinal 108, else the vendor/product of the single `IG_` raw-input HID device when only one Xbox model is attached, else `xinput:generic`), then `DI8DEVCLASS_GAMECTRL` attached devices whose path lacks `IG_`, with `DIPROP_VIDPID`, serial from `HidD_GetSerialNumberString` else `guidInstance`, `DIPROP_RANGE` [-32768, 32767], `SetDataFormat (&c_dfDIJoystick2)`, background non-exclusive cooperative level, `SetEventNotification`, and `EnumObjects` for the control list; `ReadSample` decodes through T020/T021, skips decoding when `dwPacketNumber` is unchanged, retries `Acquire` + `Poll` once on `DIERR_INPUTLOST`/`DIERR_NOTACQUIRED`, and returns `HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED)` with `connected = false` for a vanished device; `WM_DEVICECHANGE` arrival/removal schedules rescans at +300 ms and +2 s; the R4 receiver fallback per T003; EHM throughout
- [X] T026 Create `CassoEmuCore/Shell/ControllerInputThread.h/.cpp`: owns a `std::thread` that calls `CoInitializeEx (COINIT_MULTITHREADED)` (tolerating `RPC_E_CHANGED_MODE`), initializes the backend, and loops on `MsgWaitForMultipleObjectsEx` over the backend's event handles with the timeout the service requests (T034), dispatching the window's messages; each wake calls the service tick, whose submissions to the mixer request a UI-thread flush (the controller thread never writes the machine); shutdown posts `WM_QUIT` to the thread and joins; the loop stays thin so the logic is in the service
- [X] T027 Build with `-Target Rebuild` (Pch.h changed); run `scripts/RunTests.ps1 -Filter Controller`
- [X] T028 Commit: `feat(input): controller backend over XInput and DirectInput`

**Checkpoint**: Devices enumerate and read through the seam; existing input unchanged.

---

## Phase 3: User Story 1 - Play with a connected controller (Priority: P1) MVP

**Goal**: A selected, connected controller drives PDL0/PDL1 and PB0/PB1 with the default mapping and deadzone, only while Casso is active.

**Independent Test**: With `FakeControllerBackend` reporting known samples, the recording sink sees center at rest, 0/255 at extremes, proportional values between, and PB0/PB1 from the first two buttons; on hardware, quickstart scenarios 2, 3, 6 and 10.

### Tests for User Story 1

- [X] T029 [P] [US1] Create `UnitTest/ControllerTests/DeadzoneShaperTests.cpp`: inside the deadzone reads exactly center; the deadzone edge maps to center and full deflection to 0/255; radial when both axes come from one stick, axial otherwise; monotonic between; confirm failure with rescaling stubbed
- [X] T030 [P] [US1] Create `UnitTest/ControllerTests/MappingEvaluatorTests.cpp` (absolute bindings only): default mapping on the Xbox control list and on a DirectInput list lacking button 1; empty target reads center/released; two bindings on one axis choose the one furthest from center; digital pair drives 0/255 and both held reads center; analog-to-button thresholds for a trigger and each axis direction; confirm failure with button OR replaced
- [X] T031 [P] [US1] Create `UnitTest/ControllerTests/ControllerInputServiceTests.cpp` (US1 subset): the selected connected fake controller produces the expected mixer contributions; unselected controllers are ignored (FR-009); `SetActive (false)` releases the controller source and ignores samples until reactivated (FR-033); a failed read is reported as disconnected, never as a healthy rest sample (FR-015); no sink write while the sample is unchanged; the requested wait timeout is "none" while no controller is selected even with controllers attached, and the poll period only when the selected controller is XInput or polled DirectInput (SC-007)

### Implementation for User Story 1

- [X] T032 [P] [US1] Create `CassoEmuCore/Controllers/DeadzoneShaper.h/.cpp` per research R8: defaults as named constants (XInput's published 7849/32767 for Xbox-class; Casso's 12% for DirectInput), radial and axial shaping with rescale
- [X] T033 [P] [US1] Create `CassoEmuCore/Controllers/ControlMapping.h/.cpp` with `AxisBinding`, `ButtonBinding`, `ControlMapping` (pdl0, pdl1, pb0, pb1, pb2) and `DefaultMapping::For (model, controls)` per data-model.md; `response` and `maxSpeed` fields exist, but only `Absolute` is evaluated until US5
- [X] T034 [US1] Create `CassoEmuCore/Controllers/MappingEvaluator.h/.cpp` (sample + mapping + deadzone to `GamePortContribution`) and `CassoEmuCore/Controllers/ControllerInputService.h/.cpp`: holds the backend, attached device list, current selection, activation state and mixer pointer; `Tick()` reads the selected device, evaluates and submits `GamePortSource::Controller`; `SetActive (bool)` releases on false; `GetWaitTimeout()` computes the timed-poll decision from the selection and the backend's wake facts; a thread-safe snapshot (devices, connected state, last sample) for UI readers
- [X] T035 [US1] Wire into the shell: construct `Win32ControllerBackend`, `ControllerInputService` and `ControllerInputThread` in `CassoEmuCore/Shell/EmulatorShell.h/.cpp` startup and stop them before machine teardown at shutdown; call `SetActive` from `EmulatorShell::OnActivateApp` (`CassoEmuCore/Shell/Window/EmulatorWindow.cpp`), which covers the Settings sheet as part of the app
- [X] T036 [US1] Temporary selection for this phase only: the service selects the first enumerated controller when none is selected, so US1 is testable before US2; T045 replaces it with the selection policy
- [ ] T037 [US1] Build; run `scripts/RunTests.ps1 -Filter Controller`; run quickstart sections 3-4 scenarios 2, 3, 6 and 10 on hardware (launch with `--title`), recording results in `specs/034-game-controllers/validation.md`
- [X] T038 [US1] Commit: `feat(input): physical controller drives the game port`

**Checkpoint**: MVP. A plugged-in controller plays joystick games.

---

## Phase 4: User Story 2 - Choose the controller and keep the choice (Priority: P2)

**Goal**: Automatic selection and adoption per FR-032, manual selection from the Machine menu and toolbar, persisted per machine.

**Independent Test**: With two fake controllers, selecting each routes only its input; saved prefs restore the selection; a connect with none selected selects and notifies; a selected unit that returns with a new identity is adopted; menu and toolbar rows reflect attached controllers and the selection.

### Tests for User Story 2

- [X] T039 [P] [US2] Create `UnitTest/ControllerTests/ControllerSelectionPolicyTests.cpp` covering every row of the policy table in data-model.md: connect with none selected (including present at start and on machine switch) selects the first in enumeration order and requests arrows/paddle off and a notice; connect with a selection changes nothing; selected DirectInput unit absent with exactly one same-model unit attached adopts it; two same-model units attached adopts nothing; the user choosing arrows or paddle clears the selection; Xbox model selection matches any unit and the lowest slot wins; a machine without a game port keeps but ignores the selection
- [ ] T040 [P] [US2] Extend `UnitTest/UiTests/MachineInputPrefsTests.cpp`: `controller` and `controllerProfile` round trip; absent keys mean none and Default; existing `arrowsToJoystick`/`pointerMapping` behavior unchanged
- [X] T041 [P] [US2] Create `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp`: one row per attached controller plus a selected-but-disconnected one; exactly one row checked; the disconnected row disabled; the label the strip wears is the checked row's short label, and "Controller" when none is checked; a command pointer taken before a rebuild stays valid until the rebuilt rows replace the menu items
- [X] T042 [P] [US2] Create `UnitTest/ControllerTests/InputModeRulesTests.cpp`: selecting a controller turns off arrows-to-joystick and mouse-to-paddle; turning either on clears the controller selection; the resulting axis owner for each combination, including the disconnect fallback
- [X] T043 [P] [US2] SUPERSEDED with T048; `DxuiTimedInfoBanner` arrived from master with its own tests.
- [X] T044 [P] [US2] DROPPED with the submenu. `DeferredMenuRebuild` existed so a menu holding controller rows could be rebuilt while closed; the Machine menu carries no dynamic rows now, and the command bar re-lays itself on every change (T052).

### Implementation for User Story 2

- [X] T045 [US2] Create `CassoEmuCore/Controllers/ControllerSelectionPolicy.h/.cpp` per data-model.md, including adoption, and replace T036's temporary rule in `ControllerInputService`
- [X] T046 [US2] Extend `CassoEmuCore/Config/MachineInputPrefs.h/.cpp` with `controller` and `controllerProfile` per `contracts/prefs-schema.md`; extend `AdoptInputModeForMachine` and `PersistInputModeForMachine` in `CassoEmuCore/Shell/EmulatorShellPrefs.cpp`; XInput selections persist the model token, DirectInput the unit token; persist adoption
- [X] T047 [US2] Create `CassoEmuCore/Controllers/InputModeRules.h/.cpp` per T042 and route `SetArrowsJoystick`, `SetPointerMapping`, `SetInputMappingMode` and controller selection in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` through it; set the mixer's axis owner from its result
- [X] T048 [US2] SUPERSEDED by master's `DxuiTimedInfoBanner` (merge 6c5f5826), which owns the banner, scrim and layout as well as the text and expiry. The branch's own `TransientNoticeState` was deleted for it. The controller notice calls `EmulatorShell::ShowNotice`, and the controller thread reaches the UI thread through its `PostNotice` (FR-032).
- [X] T049 [US2] Build the paddle-source rows in `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp`: `SetPaddleSources` materializes one `DxuiCommand` per `InputModeRules::PaddleSource` with `isChecked` reading the entry and `isEnabled` false for a controller that is not connected, `GetPaddleSourceItems` returns them as `DxuiPopupMenuItem`s, and `SetPaddleSourcePickedFn` carries the pick back to the shell
- [X] T050 [US2] Add the `kIdPaddle` drop-down entry to `s_kToolbarRows` with a game-controller glyph, and give its command a `labelText` returning `GetCheckedPaddleSourceLabel()` so the strip wears the source that is driving (FR-008b). The Machine menu keeps its existing per-source toggles: no submenu, no cascade (FR-008)
- [X] T051 [US2] DONE, and further than planned: with the picker wearing the source on its face the cluster was left toggling one thing, so `InputClusterEntry` is deleted outright and mouse mode is a plain `kIdMouse` toggle beside the picker. Its monoline painters moved to `InputMonoGlyphs`, where the picker reaches them too.
- [X] T052 [US2] Rebuild the rows and re-lay the strip from `EmulatorShell::SyncPaddleSourceList` in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` whenever the device list, the selection or the input mode changes, and hand them to the toolbar through `SetDropDownItems (kIdPaddle, ...)`; the picker's width moves with its label, so the strip is laid out again on every change
- [ ] T053 [US2] Build; run `scripts/RunTests.ps1 -Filter Controller`, `-Filter MachineInputPrefs`, `-Filter Chrome`, `-Filter MenuBar`; run quickstart scenario 1, the selection half of scenario 8, and the controller rows of scenario 11 on hardware
- [ ] T054 [US2] Commit: `feat(input): controller selection, persistence, menu and toolbar`

**Checkpoint**: Selection is automatic, manual and persistent.

---

## Phase 5: User Story 3 - Unplug and replug mid-session (Priority: P2)

**Goal**: Disconnect releases the port and hands the axes to another attached controller, or to the arrow keys when there is none; reconnect resumes; status is visible.

**Independent Test**: A fake controller held at full deflection with PB0 is removed; the sink sees center and released, then a second attached fake owns the axes; with no second fake, arrow contributions own them instead; re-adding the original resumes it and the stand-in stops driving.

### Tests for User Story 3

- [X] T055 [P] [US3] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: removal releases within one tick and hands the axes to the longest-attached other controller, falling back to `AxisOwner::ArrowKeys` only when none is attached, in both cases without changing the saved selection; a release refused by the sink still arrives through `FlushPending`; reconnect restores `AxisOwner::Controller` and arrows held at that moment stop driving; rescans at +300 ms and +2 s are idempotent; the device list update is visible in the snapshot
- [X] T056 [P] [US3] Extend `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp`: the picker's label and tooltip for connected, not connected, and another controller standing in. The glyph does not change (T059). The cluster this task named is gone (T051).

### Implementation for User Story 3

- [X] T057 [US3] Implement disconnect release, stand-in and reconnect in `CassoEmuCore/Controllers/ControllerInputService.cpp` and `ControllerSelectionPolicy.cpp` (FR-008a, FR-010): another attached controller takes the axes, ordered by how long it has been attached so the stand-in does not change as unrelated controllers come and go, and the arrow keys stand in only when none is attached. Cover reads that fail with `ERROR_DEVICE_NOT_CONNECTED` before the removal notification arrives
- [X] T058 [US3] In `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`, run the arrow-key joystick update while the fallback is active regardless of `m_arrowsJoystick` (spec edge case)
- [X] T059 [US3] Show the stand-in on the PICKER, which is the only thing still carrying this answer once T051 folds the cluster's icons out (FR-008a, FR-013). Its label already wears the source that is driving, so a stand-in reads as the stand-in's own name; the tooltip on `kIdPaddle` says which controller is chosen and that it is not connected, and the chosen-but-absent row keeps its "(not connected)" text. DECIDED: the label and tooltip carry it, and the glyph gets no disconnected state -- it follows the device, and a stand-in is a real device, so a third state would describe a different controller than the one the label names. Recorded in the spec's Clarifications.
- [ ] T061a [US3] Rework the tests for lock-on-active selection (spec clarifications of 2026-09-12; FR-008a, FR-011, FR-013): in `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`, removal of the selected controller makes the longest-attached unassigned controller the saved selection, a returning controller does not take the axes back, and removal of the last controller leaves selection none with no `AxisOwner::ArrowKeys`; in `InputModeRulesTests.cpp` and `PaddleSourceRowsTests.cpp`, delete the stand-in cases and the not-connected row; a saved controller absent at launch is replaced per FR-032
- [ ] T061b [US3] Implement lock-on-active in `CassoEmuCore/Controllers/ControllerInputService.cpp`, `ControllerSelectionPolicy.cpp` and `InputModeRules`: remove the stand-in, take-back and not-connected-row code, keep attach order for choosing the next controller, persist the new selection, and raise the FR-013 notice; remove T058's fallback path from `EmulatorWindowInput.cpp` and the stand-in bar from `EmulatorShellChrome.cpp`
- [ ] T060 [US3] Build; run `-Filter Controller` and `-Filter Chrome`; run quickstart scenarios 4 and 5 and scenario 11's plug-in-while-menu-open on hardware
- [X] T061 [US3] Commit: `feat(input): controller hot-plug with arrow-key fallback` (landed as `13d55b2a`, subject reworded)

**Checkpoint**: Disconnects never leave the port stuck.

---

## Phase 6: User Story 4 - A worn or off-center stick still works (Priority: P3)

**Goal**: Automatic calibration for DirectInput units, a user calibration that replaces it, persisted per unit; none for Xbox-class.

**Independent Test**: A fake DirectInput unit with an offset rest and short travel reads center and 0/255 after automatic learning; a user calibration replaces it and survives a prefs round trip; an Xbox-class fake gets no calibration.

### Tests for User Story 4

- [ ] T062 [P] [US4] Create `UnitTest/ControllerTests/CalibrationTests.cpp`: every transition in the data-model state diagram; center captured at connect; limits widen only outward; user calibration skips connect-time capture (FR-007a); invariant `minimum < center < maximum` rejected on load; Xbox-class units bypass calibration (FR-018a); a unit keyed by `InstanceGuid` with no saved calibration starts automatic and never receives another unit's saved calibration
- [ ] T063 [P] [US4] Create `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp` (calibration part): calibration JSON per `contracts/prefs-schema.md` round trips through `UnitTest/UiTests/InMemoryFileSystem.h`; an invalid entry is dropped, reported, and the unit falls back to automatic; unknown keys inside `controllers` survive a save

### Implementation for User Story 4

- [ ] T064 [P] [US4] Create `CassoEmuCore/Controllers/ControllerCalibration.h/.cpp` per data-model.md, applied before `DeadzoneShaper` in `MappingEvaluator`
- [ ] T065 [US4] Create `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp` holding calibration by unit token with `ToJson`/`FromJson` per the schema, and add the `controllers` section to `CassoEmuCore/Config/GlobalUserPrefs.h/.cpp` following the `monitorTilt` pattern (field, `s_kKnownTopLevel`, `ToJson`, `FromJson`), preserving unknown keys inside `controllers`
- [ ] T066 [US4] Report unreadable calibration entries once through the transient notice (T048) on load
- [ ] T067 [US4] Build; run `-Filter Calibration`, `-Filter ControllerProfileStore`, `-Filter GlobalUserPrefs`
- [ ] T068 [US4] Commit: `feat(input): automatic and saved controller calibration`

**Checkpoint**: Worn sticks read correctly without UI. The Calibrate action's UI lands with the Controllers page.

---

## Phase 7: User Story 5 - Remap controls for a particular controller (Priority: P3)

**Goal**: A Controllers page in the Settings sheet with live readings, press-to-assign mapping (including rate response and PB2), deadzone and Calibrate under Apply/Cancel, editing and persisting the Default profile.

**Independent Test**: `ControllersPageState` with a fake controller: capture assigns the pressed control and ignores pre-held ones; edits apply only on Apply and revert on Cancel; live readings reflect the edited mapping; rate bindings hold on release; PB2 unavailable on the //c; the Default profile's mapping and the model's deadzone survive a save and reload.

### Tests for User Story 5

- [ ] T069 [P] [US5] Create `UnitTest/ControllerTests/ControlCaptureTests.cpp`: activation assigns the first control crossing its threshold after capture began; a control held or deflected when capture began is ignored until released; cancel assigns nothing
- [ ] T070 [P] [US5] Extend `UnitTest/ControllerTests/MappingEvaluatorTests.cpp`: rate response (FR-021a) accumulates by `deflection * maxSpeed * elapsed`, clamps to 0-255, holds on release, resets on selection/profile/machine change; inverted axes; PB2 bindings produce PB2 in the contribution
- [ ] T071 [P] [US5] Create `UnitTest/ControllerTests/ControllersPageStateTests.cpp`: pending edits do not reach the service before Apply; Cancel restores the baseline including a captured calibration; controls assigned to two targets are reported (FR-025); adding an assignment never removes another; a controller removed while open keeps edits and shows disconnected; Reset to Defaults; the PB2 target is unavailable when the current machine is a //c
- [ ] T072 [P] [US5] Extend `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp`: the Default profile's mapping (including `response`, `maxSpeed` and `pb2`) and the per-model deadzone round trip per `contracts/prefs-schema.md`; Default recreated when missing; out-of-range deadzone clamped

### Implementation for User Story 5

- [ ] T073 [P] [US5] Create `CassoEmuCore/Controllers/ControlCapture.h/.cpp` per T069
- [ ] T074 [US5] Implement rate response and PB2 in `CassoEmuCore/Controllers/MappingEvaluator.cpp` (accumulators owned by the evaluator; elapsed time from an injected clock so tests control it); create `CassoEmuCore/Controllers/ControlLabels.h/.cpp` with Xbox and DirectInput display labels
- [ ] T074a [P] [US5] Create `UnitTest/ControllerTests/ControlLabelsTests.cpp`: a label for every `ControlKind` on both controller kinds, Xbox names (A/B/X/Y, LT/RT) distinct from the DirectInput numbered fallback, and an unknown control index labeled rather than empty (constitution II, VI)
- [ ] T075 [US5] Extend `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp` with per-model settings holding the deadzone and the Default profile (created from `DefaultMapping` on first use), with JSON per the schema; the service reads the Default profile's mapping and the model deadzone from the store
- [ ] T076 [US5] Create `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp`: pure page model with the controller being edited, pending mapping edits per target (add by capture or list, remove, invert, response and max speed, thresholds), deadzone, the Calibrate flow (center, then full travel, then a pending user calibration), "use automatic calibration", Reset to Defaults, and live readings computed from the service snapshot under the edited mapping; while the page is open the service polls the edited controller (T034's timeout rule)
- [ ] T077 [US5] Create `CassoEmuCore/Ui/Settings/ControllersPage.h/.cpp` as a `DxuiPropertyPage` following `CassoEmuCore/Ui/Settings/PrintingPage.h`: controller list, per-target binding lists with add/remove and capture buttons, invert/response/speed/threshold controls, deadzone slider, Calibrate and Use Automatic buttons (hidden for Xbox-class), live readout of controls and PDL0/PDL1/PB0-PB2, PB2 shown unavailable on the //c; register it in `OnBuildPages` in `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`
- [ ] T078 [US5] Extend `CassoEmuCore/Ui/Settings/SettingsApplyController.h/.cpp` with the controllers baseline, dirty comparison, commit and revert following the printer fields; commit writes to `ControllerProfileStore` and saves global prefs
- [ ] T079 [US5] Add an optional page argument to `OpenSettings` in `CassoEmuCore/Shell/EmulatorShellDialogs.cpp`, threaded to `DxuiPropertySheet::SetActivePage`; add a static `Controller Settings...` command with an id in a new range in `CassoEmuCore/resource.h`, an `OnCommand` branch in `CassoEmuCore/Shell/WindowCommandManager.cpp`, and its row in the Machine menu (`s_kMenuEntries` in `EmulatorCommands.cpp`) and in `InputClusterEntry::GetPickerItems`
- [ ] T080 [US5] Build; run `-Filter Controller`, `-Filter Settings`; run quickstart scenarios 7, 9 and 10b on hardware, plus scenario 10a using a rate binding built by hand on the Default profile
- [ ] T081 [US5] Commit: `feat(input): Controllers settings page with remapping and calibration`

**Checkpoint**: Any controller can be remapped and calibrated, and the mapping persists.

---

## Phase 8: User Story 6 - Keep a mapping per game and switch between them (Priority: P3)

**Goal**: Named profiles per model, create/copy/rename/delete/reset, the Paddles template, active profile per machine, switchable from the menu and toolbar.

**Independent Test**: Two fake profiles with different PB0 bindings route only the active one; prefs round trip keeps both and the active choice; deleting the active profile falls back to Default; switching releases controls no longer bound.

### Tests for User Story 6

- [ ] T082 [P] [US6] Extend `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp`: Default never deletable or renamable; names unique case-insensitively, trimmed, 1-40 characters (FR-027); create from default, from a copy, and from the Paddles template; reset; `FindProfile (model, name)`; every remaining rejection rule in `contracts/prefs-schema.md`, including duplicate names
- [ ] T083 [P] [US6] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: a controller of a known model but an unrecognized unit drives the port with that model's profiles available on its first connection (SC-009); switching profiles releases buttons and centers axes no longer driven (FR-030) and resets rate accumulators; a remembered missing profile uses Default without recreating it (FR-029)
- [ ] T084 [P] [US6] Extend `UnitTest/ControllerTests/PaddleSourceRowsTests.cpp` with profile rows: one per profile of the selected model, checked equals active, rows rebuilt on create/rename/delete

### Implementation for User Story 6

- [ ] T085 [US6] Add named profiles to `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp` with JSON per the schema, and `DefaultMapping::MakePaddles` in `CassoEmuCore/Controllers/ControlMapping.cpp`
- [ ] T086 [US6] Active profile per machine: read and write `controllerProfile` (T046), apply it in `ControllerInputService`, release on switch (FR-030)
- [ ] T087 [US6] Add profile management to `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp` and `ControllersPage.h/.cpp`: profile list, New (Default, copy of current, Paddles), Rename, Delete, Reset; name validation messages in the fixed error format from `.github/copilot-instructions.md`; keep-or-discard prompt when switching profiles with unapplied edits; edits apply to the profile they were made on
- [ ] T088 [US6] Add profile rows to `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp` beside the paddle-source rows, as their OWN command-bar drop-down rather than a Machine-menu submenu (FR-008, FR-031: no cascades, and the Machine menu keeps its existing entries), rebuilt and re-laid the way `SyncPaddleSourceList` does it (T052)
- [ ] T089 [US6] Build; run `-Filter Controller`, `-Filter Chrome`; run quickstart scenarios 8, 10a (Paddles template) and 11 (profile rows) on hardware
- [ ] T090 [US6] Commit: `feat(input): named controller profiles`

**Checkpoint**: All six user stories work.

---

## Phase 9: User Story 7 - Two people play at once (Priority: P2)

**Goal**: Controllers are assigned to analog axes rather than to one joystick slot, so two people play at once, one controller can drive four axes, and the //c offers only the two it has.

**Independent Test**: Two fake controllers assigned to PDL0 and PDL1 each move only their own axis with both moving at once; both buttons register together; on a //c only PDL0 and PDL1 are offered.

### Tests for User Story 7

- [ ] T091 [P] [US7] Extend `UnitTest/ControllerTests/GamePortInputMixerTests.cpp`: per-axis ownership, two `Controller` contributions holding PDL0 and PDL1 independently, an axis assigned away displacing its previous owner (FR-036), and buttons still ORing across both
- [ ] T092 [P] [US7] Extend `UnitTest/ControllerTests/MappingEvaluatorTests.cpp`: a mapping driving all four axes; bindings on PDL2/PDL3 ignored on a two-axis machine without faulting (FR-035)
- [ ] T093 [P] [US7] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: two assigned fakes drive their own axes simultaneously; one disconnecting releases only its own axes and leaves the other's readings uninterrupted (SC-012)
- [ ] T094 [P] [US7] Add axis-budget tests: a //e reports four axes and a //c two; an assignment naming PDL2 on a //c is ignored but retained, and comes back on switching to a //e (FR-035)

### Implementation for User Story 7

- [ ] T095 [US7] Widen `GamePortContribution::paddle` to four per-axis optionals and `GamePortState::paddle` to four bytes in `CassoEmuCore/Controllers/ControllerTypes.h` and `GamePortInputMixer.h`; make `SetAxisOwner` take an axis index (FR-036)
- [ ] T096 [US7] Write PDL2/PDL3 through `CassoEmuCore/Shell/MachineGamePortSink.cpp`, and report the machine's axis count from the machine config so the //c reports two (FR-034)
- [ ] T097 [US7] Widen `ControlMapping` to four axis targets and update `MappingEvaluator` accordingly; keep the default mapping claiming PDL0/PDL1 only (FR-038)
- [ ] T098 [US7] Hold the assignment as controller-to-axes in `ControllerSelectionPolicy`, persist it per machine, and displace the previous owner on reassignment (FR-036, FR-037)
- [ ] T099 [US7] Add the axis assignment UI to the Controllers page: which controller holds which axis, axes the machine lacks not offered (FR-035, FR-037)
- [ ] T100 [US7] Build a four-axis readout disk from `Disks/Casso/JoystickTest.bas` showing PDL(0)-PDL(3) and PB0-PB2, for validating two-controller play without a commercial two-player disk
- [ ] T101 [US7] Build; run `-Filter Controller`; validate two controllers at once on hardware against the readout disk, and on a two-player game disk if one is available
- [ ] T102 [US7] Commit: `feat(input): assign controllers to game-port axes`

**Checkpoint**: Two people play at once; the //c offers two axes and the //e four.

---

## Phase 10: Polish and Pre-Merge Gates

- [ ] T103 [P] Add a `[Unreleased]` entry to `CHANGELOG.md` with `GH #97:` first, one or two lines of user-visible effect; show it to the user and wait for approval
- [ ] T104 [P] Update `README.md` headline features if controller support belongs there; show it to the user and wait for approval
- [ ] T105 Walk `specs/034-game-controllers/quickstart.md` sections 3-4 end to end, scenarios 1 through 15 including the two-controller scenarios 12-15 (SC-011, SC-012) on Release x64 and record outcomes in `specs/034-game-controllers/validation.md`, including any scenario that could not run and why. Measure and record, using temporary local instrumentation that is not committed: SC-002, time from a changed sample read to the sink write, plus one frame; SC-005, time from removal to the sink's release write and from arrival to the first controller write; SC-007, Casso's CPU time over 60 s idle on the //e with controllers attached and none selected, compared against the same run on master
- [ ] T106 Merge `origin/master` into the branch; rebuild with `-Target Rebuild`; fix any renames the compiler surfaces
- [ ] T107 `git add -A`, then `scripts/CheckStyle.ps1 -Mode Tree`; fix every hit
- [ ] T108 `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for Debug and Release, x64 and ARM64; zero warnings
- [ ] T109 Full suite with `scripts/RunTests.ps1` in Debug and Release; confirm `UnitTest.dll` is newer than the build before trusting the result
- [ ] T110 Present every commit subject, the CHANGELOG entry and any README change for approval; push only after approval; merge to master with `--no-ff`, subject `merge(input): 034 physical game controllers (...)`, body `Closes #97`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Phase 1**: none. T003 feeds the poll period and receiver fallback into T025, T026 and T034.
- **Phase 2**: after Phase 1. The mixer group (T009-T018) and the backend group (T019-T028) are independent and can run in parallel.
- **US1**: after Phase 2.
- **US2**: after US1 (replaces the temporary selection).
- **US3**: after US2 (fallback depends on selection state and the cluster segment).
- **US4**: after US1; independent of US2 and US3.
- **US5**: after US2 (Controller Settings entry beside the controller rows) and US4 (Calibrate UI and the profile store).
- **US6**: after US5 (profile management lives on the page).
- **US7**: after US5 (the assignment UI lives on the Controllers page) and US6. It widens types that US1-US6 already use, so doing it last means widening once; doing it first would mean building the selection policy twice.
- **Polish**: after the stories to ship.

### Within Each Story

- Tests first; confirm they fail with the implementation stubbed.
- Pure logic in `CassoEmuCore/Controllers/` before shell wiring; shell wiring before chrome.
- Commit at each phase end with `Refs #97`.

### Parallel Opportunities

- T008, T010, T020-T024 (new files).
- Test tasks marked [P] within each story.
- US4 beside US2 and US3 once US1 is done.

---

## Parallel Example: Phase 2

```text
Task: "Create CassoEmuCore/Controllers/DirectInputSampleDecoder.h/.cpp (T020)"
Task: "Create CassoEmuCore/Controllers/XInputSampleDecoder.h/.cpp (T021)"
Task: "Create UnitTest/ControllerTests/DirectInputSampleDecoderTests.cpp (T022)"
Task: "Create UnitTest/ControllerTests/XInputSampleDecoderTests.cpp (T023)"
Task: "Create UnitTest/ControllerTests/FakeControllerBackend.h (T024)"
```

## Parallel Example: User Story 2

```text
Task: "ControllerSelectionPolicyTests.cpp (T039)"
Task: "InputModeRulesTests.cpp (T042)"
Task: "TransientNoticeStateTests.cpp (T043)"
Task: "PaddleSourceRowsTests.cpp (T041)"
```

---

## Implementation Strategy

### MVP (User Story 1)

1. Phase 1 hardware check.
2. Phase 2: mixer migration, then backend and thread.
3. Phase 3: a plugged-in controller plays.
4. Stop and validate on hardware (quickstart scenarios 2, 3, 6, 10).

### Incremental Delivery

1. US2: automatic and manual selection, persistence, menu and toolbar rows.
2. US3: hot-plug fallback and status.
3. US4: calibration.
4. US5: Controllers page and persisted Default mapping.
5. US6: profiles.

Each phase ends with a commit and leaves the build and suite green, so the branch could merge to master after any story.

---

## Notes

- Never run `Get-Process Casso | Stop-Process`; stop only the PID you launched.
- Every agent launch of Casso passes `--title <worktree name>` and runs minimized unless the user asked for it.
- `.specify/feature.json` stays out of commits.
- No Claude attribution in commit messages (CheckStyle CS0008).
