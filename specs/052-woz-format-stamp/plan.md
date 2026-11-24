# Implementation Plan: WOZ Format Stamp

**Branch**: `052-woz-format-stamp` | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

## Summary

Write a `CASS` chunk of JSON into every 5.25" WOZ that a CassoLink capture
produces, and on demand from the disk inspector. The chunk lists each track's
encoding, marks, sector and volume numbers and checksum results, plus the
capture's facts. Host-side readers decode a track with its stamped marks when
detection does not verify every sector, and use the better result per sector. The
inspector seeds its decode settings from the stamp. Every save reanalyzes the
tracks that changed. `casso disk get-stamp` prints the chunk.

## Technical Context

**Language/Version**: C++23, MSVC (Visual Studio 2026)

**Primary Dependencies**: Windows SDK and STL; the tree's `JsonValue`, `JsonParser`, `JsonWriter`

**Storage**: the WOZ file itself

**Testing**: `UnitTest` (Microsoft CppUnitTest, in-memory images only); `ScenarioTests` for real image files

**Target Platform**: Windows x64 and ARM64

**Project Type**: desktop application with a core library

**Performance Goals**: stamping a 35-track disk within the inspector's whole-disk analysis time plus one save (SC-007); no extra decode work on tracks whose sectors all verify

**Constraints**: a wrong stamp never changes a read's result (FR-016); a save with no rewritten track writes the chunk back byte for byte (FR-024); the stamp is at most 1 MB

**Scale/Scope**: up to 160 quarter-track entries per image

## Constitution Check

| Principle | Status |
|---|---|
| I. Code quality, EHM, single exit | every new HRESULT function uses EHM; parsing reports `ERROR_INVALID_DATA` for bad stamps, never asserts |
| II. Testing, no file I/O in unit tests | stamps are built and parsed from in-memory `DiskImage` objects and byte vectors; the real Ultima and DOS images go to `ScenarioTests` |
| III. UX consistency | the CLI command is in `--help`; the inspector command has a tooltip for its disabled state |
| V. Simplicity, function size | each component below is a small class with one job |
| VI. Thin executable | all code is in `CassoEmuCore`; `Casso.exe` and `CassoCli.exe` change by nothing |
| Dependencies | none added |

No violations.

## Project Structure

### Documentation (this feature)

```text
specs/052-woz-format-stamp/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── cass-chunk.md
│   └── cli-disk-get-stamp.md
└── tasks.md
docs/file-formats/woz-format-stamp.md     (published form of contracts/cass-chunk.md)
```

### Source Code

```text
CassoEmuCore/Devices/Disk/Stamp/
├── FormatStamp.h                 data model (data-model.md)
├── FormatStampJson.h/.cpp        FormatStamp <-> JSON text, version checks, size limit
├── FormatStampBuilder.h/.cpp     DiskAnalysis (+ CaptureFacts) -> FormatStamp
├── MarkDiscovery.h/.cpp          finds prologs missing from every mark list (research R4)
├── FormatStampChunk.h/.cpp       finds, parses, replaces and removes CASS in WozMetadata
├── FormatStampRefresher.h/.cpp   reanalyzes dirty tracks on save (research R7)
├── StampedTrackMarks.h/.cpp      FormatStamp -> per-track decoder marks and DecodeSettings
└── FormatStampText.h/.cpp        the CLI table

CassoEmuCore/Machines/Apple2/Common/
├── WozMetadata.h                 + parsed stamp and its chunk bytes
├── WozLoader.cpp                 Load parses the stamp; Serialize calls the refresher
├── TrackSectorDecoder.h/.cpp     + optional StampedTrackMarks
├── ThirteenSectorCodec.h/.cpp    + optional StampedTrackMarks
└── Denibblizer.cpp               second decode with the stamp; the better sector is used

CassoEmuCore/CassoLink/Session/CassoLinkCapture.cpp   BuildWoz stamps the image
CassoEmuCore/Devices/Disk/Inspector/ImageDetails.*    stamp section
CassoEmuCore/Ui/DiskInspector/...                     Stamp command, seeded decode settings
CassoEmuCore/Devices/Disk/DiskGetStampCommand.h/.cpp     `disk get-stamp`

UnitTest/EmuTests/FormatStamp*Tests.cpp
ScenarioTests/FormatStampRealDiskTests.cpp
```

**Structure Decision**: a `Stamp` folder of small components in `CassoEmuCore/Devices/Disk`, beside `Inspector` (which it reads) and `Store` (whose flush it runs under). Decoders take the marks as a plain value, so they do not depend on the stamp's types.

## Component boundaries

- `FormatStampBuilder` depends on `DiskAnalysis` and `MarkDiscovery`, nothing in the UI.
- `FormatStampRefresher` depends on the builder and `DiskImage`; `WozLoader::Serialize` calls it directly, since both are in `CassoEmuCore`.
- Decoders see only `StampedTrackMarks`.
- The inspector accesses the stamp through `DiskImage::GetWozMetadata()`.

## Complexity Tracking

None.
