# Data Model: Emulated Printer Support (ImageWriter II)

**Feature**: 015-printer-support | **Date**: 2026-07-07

## Guest-side (CassoEmuCore)

### PrinterCard (`MemoryDevice`)

| Field | Type | Notes |
|---|---|---|
| m_slot | int | 1..7; I/O window `$C080 + slot*$10` |
| m_ring | PrinterByteRing | fixed-capacity SPSC ring; emu thread produces |
| m_everTouched | bool | first firmware entry or data write → panel reveal event (FR-020) |

Behavior: `Write($C0n0)` pushes byte (O(1), never blocks — ring sized so the
consumer always keeps up; overflow is a programming error asserted in debug).
`Read($C0n1)` returns status with ready asserted. Reset/PowerCycle clear
nothing on the paper (strip survives guest reset per FR-026 semantics).

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
config summary }`. Panel: `{ shown, pinned, presentedRow, perforations from
pageBoundaryRows, controls: FormFeed(eject), Copy, Discard(confirm) }`.

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
