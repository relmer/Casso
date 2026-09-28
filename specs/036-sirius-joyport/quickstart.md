# Quickstart: Validating the Sirius Joyport

**Feature**: [spec.md](spec.md) | **Contracts**: [contracts/](contracts/)

## Prerequisites

- x64 Debug build via `Casso.sln` (`scripts/Build.ps1`), then
  `scripts/RunTests.ps1` (fixture ROMs: `scripts/FetchRoms.ps1 -Fixtures` in a
  fresh worktree).
- One controller whose profile already works in joystick games; a second one
  for V4. A D-pad-bound profile for V2.
- Every launch passes `--title` with the worktree directory name, and runs
  minimized unless the user asked to see it.

## The readout disk

Source `Disks/Casso/JoyportTest.bas`; rebuild after editing:

```powershell
x64\Debug\CassoCli.exe disk create Disks\Casso\JoyportTest.dsk --bootable
x64\Debug\CassoCli.exe disk put Disks\Casso\JoyportTest.dsk Disks\Casso\JoyportTest.bas --as JOYPORT --basic
x64\Debug\CassoCli.exe disk boot Disks\Casso\JoyportTest.dsk JOYPORT
```

It shows UP, DOWN, LEFT, RIGHT and FIRE for the left and right jacks as CLOSED
or OPEN.

```powershell
$label = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Debug\Casso.exe -ArgumentList '--machine', 'Apple2e', '--title', $label, '--disk1', 'Disks\Casso\JoyportTest.dsk'
```

`--disk1` persists to UserPrefs; restore the machine's `disk1Path` afterward.

## Scenarios

| # | Covers | Steps | Expected |
|---|---|---|---|
| V1 | US1, SC-001 | //e, readout disk, check **Sirius Joyport** in the controller picker. Hold each direction and fire in turn. | Only the held switch reads CLOSED, on both jacks (single source). No reset happened when the row was checked. |
| V2 | US1 sc. 2-4 | Hold up-left; then nudge the stick a little; then use a D-pad-bound profile. | Diagonal closes UP and LEFT. A small nudge closes nothing. The D-pad closes at once. |
| V3 | US2, SC-003 | Joyport attached, nothing held: 20 Ctrl-Resets and 20 power cycles. Then hold Open Apple through Ctrl-Reset. | Normal reset every time, no self-test, no reboot. The Open Apple reset reboots. |
| V4 | US3, SC-005 | Multiplayer, a controller in each slot, slot targets set to anything. Move both at once. | Each controller closes only its own jack. Disconnect player 2: the right jack reads all OPEN, the left is unaffected. (Superseded 2026-09-27: there is no Multiplayer setting; a controller as Player 2 starts two-player play, and V18 covers the jacks.) |
| V5 | US4, SC-007 | Attach from the picker; open Settings, Machine tab. Set None, OK. Reattach; relaunch; switch to the ][+ and to the //c. | The Machine tab shows the Joyport, then the picker row unchecks. After relaunch the //e still has it; the ][+ has it only if attached there; the //c offers it in neither place. |
| V6 | R1 | ][+, Wavy Navy, Joyport option selected in-game. | The ship steers left and right with the stick's left and right, not up and down. |
| V7 | US1, SC-004 | Boulder Dash on the //e, Joyport option selected. | Eight directions and fire, fire held with a direction, within a minute of attaching. |
| V8 | US5 | Joyport attached, Settings, Controllers page. Move the stick short of and past halfway; diagonals; fire. Multiplayer: move Editing to player 2. Detach. | The five lights follow the switches; the caption shows Both, then Right jack. Detached, the stick and button lights return. (Superseded 2026-09-27: there is no Multiplayer checkbox; give Player 2 a controller instead. The heading reads "Atari joystick: both jacks", then "Atari joystick: right jack"; see V21.) |
| V9 | edge | Joyport attached, open Settings, check or uncheck the picker row, press OK without touching the Machine tab. | The picker's change stands. |
| V10 | FR-013, SC-006 | Detach; `JoystickTest.dsk` on the ][+ and //e. | Paddles and buttons exactly as before. |

AppleWin issue #1517's `JOYPORT.DSK` is a useful second check for V1 if the
user supplies it; it is third-party and is not committed.

V5 and V9 describe the 2026-09-24 design (the Machine tab entry, a per-machine
setting). V11-V20 replace them for the 2026-09-27 design; V1-V4, V6-V8 and V10
still apply, with the picker row now labeled **Joyport (Atari mode)**.

## Scenarios for the 2026-09-27 design (GH #156)

Prerequisites as above, plus: a gamepad (Xbox-class or DirectInput) and a
joystick for V16 and V17; Bandits (GH #155) for V19. Note the global
`gamePortAdapter` value in `UserPrefs.json` before starting, and restore it
afterward along with `disk1Path`.

| # | Covers | Steps | Expected |
|---|---|---|---|
| V11 | US4 sc. 1-2, FR-001, FR-012 | //e, readout disk. Check **Joyport (Atari mode)** in the picker. Open Settings, Controllers page. Move the switch up to Apple mode, without pressing OK; look at the picker. Move it back down; press Cancel. | The row checks with no reset. The page shows the switch down, labeled "Atari mode" (superseded 2026-09-27: "Apple (rear)" above the switch and "Atari (front)" below it, each centered on it side to side, and "Joyport" to its left, centered top to bottom). Apple mode takes effect at once (the readout stops answering) and the row unchecks. After Cancel the Joyport is still on. The Machine tab lists no Joyport. |
| V12 | US4 sc. 3, SC-007, FR-002 | Joyport on. Relaunch; then switch to the ][+. | On after the relaunch, and on the ][+. |
| V13 | US4 sc. 4, FR-002 | Joyport on. Switch to the //c; open the picker and the Controllers page. Switch back to the //e. | The //c offers the Joyport in neither place, and the game port reads as with no Joyport (`JoystickTest.dsk` behaves normally). Back on the //e it is on. |
| V14 | FR-002 adoption | Close Casso. In a copy of `UserPrefs.json`, remove the global `gamePortAdapter` and set the //e's `$cassoUiPrefs.gamePortAdapter` to `"siriusJoyport"`. Launch the //e with that file. Relaunch the ][+. | The //e launches with the Joyport on, the global key is written, and the ][+ has it on too. Repeating with the ][+ launched first and no key in its block starts with the Joyport off. |
| V15 | US7 sc. 1-4, FR-017, FR-018 | An Xbox controller with no Joyport-mode choice. Turn the Joyport on. Try the D-pad, left stick, right stick; A, B, X, Y, both bumpers, both triggers. Edit the Joyport profile, then Reset profile. Look for Rename and Delete. Turn the Joyport off. | Every direction control steers and every listed button fires; Back, Start and stick clicks do not. The Joyport profile is checked. Reset restores the Joyport mapping. Rename and Delete are unavailable. Off: the Default plays again. |
| V16 | FR-017 DirectInput | A DirectInput gamepad with a second stick, then a flight stick, each with the Joyport on and no choice made. | The gamepad's second stick steers and its triggers never hold a direction. The flight stick steers with its stick and hat only; a throttle at rest closes nothing. |
| V17 | US7 sc. 5, FR-020 | Joyport on: open the picker's profile list and the Controllers page list; create a profile from Copy of Default; turn the Joyport off and open both lists again. | On: the Joyport profile first, then Joyport-mode profiles only; the new profile appears here and not in normal mode. Off: Default first, then normal-mode profiles only. On the Controllers page, moving the switch swaps the list in place. A name already used in the other mode is rejected. (Superseded 2026-09-27: Copy of Default is not offered with the Joyport on; create from the Joyport mapping or a copy of a Joyport profile, and see V22 for the three kinds.) |
| V18 | US3 sc. 4-6, FR-008, FR-019, SC-009 | Joyport on, a stick and a gamepad attached at launch, both players on Automatic, readout disk. Press fire on the gamepad. Then move the stick. Then set Joyport right to Same as left and move the stick again. | The gamepad closes switches on both jacks and a notice reads "Joyport left and right: description"; the stick closes nothing. After the stick is used it is Joyport right, with its own notice, and the gamepad drives the left jack alone. With Same as left, the gamepad drives both jacks and the stick drives nothing. The picker rows read Joyport left and Joyport right throughout. With Joyport right on Automatic and no holder, its row reads "Joyport right: same as left", in lower case after the colon; the submenu entry reads "Same as left". |
| V19 | SC-010 | Bandits on the //e, Joyport on, a controller on the Joyport profile. Press Ctrl-@ to select the Joyport. | Plays with no mapping changes. |
| V20 | FR-009 | Joyport on: open Player 1's submenu. Run `JoystickTest.dsk`. | Mouse-as-paddle is not listed. The paddles read as no paddle connected. Off: mouse-as-paddle returns. (2026-09-27: it returns only while Player 1 is in Paddle mode. With the Joyport on, the Joystick/Paddle pair is disabled and shows Joystick, so Paddle mode cannot be chosen; the keys stay listed.) |
| V21 | FR-015, FR-019 (2026-09-27) | Joyport on, Settings, Controllers page, one controller, then a second controller as Player 2; move Editing to each. Open Player 2's drop-down. Turn the Joyport off. | "Apple (rear)" above the switch, "Atari (front)" below it, "Joyport" to its left. The heading above the lights reads "Atari joystick: both jacks" with one controller, then "Atari joystick: left jack" and "Atari joystick: right jack"; with no controller, "Atari joystick". Each player's note reads "both jacks", "left jack" or "right jack". Player 2's Disabled entry reads "same as left". Each mode drop-down is disabled and shows Joystick. No Multiplayer checkbox. Off: the mode drop-downs are enabled and show the saved modes. |
| V22 | FR-020 (2026-09-27) | Joyport off: set Player 1 to Joystick, open its profile list and New profile; set it to Paddle and repeat. Turn the Joyport on and repeat. | Joystick: Default first, then Joystick profiles; New profile offers Default mapping or a copy of a Joystick profile. Paddle: Paddles first, then Paddle profiles; New profile offers Paddles mapping or a copy of a Paddle profile. Joyport on: Joyport first, then Joyport profiles only, whatever the saved mode; New profile offers Joyport mapping or a copy of a Joyport profile. |

The manual's test program (validation.md, Phase 3) checks SC-008 only if
FR-016 is adopted. Until then, with two controllers playing, both of its
one-stick sections read the left controller, which is expected.
