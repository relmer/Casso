# Quickstart: validating the disk inspector

**Feature**: 040-disk-inspector | **Plan**: [plan.md](plan.md)

How to prove each part works. Contracts and data model hold the details;
this page gives the commands and what to expect.

## 1. Prerequisites

```powershell
scripts/FetchRoms.ps1 -Fixtures
```

The scenario suite fetches the DOS 3.3 and ProDOS stock disks itself.
*Bandits* and *Prince of Persia* are local only: copy them to
`%LOCALAPPDATA%\Casso\LocalDisks\` and never into the tree.

## 2. Unit tests, per phase

Build first; `RunTests.ps1` does not build without `-Build`.

| Phase | Filter | Proves |
|---|---|---|
| Loader and image details | `-Filter WozLoader`, `-Filter DamagedDisk`, `-Filter WozImageDetails` | FR-052, FR-053 split table, FR-051 problems, saves keep TMAP |
| Field formats | `-Filter DiskFieldFormat`, `-Filter DiskMarkPattern` | 6-and-2 and 5-and-3 against hand-worked vectors |
| Analyzer | `-Filter TrackAnalyzer`, `-Filter LatchFramer` | SC-001, SC-002 (framer against the engine's second turn), SC-013 |
| Sector writer | `-Filter SectorFieldWriter`, `-Filter DiskWritePath`, `-Filter CrossFormatWrite`, `-Filter DurableCommit` | SC-016, SC-017, including a failure injected at each save step (real-file `DurableCommit` cases run in the scenario suite) |
| File map | `-Filter FileMap` | SC-014, SC-015 |
| Casso host | `-Filter InspectorHost`, `-Filter Disk2WriteHook` | FR-062, FR-068, safe points, apply, undo, redo, rollback (SC-019, SC-020) |
| Comparison | `-Filter DiskComparison` | SC-021 verdicts and differences |
| Themes | `-Filter DiskInspectorPalette` | 4.5:1 text, ΔE2000 ≥ 10 per theme |

```powershell
scripts/RunTests.ps1 -Build -Filter TrackAnalyzer
```

A filtered run is not the suite; the full suite runs once per batch.

Every new test is shown to fail first: revert the fix or stub the
implementation, confirm the assertion message, restore, and stamp the file
(`(Get-Item path).LastWriteTime = Get-Date`) before rebuilding.

## 3. Scenario suite

Required: 040 changes `NibblizationLayer`, `VolumeImage`, `WozLoader` and
`Disk2Controller`.

```powershell
scripts/RunTests.ps1 -Build -Scenario
```

Expect the four SC-018 cases to pass: a guest reads a sector the inspector
edited; a guest reading in a loop during "Pause and apply" sees all old or all
new bytes; a guest writing in a loop ends an apply wait and marks the edit out
of date; a 5-and-3 read routine reads an edited 13-sector sector. Also SC-022
(compare after a guest save) and SC-020's Explorer-to-Casso case after the
rebase.

## 4. On screen, in Casso

```powershell
$label = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label
```

Test windows go on the portrait monitor. Then:

1. Storage → "Inspect disk 1..." with the DOS 3.3 master in drive 1: chips
   "16 sector", "35 tracks", "560/560 sectors good", "Volume 254"; Findings
   empty.
2. Boot with Follow head on: the platter turns, the marker reads, the track
   follows after about 0.3 s.
3. Save a file from BASIC: only the written tracks refresh within 500 ms and
   are marked written.
4. Edit a sector of a text file, apply, read it back in BASIC.
5. Compare with "As inserted": only the written tracks differ; the new file is
   only in A.
6. Each of the three Casso themes: every view, captured; grayscale capture for
   good and bad states (SC-008).
7. *Bandits*, locally: 34 tracks with data, 19 flux, fast and slow bands in
   Timing mode (SC-012).

## 5. On screen, in Explorer (after the rebase onto 033)

1. Select images: the small platter and chips appear within 300 ms for a
   140 KB image, and the catalog appears no later than before (SC-009).
2. "Inspect disk image" on DOS 3.3, ProDOS, protected and damaged images.
3. "Compare disk images" on two images.
4. Apply an edit to an image a running Casso has mounted: Casso reloads it,
   and Explorer's window shows any reply other than a reload.
5. Light and Dark themes, switched while open.

## 6. Performance (SC-003, SC-004, SC-005, SC-021)

- Analysis and comparison timings: Release `PerformanceTests` with
  `-Filter Inspector`, pinned to one CCD.
- Emulation cost: five alternating runs, inspector open and closed, following
  the head while a disk boots and saves, at Maximum speed, pinned to the same
  cores; open must be within 2% of closed. Five 60 s runs at normal speed for
  audio underruns and dropped frames.
- Zoom and pan: the scripted platter and strip walk; median frame time at most
  one refresh interval, no frame over 33 ms.

## 7. Pre-merge gates

The first release merges twice (plan, Implementation order). Merge 1, the
shared sector writer, runs items 1 to 5 and 7 with SC-016 and SC-017 through
the `disk` command; Merge 2 runs all of them.

1. Merge master; `scripts/CheckStyle.ps1 -Mode Tree` after `git add -A`.
2. Full suite, Debug and Release, x64; ARM64 builds.
3. `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`.
4. Scenario suite (section 3).
5. Dormann and Harte are not required: no CPU or assembler change.
6. SC-007: every test image byte-for-byte unchanged after opening, using and
   closing the inspector without applying.
7. CHANGELOG and README, after the owner has tested and approved.
