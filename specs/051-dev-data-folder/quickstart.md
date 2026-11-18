# Quickstart: Validating the developer data folder

Every launch passes `--title` and starts minimized without activation. Never edit files under the owner's `%LOCALAPPDATA%\Casso`; the checks below only hash them.

## Prerequisites

- A Debug x64 build of this branch: `scripts\Build.ps1`.
- ROMs for the checkout: `scripts\FetchRoms.ps1`, which writes under `<repo root>\CassoData\`.

## 1. A developer build leaves the shipped folder alone (SC-001)

```powershell
$before = Get-ChildItem "$env:LOCALAPPDATA\Casso" -Recurse -File | Get-FileHash
$label  = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label
# change a setting, mount a disk, exit with File > Exit
$after  = Get-ChildItem "$env:LOCALAPPDATA\Casso" -Recurse -File | Get-FileHash
Compare-Object $before $after -Property Path, Hash
```

Result: no differences, and `CassoData\UserPrefs.json` exists in the checkout root.

## 2. Two checkouts at different commits (SC-002)

Build master in its checkout and this branch here. Launch each with `--machine Apple2e`, then `--machine Apple2cPlus` where the build supports it. Result: each boots, and neither checkout's `CassoData\` changes while the other build runs.

## 3. An older build over newer files (SC-003)

In a scratch folder set with `CASSO_PROFILE_DIR`, copy a `Machines\` tree written by a newer build. Launch the older build. Result: every machine it supports boots, the machine list excludes every machine it lacks, and the copied files are byte-identical afterward.

## 4. Unit tests (SC-004)

```powershell
scripts\RunTests.ps1 -Build -Filter DataFolder
scripts\RunTests.ps1 -Filter MachineConfig
```

Each rule in [contracts/data-folder-rules.md](contracts/data-folder-rules.md) has a test. Break each rule in turn and confirm its test fails.

## 5. Startup cost (SC-005)

Run the measurement in research R4 on a signed zip copy and on this build, and add the results to `validation.md`.
