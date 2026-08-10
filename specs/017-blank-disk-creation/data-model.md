# Data Model: Blank Disk Creation & Mounting (017)

## BlankDiskSpec (core value type)

The in-memory description of a disk to create. Produced by the dialog, consumed
by `BlankDiskBuilder`. Pure data; no host dependencies.

| Field | Type | Values / default | Notes |
|---|---|---|---|
| `format` | `DiskFormat` | `Woz` (default) / `Dsk` / `Po` | Existing enum (`IDiskImage.h`); `Do`/`Nib` not offered |
| `contents` | `BlankDiskContents` | `Dos33` (default) / `ProDos` / `Unformatted` | New enum |
| `osVersion` | small int / id | v1: single ProDOS entry (1.1.1) | Selects the `BootDiskSpec` payload source |
| `bootable` | `bool` | `false` (default) | Only meaningful for formatted contents (FR-017) |
| `volumeNumber` | `Byte` | 254 (`kDefaultVolume`) | Not user-configurable in v1 |
| `volumeName` | string | `NEWDISK` | ProDOS only; not user-configurable in v1 |

**Validation rules** (FR-010, enforced in the dialog *and* re-checked by the
builder):

- `Dsk` pairs with `Dos33` or `Unformatted` (DOS sector order).
- `Po` pairs with `ProDos` or `Unformatted` (ProDOS sector order).
- `Woz` pairs with anything (order-agnostic bit stream).
- `bootable` requires formatted contents AND an available boot payload.

**State/derivation**: `extension()` = `.woz`/`.dsk`/`.po` from `format`
(FR-006 default-name suffix).

## BlankDiskBuilder pipeline (core)

```
BlankDiskSpec
  → skeleton sector buffer (143,360 B)      Dos33Skeleton / ProDosSkeleton / zeros
  → [bootable] payload install              tracks 0–2 + HELLO  |  boot blocks + PRODOS + BASIC.SYSTEM
  → output bytes
       Woz:  Nibblize(buffer) → DiskImage → WozLoader::Serialize
       Dsk:  buffer in DOS order (identity)
       Po:   buffer in ProDOS order (identity)
```

All-or-nothing: any failure yields no bytes (FR-011's "nothing partial" starts
here; the shell writes the file only from a complete byte vector).

## DOS 3.3 skeleton invariants (R-004)

- VTOC at T17 S0: catalog ptr → T17 S15; 35 tracks; 16 sectors; 256 B/sector;
  volume 254; free bitmap marks T0–2 + T17 allocated, all else free.
- Catalog chain: T17 S15 → S14 → … → S1, all file entries zeroed.
- Bootable adds: tracks 0–2 copied verbatim (logical sectors) from the DOS 3.3
  System Master; `HELLO` Applesoft file written (catalog entry + TS list +
  data sector, allocated honestly in the VTOC bitmap).

## ProDOS volume invariants (R-005 / R-007)

- 280 blocks. Volume directory key block 2 (header: storage 0xF, name
  `NEWDISK`, entry len 0x27, entries/block 0x0D, file count 0, bitmap ptr 6,
  total 280), chained 2↔3↔4↔5. Bitmap block 6: blocks 0–6 used, rest free.
- Data-only: blocks 0–1 zeroed. Bootable: blocks 0–1 from the Users Disk;
  `PRODOS` + `BASIC.SYSTEM` written as files (directory entries + seedling/
  sapling data blocks + bitmap allocation), extracted from the Users Disk via
  `ProDosReader`.
- Block ↔ `.po` offset: identity (block N at offset N×512). Block ↔ `.dsk`
  ordering and ↔ track/sector mapping via the existing interleave tables.

## BootPayload (R-006/R-007/R-008)

| Field | Source |
|---|---|
| identity | `BootDiskSpec` entry (`s_kDos33Disk`, `s_kProDOSDisk`) |
| availability | `fs::exists(GetDiskDirectory()/cassoName)` |
| fetch | `AssetBootstrap::DownloadStockBootDisk` (promoted public), explicit user action |
| DOS payload | tracks 0–2 sector data (from `.dsk`, DOS order) |
| ProDOS payload | blocks 0–1 + `PRODOS`, `BASIC.SYSTEM` file contents (via `ProDosReader`) |

Never bundled; cache lives in `%LOCALAPPDATA%\Casso\Disks`.

## FileBrowseModel (core, IFileSystem-injected)

State: current folder, entry list (name/isFolder/size/modifiedUnix), extension
filter (from `format`), proposed file name.

Operations (all pure over `IFileSystem`):

- `NavigateInto(entry)` / `NavigateUp()` — folder traversal; `..` row synthesized
  when a parent exists.
- `Refresh()` — list folder: folders first, then files matching the filter,
  case-insensitive name sort.
- `UniqueDefaultName(base, ext)` — `Blank Disk.woz`, `Blank Disk (2).woz`, …
  (FR-006/FR-007).
- `ValidateTarget(name)` → { ok | exists (needs confirm) | invalid-name |
  folder-not-writable | **mounted-in-drive-N** } — the last fed by the shell's
  mounted-paths set (FR-018, R-013).

## WriteProtectState (existing model, new mutations)

Existing: `WriteProtectInfo { imageFlag, userSetting, readOnlyFile,
noPermission }`, OR'd by `DiskImage::IsWriteProtected()`; padlock + tooltip
already render it.

New transitions (R-011):

| Format | Toggle ON | Toggle OFF | Persists where |
|---|---|---|---|
| WOZ | `SetImageWriteProtected(true)` + dirty + flush | `(false)` + dirty + flush | WOZ INFO byte 2 (travels with image) |
| DSK/PO | set host read-only attribute (IFileSystem seam) | clear attribute | Host filesystem |

After either: re-probe (`ProbeFileWritability`) + `ApplyExternalWriteProtect`
→ `WriteProtectInfo` → widget/tooltip/menu check state (FR-015). Failure:
report + re-read true state (FR-016). Note WOZ toggle-ON must flush BEFORE the
image flag would block the flush path — flush ordering is part of the
contract (see contracts/shell-integration.md).

## GlobalUserPrefs addition (R-012)

`lastDiskCreateFolder : std::string` (UTF-8, default empty = use
`Documents\Casso Disks`). Standard 4-edit recipe (member, `s_kKnownTopLevel`,
`ToJson`, `FromJson`). First persisted folder pref in the file.
