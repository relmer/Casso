# Data Model: Machine History in the Emulator

**Feature**: [spec.md](spec.md) | **Research**: [research.md](research.md)

Types are given as the plan proposes them; identifiers follow the tree's rules (verb-first functions, `m_` members). Existing types are marked as such.

## Keyframe (existing: `KeyframeInfo`, `KeyframeStore::Entry`)

One whole-machine snapshot, packed in the arena.

| Field | Type | Notes |
|---|---|---|
| `position` | `uint64_t` | Instructions retired since history began, along its path |
| `cycle` | `uint64_t` | CPU cycle count when taken |
| `journalIndex` | `size_t` | The journal's end index when taken, valid along its path |
| `checksum` | `uint64_t` | Over the whole state: CPU, RAM, devices, disk media |
| `stateBytes`, `storedBytes`, `sideBytes` | `size_t` | Unpacked size; packed size with side bytes; side bytes alone |
| `wallTime` | `uint64_t` | Host UTC FILETIME; not part of the state |
| `gapStart`, `hasGapBefore` | | A gap before this keyframe (R-007, R-010) |
| `isWhole`, `isBoundary` | `bool` | Whole or a difference against its group's whole snapshot; loaded by a replay rather than compared |
| `offset` | `size_t` | Where its packed bytes start in the arena |
| **new** `isLiveEnd` | `bool` | Captured where the machine moved from live into history; a branch switch loads it |

**Rule**: a difference keyframe and its group's whole snapshot may be in different segments, as long as the whole snapshot is on the path from the root to the difference.

## Segment (new: `KeyframeStore::Segment`)

A run of keyframes and the journal records between them; the store's tree.

| Field | Type | Notes |
|---|---|---|
| `id` | `uint32_t` | Unique in the store; saved with history |
| `parent` | `uint32_t` | The segment it forks from; none for the root |
| `forkPosition` | `uint64_t` | The position where it begins; its first keyframe is after it |
| `entries` | ring of `Entry` | Its keyframes, oldest first |
| `records` | `vector<InputRecord>` | Its journal records, for a segment off the active path (R-005) |
| `firstJournalIndex` | `size_t` | The absolute index of its first record |
| `leftAtWallTime` | `uint64_t` | Host time when the user left it; zero while on the active path |
| `liveEnd` | position and cycle | The newest moment of the branch that ends in it |
| `diskVersions` | list of `BranchDiskVersion` | After 041: disks that left a bay on it after the fork (R-011) |

**Rules**:

- The root segment has no parent. Every other segment's `forkPosition` lies within its parent's span.
- The active path is the chain from the root to the active leaf. `KeyframeStore`'s index interface presents its keyframes in order (R-004).
- A segment with no keyframes and no children is removed, with its fork mark.

**State transitions**:

```text
                    fork at P (a change behind live)
  active leaf S  ───────────────────────────────────▶  S split at P:
                                                        S (to P), child L (after P, left behind),
                                                        child N (empty, active, recording)

                    switch to branch B
  active path A  ───────────────────────────────────▶  path to B is active; A's segments after
                                                        the common ancestor are left behind;
                                                        machine loads B's live-end keyframe, paused

                    eviction (room short)
  left segment   ───────────────────────────────────▶  oldest group dropped; empty segment removed
  more than 8 branches left behind                      oldest branch dropped whole
  no branch left behind                                 active path's oldest group dropped
```

## Branch (derived)

Not stored: a leaf segment and the path to it. Its **live end** is the leaf's `liveEnd`; its **fork point**, as the active path draws it, is the `forkPosition` of the first segment on its path that is not on the active path.

| Derived value | Source |
|---|---|
| Last frame | The leaf's live-end keyframe, drawn by the history renderer |
| How long it ran past the fork | Live-end cycle less the fork point's cycle |
| When the user left it | The leaf's `leftAtWallTime` |

## Fork point (derived)

A position on the active path where one or more branches left behind begin. Drawn as one mark, with a count when more than one branch begins there (FR-010).

## Timeline mark (new: `DxuiStripMark`)

What a strip source returns from `GetMarks` (R-009).

| Field | Type | Notes |
|---|---|---|
| `offset` | `float` | In cells from the leading edge, as the playhead's |
| `kind` | `DxuiStripMarkKind` | `Gap`, `Reset`, `PowerCycle`, `DiskChange`, `WriteProtect`, `StateLoad`, `Fork` |
| `count` | `int` | Branches at a fork mark; 1 otherwise |
| `tip` | `wstring` | The kind and the time, labeled by time only (FR-015) |
| `id` | `uint64_t` | The source's key for a click or hover on the mark |

**Sources**: `Reset`, `PowerCycle`, `DiskChange` (`DiskMount`, `DiskEject`) and `WriteProtect` (`DriveWriteProtect`, `ImageWriteProtect`) from the journal records on the active path; `StateLoad` from a new `InputKind::StateLoad`; `Gap` from `hasGapBefore`; `Fork` from the segment tree.

## Input record (existing: `InputRecord`; new kind)

`InputKind::StateLoad`: a state file without history was loaded at this position (FR-025). The load is a boundary keyframe after it, as a mount is.

## Branch disk version (new, after 041: `BranchDiskVersion`)

| Field | Type | Notes |
|---|---|---|
| `mediaId` | `uint64_t` | The disk, as `DiskImageStore::GetMediaId` reports it |
| `packedImage` | `vector<Byte>` | Its image when it left the bay, packed with `SnapshotCompressor`; counted against the budget |
| `checksum` | `uint64_t` | Of the unpacked image, for the comparison at a switch |

**Rule** (FR-018, FR-021, SC-005): after a fork or a switch, every disk touched after the fork on either path is compared by checksum between the old and new active versions, and each that differs is written through the store's commit.

## History settings (new: `HistorySettings` in `GlobalUserPrefs`)

| Field | Type | Default | JSON key |
|---|---|---|---|
| `isRecording` | `bool` | `true` | `reverseRecording` |
| `budgetMb` | `int` | 64 | `reverseBudgetMb` |

The keys are the ones the debugger's options used, so builds that share the preferences file read the same settings. The keyframe interval is not a setting: it is `KeyframeSettings::kDefaultFrames`, 10 video frames. An older build's `reverseIntervalFrames` key is kept in the file as an unknown key.

The budget's range is `HistorySettings::kMinBudgetMb` to `kMaxBudgetMb`.

## Saved history (new: a section of `MachineStateFile` version 4)

The byte layout is in [contracts/saved-history-format.md](contracts/saved-history-format.md).

| Part | Notes |
|---|---|
| Format fingerprint | A checksum over the running build's state sections and versions; a mismatch discards the section (FR-024) |
| Keyframe settings | Interval, `wholeEvery`, `longestGroup` |
| Segment tree | Every segment's fields above, the active leaf, and every keyframe's `KeyframeInfo` |
| Arena extents | The used extents, end to end; offsets rewritten on load |
| Journal | The active path's records, and every other segment's |
| Media table | Per media id: path, format, `ImageIdentity`, and the image as in the newest keyframe that refers to it |
| Resume position | Where the machine was when saved; the launch seeks there, paused (R-010) |

## Ownership (R-003)

```text
EmulatorShell
├── ShellHistory (new)          ReverseHost, DivergenceGate, HeldInputWatch, DeferredHistoryEdits,
│                               ReplayControl, the replay caption and indicator state,
│                               HistoryThumbnails and ScratchMachineRenderer for each strip
└── ShellDebugger               the debugger window and session; heat map, call stack and
        │                       event log replays; reverse continue's stop test
        └── ShellHistory &
```
