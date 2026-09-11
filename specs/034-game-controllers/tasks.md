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
- **[Story]**: US1-US6 from spec.md
- Paths are repository-relative. Every new `.h`/`.cpp` is added to `CassoEmuCore/CassoEmuCore.vcxproj` (and `.filters`) or `UnitTest/UnitTest.vcxproj` in the same task that creates it.
- Code style: `.github/copilot-instructions.md` (EHM, column alignment, 5/3 blank lines, `////` banners in `.cpp`, verb-first function names, no magic numbers, American spelling). Run `scripts/CheckStyle.ps1 -Mode Staged` before every commit that adds files.

---

## Phase 1: Setup and Hardware Check

**Purpose**: Resolve the open hardware questions (research R2, R4, R13) before code depends on them, and create the folders.

- [ ] T001 Write a throwaway probe console program in the scratchpad directory (NOT in the repo, never committed) that: (a) calls `XInputGetState` in a tight loop for 10 s per connected slot and prints `dwPacketNumber` changes per second; (b) creates a DirectInput 8 device for each attached non-`IG_` game controller with `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE` on a message-only window, calls `SetEventNotification`, and prints event count per second plus whether `DIDEVCAPS` has `DIDC_POLLEDDEVICE`; (c) registers `RegisterDeviceNotification (GUID_DEVINTERFACE_HID)` and prints each `DBT_DEVICEARRIVAL`/`DBT_DEVICEREMOVECOMPLETE` with the device path; (d) opens a second top-level window and reports whether XInput readings keep changing while that window is active
- [ ] T002 Run the probe per `specs/034-game-controllers/quickstart.md` section 2 with the Xbox controller wired, wireless (adapter or Bluetooth), an Xbox 360 receiver if available, and a DirectInput gamepad or joystick; ask the user to physically move sticks and power controllers on and off when needed
- [ ] T003 Record the measured XInput packet rate, the chosen poll period, DirectInput event behavior, which receivers raise HID notifications on controller power on/off (with vendor and product IDs of any that do not), the second-window result, and a confirmed Microsoft Learn link for `SetEventNotification` in `specs/034-game-controllers/research.md` R2, R4 and R13, replacing each UNVERIFIED/UNMEASURED marker the probe resolved; delete the probe
- [ ] T004 [P] Create folders `CassoEmuCore/Controllers/` and `UnitTest/ControllerTests/` with their first files in later tasks, and add matching filter groups `Controllers` and `ControllerTests` to `CassoEmuCore/CassoEmuCore.vcxproj.filters` and `UnitTest/UnitTest.vcxproj.filters`
- [ ] T005 Commit: `docs(spec): hardware check results (034-game-controllers)`

**Checkpoint**: Poll period, wake strategy and receiver fallback are decided with evidence.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Types, the game-port mixer with every existing writer migrated, the device seam and backend, and the controller thread. No user-visible change yet.

**CRITICAL**: No user story work begins until this phase is complete.

### Types

- [ ] T006 Create `CassoEmuCore/Controllers/ControllerTypes.h` with `ControllerKind`, `ControllerModelKey`, `ControllerUnitKey` (with `source`), `ControllerDeviceInfo`, `ControlKind`, `ControlId`, `ControllerSample`, `GamePortContribution` (`std::optional<std::array<Byte, 2>>` paddle, `std::bitset<3>` buttons) exactly per `specs/034-game-controllers/data-model.md`; plain data only, equality operators where the data model uses keys
- [ ] T007 Create `CassoEmuCore/Controllers/ControllerTokens.h/.cpp` (class `ControllerTokens` with static members) converting `ControllerModelKey`, `ControllerUnitKey` and `ControlId` to and from the token forms in data-model.md (`xinput:045e:0b13`, `xinput:generic`, `dinput:044f:b10a/{...}`, `axis:1`, `dpad-left:0`); parsing returns `HRESULT` with `ERROR_INVALID_DATA` on malformed input
- [ ] T008 [P] Create `UnitTest/ControllerTests/ControllerTokensTests.cpp`: round trip for every `ControlKind`, both controller kinds, generic Xbox model, unit with and without id; each malformed token rejected; confirm a test fails with parsing stubbed

### Game port mixer (contracts/game-port-mixer.md)

- [ ] T009 Create `CassoEmuCore/Controllers/GamePortInputMixer.h/.cpp` with `GamePortSource`, `AxisOwner`, `IGamePortSink` and `GamePortInputMixer` (`Attach`, `SetAxisOwner`, `Submit`, `ReleaseSource`, `ReleaseAll`) per the contract: buttons OR across sources for PB0-PB2, axes from the owner or center 127, sink called only on change and outside the internal mutex
- [ ] T010 [P] Create `UnitTest/ControllerTests/RecordingGamePortSink.h` recording each `SetPaddle`/`SetButton` call
- [ ] T011 Create `UnitTest/ControllerTests/GamePortInputMixerTests.cpp`: keyboard PB0 held plus controller PB0 pressed then released keeps PB0 pressed; owner switch Controller to ArrowKeys uses the arrows' last contribution immediately; no sink call on an unchanged submission; `Attach (nullptr)` drops writes and a new sink receives current values; `ReleaseAll` returns everything to rest; PB2 ORs like the others; confirm tests fail with OR replaced by last-writer
- [ ] T012 Create `CassoEmuCore/Shell/MachineGamePortSink.h/.cpp` implementing `IGamePortSink` over `MachineRefs` (`CassoEmuCore/Shell/MachineRefs.h`): ][/][+ to `AppleGamePort::SetPaddle`/`SetButton` (indexes 0-2); //e and //c paddles to `Apple2eSoftSwitchBank::SetPaddle`, PB0/PB1 to `Apple2eKeyboard::SetOpenApple`/`SetClosedApple`; PB2 on the //e to the keyboard's Shift state (add a setter on `Apple2eKeyboard` if none exists, keeping the existing Shift key path working through the mixer's `AppleModifierKeys` source) and dropped on the //c (keyboard has a mouse attached); take `GetLifetimeLock()` with `std::try_to_lock` and leave the change pending if unavailable; no-op when both `gamePort` and `iieSoftSwitches` are null (FR-017)
- [ ] T013 Add `GamePortInputMixer m_gamePortMixer` and `MachineGamePortSink` to `CassoEmuCore/Shell/EmulatorShell.h`; attach the sink after a machine is built and `Attach (nullptr)` before teardown, at the machine build/teardown points used by `MachineManager.cpp` (exclusive lifetime lock, around `:501`)
- [ ] T014 Migrate `UpdateJoystickAxesFromKeys` (`CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp:2294`) and `UpdateJoystickButtonsFromKeys` (`:2379`) to `Submit (GamePortSource::FireKeys, ...)` and axis contributions for `AxisOwner::ArrowKeys`, preserving the X/Z plus Alt OR and the foreground-only reads
- [ ] T015 Migrate `ApplyAppleModifierKeys` (`EmulatorWindowInput.cpp:1802`), `ReleaseGuestKeys` (`:1705`), `PushPaddlePosition` (`:3049`), `PushPaddleButton` (`:3085`) and `StopPaddleCapture` (`:2960`) to `Submit`/`ReleaseSource` with `AppleModifierKeys` and `MousePaddle`; set the axis owner from `SetArrowsJoystick` (`:2489`), `SetPointerMapping` (`:2559`) and `SetInputMappingMode` (`:2440`); after this task grep `CassoEmuCore/` for `SetPaddle (`, `SetButton (`, `SetOpenApple (`, `SetClosedApple (` and confirm the only callers outside device classes are in `MachineGamePortSink.cpp`
- [ ] T016 Build Debug x64 with `scripts/Build.ps1`; run `scripts/RunTests.ps1 -Filter GamePort` and `-Filter InputEvent` and confirm `UnitTest/EmuTests/GamePortTests.cpp` and `InputEventCoalescingTests.cpp` pass unchanged; launch Casso minimized with `--title` (workspace rules), boot a //e, and confirm arrows-to-joystick, Open/Solid-Apple and mouse-to-paddle behave as before
- [ ] T017 Commit: `refactor(shell): game-port mixer as the single paddle and button writer`

### Device seam, decoders, backend, thread (contracts/controller-backend.md)

- [ ] T018 Create `CassoEmuCore/Seams/IControllerBackend.h` with `IControllerBackend` (`Initialize`, `Shutdown`, `EnumerateDevices`, `ReadSample`, `GetWakeSources`) and `IControllerBackendEvents` (`OnDevicesChanged`) per the contract
- [ ] T019 [P] Create `CassoEmuCore/Controllers/DirectInputSampleDecoder.h/.cpp`: static members decoding a `DIJOYSTATE2`-shaped plain struct (define a local POD mirror in the header so tests need no DirectInput headers) into `ControllerSample`: axes from [-32768, 32767] to [-1, 1] for the slots listed as present, POV hundredths of a degree to D-pad bits with centered when `LOWORD == 0xFFFF` and diagonals setting two bits, 128 buttons
- [ ] T020 [P] Create `CassoEmuCore/Controllers/XInputSampleDecoder.h/.cpp`: static members decoding an `XINPUT_GAMEPAD`-shaped POD into `ControllerSample` with the fixed Xbox `ControlId` layout from data-model.md (left stick axis 0/1 with Y inverted so up is negative, right stick axis 3/4, triggers 0/1 from 0-255, A/B/X/Y buttons 0-3, LB/RB 4/5, Back/Start 6/7, LS/RS 8/9, D-pad hat 0); also a static that returns the Xbox control list for `ControllerDeviceInfo::controls`
- [ ] T021 [P] Create `UnitTest/ControllerTests/DirectInputSampleDecoderTests.cpp`: axis extremes and center, POV centered, each cardinal and each diagonal, absent axis slots stay 0, button bits; confirm a test fails with POV decoding stubbed
- [ ] T022 [P] Create `UnitTest/ControllerTests/XInputSampleDecoderTests.cpp`: every button bit to its `ControlId`, thumb extremes including -32768, trigger 0 and 255, D-pad diagonals, Y orientation; confirm a test fails with the bit table altered
- [ ] T023 [P] Create `UnitTest/ControllerTests/FakeControllerBackend.h`: scripted devices with `AddDevice`, `RemoveDevice`, `SetSample`, `FailNextRead (HRESULT)`, `RaiseDevicesChanged`, and recorded `GetWakeSources` answers
- [ ] T024 Create `CassoEmuCore/Seams/Win32ControllerBackend.h/.cpp` implementing the contract: `#pragma comment (lib, ...)` for `xinput.lib`, `dinput8.lib`, `dxguid.lib`, `hid.lib` in the `.cpp` (convention of `CassoEmuCore/Audio/WasapiAudio.cpp`); add `<Xinput.h>`, `<dinput.h>` (with `DIRECTINPUT_VERSION 0x0800`), `<hidsdi.h>`, `<dbt.h>` to `CassoEmuCore/Pch.h`; `Initialize` creates the message-only window, HID notification registration, `DirectInput8Create`, resolves `XInputGetCapabilitiesEx` ordinal 108 via `GetProcAddress` and tolerates its absence; `EnumerateDevices` lists connected XInput slots (model from ordinal 108, else the single `IG_` raw-input HID vendor/product when only one Xbox model is attached, else `xinput:generic`) then `DI8DEVCLASS_GAMECTRL` attached devices skipping `DIPROP_GUIDANDPATH` paths containing `IG_`, with `DIPROP_VIDPID`, serial via `HidD_GetSerialNumberString` else `guidInstance`, `DIPROP_RANGE` set to [-32768, 32767], `SetDataFormat (&c_dfDIJoystick2)`, `SetCooperativeLevel` background non-exclusive, `SetEventNotification`, `EnumObjects` for the present-control list; `ReadSample` uses the decoders, skips decode when `dwPacketNumber` is unchanged, retries `Acquire` + `Poll` once on `DIERR_INPUTLOST`/`DIERR_NOTACQUIRED`, and returns `HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED)` with `connected = false` for a vanished device; `WM_DEVICECHANGE` arrival/removal schedules rescans at +300 ms and +2 s; the R4 receiver fallback per T003's findings; EHM throughout
- [ ] T025 Create `CassoEmuCore/Shell/ControllerInputThread.h/.cpp`: owns a `std::thread` that calls `CoInitializeEx (COINIT_MULTITHREADED)` (tolerating `RPC_E_CHANGED_MODE`), initializes the backend, and loops on `MsgWaitForMultipleObjectsEx` over the backend's wake events with a timeout only when `GetWakeSources` reports a timed poll is needed (period from T003), dispatching the message-only window's messages; each wake calls a `ControllerInputService` tick (T033); clean shutdown by posting `WM_QUIT` and joining; the thread body is a thin loop so the testable logic stays in the service
- [ ] T026 Build with `-Target Rebuild` (Pch.h changed); run `scripts/RunTests.ps1 -Filter Controller`
- [ ] T027 Commit: `feat(input): controller backend over XInput and DirectInput`

**Checkpoint**: Devices enumerate and read through the seam; existing input unchanged.

---

## Phase 3: User Story 1 - Play with a connected controller (Priority: P1) MVP

**Goal**: A selected, connected controller drives PDL0/PDL1 and PB0/PB1 with the default mapping and deadzone.

**Independent Test**: With `FakeControllerBackend` reporting known samples, the recording sink sees center at rest, 0/255 at extremes, proportional values between, and PB0/PB1 from the first two buttons; on hardware, quickstart scenarios 2, 3 and 10.

### Tests for User Story 1

- [ ] T028 [P] [US1] Create `UnitTest/ControllerTests/DeadzoneShaperTests.cpp`: inside the deadzone reads exactly center, the deadzone edge maps to center and full deflection to 0/255, radial when both axes come from one stick and axial otherwise, monotonic between; confirm failure with rescaling stubbed
- [ ] T029 [P] [US1] Create `UnitTest/ControllerTests/MappingEvaluatorTests.cpp` (absolute bindings only in this phase): default mapping on an Xbox control list and on a DirectInput list lacking button 1; empty target reads center/released; two bindings on one axis choose the one furthest from center; digital pair drives 0/255 and both held reads center; analog-to-button thresholds for trigger and each axis direction; confirm failure with OR replaced
- [ ] T030 [P] [US1] Create `UnitTest/ControllerTests/ControllerInputServiceTests.cpp` (US1 subset): selected connected fake controller produces the expected mixer contributions; unselected controllers are ignored (FR-009); inactive app releases the controller source to rest and ignores samples until reactivated (FR-033); a failed read is reported as disconnected, never as a healthy rest sample (FR-015); no sink writes while the sample is unchanged

### Implementation for User Story 1

- [ ] T031 [P] [US1] Create `CassoEmuCore/Controllers/DeadzoneShaper.h/.cpp` per research R8: defaults as named constants (XInput 7849/32767 for Xbox-class, 12% DirectInput), radial and axial shaping with rescale
- [ ] T032 [P] [US1] Create `CassoEmuCore/Controllers/ControlMapping.h/.cpp` with `AxisBinding`, `ButtonBinding`, `ControlMapping` (pdl0, pdl1, pb0, pb1, pb2) and `DefaultMapping::For (model, controls)` per data-model.md; fields for `response` and `maxSpeed` exist but only `Absolute` is evaluated until US5
- [ ] T033 [US1] Create `CassoEmuCore/Controllers/MappingEvaluator.h/.cpp` (sample + mapping + deadzone to `GamePortContribution`) and `CassoEmuCore/Controllers/ControllerInputService.h/.cpp`: holds the backend pointer, attached device list, the current selection, activation state and the mixer pointer; `Tick()` reads the selected device, evaluates, and submits `GamePortSource::Controller`; `SetActive (bool)` releases on false; exposes a thread-safe snapshot (device list, connected state, last sample) for UI readers
- [ ] T034 [US1] Wire into the shell: construct `Win32ControllerBackend`, `ControllerInputService` and `ControllerInputThread` in `CassoEmuCore/Shell/EmulatorShell.h/.cpp` startup and stop them before machine teardown at shutdown; call `SetActive` from `EmulatorShell::OnActivateApp` (`CassoEmuCore/Shell/Window/EmulatorWindow.cpp:1639`), treating the Settings sheet as part of the app
- [ ] T035 [US1] Temporary selection for this phase only: the service selects the first enumerated controller when none is selected, so US1 is testable before US2; T041 replaces this with the selection policy
- [ ] T036 [US1] Build; run `scripts/RunTests.ps1 -Filter Controller`; run quickstart sections 3 and 4 scenarios 2, 3 and 10 on hardware (launch with `--title`), recording results
- [ ] T037 [US1] Commit: `feat(input): physical controller drives the game port`

**Checkpoint**: MVP. A plugged-in controller plays joystick games.

---

## Phase 4: User Story 2 - Choose the controller and keep the choice (Priority: P2)

**Goal**: Automatic selection per FR-032, manual selection from the Machine menu and toolbar, persisted per machine.

**Independent Test**: With two fake controllers, selecting each routes only its input; saved prefs restore the selection; a connect with none selected selects and notifies; menu and toolbar rows reflect attached controllers and the selection.

### Tests for User Story 2

- [ ] T038 [P] [US2] Create `UnitTest/ControllerTests/ControllerSelectionPolicyTests.cpp` covering every row of the policy table in data-model.md: connect with none selected (including present at start and machine switch) selects first in enumeration order and requests arrows/paddle off and a notice; connect with a selection changes nothing; user choosing arrows or paddle clears the selection; Xbox model selection matches any unit and lowest slot wins; machine without a game port is inert
- [ ] T039 [P] [US2] Extend `UnitTest/UiTests/MachineInputPrefsTests.cpp`: `controller` and `controllerProfile` keys round trip; absent keys mean none and Default; existing `arrowsToJoystick`/`pointerMapping` behavior unchanged
- [ ] T040 [P] [US2] Create `UnitTest/ControllerTests/ControllerCommandsTests.cpp` (controller rows): one row per attached controller plus a selected-but-disconnected one; checked row equals the selection; rebuilding rows while an old pointer is held keeps the old command valid until the next rebuild completes (stable storage)

### Implementation for User Story 2

- [ ] T041 [US2] Create `CassoEmuCore/Controllers/ControllerSelectionPolicy.h/.cpp` per data-model.md and replace T035's temporary rule in `ControllerInputService`
- [ ] T042 [US2] Extend `CassoEmuCore/Config/MachineInputPrefs.h/.cpp` with `controller` and `controllerProfile` keys per `contracts/prefs-schema.md`; extend `AdoptInputModeForMachine` and `PersistInputModeForMachine` in `CassoEmuCore/Shell/EmulatorShellPrefs.cpp` (`:180`, `:206`); XInput selections persist the model token, DirectInput the unit token
- [ ] T043 [US2] Make selecting a controller turn off arrows-to-joystick and mouse-to-paddle, and selecting either of those clear the controller selection (FR-008), in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` setters; set `AxisOwner::Controller` while a connected controller is selected
- [ ] T044 [US2] Generalize the screenshot notice (`ShowCaptureNotice`, `CassoEmuCore/Shell/EmulatorShellPresent.cpp:1246`; `SyncCaptureNotice` `:1291`) into a shell transient notice used by both captures and controller selection, keeping the screenshot text and timing unchanged; show "Controller selected: <description>" on automatic selection (FR-032)
- [ ] T045 [US2] Create `CassoEmuCore/Ui/Chrome/ControllerCommands.h/.cpp` per research R11: owns controller rows as `std::vector<std::unique_ptr<DxuiCommand>>` (`Dxui/Core/DxuiCommand.h`) with `isChecked` reading the selection and `dispatch` calling a selection sink; `BuildControllerSubmenu()` returns `std::vector<DxuiPopupMenuItem>` (`Dxui/Widgets/DxuiPopupMenu.h`)
- [ ] T046 [US2] Extend `EmulatorMenuEntry` and `s_kMenuEntries` in `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp` with a submenu provider marker and expand it in `BuildMenuItems` to `DxuiPopupMenuItem::ForSubmenu`; add a `Controller` submenu row to the Machine menu after the input mapping items
- [ ] T047 [US2] In `CassoEmuCore/Ui/Chrome/MainMenu.cpp` add a rebuild that calls `DxuiMenuBar::SetItems` again, deferred while any menu is open (check `IsMenuOpen`) and applied when menus close; trigger it from the shell when the controller list or selection changes
- [ ] T048 [US2] In `CassoEmuCore/Ui/Chrome/InputClusterEntry.h/.cpp`: add a controller segment to the expanded cluster (gamepad glyph from `Dxui/Core/UnicodeSymbols.h` or a painted monoline glyph matching `PaintJoystickMono`, with the existing LED pattern) whose click selects the controller through the sink; append a separator and the `Controller` submenu to `GetPickerItems`; extend `SyncSelectorState` (`CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp:2690`) to push controller state and resend `SetDropDownItems (kIdInput, ...)` when rows change
- [ ] T049 [US2] Build; run `scripts/RunTests.ps1 -Filter Controller`, `-Filter MachineInputPrefs`, `-Filter Chrome`, `-Filter MenuBar`; run quickstart scenarios 1 and 8 (selection part) and scenario 11 (controller rows) on hardware
- [ ] T050 [US2] Commit: `feat(input): controller selection, persistence, menu and toolbar`

**Checkpoint**: US1 and US2 work together; selection is automatic, manual and persistent.

---

## Phase 5: User Story 3 - Unplug and replug mid-session (Priority: P2)

**Goal**: Disconnect releases the port and hands the joystick to the arrow keys; reconnect resumes; status is visible.

**Independent Test**: A fake controller held at full deflection with PB0 is removed; the sink sees center and released, then arrow contributions own the axes; re-adding resumes the controller and held arrows stop driving.

### Tests for User Story 3

- [ ] T051 [P] [US3] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: removal releases within one tick and sets `AxisOwner::ArrowKeys` without changing the saved selection; reconnect restores `AxisOwner::Controller` and arrows held at that moment stop driving; `OnDevicesChanged` at +300 ms and +2 s is idempotent; the device list update is visible in the snapshot
- [ ] T052 [P] [US3] Extend `UnitTest/UiTests/ChromeToolbarPartsTests.cpp`: `InputClusterEntry` tooltip and LED state for connected, disconnected and arrow-keys-standing-in

### Implementation for User Story 3

- [ ] T053 [US3] Implement the disconnect fallback and reconnect in `CassoEmuCore/Controllers/ControllerInputService.cpp` and `ControllerSelectionPolicy.cpp` (FR-008a, FR-010), including reads failing with `ERROR_DEVICE_NOT_CONNECTED` before the removal notification arrives
- [ ] T054 [US3] Arrow-key fallback applies even when arrows-to-joystick is off (spec edge case): in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`, run the arrow-key joystick update while the fallback is active regardless of `m_arrowsJoystick`
- [ ] T055 [US3] Add `SetControllerState` to `CassoEmuCore/Ui/Chrome/InputClusterEntry.h/.cpp` driving the controller segment LED and `GetTooltipAt` text (connected / not connected / arrow keys standing in) per FR-008a and FR-013; show a disconnected selected controller as an unchecked-but-present row in the `Controller` submenu with "(not connected)" via `labelText`
- [ ] T056 [US3] Build; run `-Filter Controller` and `-Filter Chrome`; run quickstart scenarios 4 and 5 and scenario 11's plug-in-while-menu-open on hardware
- [ ] T057 [US3] Commit: `feat(input): controller hot-plug with arrow-key fallback`

**Checkpoint**: Disconnects never leave the port stuck.

---

## Phase 6: User Story 4 - A worn or off-center stick still works (Priority: P3)

**Goal**: Automatic calibration for DirectInput units, a user Calibrate action, persisted per unit; none for Xbox-class.

**Independent Test**: A fake DirectInput unit with an offset rest and short travel reads center and 0/255 after automatic learning; a user calibration replaces it and survives a prefs round trip; an Xbox-class fake gets no calibration.

### Tests for User Story 4

- [ ] T058 [P] [US4] Create `UnitTest/ControllerTests/CalibrationTests.cpp`: every transition in the data-model state diagram; center captured at connect; limits widen only outward; user calibration skips connect-time capture (FR-007a); invariant `minimum < center < maximum` rejected on load; Xbox-class units bypass calibration (FR-018a)
- [ ] T059 [P] [US4] Create `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp` (calibration part): calibration JSON per `contracts/prefs-schema.md` round trips through `UnitTest/UiTests/InMemoryFileSystem.h`; an invalid entry is dropped, reported, and the unit falls back to automatic

### Implementation for User Story 4

- [ ] T060 [P] [US4] Create `CassoEmuCore/Controllers/ControllerCalibration.h/.cpp` per data-model.md, applied before `DeadzoneShaper` in `MappingEvaluator`
- [ ] T061 [US4] Create `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp` holding calibration by unit token with `ToJson`/`FromJson` per the schema (profiles added in US6), and add the `controllers` section to `CassoEmuCore/Config/GlobalUserPrefs.h/.cpp` following the `monitorTilt` pattern (field, `s_kKnownTopLevel` at `:50-86`, `ToJson` `:1093`, `FromJson` `:1270`), preserving unknown keys inside `controllers`
- [ ] T062 [US4] Report unreadable calibration entries once through the transient notice (T044) on load
- [ ] T063 [US4] Build; run `-Filter Calibration`, `-Filter ControllerProfileStore`, `-Filter GlobalUserPrefs`
- [ ] T064 [US4] Commit: `feat(input): automatic and saved controller calibration`

**Checkpoint**: Worn sticks read correctly without UI. The Calibrate action's UI lands with the Controllers page (T071).

---

## Phase 7: User Story 5 - Remap controls for a particular controller (Priority: P3)

**Goal**: A Controllers page in the Settings sheet with live readings, press-to-assign mapping (including rate response and PB2), deadzone and Calibrate, under Apply/Cancel.

**Independent Test**: `ControllersPageState` with a fake controller: capture assigns the pressed control and ignores pre-held ones; edits apply only on Apply and revert on Cancel; live readings reflect the edited mapping; rate bindings hold on release; PB2 unavailable on the //c.

### Tests for User Story 5

- [ ] T065 [P] [US5] Create `UnitTest/ControllerTests/ControlCaptureTests.cpp`: activation assigns the first control crossing its threshold after capture started; a control held or deflected when capture started is ignored until released; cancel assigns nothing
- [ ] T066 [P] [US5] Extend `UnitTest/ControllerTests/MappingEvaluatorTests.cpp` with rate response (FR-021a): accumulates by `deflection * maxSpeed * elapsed`, clamps to 0-255, holds on release, resets on selection/profile/machine change; inverted axes; PB2 evaluated on ][+ and //e, ignored on //c
- [ ] T067 [P] [US5] Create `UnitTest/ControllerTests/ControllersPageStateTests.cpp`: pending edits do not reach the service before Apply; Cancel restores the baseline including a captured calibration; controls assigned to two targets are reported (FR-025); adding an assignment never removes another; controller removed while open keeps edits and shows disconnected; Reset to Defaults
- [ ] T068 [P] [US5] Extend `UnitTest/ControllerTests/GamePortInputMixerTests.cpp` and add sink tests with a fake `MachineRefs` if feasible: PB2 routes to `AppleGamePort` index 2 on ][+, ORs with Shift on //e, dropped on //c

### Implementation for User Story 5

- [ ] T069 [P] [US5] Create `CassoEmuCore/Controllers/ControlCapture.h/.cpp` per T065
- [ ] T070 [US5] Implement rate response and PB2 evaluation in `CassoEmuCore/Controllers/MappingEvaluator.cpp` (accumulator state owned by the evaluator, elapsed time from the service tick using a clock injected for tests); add `ControlLabels` (`CassoEmuCore/Controllers/ControlLabels.h/.cpp`) giving Xbox and DirectInput display labels
- [ ] T071 [US5] Create `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp`: pure page model with controller choice, pending mapping edits per target (add binding by capture or list, remove binding, invert, response and max speed, thresholds), deadzone, Calibrate flow (center then full travel, then pending user calibration), "use automatic calibration", Reset to Defaults, and live readings computed from the service snapshot under the edited mapping
- [ ] T072 [US5] Create `CassoEmuCore/Ui/Settings/ControllersPage.h/.cpp` as a `DxuiPropertyPage` following `CassoEmuCore/Ui/Settings/PrintingPage.h`: controller list, per-target binding lists with add/remove and capture buttons, invert/response/speed/threshold controls, deadzone slider, Calibrate and Use Automatic buttons (hidden for Xbox-class), live readout of controls and PDL0/PDL1/PB0-PB2, PB2 shown unavailable on the //c; register it in `CassoEmuCore/Ui/Settings/SettingsSheet.cpp` `OnBuildPages`
- [ ] T073 [US5] Extend `CassoEmuCore/Ui/Settings/SettingsApplyController.h/.cpp` with the controllers baseline, dirty comparison, commit and revert (pattern of the printer fields at `:81-85`, `:323-327`, `:368`, `:474`); commit writes to `ControllerProfileStore` and saves global prefs
- [ ] T074 [US5] Add an optional page argument to `OpenSettings` (`CassoEmuCore/Shell/EmulatorShellDialogs.cpp:540`) threaded to `DxuiPropertySheet::SetActivePage`; add a static `Controller Settings...` command with a new id in `CassoEmuCore/resource.h` in a new range, an `OnCommand` branch in `CassoEmuCore/Shell/WindowCommandManager.cpp`, and its row in the Machine menu (`EmulatorCommands.cpp` `s_kMenuEntries`) and the input cluster picker
- [ ] T075 [US5] Build; run `-Filter Controller`, `-Filter Settings`; run quickstart scenarios 7, 9, 10a (with a manually built rate mapping) and 10b on hardware
- [ ] T076 [US5] Commit: `feat(input): Controllers settings page with remapping and calibration`

**Checkpoint**: Any controller can be remapped and calibrated.

---

## Phase 8: User Story 6 - Keep a mapping per game and switch between them (Priority: P3)

**Goal**: Named profiles per model with Default, create/copy/rename/delete/reset, the Paddles template, active profile per machine, switchable from the menu and toolbar.

**Independent Test**: Two fake profiles with different PB0 bindings route only the active one; prefs round trip keeps both and the active choice; deleting the active profile falls back to Default; switching releases controls no longer bound.

### Tests for User Story 6

- [ ] T077 [P] [US6] Extend `UnitTest/ControllerTests/ControllerProfileStoreTests.cpp`: Default created on first use and never deletable or renamable; names unique case-insensitively, trimmed, 1-40 characters; create from default, from a copy, and from the Paddles template; reset; `FindProfile (model, name)`; every rejection rule in `contracts/prefs-schema.md` including duplicate names and a missing Default
- [ ] T078 [P] [US6] Extend `UnitTest/ControllerTests/ControllerInputServiceTests.cpp`: switching profiles releases buttons and centers axes no longer driven (FR-030) and resets rate accumulators; a remembered missing profile uses Default without recreating it (FR-029)
- [ ] T079 [P] [US6] Extend `UnitTest/ControllerTests/ControllerCommandsTests.cpp` with profile rows: one per profile of the selected model, checked equals active, rows rebuilt on create/rename/delete

### Implementation for User Story 6

- [ ] T080 [US6] Add profiles and per-model deadzone to `CassoEmuCore/Controllers/ControllerProfileStore.h/.cpp` with JSON per the schema, and `DefaultMapping::MakePaddles` in `CassoEmuCore/Controllers/ControlMapping.cpp`
- [ ] T081 [US6] Active profile per machine: read and write `controllerProfile` (T042), apply in `ControllerInputService`, release on switch (FR-030)
- [ ] T082 [US6] Add profile management to `CassoEmuCore/Ui/Settings/ControllersPageState.h/.cpp` and `ControllersPage.h/.cpp`: profile list, New (Default, copy of current, Paddles), Rename, Delete, Reset; name validation messages in the fixed error format from `.github/copilot-instructions.md`; keep-or-discard prompt when switching profiles with unapplied edits; edits apply to the profile they were made on
- [ ] T083 [US6] Add profile rows to `CassoEmuCore/Ui/Chrome/ControllerCommands.h/.cpp`, a `Controller Profile` submenu in the Machine menu (`EmulatorCommands.cpp`) and in `InputClusterEntry::GetPickerItems`; rebuild on profile changes through T047's deferred rebuild
- [ ] T084 [US6] Build; run `-Filter Controller`, `-Filter Chrome`; run quickstart scenarios 8 and 10a (Paddles template) and scenario 11 (profile rows) on hardware
- [ ] T085 [US6] Commit: `feat(input): named controller profiles`

**Checkpoint**: All six user stories work.

---

## Phase 9: Polish and Pre-Merge Gates

- [ ] T086 [P] Add a `[Unreleased]` entry to `CHANGELOG.md` with `GH #97:` first, one or two lines of user-visible effect (show the text to the user and wait for approval before pushing)
- [ ] T087 [P] Update `README.md` headline features if controller support belongs there (show the text to the user and wait for approval)
- [ ] T088 Walk `specs/034-game-controllers/quickstart.md` sections 3-4 end to end on Release x64 and record outcomes, including any scenario that could not run and why, in `specs/034-game-controllers/validation.md`
- [ ] T089 Merge `origin/master` into the branch; rebuild with `-Target Rebuild`; resolve renames the compiler surfaces
- [ ] T090 `git add -A`, then `scripts/CheckStyle.ps1 -Mode Tree`; fix every hit
- [ ] T091 `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for Debug and Release x64 and ARM64; zero warnings
- [ ] T092 Full suite `scripts/RunTests.ps1` in Debug and Release; confirm `UnitTest.dll` is newer than the build before trusting results
- [ ] T093 Present every commit subject, the CHANGELOG entry and any README change for approval; push only after approval; the master merge is `--no-ff` with subject `merge(input): 034 physical game controllers (...)`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Phase 1** (hardware check): none. T003's poll period and receiver findings feed T024 and T025.
- **Phase 2** (foundational): after Phase 1. The mixer (T009-T017) and the backend (T018-T027) are independent of each other and can proceed in parallel.
- **US1** (Phase 3): after Phase 2.
- **US2** (Phase 4): after US1 (replaces the temporary selection).
- **US3** (Phase 5): after US2 (fallback depends on selection state and the cluster segment).
- **US4** (Phase 6): after US1; independent of US2/US3.
- **US5** (Phase 7): after US4 (Calibrate UI) and US2 (Controller Settings entry sits beside the controller rows).
- **US6** (Phase 8): after US5 (profile management lives on the page).
- **Polish** (Phase 9): after the stories to ship.

### Within Each Story

- Tests first; confirm they fail with the implementation stubbed.
- Pure logic in `CassoEmuCore/Controllers/` before shell wiring, shell wiring before chrome.
- Commit at each phase end (constitution Commit Discipline).

### Parallel Opportunities

- T008, T010, T019-T023 (different new files).
- Test tasks marked [P] within each story.
- US4 can run beside US2/US3 once US1 is done.

---

## Parallel Example: Phase 2

```text
Task: "Create CassoEmuCore/Controllers/DirectInputSampleDecoder.h/.cpp (T019)"
Task: "Create CassoEmuCore/Controllers/XInputSampleDecoder.h/.cpp (T020)"
Task: "Create UnitTest/ControllerTests/DirectInputSampleDecoderTests.cpp (T021)"
Task: "Create UnitTest/ControllerTests/XInputSampleDecoderTests.cpp (T022)"
Task: "Create UnitTest/ControllerTests/FakeControllerBackend.h (T023)"
```

## Parallel Example: User Story 1

```text
Task: "DeadzoneShaperTests.cpp (T028)"
Task: "MappingEvaluatorTests.cpp (T029)"
Task: "DeadzoneShaper.h/.cpp (T031)"
Task: "ControlMapping.h/.cpp (T032)"
```

---

## Implementation Strategy

### MVP (User Story 1)

1. Phase 1 hardware check.
2. Phase 2: mixer migration, then backend and thread.
3. Phase 3: a plugged-in controller plays.
4. Stop and validate on hardware (quickstart scenarios 2, 3, 10).

### Incremental Delivery

1. US2: automatic and manual selection, persistence, menu and toolbar rows.
2. US3: hot-plug fallback and status.
3. US4: calibration.
4. US5: Controllers page.
5. US6: profiles.

Each phase ends with a commit and leaves the build and suite green, so the branch could merge to master after any story if the owner wants the feature earlier.

---

## Notes

- Never run `Get-Process Casso | Stop-Process`; stop only the PID you launched.
- Every agent launch of Casso passes `--title <worktree name>` and runs minimized unless the user asked for it.
- `.specify/feature.json` stays out of commits.
- No Claude attribution in commit messages (CheckStyle CS0008).
