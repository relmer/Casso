# Data Model: Casso Explorer as the insert disk picker

Types this feature adds or changes. Field lists are the design intent; exact identifiers are settled in code review.

## ImageClass (`Devices/Disk/ImageClassifier.h`)

What one file is.

| Field | Type | Meaning |
|---|---|---|
| `status` | `ImageStatus` | `NotAnImage`, `Unclassified` (an image extension that failed the header and length checks), `Classified` |
| `formFactor` | `ImageFormFactor` | `None`, `Floppy525`, `Floppy35`, `HardDisk`, `Tape` |
| `formattedBytes` | `uint64_t` | capacity of the medium; 0 for a tape |
| `blockCount` | `uint32_t` | 512-byte blocks when the image is readable as blocks, else 0 |
| `isBlockReadable` | `bool` | a block device can mount it: raw block, 2IMG ProDOS order, DiskCopy 4.2, 3.5-inch WOZ |
| `container` | `ImageContainer` | `Raw`, `TwoImg`, `DiskCopy42`, `Woz`, `Nibble`, `Tape` |

Rules:
- `status != Classified` implies `formFactor == None` and `formattedBytes == 0`.
- `formFactor == Tape` implies `formattedBytes == 0` and `isBlockReadable == false`.
- A block image's form factor follows `BlockDeviceMedia::IsFloppyBlockCount`: 280, 800 and 1,600 blocks are floppies, any other count is a hard disk.
- `FormatSize` returns "140K", "400K", "800K", "113K" for a 13-sector disk, megabytes with one decimal below 10 MB and whole megabytes above, and an empty string for a tape or an unclassified file.

## ImageClassCache (`Devices/Disk/ImageClassCache.h`)

Key: full path, file length, modified time. Value: `ImageClass`. An entry whose length or time differs from the file's is replaced. Thread-safe, since the loader thread fills it and the UI thread reads it. No fixed cap; bounded by the folders visited in a run.

## ReceivingDrive (`Devices/Disk/DriveImageRule.h`)

| Field | Type | Meaning |
|---|---|---|
| `kind` | `ReceivingDriveKind` | `Floppy525`, `Apple35`, `UniDisk35`, `HardDisk`, `Recorder` |
| `maxBlocks` | `uint32_t` | a hard disk's capacity; 0 for every other kind |

`DriveImageRule::Accepts (const ReceivingDrive &, const ImageClass &)` returns `bool`. `GetAcceptedText (const ReceivingDrive &)` returns the picker header's text, such as "400K and 800K images". `DeskDrivePlan::GetReceivingKind (DeskDriveModel)` maps every desk model; the recorder has no desk model and its `ReceivingDrive` is built by the tape deck.

## DriveAddress (`Shell/Components/ShellDrivePickers.h`)

The drive of a picker: `slot`, `drive`, and `isRecorder`. Two addresses are equal when all three match. A `DeskDrive` converts to it.

## FileDialogSpec additions (`Seams/IHostDialogs.h`)

| Field | Type | Default | Meaning |
|---|---|---|---|
| `content` | `FileDialogContent` | `Any` | `Images` sends an open request to the Casso Explorer picker |
| `receivingDrive` | `std::optional<ReceivingDrive>` | empty | adds the drive rule to the masks |

The existing fields are used as today: `filters` (label and masks), `initialFolder`, `title`, `defaultFileName`, `clientGuid`. `clientGuid` selects the picker's saved view when it is set.

## FileMaskSet (`CassoExplorer/Picker/FileMaskSet.h`)

Built from a pattern string such as `*.po;*.2mg`. `Matches (name)` is case-insensitive, with `*` and `?`, and true for every name when a mask is `*.*` or `*`. Empty masks between semicolons are ignored; an empty pattern matches every name.

## PickerRequest (`CassoExplorer/Picker/PickerRequest.h`)

| Field | Type | Meaning |
|---|---|---|
| `title` | `std::wstring` | window title; composed from the drive when empty |
| `initialFolder` | `std::filesystem::path` | start here when set and present |
| `filters` | `std::vector<FileDialogFilter>` | at least one; the first is active |
| `receivingDrive` | `std::optional<ReceivingDrive>` | adds the drive rule |
| `driveLabel` | `std::wstring` | "Apple 3.5 Drive, slot 5 drive 1", for the header |
| `viewKey` | `std::wstring` | selects the saved view: one per receiving drive kind, or the caller's `clientGuid` |
| `form` | `PickerForm` | `DriveWindow` (not modal, confirm calls `onConfirm`) or `ModalPath` (returns the path) |
| `onConfirm` | `std::function<bool (const std::wstring & path)>` | `DriveWindow` only; on false the picker stays open and the failure is reported |
| `onCreateNew` | `std::function<bool (std::wstring & outPath)>` | empty hides the command |
| `stockDisks` | `std::vector<StockDiskRow>` | rows appended to the Casso node's list |
| `recent` | `std::vector<DiskMru::Entry>` | load times for the `LastUsed` column |

## PickerScope (`CassoExplorer/Picker/PickerScope.h`)

State: the request's filters, the active filter index, the receiving drive. `Passes (row)`:
- a folder: true;
- a file: `FileMaskSet` of the active filter matches the name, and, with a receiving drive, `row.imageClass.status == Classified` and `DriveImageRule::Accepts` is true.

Changing the active filter re-applies the scope to the current listing without reading the folder again.

## PickerPrefs (`CassoExplorer/Picker/PickerPrefs.h`)

Per `viewKey`: last folder (or the Casso node), column ids, order and widths, sort column and direction, window placement, pinned. Stored in a picker file under the asset base folder. `CassoExplorerPrefs.json` and the session file are not read or written.

## CatalogRow additions (`CassoExplorer/Model/CatalogModel.h`)

| Field | Type | Meaning |
|---|---|---|
| `imageClass` | `ImageClass` | filled for host files in every folder listing |
| `lastUsedUnix` | `int64_t` | load time from the recent list; 0 when never loaded |
| `isStockDisk` | `bool` | a stock system disk row in a drive picker |
| `stockDownloaded` | `bool` | false until the stock disk is on disk |

`CatalogModel::Column` gains `FormFactor`, `FormattedSize` and `LastUsed` after `Sha256`. Sort keys: `FormattedSize` by `formattedBytes`; `FormFactor` by the enum's order; `LastUsed` by `lastUsedUnix` with 0 last in a descending sort. Rows without a class sort after rows with one.

## Picker window states

```text
Closed --Open (drive)--> Open
Open   --Open (same drive)--> Open            (brought to the front)
Open   --confirm, mount ok, not pinned--> Closed
Open   --confirm, mount ok, pinned--> Open
Open   --confirm, mount failed--> Open        (failure reported in the picker)
Open   --cancel / close box / Esc--> Closed
Open   --drive removed / machine switched / emulator closing--> Closed
```

Door, for a floppy drive: opening the picker starts the door-open transition. After a confirm, the mount closes the door. Any other close runs the existing rule: a mounted drive closes back, an empty drive rests open.

## DiskMru (`Shell/DiskMru.h`)

`k_capacity` is 1,000. `kMenuEntries` is 16 and limits lists displayed as menus. The stored arrays (`recentDisks`, `recentDiskLoadedAt`) have the same format, so a file written by an older build loads without conversion.
