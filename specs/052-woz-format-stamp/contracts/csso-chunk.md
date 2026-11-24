# Contract: the CSSO chunk

This contract is published as `docs/file-formats/woz-format-stamp.md` (FR-003).

## Chunk

- id: the four bytes `43 53 53 4F` (`CSSO`)
- size: the payload length, little-endian, as for every WOZ chunk
- payload: UTF-8 JSON, no byte order mark, at most 1,048,576 bytes
- position: after every other chunk in the file. A reader finds it by walking chunks and must not assume a position.
- a file contains at most one; a reader uses the first

## Top level

```json
{
  "format": "casso-woz-stamp",
  "version": "1.0",
  "writer": "Casso 1.24.0",
  "written": "2026-11-23T21:04:00Z",
  "source": "capture",
  "decodeSettings": null,
  "capture": {
    "reads": 3,
    "revolutionCycles": 199850.5,
    "synchronized": true,
    "betweenRead": 4,
    "betweenTotal": 102
  },
  "notes": [],
  "tracks": [ ... ]
}
```

| Key | Required | Values |
|---|---|---|
| format | yes | `casso-woz-stamp` |
| version | yes | `major.minor`; readers reject a major they do not know and ignore unknown keys |
| writer | yes | free text |
| written | yes | ISO 8601 UTC |
| source | yes | `capture` or `inspector` |
| decodeSettings | no | null, or `[{ "tracks": "0-34", "address16": ["D5 AA 96"], "address13": [], "data": [], "addressEpilogue": [], "dataEpilogue": [], "matchStandardToo": true, "addressChecksum": true, "dataChecksum": true, "epilogue": true }]` |
| capture | no | object as above |
| notes | no | list of notes |
| tracks | yes | list of track entries |

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
  "capture": { "weak": false, "unstable": false, "ownStream": false, "replacedSectors": [] },
  "notes": []
}
```

| Key | Values |
|---|---|
| position | quarter-track position: whole track N is 4N |
| encoding | `16-sector`, `13-sector`, `both`, `unformatted`, `nonstandard`, `empty` |
| *Prologs, *Epilogs | lists of nibble strings, two-digit uppercase hex separated by single spaces |
| sectors, volumes | lists of integers 0 to 255 |
| addressChecksum, dataChecksum | `standard`, `not-checked`, `failing` |
| sectorsFound, sectorsGood | integers |
| verified | true when every sector found decoded with good checksums |
| changedAfterCapture | true when a write after the capture rewrote the track |
| capture | present only in a capture stamp, and removed when the track is rewritten |

## Note

```json
{ "kind": "track-alignment", "params": { "tracks": [1, 2] } }
```

Readers write back notes whose kind they do not recognize.

## Reader rules

1. Treat the stamp as a hint. Decode with the stamped marks and accept a sector only when its checksums pass, unless the entry lists them as `not-checked`.
2. When the stamped marks decode fewer good sectors than detection, use detection.
3. Ignore a stamp that does not parse, has another `format`, or has an unknown major version.

## Writer rules

1. When a track's bits change, analyze it again, replace its entry, remove its `capture`, and set `changedAfterCapture`.
2. A writer that cannot update the stamp removes the chunk when it changes any track.
3. A writer that changes no track writes the chunk back byte for byte.
