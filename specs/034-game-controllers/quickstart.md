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

## 2. Hardware check (do this first)

Research R2 and R13 depend on it. It uses a throwaway probe, not Casso.

1. **XInput packet rate**: for the Xbox controller wired and wireless (or Bluetooth), call `XInputGetState` in a tight loop for 10 seconds while moving the stick continuously, and count `dwPacketNumber` changes per second. Record the numbers in R13 and set the poll period from the fastest.
2. **DirectInput events**: confirm `SetEventNotification` fires on stick movement for the DirectInput device, and whether it reports `DIDC_POLLEDDEVICE`.
3. **Wireless arrival**: with the probe registered for HID notifications, power a wireless Xbox controller on and off through each receiver available (Xbox wireless adapter, Bluetooth, Xbox 360 receiver). **Pass**: an arrival and a removal notification each time. **Fail**: record the receiver's vendor and product ID in R4; the slot recheck fallback applies only while it is attached.
4. **Second window**: with the probe's main window active, open a second top-level window of the same process and activate it. **Pass**: XInput readings keep changing. **Fail**: they freeze; record it in R2, and the Controllers page shows Xbox controllers as paused while the Settings sheet is active.

## 3. Game port readout program

Boot the readout disk, which runs the program on boot -- there is nothing to type:

```powershell
x64\Debug\Casso.exe --machine Apple2e --title <worktree> --disk1 Disks\Casso\JoystickTest.dsk
```

It shows each axis as a number and a bar, and each button as pressed or up. The source is `Disks/Casso/JoystickTest.bas`; rebuild the disk after editing it with:

```powershell
x64\Debug\CassoCli.exe disk create Disks\Casso\JoystickTest.dsk --bootable
x64\Debug\CassoCli.exe disk put Disks\Casso\JoystickTest.dsk Disks\Casso\JoystickTest.bas --as STICK --basic
x64\Debug\CassoCli.exe disk boot Disks\Casso\JoystickTest.dsk STICK
```

The program reads `PDL(0)` through `PDL(3)` and peeks `49249`, `49250` and `49251` -- `$C061` (PB0), `$C062` (PB1) and `$C063` (PB2). On the //c the PB2 column reads the mouse button, inverted, and PDL(2)/PDL(3) are the mouse direction lines rather than axes, so they are not meaningful there (FR-034).

## 4. Scenarios

| # | Steps | Expected | Spec |
|---|---|---|---|
| 1 | Fresh machine prefs, arrows-to-joystick on. Plug in the Xbox controller. | Notice says which controller was selected; arrows-to-joystick is off; stick at rest reads 127/128. | FR-032, US1 |
| 2 | Push the stick to each extreme; half deflection; press A and B. | 0 and 255 at extremes; roughly halfway at half deflection; PB0/PB1 read 1. | FR-003-005, SC-004 |
| 3 | Leave the stick untouched for 10 s. | Both paddles stay exactly at center. | SC-003 |
| 4 | Hold the stick right and A, then unplug. | Within 100 ms both paddles center and both buttons release; arrow keys now drive the joystick; status shows the fallback. | FR-010, FR-008a, SC-005 |
| 5 | Plug back in while holding an arrow key. | Controller takes over within 2 s; the arrow no longer moves the paddles. | FR-010, SC-005 |
| 6 | Hold the stick right and A; activate another application; release; reactivate Casso. | On deactivation both paddles center and both buttons release; nothing moves while inactive; input resumes on reactivation. With the Settings sheet active, input still applies. | FR-033 |
| 7 | Settings > Controllers: edit the Default profile, assign the D-pad to both axes and LT to PB0, Apply; restart Casso. | D-pad drives the paddles to 0/255; LT past its threshold reads PB0 = 1; the mapping survives the restart. | FR-012, FR-019-022, SC-008 |
| 8 | Create profile "D-pad" as a copy of Default, reset Default, make "D-pad" active, restart Casso, switch machines and back. | "D-pad" still active on that machine with the D-pad mapping; Default active on the other with the default mapping. | FR-026-029, SC-006 |
| 9 | Connect the DirectInput joystick with the stick held off center; open Controllers; run Calibrate; Apply; restart. | User calibration persists; rest reads center; limits reach 0/255. | FR-007, FR-007a, US4 |
| 10 | Switch the ][+ and the //e with the controller selected. | PB0/PB1 reach `$C061`/`$C062` on both; Open/Solid-Apple on the //e. | US1 #5 |
| 10a | Create a profile from the Paddles starting point on the Xbox controller; push the left stick right briefly and release. | PDL0 climbs while deflected and holds its value after release; the right stick moves PDL1 the same way. | FR-021a |
| 10b | Bind LB to PB2; press it on the ][+, the //e and the //c. | Last column reads 1 on the ][+ and //e (and the //e treats it as Shift); on the //c the page shows PB2 unavailable and the mouse button column does not change. | FR-020 |
| 11 | Choose a controller and a profile from the Machine menu submenus and from the toolbar input control (expanded segment and collapsed picker); plug a controller in while the Machine menu is open. | Selection and profile change without opening Settings, without a reset. | FR-008, FR-028, FR-031, SC-010 |
| 12 | On a //e with two controllers attached, assign one to PDL0 and the other to PDL1. Move both sticks at once, then hold both buttons at once. | Each axis follows only its own controller; both button lines read 1 together. On a two-player game disk, both players control their own side. | US7, FR-036, FR-037, SC-011 |
| 13 | With those two assigned, unplug one. | Only its axis centers; the other keeps reading its own stick with no interruption, and no stand-in takes the unplugged one's axis while a second controller is still driving its own. | SC-012, FR-008a, FR-010 |
| 14 | On a //e, assign one Xbox controller's left stick to PDL0/PDL1 and its right stick to PDL2/PDL3. | All four axes move from the one controller. | US7 #7, FR-037 |
| 15 | Switch to a //c with a four-axis assignment saved. | Only PDL0 and PDL1 are offered and driven; nothing faults; switching back to the //e restores the four-axis assignment. | FR-034, FR-035 |

## 5. Pre-merge gates

- `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`, all four configurations.
- `scripts\CheckStyle.ps1 -Mode Tree` after `git add -A`.
- Full `RunTests.ps1` suite in Debug and Release.
- The scenario suite is not required: nothing here changes what a guest reads from disk.
