# Data Model: WOZ Flux Track Support

## FluxTrack (new class, `CassoEmuCore/Devices/Disk/FluxTrack.h/.cpp`)

One revolution of a flux track, stored as the file stores it.

| Field | Type | Meaning |
|-------|------|---------|
| `m_bytes` | `vector<Byte>` | Raw TRKS bytes: each is the number of ticks since the previous transition; a 255 adds to the next byte |
| `m_totalTicks` | `uint64_t` | Sum of every byte; one revolution in 125 ns ticks. Computed at load and after each re-encode |
| `m_transitionCount` | `size_t` | Number of non-255 bytes |

Validation at construction (failure means a damaged track, see R9):
- the byte count is no larger than block count × 512, and the blocks lie
  inside the file;
- the last byte is not 255 (a truncated run);
- a zero byte count is a valid track with no transitions (it reads as noise),
  not damage.

Operations:
- `FindTransitionAtOrAfter (tick)` returns a cursor {byte index, absolute
  tick}. It is used on seek and on a head-angle conversion.
- `AdvanceCursor (cursor)` moves to the next transition, summing any 255 run
  and wrapping at `m_totalTicks`.
- `SpliceWrite (startTick, bits)` re-encodes one write burst at the nominal
  cell (R6). The total tick count is unchanged, and so is every byte outside
  the burst.
- `GetBytes()` and `GetTotalTicks()` give read-only access for Serialize and
  for the future disk inspector (FR-010).

## DiskImage (extended)

| Addition | Type | Meaning |
|----------|------|---------|
| `m_slotKind` | `vector<TrackKind>` | Per slot (TRKS index): `Bits` or `Flux`. Indexed like `m_trackBits` |
| `m_fluxTracks` | `vector<FluxTrack>` | Per slot; empty for bit slots |
| `m_damagedTracks` | `vector<DamagedTrack>` | Tracks that failed validation at load (R9) |

`TrackKind` is a free enum in `DiskImage.h` (`Bits`, `Flux`), because callers
pass it and get it back.

`DamagedTrack` is a plain struct with no methods, nested in `DiskImage`:
`{ int trkIndex; bool isFlux; DamageReason reason; }`. The quarter tracks
affected are found through the map.

Slot identity: a slot number is the TRKS index, as today. TMAP and FLUX both
store TRKS indices, so `m_quarterTrackMap` stays `int` per quarter track. The
kind lives on the slot, not in the map. FLUX entries are applied after TMAP, so
FLUX wins a quarter track both maps claim (FR-002).

State rules:
- `IsWriteProtected()` is also true when `m_damagedTracks` is not empty.
- `IsTrackDirty (slot)` covers flux slots too. `SpliceWrite` marks the slot
  dirty.
- `ClearDirty()` after load, as today.

New accessors: `GetTrackKind (slot)`, `GetFluxTrack (slot)` (const and
for-write), `GetDamagedTracks()`, `HasDamagedTracks()`.

## Disk2NibbleEngine (extended state)

| Field | Meaning |
|-------|---------|
| `m_slot`, `m_isFluxSlot` | Cached on track change and disk change; no per-clock `ResolveQuarterTrack` |
| `m_fluxNow` | Current time on the flux track, in 1/45-tick units (R1) |
| `m_fluxCursor` | Next transition {byte index, due time in 1/45-tick units} |
| `m_fluxLastPulse` | Time of the last real transition, for the weak-bit window (R5) |
| `m_writeBurst` | {active, start tick, `vector<bool>` bits} while writing a flux slot (R6) |

Transitions:
- Bit to flux, or flux to flux: fraction = position / length, then
  `m_fluxNow` = fraction × total ticks × 45, and re-seek the cursor (R4).
- Flux to bit: `m_bitPos` = fraction × bit count.
- Bit to bit: unchanged (`m_bitPos %= newBits`).
- The burst ends (and `SpliceWrite` runs) on write-mode off, a step, motor
  off, `SetDiskImage`, or flush.

## WozMetadata / INFO

INFO stays held verbatim (`infoPayload`). Serialize patches byte 0 (version,
raised to 3 when a FLUX chunk is written), bytes 44-45 (largest bit track),
46-47 (FLUX block) and 48-49 (largest flux track). FLUX is no longer kept as a
pass-through chunk; it is rebuilt from the map (R7).

## MountDiagnosis

`MountFailure::MalformedWoz` covers the unusable-map cases (R8); no new enum
value is needed. The damaged-track report goes through the existing
damaged-mount report, not through `MountFailure`, because the mount succeeds.
