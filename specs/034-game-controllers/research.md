# Research: Physical Game Controllers

**Feature**: `034-game-controllers` | **Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

Each entry gives the decision, why, and what was rejected. Items marked **UNVERIFIED** rest on community reports rather than a Microsoft primary source and carry a validation task.

## R1. Device APIs: XInput 1.4 for Xbox-class, DirectInput 8 for the rest

- **Decision**: `xinput.lib` (imports `xinput1_4.dll`, in-box since Windows 8) for Xbox-class controllers; DirectInput 8 (`dinput8.lib`, `dxguid.lib`) for every other controller. Both libraries exist for x64 and ARM64 in SDK 10.0.26100.
- **Rationale**: DirectInput reports an Xbox controller's two triggers on one combined axis by design, and "In order to test the trigger values separately, you must use XInput" ([XInput and DirectInput](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput)). XInput sees no non-Xbox device. Settled in the spec's clarifications.
- **Alternatives**: `xinput9_1_0` (reports every device as a gamepad subtype, lacks `XInputEnable`); Windows.Gaming.Input and GameInput (rejected in the spec clarification; GameInput also needs an installed runtime).

## R2. Background input (FR-033) -- the gating risk

- **DirectInput**: `SetCooperativeLevel (hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)` acquires the device while the window is inactive. The HWND must be a top-level window owned by the process; a message-only window owned by the input thread meets that in practice, as SDL2 does ([SetCooperativeLevel](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417921(v=vs.85))).
- **XInput**: `XInputEnable` is documented as deprecated because "game controller input is automatically enabled/disabled by the system based on the application window focus" on Windows 10+ ([XInputEnable](https://learn.microsoft.com/en-us/windows/win32/api/xinput/nf-xinput-xinputenable)). A 2025 Microsoft Q&A thread reports `XInputGetState` still returning presses while unfocused, with only rumble stopping. **UNVERIFIED.**
- **Decision**: treat background XInput as unproven. The first implementation task is a hardware check on this machine: an Xbox controller read through `XInputGetState` from a worker thread while Casso is minimized and another application has focus. Nothing that depends on FR-033 for Xbox-class controllers proceeds until it passes.
- **Fallback if it fails**: read Xbox-class controllers through DirectInput while Casso is in the background and through XInput while it is foreground. The cost is that the two triggers read as one combined axis while unfocused, so a trigger mapped to a button may misreport in the background. That degradation must be reported to the user in the Controllers page, not hidden. The fallback is designed in now (the backend already owns both APIs) so a failed check changes a policy, not the architecture.

## R3. Threading: one dedicated controller thread

- **Decision**: a `ControllerInputThread` owns a message-only window, the device notifications, the DirectInput interface and devices, and all XInput and DirectInput polling. It samples at 250 Hz (4 ms), well above FR-003's 60 Hz floor and SC-002's one-frame budget, and publishes each processed frame to the game port.
- **Rationale**: the UI thread's per-frame hook (`TryPresentUiFrame`, `CassoEmuCore/Shell/EmulatorShellPresent.cpp:552`) does not run while the machine is idle (`WaitForFrameOrMessage`), and does not run when minimized, which FR-033 requires. Microsoft documents no thread affinity for DirectInput polling; creating, acquiring and polling on one thread with its own window is the conservative arrangement.
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

## R12. Transient notice and opening the Settings sheet to a page

- **Notice**: the only generic auto-hiding overlay is the screenshot notice (`ShowCaptureNotice`, a `DxuiInfoBanner` expiring after 4 s, `CassoEmuCore/Shell/EmulatorShellPresent.cpp:1246`). Decision: generalize it into a shell notice used by both, rather than adding a second banner.
- **Settings page**: `OpenSettings()` takes no page (`CassoEmuCore/Shell/EmulatorShellDialogs.cpp:540`), but `DxuiPropertySheet::SetActivePage (int)` exists. Decision: add an optional page argument threaded through to `SetActivePage`.
