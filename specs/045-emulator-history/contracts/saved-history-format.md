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
| Fingerprint | `HFMT` | 64-bit checksum over the tag and version of every top-level section each state part of the machine writes (`MachineHost::ComputeStateFingerprint`) |
| Settings | `HSET` | `intervalCycles`, `wholeEvery`, `longestGroup` |
| Segments | `HSEG` | Segment count; per segment: `id`, `parent` (0 for a branch forked from the active path), `forkPosition`, `forkCycle`, `firstJournalIndex`, `leftAtWallTime`, live-end position and cycle, keyframe count; then the next branch id. The first segment is the active path, whose id, parent and fork fields are zero; the branches left behind follow, oldest first |
| Keyframes | `HKEY` | Per segment, oldest first: every `KeyframeInfo` field, and the keyframe's offset into `HARN` |
| Arena | `HARN` | Byte count, then the packed bytes and side bytes of every keyframe, end to end in segment order |
| Journal | `HJRN` | Per segment: entry count, then every `InputRecord` field; the active path's are the journal's from its first index |
| Media | `HMED` | Per bay with a medium: slot, drive and the medium's id, which the keyframes use for it |
| Branch disks | `HBDV` | After 041 (R-011): per segment, its `BranchDiskVersion` entries |
| Resume | `HPOS` | The position and cycle the machine was at when saved; equal to the live end unless it was behind live |

The disks themselves are the payload's, as every state file has them. Until Phase 7, history is not saved while the disk store retains a disk that left a bay, since its keyframes may refer to it: such a file is written with no section.

## Reading

| Condition | Result |
|---|---|
| No bytes after the payload | The state loads; history starts fresh at the load (a file saved with history off) |
| Section tag, size or hash wrong | The section is ignored; the state loads; history starts fresh; no message (FR-024) |
| `HFMT` differs from the running build's | As above |
| Any section fails to parse | As above |
| Arena larger than the current budget | The oldest groups are dropped, branches left behind first (FR-016), until it fits |
| Saved with another keyframe interval | As a bad section |
| The bays with media differ from `HMED`'s | As a bad section |
| All present and valid | Each bay's medium is given its saved id, and the id counter moves past it; the store is rebuilt with these segments; the machine state is the live end of the active branch, where a boundary keyframe is taken; the machine then seeks to `HPOS`, paused |

**Rules**:

- The state in the payload is always the live end of the active branch. A save made behind live seeks there first and records the position on screen in `HPOS` (R-010, FR-020).
- Loading a file with a history section replaces the current history. Loading one without makes the load a marked point in the current history (`InputKind::StateLoad`), and forks it when the machine is behind live (FR-025).
- With history off, a save writes no section, and a load ignores any section.

## Writing

The section is laid out from the store directly: settings, segments, keyframe infos, then each keyframe's stored bytes copied in order, so the file has none of the arena's free space. The writer waits for keyframes in flight to finish packing before laying out the arena. At exit, the section is built as the CPU thread stops, before recording stops, and the automatic save then writes it to the temporary file the state file already uses, and renames it over the old file.

## Tests

- Round trip: a history with forks, saved and loaded, has the same segments, keyframe checksums, journal records and fork marks, and a branch switch after the load lands at the branch's live end with its checksum (SC-004, SC-007).
- Each discard row above, from synthetic bytes: the state loads, history is fresh, and no error is reported.
- A file in format version 3 loads, with no history.
