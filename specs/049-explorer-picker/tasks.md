# Tasks: Casso Explorer as the insert disk picker

**Input**: Design documents from `/specs/049-explorer-picker/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. Every test of new API is checked against a stubbed implementation and must fail there before the task is marked done.

**Organization**: Phases follow plan.md's build order, which is dependency order, not priority order. User Story 1 (P1) is built late because it depends on the classifier, the rule, the columns, the Casso node's list and the picker core. There is no MVP: the branch is reported done only when every phase is complete.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: The user story the task serves (US1 to US5)
- Paths are relative to the repository root

## Rules for every task

- Read `.github/copilot-instructions.md` first. EHM in every function with a failable call, declarations at the top, column alignment, five blank lines between top-level constructs, no anonymous namespaces, no angle-bracket includes outside `Pch.h`.
- New files: add to `CassoEmuCore.vcxproj` / `UnitTest.vcxproj` and their filters, and run `scripts\CheckStyle.ps1 -Mode Staged` before the commit.
- Insert a new function ahead of the next function's `////` banner, never between a banner and its signature.
- Functions stay short, per the constitution's function size rule; a `.cpp` that approaches the size limit gets a component class, not a longer file.
- Commits: subject only, `type(scope): Imperative ...`, checked with `~/.claude/scripts/Check-Prose.ps1 -Commit`, no AI attribution. Push after each checkpoint.
- Never start the owner's Casso Explorer and never open a serial port. Window checks follow quickstart.md's hard rules.
- Touch 048's drive bar and desk files only at the call sites listed here.

---

## Phase 1: Setup

- [ ] T001 Confirm the fixtures are present by running `scripts\RunTests.ps1 -Build -Filter BlockDeviceMedia` and record the result; if the suite stops for missing ROMs, run `scripts\FetchRoms.ps1 -Fixtures` and `scripts\FetchCiderPressTestFiles.ps1 -Extract`
- [ ] T002 Create the folder `CassoEmuCore/CassoExplorer/Picker/` and its filter entries in `CassoEmuCore/CassoEmuCore.vcxproj.filters`
- [ ] T003 [P] Verify (research.md R1): read `CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp` `Describe` and record in `specs/049-explorer-picker/research.md` whether it returns the INFO disk type and side count from a 512-byte buffer; if it does not, add a task note that T010 factors the INFO read into a function both call
- [ ] T004 [P] Verify (research.md R1): list every extension accepted by `DiskImageStore::IsMountableImageExtension`, `BlockImageFile`, `BlockDeviceMedia` and `TapeImageLoader::IsTapeFileExtension` and record the list in `specs/049-explorer-picker/research.md`
- [ ] T005 [P] Verify (research.md R2): read `CassoEmuCore/Devices/Floppy35/UniDisk35Drive.cpp` and its wiring and record in `specs/049-explorer-picker/research.md` whether the UniDisk 3.5 mounts 400K images and 3.5-inch WOZ images, and the maximum block count each of the HD20SC and the ProFile mounts; if a finding contradicts FR-011 or FR-012, update spec.md before T013
- [ ] T006 [P] Verify (research.md R7): read `CatalogModel::GetColumnId`, `TryGetColumn` and `GetDefaultOrder` in `CassoEmuCore/CassoExplorer/Model/CatalogModel.cpp` and record in `specs/049-explorer-picker/research.md` whether every stored column id has the same value after columns are appended after `Sha256`
- [ ] T007 [P] Verify (research.md R9): find every reader of `GetRecentMedia`, `DiskMru` and the `recentDisks` / `recentDiskLoadedAt` preferences and record each in `specs/049-explorer-picker/research.md` with whether it displays the list, prunes it or seeds folders from it

---

## Phase 2: Foundational (classifier and drive rule)

**Purpose**: The two pieces every story depends on. No story work starts before this phase is complete.

### Tests

- [ ] T008 [P] Write `UnitTest/Devices/ImageClassifierTests.cpp`: one case per row of research.md R1's table (WOZ 5.25, WOZ 3.5 one side and two sides, 2IMG ProDOS order, 2IMG DOS order, 2IMG nibble, DiskCopy 4.2 at 400K and 800K, `.nib`, `.d13`, `.dsk` / `.do` / `.po` at 143,360 bytes, raw 800 and 1,600 blocks, a raw hard disk size, each tape extension); a truncated `.dsk`, a zero-length `.po`, a WOZ with a damaged signature and a 2IMG with an inconsistent length are `Unclassified`; a `.txt` is `NotAnImage`; `status != Classified` implies `formFactor == None` and `formattedBytes == 0`; `FormatSize` returns "140K", "400K", "800K", "113K" and megabytes
- [ ] T009 [P] Write `UnitTest/Devices/DriveImageRuleTests.cpp`: every cell of the table in contracts/drive-image-rule.md, an unclassified image fails every kind, a hard disk image one block over `maxBlocks` fails, one block under `BlockDeviceCatalog::kMinHardDiskBlocks` fails and exactly that count passes, and a sweep of every `DeskDriveModel` value through `DeskDrivePlan::GetReceivingKind`

### Implementation

- [ ] T010 Add `ImageClass`, `ImageStatus`, `ImageFormFactor`, `ImageContainer` and `ImageClassifier` (`ClassifyHeader`, `Classify`, `FormatSize`, `FormatFormFactor`, `IsImageExtension`, `kHeadBytes = 512`) in `CassoEmuCore/Devices/Disk/ImageClassifier.h` and `.cpp` per contracts/image-classifier.md; floppy block counts come from `BlockDeviceMedia::IsFloppyBlockCount`
- [ ] T011 Add an extension-sweep case to `UnitTest/Devices/ImageClassifierTests.cpp` asserting `IsImageExtension` is true for every extension in T004's list
- [ ] T012 [P] Add `ImageClassCache` (key: path, length, modified time; thread-safe; no cap) in `CassoEmuCore/Devices/Disk/ImageClassCache.h` and `.cpp`, with `UnitTest/Devices/ImageClassCacheTests.cpp`: a hit, a replaced entry after a length change, and a replaced entry after a time change
- [ ] T013 Add `ReceivingDriveKind`, `ReceivingDrive` and `DriveImageRule` (`Accepts`, `GetAcceptedText`) in `CassoEmuCore/Devices/Disk/DriveImageRule.h` and `.cpp` per contracts/drive-image-rule.md, using T005's finding for `UniDisk35`
- [ ] T014 Add `DeskDrivePlan::GetReceivingKind (DeskDriveModel)` in `CassoEmuCore/Ui/Scene/DeskDrivePlan.h` and `.cpp`, as a switch with no `default:` arm
- [ ] T015 Write `UnitTest/BlockDeviceTests/DriveRuleMountAgreementTests.cpp`: for one in-memory fixture per image class and each device class (Disk II through `DiskImageStore`, `Sony35Drive`, the UniDisk 3.5, HD20SC and ProFile through `StorageCardWiring`, the tape loader), mount success for form factor and size equals `DriveImageRule::Accepts`; assert a non-zero count of pairings checked
- [ ] T016 Stub `ClassifyHeader` to return a constant and `Accepts` to return true, confirm T008, T009 and T015 fail, restore, touch the restored files' timestamps, rebuild, and confirm they pass

**Checkpoint**: classifier and rule tested; commit and push.

---

## Phase 3: User Story 3 - A drop follows the same rule (Priority: P2)

**Goal**: The no-drop cursor over a drive that does not accept the dragged image, and no mount on such a drop.

**Independent Test**: Drag one image of each form factor over each drive kind and the recorder; cursor and result match the rule in every pairing.

### Tests

- [ ] T017 [P] [US3] Write `UnitTest/EmuTests/DropRuleTests.cpp`: `EmulatorShell::TryGetReceivingDrive` for tags 0 and 1, the tape tag, a desk extra at `ShellDeskScene::kExtraDropTagBase`, a drive bar entry at `kBarDropTagBase`, and a tag with no drive; `OnFileDropped` with an 800K image on tag 0 mounts nothing and ejects nothing; with a 140K image on tag 0 it mounts; a tape on a disk tag does nothing; a drop with no drive under it mounts in the existing target drive only when the rule accepts the image there (FR-018)

### Implementation

- [ ] T018 [US3] Add `DriveAddress` (slot, drive, `isRecorder`, equality, conversion from `DeskDrive`) in `CassoEmuCore/Shell/Components/ShellDrivePickers.h` (the class itself is added in Phase 7)
- [ ] T019 [US3] Add `EmulatorShell::TryGetReceivingDrive (int dropTag, ReceivingDrive & outDrive, DriveAddress & outAddress)` in `CassoEmuCore/Shell/EmulatorShell.h` and `CassoEmuCore/Shell/EmulatorShellStorage.cpp`, covering the tag ranges that `OnFileDropped` branches on today
- [ ] T020 [US3] Rewrite `EmulatorShell::OnFileDropped` in `CassoEmuCore/Shell/EmulatorShellStorage.cpp` to classify the path, resolve the tag through `TryGetReceivingDrive`, return without a mount when `DriveImageRule::Accepts` is false, and mount by `DriveAddress` otherwise
- [ ] T021 [US3] Pass an `OverFn` through `SetDataHandlers` in `ShellWindow::InstallDragDropTarget` (`CassoEmuCore/Shell/Components/ShellWindow.cpp`): one file only, classification cached per drag by path, `DROPEFFECT_COPY` when the rule accepts and `DROPEFFECT_NONE` otherwise, including several files and a file that is not an image (FR-019)
- [ ] T022 [US3] Stub `TryGetReceivingDrive` to return false, confirm T017 fails, restore and confirm it passes
- [ ] T023 [US3] Window check per quickstart.md "Drops" with a test copy of the emulator: capture the cursor result over a Disk II and an Apple 3.5 Drive for a 140K and an 800K image, by posted drag where possible; record the result in `specs/049-explorer-picker/validation.md`

**Checkpoint**: merge `origin/048-smartport-drives`, build, run `-Filter DropRule`; commit and push.

---

## Phase 4: User Story 2 - Form factor and formatted size in every listing (Priority: P2)

**Goal**: Two new columns filled in every folder listing, sortable, without blocking the list.

**Independent Test**: Open a folder of mixed images in Casso Explorer; each row's form factor and formatted size match the image, and both columns sort correctly.

### Tests

- [ ] T024 [P] [US2] Write `UnitTest/CassoExplorer/PickerColumnTests.cpp`: `FromHostEntry` fills `imageClass` for an image, and both column texts are empty for a file that is not one; sort by `FormattedSize` orders 140K, 400K, 800K, 5 MB, 32 MB by bytes; sort by `FormFactor` orders 5.25-inch, 3.5-inch, hard disk, tape; rows without a class sort after rows with one; folders sort ahead of files in both; column ids of every column that existed before equal the values in a table written into the test
- [ ] T025 [P] [US2] Write `UnitTest/CassoExplorer/ImageVolumeFactsTests.cpp`: file system and volume name for a DOS 3.3, a ProDOS, a Pascal and an HFS image from in-memory bytes; an unreadable volume has empty values and is not a failure; a cached entry is reused for the same path, length and time

### Implementation

- [ ] T026 [US2] Append `FormFactor`, `FormattedSize` and `LastUsed` after `Sha256` in `CatalogModel::Column`, add `imageClass`, `lastUsedUnix`, `isStockDisk` and `stockDownloaded` to `CatalogRow`, and extend `GetColumnId`, `TryGetColumn`, `GetDefaultOrder`, `IsHiddenUntilChosen`, `Sort` and `IsBefore` in `CassoEmuCore/CassoExplorer/Model/CatalogModel.h` and `.cpp` per data-model.md
- [ ] T027 [US2] Classify host files while a listing is built: call `ImageClassifier::Classify` through an `ImageClassCache` on the loader's thread in `CassoEmuCore/CassoExplorer/Browser/BrowserLoader.cpp` (or where `FromHostEntry` is called for a folder) and pass the class to `FromHostEntry`
- [ ] T028 [US2] Add the column headings "Form factor", "Formatted size" and "Last used", their cell text and alignment in `CassoEmuCore/CassoExplorer/Browser/BrowserCells.cpp` and the header menu's list, and include the two columns' text where the file manager's search and column filtering read cell text (FR-038), with a case in `UnitTest/CassoExplorer/PickerColumnTests.cpp` that a search for "800K" finds an 800K image
- [ ] T029 [US2] Add `ImageVolumeFacts` (file system and volume name per image, a cache keyed like `ImageClassCache`, a worker modeled on `DiskThumbnailWorker` that reads the volume's directory header only) in `CassoEmuCore/CassoExplorer/Model/ImageVolumeFacts.h` and `.cpp`
- [ ] T030 [US2] Make `FileSystem` and `VolumeName` available in folder listings: change `CatalogModel::IsCollectionColumn` so they are no longer collection-only, fill them from `ImageVolumeFacts` as results arrive, and repaint only the rows that changed, in `CassoEmuCore/CassoExplorer/Model/CatalogModel.cpp` and the list pane
- [ ] T031 [US2] Stub the classification call in T027 to return `NotAnImage`, confirm T024 fails, restore and confirm it passes
- [ ] T032 [US2] Measure per quickstart.md "Performance" in a Release build with a folder of 500 generated images and record the three times in `specs/049-explorer-picker/validation.md`

**Checkpoint**: run `-Filter PickerColumn` and `-Filter CassoExplorer`; commit and push.

---

## Phase 5: User Story 5 (first part) - The recent list and the Casso node's list (Priority: P2)

**Goal**: The Casso node lists every image of its folders, most recently used first, in the file manager.

**Independent Test**: Insert images from two folders; the Casso node's list has the images of both, with the ones just inserted first.

### Tests

- [ ] T033 [P] [US5] Extend the `DiskMru` tests in `UnitTest/EmuTests/` (the file T007 found): the list has 1,000 entries at most, the oldest is dropped at 1,001, `kMenuEntries` is 16, and a file with 16 entries written by the old format loads
- [ ] T034 [P] [US5] Write `UnitTest/CassoExplorer/CassoImagesListingTests.cpp` over a fake `IDiskFileIo`: images of two folders in one list with `folderPath` set; no folder rows; a file that is not an image is absent; a path listed through two overlapping folders appears once; a missing folder is skipped and the rest is complete; default order is load time newest first, then never-loaded images by name; assert a non-zero row count before asserting over rows

### Implementation

- [ ] T035 [US5] Change `DiskMru::k_capacity` to 1,000 and add `kMenuEntries = 16` in `CassoEmuCore/Shell/DiskMru.h` and `.cpp`; limit each displaying reader T007 found to `kMenuEntries`; move the missing-file pruning in `WindowCommandManager::GetRecentMedia` off the per-open path, per research.md R9
- [ ] T036 [US5] Add `CassoImagesListing` in `CassoEmuCore/CassoExplorer/Model/CassoImagesListing.h` and `.cpp` per research.md R8: folders from `KnownFolderStore`, files without recursion, classification through `ImageClassCache`, duplicates removed by full path, `lastUsedUnix` from the recent list
- [ ] T037 [US5] List the root `TreeModel::kCassoRootId` through `CassoImagesListing` on the loader's thread in `CassoEmuCore/CassoExplorer/Model/BrowserModel.cpp`, with default sort `LastUsed` descending and the Folder and Last used columns in its default view; the tree's children are not changed
- [ ] T038 [US5] Remove the single-folder special case at `CassoEmuCore/CassoExplorer/CassoExplorerBrowser.cpp:1526` so the Casso node always lists images, and update `ExplorerTreePane::kCassoNodeTip` in `CassoEmuCore/CassoExplorer/Window/Panes/ExplorerTreePane.h` to "Disk and tape images in the folders Casso has opened images from, most recently used first" (owner approves the text before it is pushed)
- [ ] T039 [US5] Stub `CassoImagesListing` to return no rows, confirm T034 fails, restore and confirm it passes

**Checkpoint**: run `-Filter CassoImagesListing` and `-Filter DiskMru`; commit and push.

---

## Phase 6: User Story 4 (first part) - Picker core: masks, scope, command gate, host (Priority: P3)

**Goal**: A Casso Explorer window that can run in picker mode inside the emulator's process, scoped by masks and by the drive rule, with no session and no links.

**Independent Test**: With two filter entries over a folder of mixed files, only folders and files matching the active entry are listed, and the result is the picked path or a cancel.

### Verification first

- [ ] T040 [US4] Verify (research.md R4): read the emulator's message loop (`EmulatorShell::RunMessageLoop` and the accelerator translation) and record in `specs/049-explorer-picker/research.md` whether a second top-level `DxuiWindow` on the UI thread has its messages dispatched and whether accelerators and guest keystrokes are limited to the emulator window; if not, add the fix as the first implementation task of this phase
- [ ] T041 [P] [US4] Verify (research.md R4): read `CassoExplorerPrefs::Load` and its save in `CassoEmuCore/CassoExplorer/Model/CassoExplorerPrefs.cpp` and record how a separate file is selected; confirm where OLE is initialized on the emulator's UI thread
- [ ] T042 [P] [US4] Verify (research.md R6): read how the Settings sheet and `MessageDialog` run a modal `DxuiWindow` and record the mechanism the modal form reuses
- [ ] T043 [P] [US4] Verify (research.md R5): list every place a `CassoExplorerWindow` component dereferences `context.app.links`, `running`, `announced`, `firewall`, `cards` or `host` without a null check and record each in `specs/049-explorer-picker/research.md`

### Tests

- [ ] T044 [P] [US4] Write `UnitTest/CassoExplorer/FileMaskSetTests.cpp`: `*.po;*.2mg`, case-insensitive match, `?`, `*.*` and `*` match every name, empty masks between semicolons ignored, an empty pattern matches every name, a name with no extension
- [ ] T045 [P] [US4] Write `UnitTest/CassoExplorer/PickerScopeTests.cpp`: folders always pass; with two filter entries only the active one applies, and switching re-applies without a new listing; with a `Floppy525` drive and the mask `*.po` a 140K `.po` passes and an 800K `.po` fails; an "All files" entry with a drive lists only images the drive accepts; an unclassified image fails with a drive and passes without one when its name matches
- [ ] T046 [P] [US4] Write `UnitTest/CassoExplorer/PickerCommandGateTests.cpp`: sweep every command id of `CassoExplorerCommands`; in picker mode each is on the allowlist of contracts/picker-mode.md or reports disabled and hidden; outside picker mode the states equal a table captured from the code before the gate was added; assert a non-zero count of ids swept
- [ ] T047 [P] [US4] Write `UnitTest/CassoExplorer/ExplorerPickerHostTests.cpp` over a fake `IFileSystem`: opening and closing a picker reads and writes only the picker's preferences file; the session file and `CassoExplorerPrefs.json` are neither read nor written; the window context's `links`, `running`, `announced`, `firewall` and `cards` are null; `OpenWindow` returns null; `SaveSession` writes nothing

### Implementation

- [ ] T048 [P] [US4] Add `FileMaskSet` in `CassoEmuCore/CassoExplorer/Picker/FileMaskSet.h` and `.cpp`
- [ ] T049 [P] [US4] Add `PickerRequest`, `PickerForm` and `StockDiskRow` in `CassoEmuCore/CassoExplorer/Picker/PickerRequest.h` per data-model.md, and a `PickerRequest` member (optional) in `CassoExplorerWindowContext` in `CassoEmuCore/CassoExplorer/Window/ExplorerContext.h`
- [ ] T050 [US4] Add `PickerScope` in `CassoEmuCore/CassoExplorer/Picker/PickerScope.h` and `.cpp`, and a row filter hook in `CassoEmuCore/CassoExplorer/Model/BrowserModel.h` and `.cpp` applied where a listing becomes rows, including the Casso node's list
- [ ] T051 [US4] Add the picker allowlist to `CassoEmuCore/CassoExplorer/CassoExplorerCommands.h` and `.cpp`: in picker mode a command not on the list is disabled and hidden for every route
- [ ] T052 [US4] Add `PickerPrefs` (per `viewKey`: columns, order, widths, sort and placement; the last folder too for a request with no receiving drive) in `CassoEmuCore/CassoExplorer/Picker/PickerPrefs.h` and `.cpp`, stored in a picker file selected by the mechanism T041 found
- [ ] T053 [US4] Add `ExplorerPickerHost` in `CassoEmuCore/CassoExplorer/Picker/ExplorerPickerHost.h` and `.cpp` per contracts/picker-mode.md: builds the browser, actions, watcher and context for one picker, a window class different from `CassoExplorerShell::kWindowClass`, and `ICassoExplorerHost` methods that do nothing except `CloseWindow`, `GetWindowFor` and `ForEachWindow`
- [ ] T054 [US4] Guard each dereference in T043's list, in the component that has it, so a window with null services runs
- [ ] T055 [US4] Add `ExplorerPickerMode` in `CassoEmuCore/CassoExplorer/Window/Managers/ExplorerPickerMode.h` and `.cpp` (mode state, active filter, confirm, cancel, the start location and first-use view of contracts/picker-mode.md) and wire it into `ExplorerWindowParts` and `CassoExplorerWindow`: Enter and double-click on a file confirm, on a folder open it; a disk image or archive row is a file in picker mode; the preview pane previews the selected image (FR-029); Esc and the close box cancel
- [ ] T056 [US4] Add `ExplorerPickerBar` in `CassoEmuCore/CassoExplorer/Window/Panes/ExplorerPickerBar.h` and `.cpp` (header with drive label and `GetAcceptedText`, filter choice when there are several entries, "Create new disk..." or "New blank tape" when `onCreateNew` is set, Insert or Open, Cancel), and lay the window out in picker mode without the menu bar and the file-changing toolbar groups in `CassoEmuCore/CassoExplorer/Window/Managers/ExplorerLayout.cpp`
- [ ] T057 [US4] Report in the status bar how many files are listed and how many the scope left out, and on an empty Casso node that no recent folders exist, in `CassoEmuCore/CassoExplorer/Window/Panes/ExplorerStatusBar.cpp`
- [ ] T058 [US4] Limit `CassoExplorerDragOut` in picker mode to the plain file path format, in `CassoEmuCore/CassoExplorer/CassoExplorerDragOut.cpp`
- [ ] T059 [US4] Stub `PickerScope::Passes` to return true and the command gate to allow everything, confirm T045 and T046 fail, restore and confirm they pass

**Checkpoint**: run `-Filter Picker` and `-Filter FileMaskSet`; commit and push.

---

## Phase 7: User Story 1 - Insert a disk from a drive (Priority: P1)

**Goal**: A click on any drive opens that drive's picker; a pick is inserted in that drive.

**Independent Test**: With a folder of 5.25-inch, 3.5-inch, hard disk and tape images, the picker opened from each drive kind lists exactly what that drive accepts, and a pick is inserted in the drive the picker was opened for.

### Tests

- [ ] T060 [P] [US1] Write `UnitTest/EmuTests/DrivePickersTests.cpp` with a fake picker runner: `Open` twice for one address creates one picker and fronts it; `Open` for a second address creates a second and the first's address is the same; confirm mounts in the picker's drive, adds the image to the recent list and its folder to the Casso node's folders, and closes it; a No to the inspector confirmation mounts nothing and the picker is open; a second `Open` for a drive after its picker closed starts on the Casso node; after a failed mount the picker is open and the failure is reported; `Close` and `CloseAll` mount nothing; a machine switch calls `CloseAll`; the request for each `DeskDriveModel` has the `ReceivingDrive` from `GetReceivingKind` and, for a hard disk, the drive's capacity

### Implementation

- [ ] T061 [US1] Add `ShellDrivePickers` in `CassoEmuCore/Shell/Components/ShellDrivePickers.h` and `.cpp` per contracts/picker-request.md: one picker per `DriveAddress` through `ExplorerPickerHost`, owned by the emulator window, request built from the drive (title "Insert disk - <drive label>", filters for the drive's images plus "All files", `receivingDrive`, `viewKey` per drive kind, recent entries)
- [ ] T062 [US1] Confirm path in `ShellDrivePickers`: `ConfirmInspectorDiscard` for a Disk II drive, mount through `ShellDisks::Mount` or `ShellTapeDeck::InsertTape`, add to the recent list and to the Casso node's folders through the same calls the old dialog's mount made (FR-049), close the picker; a No to the inspector confirmation returns to the picker (FR-042); a failed mount is reported in the picker, which stays open
- [ ] T063 [US1] Door callbacks (FR-054): on open start the door-open transition, after a confirm the mount closes it, on any other close run the rule in `ShellDisks::BrowseForDisk` today (a mounted drive closes back, an empty drive rests open); move that logic from `CassoEmuCore/Shell/Components/ShellDisks.cpp` into callbacks `ShellDrivePickers` calls
- [ ] T064 [US1] Call `ShellDrivePickers::CloseAll` on a machine switch and on the emulator window's close, and `Close (address)` when a drive is removed, from the places in `CassoEmuCore/Shell/` that rebuild the machine and its drives
- [ ] T065 [US1] Replace the callers: `ShellDisks::BrowseForDisk` (clicks in `EmulatorWindowInput.cpp`, `IDM_DISK_INSERT1` / `IDM_DISK_INSERT2` in `WindowCommandManager.cpp`), the drive bar in `ShellChromeDriveBar.cpp`, `ShellDeskScene::BrowseExtraDrive`, the insert item of `EmulatorShell::ShowDriveContextMenu` in `EmulatorShellStorage.cpp`, and the tape deck's insert in `ShellTapeDeck.cpp`, each with `ShellDrivePickers::Open` for its drive
- [ ] T066 [US1] Mark rows whose image is inserted in a drive of the running machine with "In slot N drive N" (FR-064): `PickerRequest` gets the mounted paths and their drive labels from `ShellDrivePickers`, refreshed on mount and eject, with a case in `UnitTest/EmuTests/DrivePickersTests.cpp`
- [ ] T067 [US1] Stub `ShellDrivePickers::Open` to always create a picker, confirm T060 fails, restore and confirm it passes
- [ ] T068 [US1] Window checks 1 to 8 of quickstart.md for each drive kind present on a //e with a Disk II, an Apple 3.5 Drive, a UniDisk 3.5, an HD20SC and a ProFile, and on a //c Plus, using a test copy and posted input; capture each picker; confirm no `CassoExplorer.exe` was started and `CassoExplorerPrefs.json` and the session file have the timestamps read at the start of the run; record in `specs/049-explorer-picker/validation.md`

**Checkpoint**: merge `origin/048-smartport-drives`, build, run `-Filter DrivePickers`; commit and push.

---

## Phase 8: User Story 5 (second part) - New disks, blank tapes and stock disks in the picker (Priority: P2)

**Goal**: Everything the old dialog offered besides browsing is in the picker.

**Independent Test**: Create a new disk and pick a stock disk from the picker; each is inserted.

- [ ] T069 [US5] Verify (research.md R8): read the stock disk catalog, its download and its consent prompt behind `AssetBootstrap::PromptInsertDiskMru`, and `CreateBlankDiskForDrive` and `ShellTapeDeck::CreateBlankTape`, and record their entry points in `specs/049-explorer-picker/research.md`
- [ ] T070 [P] [US5] Extend `UnitTest/EmuTests/DrivePickersTests.cpp`: `onCreateNew` is set for floppy drives, hard disks and the recorder; the stock rows of a request are only those `DriveImageRule::Accepts` passes for the drive, and a request with none has no "Stock disks" node; confirming a stock row that is not downloaded runs the download step before the mount, and after a declined consent nothing is mounted and the picker is open
- [ ] T071 [US5] Set `onCreateNew` in `ShellDrivePickers` to the existing create flow limited to the drive's kinds of disk (`CreateBlankDiskForDrive`), and to `CreateBlankTape` for the recorder; the result is mounted through the confirm path of T062
- [ ] T072 [US5] Fill `PickerRequest::stockDisks` from the catalog T069 found, filtered by the drive rule; add a "Stock disks" child of the Casso node in `CassoEmuCore/CassoExplorer/Model/TreeModel.cpp` when the request has stock disks, listed through a new `StockDisksListing` in `CassoEmuCore/CassoExplorer/Model/StockDisksListing.h` and `.cpp`, with `UnitTest/CassoExplorer/StockDisksListingTests.cpp` and a "Not downloaded" mark on rows not yet on disk; confirming one that is not downloaded runs the existing consent and download, then the confirm path
- [ ] T073 [US5] Window check: create a new disk from a Disk II picker and from an Apple 3.5 Drive picker, a blank tape from the recorder's picker, and pick a stock disk; record in `specs/049-explorer-picker/validation.md`

**Checkpoint**: commit and push.

---

## Phase 9: User Story 4 (second part) - The modal open and save forms behind the dialog seam (Priority: P3)

**Goal**: A caller that waits for a path gets the picker through `IHostDialogs::PickFileToOpen` or `PickFileToSave`, in the emulator and in Casso Explorer.

- [ ] T074 [P] [US4] Write `UnitTest/Casso/CassoHostDialogsTests.cpp` with a fake inner `IHostDialogs` and a fake picker runner: `PickFileToOpen` with `content == Images` goes to the picker and returns its path or cancel; with `content == Any` it goes to the inner dialogs; `PickFolder` always goes to the inner dialogs; a picker that cannot be created is a failure and the inner dialogs are not called; empty `filters` become one "All files" entry
- [ ] T075 [US4] Add `FileDialogContent`, `FileDialogSpec::content` and `FileDialogSpec::receivingDrive` in `CassoEmuCore/Seams/IHostDialogs.h`
- [ ] T076 [US4] Add `CassoHostDialogs` in `CassoEmuCore/Seams/CassoHostDialogs.h` and `.cpp`: a decorator over `Win32HostDialogs` that runs a `ModalOpen` picker through `ExplorerPickerHost` with the mechanism T042 found, and make it the emulator's `IHostDialogs` where `EmulatorShell` constructs `m_hostDialogs`
- [ ] T077 [US4] In the Hardware page's browse callback in `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`, set `content = Images` and `receivingDrive` from the selected port's drive model through `DeskDrivePlan::GetReceivingKind`, with the port's capacity for a hard disk; pass the selected port's model from `CassoEmuCore/Ui/Settings/HardwarePage.cpp`
- [ ] T078 [US4] Set `content = Images` at the other image open requests in the emulator found by `rg -n "PickFileToOpen" CassoEmuCore/Shell CassoEmuCore/Ui` (the disk compare dialog and any remaining image request), leaving serial, printer and screenshot requests as they are
- [ ] T079 [US4] Stub `CassoHostDialogs::PickFileToOpen` to always call the inner dialogs, confirm T074 fails, restore and confirm it passes
- [ ] T080 [US4] Window check per quickstart.md "Settings": browse from the Hardware page for a 3.5-inch port and a hard disk port; the picker is modal over Settings and the path is written into the field; record in `specs/049-explorer-picker/validation.md`

- [ ] T081 [US4] Verify (research.md R6a): record in `specs/049-explorer-picker/research.md` which object a Casso Explorer window's `ExplorerContext::dialogs` refers to and where it is constructed; whether a connected Apple drive's form factor is available in `CassoLinkManager::WriteImageToDisk`; and what `ExplorerCommandManager::ReplacePartition` and the request near `ExplorerCommandManager.cpp:1375` are for
- [ ] T082 [P] [US4] Write `UnitTest/CassoExplorer/PickerSaveFormTests.cpp` over the picker mode's model: the name box starts with `defaultFileName`; selecting a file row copies its name; Save is unavailable for an empty or invalid name and on the Casso node; a name without an extension gets `defaultExtension`, or the active filter's first extension when none was passed; with `askToReplace` true an existing path gets a prompt and No returns to the picker; with it false the path returns without a prompt; "New folder" is allowed in the save form and in no other form; with a receiving drive the save form lists only images the drive accepts (FR-060); the start folder order is `initialFolder`, the saved folder, the create folder
- [ ] T083 [US4] Extend `UnitTest/Casso/CassoHostDialogsTests.cpp`: `PickFileToSave` with `content == Images` goes to the picker's save form and returns its path or cancel; with `content == Any` it goes to the inner dialogs
- [ ] T084 [US4] Add `PickerForm::ModalSave` and the fields `defaultFileName`, `defaultExtension` and `askToReplace` to `CassoEmuCore/CassoExplorer/Picker/PickerRequest.h`; add the file name box and the Save button to `ExplorerPickerBar`, and the save rules (FR-057 to FR-061) to `ExplorerPickerMode`, with "New folder" on the allowlist for this form only in `CassoEmuCore/CassoExplorer/CassoExplorerCommands.cpp`
- [ ] T085 [US4] Route `PickFileToSave` with `content == Images` to the save form in `CassoEmuCore/Seams/CassoHostDialogs.cpp`
- [ ] T086 [US4] Make `CassoHostDialogs` the `IHostDialogs` of Casso Explorer's windows at the place T081 found, with an `ExplorerPickerHost` that is separate from `CassoExplorerShell`'s windows and session
- [ ] T087 [US4] In `CassoEmuCore/CassoExplorer/Window/Managers/CassoLinkManager.cpp`, set `content = Images` on "Write image to..." (with `receivingDrive` for the connected drive), "Create image from...", "Capture raw disk from..." and "Save CassoLink disk image"; set it on any image request T081 found in `ExplorerCommandManager.cpp`; the `*.bin` and cassette bootstrap `*.wav` requests are not changed
- [ ] T088 [US4] Set `content = Images` on the emulator's "Save disk as" request in `CassoEmuCore/Shell/Components/ShellDisks.cpp` and the new tape request in `CassoEmuCore/Shell/Components/ShellTapeDeck.cpp`
- [ ] T089 [US4] Window check with a test copy of Casso Explorer that has no saved serial session and no connection opened by the run: the save form from the emulator's "Save disk as", and, if a connection-free route to "Write image to..." exists in a test harness (`ExplorerWindowParts`), the open form with a 5.25-inch receiving drive; record in `specs/049-explorer-picker/validation.md` what was and was not exercised on a real connection
**Checkpoint**: commit and push.

---

## Phase 10: Removal of the old dialog (FR-040, FR-041, SC-005)

- [ ] T090 Remove `WindowCommandManager::PromptInsertDiskMru`, `PromptInsertTapeMru` and `PromptForDiskImage` from `CassoEmuCore/Shell/WindowCommandManager.h` and `.cpp`, `ShellDeskScene::BrowseFloppy35` from `CassoEmuCore/Shell/Components/ShellDeskScene.h` and `.cpp`, the dialog call and the modal keep-alive in `ShellDisks::BrowseForDisk`, and the themed dialog behind `AssetBootstrap::PromptInsertDiskMru` with its tests; the stock disk catalog, the create flow and the recent list are not removed
- [ ] T091 Confirm with `rg -n "PromptInsertDiskMru|PromptInsertTapeMru|PromptForDiskImage|BrowseFloppy35|MakeDiskPickerKind" -g "*.cpp" -g "*.h"` that nothing references the removed code, and that every remaining `PickFileToOpen` in `CassoEmuCore/Shell` and `CassoEmuCore/Ui` is either `content = Images` or not an image request; record the two listings' totals in `specs/049-explorer-picker/validation.md`

**Checkpoint**: full unit suite; commit and push.

---

## Phase 11: Polish & Cross-Cutting Concerns

- [ ] T092 [P] Write `ScenarioTests/ImageClassifierScenarioTests.cpp`: for every fixture under `UnitTest/Fixtures/CiderPress/` and `Disks/` that the emulator mounts, the classification's form factor and size equal what the mount reports; a fixture that cannot be read fails the test; assert a non-zero count compared (SC-008)
- [ ] T093 Run `scripts\RunTests.ps1 -Build` and `scripts\RunTests.ps1 -Build -Scenario` and record both results in `specs/049-explorer-picker/validation.md`
- [ ] T094 Captures of the picker for each drive kind in a light and a dark theme, sent to the owner for the gestalt check; apply the owner's layout changes
- [ ] T095 Window check of Casso Explorer as a file manager with a test copy that has no saved serial session: the two columns in an ordinary folder and the Casso node's list; restore only the preferences the run changed
- [ ] T096 Draft the `CHANGELOG.md` entries under `[Unreleased]` (one terse line each: the picker and its save form, the columns, the Casso node's list, the drop cursor, the recent list's size) and the `README.md` changes, and get the owner's approval of the text before pushing them
- [ ] T097 Update `docs/` pages about inserting a disk or the insert disk dialog, found by `rg -il "insert disk" docs README.md`
- [ ] T098 Merge `origin/048-smartport-drives`, run `scripts\CheckStyle.ps1 -Mode Tree`, then `scripts\RunMergeGate.ps1`; record the results in `specs/049-explorer-picker/validation.md`; push; merge to another branch only on the owner's instruction

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: none.
- **Phase 2 (Foundational)**: Phase 1's T003, T004 and T005. Blocks every later phase.
- **Phase 3 (US3, drops)**: Phase 2.
- **Phase 4 (US2, columns)**: Phase 2 and T006.
- **Phase 5 (US5, Casso node's list)**: Phase 4 (the `LastUsed` column and `imageClass` on rows) and T007.
- **Phase 6 (US4, picker core)**: Phases 2, 4 and 5.
- **Phase 7 (US1, drive pickers)**: Phases 3 (`DriveAddress`, `TryGetReceivingDrive`) and 6.
- **Phase 8 (US5, new and stock disks)**: Phase 7.
- **Phase 9 (US4, modal forms)**: Phase 6. T088 edits `ShellDisks.cpp` and `ShellTapeDeck.cpp`, which Phase 7 also edits, so it follows Phase 7.
- **Phase 10 (Removal)**: Phases 7, 8 and 9.
- **Phase 11 (Polish)**: Phase 10.

### User Story Dependencies

- **US3** depends only on the foundation.
- **US2** depends only on the foundation.
- **US5** depends on US2's columns for its first part and on US1's drive pickers for its second.
- **US4** depends on US2 and US5's first part; its modal form does not depend on US1.
- **US1** depends on US3's lookup and on US4's picker core.

### Within Each Phase

- Verification tasks before the code that depends on them.
- Tests are written before or with the implementation and are confirmed to fail against a stub.
- After a restore from a stub, touch the restored file's timestamp before rebuilding.

### Parallel Opportunities

- T003 to T007 read different files.
- T008 and T009; T024 and T025; T033 and T034; T044 to T047; T048 and T049.
- Phase 3 and Phase 4 touch different files and can proceed in either order once Phase 2 is done.
- Phase 9, except T088, can proceed beside Phases 7 and 8 once Phase 6 is done.

This project's standing rule is one session working serially in this checkout unless the owner requests more, so the parallel markers indicate independence, not a staffing plan.

## Implementation Strategy

The feature ships whole. Phases are checkpoints for commits, pushes and merges of 048, not releases.

1. Phases 1 and 2: the classifier and the rule, fully tested, with no user-visible change.
2. Phase 3: the first user-visible change (the drop cursor), independent of Casso Explorer.
3. Phases 4 and 5: Casso Explorer's columns and the Casso node's list, visible in the file manager.
4. Phases 6 to 9: the picker.
5. Phase 10: the old dialog is removed only when every caller has moved.
6. Phase 11: real-file validation, captures for the owner, user-facing text for approval, merge gate.

## Notes

- A task that finds its assumption false stops and updates research.md and this file before code is written.
- `specs/049-explorer-picker/validation.md` is created by the first task with a result to write.
- `.specify/feature.json` is per-checkout state and is never committed.
