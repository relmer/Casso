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
| V4 | US3, SC-005 | Multiplayer, a controller in each slot, slot targets set to anything. Move both at once. | Each controller closes only its own jack. Disconnect player 2: the right jack reads all OPEN, the left is unaffected. |
| V5 | US4, SC-007 | Attach from the picker; open Settings, Machine tab. Set None, OK. Reattach; relaunch; switch to the ][+ and to the //c. | The Machine tab shows the Joyport, then the picker row unchecks. After relaunch the //e still has it; the ][+ has it only if attached there; the //c offers it in neither place. |
| V6 | R1 | ][+, Wavy Navy, Joyport option selected in-game. | The ship steers left and right with the stick's left and right, not up and down. |
| V7 | US1, SC-004 | Boulder Dash on the //e, Joyport option selected. | Eight directions and fire, fire held with a direction, within a minute of attaching. |
| V8 | US5 | Joyport attached, Settings, Controllers page. Move the stick short of and past halfway; diagonals; fire. Multiplayer: move Editing to player 2. Detach. | The five lights follow the switches; the caption shows Both, then Right jack. Detached, the stick and button lights return. |
| V9 | edge | Joyport attached, open Settings, check or uncheck the picker row, press OK without touching the Machine tab. | The picker's change stands. |
| V10 | FR-013, SC-006 | Detach; `JoystickTest.dsk` on the ][+ and //e. | Paddles and buttons exactly as before. |

AppleWin issue #1517's `JOYPORT.DSK` is a useful second check for V1 if the
user supplies it; it is third-party and is not committed.
