# Quickstart: Validating Machine History in the Emulator

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

## Prerequisites

- The worktree builds: `scripts\Build.ps1` (Debug) and `scripts\Build.ps1 -Configuration Release`.
- Fixture ROMs: `scripts\FetchRoms.ps1 -Fixtures`, once per worktree.
- The DOS 3.3 System Master for the scenario suite, which `scripts\RunTests.ps1 -Scenario` fetches when it is missing.
- A disk the guest writes to, for Story 3: a copy of the DOS 3.3 System Master in a scratch folder, never the cached master.

Every launch passes `--title` with the worktree's folder name, and a launch the user did not ask for is minimized and not activated:

```powershell
$label = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label
```

## Automated checks

| Check | Command | Expected |
|---|---|---|
| Unit suite | `scripts\RunTests.ps1 -Build` | All pass, Debug and Release |
| Branch store, journal and switch | `scripts\RunTests.ps1 -Filter KeyframeStoreBranch` | Forks split segments, switches land at the live end with the same checksum (SC-004), eviction order per FR-016 and FR-017 |
| Saved history | `scripts\RunTests.ps1 -Filter SavedHistory` | Round trip and every discard row of [the format contract](contracts/saved-history-format.md) |
| Strip marks and flyout | `scripts\RunTests.ps1 -Filter ImageStripMark` | Per [the strip contract](contracts/timeline-strip.md) |
| Key routing and commands | `scripts\RunTests.ps1 -Filter HistoryCommand` | Per [the commands contract](contracts/emulator-history-commands.md) |
| Scenario suite | `scripts\RunTests.ps1 -Build -Scenario` | All pass, including history persistence and, after 041, the disk rule over real DOS 3.3 writes |
| Cost at 1x | `HistoryCostTests`, Release, pinned as in research R-001 | Every ordinary workload costs under 1% of a core (SC-001a) and no more than 035's recording within noise (SC-001b) |

A filtered run is not the suite; the gate before the master merge is the full Debug and Release suites, the scenario suite, and `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`.

## Story 1: Rewind in the emulator window

1. Launch Casso with the debugger closed, boot the DOS 3.3 master, and run `10 HGR : HCOLOR= 3` then `20 HPLOT RND (1) * 279, RND (1) * 159 : GOTO 20` and `RUN`.
2. After 20 seconds, hold F2 for 3 seconds. The picture runs backward with every frame drawn and no sound; the first backward frame appears within 100 ms (SC-002).
3. Release F2: the machine pauses on the frame on screen within one frame (SC-003).
4. Press Pause to run: the plotting replays from that frame, and "Replaying" and a time pulse over the top of the screen. When the machine is at the live end, the indicator goes and the program runs on.
5. Move the pointer to the bottom 8 DIPs of the screen: the rewind bar opens within 150 ms (SC-008), with thumbnails and the playhead. Drag along it: the screen follows the pointer silently. Move off: the bar closes.
6. Press Ctrl+Shift+R to reset the machine: a reset mark appears on the bar at that moment.
7. Open the History menu: Rewind, Step back, Go live and History on are listed with their keys.

## Story 2: Fork history instead of discarding it

1. Continue from Story 1 with a program that reads the keyboard: `10 GET A$ : PRINT A$; : GOTO 10`, `RUN`, and type `AAAA`.
2. Rewind to before the first A, then type `BBBB`. History forks at the first read of B with no prompt; a fork mark appears on the bar where it split.
3. Rest the pointer on the fork mark: the flyout lists one branch, with its last frame (`AAAA`), how long it ran past the fork, and when it was left. Rest on the row: the main screen previews `AAAA`.
4. Click the row: the machine is paused at the end of the `AAAA` branch, the bar is redrawn as that branch's path, and the `BBBB` branch is a fork mark at the same point.
5. Fork from the `AAAA` branch and switch back to `BBBB`: forks within a branch are drawn only while it is active (FR-014).

## Story 3: Disk image files follow the active branch (after 041 is merged)

1. Mount the scratch copy of the DOS 3.3 master, run `SAVE T1`, and let the drive stop.
2. Rewind to before the save, then run `SAVE T2` and let the drive stop. The image file now has T2 and not T1 (the new active branch's version).
3. Switch to the first branch: the image file has T1 and not T2 (SC-005). Compare the file byte for byte with the branch's version in memory; the scenario test does this.
4. Rewind without forking and let a replayed save stop the drive: the file is not written (FR-019).
5. Exit while behind live: the file has the live end's version, never the version on screen (SC-006).

## Story 4: History persists across launches

1. Record a few minutes with a fork, and exit.
2. Launch again: the timeline spans the same history with the same fork marks, and a branch switch lands at the branch's live end. Loading a full 64 MB history adds no more than 1 s to the launch (SC-007); the scenario test logs the time.
3. Replace the automatic state's history section with bytes from a different build, or damage its hash: the launch loads the machine with history fresh, and no message.
4. Save a state with File > Save state, load it later: the timeline is the file's. Turn history off, save a state, then turn history on and load it: the load is a marked point in the current history.
5. Turn history off from the History menu: the confirmation's text specifies that all of the machine's history, in memory and saved, will be deleted. Confirm, exit and relaunch: no history is loaded.

## Story 5: One history for the emulator and the debugger

1. Fork history in the emulator window, then open the debugger: its timeline is the same strip with the same path, marks and position.
2. Switch branches in the debugger's timeline: the emulator window's rewind bar redraws the new path.
3. Set a breakpoint and run reverse continue: it stops on the active branch, as 035 defines; no mark is added for the stop.
4. Settings has a History page (on or off, and the memory budget), and the debugger's options edit the same settings. History > Rewind 10 seconds and Forward 10 seconds move ten seconds back or toward the live end, and stop at either end.

## Edge cases to check by hand

- F2 at the oldest point: the machine is paused there.
- F2 while a modal dialog is open: nothing happens.
- The rewind bar while an Apple mouse card is in use: it opens only over the bottom band, and the guest receives no click on it.
- A branch switch while the debugger is stopped at a breakpoint: the debugger displays the new position, as after any seek.
- Maximum speed for a minute: recording continues; the timeline has no gap.
