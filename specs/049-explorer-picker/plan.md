# Implementation Plan: Casso Explorer as the insert disk picker

**Branch**: `049-explorer-picker` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/049-explorer-picker/spec.md`

## Summary

Replace the insert disk dialog and the Windows file pickers for disk and tape images with Casso Explorer in a picker mode. The work has five parts: an image classifier that reads a file's extension, length and first 512 bytes; a drive rule used by the picker, by drops and by tests of the mount code; a picker window that is a `CassoExplorerWindow` hosted in the emulator's process with no session and no links; new columns and a Casso node that lists images by most recent use; and the removal of the old dialog.

Design decisions and their reasons are in [research.md](research.md). Types are in [data-model.md](data-model.md). The interfaces other code calls are in [contracts/](contracts/).

## Technical Context

**Language/Version**: C++ (MSVC v145, VS 2026), `/std:c++latest` as the solution sets it

**Primary Dependencies**: Win32, Dxui (the project's UI library), Ehm (error handling macros); no new third-party dependency

**Storage**: JSON preferences through `IFileSystem`: `GlobalUserPrefs` (the recent list) and a new picker preferences file separate from `CassoExplorerPrefs.json`

**Testing**: Microsoft Native CppUnitTest in `UnitTest`; real-file checks in `ScenarioTests`

**Target Platform**: Windows 10 and 11, x64 and ARM64 (ARM64 is build-only on this machine)

**Project Type**: Desktop application; all new logic in the `CassoEmuCore` static library

**Performance Goals**: A folder of 500 images listed in under 1 s, both new columns filled in under 2 s on a first visit and under 0.5 s on a return (SC-003)

**Constraints**: One short read per file for classification (FR-002). The picker never loads Casso Explorer's session, never constructs `CassoLinkConnections`, and never opens a serial port (FR-031). The `.cpp` size limit applies (CS0026).

**Scale/Scope**: Collections of hundreds to a few thousand images; a recent list of 1,000 entries; seven drive models plus the recorder

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | In this plan |
|---|---|---|
| I. Code Quality | Pass | EHM in every function with a failable call, top-of-function declarations, no anonymous namespaces, one class per `.h`/`.cpp` pair. New classes are listed under Project Structure. |
| II. Testing Discipline | Pass | Every decision in research.md R12 has a unit test over in-memory bytes or fakes. Real-file agreement is in the scenario suite. No test touches the real file system, registry or a serial port. Each new test is checked against a stubbed implementation. |
| III. User Experience Consistency | Pass | The picker is the existing Casso Explorer window, so lists, columns, sorting, themes and keyboard handling match the file manager. Labels are sentence case. |
| IV. Performance Requirements | Pass | Header-only classification with a cache; heavier columns fill on a worker; the Casso node's list builds off the UI thread. |
| V. Simplicity & Maintainability | Pass | One classifier, one rule, one row filter and one command gate. The four tag branches in `OnFileDropped` are replaced by one lookup. The old dialog is removed. |
| VI. Thin Executable, Testable Core | Pass | Nothing is added to `Casso/` or `CassoExplorer/`. The classifier, rule, scope, host and shell component are all in `CassoEmuCore` and linked by `UnitTest`. |

No violations, so Complexity Tracking is empty.

**Post-design re-check**: the design does not add a project, a dependency or code in an executable. One point to watch during implementation: `EmulatorShell` files are large already, so the drive picker logic goes in a new component (`ShellDrivePickers`), not into `ShellDisks.cpp` or `EmulatorShellStorage.cpp`.

## Project Structure

### Documentation (this feature)

```text
specs/049-explorer-picker/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── image-classifier.md
│   ├── drive-image-rule.md
│   ├── picker-request.md
│   └── picker-mode.md
├── checklists/requirements.md
└── tasks.md             # written by /speckit-tasks
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Devices/Disk/
│   ├── ImageClassifier.h/.cpp          # new: ImageClass, ClassifyHeader, Classify, FormatSize
│   ├── ImageClassCache.h/.cpp          # new: results keyed by path, length and modified time
│   └── DriveImageRule.h/.cpp           # new: ReceivingDrive, Accepts, GetAcceptedText
├── Ui/Scene/DeskDrivePlan.h/.cpp       # add GetReceivingKind (DeskDriveModel)
├── Seams/
│   ├── IHostDialogs.h                  # FileDialogSpec: content, receivingDrive
│   └── CassoHostDialogs.h/.cpp         # new: image requests to the picker, the rest to Win32HostDialogs
├── Shell/
│   ├── DiskMru.h/.cpp                  # capacity 1,000, kMenuEntries
│   ├── EmulatorShellStorage.cpp        # TryGetReceivingDrive, OnFileDropped through the rule
│   ├── WindowCommandManager.cpp        # remove PromptInsertDiskMru, PromptInsertTapeMru, PromptForDiskImage
│   └── Components/
│       ├── ShellDrivePickers.h/.cpp    # new: one picker per drive, door callbacks, close on machine switch
│       ├── ShellWindow.cpp             # OverFn for the drop cursor
│       ├── ShellDisks.cpp              # BrowseForDisk calls ShellDrivePickers
│       ├── ShellDeskScene.cpp          # BrowseFloppy35 removed; callers open the drive's picker
│       ├── ShellChromeDriveBar.cpp     # opens the drive's picker
│       └── ShellTapeDeck.cpp           # opens the recorder's picker
├── Ui/Settings/SettingsSheet.cpp       # Hardware page browse: content = Images, receivingDrive from the port
└── CassoExplorer/
    ├── Picker/
    │   ├── PickerRequest.h             # new: title, folder, filters, receiving drive, callbacks
    │   ├── FileMaskSet.h/.cpp          # new: "*.po;*.2mg" matching
    │   ├── PickerScope.h/.cpp          # new: the row filter (masks and drive rule)
    │   ├── ExplorerPickerHost.h/.cpp   # new: ICassoExplorerHost with no session and no links
    │   └── PickerPrefs.h/.cpp          # new: view per request kind; last folder for a request with no drive
    ├── Model/
    │   ├── CatalogModel.h/.cpp         # columns FormFactor, FormattedSize, LastUsed; ImageClass on the row
    │   ├── BrowserModel.h/.cpp         # row filter hook; Casso root lists images
    │   ├── CassoImagesListing.h/.cpp   # new: the Casso node's list
    │   ├── StockDisksListing.h/.cpp    # new: the "Stock disks" node's list in a drive picker
    │   └── ImageVolumeFacts.h/.cpp     # new: file system and volume name per image, with its worker
    ├── CassoExplorerCommands.h/.cpp    # picker allowlist
    ├── CassoExplorerBrowser.cpp        # single-folder special case of the Casso node removed
    └── Window/
        ├── ExplorerContext.h           # PickerRequest in CassoExplorerWindowContext
        ├── Managers/ExplorerPickerMode.h/.cpp   # new: mode state, confirm, cancel
        └── Panes/ExplorerPickerBar.h/.cpp       # new: header, filter choice, create, Insert, Cancel

UnitTest/
├── DiskTests/ImageClassifierTests.cpp, DriveImageRuleTests.cpp, DriveRuleMountAgreementTests.cpp
├── CassoExplorerTests/FileMaskSetTests.cpp, PickerScopeTests.cpp, PickerCommandGateTests.cpp,
│                      ExplorerPickerHostTests.cpp, CassoImagesListingTests.cpp, PickerColumnTests.cpp
├── EmuTests/DropRuleTests.cpp, DrivePickersTests.cpp, DiskMruTests.cpp (extended)
└── SeamTests/CassoHostDialogsTests.cpp

ScenarioTests/
└── ImageClassifierScenarioTests.cpp    # classification equals the mount's report over the real fixtures
```

Test files go in the folders `UnitTest/` already has; a task confirms each folder before the file is added.

**Structure Decision**: Everything is in `CassoEmuCore`, in the folders that already have the neighboring code. The classifier and rule are in `Devices/Disk` because the mount code and Casso Explorer both depend on that layer and neither depends on the other. Picker-specific Casso Explorer code is in a new `CassoExplorer/Picker/` folder so the file manager's files change only where a hook is required.

## Build order

The parts depend on each other in one direction, so they are built and committed in this order. Nothing ships until all of them are done.

1. **Classifier and cache** (R1). Nothing else depends on it yet.
2. **Drive rule, the `DeskDriveModel` mapping and the mount agreement test** (R2).
3. **Drops** (R3): `TryGetReceivingDrive`, the cursor and `OnFileDropped`. The first user-visible change, and independent of Casso Explorer.
4. **Columns** (R7): `FormFactor` and `FormattedSize` in folder listings, then file system and volume name.
5. **Recent list capacity and the Casso node's list** (R8, R9), with `LastUsed`.
6. **Picker core** (R4, R5): `FileMaskSet`, `PickerScope`, the command allowlist, `ExplorerPickerHost`, picker prefs, the picker bar.
7. **Drive pickers** (R6): `ShellDrivePickers`, door callbacks, create new disk, blank tape, the stock disks node.
8. **Modal form behind the seam** (R6): `FileDialogSpec` fields, `CassoHostDialogs`, the Settings Hardware page.
9. **Removal** (R10) of the old dialog and pickers, once every caller is moved.
10. **Validation**: scenario test over real fixtures, the window checks in quickstart.md, CHANGELOG and README, then merge 048 again and run the merge gate.

## Risks

| Risk | Effect | Handling |
|---|---|---|
| Keys go to the wrong window with a second top-level window in the emulator's loop | Typing in the picker is sent to the guest, or emulator shortcuts fire | Verified first in step 6 (research.md R4); the picker is built only after the loop is confirmed or fixed |
| `CassoExplorerWindow` assumes non-null services in places the context documents as optional | A null dereference in picker mode | `ExplorerPickerHostTests` opens a window model with every optional pointer null; each crash found is a guard added in the component, not a fake service |
| 048 changes `DeskDriveModel`, the drop tags or the drive bar while this is in progress | Merge conflicts in shell components | Shell edits are confined to call sites; `ShellDrivePickers` is a new file. 048 is merged at steps 3, 7 and 10 |
| `IHostDialogs.h` is also changed on `035-debugger` (adds `NullHostDialogs`) | Textual conflict when 035 lands | Only `FileDialogSpec` gains fields; the interface's methods are not changed |
| Classification of the UniDisk 3.5 differs from the device | The picker lists an image the drive fails to mount | The mount agreement test fails first; the rule is corrected to the device |
| 1,000 recent entries pruned for missing files on the UI thread | A pause when a picker opens | Pruning moves off the UI thread or to displayed rows (research.md R9) |

## Complexity Tracking

No violations.
