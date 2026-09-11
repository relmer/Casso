# Research: Physical Game Controllers

**Feature**: `034-game-controllers` | **Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

Each entry gives the decision, why, and what was rejected. Items marked **UNVERIFIED** rest on community reports rather than a Microsoft primary source and carry a validation task.

## R1. Device APIs: XInput 1.4 for Xbox-class, DirectInput 8 for the rest

- **Decision**: `xinput.lib` (imports `xinput1_4.dll`, in-box since Windows 8) for Xbox-class controllers; DirectInput 8 (`dinput8.lib`, `dxguid.lib`) for every other controller. Both libraries exist for x64 and ARM64 in SDK 10.0.26100.
- **Rationale**: DirectInput reports an Xbox controller's two triggers on one combined axis by design, and "In order to test the trigger values separately, you must use XInput" ([XInput and DirectInput](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput)). XInput sees no non-Xbox device. Settled in the spec's clarifications.
- **Alternatives**: `xinput9_1_0` (reports every device as a gamepad subtype, lacks `XInputEnable`); Windows.Gaming.Input and GameInput (rejected in the spec clarification; GameInput also needs an installed runtime).

## R2. Focus (FR-033)

- **XInput**: `XInputEnable` is documented as deprecated because "game controller input is automatically enabled/disabled by the system based on the application window focus" on Windows 10+ ([XInputEnable](https://learn.microsoft.com/en-us/windows/win32/api/xinput/nf-xinput-xinputenable)). A 2025 Microsoft Q&A thread reports input continuing while unfocused; either way, the documented behavior cannot be relied on for background input.
- **Decision**: controller input applies only while Casso is active, and Xbox-class controllers always go through XInput (owner decision: XInput over background input). Casso decides activation itself from `WM_ACTIVATEAPP` rather than trusting either API's focus behavior, so both device kinds stop and resume at the same moment: on deactivation the controller source is released to rest in the mixer and samples are ignored until reactivation.
- **DirectInput**: `SetCooperativeLevel (hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)` on the controller thread's message-only window. Background acquisition is used only so acquisition does not depend on which Casso window is active; the activation gate above decides whether samples reach the game port ([SetCooperativeLevel](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417921(v=vs.85))).
- **Remaining hardware check**: whether XInput keeps delivering input while the **Settings sheet** is the active window. The documentation says focus, and the sheet is a separate top-level window of the same process. The Controllers page's live readings and press-to-assign need it. If XInput stops delivering there, the page shows the Xbox controller as paused while the sheet is active, and the check result is recorded here.
- **Rejected**: background input through DirectInput for Xbox controllers (combined triggers while unfocused); raw HID reports (per-model report parsing).

## R3. Threading: one dedicated controller thread

- **Decision**: a `ControllerInputThread` owns a message-only window, the device notifications, the DirectInput interface and devices, and all XInput and DirectInput polling. It publishes each processed sample to the game port. The sample rate is set by R13.
- **Rationale**: the UI thread's per-frame hook (`TryPresentUiFrame`, `CassoEmuCore/Shell/EmulatorShellPresent.cpp:552`) does not run while the machine is idle (`WaitForFrameOrMessage`), and pauses in modal loops. DirectInput acquisition, hot-plug notifications and XInput slot backoff also want one owner with its own message window. Microsoft documents no thread affinity for DirectInput polling; creating, acquiring and polling on one thread with its own window is the conservative arrangement.
- **COM**: `DirectInput8Create` needs no `CoInitializeEx`; the thread calls `CoInitializeEx (COINIT_MULTITHREADED)` anyway, tolerating `RPC_E_CHANGED_MODE`, since DirectInput internals are COM objects.
- **Alternatives**: `WM_TIMER` on the UI window (paused by modal loops, coarse); polling from the CPU thread (couples input to emulation speed and pause state, contradicting the spec's pause edge case).

## R4. Hot-plug detection

- **Decision**: `RegisterDeviceNotification (GUID_DEVINTERFACE_HID, DEVICE_NOTIFY_WINDOW_HANDLE)` on the thread's message-only window. On `DBT_DEVICEARRIVAL` or `DBT_DEVICEREMOVECOMPLETE`, rescan at +300 ms and again at +2 s, since not every API reflects a change immediately (SDL2 does the same). Separately, an XInput slot that reports `ERROR_DEVICE_NOT_CONNECTED` is rechecked only once per second, per Microsoft's guidance not to poll empty slots every frame ([Getting Started with XInput](https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput)).
- **Rationale**: `IDirectInput8::EnumDevices` is slow enough that polling it is wasteful; notifications cover USB, Bluetooth and the Xbox wireless adapter, since all present HID interfaces.
- **Alternatives**: `CM_Register_Notification` (callback on a system thread, reports no already-present devices; no advantage once the thread has a window anyway).

## R5. Keeping Xbox controllers out of the DirectInput list

- **Decision**: during DirectInput enumeration, read `DIPROP_GUIDANDPATH` for each device and skip any whose path contains `IG_`.
- **Rationale**: current SDL2 uses this; it needs no WMI. Bluetooth Xbox controllers also carry `IG_` in their paths.
- **Alternatives**: Microsoft's WMI sample (slow, COM security setup); `GetRawInputDeviceList` plus VID/PID matching against `guidProduct.Data1` (works, but two lookups where one suffices).

## R6. Recognizing controllers

- **DirectInput model**: `DIPROP_VIDPID` (`LOWORD` vendor, `HIWORD` product). Model key `vid:pid`.
- **DirectInput unit**: the HID serial number from `HidD_GetSerialNumberString` on the `DIPROP_GUIDANDPATH` path when it is nonempty; otherwise `DIDEVICEINSTANCE::guidInstance`, documented as reusable on the same computer ([DIDEVICEINSTANCE](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416610(v=vs.85))). Whether `guidInstance` survives a port change is **UNVERIFIED**; the spec's model-level fallback already covers a unit that is not recognized.
- **XInput model**: `XInputGetCapabilitiesEx`, ordinal 108 of `xinput1_4.dll`, returns per-slot `VendorId`/`ProductId`. It is **not documented** on Microsoft Learn. Decision: resolve it with `GetProcAddress (MAKEINTRESOURCEA (108))` at startup; if it is missing or fails, fall back to the vendor and product of the `IG_` HID device from `GetRawInputDeviceList` when exactly one Xbox model is attached, and otherwise to a single generic "Xbox controller" model key. Every Xbox-class controller is usable in all three cases; only the per-model profile separation degrades.
- **XInput unit**: none (FR-018a).

## R7. Normalized sample and control identity

- **Decision**: both backends deliver one normalized sample: axes as `float` in [-1, 1] (DirectInput ranges set to [-32768, 32767] with `DIPROP_RANGE`, XInput thumbs native), triggers in [0, 1], buttons as a bit set, and each POV hat decoded into four D-pad directions (hundredths of a degree, centered when `LOWORD (pov) == 0xFFFF`; diagonals set two directions). Controls are identified by a stable `ControlId { kind, index }` so a mapping written against one sample layout reads the same control next launch.
- **DirectInput data format**: `c_dfDIJoystick2` (`DIJOYSTATE2`); its fixed slots (X, Y, Z, Rx, Ry, Rz, two sliders, four POVs, 128 buttons) cover every device class in scope. `EnumObjects` records which slots the device actually reports, so the Controllers page lists only real controls (FR-020).
- **Xbox control table**: left stick X/Y, right stick X/Y, LT, RT, D-pad (4), A, B, X, Y, LB, RB, Back, Start, LS, RS. Guide is excluded (spec).
- **Rationale**: every rule in the spec (deadzone, calibration, mapping, capture) is then a pure function over this sample, testable with synthetic data, and the Win32 code only decodes.

## R8. Deadzone

- **Decision**: default deadzone for Xbox-class sticks is XInput's `XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE` (7849 of 32767, about 24%); for DirectInput devices, 12%. Deadzone is **radial** when PDL0 and PDL1 are driven by the X and Y axes of the same stick, as Microsoft's sample applies it, and **axial** otherwise (a single axis, or axes from different sticks). Output is rescaled so the edge of the deadzone maps to center and full deflection still reaches 0/255 (FR-004, FR-006). Analog-to-button threshold defaults to `XINPUT_GAMEPAD_TRIGGER_THRESHOLD` (30 of 255) for triggers and 50% deflection for axes.
- **Alternatives**: axial everywhere (square-ish rest region causes cardinal-direction sticking on diagonals).

## R9. Combining input sources on the game port

- **Finding**: today the writers overwrite each other. `UpdateJoystickButtonsFromKeys` ORs X/Z with Alt, but `ApplyAppleModifierKeys`, `PushPaddleButton`, `ReleaseGuestKeys` and `StopPaddleCapture` each write the buttons directly (`CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp:1802`, `:3085`, `:1705`, `:2960`). A fourth writer on another thread would fight all of them.
- **Decision**: a `GamePortInputMixer` owns the final PDL0/PDL1/PB0/PB1 values. Each source (keyboard fire keys, Apple modifier keys, mouse paddle, controller) submits its own contribution; the mixer ORs buttons (FR-014), picks the axis owner by input mode (arrows, paddle, controller, or arrow fallback per FR-008a), and writes the machine only when a final value changes. Existing direct writes are rerouted through it. The mixer is a pure class guarded by a mutex; the machine write goes through an `IGamePortSink` so tests observe it.
- **Rationale**: FR-014 and the "releasing one does not release a button the other holds" edge case cannot be met by last-writer-wins.

## R10. Persistence

- **Decision**:
  - Global (`GlobalUserPrefs`, new top-level `controllers` section, following the `monitorTilt` pattern: field, `s_kKnownTopLevel`, `ToJson`, `FromJson`): per model the deadzone and the profile list; per DirectInput unit the calibration. Schema in [contracts/prefs-schema.md](contracts/prefs-schema.md).
  - Per machine (`MachineInputPrefs`, `$cassoUiPrefs`): `controller` (the selection key) and `controllerProfile` (the active profile name). The existing `arrowsToJoystick` and `pointerMapping` keys are unchanged, so older builds keep reading their own keys.
- **Rationale**: matches the spec's persistence assumption and reuses the round-trip and in-memory file-system test infrastructure (`UnitTest/UiTests/InMemoryFileSystem.h`).

## R11. The 032 dependency reaches the Machine menu too

- **Finding**: 032 replaces `MainMenu`'s `s_kEntries` table and `CommandToolbar`'s entry table with one `DxuiCommand` table in `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp`, and today's menu has no dynamic items or submenus at all (`CassoEmuCore/Ui/Chrome/MainMenu.cpp:392`). Controller and profile menu entries built now would be written twice.
- **Decision**: the Machine menu entries and the toolbar pickers are both deferred until 032 is on master (FR-031's spirit extends to the menu). Before that, the feature is fully usable through automatic selection (FR-032) and the Controllers page, which gains a per-machine "use this controller" choice and an active-profile choice. After 032: controller and profile rows become `DxuiCommand`s held in a member vector rebuilt on hot-plug or profile edits (items point at commands, so the storage must stay alive while a dropdown is open), exposed through `DxuiDropdownItem::ForSubmenu` in the Machine menu and through the input cluster's picker (`InputClusterEntry`).
- **Spec impact**: FR-008 and FR-028 say "input selector (Machine menu and toolbar input control)". This plan delivers the menu half with the toolbar half, after 032. Flagged for the owner in the plan summary.

## R13. Sample rate

- **No API recommends a rate, and neither reports a device's report interval.** The interval lives in the USB endpoint descriptor, which neither API exposes and which Bluetooth devices do not have.
- **DirectInput needs no rate: it can wake on the device's own reports.** `IDirectInputDevice8::SetEventNotification` sets an event handle whenever the device's state changes (DirectInput 8 reference, `IDirectInputDevice8::SetEventNotification`; link to be confirmed during the hardware check). The controller thread waits on those handles and reads when one fires. The exception is a device whose `DIDEVCAPS` has `DIDC_POLLEDDEVICE`, which reports nothing until `Poll` is called; those are polled on the XInput cadence below. `DIPROP_BUFFERSIZE` with `GetDeviceData` additionally delivers every button transition with a sequence number, so a press shorter than a read interval is never lost.
- **XInput has no event, only a change counter.** `XINPUT_STATE::dwPacketNumber` changes when, and only when, the controller's state changed ([XINPUT_STATE](https://learn.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_state)), so a read that returns the same packet number is skipped without further work, but the thread still has to call `XInputGetState` on a cadence.
- **Decision**: the thread waits with `MsgWaitForMultipleObjectsEx` on the DirectInput event handles and its message queue, with a timeout equal to the XInput and polled-device period. That period is set from measurement: the hardware check counts `dwPacketNumber` changes per second while the stick moves continuously, on the Xbox controller wired and wireless, and the period becomes the fastest measured interval rounded down to a whole millisecond. With no XInput controller connected and no polled DirectInput device, the timeout is the 1 s empty-slot recheck (R4), so an idle thread costs nothing (SC-007). Until the check runs, the provisional period is 8 ms, which is **UNMEASURED**.
- **Rejected**: one fixed rate for every device (wakes for no new data on some, lags others); sampling once per emulated frame (ties input to emulation speed and pause).

## R14. Paddle games: rate-mode axis bindings

- **Finding**: an absolute binding makes a self-centering stick a poor paddle. Releasing the stick sends the paddle back to the middle, and the paddle's whole travel maps onto the stick's short throw. A real paddle is a knob that stays where it is left, which is why mouse-to-paddle accumulates motion.
- **Decision** (owner): an analog axis binding has a `Rate` response in addition to `Absolute`. In rate mode, deflection beyond the deadzone moves the paddle value at a speed proportional to deflection, up to a per-binding maximum in paddle units per second, and the value holds when the stick is released. The accumulated value is integrated on the controller thread from the elapsed time between samples, clamped to [0, 255], and reset to center when the selection, profile, or machine changes. A built-in "Paddles" template, offered when creating a profile, binds left stick X in rate mode to PDL0, right stick X in rate mode to PDL1, and A and B to PB0 and PB1.
- **Rejected**: absolute only (unusable for paddle games on gamepads); a separate controller paddle input mode (a second place to configure the same thing profiles already hold).

## R15. PB2

- **Finding**: the machines already model the line, with different meanings per machine.
  - ][ and ][+: `AppleGamePort` handles PB2 at `$C063` (`CassoEmuCore/Machines/Apple2/Common/AppleGamePort.h:18`, `SetButton` index 2).
  - //e: `$C063` bit 7 reads the Shift key, the shift-key modification (`CassoEmuCore/Machines/Apple2/Apple2e/Apple2eKeyboard.cpp:92`). On real hardware the game-port PB2 pin and that modification share the line.
  - //c: `$C063` is the built-in mouse button, active low, and the //c's game port has no PB2 pin.
- **Decision**: PB2 is a mapping target on the ][, ][+ and //e. On the //e it ORs with Shift through the mixer, matching the shared line, so software reading Shift through `$C063` sees a controller PB2 press as Shift, as it would on the hardware. On the //c the PB2 target is shown as unavailable and bindings to it are ignored, so a controller cannot press the mouse button. PB2 has no default binding.
- **Rationale for the earlier exclusion being wrong**: it was scoped out as rarely used without checking the emulator, and the line already exists on two of the three machine families.

## R12. Transient notice and opening the Settings sheet to a page

- **Notice**: the only generic auto-hiding overlay is the screenshot notice (`ShowCaptureNotice`, a `DxuiInfoBanner` expiring after 4 s, `CassoEmuCore/Shell/EmulatorShellPresent.cpp:1246`). Decision: generalize it into a shell notice used by both, rather than adding a second banner.
- **Settings page**: `OpenSettings()` takes no page (`CassoEmuCore/Shell/EmulatorShellDialogs.cpp:540`), but `DxuiPropertySheet::SetActivePage (int)` exists. Decision: add an optional page argument threaded through to `SetActivePage`.
