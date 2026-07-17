# Tasks: Emulated Printer Support (ImageWriter II)

**Input**: Design documents from `/specs/015-printer-support/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: INCLUDED — constitution Testing Discipline is non-negotiable; every pure component ships with its unit suite in the same phase.

**Organization**: Phases map to spec user stories (P1→US1 … P7→US7). Constitution commit discipline: commit after each completed phase. Push after commits (feature branch).

**Status — reconciled 2026-07-16** (every `[X]` is backed by code committed on this branch):
US1–US5 plus the Phase 11 preview redesign are implemented and unit-tested (full suite green). **US6 (draft-text printing, T046–T049) is not started; US7 (disk-title recognition, T050–T055) is DROPPED — auto-attach supersedes it, recognition polish moved to GH #78 (see Phase 9).** Several completed tasks shipped under different names than their original text:
- Delivery sinks (T020/T030/T031): the planned `HostPrintServices` was split into `PrintDelivery` + `PngCodec` + `PrintFileNaming` (core) plus the Print/Save/Copy handlers in `WindowCommandManager` — no `HostPrintServices` file exists.
- Pacing (T036): shipped as `PrinterPacing` + `PrinterViewport`, not a `PrinterPresenter`.
- "Eject" (T021/T037): replaced by per-action, non-destructive Print/Save/Copy + Form Feed + tear-off Discard.
- Audio (T038/T040): the BleuLlama sample set is **bundled** (committed, extracted by `AssetBootstrap::EnsureImageWriterSounds`), not fetched by the startup downloader; volume/mute wired to the Printing page.
- Long-strip memory (T042): a dpi cap (`WholeStripDpi`, ~512 MB budget), not row-banding.
Still open: T010 (dropped), US6 T046–T049, end-to-end sign-offs T026/T032/T041/T045/T072, polish / merge-gates T056–T060, and DCR-1/DCR-2. Dropped: US7 T050–T055 (→ #78). Post-ship bug fixes BUG-1/2/3/4 all landed and user-confirmed.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

- [X] T001 Create `CassoEmuCore/Devices/Printer/` and `Casso/Print/` directory skeletons; add all planned .h/.cpp stubs to `CassoEmuCore/CassoEmuCore.vcxproj(.filters)`, `Casso/Casso.vcxproj(.filters)`, and `UnitTest/UnitTest.vcxproj` per plan.md Project Structure (empty EHM-conformant stubs, x64 + ARM64 compile clean)
- [X] T002 [P] Define shared printer types (InkPrimary, PrinterEvent variants, DotStyle, grid constants 1280 dots/row, 144 rows/inch, 60-page cap) in `CassoEmuCore/Devices/Printer/PrinterTypes.h`

## Phase 2: Foundational (blocking all user stories)

- [X] T003 [P] Implement `PrinterByteRing` (fixed SPSC ring, O(1) push, pattern from `CassoEmuCore/Devices/InputEventRing.h`) in `CassoEmuCore/Devices/Printer/PrinterByteRing.h` — 64 KiB capacity + FreeSpace() for the high-water ready guard; unit suite `UnitTest/PrinterTests/PrinterByteRingTests.cpp` (8 tests incl. two-thread stress)
- [X] T004 Implement `PrinterCard : MemoryDevice` per contracts/printer-card-io.md ($C0n0 latch → ring, tolerant status reads, first-touch event flag, Reset/PowerCycle leave paper alone) in `CassoEmuCore/Devices/Printer/PrinterCard.h/.cpp` — ready bit driven by ring FreeSpace() high-water guard (FR-002)
- [X] T005 [P] Write original slot firmware `CassoEmuCore/Devices/Printer/ParallelFirmware.a65` (PR#n hook, Pascal 1.1 signature bytes + entry table per contracts/printer-card-io.md) and generate `ParallelFirmware.h` (assembled bytes + source text literal) — 51 bytes, slot-independent CSW output routine honoring the ready bit; device-class/status values provisional pending T011
- [X] T006 [P] Unit tests: card register contract, byte ordering, ring overflow assert in `UnitTest/PrinterTests/PrinterCardTests.cpp` — window placement, FIFO ordering, tolerant status reads, high-water ready/busy transition, first-touch flag (7 tests)
- [X] T007 [P] Unit tests: assemble embedded .a65 source with CassoCore assembler, assert byte equality with `ParallelFirmware.h` array in `UnitTest/PrinterTests/FirmwareParityTests.cpp`
- [X] T008 Register device type `"parallel-printer"` in `CassoEmuCore/Core/ComponentRegistry.cpp`; install firmware via `CxxxRomRouter::SetSlotRom` and card on the bus during machine build in `Casso/EmulatorShell.cpp` — `PrinterCard::Create` factory; embedded firmware installed via `Apple2eMmu::AttachSlotRom` in `Casso/Shell/MachineManager.cpp`; +2 registry tests
- [X] T009 Add slot 1 `parallel-printer` entry to embedded machine JSONs (`Casso/Machines/Apple2e*.json`) and extend `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp` to add it to existing configs when slot 1 is free (FR-001); unit tests for the upgrade plan in `UnitTest/` alongside existing MachineConfigUpgrade tests — `Resources/Machines/Apple2e/Apple2e.json` v6→v7 + prior-hash in AssetBootstrap; `MigrateUserConfig` injects slot-1 printer honoring disabled entries; +3 tests
- [ ] T010 **DROPPED** — the byte-capture tee was removed on this branch (no `HostPrintServices` ever shipped); FR-009 unknown-command diagnostics ride the interpreter's `UnknownCommand` `PrinterEvent` instead of a capture-to-file sink
- [X] T011 CHECKPOINT: boot Print Shop, select Apple DMP/ImageWriter + Apple II Parallel Interface + slot 1, run test prints per parallel-interface menu option; archive captures as `UnitTest/Fixtures/Printer/*.bin` fixtures (own generated data); lock the R-001 status byte value and record findings in research.md — DONE 2026-07-14 for Apple II Parallel + Grappler+ (both welcome tests pass end-to-end): status locked at `$83` (Grappler+ probes `(s & $07) == $03`), `ESC L` binary-count MSB-top graphics locked, findings in research.md R-001; captured stream encoded in `ImageWriterInterpreterTests` (welcome-prefix replay) in lieu of .bin fixtures; remaining interfaces (Epson APL etc.) can be captured the same way if ever reported broken

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
- [X] T019 [US1] Implement `PrintJobStore` (per-machine PendingPrint/ load-at-open, save on exit/eject/discard, corrupt→empty-silent) in `Casso/Print/PrintJobStore.h/.cpp` (FR-026)
- [X] T020 [US1] Implement PNG file sink (WIC encode, pHYs dpi, timestamped collision-free names, configurable folder with default `<Pictures>/Casso Prints`, failure notice retains strip) in `Casso/Print/HostPrintServices.h/.cpp` (FR-012)
- [X] T021 [US1] Wire the drain path in `Casso/EmulatorShell.cpp`: ring → interpreter → raster on UI tick, with high-water backpressure on the card's ready bit (R-001) so a stalled UI thread never drops a byte (FR-002); eject forces a synchronous full ring flush before rendering; add Eject / Finish Job menu command (delivers whole strip per FR-016, clears on success) in `Casso/Shell/WindowCommandManager.cpp`
- [X] T022 [US1] End-to-end validation: quickstart scenario 1 (Print Shop sign → PNG) + scenario 8 (persistence across relaunch); record results; clean up diagnostic artifacts

## Phase 4: User Story 2 — Color Printing with a Four-Color Ribbon (P2)

**Independent test**: synthetic 7-color band stream renders correct colors; New/original Print Shop color print end-to-end.

- [X] T023 [US2] Add `ESC K n` color selection + color state to reset semantics in `CassoEmuCore/Devices/Printer/ImageWriterInterpreter.h/.cpp`; strikes OR the active primary into cells
- [X] T024 [US2] Implement subtractive overprint mixing + composite derivation (orange/green/purple, black-dominance) and per-primary ink layers in `CassoEmuCore/Devices/Printer/PaperRenderer.h/.cpp` (FR-007, R-004/R-005)
- [X] T025 [P] [US2] Unit tests: seven-color golden bands, overprint composites, no-color-command → black in `UnitTest/PrinterTests/ImageWriterInterpreterTests.cpp` + `PaperRendererGoldenTests.cpp`
- [ ] T026 [US2] End-to-end: four-color ribbon Print Shop card (quickstart scenario 2); capture color byte stream as fixture

## Phase 5: User Story 3 — Delivering the Printout: Print / Save / Copy (P3)

**Independent test**: Print / Save / Copy each deliver the same one-page strip non-destructively (paper stays loaded); Copy pastes both formats; cancel retains. (The delivery destination is chosen per action, not a stored setting — see the 2026-07-14 clarification.)

- [X] T027 [P] [US3] Add printing fields to `Casso/Config/GlobalUserPrefs.h/.cpp` per contracts/printing-settings.md (round-trip preserved; render options only — resolution + dot style)
- [X] T028 [US3] Create Settings → Printing tab (`Casso/Ui/Settings/PrintingPage.h/.cpp`), register in `Casso/Ui/Settings/SettingsSheet.cpp` (FR-011; dpi, dot style, audio volume/mute placeholder — no destination selector or folder picker: delivery target is per-action)
- [X] T029 [US3] SPIKE (time-boxed 1 day): `IPrintManagerInterop` modern print dialog from unpackaged exe; record outcome here and in research.md R-009; choose dialog path
- [X] T030 [US3] Implement Windows printer sink (dialog per R-009 outcome, GDI true-scale centered pages via StretchDIBits at device dpi, page-count confirmation before dialog, cancel retains strip; print/spooler failure notifies the user and retains the strip for retry) in `Casso/Print/HostPrintServices.h/.cpp` (FR-014, output-failure edge case)
- [X] T031 [US3] Implement clipboard copy (registered "PNG" format immediate + delayed-render CF_DIB with size cap; clipboard-open/unavailable failure notifies the user and leaves the strip untouched) in `Casso/Print/HostPrintServices.h/.cpp` (FR-013, output-failure edge case) + Copy menu command in `Casso/Shell/WindowCommandManager.cpp`
- [ ] T032 [US3] End-to-end: quickstart scenario 3 (PDF via Microsoft Print to PDF, Paint paste, editor paste, cancel); assert one rendered strip can be Printed AND Saved AND Copied without re-printing because each delivery leaves the paper loaded (SC-007)

## Phase 6: User Story 4 — The Printer on the Desk (P4)

**Independent test**: full engage-print-eject cycle through indicator + panel, including audio and discard.

- [X] T033 [P] [US4] Implement `PrinterIndicator` chrome control (right-corner anchor in command bar dead space, vanishing-point skew, idle/receiving/pending/error states, config tooltip, click toggles panel) in `Casso/Ui/Chrome/PrinterIndicator.h/.cpp` (FR-019/021; never disturbs drive centering)
- [X] T034 [US4] Implement `PrinterPanel` docked right-edge surface on ChromeLayout (transient overlay on auto-reveal, inset when pinned) in `Casso/Ui/PrinterPanel.h/.cpp` (R-016)
- [X] T035 [US4] Render skeuomorphic printer + fanfold paper in the panel (four-color ribbon cartridge, sprocket strips/holes, cross-perf page boundaries from `PageBoundaryRows`, paper shows PaperRenderer output; paper furniture panel-only per FR-027; panel hover shows the same virtual-config summary as the indicator per FR-021) in `Casso/Ui/PrinterPanel.cpp`
- [X] T036 [US4] Implement `PrinterPresenter` pacing (R-012: ~250 cps replay clock, coalescing jump-cut, FastForward) in `Casso/Print/PrinterPresenter.h/.cpp`; reveal triggers on firmware entry + first byte (FR-020); extract the pacing/coalescing/fast-forward decisions as pure clock-driven math and unit-test them with an injected clock in `UnitTest/PrinterTests/PrinterPresenterTests.cpp`
- [X] T037 [US4] Panel controls: Form Feed (eject), Copy, tear-off Discard with confirmation; each forces a synchronous ring flush before acting on the strip; Discard clears strip + persistence (FR-029); wire to WindowCommandManager equivalents in `Casso/Ui/PrinterPanel.cpp` + `Casso/Shell/WindowCommandManager.cpp`
- [X] T038 [P] [US4] Prepare printer audio sample set: source authentic ImageWriter II recording (retro community / record one) or licensed period 9-pin fallback per R-011; slice into head-burst loop, line feed, form feed, paper tear; host alongside existing drive-audio assets with license + provenance manifest (HUMAN-IN-LOOP: sourcing/licensing sign-off)
- [X] T039 [US4] Implement `PrinterAudioSource : IDriveAudioSource` (event voices driven by presenter clock) in `CassoEmuCore/Audio/PrinterAudioSource.h/.cpp`, register with `DriveAudioMixer`; synthetic-PCM unit tests in `UnitTest/PrinterTests/`
- [X] T040 [US4] Add printer sample-set row to the consent-gated startup downloader in `Casso/AssetBootstrap.h/.cpp` (FR-030); volume/mute wired to Printing settings page
- [ ] T041 [US4] End-to-end: quickstart scenario 4 + acceptance 4.5 (audio in step with paper, tear on discard, mute silences); verify indicator-only pending state after closing panel mid-job

## Phase 7: User Story 5 — Banner Printing on Continuous Fanfold Paper (P5)

- [X] T042 [US5] Banded rendering + streamed PNG encode for long strips (bounded working memory) in `CassoEmuCore/Devices/Printer/PaperRenderer.cpp` + `Casso/Print/HostPrintServices.cpp` (FR-015/FR-028)
- [X] T043 [US5] Windows-printer pagination of a banner strip (page tiling, no lost/duplicated rows) + cap-reached finalize-and-notify path in `Casso/Print/HostPrintServices.cpp` (FR-015 cap, edge case)
- [X] T044 [P] [US5] Unit tests: pagination row accounting, cap behavior in `UnitTest/PrinterTests/PrintRasterTests.cpp`
- [ ] T045 [US5] End-to-end: quickstart scenario 5 (multi-page banner → seamless PNG; same banner → paginated PDF; SC-003)

## Phase 8: User Story 6 — Text Printing from BASIC and DOS (P6)

- [X] T046 [P] [US6] Author original draft dot-matrix glyph set (7-dot-column style, full printable ASCII; original work, no copied ROM font) in `CassoEmuCore/Devices/Printer/DraftFont.h` — designed as ASCII art in `scripts/GenDraftFont.py` (the design master; regenerates the table), 5x7 body + descender row, pre-centered in a 7-column cell, bit 0 = top pin like ESC G
- [X] T047 [US6] Text rendering in `ImageWriterInterpreter` (glyph columns at current pitch, right-margin wrap, LF spacing defaults) in `CassoEmuCore/Devices/Printer/ImageWriterInterpreter.cpp` — glyph sub-columns map onto the pitch cell with the ESC L numerator/denominator scheme so every documented density lays down cleanly; the 8th data bit is masked for text/commands/ASCII-digit params (PR#1 output arrives high-bit set) while binary params + graphics data keep all 8 bits; PLUS the slot firmware now injects LF after CR like Apple's real parallel card (BASIC/DOS send bare CRs; Print Shop drives the card I/O directly and is untouched) — `ParallelFirmware.a65`/`.h` regenerated, parity test green
- [X] T048 [P] [US6] Unit tests: text line goldens, wrap, pitch matrix in `UnitTest/PrinterTests/ImageWriterInterpreterTests.cpp` — glyph geometry at pica, high-bit ASCII, space, bare-CR overprint, 80-column wrap, pitch switching (replaces the obsolete PrintableAsciiIsConsumedNotRendered)
- [ ] T049 [US6] End-to-end: `PR#1` + `LIST` and DOS 3.3 `CATALOG` (quickstart scenario 6; SC-004) — needs a live guest run (user)

## Phase 9: User Story 7 — Recognizing Printing Software (P7) — **DROPPED 2026-07-16**

**Dropped.** The printer now **auto-attaches** as an optional slot-1 `parallel-printer`
device in every machine config (`Resources/Machines/{Apple2,Apple2Plus,Apple2e}.json`,
`capabilityFlag: optional`), and the user can deselect it from the Hardware settings
checkboxes. Per-title *recognition* is therefore no longer needed to make printing work —
it would only be polish (auto-suggesting the printer for known print software). That polish,
and the shared recognition subsystem it belongs to, now live in **GH #78** ("auto-enable
joystick mode for known game disks…"), which owns the same catalog/META/hash/filesystem-sniff
machinery; see the 2026-07-16 comment there. These tasks are not being implemented in 015.

- [~] T050 [P] [US7] ~~Retain META chunk key/values in `WozLoader`~~ — **DROPPED → #78**
- [~] T051 [P] [US7] ~~DOS 3.3 / ProDOS catalog-name extraction~~ — **DROPPED → #78**
- [~] T052 [US7] ~~Three-tier matcher + signature table~~ — **DROPPED → #78**
- [~] T053 [P] [US7] ~~TitleRecognizer unit tests~~ — **DROPPED → #78**
- [~] T054 [US7] ~~Mount-time hook + dismissible notice~~ — **DROPPED → #78**
- [~] T055 [US7] ~~End-to-end: quickstart scenario 7~~ — **DROPPED → #78**

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

## Design Change Requests (post-implementation, 2026-07-16)

Raised while dogfooding the shipped feature; larger than bug fixes, so tracked as DCRs
rather than folded silently into the spec.

- [X] DCR-1 **Windows print dialog "doesn't support print preview".** SHIPPED (needs the
  user's live pass). `Casso/Shell/ModernPrintDialog.{h,cpp}` launches the Windows modern print
  UI via raw-ABI WRL (no C++/WinRT dependency): `RoGetActivationFactory` →
  `IPrintManagerInterop::GetForWindow` + `ShowPrintUIForWindowAsync`; `PrintTaskRequested`
  hands the task a `PrintPageSource` (WRL RuntimeClass: `IPrintDocumentSource` marker +
  `IPrintPreviewPageCollection` + `IPrintDocumentPageSource`). Preview pages render the same
  `PrintPagination` + `PaperRenderer` spans the classic path prints (150 dpi into a private
  D3D/D2D stack, drawn to the `IPrintPreviewDxgiPackageTarget`) so the pane shows the real
  fanfold pages; the final job spools through `ID2D1PrintControl` at the configured 288/576
  dpi into the printer's imageable rect (width-fit + top-aligned, same composition as
  `BlitRgbaToDc`). The session COPIES the strip so the worker resumes immediately (async
  dialog); completion posts `IDM_PRINTER_MODERN_SENT/_FAILED` back to the UI thread for the
  result dialogs (cancel posts nothing, matching classic S_FALSE). Any launch failure falls
  back to the classic `PrintDlg`/GDI path, which keeps its honest SP_*/GLE error mapping.
- [X] DCR-2 **Replace the chrome printer indicator with a command toolbar.** BUILT (needs the
  user's visual pass): `CommandToolbar` (Ui/Chrome) docks a 42dp band below the menu bar with
  icon+label buttons -- Settings (MDL2 gear), Printer (miniature skeuomorphic ImageWriter II
  glyph carrying the status LED, dispatching IDM_PRINTER_PREVIEW), master Volume slider + Mute
  (NEW master output gain: one atomic gain over the completed WASAPI mix, persisted as
  masterVolume/masterMuted in GlobalUserPrefs), Screenshot, Reset, Power -- all existing IDM_*
  through HandleCommand. The standalone PrinterIndicator is retired (un-adopted, click-to-open
  removed); its class + dead layout plumbing get deleted after user sign-off.

## Bug fixes (post-implementation, 2026-07-16)

Shipped on this branch after real-hardware dogfooding:

- [X] BUG-1 Printing preview + sound froze during a title-bar / resize-edge drag (OS modal
  move/size loop starves the UI pump). Host-owned keep-alive `WM_TIMER` drives a new
  `IDxuiHostClient::OnModalLoopTick` → `EmulatorShell::PumpUiFrame`; render body factored out of
  `RunMessageLoop`. `Dxui/Window/*` + `Casso/EmulatorShell.*`.
- [X] BUG-2 Print head fluidity — **closed** (user-confirmed) after a chain of video-forensics
  iterations (frame-diff / carriage-tracking on user screen recordings). Root causes found and
  fixed, in order: (a) row-reveal and column-sweep ran as two independent rates, dumping bands
  full-width ~8.6× faster than the head could sweep (82f3f1db: one unified carriage clock — a
  full-width sweep reveals exactly one pin band); (b) the head glyph traced the reveal column's
  snap-to-full / reset-to-zero sawtooth (d09c44b6: ONE head clock — glyph, ink clip, and audio
  all read the same swept column, which parks at its margin and resumes there); (c) the first
  `Advance` after the loop's idle tick leapt a whole pass (33d6090b dt cap), which then throttled
  slow frames (b9041472: resume-nudge only for genuine parks; slow frames unthrottled).
  `CassoEmuCore/Devices/Printer/PrinterPacing.*` + `Casso/Ui/PrinterPanel.*` + tests.
- [X] BUG-3 Print audio — **closed** (user-confirmed). Buzz starved: the ink sampler's fixed
  0.3" lookback was narrower than the head's per-frame advance, skipping thin ink entirely
  (044aeb5a: sample the full swept span `[prevCol, curCol]`). The interim 0.25 s hold that had
  papered over the starvation then smeared a border-only line into a full-line buzz — at
  carriage speed 0.25 s is 78% of a pass (5e38a996: restore the 0.05 s edge-trigger release;
  strike / silence / strike on a border sign). A form feed / tear still cuts the hold clean.
  `CassoEmuCore/Audio/PrinterAudioSource.*` + `Casso/Ui/PrinterPanel.cpp` + tests.
- [X] BUG-4 Delivery-failure dialog was nerdspeak + a cancelled Print-to-PDF Save-As fired it.
  Cancel now silent (S_FALSE); message is plain-language with a `Details: 0x… — <system text>`
  trailer (CWRF captures the real GDI error). `Casso/Shell/WindowCommandManager.cpp`.

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
