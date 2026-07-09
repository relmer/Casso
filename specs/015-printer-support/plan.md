# Implementation Plan: Emulated Printer Support (ImageWriter II)

**Branch**: `015-printer-support` | **Date**: 2026-07-07 (rev. 2026-07-09: live-print preview presentation) | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/015-printer-support/spec.md`

## Summary

Emulate an ImageWriter II-class printer for the Apple //e: a parallel interface
card in slot 1 (enabled by default) feeds a pure byte-stream interpreter that
rasterizes the C. Itoh 8510 + Apple command subset onto a native 160×144 dpi
dot grid. A deterministic CPU "paper renderer" turns the dot grid into
true-geometry ink imagery (round overlapping pin splats, overprint color
mixing, ribbon-weave modulation). Output is job-based: a single continuous
fanfold strip per machine that persists across sessions, ejected explicitly to
host print services — PNG file or Windows printer — with clipboard copy as an
on-demand action. A compact chrome indicator plus an on-demand panel window
(skeuomorphic printer with animated, perforated paper and sampled audio, paced
at realistic print speed) provide discoverability, preview-before-print, and
the eject/copy/discard controls. The panel's preview is a live-print
presentation (FR-032–034): a ~1-page viewport follows the print head with an
incremental new-rows-only render, wheel/touch/arrow scrollback snaps back to
the live row, ink appears per head column as the head sweeps, and a contained
real-3D scene (bottom-anchored printer body, fanfold paper curling out of
view) presents the 2D-rendered content texture. Print-title recognition
(filename → WOZ META → filesystem catalog) surfaces a setup-guidance notice at
mount time.

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145 / VS 2026), Win32 desktop

**Primary Dependencies**: Windows SDK only — GDI printing, WIC (PNG
encode/decode), existing WASAPI + `DriveAudioMixer` audio stack, existing
D3D11 dxui chrome (spec 013), existing `JsonParser`/`JsonWriter`. The panel's
3D presentation is an *additive* path on the existing Dxui D3D11 pipeline
(one MVP constant buffer + one textured/lit HLSL shader + two meshes — the
painter already owns custom shaders/vertex buffers and the text renderer
already samples textures; see research R-017). No new third-party dependencies
(constitution allowlist unchanged). The in-repo assembler (CassoCore) builds
the slot firmware.

**Storage**: Global prefs in `GlobalUserPrefs` JSON (new printing fields);
per-machine pending-strip persistence as PNG + versioned JSON sidecar under
the per-machine user-state directory; audio samples via the existing
consent-gated `AssetBootstrap` downloader.

**Testing**: Microsoft CppUnitTest (`UnitTest/` project). Interpreter,
raster, paper renderer, recognizer, serializer, firmware, pacing, status
model, and viewport are pure / data-driven per FR-017 and constitution Test
Isolation — no file, registry, network, or clipboard access in unit tests;
golden verification via checked-in hash constants and small embedded expected
tiles; clock-dependent models (pacing, status, viewport) take injected clocks.

**Target Platform**: Windows 10/11, x64 and ARM64

**Project Type**: Existing multi-project desktop application (CassoEmuCore
static lib + Casso Win32 shell + UnitTest)

**Performance Goals**: FR-018 — zero measurable emulation impact: the card's
guest-facing `Write` is an O(1) ring-buffer push on the emulation thread
(pattern: `InputEventRing`). Interpretation/rasterization drain on a dedicated
printer worker thread. Eject-time render of a typical 1-3 page job at 576 dpi
completes in ≤ ~2 s; long strips scale linearly (FR-028 allows it). Preview:
per-frame cost flat regardless of strip length — the panel renders only
newly-produced rows into a persistent buffer inside a ~1-page viewport
(FR-033, SC-010); whole-strip delivery renders are dpi-capped against a fixed
~512 MB RGBA budget (FR-028, research R-018).

**Constraints**: Deterministic interpreter and renderer (FR-009, FR-027);
strip cap 60 fanfold pages (FR-015); native grid 1280 dots × 144 rows/inch;
firmware must be original work (FR-003); no new third-party code; presentation
must never mutate the (immediately-complete) raster or worker state (FR-034 —
the mid-print preview distortion bug proved why).

**Scale/Scope**: One new CassoEmuCore device family (`Devices/Printer/`), one
audio source, one shell services module, one settings page, one chrome
indicator + panel window, a viewport state machine (core) + 3D scene module
(shell) + scoped Dxui 3D render path, machine-config upgrade step, ~10 new
unit-test suites.

## Constitution Check

*GATE: evaluated against constitution v1.8.0 — PASS (pre-Phase-0, re-checked
post-Phase-1, and re-checked 2026-07-09 for the preview presentation rev). No
violations to justify; Complexity Tracking omitted.*

- **I. Code Quality**: All new code follows EHM, declaration alignment,
  5-blank-line separation, `s_k` statics. The interpreter's command dispatch
  uses EHM bail-outs / guard-continue loops to stay flat; functions ≤ 50 lines
  (dispatch split per command family).
- **II. Testing Discipline**: FR-017 mirrors the constitution: interpreter,
  raster, renderer, recognizer, persistence serialization, pacing, status
  model, and the FR-033 `PrinterViewport` are pure functions / clock-injected
  state machines over supplied data. File/clipboard/printer/dialog I/O and the
  3D scene live in thin shell adapters excluded from unit scope. Golden
  rasters are hash-verified (no disk reads). Firmware parity test assembles
  the checked-in `.a65` source with the in-repo assembler and compares to the
  embedded bytes.
- **III. UX Consistency**: Printing settings join `SettingsSheet` as a new
  page (Disk-tab visibility precedent); eject/copy/discard also exposed as
  `WindowCommandManager` commands; existing notice/toast and themed-dialog
  patterns reused (printer dialogs are `DxuiMessageBox`, owner-centered).
  Backward compatibility: config upgrade is additive and the card is
  per-machine disable-able; with the card disabled, slot 1 behavior is
  bit-identical to today.
- **IV. Performance**: Emulation-thread work is O(1) per guest write; no
  allocation on the hot path (fixed ring). Rendering/delivery run off-thread
  at eject time; preview work is viewport-bounded (never O(strip)) per frame.
- **V. Simplicity**: One printer, one card, one command set (SSC/6551
  explicitly deferred to the //c effort). No speculative abstraction beyond
  the `IPrinterByteSink` seam the //c serial port will eventually reuse. The
  3D path is contained — one constant buffer, one shader, two meshes,
  presenting a 2D texture the testable pipeline produced; a flat fallback
  remains possible (FR-032). It is deliberately the pilot primitive the drive
  widgets adopt when they later move to true 3D.
- **VI. Thin Executable, Testable Core**: Pure logic (viewport, pacing,
  status, interpreter, renderer, serializer) lives in CassoEmuCore for
  UT-reachability; the shell contributes only irreducible platform edges
  (GDI, WIC file I/O, clipboard, dialogs, D3D presentation).
- **Tech Constraints**: No allowlist amendment needed — WIC/GDI/D3D11 are
  Windows SDK; audio samples are downloaded assets (existing consent flow),
  not vendored code.

## Project Structure

### Documentation (this feature)

```text
specs/015-printer-support/
├── plan.md              # This file
├── research.md          # Phase 0 output (R-001..R-018)
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   ├── printer-card-io.md        # Guest-visible register + firmware contract
│   ├── printer-pipeline-api.md   # C++ interfaces: card → interpreter → raster → renderer → sinks
│   └── printing-settings.md      # Prefs fields, machine-config slot entry, persistence sidecar
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Devices/Printer/
│   ├── PrinterCard.h/.cpp            # MemoryDevice on $C0n0-$C0nF; O(1) byte ring; "parallel-printer" type
│   ├── PrinterByteRing.h             # Fixed lock-free SPSC ring (pattern: InputEventRing)
│   ├── ImageWriterInterpreter.h/.cpp # 8510+Apple command subset → dot/motion events (pure); EXTEND: head-column stream (FR-034)
│   ├── PrintRaster.h/.cpp            # Native-grid strip: 4-primary bitfield cells, page boundaries, 60-page cap
│   ├── PaperRenderer.h/.cpp          # Deterministic CPU ink renderer (discs, overprint mix, weave; square style)
│   ├── PrintJobSerializer.h/.cpp     # Strip+state ⇄ memory buffers (pure); shell owns file I/O
│   ├── PrintPagination.h/.cpp        # Pure page-boundary/page-height row spans (Windows-printer delivery)
│   ├── PrinterPacing.h/.cpp          # Pure clock-driven reveal pacing; EXTEND: per-column head reveal (FR-034)
│   ├── PrinterStatusModel.h/.cpp     # Pure clock-injected indicator state machine (FR-019/020)
│   ├── PrinterViewport.h/.cpp        # NEW: pure scroll/follow/snap viewport state (FR-033)
│   ├── PngCodec.h/.cpp               # WIC PNG encode/decode (in core per Principle VI — tests drive it)
│   ├── PrintDelivery.h/.cpp          # Render span → PNG bytes (core seam the shell sinks call)
│   ├── TitleRecognizer.h/.cpp        # Pure filename / catalog-name / META-title matching
│   └── ParallelFirmware.h            # Generated: firmware bytes + embedded .a65 source text
├── Devices/Printer/ParallelFirmware.a65   # Original slot firmware source (assembled in FirmwareParityTests)
├── Devices/Disk/WozLoader.h/.cpp     # MODIFIED: retain META chunk key/values
└── Audio/PrinterAudioSource.h/.cpp   # IDriveAudioSource; event-driven sample playback

Casso/
├── Print/
│   ├── PrinterWorker.h/.cpp          # Drain thread owning PrinterJob; raster lock, SnapshotStrip, RowsUsed; EXTEND: head-column signal
│   ├── PrintJobStore.h/.cpp          # Per-machine pending-strip persistence (file I/O around serializer)
│   └── PrinterPresenter.h/.cpp       # Presentation fan-out to panel + audio (drives PrinterPacing)
├── Ui/Chrome/PrinterIndicator.h/.cpp # Corner indicator (state LEDs, tooltip, click → panel)
├── Ui/PrinterPanel.h/.cpp            # Panel window (separate DxuiWindow, peer of main — R-016 update); REWORK: viewport-driven incremental render
├── Ui/PrinterPaperView.h/.cpp        # Content control; REWORK: persistent tile buffer feeding the 3D layer
├── Ui/Printer3DScene.h/.cpp          # NEW: chassis + curled-paper meshes, perspective camera (FR-032)
├── Ui/Settings/PrintingPage.h/.cpp   # Settings → Printing tab
├── Config/GlobalUserPrefs.h/.cpp     # MODIFIED: printing fields
├── AssetBootstrap.h/.cpp             # MODIFIED: printer audio sample set row
├── Shell/WindowCommandManager.cpp    # MODIFIED: eject / copy / discard / preview commands, delivery sinks + WholeStripDpi cap + delivery tracing
└── Machines/*.json (embedded)        # MODIFIED: slot 1 parallel-printer default

Dxui/
└── Render/                           # EXTEND: scoped 3D path — MVP cbuffer + textured/lit shader (R-017)

CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp  # MODIFIED: add printer card on upgrade

UnitTest/
└── PrinterTests/
    ├── ImageWriterInterpreterTests.cpp
    ├── ImageWriterCalibrationTests.cpp   # Bit order / pin pitch locked from real Print Shop capture (T011)
    ├── PrintRasterTests.cpp
    ├── PaperRendererGoldenTests.cpp
    ├── PrinterCardTests.cpp          # Ring semantics, register contract, ordering
    ├── FirmwareParityTests.cpp       # Assemble .a65 source, compare to embedded bytes
    ├── TitleRecognizerTests.cpp
    ├── PrintJobSerializerTests.cpp
    ├── PrinterPresenterTests.cpp     # Pure pacing/coalescing/fast-forward math, injected clock
    ├── PrinterViewportTests.cpp      # NEW: follow/scroll/snap math, injected clock (FR-033)
    └── PrinterAudioSourceTests.cpp   # Event-voice scheduling from the presenter clock (synthetic PCM)
```

**Structure Decision**: Follow the existing device pattern — guest-facing
hardware and all pure logic in `CassoEmuCore/Devices/Printer/` (unit-testable,
no system dependencies), host integration (files, clipboard, printers,
dialogs, chrome) in the `Casso` shell. This is the same split
`Disk2Controller` / `DiskManager` / `DriveWidget` already use, and it is what
lets FR-017 and the constitution's test-isolation rule hold without friction.
The preview presentation keeps that split: printed *content* (strip ink, paper
furniture, head-column reveal) renders to a 2D texture in pure code; the
*presentation* (3D chassis + curled paper, or a flat fallback) only maps that
texture — so the 2D-vs-3D choice is isolated to the final stage (research
R-017). Preview redesign phases (tasks Phase 11): A = viewport + incremental
render (the perf fix, presentation-agnostic) → B = paper furniture → C =
head-timing ink reveal → D = 3D scene. A–C self-verify (testable math +
capturable panel); D needs the user's eye for aesthetic tuning.
