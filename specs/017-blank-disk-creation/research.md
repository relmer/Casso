# Research: Blank Disk Creation & Mounting (017)

Decisions are grounded in a code survey of the worktree (2026-08-08). Each entry:
Decision / Rationale / Alternatives considered.

## R-001: Formatted-track generation reuses NibblizationLayer

**Decision**: Generate formatted 16-sector tracks by running the existing
`NibblizationLayer::Nibblize` over a 143,360-byte sector buffer. No new GCR
code.

**Rationale**: `NibblizeWithMap` already emits, per track, 16× (20-sync-nibble
gap, `D5 AA 96` address field with 4-and-4 V/T/S/checksum, `DE AA EB`, 6-sync
gap, `D5 AA AD` + 342 6&2 nibbles + checksum, `DE AA EB`) with correct 10-bit
self-sync nibbles (`PackSyncNibbleBits`) — this IS a formatted track. An
all-zeros buffer already produces a formatted-but-empty disk (noted in
`NibblizationTests.cpp` and `scripts/BuildDemoDisk.ps1`). Volume number
defaults to the existing `kDefaultVolume = 254`.

**Alternatives**: A standalone `FormatTrack()` bit-stream builder — rejected:
duplicate of the encoder that mounting `.dsk` files already exercises daily.

## R-002: WOZ output via WozLoader::Serialize

**Decision**: Build the new disk as a `DiskImage` (via `Nibblize`) and write it
with the existing `WozLoader::Serialize` (WOZ v2: real CRC32, INFO with
write-protect byte, TMAP from quarter-track map, block-aligned TRKS).

**Rationale**: The serializer already exists and is the flush path for mounted
WOZ images — creation gets byte-format parity with what mounting writes back.
`info[2]` carries the image write-protect flag, which is exactly where FR-014's
WOZ toggle persists.

**Alternatives**: `WozLoader::BuildSyntheticV2` — rejected: single-track test
helper. Hand-rolled WOZ writer — rejected: exists already.

## R-003: DSK/PO output is the sector buffer itself

**Decision**: For `.dsk` (DOS order) and `.po` (ProDOS order) the created file
IS the 143,360-byte sector buffer; the filesystem skeleton writer must place
sectors according to the file's ordering convention (reusing the existing
interleave tables `kDsk_LtoP` / `kPo_DosLogicalToFile` where a mapping is
needed).

**Rationale**: Sector images have no track encoding; correctness is purely
"which 256 bytes sit at which file offset." The mount path then nibblizes as it
does for any `.dsk`/`.po`.

**Alternatives**: Building DSK by denibblizing a built DiskImage
(`Denibblize`) — workable but a pointless round-trip for creation.

## R-004: DOS 3.3 skeleton (data-only)

**Decision**: Emit an INIT-compatible empty volume: VTOC at T17 S0 (first
catalog T17 S15, 35 tracks × 16 sectors, volume 254, DOS 3.3 free-track
bitmap with tracks 0–2 and 17 marked allocated), catalog chain T17 S15→S1
(each sector pointing at the next-lower, all entries zero), all other sectors
zero.

**Rationale**: Matching a real `INIT`'d disk's allocation (DOS reserves T0–2
even though data-only leaves them empty, catalog owns T17) keeps every
DOS-era tool's expectations intact and the usable capacity identical to a
real freshly-INITed disk. `CATALOG` lists clean/empty (SC-003, FR-005).

**Alternatives**: Freeing T1–2 for ~8KB extra space — rejected: diverges from
what any 1980s tool ever produced; compatibility over 8KB.

## R-005: ProDOS skeleton (data-only)

**Decision**: 280-block volume: blocks 0–1 zeroed (boot code only when
bootable, R-007), volume directory key block 2 with header (storage type 0xF,
name `NEWDISK`, entry length 0x27, entries-per-block 0x0D, file count 0,
bitmap pointer 6, total blocks 280) chained through blocks 3–5, volume bitmap
at block 6 with blocks 0–6 marked used, everything else free.

**Rationale**: This is the canonical minimal ProDOS volume; ProDOS mounts and
`CAT`s it clean regardless of boot-block content. Volume name fixed in v1 per
spec assumption (no metadata UI).

**Alternatives**: 4-block directory vs single-block — the standard 4-block
directory (2–5) matches real formatter output; chosen for tool compatibility.

## R-006: Bootable DOS 3.3 payload

**Decision**: When bootable, copy tracks 0–2 (all 16 logical sectors each)
from the downloaded **DOS 3.3 System Master** (already in the
`BootDiskSpec` catalog as `s_kDos33Disk`) into the new image, and write a
minimal Applesoft `HELLO` file (one catalog entry + TS list + one data
sector) so the boot lands at a clean prompt instead of `FILE NOT FOUND`.

**Rationale**: This mirrors what DOS's own `INIT` does (write DOS image +
greeting program). The VTOC from R-004 already reserves T0–2. Writing one
small DOS file is a modest, pure extension of the skeleton writer.

**Alternatives**: DOS tracks without HELLO — boots to `FILE NOT FOUND` then
breaks to BASIC; rejected as a polish-level defect. Bundling DOS — forbidden
(copyright); the download-with-explicit-user-action path is established.

## R-007: Bootable ProDOS payload

**Decision**: When bootable with ProDOS selected, copy boot blocks 0–1 from
the downloaded **ProDOS Users Disk** (`s_kProDOSDisk`, carries ProDOS 1.1.1)
and write `PRODOS` + `BASIC.SYSTEM` as files into the new volume, extracted
from the Users Disk by a small ProDOS directory reader. v1's contents listbox
therefore offers exactly "ProDOS 1.1.1"; more versions arrive by adding
`BootDiskSpec` entries later.

**Rationale**: A ProDOS disk boots via boot blocks → `PRODOS` file →
`XXX.SYSTEM`; all three pieces come from the already-cataloged image. File
writing needs a seedling/sapling allocator — new pure core code
(`ProDosFileWriter`), unit-testable against the bitmap/directory invariants.

**Alternatives**: Whole-disk copy of the Users Disk — rejected: not an empty
volume (spec FR-005). Skipping ProDOS-bootable in v1 — kept in scope; it is
the priced item in this feature and the listbox decision came from clarify.

## R-008: Boot-payload availability & download

**Decision**: Availability = `fs::exists (AssetBootstrap::GetDiskDirectory() /
spec.cassoName)` (the picker's own idiom). Fetch = promote the file-static
`DownloadStockBootDisk` to a public `AssetBootstrap` static and invoke it from
the create dialog when the user enables bootable and the payload is missing
(explicit click = consent, matching the picker's stock-disk download rows).
When offline/declined, the bootable toggle disables with an explanation
(FR-017) and data-only creation proceeds.

**Rationale**: Both master disks are already cataloged with sizes and URLs;
the picker already downloads stock disks on explicit selection, so no new
consent surface is needed.

**Alternatives**: A generic "ensure asset" facade — does not exist today;
building one is out of scope. `StartupAssetEntry` with `BootDisk` kind — the
startup dialog is for launch-time batches; a single on-demand fetch fits the
`DownloadStockBootDisk` shape better.

## R-009: Pinned `<Create new disk...>` picker row

**Decision**: Add a sentinel `ModelRow` to `DiskMruPickerSession` with a
dedicated result code; teach `RebuildView`'s `sortLess` to order the sentinel
first regardless of sort column/direction and `rowPasses` to always include it
regardless of the search filter. Decode the new code in
`AssetBootstrap::PromptInsertDiskMru` to launch the create dialog for the
picker's drive.

**Rationale**: The user pinned this exact behavior ("first item regardless of
sort order"). The survey confirmed both lambdas are small and local; a footer
button was considered and rejected because the user asked for a row.

**Alternatives**: Footer button next to Browse... — rejected per user decision.

## R-010: Create dialog composition

**Decision**: A `DxuiDialogWindow` in the `PickerDialog`/`PickerBodyPanel`
mold, composed from existing widgets: `DxuiListView` (current folder's
entries: name/size/modified; folders sort first; `..` row navigates up;
double-click/Enter on a folder navigates into it), `DxuiTextInput` (file
name), `DxuiDropdown` (format WOZ/DSK/PO), `DxuiDropdown` (contents: DOS 3.3
/ ProDOS 1.1.1 / Unformatted), `DxuiCheckbox` (bootable), Create/Cancel
buttons. Navigation/validation logic lives in a pure, `IFileSystem`-injected
`FileBrowseModel` in core (folder listing, up/into navigation, extension
filter, name validation, unique-name generation, mounted-path refusal input);
the dialog is a thin view over it (constitution VI).

**Rationale**: Every widget exists (`DxuiTextInput` verified — the
color-picker hex field and Disk2 debug panel already use it). The model in
core keeps FR-006/007/018 logic unit-tested without a window.

**Alternatives**: Native `IFileDialog` — rejected in the design session
(cohesion; the user chose the themed in-app dialog). `DxuiTreeView` folder
pane — deferred; up-button + folder rows cover FR-006 with less surface.

## R-011: Write-protect toggle mechanics & surface

**Decision**: Toggle acts on the IMAGE level so it travels with the disk:
WOZ → `DiskImage::SetImageWriteProtected` + mark dirty + flush
(`WozLoader::Serialize` persists INFO byte 2); DSK/PO → set/clear the host
file's read-only attribute (new `IFileSystem` seam), then re-run
`DiskManager::ProbeFileWritability` + `ApplyExternalWriteProtect` so
`WriteProtectInfo` and the drive widget's existing padlock + tooltip update
immediately (FR-015). Surface: per-drive checkable **Disk menu** items
("Write-Protect Disk 1/2"), checked from `WriteProtectInfo` (imageFlag ‖
readOnlyFile), disabled when the drive is empty; failure paths report and
re-read true state (FR-016).

**Rationale**: The four-source `WriteProtectInfo` model, the padlock, and the
cause-naming tooltip already exist — the toggle only adds the two mutation
paths. The Disk menu is where per-drive disk actions live; the existing
Settings>Disk checkbox remains the separate per-drive *user* setting
(different `userSetting` bit, per-machine pref) and is untouched.

**Alternatives**: Drive-widget click affordance — the widget's click surface
is fully claimed (body=insert, eject region); a context menu is future polish.
Merging with the Settings user-WP checkbox — rejected: different semantics
(travels-with-image vs this-machine preference), both legitimately coexist in
the OR model.

## R-012: Default folder + persistence

**Decision**: First-use default `Documents\Casso Disks` (created on demand via
`SHGetKnownFolderPath(FOLDERID_Documents)`, mirroring the `Pictures\Casso
Prints` idiom); thereafter the last folder a disk was created in, persisted as
a new `GlobalUserPrefs::lastDiskCreateFolder` (UTF-8 string; the standard
4-edit recipe: member, `s_kKnownTopLevel`, `ToJson`, `FromJson`). This is the
repo's first persisted folder pref — the print folder is recomputed each time
and is NOT a precedent to copy for semantics, only for the known-folder call.

**Rationale**: User decision from clarify. Note `AssetBootstrap::
GetDiskDirectory()` (`%LOCALAPPDATA%\Casso\Disks`) was considered as default
and rejected — hidden from casual browsing; downloads live there, user disks
belong in Documents.

## R-013: Mounted-target refusal source of truth

**Decision**: The create dialog's confirm path asks the shell for the set of
currently-mounted backing paths (`DiskImageStore` entries' source paths, both
drives) and refuses a case-insensitive path match, naming the drive (FR-018).

**Rationale**: `DiskImageStore` already tracks each mount's source path (it is
the flush target); comparing there catches both drives and any future slots.
