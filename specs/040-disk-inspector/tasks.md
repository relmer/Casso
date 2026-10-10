---
description: "Task list for 040-disk-inspector, first release"
---

# Tasks: Disk Inspector (first release)

**Input**: Design documents from `specs/040-disk-inspector/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. The constitution (Principle II) requires unit tests for all
production code, and every new test is shown to fail first (revert the fix or
stub the implementation, read the assertion message, restore, stamp the file).
Test images are made up; nothing reads a real file in `UnitTest`.

**Organization**: Build order follows plan.md. The first release reaches
master in two merges (owner decision, 2026-10-08): **Merge 1** (Phases 1 to 4)
is the shared sector writer, the GH #170 fix; **Merge 2** (Phases 5 to 17) is
the rest of User Stories 1 to 10. User Stories 11 to 16 (later release) are
not in this file. Story priorities give build order only; no story ships
without the others in its merge.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story the task belongs to (US1 to US10)
- Paths are from the repository root. New `.h`/`.cpp` pairs are added to their
  project's `.vcxproj` and `.vcxproj.filters` in the same task.
- `CEC/` abbreviates `CassoEmuCore/`; `DD/` abbreviates `CassoEmuCore/Devices/Disk/`;
  `A2C/` abbreviates `CassoEmuCore/Machines/Apple2/Common/`; `UI/` abbreviates
  `CassoEmuCore/Ui/DiskInspector/`; `UT/` abbreviates `UnitTest/EmuTests/`.

---

## Phase 1: Setup (Merge 1)

- [X] T001 Merge `origin/master` (with the woz-info-fields merge `d78d2941a`) into `040-disk-inspector` with `--no-ff`
- [X] T002 Build x64 Debug and run the full unit suite to record the post-merge baseline (counts and any pre-existing failures) in this file's Notes section
- [X] T003 Confirm none of the "built once" parts is on master yet (`git grep` for `DiskMarkPattern`, `DiskFieldFormat`, `DurableCommit`, `OnLatchLoad`, `DriveStatusPublisher` on `origin/master`) and record the result in Notes
- [X] T004 [P] Add a `Devices\Disk\Inspector` filter to `CassoEmuCore/CassoEmuCore.vcxproj.filters` and an `EmuTests\Inspector` filter to `UnitTest/UnitTest.vcxproj.filters`

---

## Phase 2: Foundational (Merge 1): field formats, framing and stored maps

**Purpose**: What both the shared sector writer and the analyzer stand on.

### Tests

- [X] T005 [P] Write `UnitTest/Devices/DiskMarkPatternTests.cpp`: parse `D5 AA 96`, `D5 AA ??`, two-nibble marks, rejection of bad text (error format per coding standards), match and no-match over nibble spans
- [X] T006 [P] Write `UnitTest/Devices/DiskFieldFormatTests.cpp`: 4-and-4 round trip; 6-and-2 encode/decode of every byte value with checksum; 5-and-3 encode/decode against at least three vectors worked by hand from Beneath Apple DOS's description (recorded in the test file's comments, not computed by Casso's encoder); body lengths 342 and 410; checksum of all-zero and all-$FF sectors
- [X] T007 [P] Create `UT/InspectorTestImages.h` / `.cpp`: builders for made-up bit tracks (16-sector, 13-sector, mixed, given volume, given sync widths, extra zero cells after given nibbles, field spanning the index, planted anomalies) and flux tracks from cell lists with per-stretch cell times, reusing `WozLoader::BuildSyntheticV21` for WOZ output
- [X] T008 [P] Write `UT/LatchFramerTests.cpp`: framing from the index after one turn, loop wrap, sync widths 9 and 10, extra-zero counts, noise kind (three zero cells in a nibble), random-bit regions at the drive's limit, NIB FF runs as sync; SC-002: for every test track, framed nibbles equal the nibbles `Disk2NibbleEngine` latches on its second turn outside random-bit regions and the nibbles before the next sync run
- [X] T009 [P] Write `UT/FieldLocatorTests.cpp`: 16-sector and 13-sector address and data fields located with cells and passing order; data search window of 48 nibbles (just inside, just outside); search ends at the next address prologue; an address field with a failed checksum still pairs; repeated sector numbers kept in passing order
- [X] T010 [P] Extend `UT/WozLoaderTests.cpp`: a save keeps the stored TMAP for every quarter track whose record exists (standard layout N-0.25/N/N+0.25, layout with N+0.5 to a neighbor, Casso's layout), keeps a bit record that FLUX overrides with its TMAP entry, and keeps the bytes of records no map refers to

### Implementation

- [X] T011 [P] Create `CassoCore/DiskFieldKind.h` (`Sixteen`, `Thirteen`) and `CassoCore/DiskMarkPattern.h` / `.cpp` (up to three nibbles, each a value or any; `TryParse`, `Matches`, `ToText`), pure, to 035 T713's design
- [X] T012 Create `DD/DiskFieldFormat.h` / `.cpp`: standard prologues and epilogues as `DiskMarkPattern`s, 4-and-4 encode/decode, 6-and-2 and 5-and-3 translate tables and their inverses, encode and decode of 256 bytes with checksum (decode keeps going past a bad checksum and reports it), body lengths `kBody62 = 342` and `kBody53 = 410` (depends on T011)
- [X] T013 Create `DD/Inspector/TrackCopy.h` (slot, kind, bits and bit count or flux bytes, `guestWriteCount`; immutable, held by `shared_ptr<const>`) and `DD/Inspector/DiskCopy.h` (`mediaId`, format, quarter-track map, copies per record, image details pointer, write-protect causes)
- [X] T014 Create `DD/Inspector/LatchFramer.h` / `.cpp`: frames cells into `Nibble`s (value, startCell, widthCells, extraZeroCells) per research R3; flux decoded to cells with `Disk2NibbleEngine`'s unit constants and `FluxBitView`'s rule, keeping per-cell recorded times (an interval read as n cells gives each cell interval/n); random-bit regions from the engine's MC3470 window constant (shared, not copied); NIB and NB2 FF runs as sync
- [X] T015 Create `DD/Inspector/FieldLocator.h` / `.cpp`: finds address and data fields for 16- and 13-sector formats with `DiskFieldFormat`, pairs them (FR-013; `kDataSearchWindowNibbles = 48`), records each field's start and end cell, marks found, stored and computed checksums, and decodes data bodies (depends on T012, T014)
- [X] T016 Add `WozFileLayout` to `A2C/WozMetadata.h` (stored `tmap[160]`, `flux[160]`, `hasFluxMap`, `records` with v2 and v1 fields, `isReferenced`, `sourceMap[160]`) and fill it in `WozLoader::Load` in `A2C/WozLoader.cpp`; copy it wherever `WozMetadata` is copied, including `DD/DiskImageStore.cpp` near `:1501`
- [X] T017 Change `WozLoader::Serialize` in `A2C/WozLoader.cpp` to write TMAP from the stored map for every quarter track whose record still exists, keep a bit record FLUX overrides, and keep unreferenced records' bytes (contracts/woz-image-details.md, Saving) (depends on T016)

**Checkpoint**: T005 to T010 pass; the full suite is still green.

---

## Phase 3: User Story 9, part 1 - Shared sector writer (Merge 1, GH #170)

**Goal**: Every direct sector write changes only the data fields it writes, finds tracks through the map, keeps flux timing, and saves durably (FR-110 to FR-116).

**Independent Test**: SC-017 through the `disk` command on DSK, DO, PO, NIB, NB2 and WOZ (bit, flux, standard layout, N+0.5 layout, Casso's layout, volume 130, records out of track order).

### Tests

- [X] T018 [P] [US9] Write `UT/DurableCommitTests.cpp` over `UT/FakeDiskFileIo.h` extended with per-step fault injection: Replace and CreateNew each succeed; a failure at each step (write temporary, copy metadata, flush, replace or rename) leaves the target byte-for-byte as it was and removes the temporary
- [X] T019 [P] [US9] Write `ScenarioTests/DurableCommitFileTests.cpp`: real-file `FlushToStorage`, `CopyFileMetadata` and `RenameWithoutReplacing` in a scratch folder of their own, cleaned up before any assert (owner decision relayed by 041)
- [X] T020 [P] [US9] Write `UT/SectorFieldWriterTests.cpp`: bit track (cells outside the body and checksum unchanged, each nibble's extra zero cells kept, bit count unchanged), field spanning the index, two sectors numbered $5 (only the selected place changes), flux track (each new cell takes the old cell's recorded time, turn time unchanged, transitions outside the field unchanged), quarter track in TMAP and FLUX (only the flux record changes), checksum Recompute vs KeepStored, 13-sector write decodes by the hand-worked vectors, nibble-count mismatch changes nothing, all-or-nothing over several writes, a record read by several quarter tracks changed once
- [X] T021 [P] [US9] Write `UT/TrackWritabilityTests.cpp` cases (extend the existing file if present): standard layout, N+0.5 to a neighbor and Casso's layout accepted; a quarter track mapped to a record of its own fails with that quarter track and record in the message; unformatted and nothing-recorded tracks fail (FR-115); keep `TrackWritability_HalfTrackData_RefusesTheWholeImage` passing
- [X] T022 [P] [US9] Extend `UT/DiskWritePathTests.cpp` and `UT/CrossFormatWriteTests.cpp` for SC-017: `sectorwrite`, `blockwrite`, `put`, `delete`, `boot` on each format; volume 130 kept in every address field; changed tracks keep length, sync runs and sector places; record 14 holding track 13 written and read back by `sectorread`; a track with a bad checksum fails `put` with the track and reason; the file byte-for-byte unchanged on every failure; update any existing assertion of volume 254 or 50,624 bits after a write
- [X] T023 [P] [US9] Extend `UT/DiskCommandRunnerTests.cpp`: assembler `--disk` output through `Cli/ImageArtifactSink.cpp` keeps a WOZ's volume and lengths; error text for FR-114 and FR-115 in the standard format

### Implementation

- [X] T024 [US9] One self-contained commit for 041 to cherry-pick: `DD/DurableCommit.h` / `.cpp` to 041's settled signature (`Commit (IDiskFileIo &, targetPath, bytes, invocationTag, CommitMode, progress)`, `CommitMode { Replace, CreateNew }`), `IDiskFileIo` gains `FlushToStorage`, `CopyFileMetadata`, `RenameWithoutReplacing` in `DD/IDiskFileIo.h`, `CEC/Seams/Win32DiskFileIo.h` / `.cpp` and `UT/FakeDiskFileIo.h`, project entries, T018 and T019; send the pushed SHA to the 041 session. **Blocked until 041 sends the settled signature**
- [X] T025 [US9] Add `DiskImage::ResolveWholeTrack (int track)` in `DD/DiskImage.h` / `.cpp` and route every slot-N-as-track-N use through it: `NibblizationLayer::GetTrackBits`, `DecodeTracks`, `RenibblizeTracks`, `WriteFluxTrackSectors` in `A2C/NibblizationLayer.cpp`
- [X] T026 [US9] Add a per-slot `isChangedByWriter` flag to `DD/DiskImage.h` / `.cpp`, set by the writer and cleared by a save, never set by guest writes
- [X] T027 [US9] Create `DD/SectorWrite.h` (API type: slot, fieldStartCell or -1, sectorNumber, fieldKind, bytes[256], table, checksumMode, policy) and `DD/SectorFieldWriter.h` / `.cpp` per contracts/sector-writer.md: locate fields with `FieldLocator`, encode with `DiskFieldFormat`, write bit tracks nibble by nibble in place keeping extra zero cells, write flux fields by recorded cell times (FR-111, FR-113), all or nothing, `SectorWriteError` with quarter track, record and reason (depends on T012, T015, T025, T026)
- [X] T028 [US9] Rewrite `TrackWritability::Evaluate` in `A2C/TrackWritability.cpp` to FR-114 and FR-115 (Strict policy), with messages in the standard error format; leave the editor exemption to the `Editor` policy in `SectorFieldWriter`
- [X] T029 [US9] Change `VolumeImage::SaveBitStream` in `A2C/VolumeImage.cpp` to decode the prior state, diff by sector, and hand each changed sector to `SectorFieldWriter` with `Strict` instead of calling `RenibblizeTracks`; remove `WriteFluxTrackSectors`' blank-track fallback and nominal-cell timing in `A2C/NibblizationLayer.cpp`, leaving `NibblizeWithMap` for formatting only (depends on T027, T028)
- [X] T030 [US9] Write NIB and NB2 records whose only change is the writer's by copying the changed nibbles into the stored track bytes in place in `A2C/NibbleImageCodec.cpp` `Serialize` (R17) (depends on T026)
- [X] T031 [US9] Move `DiskImageSession::CommitImage` in `DD/DiskImageSession.cpp` onto `DurableCommit` (Replace) (depends on T024)
- [X] T032 [US9] Run the scenario suite (`scripts/RunTests.ps1 -Build -Scenario`) and fix every failure; DOS 3.3 and ProDOS must read images the `disk` command wrote

**Checkpoint**: SC-017 passes in full; the scenario suite is green.

---

## Phase 4: Merge 1 gate

- [X] T033 Merge `origin/master` again; `git add -A`; `scripts/CheckStyle.ps1 -Mode Tree`
- [X] T034 Full unit suite Debug and Release x64; ARM64 Debug and Release builds; `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`; scenario suite
- [X] T035 Draft the CHANGELOG `[Unreleased]` entry (GH #170: `disk` command writes keep each track's volume, sync and length, accept standard WOZ layouts and find tracks through the map; sector writes on flux tracks keep the recorded timing) and send the text to the owner before pushing
- [X] T036 (merged as 7e6addc6e from 040-merge1, which cherry-picked the GH #170 commits onto master so the unfinished inspector stayed off it) Merge `040-disk-inspector` to master with `--no-ff` (`merge(disk): ...` subject), push, watch CI to completion, close GH #170, message the 035 session (T713 files on master) and the 041 session (DurableCommit SHA)

---

## Phase 5: Foundational (Merge 2): loader, analyzer core, window skeleton

**Purpose**: Everything every view needs. Blocks Phases 6 to 17.

### Tests

- [X] T037 [P] Write `UT/WozImageDetailsTests.cpp`: `ReadInfo` reads every INFO field for v1, v2, v3 with `has...` flags; `Describe` and `WozCompatibility::ReadRequirements` agree with it; the FR-053 split table in contracts/woz-image-details.md row by row (all-zero record writable; count with zero start or block count damaged; start block 1 or 2 damaged; bit count past blocks damaged; map entry 160-254 damaged per quarter track; 255 unmapped); every FR-051 problem detected once, with "damaged" only where FR-048 allows; SC-010: each damaged-track case from spec 038 loads with its damaged quarter tracks and reasons
- [X] T038 [P] Write `UT/TrackAnalyzerTests.cpp`: classification order of FR-016 (nothing, damaged, 16, 13, 13 and 16, unformatted over 50%, nonstandard); FR-015 detections each once; measurements of FR-017; physical track of a record from the middle of its quarter tracks (N.5 accepts N or N+1); sectors not checked when checks are off (FR-020)
- [X] T039 [P] Write `UT/DiskAnalyzerTests.cpp`: summary of FR-018 (each record once, format chip rule, hidden zero chips); quarter-track findings of FR-048 including all exempt layouts; SC-013 (DSK, NIB, WOZ standard layout, Casso's WOZ, N+0.5 WOZ of one disk: no findings); SC-001 over every made-up anomaly image; SC-003 timing in Release
- [X] T040 [P] Write `UT/DecodeSettingsTests.cpp`: custom marks with `??` per track range, "Match standard marks too" on and off, checks off, `IsStandard`, reset

### Implementation

- [X] T041 Add `WozInfo` to `A2C/WozMetadata.h` and `static void WozLoader::ReadInfo (const std::vector<Byte> &, WozInfo &)` in `A2C/WozLoader.h` / `.cpp`; make `kInfoOffsetDiskSides = 37` public and remove the file-static `kInfoDiskSidesOff`; `WozLoader::Description` holds a `WozInfo` filled by `ReadInfo`; `WozCompatibility::ReadRequirements` reads through `ReadInfo` with its signature unchanged; update `UT/WozBitTimingTests.cpp` asserts to `desc.info.bitTiming`
- [X] T042 Complete `WozFileLayout` in `A2C/WozMetadata.h` / `A2C/WozLoader.cpp` with chunks (ID, offset, size), META entries in file order, CRC stored/computed/match, and `ImageFileProblem` list (FR-051)
- [X] T043 Split zero-length records in `WozLoader::ParseV2Track` and `ParseV2FluxTrack` (`A2C/WozLoader.cpp`) per R9; add `DamageReason::RecordLocationMissing`, `RecordInHeader`, `MapEntryOutOfRange`, extend `CountExceedsBlocks` to bit records, and add a per-quarter-track `DamagedQuarterTrack` list in `DD/DiskImage.h` / `.cpp` merged into `GetDamagedQuarterTracks`
- [X] T044 Create `DD/Inspector/DecodeSettings.h` / `.cpp` (list of `DecodeRange` with marks, `matchStandardToo`, checks; later-release slot left empty; `IsStandard`, `MakeStandard`)
- [X] T045 Create `DD/Inspector/ITrackFormatDecoder.h` and the 16- and 13-sector decoders over `FieldLocator`, honoring `DecodeSettings` (R30)
- [X] T046 Create `DD/Inspector/TrackAnalysis.h` (Nibble, Field, Sector, TrackMeasurements, TrackClass per data-model.md) and `DD/Inspector/TrackAnalyzer.h` / `.cpp`: nibble kinds, pairing, sector states, DOS 3.3 logical sector and ProDOS block for 16-sector tracks, editability with reason (FR-096), classification, measurements, per-track findings (depends on T014, T015, T044, T045)
- [X] T047 Create `DD/Inspector/Finding.h` and `DD/Inspector/FindingFormatter.h` / `.cpp` (neutral wording per FR-048; text reviewed against the spec's quoted examples)
- [X] T048 Create `DD/Inspector/ImageDetails.h` / `.cpp` from `WozInfo`, `WozFileLayout`, file attributes and sector order (depends on T041, T042)
- [X] T049 Create `DD/Inspector/DiskAnalyzer.h` / `.cpp`: quarter-track entries, records shared by quarter tracks, cross-track findings (FR-048 quarter-track rule, volume differences, 13-sector fields beside 16-sector, track length on WOZ bit and flux only), `DiskSummary` (FR-018), `headLimit` from `Disk2Controller::kMaxQuarterTrack` (depends on T046 to T048)
- [X] T050 Create `CEC/Ui/DiskInspector/AnalysisScheduler.h` / `.cpp` on `CEC/Shell/BackgroundWorkQueue.h`: per-record newest-wins work items with `mediaId` and write count, stale results dropped, "Analyzing" state (FR-021, R18)
- [X] T051 Spike the Dxui custom drawing hook: `Dxui/Widgets/DxuiCustomVisual.h` / `.cpp` rendering D3D11 content into its rectangle within the window's frame; then `UI/PlatterRenderer.h` / `.cpp` and `UI/PlatterShader.hlsl` (polar mapping, kind texture with wrapped rows, priority-reduced mip levels, palette constant buffer); measure SC-004 at fit and 600× and record the result in research.md R19; fall back to CPU quads per R19 if SC-004 fails with the shader or the hook changes the frame timing or device state of any other Dxui window
- [X] T052 [P] Create `UI/DiskInspectorPalette.h` / `.cpp` (fallback colors for dark and light surfaces, symbol table per FR-079 from `UnicodeSymbols.h`) and add `DiskInspectorColors` to `Dxui/Theme/DxuiTheme.h` with zero meaning fallback; set values in the three factories in `CEC/Ui/Chrome/CassoTheme.h`
- [X] T053 [P] Write `UT/DiskInspectorPaletteTests.cpp`: in each Casso theme, text at 4.5:1 or better against its background; every pair of Structure kind colors and every pair of map role colors at ΔE2000 10 or more; good and bad states distinct in grayscale
- [X] T054 Create `UI/IDiskInspectorHost.h` (disk source, requests, preferences, theme, machine state; `NullDiskInspectorHost`) and `UI/DiskInspectorWindow.h` / `.cpp` (`DxuiWindow`, 980×660 opening size, 640×460 minimum, both DPI-scaled, two columns with a splitter, toolbar, disk tabs, track tabs, square platter; `CreateParams::paceFrames` once 033 is on master)
- [X] T055 Create `UI/InspectorViewModel.h` / `.cpp`: selection (quarter track, sector, nibble range), first sector in passing order on a track change (FR-039), nearest sector after re-analysis (FR-038), state that lasts only while the window shows the same disk
- [X] T055a Create `DD/Inspector/InspectorFormat.h` / `.cpp` (FR-006: decimal tracks, blocks, volumes, counts, cell positions, times and angles in degrees clockwise from the index; hex sector numbers, byte and nibble values and offsets with `$` except in hex dumps, the Nibbles grid, sector buttons, strip labels and byte sequences; gaps in nibbles) with tests in `UT/InspectorFormatTests.cpp`; every view and the Go to parser use it
- [X] T056 Wire a minimal Casso open path: `CEC/Shell/EmulatorShellInspector.cpp` creates the window like `OpenDisk2DebugDialog` (`CEC/Shell/EmulatorShellDebug.cpp:510-583`) and shows a `DiskCopy` taken on the emulation thread at open through `CEC/Shell/InspectorRequestQueue.h` / `.cpp`, built here with its copy requests (`CopyDisk`, `CopyTracks`, `Export`) per contracts/emulation-thread.md; apply, undo and redo requests come in US9

**Checkpoint**: T037 to T040 and T053 pass; the window opens in Casso on drive 1's disk, showing an empty layout.

---

## Phase 6: User Story 1 - See how a disk is recorded (P1)

**Goal**: Platter, chips, strip, sector row, Sector data and Nibbles tabs for every format, 13- and 16-sector.

**Independent Test**: User Story 1's acceptance scenarios on made-up DSK, NIB, 13-sector WOZ, mixed and flux images.

- [X] T057 [P] [US1] Write `UT/PlatterGeometryTests.cpp`: ring per quarter track with track 0 at the rim, angle clockwise from the index, hit test returns quarter track and the sector whose field is under the point (not the first with that number), grooves at whole tracks
- [X] T058 [P] [US1] Write `UT/InspectorViewModelTests.cpp` for US1: chips text and hiding rules; sector row states, symbols and tooltips ("Sector $5, 6th past the index"); Sector data header fields; empty, damaged and no-data-field texts as FR-040 quotes them
- [X] T059 [US1] Create `UI/PlatterGeometry.h` / `.cpp` (pure: ring radii, angle mapping, hit testing, zoom and pan transforms)
- [X] T060 [US1] (nibble values and cell ticks on the zoomed platter per FR-026 still to do) Create `UI/PlatterView.h` / `.cpp`: Structure mode via `PlatterRenderer`, index mark, hub, grooves, hover and selected ring outlines, pending pattern, beyond-reach dimming and limit line, damaged hatch (FR-022, FR-023, FR-028)
- [X] T060a [US1] Platter tooltips per FR-027: track, kind, sector, the ring's sectors good out of found, flux cell time and deviation ("4.10µs cells (+4.8%)"); once nibble values show, the nibble value, offset and "cell N of M"; "Nothing recorded" with the random-bits note; none during a drag; text from a core formatter tested in `UT/InspectorViewModelTests.cpp`
- [X] T061 [US1] Create the toolbar chips with file name ellipsis and full-name tooltip in `UI/DiskInspectorWindow.cpp` (FR-018 display rules)
- [X] T062 [US1] Create `UI/TrackHeaderView.h` / `.cpp` (FR-032 lines) and `UI/TrackStripView.h` / `.cpp` with `UI/StripGeometry.h` / `.cpp`: unrolled track, kind colors, sector labels, selected sector outline in two parts across the index, write seam mark (FR-033, FR-036)
- [X] T063 [US1] Create `UI/SectorRowView.h` / `.cpp` (FR-039)
- [X] T064 [US1] Create `UI/SectorByteView.h` / `.cpp` (read-only in this phase): 16×16 hex and text columns as separate tab stops, zero and high-bit colors, bad data marked (FR-040, R20)
- [X] T065 [US1] Create `UI/NibblesTab.h` / `.cpp`: rows in steps of 8 sized to the pane, kind colors, sync widths, extra-zero counts, invalid nibbles and random-bit regions, tooltips, scroll-to-a-third on selection (FR-041)
- [X] T066 [US1] Notes in the track header and Image tab for sector images ("built from sector data") and NIB/NB2 ("no timing bits") (FR-003)

---

## Phase 7: User Story 2 - Find what is unusual or damaged (P1)

**Goal**: Findings, Tracks and Fields tabs, nibble timing details, decode settings.

**Independent Test**: Made-up image with one of each planted anomaly; Findings, Tracks and Fields against the planted list.

- [X] T067 (table rows, filters, counts and the decode settings form are tested; selecting a row is window code) [P] [US2] Extend `UT/InspectorViewModelTests.cpp`: Findings ordering, sorting, filtering and category counts (FR-049); selecting a finding selects its track, sector and field; Tracks rows (FR-047) including shared records and the synchronized-flag note; decode-settings chip and reset
- [X] T068 [US2] Create `UI/FindingsTab.h` / `.cpp` (FR-048, FR-049)
- [X] T069 [US2] Create `UI/TracksTab.h` / `.cpp` (FR-047 columns without the Casso-only marks)
- [X] T070 [US2] Create `UI/FieldsTab.h` / `.cpp` (FR-042, "Address field, checksum failed" wording per FR-023)
- [X] T071 [US2] Create `UI/DecodeSettingsDialog.h` / `.cpp`: marks with `??`, checks, track range, "Match standard marks too", "Reset to standard"; a change re-analyzes every track it covers; settings chip (FR-019, FR-020)
- [X] T071a (the switch is saved with the window's preferences in T101) [US2] "Alignment" overlay marking each track's sector 0 address field and longest sync run on the platter, with the overlay switch saved in preferences (FR-031)
- [X] T072 [US2] Show "not checked" sectors with their own color and symbol in the sector row, header, Fields tab and strip labels (FR-020)

---

## Phase 8: User Story 3 - Read flux timing (P2)

**Goal**: Timing mode on the platter and strip, Flux timing tab.

**Independent Test**: Made-up flux track mixing 3.7 µs and 4.1 µs cells.

- [X] T073 [P] [US3] Write `UT/FluxTimingTests.cpp`: per-cell deviation against 3.91 µs (about -5% and +5% for 3.7 and 4.1 µs), histogram peaks at written intervals, selection histogram covers only the sector, pairs within one cell marked, bit-track note
- [X] T074 [US3] Add Timing mode to `UI/PlatterView.cpp` and `UI/PlatterShader.hlsl` (deviation texture, range ±1% to ±25%, default ±5%, dimmed bit and sector-image tracks) and the legend for both modes (FR-024, FR-030)
- [X] T075 [US3] Add the timing line and per-cell timing to `UI/TrackStripView.cpp` (FR-033, FR-035)
- [X] T076 [US3] Create `UI/FluxTimingTab.h` / `.cpp`: interval plot at recorded time with 1, 2 and 3 cell lines, index and sector marks, linked zoom and pan, histogram with "Whole track" and "Selection" (FR-044 to FR-046)

---

## Phase 9: User Story 4 - Zoom, navigate, search and copy (P2)

**Goal**: Zoom and pan, keyboard, Go to, Find, selection, Copy, Export.

**Independent Test**: Keyboard-only walk; zoom, search and copy on a made-up image.

- [X] T077 [P] [US4] Extend `UT/PlatterGeometryTests.cpp` and create `UT/StripGeometryTests.cpp`: zoom about the pointer to 600×, recenter at fit, pan clamped to the disk, drag threshold as click; strip zoom to about 20 cells, fraction of the turn kept across tracks (FR-025, FR-034, FR-038)
- [X] T078 [P] [US4] Write `UT/InspectorSearchTests.cpp`: nibble patterns with `?` and `+`, "Any bit offset" finds unaligned matches, sector-data hex and text search with offsets, "No matches", Go to each target kind in its displayed base (FR-055, FR-056)
- [X] T079 [P] [US4] Write `UT/TrackExportTests.cpp`: sectors in physical, DOS 3.3 and ProDOS order with the bad and missing list; nibbles one byte each from the index; one-record WOZ 2.1 reopens with cell times equal to the original; image unchanged (FR-058, US4 scenario 10)
- [X] T080 [US4] Add zoom, pan, level-of-detail labels (nibble values, cell ticks) and keyboard to `UI/PlatterView.cpp` and `UI/TrackStripView.cpp` (FR-025, FR-026, FR-029, FR-034, FR-035, FR-037)
- [X] T081 [US4] Create `DD/Inspector/InspectorSearch.h` / `.cpp` and `UI/FindPanel.h` / `.cpp` (Ctrl+F, F3, Shift+F3) and `UI/GoToDialog.h` / `.cpp` (Ctrl+G). The CP/M sector target of FR-055 comes with the CP/M map in T109
- [X] T082 [US4] Add range selection and the length readout (nibbles, cells, µs) across strip, Nibbles and Sector data (FR-043)
- [X] T083 [US4] Add Copy formats of contracts/inspector-window.md, Clipboard, and "Copy sector" (FR-057) through a core clipboard text formatter in `DD/Inspector/InspectorClipboard.h` / `.cpp` with tests in `UT/InspectorClipboardTests.cpp`
- [X] T084 [US4] Create `DD/Inspector/TrackExport.h` / `.cpp` and the Export dialog in `UI/ExportDialog.h` / `.cpp` with default file names from contracts/inspector-window.md; write through `DurableCommit` (CreateNew; Replace after confirmation)
- [X] T085 [US4] Tooltips on every button, visible focus, hints text exactly as FR-007 and FR-034 quote (FR-059)

---

## Phase 10: User Story 5 - Check the image file itself (P2)

**Goal**: Image tab.

**Independent Test**: Made-up WOZ 1, WOZ 2, WOZ 2.1, bad checksum, damaged records, 3.5" disk type, DSK.

- [X] T086 [P] [US5] Extend `UT/WozImageDetailsTests.cpp` for the Image tab model: every row FR-050 lists per version, unreferenced records listed, 3.5" note on every other view (FR-054), unopenable file reason
- [X] T087 [US5] Create `UI/ImageTab.h` / `.cpp` (FR-050, FR-051, FR-054)

---

## Phase 11: User Story 6 - Watch the drive work in Casso (P3)

**Goal**: Live drive state, head marker, Follow head, refresh after writes, marks.

**Independent Test**: Boot DOS 3.3 with the inspector on drive 1, save a file from BASIC.

- [X] T088 [P] [US6] Write `UnitTest/Devices/Disk2WriteHookTests.cpp`: `OnLatchLoad` per FR-161's rule (motor running including spindown; not with the motor stopped; not for the //c IWM mode-register load); a dropped write sets WriteBlocked and counts nothing; counts per record; visit counts per quarter track; insert and reload reset; a save does not; state restore resets too once 035's machine state is on master (until then, no state restore exists to test)
- [ ] T089 [P] [US6] Write `UT/InspectorHostTests.cpp`: published record per drive matches the engine after a frame; requests handled on the emulation thread between passes; `mediaId` mismatch gives `DiskChanged`; one track analysis per written track and no whole-disk analysis (SC-006); hidden window does no work and catches up on show (FR-070)
- [ ] T090 [P] [US6] Write `UT/FollowHeadTests.cpp`: 0.3 s settle, hand selection turns it off, "Go to head" selects once
- [X] T091 [US6] Add `Disk2Controller::OnLatchLoad` (private) in `A2C/Disk2Controller.h` / `.cpp`, per-record `guestWriteCount` in `DD/DiskImage.h` / `.cpp`, per-drive visit counts, and make `Disk2NibbleEngine::GetAngle` public in `A2C/Disk2NibbleEngine.h`
- [ ] T092 [US6] Create `CEC/Shell/InspectorStatusPublisher.h` / `.cpp`: per-drive `DriveHeadState` (data-model.md section 5) built on the emulation thread at the end of `RunCpuThreadFrame` and swapped as `shared_ptr<const>` under a leaf mutex; never changed after publishing
- [ ] T093 [US6] Extend `CEC/Shell/InspectorRequestQueue.cpp` (built in T056) with `CopyAsInserted`, per-record write counts on copies, and re-copy of written tracks for re-analysis; copies share track buffers
- [ ] T094 [US6] Add "As inserted" copies per bay in `DD/DiskImageStore.cpp` at mount and reload, and at state restore once 035's machine state is on master, shared until the guest first writes a record (R13)
- [ ] T095 [US6] Add menu items "Inspect disk 1..." and "Inspect disk 2..." after salvage in `CEC/Ui/Chrome/EmulatorCommands.cpp` with ids in `Casso/resource.h`, routing in `CEC/Shell/WindowCommandManager.cpp`, enable rules in `CEC/Shell/Window/EmulatorWindow.cpp`, and right-click rows in `CEC/Shell/EmulatorShellStorage.cpp` `ShowStorageContextMenu` (FR-060)
- [ ] T096 [US6] Complete `CEC/Shell/EmulatorShellInspector.cpp`: at most one window, drive selector ("Slot 6 drives only" tooltip on a slot 5 machine), no-disk, not-attached and no-controller states, re-attach after a machine change, stays open across reset and pause (FR-061, FR-070)
- [ ] T097 [US6] Turning platter, head marker states (idle, reading, writing, write blocked), zoomed platter stays still with the marker moving, strip head position with fading trail and drive state (FR-064, FR-066)
- [ ] T098 [US6] Follow head and "Go to head" in `UI/InspectorViewModel.cpp` and the toolbar (FR-067)
- [ ] T099 [US6] Re-analysis of written tracks within 500 ms (twice a second during continuous bit writes), written and read marks with visit counts in the Tracks tab, "Reads" overlay, Find results marked out of date, write-protect state and causes in the toolbar (FR-047, FR-056, FR-065, FR-068, FR-069)
- [ ] T100 [US6] Image tab changed-value marks, records added since load, "Checked when the file was read", save note (FR-071)
- [ ] T101 [US6] Casso preferences: `diskInspector` block in `CEC/Config/GlobalUserPrefs.h` / `.cpp` (placement, mode, tabs, timing range, overlays, "Show deleted files", Follow head, last drive, open at exit) saved through `SaveGlobalPrefsDeferred`; reopen at launch on the last drive (FR-080)
- [ ] T102 [US6] Measure SC-005 (five alternating pinned runs at Maximum speed; five 60 s runs at normal speed) and record results in Notes

---

## Phase 12: User Story 8 - See which file owns each sector (P4)

**Goal**: File map tab for DOS 3.3, ProDOS (subdirectories after the rebase), Apple Pascal and CP/M.

**Independent Test**: SC-014 and SC-015 images.

- [X] T103 [P] [US8] Create made-up volume builders in `UT/FileMapTestImages.h` / `.cpp`: DOS 3.3 (three T/S lists, sparse text file, empty entry, deleted file), ProDOS (tree file with a hole, forked file, deleted file; subdirectories added in Phase 16), Apple Pascal in DO and PO order (varied last-block counts, a gap, `.BAD`), CP/M in DO and PO order (two extents, sparse, deleted, user 5, user 31 system entry), and each damaged case of SC-015
- [X] T104 [P] [US8] Write `UT/FileMapTests.cpp`: SC-014 roles, owners and file order with holes; no findings on standard disks of each system; ProDOS, DOS 3.3 and all-zero disks never mapped as Pascal or CP/M; SC-015 each problem once with place and reason, files before the damage still listed, map within 100 ms; volume-found tests of FR-084 and FR-085; not-mapped reasons of FR-094
- [X] T105 [US8] Create `DD/Inspector/FileMap/SectorSource.h` / `.cpp` and `DD/Inspector/FileMap/FileMap.h` (roles, results, owners, files, `MappedFile` per data-model.md)
- [X] T106 [P] [US8] Create `DD/Inspector/FileMap/Dos33MapReader.h` / `.cpp` (catalog chain, T/S lists, deleted entries, VTOC bitmap checks, bounded walks)
- [X] T107 [P] [US8] Create `DD/Inspector/FileMap/ProDosMapReader.h` / `.cpp` (volume directory, seedling, sapling, tree, extended key blocks for forks, deleted entries, bitmap checks, and subdirectories at any depth, which needed no stub)
- [X] T108 [P] [US8] Create `DD/Inspector/FileMap/PascalMapReader.h` / `.cpp` (header test of FR-085, runs of blocks, `.BAD` files, entry order and range findings)
- [X] T109 [P] [US8] Create `DD/Inspector/FileMap/CpmMapReader.h` / `.cpp` (directory test of FR-085, skew 3n mod 16, allocation blocks, extents, user 31 system entry, deleted entries)
- [X] T110 [US8] Add file-system findings (FR-093) and CP/M sector and allocation block to sectors (FR-040) in `DD/Inspector/DiskAnalyzer.cpp`; rebuild the map after each re-analysis without decoding again (FR-083)
- [X] T111 [US8] Create `UI/FileMapTab.h` / `.cpp`: grid per system, file list with sorting and "Files touching bad sectors", "Show deleted files", Previous and Next sector in file, "Copy map", volume choice when two are found, tooltips (FR-086 to FR-095)
- [X] T112 [US8] "Files" overlay on the platter and strip, Sector data header role and owner ("HELLO, data sector 3 of 5"), Go to a file, owners in Find results (FR-090, FR-091, FR-095)

---

## Phase 13: User Story 9, part 2 - Edit a sector (P5)

**Goal**: The editor on Merge 1's writer, in Casso.

**Independent Test**: SC-016 through the editor, SC-018 to SC-020.

- [ ] T113 [P] [US9] Write `UT/EditControllerTests.cpp`: overwrite-only typing in hex and text (high bit kept), paste stops at $FF and reports the overflow, per-sector "Recompute the checksum" with the header's predicted result (US9 scenario 3), Ctrl+Z and Ctrl+Y on pending edits only, Discard and Discard sector, editability reasons (FR-096 to FR-098)
- [ ] T114 [P] [US9] Extend `UT/InspectorHostTests.cpp`: apply at a safe point only; machine held from the change until saved or rolled back; post-apply verification failure restores; save failure restores and keeps edits pending ("Not applied: the file could not be saved"); preserved-copy save; undo and redo restore records exactly; unavailable after a guest write with the reason; guest write during a wait ends it and marks edits out of date; pending edits follow fields after a guest write, undo, redo and reload, and after a state restore once 035's machine state is on master (FR-099 to FR-109, SC-019, SC-020)
- [ ] T115 [P] [US9] Write `ScenarioTests/GuestVisibleInspectorEditTests.cpp`: SC-018's four cases (guest reads an edited text file; reads in a loop during "Pause and apply" see all old or all new bytes; writes in a loop end an apply wait; a test-written 5-and-3 read routine reads an edited 13-sector sector)
- [ ] T116 [US9] Create `UI/EditController.h` / `.cpp` (`PendingEdit`, `EditSet`, `AppliedEdit` per data-model.md section 4) and make `UI/SectorByteView.cpp` editable with change marks by color and symbol; mark sectors with pending edits in the sector row, Tracks tab and File map grid (FR-097)
- [ ] T117 [US9] Implement `Apply`, `Undo`, `Redo` and `CancelApply` in `CEC/Shell/InspectorRequestQueue.cpp` per contracts/emulation-thread.md, Safe point (R14): writer with `Editor` policy, re-analysis check, save through the store with `DurableCommit`, rollback, `isChangedByWriter` for the NIB rule, no change to `guestWriteCount` (FR-109)
- [ ] T118 [US9] Apply confirmation listing sectors, quarter tracks reading each record, file, TMAP-and-FLUX note, WOZ 1 to WOZ 2 note, NIB rebuild note, DOS-may-hold-the-sector note before the first apply; "Waiting for the drive to stop", "Pause and apply", "Cancel", "Run to a safe point and apply" (FR-100, FR-103, FR-104)
- [ ] T119 [US9] Write-protect causes beside "Apply"; "Save edited copy..." through `DurableCommit` (CreateNew; Replace after confirmation), Save dialog excludes the disk's file and mounted files, damaged-image notes (FR-101, FR-102)
- [ ] T120 [US9] Out-of-date edits, discard list after re-analysis, confirmations on drive switch, close, quit, machine change, decode settings change and eject (FR-108, Edge Cases)
- [ ] T121 [US9] Tracks tab mark for records an applied edit changed, taken from the applied-edit history, not from the writer flag a save clears (FR-109)

---

## Phase 14: User Story 10 - Compare two disks (P6)

**Goal**: Comparison in Casso.

**Independent Test**: SC-021 pairs and SC-022.

- [X] T122 [P] [US10] Create made-up pairs in `UT/ComparisonTestImages.h` / `.cpp` for every SC-021 case
- [X] T123 [P] [US10] Write `UT/DiskComparisonTests.cpp`: each planted verdict and difference once and nothing else; "Standard layout" and "Nothing recorded" not counted; tracks not compared never counted as matching; options change only the Differences tab and file comparison; timing under 1 s and 5 s in Release
- [X] T124 [US10] Create `DD/Inspector/DiskComparer.h` / `.cpp`: alignment by first shared address field or voted 8-nibble rotation, bounded Myers diff (64), flux timing within ±1%, verdicts of FR-118, sector pairing and volume differences (FR-120), file comparison by path in catalog order (R27)
- [ ] T125 [US10] Comparison sources in Casso (`ComparisonSource`: image file, drive now, as inserted, its file, other drive) through the request queue; B's own decode settings; "Swap A and B", "Stop comparing" (FR-117)
- [X] T126 [US10] Comparison bar, Differences tab, result chip, Tracks "Comparison" column, "Differences" overlay, B's strip linked below A's, B's bytes in Sector data, differing nibbles marked, Previous and Next difference, Copy (FR-121)
- [ ] T127 [US10] Re-comparison of changed records only, ends on eject, file deleted note, "Use B's bytes" as a pending edit (FR-122, FR-123)
- [ ] T128 [US10] Add SC-022 to `ScenarioTests/GuestVisibleInspectorEditTests.cpp`: after a guest SAVE, disk now vs as inserted differs only on written tracks and the new file is only in A

---

## Phase 15: Rebase onto 033

- [ ] T129 Wait until 033 has merged master (its T082); rebase `040-disk-inspector` onto `origin/033-casso-explorer` (Merge 1's commits drop out as already upstream); rebuild and run the full suite; fix renames surfaced by the compiler; `CheckStyle.ps1 -Mode Tree`

---

## Phase 16: User Story 7 - Inspect a disk image from Casso Explorer (P7), with Explorer parts of US8 to US10

**Goal**: Explorer preview, windows, edits, comparisons; ProDOS subdirectories and forks in them.

**Independent Test**: User Story 7's scenarios; Explorer parts of SC-007, SC-009, SC-017, SC-019, SC-020.

- [ ] T130 [P] [US7] Write `UnitTest/CassoExplorer/InspectorWindowSetTests.cpp`: one window per image, second "Inspect disk image" brings it to the front, several selected open several, "Compare disk images" with exactly two and A as the right-clicked image, windows close with Explorer and do not reopen; an Explorer window shows none of the Casso-only elements FR-001 lists (drive selector, Follow head, Go to head, turning disk, head marker and strip head state, drive write-protect causes, no-disk states, written and read marks, "Reads" overlay, out-of-date Find marks, Image tab changed-value marks and save note, safe-point wait, drive comparison sources)
- [ ] T131 [P] [US7] Write `UnitTest/CassoExplorer/DiskThumbnailTests.cpp`: catalog shown at once, placeholder then platter and chips, stale selections dropped, 3.5" note, error-only on failure, other formats get nothing (FR-072, SC-009 timing in Release)
- [ ] T132 [US7] Create `CEC/CassoExplorer/InspectorWindowSet.h` / `.cpp` beside `CassoExplorerShell::m_window`, with file watching through the folder watcher, re-analysis on change keeping the selected track, and the "no longer there" note (FR-073 to FR-075)
- [ ] T133 [US7] Add `Verb`s "Inspect disk image" and "Compare disk images" in `CEC/CassoExplorer/CassoExplorerActions.cpp` `GetListVerbs` (single image, two images, list background inside an image) and `ShowTreeContextMenu` in `CEC/CassoExplorer/CassoExplorerWindow.cpp`; command bar unchanged (FR-073, FR-076)
- [ ] T134 [US7] Create `CEC/CassoExplorer/DiskThumbnail.h` / `.cpp` above the existing preview in `CassoExplorerWindow::FillPreview`, on a shared newest-wins preview queue (R24); clicking opens the inspector
- [ ] T135 [US7] Explorer theme colors: `DiskInspectorColors` in `Dxui/Theme/DxuiLightTheme.cpp` and `DxuiDarkTheme.cpp`; extend `UT/DiskInspectorPaletteTests.cpp` to both; live theme switch redraws every view (FR-077, FR-078)
- [ ] T136 [US7] Explorer preferences block in `CEC/CassoExplorer/Model/CassoExplorerPrefs.h` / `.cpp` (FR-080)
- [ ] T137 [US9] Add `DiskOperations::ApplySectorEdits` ending in `CommitEdit` in `CEC/CassoExplorer/Model/DiskOperations.h` / `.cpp`; Explorer apply, undo and redo with the stale-file check, `ReloadInPlace` intent, reply shown when not a reload, edited tracks only re-analyzed, Explorer write-protect causes (FR-102, FR-105 to FR-107); tests in `UnitTest/CassoExplorer/`
- [ ] T138 [US9] Confirm 033's `RunMkdir`, `RunRmdir` and `CommitEdit` writes go through the shared writer and pass SC-017's checks; add those cases to `UT/DiskWritePathTests.cpp`
- [ ] T139 [US10] Explorer comparison sources ("As first opened"), "Compare disk images" switching an existing window after FR-108's confirmation (FR-117)
- [ ] T140 [US8] ProDOS subdirectories at any depth and forks inside them in `DD/Inspector/FileMap/ProDosMapReader.cpp`, using 033's subdirectory constants; extend `UT/FileMapTestImages.cpp` and `UT/FileMapTests.cpp` (two levels deep, US8 scenario 3); file comparison paths through subdirectories (FR-087, FR-120)
- [ ] T141 [US7] Add SC-020's Explorer-to-Casso scenario to `ScenarioTests/GuestVisibleInspectorEditTests.cpp` (with and without unsaved guest writes)

---

## Phase 17: Polish and Merge 2 gate

- [ ] T142 Keyboard walk through every command and view (SC-011), scripted where the view models allow, in `UT/InspectorKeyboardWalkTests.cpp`
- [ ] T143 On-screen checks in every theme of each host with captures (Casso: three themes; Explorer: Light and Dark), grayscale capture for good and bad states (SC-008); launch per the workspace rules (minimized, `--title`, portrait monitor)
- [ ] T144 SC-004 scripted zoom and pan measurement; SC-003, SC-015 and SC-021 timings in Release `PerformanceTests`; record in Notes
- [ ] T145 SC-007: open, use and close the inspector over every test image in both hosts without applying; every file byte-for-byte unchanged and no disk marked changed; SC-010 damaged cases shown as damaged in both hosts
- [ ] T146 *Bandits* locally (SC-012); real demo disks in `Apple2/Demos/` show no unexpected findings; tune R4 thresholds if needed and record in research.md
- [ ] T147 `scripts/CheckStyle.ps1 -Mode Tree`, full suite Debug and Release, ARM64 builds, code analysis, scenario suite (quickstart section 7)
- [ ] T148 CHANGELOG and README text to the owner after the owner has tested and approved; then merge to master with `--no-ff` once 033 is on master, watch CI to completion

---

## Dependencies & Execution Order

- **Phase 1 → Phase 2 → Phase 3 → Phase 4 (Merge 1)**. T024 is blocked on 041's settled `DurableCommit` signature; T025 to T030 can proceed before it, and T031 follows it.
- **Phase 5** depends on Phase 2 (framer, locator, field formats) and blocks Phases 6 to 17. It can start before Merge 1 lands.
- **Phases 6 to 10** (US1 to US5) depend on Phase 5; US2 to US5 build on US1's views.
- **Phase 11** (US6) depends on Phases 6 to 10's views.
- **Phase 12** (US8) depends on the analyzer (Phase 5) and the views.
- **Phase 13** (US9 editing) depends on Merge 1's writer, Phase 11 (requests, safe points) and Phase 12 (owner display).
- **Phase 14** (US10) depends on Phases 11 and 12.
- **Phase 15** waits for 033's master merge; **Phase 16** depends on it and on Phases 6 to 14.
- **Phase 17** depends on everything.

## Parallel Opportunities

- Phase 2 tests T005 to T010 are independent files; T011, T013, T016 can be written in parallel.
- Phase 3 tests T018 to T023 are independent; T025, T026 and T028 touch different files.
- Phase 5 tests T037 to T040 and T053 in parallel; T052 beside T041 to T049.
- Phase 12 walkers T106 to T109 in parallel once T105 is done.

## Parallel Example: Phase 12 (US8)

```text
Task: "Create Dos33MapReader in DD/Inspector/FileMap/Dos33MapReader.h / .cpp"
Task: "Create ProDosMapReader in DD/Inspector/FileMap/ProDosMapReader.h / .cpp"
Task: "Create PascalMapReader in DD/Inspector/FileMap/PascalMapReader.h / .cpp"
Task: "Create CpmMapReader in DD/Inspector/FileMap/CpmMapReader.h / .cpp"
```

## Implementation Strategy

No MVP. The first release ships whole, in two merges by owner decision:

1. **Merge 1** (Phases 1 to 4): the shared sector writer and what it needs, the
   GH #170 fix. Gates: full suite, code analysis, scenario suite, tree style,
   CHANGELOG text approved by the owner.
2. **Merge 2** (Phases 5 to 17): User Stories 1 to 10 in Casso, then in
   Explorer after the rebase onto 033. Merges only after 033 is on master.

Commit after each phase (constitution, Commit Discipline), with
`type(scope): description` subjects and GH #159 (Merge 2) or GH #170
(Merge 1) referenced.

## Notes

- Built-once parts (field formats and matcher, write hook, `DurableCommit`,
  drive record fields, loader split): check master before building each.
- Every label or message the spec does not quote goes to the owner before it
  ships.
- Baseline and measurement results are recorded below as they are taken.
- T002 (2026-10-08): after merging master `d78d2941a`, Release x64 ran 6,514 tests, all passing.
- T003 (2026-10-08): none of `DiskMarkPattern`, `DiskFieldFormat`, `DurableCommit`, `OnLatchLoad` or `DriveStatusPublisher` is on `origin/master`.
- T004: the projects keep no `.vcxproj.filters` files, so there was nothing to add.
- T013: `TrackCopy` is built; `DiskCopy` moves to T056, where the window first needs it.
- T021: the FR-114/FR-115 rules are covered by `SectorWriteThroughVolumeTests` (refusal with the quarter track and record), `ContainerIntegrityTests` (an unformatted bit-stream container takes no write) and `CrossFormatWriteTests`.
- T024 and T031 wait for 041's settled `DurableCommit` signature (041 session, 2026-10-08).
- 5-and-3 layout facts come from Apple's May 1978 13-sector read/write routines (Computer History Museum release) and Beneath Apple DOS; no code was copied.
