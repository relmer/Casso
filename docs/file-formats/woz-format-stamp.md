# WOZ format stamp (`CSSO` chunk)

Casso writes a format stamp into the 5.25" WOZ files it captures over CassoLink, and into any WOZ the disk inspector's **Stamp format** command saves. The stamp lists, for each track, the encoding, the address and data prologs and epilogs, the sector and volume numbers and the checksum results Casso found, and, for a capture, facts about the reads that can't be recovered from the bits alone.

A reader uses the stamp as a hint. The bits in TRKS and FLUX remain the disk. A reader that decodes a track with the stamped marks accepts a sector only when its checksums pass, so a stale or wrong stamp can slow a read but can't change what it returns.

Other tools skip the chunk, as the WOZ reference requires for an id they don't recognize.

## Chunk

| Field | Value |
|---|---|
| id | `43 53 53 4F` (`CSSO`) |
| size | the payload length, 32-bit little-endian, as for every WOZ chunk |
| payload | UTF-8 JSON, with no byte order mark, at most 1,048,576 bytes |
| place | after every other chunk. Find it by walking the chunks; don't assume an offset. |
| count | at most one. A reader uses the first. |

## Top level

```json
{
  "format": "casso-woz-stamp",
  "version": "1.0",
  "writer": "Casso 1.32.0",
  "written": "2026-11-23T21:04:00Z",
  "source": "capture",
  "decodeSettings": [],
  "capture": {
    "reads": 105,
    "revolutionCycles": 199850.5,
    "synchronized": true,
    "betweenRead": 4,
    "betweenTotal": 102
  },
  "notes": [],
  "tracks": [ ]
}
```

| Key | Required | Value |
|---|---|---|
| `format` | yes | `casso-woz-stamp` |
| `version` | yes | `major.minor`. A reader ignores a stamp with an unrecognized major version, and ignores unrecognized keys. A minor version adds optional keys only. |
| `writer` | no | the program and version that wrote the stamp |
| `written` | no | ISO 8601 UTC |
| `source` | yes | `capture` or `inspector` |
| `decodeSettings` | no | the disk inspector's custom decode settings when the stamp was built with them, as a list of ranges (below) |
| `capture` | no | the capture's facts, or `null` for a stamp that isn't from a capture |
| `notes` | no | protection notes for the whole disk |
| `tracks` | yes | one entry per whole track with a record, and per half or quarter track with a separate record |

A decode settings range:

```json
{ "firstTrack": 1, "lastTrack": 34,
  "address16": ["D4 AA ??"], "address13": [], "data": [], "addressEpilogue": [], "dataEpilogue": [],
  "matchStandardToo": true, "addressChecksum": true, "dataChecksum": true, "epilogue": true }
```

`??` matches any nibble.

## Track entry

```json
{
  "position": 4,
  "encoding": "13-sector",
  "addressPrologs": ["D7 AA B7"],
  "addressEpilogs": ["DE AA EB"],
  "dataPrologs": ["D7 AA BD"],
  "dataEpilogs": ["DE AA EB"],
  "sectors": [0, 10, 7, 4, 1, 11, 8, 5, 2, 12, 9, 6, 3],
  "volumes": [254],
  "addressChecksum": "standard",
  "dataChecksum": "standard",
  "sectorsFound": 13,
  "sectorsGood": 13,
  "verified": true,
  "changedAfterCapture": false,
  "bitsCrc": "1A2B3C4D",
  "capture": { "weak": false, "unstable": false, "ownStream": false, "replacedSectors": [] },
  "notes": []
}
```

| Key | Value |
|---|---|
| `position` | the quarter-track position, 0 to 159. Whole track N is 4N. |
| `encoding` | `16-sector`, `13-sector`, `both`, `unformatted`, `nonstandard` or `empty` |
| `addressPrologs`, `dataPrologs` | the three-nibble prologs found, in the order first found |
| `addressEpilogs`, `dataEpilogs` | the epilogs found: the matched mark and its tail nibble, or the three nibbles after the field when no epilog mark matched |
| nibble text | two uppercase hex digits per nibble, separated by single spaces |
| `sectors` | the sector numbers of the address fields with good checksums, in the order on the track |
| `volumes` | the distinct volume numbers of those fields |
| `addressChecksum`, `dataChecksum` | `standard`, `not-checked` (the decode settings turned the check off) or `failing` (a checked field failed) |
| `sectorsFound`, `sectorsGood` | the sectors found, and those with good checksums |
| `verified` | whether every sector found has good checksums |
| `changedAfterCapture` | whether a write after the stamp was built rewrote the track |
| `bitsCrc` | the CRC-32 of the track record's bytes (its flux bytes for a flux track), eight hex digits. A writer compares it with the track to find tracks rewritten since the stamp was built. |
| `capture` | only in a capture's stamp, and removed when the track is rewritten. `ownStream` is a half or quarter track with a stream separate from its neighbors'. `replacedSectors` are sectors taken whole from another read. |
| `notes` | protection notes for the track |

## Protection note

```json
{ "kind": "track-alignment", "params": { "tracks": [1, 2] } }
```

A writer writes back notes with an unrecognized `kind`.

## Reader rules

1. Decode a track with its stamped marks and accept a sector only when its checksums pass.
2. Use the stamped result only where it's better than the result without the stamp, so a sector the plain read verified is never replaced.
3. Ignore a stamp that doesn't parse, has another `format`, or has an unknown major version.

## Writer rules

1. When a track's bits change, analyze it again, replace its entry, remove its `capture`, and set `changedAfterCapture`. Find the changed tracks by `bitsCrc`.
2. A writer that can't update the stamp removes the chunk when it changes any track.
3. When no track changed, a writer writes the chunk back byte for byte.
