# Contract: Saved History in a Machine State File

**Requirements**: FR-022 to FR-026, SC-007 | **Research**: R-010

`MachineStateFile` writes every save (the automatic state at exit and at a machine switch, and File > Save state) in format version 4 while history is on. Version 4 is version 3 with an optional history section after the payload. A file without history is written as version 4 with no section.

## File layout

```text
magic             8 bytes   "CASSOSTA"
format version    2 bytes   4
reserved          2 bytes   zero
payload size      8 bytes
payload hash      8 bytes   StateHash over the payload as stored
payload           as version 3: size before compression, then the 'SFIL' section packed with MSZIP
history section   optional; present when the file continues past the payload
```

All values are little-endian. A reader checks the header, the payload size and the payload hash first, as version 3 does; the history section is read only after the payload parses.

## History section

```text
tag               4 bytes   'HIST'
section version   2 bytes   1
reserved          2 bytes   zero
body size         8 bytes
body hash         8 bytes   StateHash over the body
body              a StateWriter stream of the sections below, in this order, stored unpacked
```

The body is not compressed: the keyframes in it are packed already (XPRESS, by `KeyframeStore`).

| Section | Tag | Contents |
|---|---|---|
| Fingerprint | `HFMT` | 64-bit checksum over the tag and version of every section `MachineHost::SaveState` writes for this machine |
| Settings | `HSET` | `intervalCycles`, `wholeEvery`, `longestGroup` |
| Segments | `HSEG` | Segment count; per segment: `id`, `parent` (`UINT32_MAX` for the root), `forkPosition`, `firstJournalIndex`, `leftAtWallTime`, live-end position and cycle, keyframe count; then the active leaf's `id` |
| Keyframes | `HKEY` | Per segment, oldest first: every `KeyframeInfo` field, and the keyframe's offset into `HARN` |
| Arena | `HARN` | Byte count, then the packed bytes and side bytes of every keyframe, end to end |
| Journal | `HJRN` | Per segment: record count, then every `InputRecord` field; the active path's records are listed under their segments |
| Media | `HMED` | Per media id a keyframe refers to: `mediaId`, path (UTF-8), `DiskFormat`, `ImageIdentity`, image byte count, image bytes |
| Branch disks | `HBDV` | After 041 (R-011): per segment, its `BranchDiskVersion` entries |
| Resume | `HPOS` | The position and cycle the machine was at when saved; equal to the live end unless it was behind live |

## Reading

| Condition | Result |
|---|---|
| No bytes after the payload | The state loads; history starts fresh at the load (a file saved with history off) |
| Section tag, size or hash wrong | The section is ignored; the state loads; history starts fresh; no message (FR-024) |
| `HFMT` differs from the running build's | As above |
| Any section fails to parse | As above |
| Arena larger than the current budget | The oldest groups are dropped, branches left behind first (FR-016), until it fits |
| A media file's `ImageIdentity` differs at load | The disk mounts from its file; a gap mark at the load's keyframe; the stretch before it can be viewed and replayed, and a change there is ignored with a notice |
| All present and valid | The store is rebuilt with these segments; the machine state is the live end of the active branch; the machine then seeks to `HPOS`, paused |

**Rules**:

- The state in the payload is always the live end of the active branch. A save made behind live seeks there first and records the position on screen in `HPOS` (R-010, FR-020).
- Loading a file with a history section replaces the current history. Loading one without makes the load a marked point in the current history (`InputKind::StateLoad`), and forks it when the machine is behind live (FR-025).
- With history off, a save writes no section, and a load ignores any section.

## Writing

The section is laid out from the store directly: settings, segments, keyframe infos, then each used extent copied in order, which also compacts the arena's free space out of the file. The writer waits for keyframes in flight to finish packing before laying out the arena. The automatic save at exit writes it to the temporary file the state file already uses, and renames it over the old file.

## Tests

- Round trip: a history with forks, saved and loaded, has the same segments, keyframe checksums, journal records and fork marks, and a branch switch after the load lands at the branch's live end with its checksum (SC-004, SC-007).
- Each discard row above, from synthetic bytes: the state loads, history is fresh, and no error is reported.
- A file in format version 3 loads, with no history.
