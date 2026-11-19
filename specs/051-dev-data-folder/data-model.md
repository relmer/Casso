# Data model: Separate data folder for developer builds

## DataFolderInputs

A plain struct passed to `DataFolderResolver::Resolve`. Fields are listed in [contracts/data-folder-rules.md](contracts/data-folder-rules.md).

- `profileOverride`: `std::wstring`, empty when unset.
- `installType`: `InstallType` (`Unknown`, `Msix`, `Zip`, `Developer`). `Unknown` is treated as `Developer`.
- `modulePath`: `fs::path`, absolute.
- `localAppData`: `fs::path`, may be empty when every fallback failed.
- `hasGitEntry`: `std::function<bool (const fs::path &)>`.

## DataFolderResult

- `folder`: `fs::path`, empty only when rule 2 applies and `localAppData` is empty.
- `rule`: enum `DataFolderRule` (`ProfileOverride`, `Official`, `RepoRoot`, `BesideModule`), for logging and tests.

## MachineConfigChoice

Returned by `MachineConfigSource::ChooseText`, the pure half of `ReadConfigText`.

- `useEmbedded`: `bool`, true when the disk stamp is higher than the embedded stamp.
- `diskStamp`, `embeddedStamp`: `int`, for logging.

## State changes

- The data folder is resolved once per process and doesn't change afterward.
- A machine config file changes only through `EnsureMachineConfigs`. Choosing the embedded text for a session never writes the file.
- `MachineConfigUpgrade::Plan` gains one path: a lower stamp whose content doesn't match any known default returns `BackupAndReplace` in place of `OverwriteSilent`.
