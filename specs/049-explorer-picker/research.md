# Research: Casso Explorer as the insert disk picker

Findings from the tree at `c1b467925` (048 merged, including `0436d69df`). Each entry is a decision the plan depends on. Items marked **Verify** are assumptions a task confirms before the code that depends on them is written.

## R1. Where the classifier goes and what it reads

**Decision**: A new `ImageClassifier` class in `CassoEmuCore/Devices/Disk/`. `ClassifyHeader (extension, fileBytes, head)` is a pure function over the extension, the file length and the first 512 bytes. `Classify (IDiskFileIo &, path)` calls `Stat` and one `ReadAt` and then `ClassifyHeader`.

**Rationale**: `IDiskFileIo` already has `Stat` and `ReadAt`, so one short read per file is possible through the existing seam, and a test can call the pure function with bytes. Every container puts what is required in its first bytes:

| Container | Recognized by | Form factor and size from |
|---|---|---|
| WOZ 1 and 2 | `WOZ1` / `WOZ2` signature | INFO chunk disk type (`WozLoader::kDiskType525`, `kDiskType35`) and, for 3.5-inch, the side count: one side is 400K, two is 800K |
| 2IMG | `2IMG` signature | image format field (DOS order, ProDOS order, nibble) and block count; nibble and DOS order are 5.25-inch |
| DiskCopy 4.2 | magic word at offset 0x52 and a consistent data length | data length: 409,600 is 400K, 819,200 is 800K, anything else by block count |
| `.nib` | extension and length 232,960 | 5.25-inch 140K |
| `.d13` | extension and length 116,480 | 5.25-inch, 13-sector, labeled 113K |
| `.dsk`, `.do` | extension and length 143,360 | 5.25-inch 140K; other lengths that are a multiple of 512 follow the block rule |
| `.po`, `.hdv`, raw | extension and a length that is a non-zero multiple of 512 | block count through `BlockDeviceMedia::IsFloppyBlockCount`: 280 is 5.25-inch 140K, 800 is 3.5-inch 400K, 1,600 is 3.5-inch 800K, 2,880 is 3.5-inch 1.44 MB (no drive accepts it), any other count is a hard disk |
| tape | `TapeImageLoader::IsTapeFileExtension` | tape, no size |

**Alternatives considered**: Calling `BlockImageFile::Open` and `WozLoader::Describe` per file. Both read the whole file (`ReadAllBytes`), which fails FR-002 for a 32 MB volume. The classifier reuses their constants and, for WOZ, calls `WozLoader::Describe` on the 512-byte head.

**Verify**: `WozLoader::Describe` returns the INFO fields from a buffer that ends after the INFO chunk. If it requires the whole file, factor the INFO read out of it into a function both call.

**Verify**: the full list of extensions `DiskImageStore::IsMountableImageExtension`, `BlockImageFile` and `BlockDeviceMedia` accept (`.dc`, `.dc42`, `.image`, `.2img`, `.img`), so the classifier's table is total over them. A test sweeps the extension lists of those three against the classifier.

## R2. The drive rule and its layering

**Decision**: A `DriveImageRule` class next to the classifier, with a `ReceivingDrive` value: a kind (`Floppy525`, `Apple35`, `UniDisk35`, `HardDisk`, `Recorder`) and, for a hard disk, the capacity in blocks. `DriveImageRule::Accepts (drive, imageClass)` is the only place the rule is written. `DeskDrivePlan::GetReceivingKind (DeskDriveModel)` in `Ui/Scene` maps the desk's models to it.

**Rationale**: `DeskDriveModel` is in `Ui/Scene`. A file in `Devices/Disk` that includes it would point the dependency the wrong way, and the recorder is not a `DeskDriveModel` at all. The mapper is a switch with no `default:` arm, and a test sweeps the `DeskDriveModel` values, so a model added by 048 without a mapping fails to compile or fails the sweep (FR-014).

**The table**:

| Receiving drive | Accepts |
|---|---|
| Floppy525 (Disk II, //c drives) | 5.25-inch |
| Apple35 (Apple 3.5 Drive, //c Plus drive) | 3.5-inch 400K and 800K, block or WOZ |
| UniDisk35 | 3.5-inch images readable as blocks |
| HardDisk (HD20SC, ProFile) | block images of any size that is not exactly a floppy's (140K, 400K, 800K, 1.44 MB) and no larger than the drive's capacity |
| Recorder | tape |

**Verify**: whether the UniDisk 3.5 device in 048 mounts a 400K image and a 3.5-inch WOZ. `BlockDeviceMedia::Attach` decodes a WOZ to blocks, so both are likely. The rule is written to match the device, and the agreement test below fails if it does not.

**Consistency with mount (FR-015)**: a table-driven test attaches one fixture per image class to each device class through the same entry points the shell uses and asserts that success equals `DriveImageRule::Accepts`. With 048's `StorageCardWiring::RejectFloppyImage`, the hard disk rows already match.

## R3. Drops

**Decision**: `ShellWindow::InstallDragDropTarget` passes an `OverFn` to `DxuiDragDropTarget::SetDataHandlers`. The callback takes the first path from the data object, classifies it (one cached result per drag), resolves the tag to a `ReceivingDrive` through a new `EmulatorShell::TryGetReceivingDrive (tag, outDrive)`, and returns `DROPEFFECT_COPY` or `DROPEFFECT_NONE`. `EmulatorShell::OnFileDropped` calls the same two functions before any mount.

**Rationale**: `DxuiDragDropTarget` already has the per-target `m_over` callback that Casso Explorer uses for the no-drop cursor. Today `OnFileDropped` branches on tag ranges and extension checks in several places; `TryGetReceivingDrive` replaces those branches with one lookup, which also serves the picker.

**Alternatives considered**: Rejecting only in `OnFileDropped`. The cursor would not change, which is the defect the owner reported.

## R4. Hosting the picker

**Decision**: The picker is a `CassoExplorerWindow` created inside the emulator's process by a new `ExplorerPickerHost` (`CassoEmuCore/CassoExplorer/Picker/`), which implements `ICassoExplorerHost`.

**Rationale**: Casso Explorer's code is already in `CassoEmuCore`, and `CassoExplorerWindowContext` documents every pointer as optional: without `links` no CassoLink connection is listed, without `host` the window is the process's only window. From `CassoExplorerShell::OpenWindow`, one window requires: a `CassoExplorerBrowser`, a `CassoExplorerActions`, a folder watcher and the context. `ExplorerPickerHost` builds exactly that and passes null for `links`, `running`, `announced`, `firewall` and `cards`.

**How FR-031 is met by construction**:
- The serial link is opened only through `CassoLinkConnections`. The picker's context has `links == nullptr`, and `ExplorerPickerHost` does not have a `CassoLinkConnections` member.
- `CassoExplorerSession::Load` and the session save are called only by `CassoExplorerShell`. `ExplorerPickerHost::SaveSession` is empty and `OpenWindow`, `RunLaunch` and `ExitAll` do nothing.
- The picker's view state is a separate `CassoExplorerPrefs` object loaded from and saved to a picker file, not `CassoExplorerPrefs.json`.
- The picker window has a window class different from `CassoExplorerShell::kWindowClass`, so a launch of Casso Explorer never finds it and forwards a command line to it.

A test builds an `ExplorerPickerHost` over a fake file system and asserts that the session file and `CassoExplorerPrefs.json` are neither read nor written, and that the context's link pointers are null.

**Alternatives considered**: Starting `CassoExplorer.exe --picker`. It isolates the picker but requires a result channel, cannot be modal to a Settings sheet without cross-process window ownership, and a second launch is forwarded to a running Casso Explorer by design (`wCassoExplorerMain`), which is the session the owner must never have touched.

**Verify**: `CassoExplorerPrefs::Load` and its save take a base folder; a file name parameter is added so the picker's file is separate.

**Verify**: the emulator's message loop dispatches messages for a second top-level `DxuiWindow` on the UI thread, and its accelerator translation is limited to the emulator window, so keys typed in the picker are not taken as emulator shortcuts or as guest keystrokes.

**Verify**: OLE is initialized on the emulator's UI thread before the picker window registers for drops (the emulator already registers a drop target).

## R5. Picker mode inside the window

**Decision**: A `PickerRequest` in `CassoExplorerWindowContext` switches the window to picker mode. A new manager, `ExplorerPickerMode`, has the mode's state. Three mechanisms scope the window:

1. **Commands**: `CassoExplorerCommands` gets a picker allowlist (navigation, view, sort, column choice, search, refresh, preview toggle, confirm, cancel, create new disk). Every other command reports disabled and hidden in picker mode, by every route: menu, toolbar, shortcut or context menu. A test sweeps every command id and asserts each is on the allowlist or disabled (FR-030, SC-007).
2. **Rows**: `BrowserModel` gets a row filter, `PickerScope`, applied where a listing becomes rows. A file row passes when its name matches the active filter entry's masks (`FileMaskSet`) and, with a receiving drive, `DriveImageRule::Accepts` passes its `ImageClass`. Folders always pass.
3. **Chrome**: the menu bar and the file-changing toolbar groups are not built. A picker bar is added with the receiving drive and what is listed (FR-034), the filter entries as a choice when there are several (FR-025), "Create new disk..." or "New blank tape", and Insert and Cancel.

**Rationale**: Scoping by command id covers every input route at one point, where a per-menu approach would miss a shortcut. The row filter is in the model so a test exercises mask and drive scoping without a window (FR-028).

**Classification in the listing path**: a row is listed or not by its class, so the class must exist before the row is delivered. It is header-only (R1), so it runs on the loader's thread while the listing is built, through an `ImageClassCache` keyed by path, length and modified time (the pattern of `CassoExplorerActions::ImageFacts`, without its 256-entry cap). The file system and volume name columns are heavier and fill afterward on a worker (R7).

**Drag out of the picker**: `CassoExplorerDragOut` is limited in picker mode to the plain file path format, so the emulator's drop target accepts it and the drive rule in R3 applies.

## R6. Two forms of the picker

**Decision**:
- **Drive picker, not modal** (FR-026, FR-050 to FR-054). A new shell component, `ShellDrivePickers`, has one entry per receiving drive (slot and drive, or the recorder). `Open (drive)` fronts the drive's picker when one exists, else creates one owned by the emulator window. A confirm calls a callback that mounts through `ShellDisks::Mount` or `ShellTapeDeck::InsertTape`, then closes the picker. A machine switch, a drive's removal and the emulator window's close call `CloseAll` or `Close (drive)`.
- **Path picker, modal** (the Hardware page of Settings). `CassoHostDialogs`, a decorator over `Win32HostDialogs`, implements `IHostDialogs::PickFileToOpen`: when the `FileDialogSpec` is for images, it runs a picker window modally over `owner` and returns the path; every other open request, and `PickFolder`, pass through to `Win32HostDialogs`. Save requests are in R6a.

**`FileDialogSpec` additions**: `content` (`Any` or `Images`, default `Any`) and `receivingDrive` (an optional `ReceivingDrive`). An image caller sets `content`, which is a one-line change per caller. Inferring "image" from the masks is not used: a caller with `*.*` would be routed by accident.

**Door (FR-054)**: `ShellDisks::BrowseForDisk` today opens the door, blocks in the dialog under the modal keep-alive, and closes the door on cancel. It becomes `ShellDrivePickers::Open` plus two callbacks: the door opens on open, after a confirm the mount's `BeginInsert` closes it, and a close without a mount runs the existing close-back rule. The modal keep-alive is not required, because the emulator's loop runs the whole time.

**`ConfirmInspectorDiscard`** runs at confirm time, before the mount, since the picker can stay open while the inspector's state changes.

**Verify**: how Dxui runs a modal top-level window (the Settings sheet and `MessageDialog` are modal `DxuiWindow`s), so the modal form reuses that loop.

## R6a. The save form and Casso Explorer's callers

**Decision**: `PickFileToSave` with `content == Images` runs the picker in a save form (`PickerForm::ModalSave`): the same window with a file name box, a Save button, "New folder" allowed, and the replacement prompt of FR-059. `CassoHostDialogs` becomes the `IHostDialogs` of Casso Explorer's windows as well as the emulator's, so `CassoLinkManager::WriteImageToDisk` and the image saves in `CassoLinkManager` get the picker by setting `content`.

**Rationale**: The owner requested that Casso Explorer's "Write image to..." and its companions use the picker. "Write image to..." is the same question as inserting in a drive: the connected Apple's drive is the receiving drive, so the drive rule lists only images that drive can take. The save requests already pass `askToReplace = false` and ask in the theme themselves, which the save form supports through `askToReplace`.

**In Casso Explorer's process** the picker is a second `CassoExplorerWindow` made by an `ExplorerPickerHost`, modal over the requesting window. It is not opened through `CassoExplorerShell::OpenWindow`, so it is not in the session, not counted as an app window, and has null `links`. The requesting command continues over its existing connection when the picker returns.

**Verify**: which object a Casso Explorer window's `ExplorerContext::dialogs` refers to and where it is constructed, so `CassoHostDialogs` is put there. **Verify** the form factor of a connected Apple's drive is available to `CassoLinkManager::WriteImageToDisk` (5.25-inch today; 3.5-inch if CassoLink reports one). **Verify** what `ExplorerCommandManager::ReplacePartition` and the request near `ExplorerCommandManager.cpp:1375` ask for, and set `content` where they ask for an image.

**Alternatives considered**: Leaving saves on the Windows dialog. Not chosen: the owner requested the picker, and a user would get two different dialogs for reading and writing the same disk.

## R7. Columns

**Decision**: Append `FormFactor`, `FormattedSize` and `LastUsed` to `CatalogModel::Column` after `Sha256`. `CatalogRow` gets an `ImageClass` and `lastUsedUnix`. `FromHostEntry` receives the class. Sorting by `FormattedSize` compares bytes; `FormFactor` sorts in the order 5.25-inch, 3.5-inch, hard disk, tape.

**Rationale**: Column ids are stored in preferences and sessions. Ids of added columns are assigned from `kFirstAddedColumnId` by position among the added columns, so a new column goes at the end of the enum. **Verify** this against `GetColumnId` and add the three to `GetDefaultOrder` beside Size.

**File system and volume name in folder listings (FR-039)**: `IsCollectionColumn` currently limits `FileSystem` and `VolumeName` to collection results. They become available in folder listings and are filled by a background worker modeled on `DiskThumbnailWorker`, through a cache keyed like `ImageClassCache`. The worker opens the volume and reads its directory header only; it does not walk files as `CollectionWalk` does. `FileCount`, `FreeSpace`, `Crc32` and `Sha256` remain collection-only.

## R8. The Casso node's list

**Decision**: Listing the root `TreeModel::kCassoRootId` produces image rows, not folder rows. A new model class, `CassoImagesListing`, takes the node's folders (from `KnownFolderStore`, the same source as the tree's children), lists each folder's files without recursion, drops the files `ImageClassifier` does not classify, removes duplicates by full path, and fills `folderPath` / `folderShown` and `lastUsedUnix` from the recent list. Default sort is `LastUsed` descending, then name. The tree's children are not changed.

**Rationale**: `CatalogRow` already has `folderPath` and `folderShown` for search results and a Folder column, so the list reuses the results-list drawing. `CassoExplorerBrowser.cpp:1526` opens the single folder directly when the node has one folder; that special case is removed so the node always lists images.

**Stock disks (FR-048)**: in a drive picker, the tree has a "Stock disks" node under the Casso node. Its listing is the stock system disks that pass the drive rule, from the same catalog `AssetBootstrap::PromptInsertDiskMru` reads, built by a `StockDisksListing` model class. The node is not added when the listing would be empty. Confirming one that is not downloaded runs the existing consent and download, then mounts. **Verify** the catalog's API and where the consent prompt is.

## R9. The recent list

**Decision**: `DiskMru::k_capacity` goes from 16 to 1,000. A new `DiskMru::kMenuEntries = 16` limits any place that displays the list as a menu.

**Rationale**: An entry is a path and a 64-bit time, about 150 bytes in the preferences JSON, so the full list is about 150 KB at most. `KnownFolderStore::SeedFromMru` and `DistinctFolders` then see more folders, which is the intent: the Casso node's folders are the folders of the last 1,000 images.

**Verify**: every reader of `GetRecentMedia` and of the `recentDisks` / `recentDiskLoadedAt` arrays. `GetRecentMedia` prunes entries whose files are missing; with 1,000 entries that check moves off the UI thread or is done per displayed row.

## R10. What is removed

- `ShellDisks::BrowseForDisk`'s dialog call, `WindowCommandManager::PromptInsertDiskMru`, `PromptInsertTapeMru`, `PromptForDiskImage`, and the themed dialog behind `AssetBootstrap::PromptInsertDiskMru`.
- `ShellDeskScene::BrowseFloppy35` and the `PickFileToOpen` calls in `ShellTapeDeck`.
- The standing rule in this project is that unused library code is not deleted. The owner's instruction for this feature is to throw the dialog away, so the dialog is removed. The stock disk catalog, the create disk flow and the recent list are not removed; the picker calls them.

## R11. Unmerged branches

Searched every unmerged remote branch for picker, classifier and dialog seam changes. `035-debugger` and the branches built on it add `NullHostDialogs` to `IHostDialogs.h` and a `KnownFolderPicker`; neither overlaps this work, and `NullHostDialogs` would have to follow any change to `IHostDialogs`' methods. This plan does not change the interface's methods, only `FileDialogSpec`'s fields, so the merge is textual.

## R12. Tests

| Area | Test | Kind |
|---|---|---|
| Classifier | one case per container and size, damaged and truncated headers, the extension sweep (R1) | unit, bytes in memory |
| Drive rule | the full table; the `DeskDriveModel` sweep | unit |
| Rule and mount agreement | fixture per image class against each device class | unit with in-memory `IDiskFileIo` |
| Masks | `FileMaskSet`: `*`, `?`, `;`, case, `*.*` | unit |
| Scope | `PickerScope` over a fake listing, with and without a drive | unit |
| Command gate | sweep of every command id in picker mode | unit |
| Host | `ExplorerPickerHost` over a fake file system: no session, no explorer prefs, null links | unit |
| Seam routing | `CassoHostDialogs`: image open and save requests to the picker, others to the inner dialogs | unit with a fake inner |
| Save form | name box rules, default extension, replacement prompt, start folder, "New folder" allowed and nothing else | unit over the picker mode's model |
| Drops | `TryGetReceivingDrive` per tag range; `OnFileDropped` with a mismatched image mounts nothing | unit (`EmulatorShell` is unit-testable) |
| Casso node list | folders merged, duplicates removed, missing folder skipped, sort order | unit |
| Recent list | capacity, menu limit | unit |
| Real files | every CiderPress and stock fixture the emulator mounts: classification equals what the mount reports (SC-008) | scenario suite |
| Window | picker opened from each drive kind, posted input, captured | manual validation with a test instance, see quickstart.md |

Each new test is checked against a stubbed implementation to confirm it fails, as `.github/copilot-instructions.md` requires for tests of new API.
