# Quickstart: WOZ Format Stamp

## Unit tests

```powershell
scripts/RunTests.ps1 -Build -Filter "FormatStamp*"
```

They cover:

- a JSON round trip of every field, and the rejection of a bad format, a 2.x version and an oversized chunk (FR-002, FR-008)
- building a stamp from an in-memory 16-sector disk, a 13-sector disk, a D4 AA 96 disk and a disk with a different prolog variant on each track (User Story 1)
- mark discovery on a track whose prolog no list contains (research R4)
- a file read from an in-memory image whose marks are on no list, with the stamp and without it (SC-002)
- the same read with deliberately wrong stamped marks (SC-004)
- a save after one sector write: one entry changes, the others are byte for byte identical (User Story 5)
- a save of an image with an unreadable stamp, with and without dirty tracks (FR-023, FR-024)

## Scenario tests

```powershell
scripts/RunTests.ps1 -Scenario -Build -Filter "FormatStamp*"
```

For each image in the scenario disk set: stamp it, then compare a full read with and without the stamp, and with wrong marks (SC-003, SC-004). Includes the 1981 Ultima program disk when its image is in the fixture set.

## By hand

1. Capture a disk with CassoLink, then run `casso disk stamp <file>.woz`: one row per track.
2. Open the image in Casso Explorer's disk inspector: Image details lists the stamp; Decode settings lists the stamped ranges.
3. Open an older WOZ, run Stamp format in the inspector, save, and run `casso disk stamp` again.
4. Copy a file onto the stamped image and run `casso disk stamp`: the touched tracks list `changedAfterCapture`.
