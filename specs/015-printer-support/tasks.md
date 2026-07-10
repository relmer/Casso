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
- [X] T005 [P] Write original slot firmware `CassoEmuCore/Devices/Printer/ParallelFirmware.a65` (PR#n hook, Pascal 1.1 signature bytes + entry table per contracts/printer-card-io.md) and generate `ParallelFirmware.h` (assembled bytes + source text literal) — 51 bytes, slot-independent CSW output routine honoring the ready bit; device-class/status values provisional pending T011
- [X] T006 [P] Unit tests: card register contract, byte ordering, ring overflow assert in `UnitTest/PrinterTests/PrinterCardTests.cpp` — window placement, FIFO ordering, tolerant status reads, high-water ready/busy transition, first-touch flag (7 tests)
- [X] T007 [P] Unit tests: assemble embedded .a65 source with CassoCore assembler, assert byte equality with `ParallelFirmware.h` array in `UnitTest/PrinterTests/FirmwareParityTests.cpp`
- [X] T008 Register device type `"parallel-printer"` in `CassoEmuCore/Core/ComponentRegistry.cpp`; install firmware via `CxxxRomRouter::SetSlotRom` and card on the bus during machine build in `Casso/EmulatorShell.cpp` — `PrinterCard::Create` factory; embedded firmware installed via `Apple2eMmu::AttachSlotRom` in `Casso/Shell/MachineManager.cpp`; +2 registry tests
- [X] T009 Add slot 1 `parallel-printer` entry to embedded machine JSONs (`Casso/Machines/Apple2e*.json`) and extend `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp` to add it to existing configs when slot 1 is free (FR-001); unit tests for the upgrade plan in `UnitTest/` alongside existing MachineConfigUpgrade tests — `Resources/Machines/Apple2e/Apple2e.json` v6→v7 + prior-hash in AssetBootstrap; `MigrateUserConfig` injects slot-1 printer honoring disabled entries; +3 tests
- [ ] T010 Implement byte-capture debug sink (menu-gated capture-to-file of the raw card stream, doubling as FR-009 unknown-command diagnostics) in `Casso/Print/HostPrintServices.h/.cpp` + `Casso/Shell/WindowCommandManager.cpp` (R-014)
- [ ] T011 CHECKPOINT: boot Print Shop, select Apple DMP/ImageWriter + Apple II Parallel Interface + slot 1, run test prints per parallel-interface menu option; archive captures as `UnitTest/Fixtures/Printer/*.bin` fixtures (own generated data); lock the R-001 status byte value and record findings in research.md

## Phase 3: User Story 1 — Print a Print Shop Page to a PNG File (P1) 🎯 MVP

**Goal**: full pipeline — guest bytes → interpreter → native raster → ink render → PNG file, with menu eject.

**Independent test**: unit goldens on synthetic + captured streams; end-to-end quickstart scenario 1 (sign → PNG, SC-009 circle check).

- [X] T012 [P] [US1] Implement `PrintRaster` (bitfield cells, chunked growth, page boundaries, FF marks, 60-page cap, Clear) in `CassoEmuCore/Devices/Printer/PrintRaster.h/.cpp`
- [X] T013 [P] [US1] Unit tests: strikes, boundaries, cap, clear in `UnitTest/PrinterTests/PrintRasterTests.cpp` — 10 tests (overprint OR, blank-feed preservation, FF page-top advance, cap)
- [X] T014 [US1] Implement `ImageWriterInterpreter` parser core + monochrome subset (ASCII passthrough consumed-not-rendered until US6 font; CR/LF/FF; pitch selections; line-spacing incl. half-height; bit-image graphics commands; reset; unknown-command consumption + event) emitting strikes + PrinterEvents, per R-003, in `CassoEmuCore/Devices/Printer/ImageWriterInterpreter.h/.cpp` — parser framework + control codes exact; ESC-command bytes / bit-image geometry marked PROVISIONAL pending T011 capture
- [X] T015 [US1] Unit tests: per-command-family goldens (cell spot checks), determinism (identical stream → identical raster+events) in `UnitTest/PrinterTests/ImageWriterInterpreterTests.cpp` — 10 tests; captured Print Shop fixture replay deferred to T011
- [X] T016 [US1] Implement `PaperRenderer` per R-005 (true-geometry resample, precomputed AA disc kernels at pin diameter, black ink path, Plain square style, 288/576 dpi, deterministic) in `CassoEmuCore/Devices/Printer/PaperRenderer.h/.cpp` — full 7-colour overprint palette + ribbon weave; `RgbaImage` container added
- [X] T017 [US1] Unit tests: geometry (SC-009 circle aspect ≤1%), dot roundness spot pixels, style/dpi matrix, determinism hashes in `UnitTest/PrinterTests/PaperRendererTests.cpp` — 9 tests
- [X] T018 [P] [US1] Implement `PrintJobSerializer` (strip+meta ⇄ indexed pixel plane + sidecar JSON per contracts/printing-settings.md) in `CassoEmuCore/Devices/Printer/PrintJobSerializer.h/.cpp` with round-trip tests in `UnitTest/PrinterTests/PrintJobSerializerTests.cpp` — pure (WIC PNG wrap deferred to PrintJobStore per R-007/R-010); added `PrintRaster::RestoreFromIndexed`; 5 tests
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

## Phase 11: User Story 4 (cont.) — Live-Print Preview Redesign (P4)

**Context**: Real Print Shop banner testing (2026-07-09) showed the first-cut panel re-rendered the whole strip every frame — O(rows²) time, unbounded memory — and delivery of a long banner exhausted memory. This phase implements the FR-032/033/034 presentation (1-page live viewport, scrollback, head-timing ink reveal, 3D scene) per the plan's "Preview Presentation Architecture" addendum. It refines Phase 6 (US4); the content pipeline stays 2D/testable and the 3D layer only presents the resulting texture.

**Independent test**: print a max-length banner — preview stays responsive with flat per-frame cost, memory bounded (SC-010); head sweeps L→R laying ink, viewport follows newest row, scrollback + snap-to-live work (SC-011).

**Shipped (2026-07-09):**

- [X] T061 [US4] Delivery memory bound: `WholeStripDpi()` caps whole-strip PNG/clipboard render dpi to a ~512 MB budget (toward but not below native); `CopyPrintoutToClipboard` renders once and encodes the PNG from that image (was a double render) in `Casso/Shell/WindowCommandManager.cpp` (FR-028; commit `de6ee886`)
- [X] T062 [US4] Windows-printer delivery tracing (driver name, page geometry, per-GDI-call GetLastError) to diagnose the PDF "failed to deliver" in `Casso/Shell/WindowCommandManager.cpp` (commit `f7b5cf92`)
- [X] T063 [US4] Interim live-preview fixes: non-destructive snapshot under raster lock, activity-resume auto-open, blank-sheet empty preview, message boxes owned by/centered on the panel (`PrinterDialogOwner` + `DxuiMessageBox` owner-centering), Escape-to-close in `Casso/EmulatorShell.cpp` + `Casso/Ui/PrinterPanel.cpp` + `Dxui/Window/DxuiMessageBox.cpp` (the strip-scaled refresh throttle here is a stopgap superseded by T065; commits `de6ee886`/`f7b5cf92`)

**Phase A — viewport + incremental render (the real perf fix; presentation-agnostic):**

- [X] T064 [P] [US4] Implement `PrinterViewport` — pure clock-injected scroll/follow/snap state (follow newest row while printing; wheel/touch/arrow offset; snap to live row after ~2 s idle; expose visible native-row span) in `CassoEmuCore/Devices/Printer/PrinterViewport.h/.cpp` + 11 unit tests in `UnitTest/PrinterTests/PrinterViewportTests.cpp` (FR-033)
- [X] T065 [US4] Rework `PrinterPanel` to a viewport-driven span render (~1-page span snapshot via new `PrintRaster::CopyRowSpan` + `PrinterWorker::SnapshotStripSpan`, rendered at fixed 144 dpi onto a constant full-page canvas, bottom-anchored, live row at the platen edge; no whole-strip snapshot or re-render anywhere — replaces the T063 throttle) plus the on-screen scroll hint, in `Casso/Ui/PrinterPanel.cpp` (FR-033, SC-010). NOTE: cost is bounded by the span (flat per frame); strict delta-only tile updates fold into T069's head-reveal dirty regions.
- [X] T066 [US4] Wire panel input: mouse wheel → viewport scroll (`OnMouse` Wheel intercept), Up/Down/PageUp/PageDown arrows (`OnKey`), snap-back timer via per-frame `RefreshLive` tick; Escape-to-close kept, in `Casso/Ui/PrinterPanel.cpp` (FR-033)

**Phase B — paper realism (into the content texture):**

- [X] T067 [US4] Render tractor-feed sprocket strips + holes down both edges and light perforations along the strips and between pages into the panel's content canvas (panel-only per FR-027): 9.5" stock at 144 dpi, 0.5" strips, 5/32" holes on 1/2" pitch punched transparent (mat shows through), perf dashes darken-blend over ink, phase strip-absolute so furniture feeds with the paper — `PrinterPanel::ComposeCanvas` in `Casso/Ui/PrinterPanel.cpp` (FR-032)

**Phase C — head-timing ink reveal:**

- [X] T068 [US4] Expose the print head's position from the pipeline without mutating the raster: `ImageWriterInterpreter::HeadColumnDots`, `PrinterJob::HeadRow/HeadColumnDots`, `PrinterWorker::HeadPosition` (row+column packed in one atomic, published per drain) in `CassoEmuCore/Devices/Printer/` + `Casso/Print/PrinterWorker.*` (FR-034)
- [X] T069 [US4] Extend `PrinterPacing` with a per-line column channel (`SetTargetPosition`/`RevealedColDots`: sweep at dotsPerSecond on arrival at the live line, full width while catching up, same-row max-hold so overprint passes never un-reveal ink, FF/jump-cut snap) + 5 unit tests; `PrinterPanel::RefreshLive` drives it (primed caught-up so restored strips don't replay; viewport follows the REVEALED edge) and `ComposeCanvas` clips the 16-row pin band at the paced head column in `CassoEmuCore/Devices/Printer/PrinterPacing.*` + `Casso/Ui/PrinterPanel.cpp` (FR-034, FR-031)

**Phase D — 3D presentation scene (needs user eyeball for final tuning):**

- [X] T070 [US4] Add a scoped 3D path to Dxui's D3D11 renderer — `Dxui3DRenderer`: one MVP cbuffer (row_major, row-vector), one textured+tinted VS/PS pair, dynamic VB, dynamic content texture + 1x1 white for untextured geometry, premultiplied source-over (same compositing as DxuiPainter), full state set per draw, no depth (painter's algorithm) — in `Dxui/Render/Dxui3DRenderer.h/.cpp` (FR-032)
- [X] T071 [US4] Implement `Printer3DScene` — procedural bottom-anchored ImageWriter body (platinum case, deck + slot recess, smoked carriage window, paced head carriage w/ four-color ribbon cartridge, power LED) + paper strip rising from the platen with backward lean and a backward curl (content canvas mapped 1:1 by arclength, slices darken as they turn from the light), drawn back-to-front from the panel window's before-present hook (backdrop → body-behind-paper → paper → body-front); panel leaves the paper rect unfilled and keeps `PrinterPaperView` as the flat fallback — in `Casso/Ui/Printer3DScene.h/.cpp` + `Casso/Ui/PrinterPanel.*` (FR-032)
- [ ] T072 [US4] End-to-end: long-banner print shows head sweeping L→R laying ink, viewport follows newest row, scrollback + snap-to-live, bounded memory / flat frame cost (SC-010/SC-011); user reviews the 3D scene aesthetics

## Dependencies

- Phase 2 blocks all stories (card, firmware, config, capture corpus).
- US1 blocks US2 (interpreter/renderer host the color additions) and provides eject/delivery for US3.
- US3's settings page precedes US4's audio-volume wiring (T028 → T040) — otherwise US4 depends only on US1.
- US5 depends on US1 (+US3 for Windows pagination); US6 depends on US1 only; US7 is independent of all printing phases after Phase 2 (pure recognition + toast).
- MVP = Phases 1–3. Each later phase is an independently shippable increment; commit per phase, push after commit.
- Phase 11 (preview redesign) refines Phase 6 (US4): needs the US1 pipeline + the US4 panel (T034/T035). Internally A→B→C→D — Phase A (viewport/incremental render) is the perf fix and lands first; B/C add into the same content texture; D (3D scene) presents it last and is the only part needing a live eyeball. T061–T063 already shipped.

## Parallel opportunities

- Phase 2: T003/T005/T006/T007 in parallel after T001–T002.
- US1: T012+T013 ∥ T018 while T014 proceeds; T016 after T012.
- US4: T033 ∥ T038 ∥ (T034→T035); US6 T046 ∥ T048 prep; US7 T050 ∥ T051 ∥ T053.
- US5/US6/US7 are mutually independent once US1 lands — parallelizable across sessions.
- Phase 11: T064 (`PrinterViewport`, pure) ∥ T067 (paper furniture) ∥ T068 (head-column signal) once Phase A's panel rework (T065) lands; T070 (3D pipeline) ∥ T068/T069 before T071 composes them.
