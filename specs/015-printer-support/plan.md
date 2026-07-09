# Implementation Plan: Emulated Printer Support (ImageWriter II)

**Branch**: `015-printer-support` | **Date**: 2026-07-07 (rev. 2026-07-09: preview presentation addendum) | **Spec**: [spec.md](spec.md)

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
on-demand action. A compact chrome indicator plus an on-demand docked panel
(skeuomorphic printer with animated, perforated paper and sampled audio, paced
at realistic print speed) provide discoverability, preview-before-print, and
the eject/copy/discard controls. Print-title recognition (filename → WOZ META →
filesystem catalog) surfaces a setup-guidance notice at mount time.

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145 / VS 2026), Win32 desktop

**Primary Dependencies**: Windows SDK only — GDI printing, WIC (PNG
encode/decode), existing WASAPI + `DriveAudioMixer` audio stack, existing
D3D11/D2D dxui chrome (spec 013), existing `JsonParser`/`JsonWriter`. No new
third-party dependencies (constitution allowlist unchanged). The in-repo
assembler (CassoCore) builds the slot firmware.

**Storage**: Global prefs in `GlobalUserPrefs` JSON (new printing fields);
per-machine pending-strip persistence as PNG + versioned JSON sidecar under
the per-machine user-state directory; audio samples via the existing
consent-gated `AssetBootstrap` downloader.

**Testing**: Microsoft CppUnitTest (`UnitTest/` project). Interpreter,
raster, paper renderer, recognizer, serializer, and firmware are pure /
data-driven per FR-017 and constitution Test Isolation — no file, registry,
network, or clipboard access in unit tests; golden verification via checked-in
hash constants and small embedded expected tiles.

**Target Platform**: Windows 10/11, x64 and ARM64

**Project Type**: Existing multi-project desktop application (CassoEmuCore
static lib + Casso Win32 shell + UnitTest)

**Performance Goals**: FR-018 — zero measurable emulation impact: the card's
guest-facing `Write` is an O(1) ring-buffer push on the emulation thread
(pattern: `InputEventRing`). Interpretation/rasterization drain off the
emulation thread. Eject-time render of a typical 1-3 page job at 576 dpi
completes in ≤ ~2 s; long strips scale linearly (FR-028 allows it).

**Constraints**: Deterministic interpreter and renderer (FR-009, FR-027);
strip cap 60 fanfold pages (FR-015); native grid 1280 dots × 144 rows/inch;
firmware must be original work (FR-003); no new third-party code.

**Scale/Scope**: One new CassoEmuCore device family (`Devices/Printer/`), one
audio source, one shell services module, one settings page, one chrome
indicator + docked panel, machine-config upgrade step, ~9 new unit-test
suites.

## Constitution Check

*GATE: evaluated against constitution v1.7.0 — PASS (pre-Phase-0 and
re-checked post-Phase-1). No violations to justify; Complexity Tracking
omitted.*

- **I. Code Quality**: All new code follows EHM, declaration alignment,
  5-blank-line separation, `s_k` statics, no anonymous namespaces. The
  interpreter's command dispatch uses EHM bail-outs / guard-continue loops to
  stay flat; functions ≤ 50 lines (dispatch split per command family).
- **II. Testing Discipline**: FR-017 mirrors the constitution: interpreter,
  raster, renderer, recognizer, and persistence serialization are pure
  functions over supplied bytes/buffers. File/clipboard/printer/dialog I/O
  lives in thin shell adapters excluded from unit scope. Golden rasters are
  hash-verified (no disk reads). Firmware parity test assembles the checked-in
  `.a65` source with the in-repo assembler and compares to the embedded bytes.
- **III. UX Consistency**: Printing settings join `SettingsSheet` as a new
  page (Disk-tab visibility precedent); eject/copy/discard also exposed as
  `WindowCommandManager` commands; existing notice/toast and themed-dialog
  patterns reused. Backward compatibility: config upgrade is additive and the
  card is per-machine disable-able; with the card disabled, slot 1 behavior is
  bit-identical to today.
- **IV. Performance**: Emulation-thread work is O(1) per guest write; no
  allocation on the hot path (fixed ring). Rendering/delivery run at eject
  time off-thread.
- **V. Simplicity**: One printer, one card, one command set (SSC/6551
  explicitly deferred to the //c effort). No speculative abstraction beyond
  the `IPrinterByteSink` seam the //c serial port will eventually reuse.
- **Tech Constraints**: No allowlist amendment needed — WIC/GDI are Windows
  SDK; audio samples are downloaded assets (existing consent flow), not
  vendored code.

## Project Structure

### Documentation (this feature)

```text
specs/015-printer-support/
├── plan.md              # This file
├── research.md          # Phase 0 output
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
│   ├── ImageWriterInterpreter.h/.cpp # 8510+Apple command subset → dot/motion events (pure)
│   ├── PrintRaster.h/.cpp            # Native-grid strip: 4-primary bitfield cells, page boundaries, 60-page cap
│   ├── PaperRenderer.h/.cpp          # Deterministic CPU ink renderer (discs, overprint mix, weave; square style)
│   ├── PrintJobSerializer.h/.cpp     # Strip+state ⇄ memory buffers (pure); shell owns file I/O
│   ├── TitleRecognizer.h/.cpp        # Pure filename / catalog-name / META-title matching
│   └── ParallelFirmware.h            # Generated: firmware bytes + embedded .a65 source text
├── Devices/Printer/ParallelFirmware.a65   # Original slot firmware source (assembled in FirmwareParityTests by the in-repo CassoCore assembler — no build step, per R-002)
├── Devices/Disk/WozLoader.h/.cpp     # MODIFIED: retain META chunk key/values
└── Audio/PrinterAudioSource.h/.cpp   # IDriveAudioSource; event-driven sample playback

Casso/
├── Print/
│   ├── HostPrintServices.h/.cpp      # PNG writer (WIC), Windows print (GDI), clipboard ("PNG" + delayed DIB)
│   ├── PrintJobStore.h/.cpp          # Per-machine pending-strip persistence (file I/O around serializer)
│   └── PrinterPresenter.h/.cpp       # Presentation pacing, event fan-out to panel + audio
├── Ui/Chrome/PrinterIndicator.h/.cpp # Corner indicator (state LEDs, tooltip, click → panel)
├── Ui/PrinterPanel.h/.cpp            # Docked side panel: skeuomorphic printer, perforated paper, controls
├── Ui/Settings/PrintingPage.h/.cpp   # Settings → Printing tab
├── Config/GlobalUserPrefs.h/.cpp     # MODIFIED: printing fields
├── AssetBootstrap.h/.cpp             # MODIFIED: printer audio sample set row
├── Shell/WindowCommandManager.cpp    # MODIFIED: eject / copy / discard / show-panel commands
└── Machines/*.json (embedded)        # MODIFIED: slot 1 parallel-printer default

CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp  # MODIFIED: add printer card on upgrade

UnitTest/
└── PrinterTests/
    ├── ImageWriterInterpreterTests.cpp
    ├── PrintRasterTests.cpp
    ├── PaperRendererGoldenTests.cpp
    ├── PrinterCardTests.cpp          # Ring semantics, register contract, ordering
    ├── FirmwareParityTests.cpp       # Assemble .a65 source, compare to embedded bytes
    ├── TitleRecognizerTests.cpp
    ├── PrintJobSerializerTests.cpp
    ├── PrinterPresenterTests.cpp     # Pure pacing/coalescing/fast-forward math, injected clock (deterministic)
    └── PrinterAudioSourceTests.cpp   # Event-voice scheduling from the presenter clock (synthetic PCM)
```

**Structure Decision**: Follow the existing device pattern — guest-facing
hardware and all pure logic in `CassoEmuCore/Devices/Printer/` (unit-testable,
no system dependencies), host integration (files, clipboard, printers,
dialogs, chrome) in the `Casso` shell. This is the same split
`Disk2Controller` / `DiskManager` / `DriveWidget` already use, and it is what
lets FR-017 and the constitution's test-isolation rule hold without friction.

## Preview Presentation Architecture (2026-07-09 addendum)

Covers FR-032/FR-033/FR-034 and SC-010/SC-011 — the panel's live-print
presentation, decided after real Print Shop banner testing exposed that the
first-cut "render the whole strip every ~100 ms" preview was O(rows²) in time
and unbounded in memory. The redesign is staged so the perf-critical, testable
work lands first and the aesthetic 3D scene last.

**Core principle — decouple content from presentation.** The printed *content*
(strip ink + paper furniture + the head-column ink reveal) renders to a **2D
texture** in plain, unit-testable code; the *presentation* layer only maps that
texture onto the paper surface and draws the printer. So the 2D-vs-3D decision
is isolated to the final stage, everything upstream stays testable, and a flat
fallback is always available (FR-032).

**Rendering approach (real 3D, contained).** Dxui's painter is already a custom
**D3D11** pipeline with its own HLSL vertex/pixel shaders, vertex buffers, and
blend state (NOT Direct2D); `DxuiTextRenderer::DrawIconBitmap` already samples
textures. So a 3D scene is an *additive* path, not a new engine: add one MVP
constant buffer, one textured+lit shader, and two meshes (a static chassis and
a dynamically-curled paper strip whose UVs map the content texture). The
printer is anchored at the panel bottom; paper rises from the platen and curls
out of view above a ~1-page viewport (FR-032). This is the pilot 3D primitive
the drive widgets adopt when they later move from flat/faux-3D to true 3D.

**Viewport + scroll (FR-033), pure/testable.** A `PrinterViewport` value type
owns the scroll/follow state machine: follow the newest row while printing;
wheel/touch/Up-Down scroll offsets earlier pages into view; after ~2 s idle,
snap back to the live row; expose the visible native-row span. Clock-injected,
no UI deps — unit-tested like `PrinterPacing`/`PrinterStatusModel`. The panel
renders only newly-produced rows into a persistent tile buffer (incremental),
so per-frame cost is flat regardless of strip length (SC-010).

**Head-timing ink reveal (FR-034).** The interpreter/worker exposes the print
head's column position + timestamp as it lays dots (without touching the
already-complete raster); `PrinterPacing` drives the left→right per-column
reveal within the current line. Extends the existing pacing model rather than
replacing it.

**Delivery memory bound (FR-028) — SHIPPED.** `WholeStripDpi()` caps the
whole-strip PNG/clipboard render dpi against a ~512 MB RGBA budget (drops toward
but not below native); `CopyPrintoutToClipboard` renders once and encodes the
PNG from that image (was a double render). Commit `de6ee886`.

**Also SHIPPED this pass (interim, superseded where noted):** non-destructive
live snapshot + activity-resume auto-open + strip-scaled refresh throttle
(the throttle is the stopgap the FR-033 incremental viewport replaces);
blank-sheet empty preview; message boxes owned by / centered on the panel
(`PrinterDialogOwner` + `DxuiMessageBox` owner-centering); Escape-to-close;
Windows-printer delivery tracing to diagnose the PDF "failed to deliver"
(commit `f7b5cf92`). Commits `de6ee886`, `f7b5cf92`.

**New/changed components for the redesign:**

```text
CassoEmuCore/Devices/Printer/
├── PrinterViewport.h/.cpp        # NEW: pure scroll/follow/snap state (clock-injected)
├── PrinterPacing.*               # EXTEND: per-column head reveal timing
└── ImageWriterInterpreter/PrinterJob  # EXTEND: expose head-column + timestamp stream

Casso/
├── Ui/PrinterPanel.*             # REWORK: viewport-driven incremental render (no whole-strip re-render)
├── Ui/PrinterPaperView.*         # REWORK/REPLACE: persistent tile buffer; feeds the 3D layer
├── Ui/Printer3DScene.h/.cpp      # NEW: chassis + curled-paper meshes, camera, textured/lit shader
└── Print/PrinterWorker.*         # DONE: RowsUsed() atomic; EXTEND: head-column signal

Dxui/Render/                      # EXTEND: MVP cbuffer + textured/lit shader path (or a scoped 3D helper)

UnitTest/PrinterTests/
└── PrinterViewportTests.cpp      # NEW: follow/scroll/snap math, injected clock
```

**Phasing:** (A) `PrinterViewport` + incremental render into the content
texture, 1-page follow/scroll/snap, on-screen hint — the real perf fix,
presentation-agnostic. (B) tractor-feed sprocket strips/holes + perforations in
the content texture. (C) head-timing per-column ink reveal (worker signal +
pacing). (D) the 3D scene — chassis + curled paper mapping the content texture.
A–C are self-verifiable (testable math + capturable panel); D needs the user's
eye for final aesthetic tuning.
