# Contract: Data folder rules

The rules below are the contract that `DataFolderResolver::Resolve` implements and that the unit tests check row by row.

## Inputs

| Input | Source in production |
|---|---|
| `profileOverride` | `CASSO_PROFILE_DIR`, empty when unset |
| `installType` | `InstallTypeDetector::Detect` on the module from research R1; `Developer` when it fails |
| `modulePath` | full path of the module that contains `CassoEmuCore` |
| `localAppData` | `SHGetKnownFolderPath (FOLDERID_LocalAppData)` with the existing fallbacks |
| `hasGitEntry (folder)` | true when `<folder>\.git` exists as a file or a folder |

## Rules, first match wins

| # | Condition | Data folder |
|---|---|---|
| 1 | `profileOverride` is not empty | `profileOverride` |
| 2 | `installType` is `Msix` or `Zip` | `<localAppData>\Casso` |
| 3 | some folder from the module's folder upward has `hasGitEntry`; the nearest one is the repo root | `<repo root>\CassoData` |
| 4 | otherwise | `<module folder>\CassoData` |

## Guarantees

- One process resolves once and every caller receives the same path.
- Rules 3 and 4 never return a path under `<localAppData>\Casso`.
- A failure to create the folder is reported by the caller in the same way a `%LOCALAPPDATA%` failure is reported today; the resolver doesn't fall back to another rule.
- The layout below the data folder is the same for every rule.

## Machine config text

| Disk stamp compared with embedded | Text used for this session | File written |
|---|---|---|
| no file | embedded, after extraction | yes, extracted |
| lower or equal | disk | per `MachineConfigUpgrade::Plan` |
| higher | embedded | no |

## Machine list

A subfolder of `Machines\` appears in the list only when the build has an embedded config with that machine name.
