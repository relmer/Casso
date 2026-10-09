# Data Model: Disk Inspector

**Feature**: 040-disk-inspector | **Plan**: [plan.md](plan.md) | **Research**: [research.md](research.md)

Types are listed by layer. Plain-data structs with no methods are nested in the
class that uses them; API types are free types in their header (coding
standards, "Where a file-local type goes"). Field lists give meaning, not
declarations. "Later release" marks a member that exists for R30's seams and
does nothing in the first release.

## 1. Image layer (`CassoEmuCore/Devices/Disk`, `Machines/Apple2/Common`)

### WozInfo (in `WozMetadata.h`)

Every INFO field of the file's version, each later-version field with a
`has...` flag. Filled by `WozLoader::ReadInfo` (R8).

| Field | INFO version | Notes |
|---|---|---|
| version, diskType, isWriteProtected, isSynchronized, isCleaned, creator | 1 | creator is 32 bytes, trailing spaces kept for display |
| sides, bootSectorFormat, optimalBitTiming, compatibleHardware, requiredRamK, largestTrack | 2 | compatibleHardware and requiredRamK are what `WozCompatibility::ReadRequirements` reads |
| fluxBlock, largestFluxTrack | 3 | 0 when absent |

### WozFileLayout (in `WozMetadata.h`)

What the file holds, as read (FR-052).

- `tmap[160]`, `flux[160]`: the maps as stored; `hasFluxMap`.
- `records`: one `TrackRecordFields` per TRKS entry: v2 `startBlock`,
  `blockCount`, `bitOrByteCount`; v1 `bytesUsed`, `bitCount`, `splicePoint`,
  `spliceNibble`, `spliceBitCount`; `isReferenced`; `isFlux` (which map points
  at it).
- `sourceMap[160]`: TMAP, FLUX, or none, per quarter track.
- `chunks`: ID, offset, size, in file order.
- `metaEntries`: key and value, in file order.
- `crc`: stored, computed, `isMatch`.
- `problems`: `ImageFileProblem` list (FR-051).

### ImageFileProblem

`kind` (ChecksumMismatch, LargestTrackTooSmall, CountExceedsBlocks,
MetaValueOutsideList, ImageDateNotRfc3339, DuplicateChunk, ChunkOutOfOrder,
DataPastLastChunk, UnreferencedRecord, MapEntryToEmptyRecord, NibTrackWithoutSync),
`isDamage` (true only for the kinds FR-048 calls damaged), location (record,
quarter tracks, chunk), and the values involved.

### DamageReason (extended)

Existing: `OutsideFile`, `CountExceedsBlocks` (now bit and flux),
`TruncatedRun`, `V1RecordPastTrks`. New (R9): `RecordLocationMissing`,
`RecordInHeader`, `MapEntryOutOfRange`. `MapEntryOutOfRange` is kept per
quarter track in a new `DamagedQuarterTrack { quarterTrack, mapIsFlux, entry }`
list; `GetDamagedQuarterTracks` merges both lists.

### DiskImage additions

- `ResolveWholeTrack (int track)`: `ResolveQuarterTrack (4 × track)`.
- Per slot: `guestWriteCount` (uint64), `isChangedByWriter` (bool, set by the shared
  sector writer for every caller, cleared by a save), the `WozFileLayout` and `WozInfo` it was loaded with (inside
  `WozMetadata`).
- `GetAngle` on `Disk2NibbleEngine` becomes public.
- Rule: a guest write never sets `isChangedByWriter`; an edit never increments
  `guestWriteCount` (FR-109).

### SectorWrite (API type, `SectorFieldWriter.h`)

What every caller hands the shared sector writer (spec Key Entities).

| Field | Meaning |
|---|---|
| slot | Track record index, found through the map |
| fieldStartCell | Cell of the data prologue's first nibble, from the analyzer; -1 lets the writer find the sector by number (disk command) |
| sectorNumber, fieldKind | For locating by number and for the encoding |
| bytes[256] | New data |
| table | Standard (first release); a custom table in the later release |
| checksumMode | Recompute, or KeepStored |
| policy | `Strict` (disk command, Explorer: FR-114, FR-115) or `Editor` (FR-096) |

Result per write: success, or a `SectorWriteError` with the quarter track,
record and reason (no data field, track does not decode completely, record of
its own between whole tracks, nibble count differs, noise in field, outside
table, damaged record).

### DiskCopy, TrackCopy

- `TrackCopy`: slot, kind (bits, flux), bits and bit count or flux bytes,
  `guestWriteCount` at copy time. Immutable, shared. Later release:
  `TrackCaptures` holds several (R30).
- `DiskCopy`: `mediaId`, format, the quarter-track map, a `TrackCopy` per
  record (shared pointers, so unchanged records are shared between copies),
  `ImageDetails`, the write-protect causes.

### ImageDetails

File name, format, size, read-only attribute; for a WOZ, `WozInfo`,
`WozFileLayout`; for a sector image, its sector order. In Casso also
`changedSinceLoad`: per value, the value at load and now (FR-071), records the
guest added, and `wasSavedSinceLoad`.

## 2. Analysis layer (`CassoEmuCore/Devices/Disk/Inspector`)

### DecodeSettings

List of `DecodeRange { firstTrack, lastTrack, marks, checks, laterRelease }`.

- `marks`: `DiskMarkPattern` for the 16-sector address prologue, 13-sector
  address prologue, data prologue, address epilogue, data epilogue;
  `matchStandardToo` (default on).
- `checks`: address checksum, data checksum, epilogue (each on by default).
- later release: encoding, table, checksum seeds, sector-number rule,
  per-decoder switches.
- `IsStandard()`: drives the settings chip.

Lifetime: while the window shows the disk (FR-019).

### Nibble (in `TrackAnalysis`)

`value`, `startCell`, `widthCells` (8, 9 or 10 for sync), `extraZeroCells`,
`kind` (Sync, AddressPrologue, AddressField, AddressEpilogue, DataPrologue,
DataField, DataEpilogue, Other, Noise), `isFailedChecksum`, `fieldIndex`
(-1 when outside a field).

### TrackAnalysis

For one record: `nibbles` in passing order from the index, `randomBitRegions`
(start cell, length), `fields`, `sectors`, `measurements`, `findings`, and on a
flux track `cellTimes` (one per cell, in flux units) and `transitions` (time,
cell). Built from a `TrackCopy` and the `DecodeSettings` for its track.

### Field

`kind` (Address16, Address13, Data62, Data53), `startCell`, `endCell` (can
wrap), `prologueFound`, `epilogueFound`, address `volume`, `track`, `sector`,
`checksumStored`, `checksumComputed`, `result` (Good, Bad, NotChecked),
`pairedField`, `invalidNibbles` (value, count), `hasNoise`.

### Sector

`addressField`, `dataField` (or none), `bytes[256]` (decoded, even when bad),
`state` (Good, BadAddress, BadData, NotChecked, NoDataField), `dos33Logical`,
`prodosBlock` and half (16-sector only), `cpmSector` and `allocationBlock` (on a
mapped CP/M volume), `passingIndex`, `isEditable` and the reason when not
(FR-096).

### TrackMeasurements

`lengthCells`, `nibbleCount`, `syncRuns` (start, count, widths),
`gapsBeforeAfter` per field, `longestSync` (start cell, count),
`sector0Angle`, `firstFieldAngle`; flux: `turnTime`, `rpm`, `meanCellTime`,
`deviation`.

### TrackClass

NothingRecorded, Damaged, Sixteen, Thirteen, ThirteenAndSixteen, Unformatted,
Nonstandard; later release adds a custom-format class per decoder (FR-016).

### QuarterTrackEntry

`content` (Nothing, BitTrack, FluxTrack, Damaged), `slot`, `sourceMap`,
`sharesWith` (quarter tracks), `class`, counts (good, found, not checked),
`isBeyondHeadReach`, `damageReason`. Casso: `isWrittenSinceInsert`,
`visitCount`, `isChangedByApply` (from the applied-edit history, FR-109).

### Finding

`category` (Track, Field, Sector, QuarterTrack, ImageFile, FileSystem;
later release `Protection`), `quarterTrack`, `sector`, `cell`, `kind`, values,
and the text built by a core formatter. Ordered by quarter track and cell.

### DiskSummary

`format` (FR-018's rule), `tracksWithData`, `sectorsGood`, `sectorsFound`,
`sectorsNotChecked`, `badSectors`, `nonstandardTracks`, `unformattedTracks`,
`fluxTracks`, `damagedTracks`, `commonVolume`; each record counted once.

### DiskAnalysis

`mediaId`, `settings`, `entries[160]`, `tracks` (per record), `findings`,
`summary`, `imageDetails`, `fileMaps` (section 3), `headLimit`
(`Disk2Controller::kMaxQuarterTrack`).

State: a record is Pending (copy taken, not analyzed), Done, or Dropped (its
copy is stale). A record's analysis is replaced when a newer copy with a
different write count arrives (FR-069).

## 3. File map (`Inspector/FileMap`)

### SectorSource

Built from a `DiskAnalysis`: for each volume track and logical sector (or
block half) the bytes and the result (Good, NotChecked, Bad, Missing), read
from the first field in passing order with that physical sector and a good or
not-checked address field (FR-084).

### FileMap

`fileSystem` (Dos33, ProDos, Pascal, Cpm), `volumeName`, `cells` (one per
logical sector, block or CP/M sector: `role`, `result`, `owners`),
`files`, `isCatalogComplete` with the unreadable sector, `notMappedReason`
(no file system, 13-sector, other size with the size, likely cause).

`role`: Free, BootArea, VtocOrKeyBlock, CatalogOrDirectory,
UnusedCatalogSector, VolumeBitmap, Subdirectory, IndexBlock (track/sector
list, index, master index, extended key), FileData, BadBlocksFile,
AllocatedUnowned, OwnedMarkedFree, CrossLinked.

### MappedFile

`path` (CP/M: "user:NAME"), `type`, `isLocked`, `recordedSize`, `sectors`
(in file order, each with role and place; holes as entries with no sector),
`forks` (ProDOS extended files), `chainState` (Complete, or Broken with place
and reason: Loop, OutsideVolume, BadSector, MissingSector, TooLong),
`isDeleted` with `stillFree` and `usedByOthers`, `states` (ChainBroken,
CrossLinked, TouchesBadSectors).

## 4. Editing

### PendingEdit

`slot`, `fieldStartCell`, `sectorNumber`, `fieldKind`, `bytesAtStart[256]`,
`writeCountAtStart`, `changed` (bitmap of byte offsets) and the new values,
`checksumMode`, `isOutOfDate`. Follows its field after a guest write, undo,
redo or reload by sector number and encoding nearest `fieldStartCell`
(FR-108). Discarded when its sector is no longer editable.

States: Editing → Pending → (OutOfDate) → Applying → Applied, or Discarded.

### EditSet

All pending edits on the disk, grouped by record; an undo stack of edit
actions (Ctrl+Z, Ctrl+Y) over pending edits only.

### AppliedEdit

Per apply: `records` (slot, record before, record after), `savedTo` (file
path, or the preserved copy's path), `writeCountAfter` per record. The
history is a stack with a redo position; a new apply clears redo; eject,
replace, reload or close clears it (FR-107). Undo or redo is available only
while each record's `guestWriteCount` equals `writeCountAfter`, and in
Explorer while the file is unchanged since the window's last save.

### ApplyRequest (to the emulation thread)

`mediaId`, `edits`, `mode` (WaitForStop, PauseAtSafePoint,
RunToSafePointStayPaused), `kind` (Apply, Undo, Redo). Reply: Applied (path),
AppliedToPreservedCopy (path), NotApplied (reason, failed sector), Cancelled,
DiskChanged, WaitEnded (a guest write to an edited record).

## 5. Casso host

### DriveHeadState (per drive, published per frame, R11)

`isMotorOn`, `headQuarterTrack`, `readNibbles`, `writeNibbles` (041's names);
`angle`, `trackRecord`, `isEnabled`, `isTurning`, `activity` (Idle, Reading,
Writing, WriteBlocked), `writeProtect` (causes), `mediaId`, `hasDisk`,
`isAttached`, `hasController`, `countsGeneration`, and when it changes
`recordWriteCounts[]` and `visitCounts[160]`.

### FollowHead

`isOn`, `candidateQuarterTrack`, `candidateSince`. Selects after 0.3 s on one
quarter track; a hand selection turns it off.

## 6. Comparison

### ComparisonSource

ImageFile (path), DriveNow (drive), DriveAsInserted (drive), DriveFile (drive),
ExplorerNow, ExplorerAsFirstOpened; later release QuarterTracks (list).

### Comparison

`a`, `b` (sources, each with its `DiskAnalysis` and settings), `options`
(ignore sync, volumes, dates), `verdicts[160]`, `differences`, `fileComparison`,
`summary` chip counts.

### TrackVerdict

`verdict` (NothingRecorded, StandardLayout, Identical, SameCells, SameNibbles,
SameSectorData, SectorsDiffer, NibblesDiffer, OnlyInA, OnlyInB, NotCompared,
Comparing), `rotationDegrees`, `lengthDifferenceCells`, `timingNote`
(TimingDiffers, OnlyOneSideFlux), `sectorsDiffering`, `reason`.

### Difference

`kind` (Cells, Nibbles, SectorBytes, FluxTiming, VolumeNumber, File),
`quarterTrack`, `sector`, cell range in A and in B, for a file its path and
the first differing offset and count.

## 7. Window and preferences

### InspectorViewState (per window, not saved)

Selected quarter track, sector, nibble range; platter zoom and pan; strip zoom
and the fraction of the turn in view; Find results; decode settings; pending
edits; comparison.

### InspectorPrefs (saved per host, R23)

Placement, mode (Structure, Timing), disk tab, track tab, timing range,
overlays, "Show deleted files"; Casso also Follow head, last drive, open at
exit.

### DiskInspectorColors (in `DxuiTheme`, R21)

One color per Structure kind, sector state, map role, timing (fast, nominal,
slow), pending edit, difference, damage hatch, pending pattern, beyond-reach
dimming, head marker state. Zero means the fallback from
`DiskInspectorPalette::MakeFallback`.
