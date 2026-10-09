# Quickstart: validating disk integrity

Run everything from the worktree root,
`C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity`.

## Prerequisites

- The fixture ROMs: `scripts\FetchRoms.ps1 -Fixtures`.
- The baseline (SC-004): the Debug and Release test counts of the 035 (later
  `master`) commit most recently merged into this branch, measured at that
  merge and recorded in tasks.md's Baselines table. The branch started from
  9,525 tests in Debug and 9,519 in Release, all passing (035 at
  `811a6f727`). A gate's expected count is the latest row plus the tests this
  spec has added since (those cherry-picked from spec 040 included), less the
  real-file unit tests tasks.md T057 moves from the unit suite to the scenario
  suite once it has landed, 65 at the inventory of 2026-10-09.

## Build and test

```powershell
.\scripts\Build.ps1 -Configuration Debug   -Platform x64
.\scripts\RunTests.ps1 -Configuration Debug   -Platform x64
.\scripts\RunTests.ps1 -Configuration Debug   -Platform x64 -Build -Scenario
.\scripts\Build.ps1 -Configuration Release -Platform x64
.\scripts\RunTests.ps1 -Configuration Release -Platform x64
.\scripts\RunTests.ps1 -Configuration Release -Platform x64 -Build -Scenario
```

The scenario suite is part of every phase gate; CI does not run it (CI runs
only `UnitTest.dll`, `.github/workflows/ci.yml:73`). After tasks.md T057 it
holds every test that touches a real file, moved from the unit suite
(`Win32DiskFileIoTests`, `Win32ImageWatcherTests`, the store's
`WriteFileAtomically` and real-folder cases and the rest of T057's list), and
the real-file seam cases, so those run only here. If the scenario suite
cannot run, say so in the summary.

Before the merge, the code-analysis rebuild and the tree style sweep:

```powershell
.\scripts\Build.ps1 -Configuration Debug -Platform x64 -Target Rebuild -RunCodeAnalysis
.\scripts\CheckStyle.ps1 -Mode Tree
```

## The race proof (SC-002)

```powershell
.\scripts\RunTests.ps1 -Build -Sanitize
```

Passes when every test passes in the AddressSanitizer build and
AddressSanitizer reports nothing. The canary test passes in every build: it
checks that the sanitizer build is instrumented. The run that shows the
stress test failing against the live bays is a local change, made once,
reverted, and recorded in the commit message; nothing committed fails under
`-Sanitize`.

## What each story's tests show

| Story | Tests | Expected |
|---|---|---|
| US1 saves stay saved | `DiskWritePathTests`, `DiskImageStoreTests`, `DurableCommitTests`, `DiskFlushHoldTests`, `DiskResetRemountHoldTests`, `DiskHistoryTests` regressions; in the scenario suite, `Win32DiskFileIoTests` (its four cases and R3's six real-file cases, or 040's) and the store's moved real-file cases | The file matches the guest's writes after two `.nib` saves; a failed save keeps the writes through re-insert, reset, power cycle, switch, eject, quit and state load; a reset or re-insert while dirty leaves the writes in the file and the mounted disk; one recovery copy per session; a power cut after a save leaves the file whole; the three new `Win32DiskFileIo` operations and `ReplaceAtomically` give the right Win32 error codes, and attributes and explicit permissions are kept on real files |
| US2 no crashes | `DiskOwnershipTests`, the stand-in tests, the `DiskImageStoreTests` failed-mount regressions, `DriveStatusStressTests`, the Debug app run (T051) | A failed insert leaves the old disk attached; every guarded entry point asserts off its owning thread; no window-thread reader reaches the store; a Debug `Casso` shows no ownership assertion |
| US3 status and changes | `DriveStatusPublisherTests`, `DiskManagerDriveStatusTests`, `SharedImageTests` watch regressions, the relative-path regressions, `ChromeBandLayoutTests` | The published status equals the bay after each change; a relative mount is made absolute and watched; layout keeps consistent facts during a switch |
| US4 40 tracks, blank tracks | `Disk2Tests`, `Disk2ControllerEventTests`, `WozLoaderTests`, `DiskWritePathTests`, the two new scenario tests | The head reaches quarter track 158; a guest format of a WOZ with blank tracks saves every track |
| US5 settings | `UserConfigStoreTests` concurrency regressions | Concurrent saves both land; a CPU-thread save leaves the window's global section alone |

## On screen

Built; start it when you want to look, in the background with
`--title 041-disk-integrity`. With a disk in drive 1:

1. Insert a truncated `.dsk` into the occupied drive: the old disk stays, and the
   message reports that the new one could not be used.
2. Write to a `.nib`, let the motor stop, write again, let it stop, then eject and
   remount: both writes are there.
3. Mount a 40-track WOZ and catalog a file on track 39.

With reverse execution recording:

4. Insert an unloadable file over a disk, step back past it and step forward
   again: no divergence, and the old disk is in the drive.
5. Write to the disk, reset, step back across the reset and seek to the live
   end: no divergence, and the image file's modified time does not change
   during the seek.
6. Step back, then change write protection in Settings: the divergence
   question appears.
7. If an update install can be driven (tasks.md T142; T133 skips this step,
   because the call sites exist only after the `master` merge): write to the
   disk, step back, run one instruction with the debugger's Step, then install
   the update; the saved file holds the disk as it was at the live end. Do it
   for an MSIX update installed at once, a zip update installed now, and an
   update left until Casso closes, then quit (owner confirmed 2026-10-09).

In a Debug build (the ownership check, SC-003):

8. Go through startup, a mount, an eject, an eject with the machine paused
   (the drive's label and tooltip show it gone at once: FR-003 on screen, where
   the `DiskManager` rig test checks the publish and the wake, tasks.md T030),
   hovering over the drives,
   the Disk menu and the drive menus, the Disk II and input debug panels, a
   salvage of a damaged disk with the copy inserted, a lost-file question answered with
   Save as (rename the image's file while the guest's writes are still
   unsaved, for example with the machine paused before the motor stops), a
   machine switch and quit: no ownership assertion dialog appears (tasks.md
   T051). This run is also the proof for the window code around a modal
   dialog or a debug panel window, which no unit test can drive.
