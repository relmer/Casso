# Implementation Plan: Separate data folder for developer builds

**Branch**: `051-dev-data-folder` | **Date**: 2026-11-18 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/051-dev-data-folder/spec.md`

## Summary

Choose the data folder once per process: `CASSO_PROFILE_DIR` when set, `%LOCALAPPDATA%\Casso` for a signed or MSIX copy, and `CassoData\` at the repo root, or beside the binary, for everything else. The choice is a pure function in `CassoEmuCore` with the existing install type detection as its input. Every runtime path resolves through it. Separately, an older build boots its embedded machine config when the file on disk has a higher version, lists only machines it has a config for, and backs up an edited config with a lower version before replacing it.

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145)

**Primary Dependencies**: Win32 (`WinVerifyTrust` through `AuthenticodeVerifier`, `GetModuleHandleExW`, `SHGetKnownFolderPath`), the project JSON parser

**Storage**: Files under the data folder: `Machines\`, `UserPrefs.json`, `Themes\`, `Disks\`, `Devices\`, `Links\`, `Logs\`, `Thumbnails\`, `CrashDumps\`, `Updates\`

**Testing**: Microsoft C++ Unit Test Framework (`UnitTest`), the scenario suite for the SSC ROM lookups, and the quickstart checks on screen

**Target Platform**: Windows 10/11, x64 and ARM64

**Project Type**: Desktop application with core static libraries

**Performance Goals**: Folder resolution, signature check included, under 50 ms on Release (SC-005)

**Constraints**: Unit tests must not read the real environment or file system (Principle II); the crash handler must not run the check or allocate

**Scale/Scope**: About eight runtime call sites, about ten machine config readers, three scanner callers, three scripts

## Constitution Check

| Principle | Status |
|---|---|
| I. Code Quality | Pass. New code follows EHM, alignment and banner rules. |
| II. Testing Discipline | Pass. `DataFolderResolver::Resolve`, `MachineConfigSource::ChooseText`, the scanner predicate and the `Plan` change are pure and take fakes. The real environment is read only in the production adapters. |
| III. User Experience Consistency | Pass. No new flags. `--machine` with an unsupported machine returns the existing error. |
| IV. Performance | Pass, subject to the R4 measurement. |
| V. Simplicity | Pass. Reuses `InstallTypeDetector` and the seams it already has; one new pure class and one new adapter. |
| VI. Thin Executable | Pass. Everything lives in `CassoEmuCore`; no exe gains code. |

Re-checked after Phase 1: no violations, so Complexity Tracking is empty.

## Project Structure

### Documentation (this feature)

```text
specs/051-dev-data-folder/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── data-folder-rules.md
├── checklists/
│   └── requirements.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Core/
│   ├── DataFolderResolver.h/.cpp        # new: the pure rules in the contract
│   ├── PathResolver.h/.cpp              # GetLocalAppDataDir becomes GetDataFolder, cached
│   ├── MachineConfigSource.h/.cpp       # new: disk or embedded text per session
│   ├── MachineConfigUpgrade.cpp         # lower stamp goes through the hash check
│   └── MachineScanner.h/.cpp            # HasEmbeddedConfig predicate
├── Update/
│   └── ModuleInstallEnvironment.h/.cpp  # new: IInstallEnvironment over the core module's path
├── Gui/GuiMain.cpp                      # crash dump buffer, config text, scanner predicate
├── DiskCheck/ThumbnailStore.cpp
├── Update/Win32UpdateHost.cpp
├── Seams/RawDevice/Win32StorageCardBroker.cpp
└── (machine config readers listed in research R6)

UnitTest/
├── DataFolderResolverTests.cpp          # new
├── MachineConfigSourceTests.cpp         # new
├── MachineConfigUpgradeTests.cpp        # lower-stamp cases
└── MachineScannerTests.cpp              # predicate cases

ScenarioTests/
├── CassoLinkSuperSerialTests.cpp
└── GuestVisibleSuperSerialCardTests.cpp

scripts/
├── FetchRoms.ps1
├── FetchStockDisks.ps1
└── RunBuildLoopGate.ps1

.gitignore                               # CassoData/
.github/workflows/ci.yml                 # stale comment at line 220
CLAUDE.md, docs/testing.md, .github/copilot-instructions.md
```

**Structure Decision**: All logic goes in `CassoEmuCore` beside the code it replaces. `PathResolver::GetLocalAppDataDir` is renamed to `GetDataFolder` and drops its `appName` parameter, since every caller passes `L"Casso"` and the result is no longer always under `%LOCALAPPDATA%`.

## Phases

1. **Resolver**: `DataFolderResolver`, `ModuleInstallEnvironment`, the cached `PathResolver::GetDataFolder`, and the rename across callers. Unit tests for every contract row.
2. **Call sites**: the R5 table, including the crash dump buffer and the scenario tests.
3. **Older builds over newer files**: `MachineConfigSource` across the R6 readers, and the scanner predicate across the R7 callers.
4. **Upgrade backup and comments**: the `Plan` change, its header table and the `ci.yml` comment.
5. **Scripts and docs**: `.gitignore`, the three scripts, `CLAUDE.md`, `docs/testing.md` and the launch guidance.
6. **Validation**: the quickstart checks, the R4 measurement, and the full merge gate before master.

## Risks

- **Every existing checkout starts from defaults.** This is accepted (spec Assumptions). `FetchRoms.ps1` seeds ROMs.
- **A missed call site writes to `%LOCALAPPDATA%\Casso`.** Quickstart check 1 hashes the whole folder, so any stray write appears in the comparison.
- **Tests that today find ROMs under `LOCALAPPDATA`** fail until the checkout has them. Their messages list the folder searched and the fetch command.

## Complexity Tracking

None.
