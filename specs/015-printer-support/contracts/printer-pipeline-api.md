# Contract: Printer Pipeline — Internal C++ Interfaces

**Consumers**: Casso shell, UnitTest. Signatures indicative (final naming per
code review); EHM + house style apply throughout.

## CassoEmuCore/Devices/Printer

```cpp
// Pure byte-stream interpreter. No system dependencies (FR-017).
class ImageWriterInterpreter
{
public:
    void    Reset          ();                       // FR-010 power-on defaults
    void    Consume        (const Byte * data,
                            size_t       count,
                            PrintRaster & raster,
                            vector<PrinterEvent> & outEvents);
};

// Native-grid strip (160 dpi x 144 rows/inch), 4-bit ink bitfield cells.
class PrintRaster
{
public:
    void    Strike         (int columnDot, int row, InkPrimary ink);
    void    AdvanceRows    (int rows);
    void    MarkFormFeed   ();
    int     RowsUsed       () const;
    const vector<int> & PageBoundaryRows () const;
    bool    CapReached     () const;                 // 60 form lengths
    void    Clear          ();                       // eject / discard
};

// Deterministic ink renderer (FR-027/FR-028). Pure: RGBA out.
class PaperRenderer
{
public:
    struct Options { int outputDpi; DotStyle style; };   // 288|576, Ink|Plain
    HRESULT Render         (const PrintRaster & raster,
                            int                 firstRow,
                            int                 lastRow,
                            const Options     & options,
                            RgbaImage         & outImage);  // pHYs dpi set by caller
};

// Strip + interpreter-visible job state <-> memory buffers (FR-026). Pure.
class PrintJobSerializer
{
public:
    static HRESULT Save    (const PrintRaster & raster, const StripMeta & meta,
                            vector<Byte> & outPng, string & outSidecarJson);
    static HRESULT Load    (const vector<Byte> & png, const string & sidecarJson,
                            PrintRaster & outRaster, StripMeta & outMeta);
};

// Pure recognition (FR-022/023/025). All inputs supplied by caller.
class TitleRecognizer
{
public:
    static bool Match      (const string & imageBasename,
                            const vector<pair<string,string>> & wozMeta,
                            const vector<string> & catalogNames,
                            RecognizedTitle & outTitle);
};

// Live-preview viewport (FR-033). Pure clock-injected state: follows the
// newest printed row, absorbs user scrollback, snaps back to live after
// ~2 s idle. Yields the native-row span the panel renders incrementally.
class PrinterViewport
{
public:
    struct Span { int firstRow; int lastRow; };
    void    Advance        (int liveRow);                    // newest printed row
    void    Scroll         (int deltaRows, int64_t nowMs);   // wheel / touch / arrows
    void    Tick           (int64_t nowMs);                  // snap-back timing
    Span    VisibleSpan    (int rowsUsed) const;
    bool    FollowingLive  () const;
};
```

## Casso shell seams

```cpp
// Destination sinks (FR-011..FR-015). One rendered strip/page set in;
// side effects out. Each sink individually replaceable (R-009 spike).
class HostPrintServices
{
public:
    HRESULT DeliverToPngFile     (const RgbaImage & strip, const fs::path & folder, fs::path & outFile);
    HRESULT DeliverToWindowsPrinter (const PrintRaster & raster, const PaperRenderer::Options & opt,
                                     HWND owner, int pageCount);   // dialog once per job
    HRESULT CopyToClipboard      (const RgbaImage & strip, int dpi); // "PNG" + delayed CF_DIB
};

// Presentation pacing (FR-031): consumes PrinterEvents, drives panel + audio.
// FR-034: also exposes the paced head position within the current line so the
// panel reveals ink per column, left->right, as the head sweeps.
class PrinterPresenter
{
public:
    void    OnEvents       (span<const PrinterEvent> events);
    void    FastForward    ();                       // eject/discard path
    int     PresentedRow   () const;                 // panel paper position
    int     PresentedHeadColumn () const;            // paced column within the live row (FR-034)
};

// Drain worker thread-safe signals sampled by the UI thread (existing:
// ActivityCount/HasContent/RowsUsed/SnapshotStrip; FR-034 adds head position).
class PrinterWorker
{
public:
    uint64_t ActivityCount () const;   // monotonic, per drained byte
    bool     HasContent    () const;
    int      RowsUsed      () const;   // strip height, no raster copy
    bool     SnapshotStrip (PrintRaster & out);   // consistent copy under raster lock
    // FR-034: newest {row, column} the interpreter struck, published atomically.
};
```

## Threading contract

- `PrinterCard::Write` runs on the emulation thread: O(1) ring push only.
- Ring drain, `Consume`, raster strikes: dedicated printer worker thread
  (`PrinterWorker`), mutating the raster only under its raster lock — raster
  is complete as soon as the ring drains; eject/copy/discard stop the worker
  and force a full synchronous drain first.
- UI thread reads: atomic signals (`ActivityCount`/`HasContent`/`RowsUsed`,
  FR-034 head position) any time; `SnapshotStrip` copies the raster under the
  lock without stopping the drain — the live preview never perturbs job state.
- `PaperRenderer::Render` and sink delivery: at eject on the UI thread today
  (whole-strip renders dpi-capped per R-018); may move to a worker with
  progress UI for very long strips.
