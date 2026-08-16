# Phase 1 Data Model: Disk File Access for the Build Loop

**Feature**: `specs/020-disk-file-access` | **Date**: 2026-08-15

Entities are plain data owned by `CassoEmuCore`. Nothing here holds a handle, a
window, or a path to the host — the shell reads bytes in and writes bytes out, so
every type below is constructible from a byte buffer and assertable in a unit
test.

---

## Layering

```text
   image file bytes  ── shell ──▶  DiskImage  ──▶  flat 143,360-byte sector buffer
                                    (existing)      (NibblizationLayer::Denibblize,
                                                     extended per FR-018)
                                                             │
                                                             ▼
                                                    IVolume  ── Dos33Volume
                                                             └─ ProDosVolume
                                                             │
                                                             ▼
                                              VolumeIntegrityReport
                                                             │
        shell ◀── complete new image bytes ◀── write path ◀──┘  (checked pre-commit)
```

The flat sector buffer is the single currency between the track layer and the
filesystem layer. Both volume types address it; neither knows what image format
it came from.

---

## SectorDecodeReport

What denibblization recovered, per track and per sector. Fills the gap described
in research R-002 and satisfies FR-018.

| Field | Meaning |
|---|---|
| `trackCount` | Tracks examined |
| `sectorRecovered[track][sector]` | Whether this sector decoded to a valid standard sector |
| `trackFullyRecovered(track)` | All sixteen sectors recovered |
| `isFullyRecovered` | Every track fully recovered |
| `unrecoveredCount` | Total sectors not recovered |

**Rules**

- A sector that did not decode MUST be marked unrecovered rather than left as
  zeros indistinguishable from genuinely zeroed data (FR-018, Edge Cases).
- Decoding MUST continue past a failed sector rather than abandoning the rest of
  the track — the present `break` is the defect this replaces.
- The report is an output of reading, not of writing: it describes the image as
  found.

---

## TrackWritability

Derived from `SectorDecodeReport` plus two whole-image checks. Answers FR-016,
FR-017, FR-019.

| Field | Meaning |
|---|---|
| `imageRefusalReason` | Set when the whole image is unwritable — a quarter-track map resolving off whole-track positions, or metadata declaring timing-sensitive capture. Checked before any track is decoded. |
| `isTrackWritable(track)` | The track decoded to sixteen distinct valid standard sectors |

**Rules**

- Writability is **positive proof**, never protection-scheme recognition (FR-019).
- A write is refused only when it needs a track that is not writable (FR-016) —
  an unwritable track elsewhere on the disk does not block it.
- Tracks not being written are never re-encoded (FR-017).

---

## IVolume — the filesystem seam

One interface, two implementations. The two filesystems share almost no structure
below this surface (research R-001), so the seam is deliberately narrow.

| Operation | Notes |
|---|---|
| `Enumerate` | Every entry the catalog yields, plus free space and a damage report |
| `Read` | One file's payload by path |
| `Write` | Add or replace, computed whole (FR-012, FR-013) |
| `Delete` | Free only uniquely-owned space (FR-011) |
| `BuildIntegrityReport` | The pass of FR-037..FR-040 |
| `SetStartupProgram` | Mechanism differs entirely per filesystem (R-003, R-004) |

Every mutating operation takes the current sector buffer and produces a **new**
one. None mutates in place. That is what makes FR-013's all-or-nothing guarantee
structural rather than disciplined.

---

## FilePath

Files are addressed by path from the outset (FR-009), so subdirectory support
fills in a capability instead of changing a signature later.

| Field | Meaning |
|---|---|
| `components` | One or more name components |
| `isRooted` | Leading separator present |

A DOS 3.3 path always has exactly one component. A ProDOS path may have several;
volume-directory-only support means paths longer than one component are refused
with a clear reason until traversal lands, not silently truncated.

---

## FileEntry

One catalog record, normalized across both filesystems. Fields a filesystem does
not store are absent rather than zero, so a caller can tell "no load address"
from "loads at $0000".

| Field | DOS 3.3 | ProDOS |
|---|---|---|
| `name` | 30 bytes, high ASCII, `$A0`-padded | up to 15, length in the type nibble |
| `type` | catalog type byte, lock bit masked off | file type byte |
| `isLocked` | `$80` bit of the type byte — **verified**: the stock master's HELLO is `$82` | access byte |
| `sizeInSectors` / `sizeInBlocks` | sector count | blocks used |
| `eofBytes` | absent — DOS 3.3 does not store it | 3-byte EOF |
| `loadAddress` | from the file's own header for `B` files | `auxType` for `BIN` |
| `auxType` | absent | present |
| `timestamps` | absent | creation / modification where present |
| `storageKind` | track/sector list | seedling / sapling / tree |

**Rules**

- A name that is not legal on the target filesystem MUST be reported as rejected,
  never truncated or transliterated (Edge Cases).
- Lock state is enforced on overwrite (FR-014).

---

## FilePayload

The bytes of one file plus what is needed to place it correctly.

| Field | Meaning |
|---|---|
| `bytes` | Contents, already in on-disk form |
| `type` | Target file type |
| `loadAddress` | Where the filesystem stores one (FR-020) |
| `encoding` | Raw, host text, or Applesoft listing — selects the conversion (FR-021, FR-022) |

Text conversion is bidirectional and lossless in the round trip that matters:
host text to high-ASCII with the target's line ending on the way in, the reverse
on the way out (FR-021).

---

## VolumeIntegrityReport

The output of the one pass with four consumers (research R-005; FR-037..FR-040).

| Field | Meaning |
|---|---|
| `claimedBy[unit]` | Which entries claim each sector or block — empty, one, or several |
| `crossLinked` | Units claimed by more than one entry |
| `allocatedButUnclaimed` | Marked used in the free map, claimed by no readable entry |
| `claimedButFree` | Claimed by an entry, marked free — the map is already wrong |
| `unfollowableChains` | Entries whose chain could not be walked to its end, including those that hit the traversal bound |
| `catalogFullyParsed` | False when some catalog entries were unreadable |
| `isClean` | No cross-links, no disagreement, no unfollowable chains |

**Rules**

- **Termination is mandatory** (FR-038). Traversal is bounded by a visited set
  and a ceiling derived from the volume's own capacity. A chain hitting the bound
  is recorded as unfollowable, never followed further. This pass runs by design
  on volumes chosen for being damaged.
- **Delete frees only uniquely-owned units** (FR-011) — those where `claimedBy`
  names the deleted entry and nothing else. Everything else is reported leaked.
- **`allocatedButUnclaimed` is never freed.** It is either already-leaked space or
  an invisible file's data, and refusing to guess between them is correct
  behavior, not a gap.
- **The guarantee is bounded by `catalogFullyParsed`** (FR-040). When the catalog
  did not fully parse, an unreadable entry claims nothing observable, so a unit it
  shares with the deleted file would be freed. This needs catalog damage *and*
  cross-linking together — narrow, but the one case the rule can still lose data.
  It MUST be warned about distinctly rather than assumed away.
- **Every write runs the pass over its computed result** and refuses to commit a
  result that fails it (FR-039).

---

## CommitPlan

What the shell needs to land a computed image safely (FR-013, FR-036, R-007).

| Field | Meaning |
|---|---|
| `imageBytes` | The complete new image — the only thing written |
| `expectedSize` / `expectedModifiedTime` | Recorded at read, re-verified immediately before commit |

**Rules**

- Commit writes to a uniquely named temporary file beside the target, then
  replaces atomically. The temporary is removed on any failure and its name
  cannot collide between concurrent invocations.
- Re-verification failure refuses the commit. It cannot detect a write landing
  after the commit, and the documentation says so (FR-036).

---

## StartupProgram

Deliberately **not** unified behind one "write the boot name" helper — the two
mechanisms are different in kind (research R-003, R-004).

| Filesystem | Mechanism |
|---|---|
| DOS 3.3 | Patch the greeting filename inside the DOS image at **T01 S09 `+$75`**, 30 bytes, high ASCII, `$A0`-padded (verified against the stock master). No catalog change. |
| ProDOS | Reorder the volume directory so the desired `SYS` file is the first one the boot path finds. No stored name exists to patch. |

Setting a startup program that is not present on the volume is refused (FR-024).
