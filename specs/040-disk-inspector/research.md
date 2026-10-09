# Research: Disk Inspector

**Feature**: 040-disk-inspector | **Date**: 2026-10-08 | **Plan**: [plan.md](plan.md)

Phase 0 of the plan. Each item gives the decision, why, and what else was
considered. Line numbers are from `origin/master` at `fda167d42` unless another
ref is given; the 040 worktree's disk files are identical to it.

## Branch survey (2026-10-08)

A read-only survey of the branches 040 depends on. It is a snapshot: re-check
each item before the phase that relies on it.

| Branch / work | State | What 040 takes from it | What 040 must not assume |
|---|---|---|---|
| `origin/master` `fda167d42` | Baseline | Everything below that is not marked otherwise | - |
| `woz-info-fields` `9749d2d0a` | On master as `d78d2941a` (2026-10-08). The branch history was rewritten, so no `woz-info-fields` SHA is a valid base | INFO offsets for bit timing, compatible hardware and required RAM (`WozLoader.h:181-183`); `WozCompatibility::ReadRequirements`; `PlaceHead (double angle)` without `cameFromFlux` | That INFO is read in one place: it is read in `WozLoader::Describe` and in `WozCompatibility::ReadRequirements`, with two version checks |
| `033-casso-explorer` `019af0c9a` | 137 of 150 tasks; 5 behind master, 270 ahead; T082 (merge master) open | ProDOS subdirectory reading; `DiskOperations::CommitEdit` with the `ReloadInPlace` intent; `CassoExplorerPrefs`; Light and Dark themes; the list verbs and context menus | A modeless second window (Explorer has none); an async preview (it is synchronous on the UI thread); an editable hex view (033's `DxuiHexView` is read-only) |
| `035-debugger` `d3c15b55c` | US20 (disk breakpoints) is only on `035-disk-breakpoints-spec-2` `eae182ff5`; none of T712-T734 is built | The design of the three shared parts (T713 field formats and `??` matcher, T714 per-drive head, T715 write hook); the snapshot publish pattern (`DebugViewPublisher`, `PublishDebugSnapshot`) | `DiskTrackSnapshot`, `TouchTrack`, `GetTrackGeneration`, machine state, docking, `DebuggerThemes`: all 035-only, none on master |
| `041-disk-integrity` `e32b2b68c` | One code commit (soft-reset flush); T001 of 68 (140 in its working copy) done; 197 behind master, built on 035 | The contracts of `DurableCommit`, `DriveStatus` / `DriveStatusPublisher`, `ThreadOwnership`, the head stop at 158, `ReserveBlankTracks` | Any of them in code: `git grep` finds none of them on the branch |
| GH #170, #171, #172 | All open, no comments, **no fix on any branch, worktree or stash**; no live session holds them | Nothing yet | That a map-based track lookup or an in-place bit-track writer will exist before 040 builds its own |

## R1. Where the code lives

**Decision**: All logic in core libraries, per Principle VI.

- `CassoCore/`: the mark pattern and `??` matcher (`DiskMarkPattern`,
  `DiskFieldKind`), pure and shared with 035 and the AppleWin parser (R5).
- `CassoEmuCore/Devices/Disk/Inspector/`: the analyzer, decode settings,
  findings, summary, comparison, file map readers, export writers. Pure
  data-in, data-out over immutable track copies.
- `CassoEmuCore/Devices/Disk/`: the shared sector writer, `DurableCommit`, the
  loader changes, the image details.
- `CassoEmuCore/Ui/DiskInspector/`: the window, views and their view models,
  on Dxui.
- `CassoEmuCore/Shell/`: the Casso host wiring (menus, drive record, requests to
  the emulation thread).
- `CassoEmuCore/CassoExplorer/`: the Explorer hosts, after the rebase onto 033.

No code goes in `Casso/` or `CassoExplorer/`.

**Rationale**: Every FR in 040 except screen drawing is a function of bytes and
can be tested from `UnitTest` with made-up images. View models (hit testing,
zoom math, selection, keyboard navigation, tooltips) are core classes the
tests drive without a window.

**Alternatives**: A separate `CassoInspector` library. Rejected: the inspector
needs `DiskImage`, `WozLoader` and the drive constants, all in `CassoEmuCore`,
and a fourth core library adds build plumbing for no testability gain.

## R2. What the analyzer reads: immutable track copies

**Decision**: The analyzer takes a `TrackCopy` per track record: the record's
kind (bit or flux), its bits and bit count or its flux bytes, the record index,
and the guest-write count when copied. A `DiskCopy` holds every record, the
quarter-track map, and the image details (R8). Both are `shared_ptr<const>`
values; nothing in the analyzer touches a live `DiskImage`.

- In Casso, the emulation thread makes the copies (R12).
- In Explorer, a `DiskImage` loaded from the file on a worker thread makes them.

**Rationale**: FR-063 forbids reading the drive's disk while the guest can
change it, and 041 will assert on any window-thread read. The same type serves
both hosts and every test.

**Alternatives**: Analyze the live image under a lock. Rejected: it would hold
the emulation thread for the whole analysis (SC-005) and conflicts with 041's
ownership rule.

## R3. Framing nibbles as the drive does

**Decision**: A `LatchFramer` reproduces the read latch: it shifts cells into a
byte and emits a nibble when the high bit is set, starting from the index after
one full turn of the track so the latch state at the index matches a drive that
has been spinning (FR-008). It treats the track as a loop. Random-bit regions
come from the drive's own rule: a stretch longer than the MC3470 window with no
transition (`Disk2NibbleEngine::ApplyHeadWindow`, a 4-cell window,
`Disk2NibbleEngine.cpp:822-869`). The constant is shared with the engine, not
copied.

Flux tracks are decoded to cells at the drive's own cell
(`kFluxUnitsPerCell = 1408`, 45 units per tick, `Disk2NibbleEngine.h:125-127`)
through the same arithmetic `FluxBitView` uses, and the framer also keeps each
cell's recorded time (an interval read as n cells gives each cell interval/n,
FR-111) for the timing views.

**Rationale**: SC-002 compares the inspector's nibbles to what the drive
delivers on its second turn. Sharing the constants and the flux-to-cell rule
with the engine makes that a test of the framer, not of two divergent
decoders.

**Test**: SC-002 is a unit test that runs `Disk2NibbleEngine` over each test
track for two turns, collects the nibbles it latches, and compares them with
the framer's output outside random-bit regions.

**Alternatives**: Run the engine itself to frame. Rejected: it is stateful,
cycle-driven and random inside random-bit regions, and the analyzer needs cell
positions, extra-zero counts and widths the engine does not keep.

## R4. Analysis thresholds

**Decision**:

| Quantity | Value | Source |
|---|---|---|
| Data search window (FR-013) | 48 nibbles after the address epilogue | AppleEm. DOS 3.3 writes 5-10 sync nibbles between address and data fields, so 48 leaves room for slow-drive gaps without reaching the next address field (about 400 nibbles later) |
| Unformatted test (FR-016) | more than 50% of the track's cells in noise or random-bit regions | Spec; a constant, to be tuned against the real disks in `Apple2/Demos/` during the analyzer phase |
| Track length finding (FR-048) | more than 2% from 51,200 cells, WOZ bit and flux tracks only | Spec |
| Nominal cell | 4 CPU cycles, 3.91 µs, 31.29 flux ticks | The drive's own cell (Assumptions) |
| Nominal turn | 51,200 cells; 200 ms on flux | `kUnformattedTrackBits = 51200` (`Disk2NibbleEngine.h:61`) |
| Timing range default and limits (FR-024) | ±5%, adjustable ±1% to ±25% | Spec |
| Flux comparison tolerance (FR-118) | ±1% of a cell's time | Spec; the test pairs plant 4% |
| Nibble alignment limit (FR-119) | runs of up to 64 inserted or deleted nibbles | Spec |
| Follow head settle (FR-067) | 0.3 s | Spec |

Each is a constant in the class that uses it. The 50% and 2% values are
checked once against the demo disks before the analyzer phase closes; any
change is recorded here.

## R5. The parts shared with 035: field formats and the `??` matcher

**Decision**: 040 builds 035's T713 design exactly, with its file and type
names, so that whichever of 035 and 040 merges first provides it and the other
uses it (035 FR-162, spec Assumptions):

- `CassoCore/DiskMarkPattern.h/.cpp`: a mark of up to three nibbles, each a
  value or "any", parsed from text such as `D5 AA ??`; pure.
- `CassoCore/DiskFieldKind.h`: `Sixteen`, `Thirteen`.
- `CassoEmuCore/Devices/Disk/DiskFieldFormat.h/.cpp`: the prologues, 4-and-4,
  the 6-and-2 and 5-and-3 translate tables, body lengths 342 and 410,
  checksums, and the DE AA EB epilogue.
- Tests `UnitTest/Devices/DiskFieldFormatTests.cpp` and
  `DiskMarkPatternTests.cpp`.

What is shared is the pattern and its match. 040's "match standard marks too"
rule (FR-019) lives in 040's decode settings, not in the pattern.

The 5-and-3 tables are new to Casso: today's `NibblizationLayer` has only
6-and-2. They are written from Beneath Apple DOS, and SC-016 checks them
against vectors worked out by hand from that description.

**Rationale**: 035's spec-2 already fixes the names and the build-once rule.
Matching them now avoids a rename or a second copy at merge.

**Alternatives**: Reuse `NibblizationLayer`'s private tables. Rejected: they
are private to the codec, 6-and-2 only, and 035 needs them from CassoCore.

## R6. The per-drive head and the write hook (shared with 035)

**Decision**:

- **Per-drive head (035 T714, GH #135)**: 040 does not fix GH #135. The spec's
  edge cases give what the marker shows while it is open, and 035's US20
  specifies the fix. 040 reads each drive's engine for its own quarter track
  (`Disk2NibbleEngine::GetCurrentTrack`), which is what Casso holds for that
  drive today. If 035's T714 lands first, 040 reads `GetQuarterTrack (drive)`
  instead; the drive record (R11) hides the difference.
- **Write hook (035 T715, FR-161)**: 040 builds the hook if it merges first,
  to 035's exact rule: one private `Disk2Controller::OnLatchLoad`, called where
  `Write` loads the data latch with Q6 and Q7 set while the motor runs,
  spindown included; not called with the motor stopped or for the //c IWM
  mode-register load; it reports the drive, the nibble, the head's quarter
  track, and whether write protection dropped the write. 040's write counts
  (FR-068) are kept there. A dropped write increments the drive's
  write-blocked state, never a count.
- **Event listener**: 040 does not use `IDisk2EventSink`. The single sink stays
  with the Disk ][ debug window (035 FR-162). Everything 040 needs comes from
  the hook and the drive record.

**Rationale**: The hook is the one place both features count from, and 035
already wrote its rule. The listener is taken, and the inspector needs state,
not a stream of events.

**Alternatives**: Count writes from the dirty flags. Rejected: a save clears
them (`DiskImage.cpp:624-633`), and FR-068 says a save must not reset counts.
Count from `GetWriteNibbles`. Rejected: lifetime per engine, not per record.

## R7. Write and visit counts (FR-068)

**Decision**: `DiskImage` gains a per-record `guestWriteCount` (uint64, one per
slot), incremented in `OnLatchLoad` for every nibble the guest writes to that
record when the write is not dropped. "Written since inserted" is a count above
zero; "written since the edit" is a count that differs from the one the edit
recorded (FR-107). Each drive keeps a per-quarter-track `visitCount` (uint32 ×
160), incremented when the head arrives on a quarter track with the motor on,
or when the motor turns on while the head is there. Insert and reload reset
both; a save does not.

Neither is part of machine state in 040. If 035 merges first, its state version
bump decides whether they are saved; 040's position is that they reset on a
state load, like a reload.

**Rationale**: A per-nibble count is monotonic, cheap (one increment per write
latch), and covers every use the spec has for it: re-analysis (FR-069), marks
(FR-047), undo and redo availability (FR-107), and pending-edit staleness
(FR-108).

## R8. What Casso keeps from a WOZ file (FR-052, FR-053)

**Decision**: Two additions to `WozMetadata` (`WozMetadata.h`), filled by
`WozLoader::Load`, and one parser:

- `WozInfo`: every INFO field of the file's version, each later-version field
  with a `has...` flag (version, disk type, write protected, synchronized,
  cleaned, creator, sides, boot sector format, optimal bit timing, compatible
  hardware, required RAM, largest track, flux block, largest flux track).
  Filled by one parser, `static void WozLoader::ReadInfo (const std::vector<Byte> &, WozInfo &)`.
  `WozLoader::Description` holds a `WozInfo` that `Describe` fills through
  `ReadInfo`, and `WozCompatibility::ReadRequirements` reads from it with its
  signature unchanged. The sides offset becomes public
  (`kInfoOffsetDiskSides = 37`) and the file-static `kInfoDiskSidesOff` goes.
- `WozFileLayout`: the TMAP and FLUX maps as stored (160 bytes each), every
  track record's fields as the version stores them (v2: start block, block
  count, bit or byte count; v1: bytes used, bit count, splice point, splice
  nibble, splice bit count), the records no map refers to, the map each
  quarter track came from, the chunk list (ID, offset, size, in file order),
  the META entries in file order, the CRC result, and the image file problems
  of FR-051.

The image details the inspector shows (`ImageDetails`) are built from these
plus the file's name, size and read-only attribute, and in Casso the values
changed since load (FR-071).

`DiskImageStore.cpp:1501` copies only `passThrough` when it rebuilds metadata;
the new members are copied there too.

**Rationale**: woz-info-fields is merged locally (`d78d2941a`) and will
reach master first, so its INFO readings cannot be moved before it merges.
040 makes the single home after it lands, without touching that branch. The
woz-info-fields survey recommends this layout.

**Alternatives**: Re-read the file for the Image tab. Rejected: in Casso the
file can change under the mounted disk, and FR-071 shows the file as read at
insert.

## R9. The loader split for zero-length records (FR-053)

**Decision**: 040 makes the split in `WozLoader::ParseV2Track` and
`ParseV2FluxTrack` (`WozLoader.cpp:288`, `:365`), because 041 does not change
the parse (its documents mention only the audit note at its `audit.md:2493`):

| Record (as a map entry points at it) | Today | After |
|---|---|---|
| Start block, block count and bit or byte count all 0 | Empty slot, not damaged | Unchanged: empty slot, not damaged; FR-051 image file problem "map entry points at an empty record" |
| Count > 0, start block 0 or block count 0 | Empty slot, not damaged | Damaged, `DamageReason::RecordLocationMissing` |
| Start block 1 or 2 (inside the header) | Read from the header | Damaged, `DamageReason::RecordInHeader` |
| Bit record whose count exceeds its blocks | Not checked | Damaged, `DamageReason::CountExceedsBlocks` (already used for flux; owner decision, 2026-10-08) |
| TMAP or FLUX entry 160 to 254 | Skipped silently | Damaged, `DamageReason::MapEntryOutOfRange`, kept per quarter track |

Today's `DamagedTrack` is keyed by record. An out-of-range map entry has no
record, so `DiskImage` gains a per-quarter-track damage list for it, and
`GetDamagedQuarterTracks` merges both. Every damaged entry write-protects the
image as today (`DiskImage.cpp:511-518`).

**Coordination** (agreed with the 041 session, 2026-10-08): whichever branch
merges first makes the parse change. 041's planned `UnmapEmptyBitSlots` checks
damage by slot, so it would have missed `MapEntryOutOfRange`, which is kept
per quarter track. 041 is changing its rule so `ReserveBlankTracks` does
nothing on an image with any damage (`DiskImage::IsDamaged`); such an image is
write-protected, so every map entry stays as loaded whatever the reason.

## R10. Head limit

**Decision**: Both hosts read `Disk2Controller::kMaxQuarterTrack` (139 on
master, 158 after 041). The constant is public on master
(`Disk2Controller.h:41`) and lives in `CassoEmuCore`, which Explorer links, so
Explorer reads the same value. Tests derive expectations from the constant, so
they pass on both sides of 041's change. `DriveWidget::kMaxQuarterTrack` is
not used.

## R11. The drive record (FR-062) before and after 041

**Decision**: 040 publishes a per-drive `DriveHeadState` record from the
emulation thread once per frame, at the end of `RunCpuThreadFrame`, as one
`shared_ptr<const>` swapped under a leaf mutex: the publish pattern 035 uses
(`PublishDebugSnapshot`) and 041 plans for `DriveStatus`. Fields:

- From 041's planned `DriveActivity`, with its field names: `isMotorOn`,
  `headQuarterTrack` (-1 with no controller), `readNibbles`, `writeNibbles`.
- 040's additions: `angle` (fraction of one turn), `trackRecord` (the slot
  under the head, -1 for none), `isEnabled` (the controller's selected drive),
  `activity` (idle, reading, writing, write blocked, sticky for the frame),
  `isTurning` (spindown included), the write-protect causes
  (`GetWriteProtectInfo`), `mediaId` (changes on insert, eject, reload), and a
  `countsGeneration` with the per-record write counts and per-quarter-track
  visit counts (R7).

`Disk2NibbleEngine::GetAngle` becomes public (it is private,
`Disk2NibbleEngine.h:195`).

If 041 merges first, these fields move into its `DriveActivity` and 040's
publisher goes; if 040 merges first, 041 folds the record in. The struct is
laid out so the move is a rename. The counts are published only when their
generation changes, so the per-frame cost is one small struct per drive.

**Rationale**: The spec allows either order, and the inspector cannot wait for
041, which is gated behind 035. One record with 041's names keeps the merge
mechanical.

## R12. Requests to the emulation thread

**Decision**: An `InspectorRequestQueue` that the window thread posts into and
the emulation thread drains at the end of each service pass, between guest
accesses. Replies go into a mailbox the window takes once per window frame.
Requests:

| Request | Done on the emulation thread | Reply |
|---|---|---|
| `CopyDisk (drive)` | Copy every record, the map and the image details into a `DiskCopy` | The copy |
| `CopyTracks (drive, records)` | Copy the listed records | The copies with their write counts |
| `CopyAsInserted (drive)` | Return the drive's as-inserted copy (R13) | The copy |
| `ApplyEdits (drive, edit set)` | At a safe point (R14): write, re-analyze, save or roll back | Result |
| `UndoApply` / `RedoApply` | Same, with the stored records | Result |
| `Export (drive, quarter track)` | Copy one record | The copy |

Each request holds the drive's `mediaId`; a request for a disk that has
since changed gets the reply "disk changed" and does nothing.

This follows 041's rule that disk state changes only on its owner. The posted
`PostCommand` / `WM_APP_*` pattern 041 plans for salvage would also work, but
the inspector's traffic is frequent and bulk (track copies several times a
second during writes), so a typed queue with a mailbox avoids string payloads
and heap messages. Any request that changes the disk is classified as
state-changing for 035's `DivergenceGate` and declines while history is
replaying, once 035 is on master.

**Cost**: A whole-disk copy of a DOS 3.3 disk is about 230 KB; a 160-record
flux WOZ is a few MB. Copies happen on insert, on opening, and for written
tracks only (FR-069), which SC-005 measures.

## R13. "As inserted" copies

**Decision**: `DiskImageStore` keeps, per bay, a `DiskCopy` taken at mount and
at each reload, on the emulation thread, in the same step that resets the
counts (`MountExternallyModifiedDisk`, `DiskImageStore.cpp:3049-3086`, and the
mount path). Track buffers are shared with the live image until the guest
first writes a record, then copied (copy on first write), so an untouched disk
costs one pointer per record.

**Rationale**: FR-117 needs "As inserted" in Casso at any time, without having
opened the inspector earlier. Copy on first write keeps the cost at the
written records.

## R14. Safe points and the apply sequence (FR-103, FR-104)

**Decision**: An apply runs entirely on the emulation thread, between service
passes, so the machine does not run while it is in progress:

1. Wait for a safe point, checked at the end of each service pass: the drive
   is not writing (no write latch since the last pass and Q7 off), no flux
   write burst is open, and the head's position is outside every data field
   the apply changes (computed from the angle and the field's span), or the
   drive is not turning.
2. Keep each changed record as it was (for rollback and undo).
3. Write each sector through the shared sector writer (R15).
4. Analyze the changed records again and check every edited sector and every
   other field on those records (FR-100). On a mismatch, restore and reply.
5. Save through Casso's usual flush path and `DurableCommit` (R16), with any
   unsaved guest writes. On failure, restore every changed record and reply
   "Not applied" with the reason. On a save to a preserved copy, reply with its
   file name.
6. Reply; the machine runs again.

"Waiting for the drive to stop" waits for `isTurning` to fall; "Pause and
apply" sets the request to take the next safe point; "Run to a safe point and
apply" does the same from a paused machine and leaves it paused. A service
pass is at most one frame (17,030 cycles, 16.7 ms), so the check runs at least
60 times a second, and a 16-sector track's data fields cover about 40% of a
turn, so a safe point arrives within a few passes.

A guest write to a record the apply changes, while it waits, ends the wait
(FR-103): the request sees the record's write count change.

The edit never counts as a guest write (FR-109). `DiskImage` gets a separate
per-record `isChangedByWriter` flag, set by the shared writer for every caller and cleared by a save, for the NIB
save rule (R17). The Tracks tab's mark for records an apply changed (FR-109)
comes from the applied-edit history instead, since every apply saves at once.

## R15. The shared sector writer (FR-110 to FR-116)

**Decision**: `SectorFieldWriter` in `CassoEmuCore/Devices/Disk/` is the one
writer. Input is a list of `SectorWrite` (data-model.md); it locates each field
by running the analyzer's framer on the target record, so the writer and the
inspector agree on every field's place by construction.

- **Bit track**: encode the 256 bytes (6-and-2 or 5-and-3, standard table, or
  in the later release the window's table) into 342 or 410 nibbles and a
  checksum nibble (recomputed or the stored one kept). Write each nibble into
  the cells of the nibble at the same place, leaving its extra zero cells as
  they were. The nibble count never changes (FR-112); if it ever differs, the
  writer returns an error and changes nothing.
- **Flux track**: compute each cell's recorded time from the field's
  intervals (an interval read as n cells gives each cell interval/n), then lay
  the new transitions at the field's start plus the running sum of those
  times, so the field's total time and every transition outside it are
  unchanged (FR-111, FR-113). This replaces `WriteFluxTrackSectors`' write at
  the nominal cell (`NibblizationLayer.cpp:1627-1729`) and removes its blank
  track fallback (`:1686`, `:1764-1787`, FR-115).
- **NIB, NB2**: the same as a bit track; their nibbles have no extra zero
  cells, so the field changes in whole nibbles in place.
- **DSK, DO, PO**: through `DiskImage`, the same bit-track write on the built
  track; `Serialize` denibblizes it, so the file holds the logical sector's new
  bytes. Through `VolumeImage` (the `disk` command), the flat buffer is already
  the file, and the writer is not involved.
- **Map lookup**: `DiskImage::ResolveWholeTrack (N)` returns
  `ResolveQuarterTrack (4 × N)` and replaces every place that takes slot N as
  track N: `GetTrackBits` (`NibblizationLayer.cpp:715`), `DecodeTracks`
  (`:1407`), `RenibblizeTracks` (`:445-459`), `WriteFluxTrackSectors`
  (`:1641`, `:1725`). A record several quarter tracks read is changed once.
- **Caller policy**: the `disk` command and Explorer pass `Strict`: a record
  of its own on a quarter track between whole tracks stops every write
  (FR-114), and a track that does not decode completely stops the write
  (FR-115). The inspector's editor passes `Editor`: FR-096's rules instead.
- **Callers moved onto it**: `VolumeImage::SaveBitStream`
  (`VolumeImage.cpp:433-494`) decodes the prior state, diffs the flat volume
  bytes by sector, and hands each changed sector to the writer instead of
  calling `RenibblizeTracks`. Every `disk` command write (`sectorwrite`,
  `blockwrite`, `put`, `delete`, `boot`) and the assemblers' `--disk` output
  reach it through `DiskImageSession::SaveAndCommit`, and after the rebase so
  do 033's `RunMkdir`, `RunRmdir` and `DiskOperations::CommitEdit`.
  `RenibblizeTracks` is then unused by writes and is kept only where formatting
  needs it (`NibblizeWithMap` stays for `create` and `init`).
- **`TrackWritability::Evaluate`** (`TrackWritability.cpp:31-74`) takes
  FR-114's rule (a quarter track between N and N+1 that is unmapped or maps to
  N's or N+1's record is fine), gives the quarter track and record in its
  message, and stops treating unformatted tracks as writable (FR-115).

**GH #170**: the writer, the map lookup and the `Evaluate` rule fix all three
of #170's defects. No fix exists elsewhere today (Branch survey), so by the
owner's decision (2026-10-08) the writer and everything it needs go to master
first, as their own merge, before the rest of 040 (R29).

**GH #171 and #172** are not 040's: `disk create --volume` is formatting, and
the write-protect check belongs in `SaveAndCommit` for whoever fixes it. 040
leaves both alone and adjusts at merge if they land first.

**Alternatives**: Patch `RenibblizeTracks` to keep the volume and lengths.
Rejected: it still regenerates sync and sector places, loses timing bits, and
cannot keep a flux track's timing.

## R16. Durable saves (FR-116) before and after 041

**Decision**: 040 builds `DurableCommit` to 041's contract (its working copy,
`contracts/internal-interfaces.md:200-239`): `DurableCommit::Commit
(IDiskFileIo &, targetPath, bytes, invocationTag, CommitMode, progress)` with
`CommitMode::Replace` and `CommitMode::CreateNew`, writing a temporary beside
the target, copying the target's metadata, flushing to storage, then
`ReplaceAtomically` (`MoveFileExW` with `REPLACE_EXISTING | WRITE_THROUGH`) or
`RenameWithoutReplacing`, and removing the temporary on any failure.
`IDiskFileIo` gains `FlushToStorage`, `CopyFileMetadata` and
`RenameWithoutReplacing`.

040 routes through it only the saves it owns: the shared writer's callers
(`DiskImageSession::CommitImage`), the store's flush of an applied, undone or
redone edit, and "Save edited copy..." (`CreateNew`, or `Replace` after the
user confirms). Whichever of 040 and 041 merges first provides the class; the
other uses it. 041 then routes the rest (`WriteFileAtomically`, Cassque).

**Rationale**: Neither branch has built it, both need it, and 041's contract
already fixes the signature. Waiting for 041 would gate 040 behind 035.

**Status**: 041's contract is still being revised (041 session, 2026-10-08),
and the settled signature is to follow by message. 040 builds against it only
once it is settled.

**Test**: SC-017's "made to fail at each step" uses a fault-injecting
`IDiskFileIo` in `UnitTest`, one run per step. Real-file tests of
`FlushToStorage`, `CopyFileMetadata` and `RenameWithoutReplacing` go in
`ScenarioTests`, in their own scratch folder cleaned up before any assert (owner
decision recorded by 041, 2026-10-08).

## R17. Saving an edited disk (FR-104)

**Decision**:

- **WOZ**: `WozLoader::Serialize` rebuilds TMAP from `ResolveQuarterTrack`
  (`WozLoader.cpp:1496-1504`), which drops the TMAP entry of a quarter track
  FLUX also maps, writes unreferenced records as zeros, and unmaps zero-length
  records. With `WozFileLayout` (R8) kept, Serialize writes TMAP from the
  stored map for every quarter track whose record still exists, keeps a bit
  record that FLUX overrides, and keeps unreferenced records' bytes. This makes
  FR-099's "the bit record is left as it was" true after a save. Each track
  keeps its kind (spec 038).
- **NIB, NB2**: `NibbleImageCodec::Serialize` writes a record whose only
  change is an edit, undo or redo (the `isChangedByWriter` flag, R14, with no
  guest write) by copying the field's new nibbles into the stored track bytes
  at their place. A record with a guest write is rebuilt as today, and the
  window says so before the apply.
- **WOZ 1** is saved as WOZ 2, as today; the window says so before the first
  apply.

## R18. Analysis threading and cost

**Decision**: Each inspector window owns one `BackgroundWorkQueue`
(`CassoEmuCore/Shell/BackgroundWorkQueue.h`) for analysis. Work items are per
record, newest wins per record (a pending analysis of record 7 is replaced by
a newer copy of record 7), and each result holds the `mediaId` and the copy's
write count, so a result for a disk or a record state that is gone is dropped.
The window shows "Analyzing" for records not yet done (FR-021). Explorer's
preview uses one shared queue that keeps only the last selection.

**Cost targets** (SC-003): a 140 KB DOS 3.3 disk is 35 records of 50,624
cells; framing is one pass of shifts per cell, about 1.8 M steps, well under
100 ms. A 160-record flux WOZ is about 160 × 25,000 transitions; the
dominant cost is the comparison and timing arrays, measured in the analyzer
phase. If either target is missed, records are analyzed in parallel on a
small pool, since they are independent.

## R19. Drawing the platter (FR-022 to FR-031, SC-004)

**Decision**: The platter is drawn on the GPU by a polar-mapping pixel shader:
each pixel's radius and angle select a ring and a position along it, which
index a texture holding one byte per cell (its kind) and, for Timing mode, a
second texture holding each cell's deviation. A palette constant buffer maps
kinds to the theme's colors. Rings longer than the device's texture width wrap
onto several texture rows. Text and marks (nibble values, cell ticks, the
selection outline, the head marker, overlays) are drawn by the Dxui painter
over it, only for what is in view.

Dxui's painter has no texture or custom shader draw today (`IDxuiPainter.h`:
rectangles, quads, circles, ellipses, lines). 040 adds one generic Dxui hook, a
widget that renders D3D11 content into its rectangle in the window's frame
(`DxuiCustomVisual`), in the same pass the painter draws, and implements the
polar shader in `CassoEmuCore`. `Dxui3DRenderer` already composites textures
in the main window, so the device plumbing exists.

At fit, a pixel covers many cells; the shader picks the kind by FR-023's
priority over the cells it covers using a mip-like reduction texture built
once per analysis (failed checksum over noise over random bits over the rest),
so narrow features stay one pixel wide.

**Spike first**: the hook and shader are built and measured at the start of
the views phase, before any view depends on them. If the hook cannot be added
without disturbing Dxui's frame, the fallback is CPU tessellation through
`FillConvexQuad`, with per-ring run lists reduced to one run per device pixel
at the current zoom; that path is measured against SC-004 the same way.

**Rationale**: Zoom from fit to 600× over 160 rings of 51,200 cells is a
level-of-detail problem; a shader samples exactly the cells under each pixel
at every zoom with no tessellation, so frame time does not depend on zoom.

**Spike result (2026-10-09)**: The shader path holds, so the CPU fallback is
not needed. The hook is `IDxuiPainter::DrawCustom`, used through the
`DxuiCustomVisual` control. It flushes the shapes painted so far, then runs
the custom draw, and the shapes and text painted after it cover the result.
`DxuiHwndSource` hands the page's target to the painter at `Begin`. Popups
pass none, so there `DrawCustom` does nothing. The painter already sets its
full pipeline state at every flush, so the hook changes no state that any
other window relies on, and windows without a custom visual flush exactly as
before.

`PlatterRenderer` lays every distinct record's cells and their priority-max
levels end to end in one R8_UINT texture, 4096 texels wide. A per-ring table
holds each ring's cell count, state and level starts.

`PlatterRendererTests` measured it on the development machine's GPU with GPU
timestamps:
- 160 rings of 51,200 cells, none shared, 1400 pixels across.
- A zoom from fit to 600x about track 0 and back, 240 frames.
- Median 0.015 ms and worst 0.019 ms per frame, against SC-004's one
  refresh interval (16.7 ms) and 33 ms.

The window-level measurement, with the strip and the painter's marks, is
repeated once the window exists (T054).
## R20. The strip, Nibbles, Fields, Flux timing and Sector data views

**Decision**: The strip and the Flux timing plot use the painter directly
(rectangles and lines for what is in view, with run lists reduced per device
pixel). The Nibbles, Fields, Tracks, Findings, Image, File map and Differences
tabs use Dxui's list and grid widgets with virtualized rows.

**Sector data tab**: a `SectorByteView` of 040's own: 256 bytes in 16 rows,
hex and text columns as separate tab stops, overwrite-only editing, per-byte
pending and difference marks, and B's bytes beside A's while comparing. Its
selection and copy behavior follow 033's FR-012c, d, f and j.

The spec leaves to planning whether this tab moves onto 033's `DxuiHexView`
after the rebase. Decision: it does not. 033's view is read-only, 035 has
a separately edited copy with `WriteBytes` and `SetEditable`, and moving onto
either would make 040 a third party to a widget fork that 033 and 035 must
reconcile. A fixed 256-byte overwrite editor is small. After 033 and 035 have
merged a single `DxuiHexView`, a follow-up can move this tab onto it.

## R21. Themes (FR-077 to FR-079, SC-008)

**Decision**: A `DiskInspectorColors` struct (the Structure kinds, sector
states, map roles, pending edits, differences, timing fast, nominal and slow,
damage hatch, pending pattern, beyond-reach dimming, head marker states) is a
member of `DxuiTheme`, where a zero color means "use the fallback" (035's
pattern for `resultText` and `romText`). `DiskInspectorPalette::MakeFallback
(bool isDarkSurface)` in core gives the fallback. Values are set in
`CassoTheme`'s three factories and, after the rebase, in 033's
`DxuiLightTheme` and `DxuiDarkTheme`.

Contrast and ΔE2000 are computed by a unit test over every theme's resolved
palette (4.5:1 text, ΔE2000 ≥ 10 between Structure kinds and between map
roles), so SC-008's numeric part runs in CI; the on-screen captures cover the
rest.

States that must not differ by color alone (FR-079) get a symbol or pattern
from a fixed table in core (good ✓, bad ✗, not checked ?, no data field ∅,
address marks and data marks as distinct glyphs), drawn from
`UnicodeSymbols.h`.

## R22. Window and host structure

**Decision**:

- `DiskInspectorWindow : DxuiWindow` in `CassoEmuCore/Ui/DiskInspector/`,
  created with `Create (hInstance, hwndOwner, const CassoTheme *,
  IDiskInspectorHost *, bool activate)`, the 035 `DebuggerWindow` pattern.
- `IDiskInspectorHost` is the seam each host implements: the disk source
  (drive or file), requests (R12), saving preferences, theme changes, and in
  Casso the machine state. `NullDiskInspectorHost` lives in its header.
- **Casso**: `EmulatorShell` owns at most one window, created lazily like
  `OpenDisk2DebugDialog` (`EmulatorShellDebug.cpp:510-583`): lifetime lock,
  app icon, re-attach after a machine switch (`AttachDebugSinksIfOpen`,
  `:694`). Menu items in `EmulatorCommands.cpp:36-53` beside salvage, and
  rows in `ShowStorageContextMenu` (`EmulatorShellStorage.cpp:192-245`), which
  serves the desk scene, the drive band and the fullscreen strip.
- **Explorer**: one modeless `DiskInspectorWindow` per image, owned by a new
  `InspectorWindowSet` beside `CassoExplorerShell::m_window`, closed when
  Explorer exits and not reopened at launch (FR-075). Explorer has no modeless
  second window today; this set is 040's.
- **From 033** (on master once 033 merges): `CreateParams::paceFrames` is set on
  the inspector window, since the platter animates while the disk turns, and
  the tab rows use `DxuiTabStrip`'s standard style in both hosts.
- **Docking**: not used. The window is a fixed layout of two columns with a
  splitter. If 035's docking is on master when the views are built, it is
  still not adopted in 040.

## R23. Preferences (FR-080)

**Decision**: Casso stores a `diskInspector` block in `GlobalUserPrefs` (window
placement keyed like the main window's monitor-topology placements, mode,
tabs, timing range, Follow head, last drive, open at exit, "Show deleted
files", overlays), saved through `SaveGlobalPrefsDeferred`. Explorer stores the
same block, without the Casso-only items, in `CassoExplorerPrefs`. Zoom, pan,
selection and decode settings are never saved.

No secondary window has saved placement on master today; 035's debugger has
`SavePlacementIfMoved`. 040 follows that method and, if 035 is on master
first, uses its helper.

## R24. Explorer preview (FR-072, SC-009)

**Decision**: The preview pane gets a `DiskThumbnail` control above the
existing catalog or details preview, not a new `PreviewContent::Kind`, because
the spec keeps 033's preview whole and adds the platter above it. On selection
of an image, the catalog preview is filled as today; then the image is read and
analyzed on the shared preview queue (R18) and the thumbnail and chips appear
when done, or are dropped if the selection moved on. The thumbnail is drawn
by the same renderer (R19) at a fixed small size.

`CassoExplorerBrowser::UpdatePreview` runs on the UI thread and has no
cancellation today (033 survey); the thumbnail's work never runs there.

## R25. Explorer context menus and commits

**Decision**: "Inspect disk image" and "Compare disk images" are new
`CassoExplorerActions::Verb`s, added in `GetListVerbs`' single-image and
two-image branches, on the list background in an image location, and in
`ShowTreeContextMenu`. An edit applied in Explorer goes through a new
`DiskOperations::ApplySectorEdits` that ends in `CommitEdit`, so it keeps 033's
stale-file check and the `ReloadInPlace` intent (FR-105, FR-106), and through
the shared sector writer with `CommitImage` on `DurableCommit`.

The folder watcher already reloads a location when an image's folder changes;
the inspector window watches its own file through the same watcher and
re-reads it once per change, and after its own save, analyzes only the edited
records (FR-074).

## R26. The file and sector map (FR-083 to FR-095)

**Decision**: Four map readers in `CassoEmuCore/Devices/Disk/Inspector/FileMap/`:
`Dos33MapReader`, `ProDosMapReader`, `PascalMapReader`, `CpmMapReader`. Each
reads only through a `SectorSource` built from the analysis (each logical
sector or block half with its bytes and result: good, not checked, bad,
missing), so it never decodes a track again and always agrees with the Sector
data tab (FR-083).

They are walkers of their own rather than `IVolume`:

- They need what `IVolume` does not give: roles for every structure sector,
  deleted entries, forks (storage type $5), holes, results per sector, chain
  damage with its place, and the "volume found" test of FR-084 and FR-085.
- `IVolume` works on a flat 143,360-byte buffer and fails a whole read on
  damage, which FR-092 forbids for the map.

They reuse the skeletons' layout constants (`Dos33Skeleton.h`,
`ProDosSkeleton.h`; after the rebase, 033's subdirectory header constants at
`ProDosSkeleton.h:110-123`), not their logic. ProDOS subdirectories and forks
inside them are built after the rebase onto 033, as the spec orders.

Every walk is bounded: a chain stops at a loop (visited set), a pointer
outside the volume, a bad or missing chain sector, or a length beyond the
volume's sector count, so the map always finishes (SC-015).

Apple Pascal and CP/M readers are written from public descriptions (the
Apple Pascal directory in ProDOS block order; CP/M 2.2's directory and the
Apple II CP/M skew and allocation of FR-085). No CiderPress II or cpmtools
code is used.

## R27. Comparison (FR-117 to FR-122)

**Decision**:

- **Alignment**: on the first address field both tracks hold (by sector and
  encoding); with none, by the rotation at which the most cells match, found
  by voting over shared 8-nibble sequences (each match proposes an offset), then
  checked at the cell level. This is linear in track length, where trying every
  rotation is quadratic.
- **Nibble differences**: a Myers O(ND) diff over the aligned nibble
  sequences, with D capped by the alignment limit (64) per run; a run past the
  cap is marked whole as different.
- **Flux timing**: cell by cell after alignment, within ±1%.
- **Verdicts**: computed in the order of FR-118, strongest first.
- **Files**: paired by path within the volume shown in the File map tab,
  repeated paths paired in catalog order; contents compared as FR-120 defines
  them.
- **Re-comparison**: only changed records, by the same write-count rule as
  re-analysis (FR-122).

Timing targets (SC-021: two 140 KB disks under 1 s, two 160-record flux WOZ
images under 5 s) are measured in the comparison phase.

## R28. Export (FR-058)

**Decision**: Three writers in core, each taking a copy, never the live disk:
sectors (raw 256-byte sectors in the chosen order, with the list of bad, not
checked and missing sectors returned to the window before writing), track
nibbles (one byte per framed nibble from the index), and track bits as a
one-record WOZ 2.1 built by `WozLoader::BuildSyntheticV21` with the record's
bits or flux bytes unchanged. Files are written through `DurableCommit` with
`CreateNew`, or `Replace` after the user confirms.

## R29. Delivery, merge order and rebases

**Decision**:

1. Merge master into 040 now (woz-info-fields once `woz-merge` is pushed).
2. **Merge 1** (owner decision, 2026-10-08): the shared sector writer and what
   it needs (field formats, framer and field locator, the stored maps for
   saves, `DurableCommit`) go to master on their own as the GH #170 fix (plan,
   Phases 0 to 6). It does not depend on 033.
3. Build the rest that does not need 033 on 040 (plan, Phases 7 to 13),
   merging master early and often (CLAUDE.md rename hazard).
4. When 033 has merged master (its T082), rebase 040 onto 033, then build the
   Explorer phase. Merge 1's commits are already upstream and drop out of the
   rebase.
5. **Merge 2**, the rest of the first release, goes to master only after 033
   is on master.
6. For each part built once (field formats and matcher, write hook,
   `DurableCommit`, the drive record's fields, the loader split), whichever of
   035, 040 and 041 merges first provides it, and the others adapt. Before
   building each part, 040 checks whether it is already on master.

The later release (User Stories 11 to 16) gets its own plan update and merge.
The first release reserves only the seams in plan.md, Later release.

## R30. Later-release seams reserved in the first release

**Decision**: These members and types are in the first release so the later
release adds to them rather than changing them; none has any behavior in the
first release:

- Decode settings are a list of per-track-range entries, each with marks,
  checks and a slot for encoding, table, checksum seeds and sector-number rule
  (FR-124).
- The analyzer's per-track decoders are a list of `ITrackFormatDecoder`, with
  the 16-sector and 13-sector decoders as its first two members (FR-141).
- A `TrackCopy` holds one revolution, inside a `TrackCaptures` holding a list
  of them, so A2R's several revolutions (FR-137) fit without a type change.
- The Nibbles tab's lane list has one lane, so the slipped framing (FR-128)
  adds a second.
- Findings hold a category enum with a reserved `Protection` value.
- "Compare with..." takes a source list in which quarter tracks of the same
  disk can be a source (FR-145).
