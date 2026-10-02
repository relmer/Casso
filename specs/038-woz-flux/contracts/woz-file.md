# Contract: WOZ files Casso reads and writes

## Reading

| Input | Result |
|-------|--------|
| FLUX chunk, 160+ bytes, INFO flux block non-zero (any INFO version) | Quarter tracks in FLUX play as flux; FLUX overrides TMAP |
| FLUX chunk under 160 bytes | Mount refused, `MalformedWoz` |
| FLUX chunk present, INFO flux block = 0 | Mount refused, `MalformedWoz` |
| No FLUX chunk | Exactly today's behavior |
| A TMAP or FLUX track whose blocks lie outside the file, whose count overruns its blocks, or (flux) whose data ends in 255 | Mount succeeds read-only; the track reads as unformatted; the report on insert lists it |
| A WOZ 1 track record past the end of TRKS | Same as the row above (today: mount refused) |
| Missing INFO/TMAP/TRKS, a truncated chunk, a TRKS record table cut short | Mount refused, `MalformedWoz` (unchanged) |

## Writing (a flush of a dirty image)

- Header, INFO, TMAP and TRKS stay in the same order. TRKS data starts at block
  3, as today.
- A flux track's TRKS record: Starting Block, Block Count, and Bit Count = flux
  byte count. An unwritten flux track's bytes are identical to the source.
- TMAP is 0xFF at every quarter track FLUX claims.
- FLUX (160 bytes) starts on the first 512-byte boundary after TRKS. Its
  entries are TRKS indices.
- INFO: version >= 3 when FLUX is written; +44 = largest bit track in blocks;
  +46 = FLUX block; +48 = largest flux track in blocks. Without flux tracks,
  +46/+48 = 0.
- Pass-through chunks (META, unknown) follow FLUX, as today. A source FLUX
  chunk is not passed through.
- CRC32 is computed last.
- An image that was never written is not rewritten.

## Read-back guarantee

Loading a file Casso wrote gives the same slots, kinds, map and flux bytes it
was saved from (round-trip test).
