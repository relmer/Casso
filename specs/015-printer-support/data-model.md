# Data Model: Emulated Printer Support (ImageWriter II)

**Feature**: 015-printer-support | **Date**: 2026-07-07 (rev. 2026-07-09: viewport + head-position stream)

## Guest-side (CassoEmuCore)

### PrinterCard (`MemoryDevice`)

| Field | Type | Notes |
|---|---|---|
| m_slot | int | 1..7; I/O window `$C080 + slot*$10` |
| m_ring | PrinterByteRing | fixed-capacity SPSC ring; emu thread produces |
| m_everTouched | bool | first firmware entry or data write → panel reveal event (FR-020) |

Behavior: `Write($C0n0)` pushes byte (O(1)). `Read($C0n1)` returns status with
ready asserted while the ring has headroom, **de-asserted within a high-water
margin of capacity** so a handshake-honoring guest waits instead of overflowing
if the drain stalls (e.g. a modal print dialog holds the UI thread) — this is
what upholds FR-002 (see research R-001). The ring is fixed-capacity (≥ 64 KB)
and normally drained near-empty each presenter tick, so ready stays
continuously asserted in practice; overflow past the guard is a programming
error asserted in debug. Reset/PowerCycle clear nothing on the paper (strip
survives guest reset per FR-026 semantics).

### ImageWriterInterpreter (pure)

State (resets on printer-reset command and machine start, FR-010):

| Field | Type | Default |
|---|---|---|
| pitchDotsPerLine | int | 80-col pitch equivalent (TRM default) |
| lineFeedRows | int | 12 (6 lpi at 144 rows/inch) |
| color | InkPrimary | Black |
| headColumnDots | int | 0 |
| paperRow | int | 0 (monotonic; strip-relative) |
| escState | enum | parser state machine (Idle / Esc / EscParam(n) / GraphicsRun(count)) |

Input: bytes. Output: calls into `PrintRaster` (strike cells) and an ordered
`PrinterEvent` stream (for presentation): `HeadBurst{fromDot,toDot,row}`,
`LineFeed{rows}`, `FormFeed`, `ColorChange{c}`, `ResetSeen`,
`UnknownCommand{bytes}`.

### PrintRaster (the strip)

| Field | Type | Notes |
|---|---|---|
| cells | vector<Byte> | 4-bit ink bitfield per cell, 1280 cells/row, chunk-grown |
| rowsUsed | int | high-water mark of struck/advanced rows |
| pageBoundaryRows | vector<int> | FF positions + implicit 11" boundaries for pagination |
| capReached | bool | 60 form lengths (FR-015) |

Lifecycle (state transitions): `Empty → Printing (first strike) → Pending
(guest idle, un-ejected) → Empty (eject delivers whole strip / discard)`.
Cancelled delivery: `Pending` unchanged. Guest reset: no transition.

### PrinterViewport (pure, clock-injected — FR-033)

| Field | Type | Notes |
|---|---|---|
| viewportRows | int | ~1 page of native rows (11" × 144); user-adjustable |
| followLive | bool | true → top tracks the newest printed row |
| scrollOffsetRows | int | user scrollback offset from the live row (0 when following) |
| lastScrollMs | int64 | injected-clock time of last user scroll input |
| snapDelayMs | const | ~2000; idle beyond this while scrolled → snap back to live |

Inputs: `Advance(liveRow)`, `Scroll(deltaRows, nowMs)` (wheel/touch/arrows),
`Tick(nowMs)`. Output: visible native-row span `{firstRow, lastRow}` for the
incremental renderer. Pure math, no UI deps — unit-tested like
`PrinterPacing` / `PrinterStatusModel`.

### Head-position stream (FR-034)

The interpreter's existing `HeadBurst{fromDot,toDot,row}` events gain paced
per-column playback: `PrinterPacing` maps a burst + clock to the head's
current column, so the panel reveals ink left→right *within* a line. The
worker exposes the newest `{row, column}` thread-safely (like
`ActivityCount`/`RowsUsed`) without touching the raster, which remains
complete immediately.

### TitleRecognizer (pure)

`Signature { id, displayName, filenameSubstrings[], catalogNames[],
metaTitles[] }` — embedded constant table. Match order META → filename →
catalog (R-013); result `{ matched, displayName }` drives one FR-024 notice.

### PrinterAudioSource (`IDriveAudioSource`)

Consumes presenter-scheduled events; per-event sample voices (head-burst
loop gated by burst spans, one-shots for LF/FF/tear). Mono float pipeline
identical to `Disk2AudioSource`.

## Host-side (Casso shell)

### PrinterPresenter

| Field | Type | Notes |
|---|---|---|
| eventQueue | deque<PrinterEvent> | drained at paced rate (R-012) |
| presentedRow | int | paper animation position ≤ raster rowsUsed |
| pacing | consts | ~250 cps head, LF ms/row; fast-forward on eject/discard |

### PrintJobStore (persistence, FR-026)

Sidecar schema (versioned JSON): `{ formatVersion, paperRow, rowsUsed,
pageBoundaryRows[], capReached }` + `strip.png` (native-grid indexed PNG).
Load at machine open; save on clean exit, eject, discard.

### HostPrintServices

| Sink | Input | Output |
|---|---|---|
| PngFileSink | rendered RGBA + dpi | timestamped file in configured folder (FR-012) |
| WindowsPrintSink | per-page rendered RGBA | PrintDlgEx/GDI (or PrintManager per R-009 spike), true scale (FR-014) |
| ClipboardCopy | rendered RGBA + dpi | "PNG" format + delayed CF_DIB (FR-013) |

### PrintingSettings (`GlobalUserPrefs` additions)

| Field | Type | Default |
|---|---|---|
| printDestination | enum {PngFile, WindowsPrinter} | PngFile |
| printPngFolder | path string | `<Pictures>/Casso Prints` (created on first use) |
| printOutputDpi | int {288, 576} | 576 |
| printDotStyle | enum {Ink, Plain} | Ink |
| printerAudioVolume / Mute | existing drive-audio pattern | on, moderate |

### Machine config (per machine)

Slot entry `{ slot: 1, device: "parallel-printer", capability: optional,
enabled: true }` — default in embedded machine JSONs; added by
`MachineConfigUpgrade` when absent and slot 1 is free.

### Widget state

Indicator: `{ visible, activity (idle/receiving/pending/error), tooltip
config summary }`. Panel: `{ shown, presentedRow, viewport (PrinterViewport),
perforations from pageBoundaryRows, controls: FormFeed(eject), Copy,
Discard(confirm), scroll hint }`.

### Panel content texture + 3D scene (FR-032)

The panel renders printed content — strip ink, tractor-feed sprocket
strips/holes, perforations, head-column reveal — into a persistent 2D tile
buffer covering the viewport span (new rows only; never a whole-strip
re-render). The presentation maps that texture onto a curled-paper mesh in
front of a procedurally-built, bottom-anchored printer chassis under a
perspective camera (Dxui D3D11 additive path — research R-017); a flat blit of
the same texture is the fallback. Paper furniture stays panel-only (FR-027).

## Relationships

```text
guest CPU ──write──▶ PrinterCard ──ring──▶ ImageWriterInterpreter ──strikes──▶ PrintRaster
                                          │ events                                │
                                          ▼                                       │ (complete immediately)
                                   PrinterPresenter ──paced──▶ PrinterPanel/paper │
                                          │                └──▶ PrinterAudioSource│
                                          ▼                                       ▼
                              eject/copy/discard ──────▶ PaperRenderer ──RGBA──▶ HostPrintServices
                                                                       └────────▶ PrintJobStore (persist native grid)
```

**Ring flush before delivery**: eject, copy, and discard first force a
synchronous `ring → interpreter → raster` drain, so no in-flight byte is left
unrendered; only then do they render/deliver/clear the complete strip. The
presentation may still be mid-replay (R-012) — it fast-forwards — but the
raster operated on is always complete.
