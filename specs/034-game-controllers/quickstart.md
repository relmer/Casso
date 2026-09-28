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
| 1 | Fresh machine prefs, arrows-to-joystick on. Plug in the Xbox controller. | The picker shows the controller; arrows-to-joystick is off; stick at rest reads 127/128. | FR-032, US1 |
| 2 | Push the stick to each extreme; half deflection; press A and B. | 0 and 255 at extremes; roughly halfway at half deflection; PB0/PB1 read 1. | FR-003-005, SC-004 |
| 3 | Leave the stick untouched for 10 s. | Both paddles stay exactly at center. | SC-003 |
| 4 | With only the Xbox controller attached, hold the stick right and A, then unplug. | Within 100 ms both paddles center and both buttons release; the notice names the controller and the picker reads "Controller"; the arrow keys do not move the paddles and X/Z still type. | FR-010, FR-008a, FR-013, SC-005 |
| 5 | Plug it back in. Then attach the DirectInput joystick, unplug the Xbox controller, and plug it back in. | The Xbox controller is selected again within 2 s. When it is unplugged the joystick becomes the selection and the notice names the Xbox controller; when it returns the joystick keeps the axes and the Xbox row is unchecked. | FR-008a, FR-032, SC-005 |
| 6 | Hold the stick right and A; activate another application; release; reactivate Casso. | On deactivation both paddles center and both buttons release; nothing moves while inactive; input resumes on reactivation. With the Settings sheet active, input still applies. | FR-033 |
| 7 | Settings > Controllers: edit the Default profile, assign the D-pad to both axes and LT to PB0, Apply; restart Casso. | D-pad drives the paddles to 0/255; LT past its threshold reads PB0 = 1; the mapping survives the restart. | FR-012, FR-019-022, SC-008 |
| 8 | Create profile "D-pad" as a copy of Default, reset Default, make "D-pad" active, restart Casso, switch machines and back. | "D-pad" still active on that machine with the D-pad mapping; Default active on the other with the default mapping. | FR-026-029, SC-006 |
| 9 | Connect the DirectInput joystick with the stick held off center; open Controllers; run Calibrate; Apply; restart. | User calibration persists; rest reads center; limits reach 0/255. | FR-007, FR-007a, US4 |
| 10 | Switch the ][+ and the //e with the controller selected. | PB0/PB1 reach `$C061`/`$C062` on both; Open/Solid-Apple on the //e. | US1 #5 |
| 10a | (Since 2026-09-27: set Player 1 to Paddle mode first; the starting point is "Paddles mapping", offered only for a Paddle profile.) Create a profile from the Paddles starting point on the Xbox controller; push the left stick right briefly and release. | PDL0 climbs while deflected and holds its value after release; A reads PB0; PDL1 and PB1 are unassigned. | FR-021a |
| 10b | Bind LB to PB2; press it on the ][+, the //e and the //c. | Last column reads 1 on the ][+ and //e (and the //e treats it as Shift); on the //c the page shows PB2 unavailable and the mouse button column does not change. | FR-020 |
| 11 | Choose a controller from the command bar's paddle-source picker, and a profile from its Profiles submenu; with the Profiles submenu open, hover and click the picker's other rows; with the picker open, click a menu title; plug a controller in while the Machine menu is open. | Selection and profile change without opening Settings, without a reset. The picker's rows highlight and pick while the submenu is open. | FR-008, FR-028, FR-031, SC-010 |
| 11a | In multiplayer with two pads of one model, pick a different profile for each from the Profiles submenu; relaunch; open Settings and move Editing between the two pads. | Each player's section has its own header and check, each pad uses its own profile, both survive the relaunch, and the Profile drop-down follows the pad in Editing. | FR-028, FR-029 |
| 12 | On a //e with two controllers attached, turn multiplayer on and set player one to joystick 0 and player two to joystick 1. Move both sticks at once. | Each player's paddles follow only their own controller; neither moves the other's. On a two-player game disk, both players control their own side. | US7, FR-037, FR-038, SC-011 |
| 13 | With those two players set, hold both first buttons at once; then press a control each profile binds to PB1 or PB2. | PB0 reads player one and PB1 reads player two, together. The PB1/PB2 bindings do nothing while the mode is on, and the profile still holds them. | FR-039, US7 #2-3 |
| 14 | Unplug one player's controller. | Only that player's paddles center and only their button line releases; the other keeps reading their own stick with no interruption and does not move onto the freed paddles. Plug it back in: that player plays again. | SC-012, FR-008a, FR-010 |
| 15 | With a four-axis setup saved, switch to a //c and back to the //e. Then pick a single controller from the toolbar's paddle-source picker. | On the //c only player one plays, nothing faults, and the //e plays both players again. Picking a single source turns multiplayer off and hands the game port to that one controller, with both player slots kept for when it goes back on. | FR-034, FR-035, FR-037 |

**Superseded in part (2026-09-27)**: scenario 1's "arrows-to-joystick is off", scenario 5's replacement by the joystick, scenario 11's Profiles submenu, scenario 11a's per-player headers, and the multiplayer mode switch in scenarios 12, 13 and 15 describe the design before GH #156. Section 4a replaces them, and section 4b replaces the per-player targets set in scenario 12 (each player's mode sets them now); the rest of each scenario still applies.

## 4a. Scenarios for two always-present player slots (2026-09-27)

Start each from global prefs with no `controllers.players` key unless the row says otherwise, launching with `--title` and minimized unless the scenario needs the window. Unit tests for these rules are listed in section 1 through [contracts/game-port-mixer.md](contracts/game-port-mixer.md), [contracts/controller-backend.md](contracts/controller-backend.md), [contracts/prefs-schema.md](contracts/prefs-schema.md) and [contracts/notice-stack.md](contracts/notice-stack.md).

| # | Steps | Expected | Spec |
|---|---|---|---|
| 16 | Both players on Automatic, the DirectInput stick and the Xbox pad attached at launch. Boot the readout disk. Touch nothing for 5 s; then move the pad's stick and press A. | Nothing drives the port until the pad is used. The pad is Player 1, a "Player 1: <pad>" notice shows, and it drives PDL0, PDL1 and PB0-PB2 (PB2 on the //e via LB bound to PB2). The stick drives nothing. | FR-042, SC-013, US7 #10 |
| 17 | Then move the stick past half travel. | The stick becomes Player 2 on Joystick 1: PDL2/PDL3 and its first button on PB2; PDL0, PDL1, PB0 and PB1 still follow the pad and do not change. "Player 2: <stick>" shows below the first notice if it is still up. | FR-039, SC-014, edge case "bumped" |
| 18 | Quit Casso, detach everything, relaunch, then plug in the pad, then a second pad (or the stick). | The first connected is Player 1 and the second Player 2, each shown by a notice only if it differs from the last holder of that slot. Each gives input and plays its own side. | FR-042, FR-044, SC-014, US7 #9 |
| 19 | Relaunch with only the pad attached that was Player 1 last time. Then relaunch with only the stick attached. | The pad plays at once and no notice shows. The stick plays at once and "Player 1: <stick>" shows. | FR-032, FR-044, US7 #12, SC-005 |
| 20 | Two players playing. Unplug Player 2's controller while holding Player 1's stick right and A. | Only PDL2/PDL3 center and PB2 releases; Player 1's readings do not flicker, and Player 1 does not gain PB2 or PDL2/PDL3. A disconnect notice shows. Plug it back in: it plays as Player 2 again. | FR-040, FR-010, SC-012, US7 #6 |
| 21 | Two players playing. Unplug Player 2, then Player 1; plug the original Player 2 back in and use it. | With nobody left playing, Automatic starts over: the returning controller becomes Player 1. | FR-040, US3 #2a, edge case "took over" |
| 22 | Pick Keys for Player 1 from the picker, then plug in a controller and relaunch. | The keys keep driving Player 1 across the connect and the relaunch; the controller is listed in Player 1's submenu and plays as Player 2 once used. | FR-032, US2 #5 |
| 23 | (Superseded 2026-09-27 by scenario 30: the profile no longer sets the target.) Give each of two pads a profile that binds PDL0 alone (Paddles starting point); both players use their pads. | Player 1 drives PDL0 and PB0, Player 2 drives PDL1 and PB1. | FR-043, US7 #11 |
| 24 | (The Multiplayer step is superseded 2026-09-27: there is no checkbox and Player 2's row is always shown; see scenario 33.) Open the picker. Open each player row's submenu. Pick the controller the other player holds; pick Disabled for Player 2; untick and tick Multiplayer on Settings > Controllers. | Player rows show what is playing. Each submenu lists Automatic ("Automatic (<description>)" once chosen), the controllers, then keys/mouse or Disabled, and a profile section under the playing controller's description. Picking the other's controller returns the other to Automatic. Disabled leaves Player 1 driving alone. The checkbox tracks Player 2's Disabled entry and the rows below it slide down and back over the menus' open duration. | FR-008, FR-028, FR-037, FR-041 |
| 25 | Take a screenshot, toggle write protect and unplug a controller within two seconds of each other. | Three notices stack in arrival order, each up for its full time; as each expires the ones below slide up. With Windows animation effects off they jump instead. | FR-044 |
| 26 | With a controller whose description is long (or a long test description through a fake), have two players playing; narrow the window. | The picker reads "<Player 1 description> +1"; when too long the description loses its middle to an ellipsis and "+1" stays whole; the strip's other entries move as little as the cap allows. | FR-008b |
| 27 | First launch of this build over prefs from the previous release, on a machine whose `$cassoUiPrefs` holds a controller and a `multiplayer` block, with a second machine holding a different controller. | The launched machine's controller and slots become the players' picks, and each slot's `maps` becomes that player's mode (a single paddle gives Paddle mode); the second machine's are ignored; switching machines keeps the players. The global prefs gain `players`; the per-machine keys are left in the files. | FR-011, contracts/prefs-schema.md |
| 28 | Leave Casso idle on the //e for 60 s with both players on Automatic and two controllers attached, none used; compare Casso's CPU time with the same run with no controllers attached. | No measurable difference while the idle watch runs. | SC-007, R23 |

## 4b. Scenarios for a Joystick or Paddle mode per player (2026-09-27, later)

Replace every step above that sets a player's "maps to" target or ticks the Multiplayer checkbox. Boot the readout disk for each.

| # | Steps | Expected | Spec |
|---|---|---|---|
| 29 | (Superseded in part 2026-09-28: the submenu lists Joystick, Joyport left (Atari), Joyport right (Atari) and Paddle, and the keys are listed in Joystick and either jack mode; the row still ends in " (paddle)" in Paddle mode; see scenario 35.) Open the picker and Player 1's submenu; pick Paddle, then Joystick. Then do the same from the Joystick / Paddle drop-down on Player 1's row in Settings > Controllers, and Apply. | The submenu lists Joystick and Paddle after a separator, the player's mode checked. In Paddle mode the row reads "Player 1: <description> (paddle)", "Use keys as joystick" is not listed and "Use mouse as paddle" is; in Joystick mode the reverse. The page's note for the row reads "paddle 0" in Paddle mode and "joystick 0" in Joystick mode, and the profile list holds only profiles of that mode (Paddles first, or Default first). Picking Paddle with the keys on returns Player 1 to Automatic and turns the keys off. | FR-008, FR-037, FR-043 |
| 30 | On the //e, two pads, both players in Paddle mode; both pads used. | Player 1 drives PDL0 and PB0, Player 2 drives PDL1 and PB1; Player 2's row reads "Player 2: <description> (paddle)" and its note "paddle 1". Neither pad's other bindings reach the port. | FR-039, FR-043, US7 #11 |
| 31 | Then set Player 1 to Joystick mode, leaving Player 2 in Paddle mode. | Player 1 drives PDL0, PDL1, PB0 and PB1; Player 2 moves to PDL2 and PB2, and its note reads "paddle 2". | FR-039, US7 #5 |
| 32 | On a //c, two pads, both players in Joystick mode; then both in Paddle mode. | In Joystick mode Player 2 drives nothing and its note reads "nothing on this machine"; Player 1 plays on joystick 0. In Paddle mode each drives its own paddle, PDL0/PB0 and PDL1/PB1. | FR-035, FR-039, US7 #4 |
| 33 | (Superseded in part 2026-09-28: there is no Joyport switch; the mode drop-downs stay enabled, Player 2's Disabled entry reads Disabled, and a Joyport player is set by its mode; see scenarios 35-38.) Settings > Controllers with one pad attached; then set Player 2 to Disabled. Turn the Joyport on (not on the //c). | Both player rows show at all times, each with an entry drop-down, a Joystick / Paddle drop-down and a note; there is no Multiplayer checkbox. With Player 2 Disabled its note is empty and Player 1 drives alone. With the Joyport on both mode drop-downs are disabled and show Joystick, Player 2's Disabled entry reads "same as left", the notes read "left jack", "right jack" or "both jacks", and the picker's Joystick and Paddle entries are disabled with Joystick checked. | FR-037, spec 036 FR-009, FR-019 |
| 34 | Two players playing; unplug Player 1's controller while Player 2 plays on. | The picker's face reads "<Player 1 description> (disconnected) +1", within the same length cap as scenario 26. Plug it back in: the face returns to "<Player 1 description> +1". | FR-008b, FR-040 |
| 35 | On the //e open each player's submenu, then each player's mode drop-down on Settings > Controllers. Put Player 1 on Joyport left (Atari). | Each list reads Joystick, Joyport left (Atari), Joyport right (Atari), Paddle; Player 2's starts with Same as Player 1, checked by default. There is no Joyport row in the picker and no Apple / Atari switch on the page. With Player 1 on Joyport left, Player 2's Joyport left is disabled in both places, drawn in the disabled text color, skipped by the arrow keys and not taken by a click. | FR-008, FR-036, FR-037 |
| 36 | Player 1 on Joyport left, Player 2 on Same as Player 1, two pads; use one, then the other. | The notices read "Player 1 (Joyport left and right): <description>", then "Player 2 (Joyport right): <description>". The rows read "Player 1: <description>" and "Player 2: <description>"; the notes read "both jacks", then "left jack" and "right jack". | FR-044, spec 036 FR-008, FR-019 |
| 37 | Player 1 on Joyport left, Player 2 on Joystick, both pads used; open Player 2's profile list and the Controllers page with Editing on Player 2's pad. | Player 2 drives PDL0 and PDL1 and no button line; its list holds Joystick profiles; its button binding rows are disabled, and under its row the warning triangle and "This controller's buttons are disabled because Player 1 is using the Joyport." Player 1's list holds Joyport profiles. | FR-039, FR-043 |
| 38 | Player 1 on the keys: Joystick, then Joyport left, then Paddle. Then Player 1 on the mouse in Paddle mode, with a pad as Player 2 in Paddle mode, both used. | The keys are listed on Joystick and Joyport left, not on Paddle; on Joyport left the arrows close Player 1's switches and move no paddle. The mouse is listed only on Paddle, and with Player 2 playing it moves PDL0 alone while Player 2 drives PDL1. | FR-008, FR-036, FR-039 |
| 39 | Player 1 on Joyport left; switch to the //c, open the picker and the page; switch back to the //e. | The //c lists no Joyport entry and Player 1 plays as Joystick; back on the //e Player 1 is on Joyport left. | FR-035 |
| 40 | Two players playing with a long Player 1 description; unplug Player 1's pad; narrow the window until the picker's face shortens. | The face keeps the whole "(disconnected) +1"; the ellipsis falls inside the description. | FR-008b, SC-015 |
| 41 | Close Casso; set the global `gamePortAdapter` to `"siriusJoyport"` in a copy of `UserPrefs.json`; launch. | Player 1 is on Joyport left and Player 2 on Same as Player 1; the global key is gone afterward. | spec 036 FR-002 |

## 5. Pre-merge gates

- `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`, all four configurations.
- `scripts\CheckStyle.ps1 -Mode Tree` after `git add -A`.
- Full `RunTests.ps1` suite in Debug and Release.
- The scenario suite is not required: nothing here changes what a guest reads from disk.
