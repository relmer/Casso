# Quickstart: Validating Physical Game Controllers

**Feature**: `034-game-controllers` | **Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

## Prerequisites

- An Xbox-class controller (USB, Xbox wireless adapter, or Bluetooth).
- A non-Xbox DirectInput controller: a USB joystick or generic gamepad. Validation of SC-004 needs both a gamepad and a joystick.
- A build of this branch: `scripts\Build.ps1` (Debug, x64). Confirm `x64\Debug\Casso.exe` and `UnitTest.dll` are newer than the build start before trusting any result.

## 1. Unit tests

```powershell
scripts\RunTests.ps1 -Build -Filter Controller
scripts\RunTests.ps1 -Build -Filter GamePort
```

Then the full suite before merging (Debug and Release). A filtered pass is not a suite pass.

Expected coverage, by contract:

- [contracts/controller-backend.md](contracts/controller-backend.md): decoders over synthetic DirectInput and XInput states; each read failure observed as disconnected.
- [contracts/game-port-mixer.md](contracts/game-port-mixer.md): OR of button sources, axis owner switching, no redundant writes.
- [contracts/prefs-schema.md](contracts/prefs-schema.md): round trip and every rejection rule.
- Selection policy, calibration state transitions, mapping evaluation, deadzone, profile store, capture-by-press: rules in [data-model.md](data-model.md).

## 2. Hardware check: background XInput (do this first)

Research R2 gates FR-033 for Xbox-class controllers on this check.

1. Launch minimized with a label, per the workspace rules:

   ```powershell
   $label = Split-Path -Leaf (git rev-parse --show-toplevel)
   Start-Process .\x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label
   ```

2. With the Xbox controller connected, focus another application.
3. Watch the Controllers page's live readings, or the input debug panel, while moving the stick.
4. **Pass**: readings follow the stick while Casso is unfocused and while minimized. **Fail**: readings freeze at rest. On failure, record it in `research.md` R2 and switch `IsBackgroundCapable (XInput)` to false per the fallback.

## 3. Game port readout program

Boot DOS 3.3 or any Applesoft prompt and enter:

```basic
10 PRINT PDL(0); " "; PDL(1); " "; PEEK(49249) > 127; " "; PEEK(49250) > 127
20 GOTO 10
```

`49249` and `49250` are `$C061` (PB0) and `$C062` (PB1); `1` means pressed.

## 4. Scenarios

| # | Steps | Expected | Spec |
|---|---|---|---|
| 1 | Fresh machine prefs, arrows-to-joystick on. Plug in the Xbox controller. | Notice says which controller was selected; arrows-to-joystick is off; stick at rest reads 127/128. | FR-032, US1 |
| 2 | Push the stick to each extreme; half deflection; press A and B. | 0 and 255 at extremes; roughly halfway at half deflection; PB0/PB1 read 1. | FR-003-005, SC-004 |
| 3 | Leave the stick untouched for 10 s. | Both paddles stay exactly at center. | SC-003 |
| 4 | Hold the stick right and A, then unplug. | Within 100 ms both paddles center and both buttons release; arrow keys now drive the joystick; status shows the fallback. | FR-010, FR-008a, SC-005 |
| 5 | Plug back in while holding an arrow key. | Controller takes over within 2 s; the arrow no longer moves the paddles. | FR-010, SC-005 |
| 6 | Focus another window; move the stick; minimize Casso. | Readings keep following the controller; a held keyboard Open-Apple is released on focus loss. | FR-033 |
| 7 | Settings > Controllers: create profile "D-pad", assign the D-pad to both axes and LT to PB0, Apply. | D-pad drives the paddles to 0/255; LT past its threshold reads PB0 = 1. | FR-019-022, SC-008 |
| 8 | Make "D-pad" active, restart Casso, switch machines and back. | "D-pad" still active on that machine; Default active on the other. | FR-029, SC-006 |
| 9 | Connect the DirectInput joystick with the stick held off center; open Controllers; run Calibrate; Apply; restart. | User calibration persists; rest reads center; limits reach 0/255. | FR-007, FR-007a, US4 |
| 10 | Switch the ][+ and the //e with the controller selected. | PB0/PB1 reach `$C061`/`$C062` on both; Open/Solid-Apple on the //e. | US1 #5 |
| 11 | After 032 is on master: choose a controller and a profile from the Machine menu and the toolbar input control. | Selection and profile change without opening Settings, without a reset. | FR-008, FR-028, FR-031, SC-010 |

## 5. Pre-merge gates

- `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`, all four configurations.
- `scripts\CheckStyle.ps1 -Mode Tree` after `git add -A`.
- Full `RunTests.ps1` suite in Debug and Release.
- The scenario suite is not required: nothing here changes what a guest reads from disk.
