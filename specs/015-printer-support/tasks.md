# Tasks: Emulated Printer Support (ImageWriter II)

**Input**: Design documents from `/specs/015-printer-support/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: INCLUDED — constitution Testing Discipline is non-negotiable; every pure component ships with its unit suite in the same phase.

**Organization**: Phases map to spec user stories (P1→US1 … P7→US7). Constitution commit discipline: commit after each completed phase. Push after commits (feature branch).

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

- [ ] T001 Create `CassoEmuCore/Devices/Printer/` and `Casso/Print/` directory skeletons; add all planned .h/.cpp stubs to `CassoEmuCore/CassoEmuCore.vcxproj(.filters)`, `Casso/Casso.vcxproj(.filters)`, and `UnitTest/UnitTest.vcxproj` per plan.md Project Structure (empty EHM-conformant stubs, x64 + ARM64 compile clean)
- [X] T002 [P] Define shared printer types (InkPrimary, PrinterEvent variants, DotStyle, grid constants 1280 dots/row, 144 rows/inch, 60-page cap) in `CassoEmuCore/Devices/Printer/PrinterTypes.h`

## Phase 2: Foundational (blocking all user stories)

- [X] T003 [P] Implement `PrinterByteRing` (fixed SPSC ring, O(1) push, pattern from `CassoEmuCore/Devices/InputEventRing.h`) in `CassoEmuCore/Devices/Printer/PrinterByteRing.h` — 64 KiB capacity + FreeSpace() for the high-water ready guard; unit suite `UnitTest/PrinterTests/PrinterByteRingTests.cpp` (8 tests incl. two-thread stress)
- [X] T004 Implement `PrinterCard : MemoryDevice` per contracts/printer-card-io.md ($C0n0 latch → ring, tolerant status reads, first-touch event flag, Reset/PowerCycle leave paper alone) in `CassoEmuCore/Devices/Printer/PrinterCard.h/.cpp` — ready bit driven by ring FreeSpace() high-water guard (FR-002)
- [ ] T005 [P] Write original slot firmware `CassoEmuCore/Devices/Printer/ParallelFirmware.a65` (PR#n hook, Pascal 1.1 signature bytes + entry table per contracts/printer-card-io.md) and generate `ParallelFirmware.h` (assembled bytes + source text literal)
- [X] T006 [P] Unit tests: card register contract, byte ordering, ring overflow assert in `UnitTest/PrinterTests/PrinterCardTests.cpp` — window placement, FIFO ordering, tolerant status reads, high-water ready/busy transition, first-touch flag (7 tests)
- [ ] T007 [P] Unit tests: assemble embedded .a65 source with CassoCore assembler, assert byte equality with `ParallelFirmware.h` array in `UnitTest/PrinterTests/FirmwareParityTests.cpp`
- [ ] T008 Register device type `"parallel-printer"` in `CassoEmuCore/Core/ComponentRegistry.cpp`; install firmware via `CxxxRomRouter::SetSlotRom` and card on the bus during machine build in `Casso/EmulatorShell.cpp`
- [ ] T009 Add slot 1 `parallel-printer` entry to embedded machine JSONs (`Casso/Machines/Apple2e*.json`) and extend `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp` to add it to existing configs when slot 1 is free (FR-001); unit tests for the upgrade plan in `UnitTest/` alongside existing MachineConfigUpgrade tests
- [ ] T010 Implement byte-capture debug sink (menu-gated capture-to-file of the raw card stream, doubling as FR-009 unknown-command diagnostics) in `Casso/Print/HostPrintServices.h/.cpp` + `Casso/Shell/WindowCommandManager.cpp` (R-014)
- [ ] T011 CHECKPOINT: boot Print Shop, select Apple DMP/ImageWriter + Apple II Parallel Interface + slot 1, run test prints per parallel-interface menu option; archive captures as `UnitTest/Fixtures/Printer/*.bin` fixtures (own generated data); lock the R-001 status byte value and record findings in research.md

## Phase 3: User Story 1 — Print a Print Shop Page to a PNG File (P1) 🎯 MVP

**Goal**: full pipeline — guest bytes → interpreter → native raster → ink render → PNG file, with menu eject.

**Independent test**: unit goldens on synthetic + captured streams; end-to-end quickstart scenario 1 (sign → PNG, SC-009 circle check).

- [ ] T012 [P] [US1] Implement `PrintRaster` (bitfield cells, chunked growth, page boundaries, FF marks, 60-page cap, Clear) in `CassoEmuCore/Devices/Printer/PrintRaster.h/.cpp`
- [ ] T013 [P] [US1] Unit tests: strikes, boundaries, cap, clear in `UnitTest/PrinterTests/PrintRasterTests.cpp`
- [ ] T014 [US1] Implement `ImageWriterInterpreter` parser core + monochrome subset (ASCII passthrough consumed-not-rendered until US6 font; CR/LF/FF; pitch selections; line-spacing incl. half-height; bit-image graphics commands; reset; unknown-command consumption + event) emitting strikes + PrinterEvents, per R-003, in `CassoEmuCore/Devices/Printer/ImageWriterInterpreter.h/.cpp`
- [ ] T015 [US1] Unit tests: per-command-family goldens (hash + cell spot checks), determinism (identical stream → identical hash), captured Print Shop fixture replay in `UnitTest/PrinterTests/ImageWriterInterpreterTests.cpp`
- [ ] T016 [US1] Implement `PaperRenderer` per R-005 (true-geometry resample, precomputed AA disc kernels at pin diameter, black ink path, Plain square style, 288/576 dpi, deterministic) in `CassoEmuCore/Devices/Printer/PaperRenderer.h/.cpp`
- [ ] T017 [US1] Unit tests: geometry (SC-009 circle aspect ≤1%), dot roundness spot pixels, style/dpi matrix, determinism hashes in `UnitTest/PrinterTests/PaperRendererGoldenTests.cpp`
- [ ] T018 [P] [US1] Implement `PrintJobSerializer` (strip+meta ⇄ indexed PNG bytes + sidecar JSON per contracts/printing-settings.md) in `CassoEmuCore/Devices/Printer/PrintJobSerializer.h/.cpp` with round-trip tests in `UnitTest/PrinterTests/PrintJobSerializerTests.cpp`
- [ ] T019 [US1] Implement `PrintJobStore` (per-machine PendingPrint/ load-at-open, save on exit/eject/discard, corrupt→empty-silent) in `Casso/Print/PrintJobStore.h/.cpp` (FR-026)
- [ ] T020 [US1] Implement PNG file sink (WIC encode, pHYs dpi, timestamped collision-free names, configurable folder with default `<Pictures>/Casso Prints`, failure notice retains strip) in `Casso/Print/HostPrintServices.h/.cpp` (FR-012)
- [ ] T021 [US1] Wire the drain path in `Casso/EmulatorShell.cpp`: ring → interpreter → raster on UI tick, with high-water backpressure on the card's ready bit (R-001) so a stalled UI thread never drops a byte (FR-002); eject forces a synchronous full ring flush before rendering; add Eject / Finish Job menu command (delivers whole strip per FR-016, clears on success) in `Casso/Shell/WindowCommandManager.cpp`
- [ ] T022 [US1] End-to-end validation: quickstart scenario 1 (Print Shop sign → PNG) + scenario 8 (persistence across relaunch); record results; clean up diagnostic artifacts

## Phase 4: User Story 2 — Color Printing with a Four-Color Ribbon (P2)

**Independent test**: synthetic 7-color band stream renders correct colors; New/original Print Shop color print end-to-end.

- [ ] T023 [US2] Add `ESC K n` color selection + color state to reset semantics in `CassoEmuCore/Devices/Printer/ImageWriterInterpreter.h/.cpp`; strikes OR the active primary into cells
- [ ] T024 [US2] Implement subtractive overprint mixing + composite derivation (orange/green/purple, black-dominance) and per-primary ink layers in `CassoEmuCore/Devices/Printer/PaperRenderer.h/.cpp` (FR-007, R-004/R-005)
- [ ] T025 [P] [US2] Unit tests: seven-color golden bands, overprint composites, no-color-command → black in `UnitTest/PrinterTests/ImageWriterInterpreterTests.cpp` + `PaperRendererGoldenTests.cpp`
- [ ] T026 [US2] End-to-end: four-color ribbon Print Shop card (quickstart scenario 2); capture color byte stream as fixture

## Phase 5: User Story 3 — Choosing the Output Destination (P3)

**Independent test**: each destination delivers a one-page job; Copy pastes both formats; cancel retains.

- [ ] T027 [P] [US3] Add printing fields to `Casso/Config/GlobalUserPrefs.h/.cpp` per contracts/printing-settings.md (round-trip preserved)
- [ ] T028 [US3] Create Settings → Printing tab (`Casso/Ui/Settings/PrintingPage.h/.cpp`), register in `Casso/Ui/Settings/SettingsSheet.cpp` (FR-011; destination, folder picker, dpi, dot style, audio volume/mute placeholder)
- [ ] T029 [US3] SPIKE (time-boxed 1 day): `IPrintManagerInterop` modern print dialog from unpackaged exe; record outcome here and in research.md R-009; choose dialog path
- [ ] T030 [US3] Implement Windows printer sink (dialog per R-009 outcome, GDI true-scale centered pages via StretchDIBits at device dpi, page-count confirmation before dialog, cancel retains strip; print/spooler failure notifies the user and retains the strip for retry) in `Casso/Print/HostPrintServices.h/.cpp` (FR-014, output-failure edge case)
- [ ] T031 [US3] Implement clipboard copy (registered "PNG" format immediate + delayed-render CF_DIB with size cap; clipboard-open/unavailable failure notifies the user and leaves the strip untouched) in `Casso/Print/HostPrintServices.h/.cpp` (FR-013, output-failure edge case) + Copy menu command in `Casso/Shell/WindowCommandManager.cpp`
- [ ] T032 [US3] End-to-end: quickstart scenario 3 (PDF via Microsoft Print to PDF, Paint paste, editor paste, cancel, persistence of destination selection); assert a destination change in Settings takes effect on the very next eject with no relaunch (SC-007)

## Phase 6: User Story 4 — The Printer on the Desk (P4)

**Independent test**: full engage-print-eject cycle through indicator + panel, including audio and discard.

- [ ] T033 [P] [US4] Implement `PrinterIndicator` chrome control (right-corner anchor in command bar dead space, vanishing-point skew, idle/receiving/pending/error states, config tooltip, click toggles panel) in `Casso/Ui/Chrome/PrinterIndicator.h/.cpp` (FR-019/021; never disturbs drive centering)
- [ ] T034 [US4] Implement `PrinterPanel` docked right-edge surface on ChromeLayout (transient overlay on auto-reveal, inset when pinned) in `Casso/Ui/PrinterPanel.h/.cpp` (R-016)
- [ ] T035 [US4] Render skeuomorphic printer + fanfold paper in the panel (four-color ribbon cartridge, sprocket strips/holes, cross-perf page boundaries from `PageBoundaryRows`, paper shows PaperRenderer output; paper furniture panel-only per FR-027; panel hover shows the same virtual-config summary as the indicator per FR-021) in `Casso/Ui/PrinterPanel.cpp`
- [ ] T036 [US4] Implement `PrinterPresenter` pacing (R-012: ~250 cps replay clock, coalescing jump-cut, FastForward) in `Casso/Print/PrinterPresenter.h/.cpp`; reveal triggers on firmware entry + first byte (FR-020); extract the pacing/coalescing/fast-forward decisions as pure clock-driven math and unit-test them with an injected clock in `UnitTest/PrinterTests/PrinterPresenterTests.cpp`
- [ ] T037 [US4] Panel controls: Form Feed (eject), Copy, tear-off Discard with confirmation; each forces a synchronous ring flush before acting on the strip; Discard clears strip + persistence (FR-029); wire to WindowCommandManager equivalents in `Casso/Ui/PrinterPanel.cpp` + `Casso/Shell/WindowCommandManager.cpp`
- [ ] T038 [P] [US4] Prepare printer audio sample set: source authentic ImageWriter II recording (retro community / record one) or licensed period 9-pin fallback per R-011; slice into head-burst loop, line feed, form feed, paper tear; host alongside existing drive-audio assets with license + provenance manifest (HUMAN-IN-LOOP: sourcing/licensing sign-off)
- [ ] T039 [US4] Implement `PrinterAudioSource : IDriveAudioSource` (event voices driven by presenter clock) in `CassoEmuCore/Audio/PrinterAudioSource.h/.cpp`, register with `DriveAudioMixer`; synthetic-PCM unit tests in `UnitTest/PrinterTests/`
- [ ] T040 [US4] Add printer sample-set row to the consent-gated startup downloader in `Casso/AssetBootstrap.h/.cpp` (FR-030); volume/mute wired to Printing settings page
- [ ] T041 [US4] End-to-end: quickstart scenario 4 + acceptance 4.5 (audio in step with paper, tear on discard, mute silences); verify indicator-only pending state after closing panel mid-job

## Phase 7: User Story 5 — Banner Printing on Continuous Fanfold Paper (P5)

- [ ] T042 [US5] Banded rendering + streamed PNG encode for long strips (bounded working memory) in `CassoEmuCore/Devices/Printer/PaperRenderer.cpp` + `Casso/Print/HostPrintServices.cpp` (FR-015/FR-028)
- [ ] T043 [US5] Windows-printer pagination of a banner strip (page tiling, no lost/duplicated rows) + cap-reached finalize-and-notify path in `Casso/Print/HostPrintServices.cpp` (FR-015 cap, edge case)
- [ ] T044 [P] [US5] Unit tests: pagination row accounting, cap behavior in `UnitTest/PrinterTests/PrintRasterTests.cpp`
- [ ] T045 [US5] End-to-end: quickstart scenario 5 (multi-page banner → seamless PNG; same banner → paginated PDF; SC-003)

## Phase 8: User Story 6 — Text Printing from BASIC and DOS (P6)

- [ ] T046 [P] [US6] Author original draft dot-matrix glyph set (7-dot-column style, full printable ASCII; original work, no copied ROM font) in `CassoEmuCore/Devices/Printer/DraftFont.h`
- [ ] T047 [US6] Text rendering in `ImageWriterInterpreter` (glyph columns at current pitch, right-margin wrap, LF spacing defaults) in `CassoEmuCore/Devices/Printer/ImageWriterInterpreter.cpp`
- [ ] T048 [P] [US6] Unit tests: text line goldens, wrap, pitch matrix in `UnitTest/PrinterTests/ImageWriterInterpreterTests.cpp`
- [ ] T049 [US6] End-to-end: `PR#1` + `LIST` and DOS 3.3 `CATALOG` (quickstart scenario 6; SC-004)

## Phase 9: User Story 7 — Recognizing Printing Software (P7)

- [ ] T050 [P] [US7] Retain META chunk key/values in `CassoEmuCore/Devices/Disk/WozLoader.h/.cpp` (currently ignored at load) and expose on the loaded image
- [ ] T051 [P] [US7] Implement DOS 3.3 / ProDOS catalog-name extraction as pure functions over denibblized sector data (high-bit masking for DOS 3.3; ProDOS volume+file names) in `CassoEmuCore/Devices/Printer/TitleRecognizer.h/.cpp`
- [ ] T052 [US7] Implement three-tier matcher (META > filename substring > catalog) + embedded signature table per contracts/printing-settings.md in `CassoEmuCore/Devices/Printer/TitleRecognizer.cpp`
- [ ] T053 [P] [US7] Unit tests: synthetic filenames/META/catalogs, tier precedence, no-match silence in `UnitTest/PrinterTests/TitleRecognizerTests.cpp`
- [ ] T054 [US7] Mount-time hook (Drive 1 only) + one dismissible setup-guidance notice per mount, zero config side effects, in `Casso/Shell/DiskManager.cpp` (FR-024)
- [ ] T055 [US7] End-to-end: quickstart scenario 7 (known name, generic name w/ standard filesystem, unknown disk)

## Phase 10: Polish & Cross-Cutting

- [ ] T056 [P] FR-018 validation: sustained print burst with audio/video observation; confirm O(1) emu-thread cost (no allocation on write path)
- [ ] T057 [P] SC-005 coverage audit: every supported command has a golden; SC-008 walkthrough (setup questions answerable from UI alone)
- [ ] T058 Update `CHANGELOG.md` and `README.md` (feature, test counts, roadmap, and the documented paper-model defaults — 8.5″ stock / 8″ printable / 11″ page per FR-008) per merge gates
- [ ] T059 Run `scripts\Build.ps1 -RunCodeAnalysis` both archs + full `scripts\RunTests.ps1`; fix all findings (merge gate)
- [ ] T060 Workspace hygiene sweep: remove capture files, test PNGs, stray diagnostics; final quickstart full pass

## Dependencies

- Phase 2 blocks all stories (card, firmware, config, capture corpus).
- US1 blocks US2 (interpreter/renderer host the color additions) and provides eject/delivery for US3.
- US3's settings page precedes US4's audio-volume wiring (T028 → T040) — otherwise US4 depends only on US1.
- US5 depends on US1 (+US3 for Windows pagination); US6 depends on US1 only; US7 is independent of all printing phases after Phase 2 (pure recognition + toast).
- MVP = Phases 1–3. Each later phase is an independently shippable increment; commit per phase, push after commit.

## Parallel opportunities

- Phase 2: T003/T005/T006/T007 in parallel after T001–T002.
- US1: T012+T013 ∥ T018 while T014 proceeds; T016 after T012.
- US4: T033 ∥ T038 ∥ (T034→T035); US6 T046 ∥ T048 prep; US7 T050 ∥ T051 ∥ T053.
- US5/US6/US7 are mutually independent once US1 lands — parallelizable across sessions.
