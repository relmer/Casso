# Implementation Plan: Disk Inspector

**Branch**: `040-disk-inspector` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/040-disk-inspector/spec.md`

This plan covers the first release (User Stories 1 to 10). The later release
(User Stories 11 to 16) gets its own plan update before it is built; this plan
reserves only its seams (research R30).

## Summary

Casso gets a disk inspector window, opened from Casso on a drive or from Casso
Explorer on a file, that analyzes every quarter track of a disk image and
shows it as a platter, a track strip, sector, nibble, field and flux timing
views, a findings list, the image file's own structure, a file and sector map,
sector editing, and comparison of two disks (GH #159). The plan:

- **Loader**: `WozLoader` keeps what the file holds (INFO through one parser,
  the maps as stored, every record's fields, chunks, META) and splits
  zero-length records into empty and damaged (FR-053).
- **Analyzer**: a pure core library over immutable track copies. A latch
  framer reproduces the drive's read latch and MC3470 rule; field decoders for
  16- and 13-sector formats sit on field formats and a `??` mark matcher shared
  with 035.
- **Views**: a Dxui window, with the platter drawn by a polar-mapping pixel
  shader through one new Dxui hook, so zoom from fit to 600× costs the same at
  every level.
- **Casso host**: the emulation thread publishes a per-drive head record once
  per frame, counts guest writes in one write hook shared with 035, and serves
  copy, apply, undo and redo requests at safe points.
- **Shared sector writer**: rewrites a data field in place on bit, flux, NIB
  and sector images, finding tracks through the map. The `disk` command's
  writes, Explorer's writes and the inspector's editor all use it, which also
  fixes GH #170's three defects. It goes to master first, in a merge of its own. Saves go through `DurableCommit`, built to
  041's contract.
- **File map**: four read-only walkers (DOS 3.3, ProDOS, Apple Pascal, CP/M)
  over the analyzed sectors.
- **Comparison**: aligns tracks by a shared address field or a voted rotation
  and diffs nibbles with a bounded Myers diff.
- **Explorer hosts**: built last, after 040 is rebased onto 033.

## Technical Context

**Language/Version**: C++ (MSVC v145, VS 2026), stdcpplatest; HLSL for the
platter shader

**Primary Dependencies**: None new. Dxui (in tree) gains one hook for custom
D3D11 drawing in a widget (R19).

**Storage**: Disk image files through `IDiskFileIo`; preferences in
`GlobalUserPrefs` (Casso) and `CassoExplorerPrefs` (Explorer)

**Testing**: Microsoft Native CppUnitTest (`UnitTest`), `ScenarioTests`
(required: the drive, loader and sector code change), Release
`PerformanceTests`; all images made up, *Bandits* and *Prince of Persia* local
only

**Target Platform**: Windows 10/11 x64; ARM64 build-only

**Project Type**: Desktop emulator; core static libraries with zero-code
executables

**Performance Goals**: Analyze a 140 KB disk under 100 ms and a 160-record flux
WOZ under 1 s (SC-003); median platter and strip frame time at most one refresh
interval, none over 33 ms (SC-004); emulation at Maximum speed within 2% with
the window open (SC-005); written tracks shown within 500 ms (SC-006); map
within 100 ms of analysis (SC-015); compare two 140 KB disks under 1 s, two
flux WOZ images under 5 s (SC-021); Explorer thumbnail within 300 ms (SC-009)

**Constraints**: Viewing never changes a disk (FR-002, SC-007). The window
thread never touches live disk state (FR-063; 041's ownership rule). A write
never moves a field, a sync run or a flux transition outside the data field
(FR-110 to FR-113). Saves are durable (FR-116).

**Scale/Scope**: 160 quarter tracks; up to about 51,200 cells and 25,000 flux
transitions per record; images up to a few MB; 147 FRs in the spec, of which
FR-124 to FR-147 are the later release.

## Constitution Check

*Checked before Phase 0 and again after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Code Quality | Pass | EHM throughout; analyzer and writer return failure, never `S_OK`, after a partial change. Every threshold is a constant, not a literal (R4). New classes each get a `.h`/`.cpp` pair |
| II. Testing Discipline | Pass | Every FR in the first release is reachable from `UnitTest` with made-up images; file I/O through `IDiskFileIo` fakes, including a fault-injecting one for every save step; real-file I/O tests only in `ScenarioTests`. Each new test is shown to fail first. Scenario suite required and run |
| III. UX Consistency | Pass | Error messages in the standard format; every label and message the spec does not give goes to the owner; `disk` command exit codes unchanged except where a write now fails under FR-114 or FR-115 |
| IV. Performance | Pass, with measurement | SC-003 to SC-006, SC-009, SC-015 and SC-021 measured on the owner's machine; the GPU platter is spiked before views depend on it (R19) |
| V. Simplicity | Pass, with one justified addition | One writer replaces the rebuild path. The Dxui custom-drawing hook is the one framework addition (Complexity Tracking) |
| VI. Thin Executable | Pass | Nothing in `Casso/` or `CassoExplorer/`; window logic in core view models the tests drive |

Re-checked after Phase 1 design: no violations beyond the one in Complexity
Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/040-disk-inspector/
├── spec.md
├── plan.md                      # this file
├── research.md                  # R1-R30, and the branch survey
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── sector-writer.md
│   ├── emulation-thread.md
│   ├── woz-image-details.md
│   └── inspector-window.md
└── checklists/requirements.md
```

### Source code

```text
CassoCore/
├── DiskMarkPattern.h / .cpp                 # NEW (shared with 035 T713): marks with ?? wildcards
└── DiskFieldKind.h                          # NEW (shared with 035 T713)

Dxui/
└── Widgets/DxuiCustomVisual.h / .cpp        # NEW: a widget that draws D3D11 content in its rectangle

CassoEmuCore/
├── Devices/Disk/
│   ├── DiskFieldFormat.h / .cpp             # NEW (shared with 035 T713): 4-and-4, 6-and-2, 5-and-3, checksums
│   ├── SectorFieldWriter.h / .cpp           # NEW: the shared sector writer
│   ├── DurableCommit.h / .cpp               # NEW (041's contract, built once)
│   ├── DiskImage.h / .cpp                   # ResolveWholeTrack, write counts, edit flag, per-quarter-track damage
│   ├── DiskImageSession.cpp                 # CommitImage on DurableCommit
│   ├── DiskImageStore.h / .cpp              # as-inserted copies, metadata copy, edit saves
│   ├── IDiskFileIo.h, Seams/Win32DiskFileIo # FlushToStorage, CopyFileMetadata, RenameWithoutReplacing
│   └── Inspector/                           # NEW
│       ├── TrackCopy.h, DiskCopy.h          # immutable copies
│       ├── LatchFramer.h / .cpp             # cells and nibbles as the latch frames them
│       ├── TrackAnalyzer.h / .cpp           # fields, sectors, classes, measurements, findings
│       ├── ITrackFormatDecoder.h            # 16- and 13-sector decoders; later-release seam
│       ├── DecodeSettings.h / .cpp
│       ├── DiskAnalyzer.h / .cpp            # summary, cross-track findings, quarter-track entries
│       ├── FindingFormatter.h / .cpp        # finding text
│       ├── ImageDetails.h / .cpp            # Image tab model, FR-051 checks
│       ├── TrackExport.h / .cpp             # sectors, nibbles, one-record WOZ
│       ├── DiskComparer.h / .cpp            # verdicts, alignment, bounded diff, file comparison
│       └── FileMap/
│           ├── SectorSource.h / .cpp
│           ├── Dos33MapReader.h / .cpp
│           ├── ProDosMapReader.h / .cpp
│           ├── PascalMapReader.h / .cpp
│           └── CpmMapReader.h / .cpp
├── Machines/Apple2/Common/
│   ├── WozLoader.h / .cpp                   # ReadInfo, file layout, FR-053 split, Serialize keeps maps
│   ├── WozMetadata.h                        # WozInfo, WozFileLayout
│   ├── WozCompatibility.cpp                 # ReadRequirements over ReadInfo
│   ├── NibblizationLayer.h / .cpp           # map lookup; rebuild path no longer used by writes
│   ├── NibbleImageCodec.cpp                 # edited NIB tracks written in place
│   ├── VolumeImage.cpp                      # SaveBitStream hands changed sectors to the writer
│   ├── TrackWritability.cpp                 # FR-114, FR-115
│   ├── Disk2Controller.h / .cpp             # OnLatchLoad (shared with 035 T715), visit counts
│   └── Disk2NibbleEngine.h / .cpp           # GetAngle public, activity state
├── Shell/
│   ├── InspectorStatusPublisher.h / .cpp    # NEW: per-frame drive record
│   ├── InspectorRequestQueue.h / .cpp       # NEW: requests and replies, safe points
│   ├── EmulatorShellInspector.cpp           # NEW: open, switch drive, re-attach after machine change
│   ├── EmulatorShellStorage.cpp             # right-click items
│   └── WindowCommandManager.cpp             # menu routing
├── Ui/Chrome/EmulatorCommands.cpp, CassoTheme.h   # menu items; inspector colors
├── Ui/DiskInspector/                        # NEW
│   ├── DiskInspectorWindow.h / .cpp
│   ├── IDiskInspectorHost.h                 # host seam; NullDiskInspectorHost
│   ├── InspectorViewModel.h / .cpp          # selection, navigation, Follow head, pending edits
│   ├── PlatterView.h / .cpp, PlatterRenderer.h / .cpp, PlatterShader.hlsl
│   ├── PlatterGeometry.h / .cpp             # zoom, pan, hit testing (pure)
│   ├── TrackStripView.h / .cpp, StripGeometry.h / .cpp
│   ├── SectorByteView.h / .cpp              # 256-byte view and editor
│   ├── NibblesTab, FieldsTab, FluxTimingTab, TracksTab, FindingsTab, ImageTab, FileMapTab, DifferencesTab (.h / .cpp each)
│   ├── EditController.h / .cpp              # pending, apply, undo, redo, out of date
│   └── DiskInspectorPalette.h / .cpp        # fallback colors, symbols
├── Config/GlobalUserPrefs.h / .cpp          # diskInspector block
└── CassoExplorer/                           # after the rebase onto 033
    ├── InspectorWindowSet.h / .cpp          # NEW: modeless windows per image
    ├── DiskThumbnail.h / .cpp               # NEW: preview platter and chips
    ├── Model/DiskOperations.h / .cpp        # ApplySectorEdits
    ├── Model/CassoExplorerPrefs.h / .cpp    # inspector block
    └── CassoExplorerActions.cpp, CassoExplorerWindow.cpp   # verbs, menus

UnitTest/
├── Devices/DiskFieldFormatTests.cpp, DiskMarkPatternTests.cpp, Disk2WriteHookTests.cpp
└── EmuTests/
    ├── InspectorTestImages.h / .cpp        # made-up images with planted anomalies
    ├── LatchFramerTests.cpp, TrackAnalyzerTests.cpp, DiskAnalyzerTests.cpp
    ├── WozImageDetailsTests.cpp, SectorFieldWriterTests.cpp, DurableCommitTests.cpp
    ├── FileMapTests.cpp, DiskComparisonTests.cpp, InspectorHostTests.cpp
    ├── InspectorViewModelTests.cpp, PlatterGeometryTests.cpp, DiskInspectorPaletteTests.cpp
    └── (existing) WozLoaderTests, DamagedDiskMountTests, DiskWritePathTests, CrossFormatWriteTests, DiskCommandRunnerTests

ScenarioTests/
└── GuestVisibleInspectorEditTests.cpp      # NEW: SC-018, SC-020, SC-022
```

**Structure Decision**: The existing layout. New code goes in
`Devices/Disk/Inspector/` (analysis), `Ui/DiskInspector/` (views) and
`CassoExplorer/` (Explorer host); the writer and `DurableCommit` sit beside
the image code they serve.

## Implementation order

Build order only. By the owner's decision of 2026-10-08, the first release
ships in **two merges**: the shared sector writer goes to master first, on its
own, as the fix for GH #170, and everything else in User Stories 1 to 10
follows in one merge after 033 is on master. Each phase ends with its tests
green and is committed.

### Merge 1: shared sector writer (GH #170)

Built on `040-disk-inspector` from master, merged to master with `--no-ff`
under its own gates, before any view exists. It changes what the `disk`
command and the assemblers' `--disk` output write; Explorer's writes inherit it
through `SaveAndCommit` once 033 merges master.

0. **Merge master** (once the woz-info-fields merge `d78d2941a` is on master). Check
   which "built once" parts are already on master.
1. **Field formats and the `??` matcher** (035 T713's design): 6-and-2,
   5-and-3, 4-and-4 tables and checksums against vectors worked by hand.
2. **Latch framer and field locator**: the part of the analyzer the writer
   needs to find each data field (R3, R15): `TrackCopy`, `LatchFramer`, and the
   16- and 13-sector field decoders with pairing. Test: SC-002.
3. **What saves need from the loader**: the stored TMAP and FLUX maps and the
   record table of `WozFileLayout` (R8), and `WozLoader::Serialize` writing
   TMAP from them, so a save keeps the standard layout, a bit record FLUX
   overrides, and unreferenced records (R17). The rest of R8 and R9 stays in
   Merge 2.
4. **`DurableCommit`** to 041's settled signature (R16; 041 is sending it), in
   **one self-contained commit** that 041 cherry-picks: `DurableCommit.h/.cpp`,
   the `IDiskFileIo` additions with their Win32 and fake implementations,
   project and filter entries, fake-based tests in `UnitTest` and real-file
   cases in a new `ScenarioTests` file. The commit's SHA goes to 041 when
   pushed. Moving `Win32DiskFileIoTests.cpp` stays with 041.
5. **The writer and its callers**: `ResolveWholeTrack` and every map lookup
   (`GetTrackBits`, `DecodeTracks`, `RenibblizeTracks`,
   `WriteFluxTrackSectors`), `SectorFieldWriter` with both policies,
   `TrackWritability` under FR-114 and FR-115 with the new messages, the flux
   timing fix (FR-113), the NIB in-place save (R17, keyed on a "changed by the
   writer" flag), `VolumeImage::SaveBitStream` on the writer, and
   `DiskImageSession::CommitImage` on `DurableCommit`. Tests: SC-017 in full,
   and SC-016 for every format through the `disk` command.
6. **Merge 1 gate**: full suite (Debug and Release), code analysis, scenario
   suite, `CheckStyle.ps1 -Mode Tree`, CHANGELOG entry for GH #170 and FR-113
   (text to the owner first). Close GH #170 with the merge.

GH #171 and #172 stay out of Merge 1 (R15).

### Merge 2: the rest of the first release

7. **Loader and image details** (US5, FR-050 to FR-054): `WozInfo` and
   `ReadInfo`, the rest of `WozFileLayout`, the FR-053 split and new damage
   reasons including a bit record whose count exceeds its blocks (owner
   decision, 2026-10-08), per-quarter-track damage, FR-051 checks. Tests:
   SC-010's damaged cases, the split table.
8. **Analyzer** (US1, US2, US3's data), on Merge 1's framer and decoders:
   classes, measurements, findings, summary, decode settings. Tests: SC-001,
   SC-003, SC-013. Tune the 50% and 2% thresholds against the demo disks (R4).
9. **Views** (US1 to US5 on screen): spike the Dxui hook and platter shader
   first and measure SC-004; then the window, platter, strip, tabs, Go to,
   Find, Copy, Export, themes and palette tests. In Casso only, opened on a
   copy.
10. **Casso host** (US6): write hook and counts, visit counts, the drive
    record, the request queue, menus and right-click items, Follow head,
    refresh after writes, preferences. Tests: FR-062, FR-068, FR-069 (one
    track analyzed per written track, SC-006). Measure SC-005.
11. **File map** (US8, without ProDOS subdirectories): sector source, the four
    walkers, roles, findings, the File map tab, "Files" overlay. Tests:
    SC-014, SC-015.
12. **Editing** (US9 in Casso), on Merge 1's writer: the editor, pending edits,
    safe-point apply, undo, redo, rollback, "Save edited copy...". Tests:
    SC-016 through the editor, SC-019, SC-020; scenario SC-018.
13. **Comparison** (US10 in Casso): sources including "As inserted",
    alignment, verdicts, differences, file comparison, the comparison views.
    Tests: SC-021; scenario SC-022.
14. **Rebase onto 033** once 033 has merged master (its T082); Merge 1's
    commits are already upstream by then. Then the Explorer hosts (US7),
    editing and comparing in Explorer, ProDOS subdirectories and forks in
    them, Light and Dark colors. Tests: Explorer parts of SC-007, SC-009,
    SC-017, SC-019, SC-020.
15. **Merge 2 gate**: quickstart section 7, on-screen checks in every theme
    (SC-008), keyboard walk (SC-011), *Bandits* locally (SC-012). Merges only
    after 033 is on master.

## Coordination

By message, never by editing another branch:

| Session | Message | When |
|---|---|---|
| 041-disk-integrity | Loader split, `DurableCommit`, drive record (R9, R11, R16). Sent; 041 replied: split agreed, `ReserveBlankTracks` skips damaged images, the `DurableCommit` signature follows when its revision settles, and the drive record must be built on the emulation thread and never changed after publishing (as R11 plans). Tell it Merge 1 puts `DurableCommit` on master first | Sent; follow-up sent 2026-10-08 |
| 035-debugger | Merge 1 puts T713 (field formats, matcher) on master early; 040 builds T715 (`OnLatchLoad`) to its design if it merges first, and does not build T714 (R5, R6); ask when spec-2 merges into 035-debugger | Sent 2026-10-08 |
| 033-Casso Explorer | Merge 1 changes `SaveAndCommit`'s write path under 033's callers; after the rebase 040 adds a modeless window set, a thumbnail above the preview, new verbs and `DiskOperations::ApplySectorEdits` (R22, R24, R25) | Sent 2026-10-08 |
| Woz info fields | 040 moves INFO parsing into `WozLoader::ReadInfo` and `WozInfo` after the woz-info-fields merge `d78d2941a` lands, without changing `ReadRequirements`' signature (R8) | Sent 2026-10-08 |

## Owner decisions (2026-10-08)

1. **GH #170 gets the shared sector writer early**, as Merge 1, ahead of the
   rest of 040 (Implementation order). The spec's Delivery paragraph and its
   GH #170 Assumptions still describe a single first-release merge and a
   separate #170 fix; they are updated to match.
2. **A bit record whose count exceeds its blocks is damage** (R9), as for
   flux records, so such images are write-protected at mount.
3. **Keyboard shortcuts** Ctrl+G for Go to, and Ctrl+F, F3 and Shift+F3 for
   Find, are approved (contracts/inspector-window.md).
## Risks

- **The Dxui hook disturbs the frame or the device's state.** Mitigation: it
  is the first item of the views step (tasks.md T051), measured before anything depends on it, with
  the CPU fallback in R19.
- **The framer disagrees with the engine** (SC-002). Mitigation: the test
  compares against the engine itself, and the two share constants.
- **Merge churn**: master's sweeping renames, 033's `CassoExplorerWindow.cpp`
  (about 10.9k lines, changing daily), and `DxuiTheme.h` and `CassoTheme.h`
  preset blocks that 033 and 035 also change. Mitigation: merge master early
  and often, keep Explorer edits to small, separate insertions, and run
  `CheckStyle.ps1 -Mode Tree` before each master merge.
- **Built-once parts collide.** 035, 040 and 041 could each build a part the
  others plan. Mitigation: Phase 0 checks master first; names and contracts
  already match the other specs.
- **Safe points never arrive** on a disk the guest writes continuously.
  Mitigation: the wait ends on each guest write to an edited record (FR-103),
  "Cancel" is always available, and "Pause and apply" takes the next pass
  boundary outside the edited fields.

## Later release

Its plan update covers User Stories 11 to 16 (FR-124 to FR-147), A2R reading,
"Trace boot" in a hidden machine, the RW18 decoder and the protection
pattern table. The first release reserves its seams (R30): per-range decode
settings, `ITrackFormatDecoder`, `TrackCaptures`, a lane list in the Nibbles
tab, a `Protection` finding category, and quarter tracks as a comparison
source.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| A new Dxui hook for custom D3D11 drawing (`DxuiCustomVisual`) | The platter must zoom from 160 rings to single cells (600×) with every frame within one refresh interval (SC-004); a shader samples only the cells under each pixel at any zoom | CPU quads through `FillConvexQuad` need a level-of-detail run list rebuilt on every zoom step; kept as the fallback if the spike fails |
