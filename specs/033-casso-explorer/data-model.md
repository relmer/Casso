# Data Model: Casso Explorer

**Feature**: 033-cassque | **Date**: 2026-09-10

Persisted state is two small JSON files beside `UserPrefs.json` (research
R3). Everything else is in memory per window.

## Persisted

### KnownFolders.json

```json
{ "folders": [ { "path": "C:\\Apple2\\Disks", "lastUsedUnix": 1780000000 } ] }
```

| Field | Meaning |
|---|---|
| path | Absolute host folder, stored as written, compared case-insensitively |
| lastUsedUnix | Last hand-off or manual add |

Rules: written by atomic replace through one core helper that reads,
merges, and writes. Read on demand by both executables, never cached
across a picker open or a tree refresh. Seeded once from the disk MRU's
distinct folders when the file is absent. Nothing is ever auto-pruned; a
folder that no longer exists stays listed until Remove from Casso.

### CassoExplorerPrefs.json

| Field | Type | Meaning |
|---|---|---|
| theme | string | `Light`, `Dark`, `FollowSystem`, `Skeuomorphic`, `DarkModern`, `RetroTerminal` |
| previewVisible | bool | Alt+P state |
| hostNaming | string | `Descriptive` or `CiderPress` |
| placement | object | Window rect, maximized flag, monitor |
| splitters | object | Tree and preview widths in dp |
| tabs | array | Each tab's location, restored on launch |

Seeded on first run: `theme` from Casso's `activeTheme` under the
`global` object in `UserPrefs.json`, read-only. Not the top-level
`activeTheme` in `GlobalUserPrefs.json`, which exists and does nothing.

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
| loadError | Non-empty when a disk image failed to parse: no expand affordance, the text is the tooltip, and selecting the node shows it in the list area |

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
| modified | Host files and ProDOS entries (`FileEntry::modifiedUnix` when `hasModified`); blank for DOS 3.3 |
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
| previewScroll | Scroll position of the preview for this tab |
| disassemble | Whether the hex preview shows disassembly for this tab |

Preview-pane visibility is window-wide, in `CassoExplorerPrefs`, not here.

## Intent channel messages

| Message | Direction | Payload |
|---|---|---|
| ReloadInPlace, Restart | Casso Explorer to Casso | intent byte, path (existing) |
| InsertDisk | Casso Explorer to Casso | intent byte, drive byte, path |
| DescribeMachine | Casso Explorer to Casso | intent byte |
| MachineDescription | Casso to Casso Explorer | reply id, drive count byte, display name UTF-8 |
| InsertDone, InsertRefused | Casso to Casso Explorer | reply id, reason UTF-8 on refusal |
| ReloadDone, ReloadConflict, ReloadRefused | Casso to Casso Explorer | reply id, description or reason UTF-8 |

## CassoLink (section H)

### Persisted: `links` in CassoExplorerPrefs.json

| Field | Type | Meaning |
|---|---|---|
| name | string | The root's label, made from the other fields (R27) and unique among links |
| transport | string | `Serial`, `Tcp`, `WifiModem` or `AdtProServer` |
| port | string | `COM3` for serial and a serial ADTPro server; empty to search (FR-083a) |
| address | string | `host:port` for TCP and a TCP ADTPro server |
| baud | int | Default 115200 (FR-094) |
| bootstrapBaud | int | Default 9600; serial only (FR-094) |
| hardwareHandshake | bool | Default false; serial, the Wi-Fi modem and a serial ADTPro server only |
| folder | string | The folder an ADTPro server shares; its requests are confined to it (FR-088) |

A Wi-Fi modem listens on TCP port 6400, which is not stored. A link saved
as `Emulated` by an older build is dropped when the links are read, and the
name of every other link is made again. Of two links with the same name,
only the first is listed. The dialog contains only the fields its
transport uses (FR-083c), and no Apple-side port or slot (R29).

### Running Casso: `%LOCALAPPDATA%\Casso\Links\<pid>-<port>.json`  (R25)

| Field | Type | Meaning |
|---|---|---|
| pid | int | The Casso process |
| started | int64 | The process's start time as a FILETIME, so a reused pid is not mistaken for it |
| machine | string | The machine's name, as in Casso's title, such as `Apple //c` |
| serialPort | int | a //c's port, 1 or 2, or with onCard a Super Serial Card's slot, 1 to 7 |
| address | string | `127.0.0.1:<port>` |

Written when Casso connects a serial port to TCP; deleted when the port is
connected elsewhere, when Casso exits, and by Casso Explorer when the
process is gone. Each record is a root with an `Emulated` link's settings,
never stored in `links`.

### Super Serial Card: `switches` in a slot entry  (R32)

| Field | Type | Meaning |
|---|---|---|
| sw1 | string | SW1-1 to SW1-7, `on` or `off` each |
| sw2 | string | SW2-1 to SW2-7 |
| sw1BeforePrinter | string | SW1 as it was before the ImageWriter II's connection set the card to printer mode; absent when the mode was not changed for the printer |

### In memory

- **CassoLinkSession**: one per open link. It owns the `IByteStream`, the
  protocol engine, a `LinkedDisk` per drive and the operation queue, and
  runs on its own thread.
  - **States**: Closed → (Searching, a serial link with no port) → Opening → Waiting for Ready → Loading program →
    Ready ⇄ Busy → Closed. Any state goes to Disconnected on a timeout or
    a closed stream; Disconnected goes to Opening on the next request.
- **LinkedDisk**: a 140 KB sector image with a presence bit per sector, the
  volume's kind, and the miss list of the last run (R20). It is cleared
  when the drive's Ready map or volume changes.
- **LinkOperation**:
  - **Fields**: kind (enumerate, read file, write file, delete, rename,
    make directory, read disk, write disk, bootstrap, save link), drive,
    paths, progress (0 to 100), result HRESULT, and a message.
  - **States**: Queued → Running → Done | Failed | Canceled. One runs at a
    time (FR-098).
- **LinkProgram**: the assembled shim and program bytes, their load
  addresses and the version constant, assembled once per session (R22).

### Validation

- A write to a `LinkedDisk` re-reads the catalog sectors first, and fails
  if the volume name or catalog differs from the listing it started from.
- A Write image to disk on an image whose tracks do not decode to standard
  sectors fails before any track is written (FR-086).
- A host file from a failed Read disk to image or a failed copy is never
  committed; the atomic replace of `IDiskFileIo` only runs on success
  (FR-087).

### Full send (Story 17, R34)

`full-send.csv`, one row per disk:

| Column | Contents |
|---|---|
| `sequence` | the image's sequence number |
| `file` | the image's file name, empty for a duplicate |
| `drive` | 1 or 2 |
| `fingerprint` | SHA-256 of track 0 and the catalog track, in hex |
| `hash` | SHA-256 of the whole image, in hex |
| `time` | UTC, in ISO 8601 |
| `unreadable` | the count of unreadable sectors |
| `status` | `sent`, `duplicate` or `possibly-protected` |
| `duplicateOf` | the earlier image's file name, for a duplicate |

`FullSendSession` states: Checking (drive), Sending (drive), Paused, Ended (reason). A session reads an existing manifest once, at its start, for its fingerprints, hashes and the next sequence number. A manifest that cannot be parsed ends the session before any disk is read, and is never overwritten.
