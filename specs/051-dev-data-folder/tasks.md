# Tasks: Separate data folder for developer builds

**Input**: Design documents from `specs/051-dev-data-folder/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/data-folder-rules.md](contracts/data-folder-rules.md), [quickstart.md](quickstart.md)

**Tests**: Required. FR-018 and SC-004 call for a unit test per contract row and per older-build case, each confirmed to fail when its rule is broken.

**Scope**: No MVP. Every phase ships together in one merge to master.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: The user story the task serves (US1 to US4)

Every new `.cpp` and `.h` goes into its `.vcxproj` and `.vcxproj.filters`. Every new file is checked with `scripts/CheckStyle.ps1 -Mode Staged` before its commit.

---

## Phase 1: Setup

- [x] T001 Merge `origin/master` into `051-dev-data-folder` and build Debug x64 with `scripts/Build.ps1`, so the work starts from the current tree.
- [x] T002 Add `CassoData/` to `.gitignore` at the repo root.

---

## Phase 2: Foundational (the resolver)

Every user story depends on this phase.

- [x] T003 Add `DataFolderInputs`, `DataFolderResult` and `enum class DataFolderRule { ProfileOverride, Official, RepoRoot, BesideModule }` in `CassoEmuCore/Core/DataFolderResolver.h`, with the fields in data-model.md: `profileOverride` ("empty when unset"), `installType` ("`Unknown` is treated as `Developer`"), `modulePath` ("absolute"), `localAppData` ("may be empty when every fallback failed"), `hasGitEntry`.
- [x] T004 Write `UnitTest/EmuTests/DataFolderResolverTests.cpp` with one test per row of contracts/data-folder-rules.md: the override is returned for an official build; `Msix` and `Zip` return `<localAppData>\Casso`; `Developer` and `Unknown` with a `.git` folder return `<root>\CassoData`; a `.git` file (worktree) returns its folder; the nearest `.git` is used over a farther one; no `.git` returns `<module folder>\CassoData`; rules 3 and 4 never return a path under `<localAppData>\Casso`; an official build with empty `localAppData` returns an empty folder.
- [x] T005 Implement `DataFolderResolver::Resolve` in `CassoEmuCore/Core/DataFolderResolver.cpp` as a pure function over `DataFolderInputs`, walking from `modulePath`'s folder to the drive root. Run T004, then break each rule in turn and confirm its test fails.
- [x] T006 [P] Add `ModuleInstallEnvironment` in `CassoEmuCore/Update/ModuleInstallEnvironment.h/.cpp`: an `IInstallEnvironment` whose `GetExecutablePath` returns the path of the module that contains `CassoEmuCore`, from `GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, ...)` and `GetModuleFileNameW`, and whose `HasPackageIdentity` matches `Win32InstallEnvironment`.
- [x] T007 Rename `PathResolver::GetLocalAppDataDir (appName)` to `PathResolver::GetDataFolder()` in `CassoEmuCore/Core/PathResolver.h/.cpp`. It builds `DataFolderInputs` from `CASSO_PROFILE_DIR`, `InstallTypeDetector::Detect` with `ModuleInstallEnvironment` and `AuthenticodeVerifier` (a failed `Detect` counts as `Developer`), the module path, the existing `LOCALAPPDATA` fallbacks, and an `fs::exists` check for `.git`. It caches the result in a function-local static and creates the folder.
- [x] T008 Update every caller of `GetLocalAppDataDir` to `GetDataFolder`: `AssetBootstrap.cpp`, `CassoExplorerShell.cpp`, `PreviewPictureView.cpp`, `RunningCassoRegistry.cpp`, `StockBootDisks.cpp`, `ShellSettings.cpp` and `PathResolver.cpp`, then fix the comments that mention `%LOCALAPPDATA%\Casso` as the only location in `PathResolver.h`, `AssetBootstrap.h/.cpp` and `ShellSettings.h/.cpp`.
- [x] T009 Update `UnitTest/EmuTests/PathResolverTests.cpp` for the rename, and confirm no unit test calls `GetDataFolder` directly (Principle II).

**Checkpoint**: Debug x64 builds; `scripts/RunTests.ps1 -Filter DataFolder` and `-Filter PathResolver` pass.

---

## Phase 3: User Story 1, a developer switches branches (P1)

**Goal**: Each checkout reads and writes only `<repo root>\CassoData\`.

**Independent test**: Quickstart checks 1 and 2.

- [x] T010 [US1] Point `scripts/FetchRoms.ps1`'s default layout at `<repo root>\CassoData\<Subdir>`, and update its help text.
- [x] T011 [P] [US1] Make `scripts/FetchStockDisks.ps1` look for its cache under `<repo root>\CassoData\Disks` in place of `%LOCALAPPDATA%\Casso\Disks`, and update its help text and the comment at line 228.
- [x] T012 [P] [US1] Point `scripts/RunBuildLoopGate.ps1:76` at `<repo root>\CassoData\Disks\DOS 3.3 System Master.dsk`.
- [x] T013 [US1] Run `scripts/FetchRoms.ps1` in this checkout and launch Debug x64 with `--title` per quickstart check 1. Confirm `CassoData\UserPrefs.json` exists and the hash comparison of `%LOCALAPPDATA%\Casso` is empty.

---

## Phase 4: User Story 2, an agent's throwaway test launch (P1)

**Goal**: No developer build touches `%LOCALAPPDATA%\Casso`.

**Independent test**: Quickstart check 1, plus a forced crash dump.

- [x] T014 [US2] In `CassoEmuCore/Gui/GuiMain.cpp`, copy `PathResolver::GetDataFolder() / L"CrashDumps"` into a static `wchar_t` buffer before the unhandled-exception filter is installed, and have the filter at line 566 use that buffer and nothing else.
- [x] T015 [P] [US2] Make `ThumbnailStore::GetDefaultFolder` in `CassoEmuCore/DiskCheck/ThumbnailStore.cpp` return `PathResolver::GetDataFolder() / L"Thumbnails"`.
- [x] T016 [P] [US2] Make `Win32UpdateHost::GetDownloadFolder` in `CassoEmuCore/Update/Win32UpdateHost.cpp` return `<data folder>\Updates`.
- [x] T017 [P] [US2] Make `Win32StorageCardBroker::AppendLog` in `CassoEmuCore/Seams/RawDevice/Win32StorageCardBroker.cpp` write under the data folder at the same relative path.
- [x] T018 [P] [US2] Make the SSC ROM lookups in `ScenarioTests/CassoLinkSuperSerialTests.cpp:105` and `ScenarioTests/GuestVisibleSuperSerialCardTests.cpp:448` use `PathResolver::GetDataFolder()`, with the folder searched and `scripts/FetchRoms.ps1` in the failure message.
- [x] T019 [US2] Search the tree for any remaining `LOCALAPPDATA`, `FOLDERID_LocalAppData` and `AppData\\Local` references in `CassoEmuCore`, `CassoCore`, `ScenarioTests` and `scripts`. Each hit outside the CiderPress2 lookups and `RunHarteTests.ps1` gets a task here or a reason in research.md R5.
- [x] T020 [US2] Launch with `CASSO_PROFILE_DIR` set to a scratch folder and confirm every file the run writes lands there, crash dumps included (trigger one with a debugger break).

---

## Phase 5: User Story 3, an older release over newer files (P2)

**Goal**: An older build boots every machine it supports from a folder a newer build wrote, and doesn't write those files.

**Independent test**: Quickstart check 3.

- [ ] T021 [US3] Write `UnitTest/EmuTests/MachineConfigSourceTests.cpp`: `ChooseText` with a lower disk stamp, an equal stamp and an unstamped file returns the disk text; a higher disk stamp returns the embedded text with `useEmbedded` true; unparseable disk text with a higher legacy `$cassoDefault` stamp is treated like `$cassoMachineVersion`.
- [ ] T022 [US3] Add `MachineConfigSource` in `CassoEmuCore/Core/MachineConfigSource.h/.cpp`: pure `ChooseText (diskText, embeddedText)` using `MachineConfigUpgrade::ParseStamp`, and `ReadConfigText (hInstance, machineName, diskPath, outText)` that reads the file and the embedded resource and never writes. Run T021 and confirm each case fails when its branch is broken.
- [ ] T023 [US3] Classify each reader in research.md R6 as reading the JSON text or building a path only, and record the result in research.md.
- [ ] T024 [US3] Route every text reader from T023 through `MachineConfigSource::ReadConfigText`, starting with `GuiMain.cpp:386` and `MachineManager.cpp:228`.
- [ ] T025 [US3] Add cases to `UnitTest/EmuTests/MachineScannerTests.cpp`: a subfolder whose name fails the `HasEmbeddedConfig` predicate is excluded; a passing one is included with its display name and release year.
- [ ] T026 [US3] Add the `HasEmbeddedConfig` predicate parameter to `MachineScanner::Scan` in `CassoEmuCore/Core/MachineScanner.h/.cpp`, and pass `AssetBootstrap::FindEmbeddedConfig (name) != nullptr` from `GuiMain.cpp:967`, `SettingsMachineCatalog.cpp:214` and `ExplorerCassoTarget.cpp:224`. Expose `FindEmbeddedConfig` from `AssetBootstrap.h` if it isn't public.
- [ ] T027 [US3] Make the `--machine` path in `GuiMain.cpp` check the embedded table before the folder, so a machine the build has no config for returns the existing "Unknown machine" error.
- [ ] T028 [US3] Run quickstart check 3 with a `Machines\` tree from a newer build in a `CASSO_PROFILE_DIR` scratch folder, and confirm the copied files are byte-identical afterward.

---

## Phase 6: User Story 4, upgrades keep hand edits (P3)

**Goal**: An edited config with a lower version is backed up before it's replaced.

**Independent test**: The two acceptance scenarios in spec.md US4.

- [ ] T029 [US4] Add cases to `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp`: a lower stamp whose hash matches a known prior default returns `OverwriteSilent`; a lower stamp whose hash doesn't match returns `BackupAndReplace`.
- [ ] T030 [US4] In `MachineConfigUpgrade::Plan` (`CassoEmuCore/Core/MachineConfigUpgrade.cpp`), send a lower positive stamp through the known-hash check in place of returning `OverwriteSilent`. Run T029 and confirm the second case fails without the change.
- [ ] T031 [US4] Correct the decision table in the `Plan` header comment so a missing stamp or a stamp of zero goes through the hash check, and a lower stamp does too.

---

## Phase 7: Polish and cross-cutting

- [ ] T032 [P] Correct the `env` comment at `.github/workflows/ci.yml:220`: the release job fails an unsigned release at line 447.
- [ ] T033 [P] Document the data folder rules in `docs/testing.md`, the launch guidance in `.github/copilot-instructions.md` (Workspace Hygiene), and `CLAUDE.md` Hazards: where a developer build's data lives, that test launches don't require a prefs backup, and that a new worktree gets its ROMs from `scripts/FetchRoms.ps1`.
- [ ] T034 Measure `PathResolver::GetDataFolder` per research.md R4 on Debug and Release, on a signed zip copy and on an unsigned build, and add the numbers to `specs/051-dev-data-folder/validation.md`. If Release is over 50 ms, report it to the owner before changing anything.
- [ ] T035 Add a CHANGELOG entry under `[Unreleased]`, `### Changed`: developer builds store their settings in the checkout, and an older release starts over files a newer release wrote. Get the owner's approval of the text before pushing.
- [ ] T036 Run quickstart checks 1 to 4 and record the results in `specs/051-dev-data-folder/validation.md`.
- [ ] T037 Run `scripts/CheckStyle.ps1 -Mode Tree` and `scripts/RunMergeGate.ps1` before merging to master.

---

## Dependencies

- Phase 1 comes first; Phase 2 blocks every story.
- US1 and US2 depend only on Phase 2 and can run side by side.
- US3 depends on Phase 2, and is independent of US1 and US2.
- US4 depends only on Phase 1; it touches `MachineConfigUpgrade.cpp` alone.
- Phase 7 comes last; T037 is the final step.

## Parallel examples

- After Phase 2: T015, T016, T017 and T018 touch separate files.
- US1: T011 and T012 touch separate scripts.
- US4 (T029 to T031) can run beside US3 once Phase 1 is done.
- Phase 7: T032 and T033 touch separate files.

## Implementation strategy

No MVP: all phases land in one merge. Work in phase order, build and run the touched tests per phase, and run the full gate (T037) once, right before master.
