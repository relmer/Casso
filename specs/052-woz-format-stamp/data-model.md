# Data Model: WOZ Format Stamp

All types live in `CassoEmuCore/Devices/Disk/Stamp/`.

## FormatStamp

| Field | Type | Notes |
|---|---|---|
| version | `{ int major; int minor; }` | 1.0 for this feature |
| writer | string | program and version, for example `Casso 1.24.0` |
| written | string | ISO 8601 UTC |
| source | enum `Capture`, `Inspector` | |
| decodeSettings | `optional<DecodeSettings>` | present only when not standard |
| capture | `optional<CaptureFacts>` | present when source is Capture, and kept by an inspector refresh |
| notes | `vector<ProtectionNote>` | disk level |
| tracks | `vector<StampTrack>` | ordered by quarter-track position |

Validation: at most 160 tracks (WOZ quarter-track positions); positions unique; the JSON text at most 1 MB.

## StampTrack

| Field | Type | Notes |
|---|---|---|
| position | int | quarter-track position, 0 to 159 |
| encoding | enum `Sixteen`, `Thirteen`, `Both`, `Unformatted`, `Nonstandard`, `Empty` | from `TrackClass` |
| addressPrologs | `vector<array<Byte,3>>` | as found, distinct, in order of first appearance |
| addressEpilogs | `vector<vector<Byte>>` | 2 or 3 nibbles |
| dataPrologs | `vector<array<Byte,3>>` | |
| dataEpilogs | `vector<vector<Byte>>` | |
| sectors | `vector<Byte>` | sector numbers from address fields, in disk order |
| volumes | `vector<Byte>` | distinct |
| addressChecksum | enum `Standard`, `NotChecked`, `Failing` | |
| dataChecksum | enum `Standard`, `NotChecked`, `Failing` | |
| sectorsFound | int | |
| sectorsGood | int | |
| verified | bool | sectorsGood == sectorsFound and sectorsFound > 0 |
| changedAfterCapture | bool | set by the refresher |
| bitsCrc | optional<uint32_t> | CRC-32 of the track record's bytes, compared on save to find rewritten tracks |
| capture | `optional<TrackCaptureFacts>` | |
| notes | `vector<ProtectionNote>` | |

## CaptureFacts (disk level)

| Field | Type |
|---|---|
| reads | int |
| revolutionCycles | double, absent when not measured |
| synchronized | bool |
| betweenRead / betweenTotal | int |

## TrackCaptureFacts

| Field | Type |
|---|---|
| weak | bool |
| unstable | bool |
| ownStream | bool (a half or quarter position with a separate stream) |
| replacedSectors | `vector<int>` |

## ProtectionNote

| Field | Type | Notes |
|---|---|---|
| kind | string | `track-alignment`, `nibble-count`, `weak-bits`, `half-track` or any later kind |
| params | JSON object | written back verbatim |

## StampedTrackMarks (decoder input, not stored)

Built from a `StampTrack` for one whole track:

| Field | Type |
|---|---|
| addressPrologs | `vector<array<Byte,3>>` |
| dataPrologs | `vector<array<Byte,3>>` |
| isAddressChecksumChecked | bool |
| isDataChecksumChecked | bool |

## State transitions of a stamp in a loaded image

```
none ──capture / inspector stamp──▶ parsed
parsed ──save, no dirty track──▶ parsed (bytes written back verbatim)
parsed ──save, dirty tracks──▶ parsed (dirty entries reanalyzed, capture facts cleared)
unreadable ──save, no dirty track──▶ unreadable (bytes written back verbatim)
unreadable ──save, dirty tracks──▶ none (chunk removed)
unreadable ──inspector stamp──▶ parsed (new stamp replaces the chunk)
```
