# Data Model: Cassque

**Feature**: 033-cassque | **Date**: 2026-09-10

Persisted state is two small JSON files beside `UserPrefs.json` (research
R3). Everything else is in memory per window.

## Persisted

### KnownFolders.json

```json
{ "folders": [ { "path": "C:\\Apple2\\Disks", "lastUsedUnix": 1780000000, "pinned": false } ] }
```

| Field | Meaning |
|---|---|
| path | Absolute host folder, stored as written, compared case-insensitively |
| lastUsedUnix | Last hand-off or manual add |
| pinned | True when added by Add to Casso; never auto-pruned |

Rules: written by atomic replace through one core helper that reads,
merges, and writes. Read on demand by both executables, never cached
across a picker open or a tree refresh. Seeded once from the disk MRU's
distinct folders when the file is absent. A folder that no longer exists
stays listed.

### CassquePrefs.json

| Field | Type | Meaning |
|---|---|---|
| theme | string | `Light`, `Dark`, `FollowSystem`, `Skeuomorphic`, `DarkModern`, `RetroTerminal` |
| previewVisible | bool | Alt+P state |
| hostNaming | string | `Descriptive` or `CiderPress` |
| placement | object | Window rect, maximized flag, monitor |
| splitters | object | Tree and preview widths in dp |
| tabs | array | Each tab's location, restored on launch |

Seeded on first run: `theme` from Casso's `activeTheme` in
`UserPrefs.json`, read-only.

## In memory

### Location

What a tab shows. One of:

| Kind | Fields |
|---|---|
| HostFolder | path |
| DiskImage | image path |
| DiskDirectory | image path, `FilePath` inside the image (ProDOS only) |

### TreeNode

| Field | Meaning |
|---|---|
| id | Stable string: root tag plus path plus in-image path |
| kind | CassoRoot, ThisPcRoot, KnownFolder, Drive, HostFolder, DiskImage, DiskDirectory |
| label | Display text |
| location | The `Location` selecting it fills |
| childrenLoaded | Lazy flag; children fetched on first expand |
| missing | Known folder that no longer exists, drawn grayed |

### CatalogRow

One row of the file list, built from `FileEntry` or a host directory
entry.

| Field | Source |
|---|---|
| name | `FileEntry::name` or host file name |
| typeText | DOS letter or ProDOS mnemonic; host extension |
| sizeBytes | `eofBytes` when present, else `sizeUnits` times the unit |
| addressText | `$AAAA` from `loadAddress` when `hasLoadAddress`, or aux type |
| locked | `FileEntry::isLocked` |
| modified | Host files only; blank for catalog entries |
| isDirectory | ProDOS directory or host folder or disk image |

### PreviewContent

| Kind | Payload |
|---|---|
| Listing | Lines of text, from the Applesoft or Integer detokenizer |
| Text | Lines of text, from the text conversion |
| Picture | BGRA framebuffer and its size, from a video mode |
| Hex | Rows of offset, bytes, ASCII; disassembly toggle re-renders as mnemonic rows from the load address |
| Catalog | A `VolumeListing` rendered as rows |
| Error | The decoder's message and the offset it stopped at |

### HostFileName

The rule from the clarifications, in both directions.

| Direction | Input | Output |
|---|---|---|
| Out, converted | entry, kind | `NAME.Applesoft BASIC.txt`, `NAME.Integer BASIC.txt`, `NAME.Text.txt` |
| Out, raw | entry, file system, setting | `NAME.Binary.$AAAA.bin`, `NAME.ProDOS.$TT[.$AAAA].bin`, `NAME.DOS.X.bin`, or `NAME#TTAAAA` under CiderPress |
| In | host name | catalog name, optional type, optional aux, or "undecided" for the content rule |

### ContentVerdict

Result of sniffing a host file with no usable suffix: `Applesoft`,
`Text`, or `Binary` with a suggested load address.

### DragPayload

| Field | Meaning |
|---|---|
| sourceImage | Image path, empty for host-origin drags |
| entries | Catalog paths or host paths |
| formats | Which clipboard formats the data object offers |

### CassoTarget

| Field | Meaning |
|---|---|
| kind | Owner, Running, Launch |
| hwnd | The window to send to, when Owner or Running |
| machineName | From the describe reply or the default machine |
| driveCount | From the describe reply or `AttachedDiskIiDriveCount` |

### Tab

| Field | Meaning |
|---|---|
| location | Current `Location` |
| history | Back and forward stack of locations |
| selection | Selected row ids |
| sort | Column and direction |

## Intent channel messages

| Message | Direction | Payload |
|---|---|---|
| ReloadInPlace, Restart | Cassque to Casso | intent byte, path (existing) |
| InsertDisk | Cassque to Casso | intent byte, drive byte, path |
| DescribeMachine | Cassque to Casso | intent byte |
| MachineDescription | Casso to Cassque | reply id, drive count byte, display name UTF-8 |
| InsertRefused | Casso to Cassque | reply id, reason UTF-8 |
