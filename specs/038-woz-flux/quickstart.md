# Quickstart: validating WOZ flux track support

## Prerequisites

```powershell
scripts/FetchRoms.ps1 -Fixtures      # fresh worktree: the suite does not start without them
```

## 1. Unit tests (every iteration, filtered)

```powershell
scripts/RunTests.ps1 -Build -Filter Flux
scripts/RunTests.ps1 -Filter WozLoader
scripts/RunTests.ps1 -Filter Disk2NibbleEngine
scripts/RunTests.ps1 -Filter Damaged
```

Expected: these cover FLUX parsing, precedence over TMAP, 255 runs,
mixed 3.7/4.1 µs cells read within one LSS step (SC-002), head angle across
bit/flux moves, weak bits in long gaps, write splice and round trip (SC-003),
damaged-track read-only mounts and map-level refusals (SC-006), and report
text. Per the testing rules, each new test is shown to fail with its
implementation stubbed.

## 2. Scenario suite (required: drive and loader change)

```powershell
scripts/RunTests.ps1 -Build -Scenario
```

Expected: the flux copy of the DOS 3.3 System Master boots, takes a `SAVE`,
round-trips through a flush, boots again and catalogs the file. Every existing
scenario still passes.

## 3. Bandits, by hand (local only, never committed)

```powershell
$label = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Release\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label, '--disk1', '<path to 00_Bandits.woz>'
```

Run it on an Apple //e Enhanced. Expected: reaches the title screen (SC-001). Capture a screenshot, then restore
`disk1Path` in UserPrefs (`--disk1` persists).

## 4. Performance (Release, pinned to one CCD)

Run the `Disk2NibbleEngine` bit-versus-flux microbenchmark. Then compare
maximum-speed emulation with a flux disk and a bit-only disk spinning
(SC-005: within 2%).

## 5. Pre-merge gate

Full unit suite in Debug and Release, `scripts\Build.ps1 -Target Rebuild
-RunCodeAnalysis`, `scripts/CheckStyle.ps1 -Mode Tree`, and the scenario suite
again after the master merge. SC-004: the copy-protected WOZ demos in
`Apple2/Demos` that boot today still boot.
