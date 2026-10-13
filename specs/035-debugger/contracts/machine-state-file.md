# Contract: machine state file (`.cassostate`)

Version 3, 2026-10-12 (User Story 25). Little-endian throughout.

## Header (28 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | magic `CASSOSTA` |
| 8 | 2 | format version: 3 (2 and 1 for earlier files) |
| 10 | 2 | reserved, 0 |
| 12 | 8 | payload size, as stored |
| 20 | 8 | hash of the payload, as stored |

Loading fails on a format version above the reader's own, a payload size that
does not match the file, or a hash that does not match, before any field is
read.

## Compression (format version 3)

The stored payload is the section's size before compression (u64), then the
section packed with the Windows Compression API's MSZIP algorithm (deflate)
in buffer mode. A size above 512 MB, or a stream that does not unpack to exactly
that size, is a damaged file. Format versions 1 and 2 store the section as is.

## Payload: one `SFIL` section

Section header: tag `SFIL` (u32), version (u16), size (u32). Version 2 adds
the last two fields; a version 1 section ends after the machine state.

| Field | Encoding |
|---|---|
| machine name | u32 length, then that many UTF-16 code units |
| ROM identity | u64 |
| disk count | u32, then per disk: slot u8, drive u8, format u8, path (u32 length + bytes), image (u32 length + bytes) |
| machine state | u32 length + bytes (`MachineHost::SaveState`) |
| saved at (v2) | u64 `FILETIME`, UTC |
| thumbnail (v2) | u32 length + PNG bytes, 280 by 192; length 0 for none |

Section versions 1 through the reader's own load; the section must be read
exactly, with nothing after it.

## Locations

- Default folder: `%LOCALAPPDATA%\Casso\SavedState\`.
- Automatic state: `<machine name>.autosave.cassostate` in that folder, one
  per machine kind.
- Every write goes to a temporary file in the same folder, renamed over the
  target only once complete.

## Compatibility

From the first release that writes state files, every later version loads
each file a release wrote, for the same machine kind and ROM images; each
part's section is read at every version a release wrote. A file from a newer
version, a file saved with different ROM images, and a damaged file each fail
with their own message. `UnitTest/Fixtures/States/<version>/` has one state
per machine kind from each release.
