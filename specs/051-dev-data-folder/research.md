# Research: Separate data folder for developer builds

## R1: Which binary the folder rule starts from

**Decision**: Start from the module that contains the resolver code, found with `GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, ...)` on an address inside `CassoEmuCore`, not from the process exe. Check that same module's signature.

**Rationale**: For `Casso.exe`, `CassoExplorer.exe`, `CassoCli.exe` and `CassoStorageCardBroker.exe` the core library is linked into the exe, so the module is the exe and nothing changes. The unit and scenario suites run inside `testhost.exe` from the Visual Studio install. A rule based on the process exe would treat that folder as "no repo found" and create `CassoData\` under Program Files. The test DLLs sit in the checkout's output folder, so starting from the module resolves to the checkout's folder, the same one the build under test uses.

**Alternatives considered**: The process exe path, which `Win32InstallEnvironment::GetExecutablePath` returns today, puts the scenario suite in the wrong place. A test-only override can be forgotten, and it adds a second path through the code.

## R2: Reuse of the install type detection

**Decision**: Use `InstallTypeDetector::Detect` with a new `IInstallEnvironment` implementation that returns the module path from R1. `Msix` and `Zip` count as official, and `Developer` doesn't. A failed `Detect` counts as `Developer`.

**Rationale**: The updater, the shell extension registration and the storage card broker already rely on this rule. CI checks that `AuthenticodeVerifier::kpszOfficialPublisher` matches the signing certificate, and the release job fails an unsigned release (`ci.yml:447`). A second definition of "official" could disagree with the updater about the same copy.

**Alternatives considered**: A compile-time define set by CI. It isn't used because the release binaries are the CI build-and-test outputs, so the define would mark every CI artifact as official, and a signature already exists that can't be faked by a local build.

## R3: Caching and threads

**Decision**: Compute the folder once, on first use, behind a function-local static (C++ guarantees one thread initializes it). `PathResolver::GetDataFolder` returns the cached path. `GuiMain` calls it before the crash handler is installed and copies the result into a static `wchar_t` buffer for the handler.

**Rationale**: Callers from the UI thread, the update worker and the CassoLink threads all arrive at different times, and one process must never see two folders. An unhandled-exception filter must not run a signature check or allocate, so the dump folder is computed ahead of time.

## R4: Startup cost of the signature check

**Decision**: Measure `Detect` in Debug and Release with a `QueryPerformanceCounter` bracket on a signed zip copy and an unsigned build, five cold and five warm runs each, and add the numbers to `validation.md`. The budget is 50 ms on Release (SC-005).

**Rationale**: `AuthenticodeVerifier` already disables revocation (`WTD_REVOKE_NONE`) and network retrieval (`WTD_CACHE_ONLY_URL_RETRIEVAL`), so the check reads only local data. The unsigned case returns quickly, since there is no signature to verify. The cold signed case is the one to measure.

**If over budget**: Check package identity first, which costs nothing and settles the MSIX. For a zip copy, an unsigned exe exits early. Report the measured cost to the owner before changing the check itself.

## R5: Where each direct `LOCALAPPDATA` read goes

| Call site | Today | Change |
|---|---|---|
| `GuiMain.cpp:566` crash dumps | `LOCALAPPDATA\Casso\CrashDumps` | `<data>\CrashDumps`, from the precomputed buffer (R3) |
| `ThumbnailStore::GetDefaultFolder` | `LOCALAPPDATA\Casso\Thumbnails` | `<data>\Thumbnails` |
| `Win32UpdateHost::GetDownloadFolder` | `LocalAppData\Casso\Updates` | `<data>\Updates` |
| `Win32StorageCardBroker::AppendLog` | `LocalAppData\...` | `<data>\...`, same relative path |
| `CassoLinkSuperSerialTests.cpp:105`, `GuestVisibleSuperSerialCardTests.cpp:448` | `LOCALAPPDATA\Casso\Devices\...\SSC.rom` | `PathResolver::GetDataFolder()`, with the folder searched in the failure message |
| `scripts/FetchStockDisks.ps1:237` | cache lookup in `LOCALAPPDATA\Casso\Disks` | `<repo root>\CassoData\Disks` |
| `scripts/RunBuildLoopGate.ps1:76` | `LOCALAPPDATA\Casso\Disks\DOS 3.3 System Master.dsk` | `<repo root>\CassoData\Disks\...` |
| `scripts/FetchRoms.ps1` (default layout) | `<repo root>\<Subdir>`, which no build reads | `<repo root>\CassoData\<Subdir>`, so it seeds a checkout |

The broker runs elevated and may run under another account, so `LOCALAPPDATA` there was never the user's folder. With R1 it uses the folder for the broker's exe, which sits beside `Casso.exe`.

**Found by the tree search**: `GuestSession::FindStockImage` in `UnitTest/EmuTests/GuestSession.cpp` looked for the stock disks in `LOCALAPPDATA\Casso\Disks`. It now looks in `<data folder>\Disks`, and its failure messages include the folder searched. `scripts/FetchRoms.ps1` has a row for `SSC.rom` that only the default layout uses, since the emulator downloads it when the card is turned on and the unit suite doesn't read it.

**Left as is**: the CiderPress2 lookups in `ArchiveTests.cpp`, `Dos32Tests.cpp` and `LargeVolumeTests.cpp`, and the CiderPress2 and MAME defaults in `scripts/FetchCiderPressTestFiles.ps1` and `scripts/MameSnapshot.ps1`, which locate an installed tool, not Casso data. `scripts/RunHarteTests.ps1` uses `LOCALAPPDATA\Casso\HarteTests` as a large shared test cache that no build reads at runtime; moving it would copy the full vector set into every checkout.

## R6: Readers of a machine config on disk

Every reader goes through one core function, `MachineConfigSource::ReadConfigText`, that returns the disk text when its `$cassoMachineVersion` is equal to or lower than the embedded one, and the embedded text when the disk stamp is higher. It never writes the file.

| Reader | Location |
|---|---|
| Boot | `GuiMain.cpp:386` |
| Machine switch | `MachineManager.cpp:228` |
| Boot check | `DiskCheck/BootCheck.cpp:54` |
| Explorer target | `ExplorerCassoTarget.cpp:226` |
| ROM offer | `AssetBootstrap.cpp:2955` |
| Disk settings | `Config/DiskSettings.cpp:62` |
| Settings catalog | `Ui/Settings/SettingsMachineCatalog.cpp:129` |
| Machine host | `Shell/MachineHost.cpp:302` |
| Config loader | `Core/MachineConfig.cpp:433`, `519`, `1150` |

The tasks phase confirms which of these read the JSON text and which only build a folder path; only the text readers change.

## R7: Machine list filter

`MachineScanner::Scan` is called from `GuiMain.cpp:967`, `SettingsMachineCatalog.cpp:214` and `ExplorerCassoTarget.cpp:224`. Add a predicate parameter, `HasEmbeddedConfig`, that the scanner applies to each subfolder name, so the three callers share one rule and the pure scanner stays testable with a fake predicate. The production predicate is `AssetBootstrap::FindEmbeddedConfig (name) != nullptr`.

`--machine` with a name the build has no embedded config for already fails through `FindFile`. With the folder present, the boot path calls `LoadEmbeddedJson`, which fails. The boot path checks the embedded table first and reports the existing "Unknown machine" error.

## R8: Lower-version configs with edits

`MachineConfigUpgrade::Plan` returns `OverwriteSilent` for any `0 < disk < embedded` stamp. Change that branch to fall through to the known-hash check: a match is overwritten silently, and no match returns `BackupAndReplace`. Correct the header table at the same time: a missing stamp or a stamp of zero goes through the hash check, as the code does today.
