# Tasks: WOZ Format Stamp

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

Tests are required by the constitution: every component gets unit tests with in-memory images only; real image files go to `ScenarioTests`. The feature ships whole; no phase is a stopping point.

## Phase 1: Setup

- [x] T001 Create `CassoEmuCore/Devices/Disk/Stamp/` and add it to `CassoEmuCore/CassoEmuCore.vcxproj` and its `.filters`
- [x] T002 Publish the chunk format from `contracts/cass-chunk.md` as `docs/file-formats/woz-format-stamp.md`

## Phase 2: Foundational

- [x] T003 Define `FormatStamp`, `StampTrack`, `CaptureFacts`, `TrackCaptureFacts` and `ProtectionNote` per data-model.md in `CassoEmuCore/Devices/Disk/Stamp/FormatStamp.h`; "at most 160 tracks", "positions unique", "the JSON text at most 1 MB"
- [x] T004 Write `FormatStampJson::Write` and `FormatStampJson::Parse` in `CassoEmuCore/Devices/Disk/Stamp/FormatStampJson.h/.cpp`: nibble lists as `"D5 AA 96"` strings, positions 0 to 159, `ERROR_INVALID_DATA` for another `format`, a major version other than 1, or text over 1,048,576 bytes; unknown keys ignored; unknown note kinds kept with their params
- [x] T005 [P] Unit tests for T004 in `UnitTest/EmuTests/FormatStampJsonTests.cpp`: every field round-trips; each rejection case; a 1.5 stamp with an unknown key parses
- [x] T006 Add `std::optional<FormatStamp> stamp` and `vector<Byte> stampBytes` to `WozMetadata` in `CassoEmuCore/Machines/Apple2/Common/WozMetadata.h`, cleared by `Clear`
- [x] T007 Write `FormatStampChunk` in `CassoEmuCore/Devices/Disk/Stamp/FormatStampChunk.h/.cpp`: find the first `CASS` in `passThrough`, parse it into `WozMetadata`, replace it with new text, remove it
- [x] T008 Call `FormatStampChunk` from `WozLoader::Load` in `CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp`
- [x] T009 [P] Unit tests in `UnitTest/EmuTests/FormatStampChunkTests.cpp`: a synthetic WOZ with a stamp loads it; an unreadable stamp leaves `stamp` empty and the chunk in `passThrough`; two `CASS` chunks use the first; a save of an unmodified image writes the chunk byte for byte

## Phase 3: User Story 1, a captured image records its format (P1)

**Independent test**: build captures from synthetic `Position` reads and read the stamp out of `Result::woz`.

- [x] T010 [US1] Write `MarkDiscovery` in `CassoEmuCore/Devices/Disk/Stamp/MarkDiscovery.h/.cpp` per research R4: three-nibble candidates followed by a checksum-valid 4-and-4 address body, or a 6-and-2 or 5-and-3 data body, returned as `FieldMarks`
- [x] T011 [P] [US1] Unit tests in `UnitTest/EmuTests/MarkDiscoveryTests.cpp`: finds `D7 AA B7`/`D7 AA BD` on a 13-sector track and `D4 AA 96` on a 16-sector track; finds nothing on a standard track or on random bits
- [x] T012 [US1] Write `FormatStampBuilder::Build (const DiskCopy &, const DecodeSettings &, const CaptureFacts *, StampSource, FormatStamp &)` in `CassoEmuCore/Devices/Disk/Stamp/FormatStampBuilder.h/.cpp`: runs `DiskAnalyzer`, then `MarkDiscovery` on tracks that are Nonstandard or have unverified sectors, reanalyzes with the discovered marks, and fills one `StampTrack` per quarter-track position with data
- [x] T013 [P] [US1] Unit tests in `UnitTest/EmuTests/FormatStampBuilderTests.cpp`: a DOS 3.3 disk (35 entries, 16 good each, volume); a D4 AA 96 disk; a disk with a different variant on each track; a track with two address prologs; Ultima V sector numbers $11 to $20; an unformatted track; a half track stream
- [x] T014 [US1] In `CassoLinkCapture::BuildWoz` (`CassoEmuCore/CassoLink/Session/CassoLinkCapture.cpp`) build `CaptureFacts` from `Result` (unstable, weak, replaced, streams, between-track reads, revolution cycles, synchronized), build the stamp, and attach it before `WozLoader::Serialize`
- [x] T015 [P] [US1] Unit tests in `UnitTest/EmuTests/CassoLinkCaptureStampTests.cpp`: `BuildWoz` from synthetic positions writes a stamp with the capture facts; a WOZ rebuilt from `ParseReads` output has the same stamp

## Phase 4: User Story 2, reads use the stamp (P1)

**Independent test**: an in-memory image whose marks are on no detection list reads with its stamp and fails without it.

- [x] T016 [US2] Write `StampedTrackMarks` and `StampedTrackMarks::ForTrack (const FormatStamp &, int track, StampedTrackMarks &)` in `CassoEmuCore/Devices/Disk/Stamp/StampedTrackMarks.h/.cpp`
- [x] T017 [US2] Add an optional `const StampedTrackMarks *` to `TrackSectorDecoder::DecodeOneSector`, `HasDataField` and `FindSectorBase` in `CassoEmuCore/Machines/Apple2/Common/TrackSectorDecoder.h/.cpp`; with marks, match the stamped address and data prologs in place of the constants
- [x] T018 [US2] In `Denibblizer` (`CassoEmuCore/Machines/Apple2/Common/Denibblizer.cpp`), for each track whose detection decode leaves a sector not `Verified` and whose stamp has an entry, decode again with the stamped marks and use the better outcome per sector
- [x] T019 [US2] The same in `ThirteenSectorCodec::Denibblize` and `LocateTrack` (`CassoEmuCore/Machines/Apple2/Common/ThirteenSectorCodec.cpp`), with the stamped marks as `FieldMarks`
- [x] T020 [P] [US2] Unit tests in `UnitTest/EmuTests/StampedDecodeTests.cpp`: 16-sector and 13-sector images with marks on no list read with the stamp and fail without it; a stamp with wrong marks returns exactly the detection result; a standard disk reads identically with and without a stamp; an unreadable stamp reads as no stamp
- [x] T021 [US2] Scenario tests in `ScenarioTests/FormatStampRealDiskTests.cpp`: for each WOZ in the scenario disk set, stamp it in memory and compare full reads with the stamp, with wrong marks on every track, and with no stamp (SC-003, SC-004)

## Phase 5: User Story 5, writes keep the stamp accurate (P1)

**Independent test**: one sector write and a save change exactly one entry.

- [x] T022 [US5] Write `FormatStampRefresher::Refresh (DiskImage &)` in `CassoEmuCore/Devices/Disk/Stamp/FormatStampRefresher.h/.cpp`: no dirty track, nothing; dirty tracks and a parsed stamp, reanalyze those tracks with the stamp's decode settings, replace their entries, clear `capture`, set `changedAfterCapture`, write the chunk; dirty tracks and an unreadable stamp, remove the chunk
- [x] T023 [US5] Call `FormatStampRefresher::Refresh` at the start of `WozLoader::Serialize` (on a copy of the metadata, since `Serialize` takes a const image)
- [x] T024 [P] [US5] Unit tests in `UnitTest/EmuTests/FormatStampRefresherTests.cpp`: one sector write changes one entry; a reformat to another encoding changes the entry's encoding; capture facts of the rewritten track are removed; an unreadable stamp with a dirty track is removed and the save succeeds; with no dirty track the chunk bytes are identical
- [x] T025 [US5] Scenario test in `ScenarioTests/FormatStampRealDiskTests.cpp`: put a file on a stamped copy of a real DOS 3.3 WOZ, save, reload, and confirm every entry matches a fresh analysis (SC-008)

## Phase 6: User Story 3, stamp from the inspector (P2)

**Independent test**: stamp an in-memory image through the inspector's model and save.

- [x] T026 [US3] Add a Stamp format command to the disk inspector's command set and menu (`CassoEmuCore/Ui/DiskInspector/`), disabled with a tooltip for read-only or write-protected images and absent for non-WOZ images; it builds a stamp with the current decode settings, keeps the capture facts of unchanged tracks, and saves through the existing image save path
- [x] T027 [P] [US3] Unit tests for the command's model in `UnitTest/EmuTests/InspectorStampCommandTests.cpp`: availability by image kind and protection; a refresh keeps capture facts; only the chunk and the header CRC differ after the save

## Phase 7: User Story 4, the inspector uses the stamp (P2)

- [x] T028 [US4] Seed `DecodeSettings` from the stamp when the inspector opens a stamped image, one range per run of tracks with the same marks, labeled as from the stamp (`CassoEmuCore/Devices/Disk/Inspector/DecodeSettings.h/.cpp`, `StampedTrackMarks::ToDecodeSettings`), with a control in Decode settings that turns them off
- [x] T029 [US4] Add a Format stamp section to `ImageDetails` (`CassoEmuCore/Devices/Disk/Inspector/ImageDetails.h/.cpp`): present or absent, source, writer, date, capture facts
- [x] T030 [P] [US4] Unit tests in `UnitTest/EmuTests/StampedDecodeSettingsTests.cpp` and `ImageDetailsTests`: a D4 AA 96 stamped image analyzes as 16-sector, not Nonstandard; turning the stamp ranges off returns Nonstandard; the details section lists the stamp

## Phase 8: User Story 6, `casso disk get-stamp` (P3)

- [x] T031 [US6] Write `FormatStampText::FormatTable` in `CassoEmuCore/Devices/Disk/Stamp/FormatStampText.h/.cpp` per `contracts/cli-disk-get-stamp.md`
- [x] T032 [US6] Add `DiskGetStampCommand` in `CassoEmuCore/Devices/Disk/DiskGetStampCommand.h/.cpp`, register `get-stamp` with `--json` in the disk command parser and `--help`, exit codes 0, 1, 2, 3
- [x] T033 [P] [US6] Unit tests in `UnitTest/EmuTests/DiskGetStampCommandTests.cpp`: table output, JSON byte for byte, no stamp, unreadable stamp, not a WOZ

## Phase 9: Polish

- [ ] T034 Add the stamp to `CHANGELOG.md` under the next release and to the README's disk image section (approved by the owner before pushing)
- [ ] T035 Build Debug and Release x64 and ARM64; run `scripts/CheckStyle.ps1 -Mode Tree`, the full unit suite, and the disk scenario tests
- [ ] T036 Merge into `033-casso-explorer`

## Dependencies

- Phase 2 blocks every story.
- US1 (T012) is required by US5, US3 and the scenario test T021.
- US2 and US1 are otherwise independent; T016 to T020 can start after Phase 2.
- US4 depends on T016.
- US6 depends only on Phase 2.

## Parallel opportunities

- T005, T009 alongside T006 to T008.
- US2 (T016 to T020) alongside US1 (T010 to T015).
- US6 alongside any story after Phase 2.
