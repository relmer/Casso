# Quickstart: validating disk integrity

Run everything from the worktree root,
`C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity`.

## Prerequisites

- The fixture ROMs: `scripts\FetchRoms.ps1 -Fixtures`.
- The baseline this branch started from: 9,525 tests in Debug and 9,519 in
  Release, all passing (035 at `811a6f727`). SC-004 compares against these.

## Build and test

```powershell
.\scripts\Build.ps1 -Configuration Debug   -Platform x64
.\scripts\RunTests.ps1 -Configuration Debug   -Platform x64
.\scripts\Build.ps1 -Configuration Release -Platform x64
.\scripts\RunTests.ps1 -Configuration Release -Platform x64
```

Before the merge, the code-analysis rebuild and the tree style sweep:

```powershell
.\scripts\Build.ps1 -Configuration Debug -Platform x64 -Target Rebuild -RunCodeAnalysis
.\scripts\CheckStyle.ps1 -Mode Tree
```

## The race proof (SC-002)

```powershell
.\scripts\RunTests.ps1 -Build -Sanitize
```

Passes when the drive-status stress test passes and AddressSanitizer reports
nothing. The canary test, built against the live bays instead of the published
status, is expected to fail in this build only.

## What each story's tests show

| Story | Tests | Expected |
|---|---|---|
| US1 saves stay saved | `DiskWritePathTests`, `DurableCommitTests`, `DiskFlushHoldTests` regressions | The file matches the guest's writes after two `.nib` saves; a failed save keeps the writes through re-insert, reset, power cycle, switch and eject; one recovery copy per session |
| US2 no crashes | `DiskImageStoreTests` failed-mount regression, `DiskStoreOwnershipTests`, `DriveStatusStressTests` | A failed insert leaves the old disk attached; no ownership assertion fires across the suite |
| US3 status and changes | `DriveStatusPublisherTests`, `SharedImageTests` watch and relative-path regressions | The published status equals the bay after each change; a relative mount is watched |
| US4 40 tracks, blank tracks | `Disk2Tests`, `WozLoaderTests`, the two new scenario tests | The head reaches quarter track 158; a guest format of a WOZ with blank tracks saves every track |
| US5 settings | the `UserConfigStore` concurrency regression | Concurrent saves both land |

## On screen

Built; start it when you want to look. With a disk in drive 1:

1. Insert a truncated `.dsk` into the occupied drive: the old disk stays, and the
   message says the new one could not be used.
2. Write to a `.nib`, let the motor stop, write again, let it stop, then eject and
   remount: both writes are there.
3. Mount a 40-track WOZ and catalog a file on track 39.
