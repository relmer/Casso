# Validation: Separate data folder for developer builds

Measured 2026-11-18 on the 051-dev-data-folder branch, Debug and Release x64. Every launch passed `--title` and started minimized. Each exited through File > Exit, or through the window close message when a dialog stopped startup.

## Startup cost of the signature check (SC-005)

Method as in research R4, with a temporary `QueryPerformanceCounter` bracket around `PathResolver::ChooseDataFolder` and around `InstallTypeDetector::Detect`. The bracket is not committed. Five launches of `Casso.exe` per configuration, run one after another.

| Build | Whole choice | `Detect` on this unsigned build |
|---|---|---|
| Debug | 4.4 to 4.8 ms | 2.1 to 2.4 ms |
| Release | 4.5 to 4.9 ms | 2.3 to 2.5 ms |

No signed Casso copy is installed on this machine, so the signed path is measured with a substitute: the same `AuthenticodeVerifier::IsOfficialFile` call on a Microsoft-signed `MSBuild.exe`. It runs the same `WinVerifyTrust` chain check and then fails the publisher comparison.

| Build | `IsOfficialFile` on a signed file |
|---|---|
| Debug | 17.7 to 20.7 ms |
| Release | 18.6 to 20.5 ms |

The whole choice for a signed copy is the signed-file figure plus about 2 ms for the rest, so about 22 ms on Release. That is under the 50 ms budget, and the check is as it was. The first of the five runs took the same time as the later ones, so a cold run after a reboot is not measured.

## Quickstart check 1: a developer build leaves the shipped folder alone (SC-001)

`Casso.exe --machine Apple2e --disk1 Disks\Apple\dos33-master.dsk` from this checkout booted the //e with the disk and exited through File > Exit. The hashes of every file under `%LOCALAPPDATA%\Casso` before and after are equal. `CassoData\UserPrefs.json` changed (`lastSelectedMachine`, `disk1Path`, the disk MRU).

An earlier launch of the same build with a fresh `CassoData\` stopped at the startup download dialog, as a first launch does. It wrote only under `CassoData\`, and the `%LOCALAPPDATA%\Casso` hashes are equal afterward.

Another session's `Casso.exe` (a 035 build, started before this work) runs on this machine and writes `%LOCALAPPDATA%\Casso\UserPrefs.json` and its debug project files. A hash comparison over a longer window includes those writes. None of the changed content refers to this checkout.

A launch with `CASSO_PROFILE_DIR` set to a scratch folder wrote every file there. A crash dump forced with a remote thread at an invalid address landed in `<scratch>\CrashDumps`. The hashes of `%LOCALAPPDATA%\Casso` and of the checkout's `CassoData\` are equal to those before the launch.

## Quickstart check 2: two checkouts (SC-002)

No older build that has this change exists, and an older build run now would write the shipped folder, so the check ran two checkouts of this build. The second checkout is a scratch folder with a `.git` file and a copy of `Casso.exe` under `x64\Debug`. Each checkout booted `--machine Apple2e` and then `--machine Apple2cPlus` with `--disk1`.

- Both machines booted in both checkouts.
- The hashes of this checkout's `CassoData\` are equal before and after the other checkout ran, and the reverse.
- The runs in the second checkout changed files under its `CassoData\` only.

The older-build side of SC-002 is the unit tests for the embedded text choice and the machine list filter, and check 3.

## Quickstart check 3: an older build over newer files (SC-003)

A scratch `CASSO_PROFILE_DIR` contained a `Machines\` tree with a //e config stamped 99 whose Liron ROM file doesn't exist, and a `Machines\Apple2xNew\` folder this build has no config for.

- `--machine Apple2e` booted to the boot disk dialog, so the session used the embedded config. The disk text would have failed on the missing ROM.
- The JSON files under `Machines\` have the same hashes afterward.
- `--machine Apple2xNew` stopped with the message "Unknown machine 'Apple2xNew'. This build has no configuration for it."
- The machine list filter is covered by `MachineScannerTests`. The list was not read on screen.

## Quickstart check 4: unit tests (SC-004)

| Filter | Tests | Passed |
|---|---|---|
| `DataFolder` | 17 | 17 |
| `MachineConfig` | 108 | 108 |
| `MachineScanner` | 16 | 16 |
| `PathResolver` | 5 | 5 |

Each rule was broken in turn and its test failed: nine changes to `DataFolderResolver::Resolve`, four to `MachineConfigSource::ChooseText`, one to `ParseStamp`, two to the scanner predicate, and the restored early exit in `MachineConfigUpgrade::Plan`. The two scenario files that read `SSC.rom` passed with the ROM in `CassoData\`. With the ROM removed they failed, and the message includes the folder searched.
