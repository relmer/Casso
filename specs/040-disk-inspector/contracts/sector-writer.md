# Contract: Shared sector writer

**Covers**: FR-110 to FR-116, SC-016, SC-017. **Decisions**: research R15, R16, R17.

## Interface

```cpp
class SectorFieldWriter
{
public:
    static HRESULT Write (DiskImage                    & image,
                          const std::vector<SectorWrite> & writes,
                          std::vector<SectorWriteError>  & outErrors);
};
```

- All or nothing: if any write fails its checks, `image` is unchanged, every
  failure is in `outErrors`, and the result is a failure HRESULT. It never
  returns success after a partial change.
- Each record is changed once, however many quarter tracks read it and however
  many sectors on it are written.
- Records are found with `DiskImage::ResolveWholeTrack`, never by taking slot
  N as track N.

## What changes on the record

| Track kind | Changed | Unchanged |
|---|---|---|
| Bit (WOZ, built from NIB, NB2, DSK, DO, PO) | The cells of the 342 or 410 data nibbles and the checksum nibble | Every other cell, each nibble's extra zero cells, the record's bit count |
| Flux | The transitions inside the data body and checksum, placed by the cells' recorded times | Every transition outside them, the field's total time, the time of one turn |
| Quarter track in both TMAP and FLUX | The flux record | The bit record and its TMAP entry |

A field that spans the end of the track is written whole. A nibble count that
differs from the encoding's makes the writer change nothing and report it.

## Caller policies

| Rule | `Strict` (`disk` command, Explorer) | `Editor` (inspector) |
|---|---|---|
| Quarter track between N and N+1 maps to a record other than N's or N+1's | The write fails for the whole image; message gives the quarter track and its record | Allowed; the confirmation lists every quarter track that reads the record |
| Track does not decode completely (16 sectors, once each, standard marks, good checksums) | The write fails; message gives the track and the reason | FR-096's rules: editable when the address field passes or is not checked, no noise in the field, nibbles in the table, record not damaged; a bad data checksum is editable |
| No standard address field, or nothing recorded | The write fails; no track is formatted | "Edit sector" unavailable |
| Checksum | Always recomputed | Recompute (default) or keep stored |

## Callers

| Caller | Path to the writer |
|---|---|
| `disk sectorwrite`, `blockwrite`, `put`, `delete`, `boot`; assembler `--disk` | `DiskImageSession::SaveAndCommit` → `VolumeImage::Save` → per changed sector |
| Explorer writes (after the rebase) | `DiskOperations::CommitEdit` and the runner, through `SaveAndCommit` |
| Inspector apply, undo, redo (Casso) | Emulation thread, at a safe point (emulation-thread contract) |
| Inspector apply, undo, redo (Explorer) | `DiskOperations::ApplySectorEdits` → `CommitEdit` |

`disk create`, `disk init`, Explorer's "Format disk image..." and guest writes
through the drive do not use the writer.

## Error messages (`disk` command)

Each follows the error format in the coding standards: a categorical first
line, then complete sentences. Wording goes to the owner for approval before
it ships.

```text
Error: quarter track holds its own record
       Quarter track 2.5 maps to track record 41, which holds data of its own
       between whole tracks. Writes to this image would change what that
       quarter track reads, so the image was left unchanged.

Error: track does not decode completely
       Track 17 has no sector $5. The disk command writes only to tracks on
       which all 16 sectors decode with good checksums, so the image was left
       unchanged.
```

## Saving

Every save of a changed image goes through `DurableCommit::Commit`
(`Replace`, or `CreateNew` for a new file): the file holds the old contents or
the new ones whole. A failure at any step leaves the file as it was, and the
caller reports the step and the reason.
