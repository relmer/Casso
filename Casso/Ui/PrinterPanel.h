#pragma once

#include "Window/DxuiWindow.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiTooltip.h"
#include "Core/DxuiPanZoom.h"

#include "Devices/Printer/PrinterPacing.h"
#include "Devices/Printer/PrinterViewport.h"
#include "Devices/Printer/RgbaImage.h"
#include "PrinterPaperView.h"


struct CassoTheme;
class  PrintRaster;
class  PrinterWorker;
class  Printer3DScene;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel
//
//  A DxuiWindow (like the Disk II / Input debug panels) that shows the emulated
//  printer's output and doubles as print preview (FR-020): a live ~1-page
//  viewport (FR-033) follows the print head as printing proceeds -- earlier
//  pages scroll out of view as if folded into the tray -- with a toolbar to
//  finish (deliver to the configured destination), copy, discard, or refresh.
//
//  The viewport is the pure PrinterViewport state machine; per frame the shell
//  calls RefreshLive, which snapshots ONLY the visible row span from the drain
//  worker (never the whole strip) and re-renders it -- so preview memory and
//  per-frame cost stay flat no matter how long the banner grows (SC-010).
//  Wheel / touch / Up-Down scroll earlier pages back into view; ~2 s idle
//  snaps back to the currently printing row. Toolbar actions fan back out
//  through the ActionFn callbacks, which the shell routes to the existing
//  delivery commands.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterPanel : public DxuiWindow
{
public:
    using ActionFn = std::function<void ()>;

    PrinterPanel  ();
    ~PrinterPanel () override;

    HRESULT  Create (HINSTANCE              hInstance,
                     HWND                   hwndOwner,
                     ID3D11Device         * device,
                     ID3D11DeviceContext  * context,
                     const CassoTheme     * theme);

    bool     IsOpen () const { return IsCreated (); }
    HRESULT  RenderFrame ();
    void     SetTheme (const CassoTheme * theme);

    // True while the panel needs a continuous animation cadence: the carriage
    // is still sweeping toward the guest's head position, or a pan/zoom is
    // easing. The shell forces a present each frame while true so the motion
    // runs at the present rate instead of stepping on the idle loop's coarse
    // sleep tick (which a print with a static guest screen would otherwise hit).
    bool     NeedsAnimationFrame () const { return m_sweeping || m_panZoomEasing || m_printingActive; }

    // Paced carriage position for the printer audio (Option A / FR-034): a
    // monotonic revealed-dot counter, the within-line sweep column (both in
    // native dots), and whether the paced head is currently laying ink.
    // `inkActive` is false while the paper merely feeds (form feed / blank line
    // feed) or the head traverses a wide blank margin, so the audio source can
    // keep the carriage BUZZ off during non-print motion. This reads the SAME
    // paced reveal the panel renders, so the mechanical sound follows what is
    // seen, not the raw guest byte stream. Valid after RefreshLive.
    void     GetPacedReveal (int64_t & progressDots, int & colDots, bool & inkActive) const
    {
        int  rows = m_pacing.RevealedRows ();
        colDots      = m_pacing.RevealedColDots ();
        progressDots = (int64_t) rows * PrinterGrid::kDotsPerRow + colDots;
        inkActive    = m_revealInk;
    }

    // Per-frame live update (FR-033): advance the viewport to the worker's
    // newest row, tick the snap-back clock, and re-render the visible span
    // when something changed (new bytes, viewport motion, or `force`). Cost
    // is bounded by the viewport, never the strip length.
    void     RefreshLive (PrinterWorker & worker, int64_t nowMs, bool force = false);

    // Direct raster push for paths with no live worker (e.g. no printer card:
    // an empty raster shows the blank sheet). Renders the given raster's
    // visible span through the same viewport.
    void     SetStrip (const PrintRaster & raster);

    void     SetOnPrint    (ActionFn fn) { m_onPrint    = std::move (fn); }
    void     SetOnSaveAs   (ActionFn fn) { m_onSaveAs   = std::move (fn); }
    void     SetOnCopy     (ActionFn fn) { m_onCopy     = std::move (fn); }
    void     SetOnDiscard  (ActionFn fn) { m_onDiscard  = std::move (fn); }
    void     SetOnFormFeed (ActionFn fn) { m_onFormFeed = std::move (fn); }

    void     Layout (const RECT          & boundsDip,
                     const DxuiDpiScaler & scaler) override;

    void     Paint  (IDxuiPainter        & painter,
                     IDxuiTextRenderer   & text,
                     const IDxuiTheme    & theme) override;

    // Wheel scrolls the viewport back through earlier pages (FR-033); all
    // other mouse input flows to the toolbar buttons as before.
    bool     OnMouse (const DxuiMouseEvent & ev) override;

    // Up/Down/PageUp/PageDown scroll the viewport; Escape hides the preview.
    bool     OnKey   (const DxuiKeyEvent   & ev) override;

protected:
    void     OnCreate () override;

private:
    void     ShowBlankSheet ();
    void     UpdateTooltip  (int x, int y);

    // Push the pan/zoom transform to the 3D scene and refresh the zoom chrome
    // (per-frame; the pan/zoom state itself lives in m_panZoom). Also refreshes
    // the zoom-dependent horizontal pan bounds and the drag scale.
    void     SyncTransform  ();

    // True while the mouse is dragging the paper (pan), so Move/Up events route
    // to m_panZoom instead of the toolbar. Seeded by a left-press on the paper.
    bool     PaperHit       (int x, int y) const;

    static DxuiPanZoom::Config  PanZoomConfig ();

    void     RenderSpan     (const PrintRaster & spanRaster, int firstAbsRow, int lastAbsRow,
                             bool contentDirty, int revealBandTopAbs, int revealLoDots, int revealHiDots);
    void     ComposeCanvas  (const RgbaImage * content, int contentFirstAbsRow, int bottomAbsRow,
                             int revealBandTopAbs, int revealLoDots, int revealHiDots);

    // True when the pin band starting at absolute row `revealRow` carries ink in
    // columns [sampleLoCol, sampleHiCol], sampled from the span raster (whose row
    // 0 == absolute `spanFirstRow`). Drives m_revealInk / the audio buzz gate;
    // the caller picks the range (a short window behind the head in the sweep
    // direction while printing, or the whole row while feeding).
    bool     RevealBandHasInk (const PrintRaster & spanRaster, int spanFirstRow,
                               int revealRow, int sampleLoCol, int sampleHiCol) const;

    static int64_t  NowMs ();

    // Fanfold-paper furniture helpers (panel-only): the hole / perforation
    // phase modulus, the perforation-dash pixel darken, and the embedded
    // CAD-model resource loader.
    static int          FloorMod         (int a, int m);
    static void         DarkenPerf       (uint32_t & px);
    static std::string  LoadTextResource (int resourceId);

    const CassoTheme  * m_theme   = nullptr;

    PrinterPaperView  * m_paper    = nullptr;

    // Top toolbar: document actions (Print / Save / Copy -- all
    // non-destructive) on the left, the zoom cluster on the right.
    DxuiButton        * m_print    = nullptr;
    DxuiButton        * m_saveAs   = nullptr;
    DxuiButton        * m_copy     = nullptr;
    DxuiButton        * m_zoomOut  = nullptr;
    DxuiButton        * m_zoomReset = nullptr;
    DxuiButton        * m_zoomIn   = nullptr;

    // Bottom row: paper handling (Form Feed advances a page; Discard is the
    // one destructive tear-off).
    DxuiButton        * m_formFeed = nullptr;
    DxuiButton        * m_discard  = nullptr;

    ActionFn            m_onPrint;
    ActionFn            m_onSaveAs;
    ActionFn            m_onCopy;
    ActionFn            m_onDiscard;
    ActionFn            m_onFormFeed;

    // The reusable pan + zoom controller (FR-027 zoom is preview-only chrome).
    // Owns the eased vertical scroll position (panY, in native rows), the
    // horizontal pan (panX, in content px), the zoom factor, and all of the
    // input gestures: wheel / drag / Ctrl+wheel / Ctrl +/-/0 / touchpad pinch +
    // two-finger pan. m_viewport is the follow-mode policy layered on its panY.
    DxuiPanZoom         m_panZoom;

    // Last zoom target pushed to the button chrome, so the label / enabled
    // states only refresh when the zoom actually changes (not every frame).
    float               m_zoomChromeSynced = -1.0f;

    // Hover tooltips for the toolbar (disabled buttons explain WHY they are
    // disabled), plus the guest-activity clock that drives the Form Feed
    // button's enabled state: feeding mid-print would interleave a page
    // break into the guest's stream, so it only arms once the print idles.
    DxuiTooltip         m_tooltip;
    int64_t             m_lastActivityChangeMs = 0;

    // Live-viewport state (FR-033). m_renderedSpan/-Activity detect "something
    // changed" so an idle panel does zero render work per frame.
    PrinterViewport         m_viewport;
    PrinterViewport::Span   m_renderedSpan     = {};
    uint64_t                m_renderedActivity = 0;
    int64_t                 m_lastRenderMs     = 0;
    bool                    m_hasRendered      = false;

    // Adopt the worker's activity count on the first refresh so opening over a
    // restored strip doesn't read the initial sync as a fresh receive (which
    // would flash the status LEDs bright before settling to their idle glow).
    bool                    m_activityPrimed   = false;

    // Animation-cadence signals read by NeedsAnimationFrame: the carriage is
    // still chasing the head (set in RefreshLive), the pan/zoom is easing (set in
    // RenderFrame from the controller's Tick), or the printer is actively
    // receiving. The last one holds the smooth present cadence across line
    // boundaries -- where the reveal momentarily catches the head and m_sweeping
    // would otherwise flip false for a frame, dropping the loop onto the coarse
    // idle sleep tick and jerking the carriage once per line.
    bool                    m_sweeping         = false;
    bool                    m_panZoomEasing    = false;
    bool                    m_printingActive   = false;

    // panY seeding: on the first content frame (and after a tear-off) snap
    // m_panZoom's eased position onto the target instead of gliding, so
    // opening the panel over a restored strip doesn't scroll across it.
    bool                    m_panYSeeded       = false;

    // Head-timing ink reveal (FR-034): pacing chases the worker's head
    // position at ImageWriter speed; the rendered-reveal pair detects sweep
    // motion so the panel keeps animating between byte arrivals. Primed
    // caught-up on first refresh so a restored strip never replays history.
    PrinterPacing           m_pacing;
    bool                    m_pacingPrimed     = false;
    int                     m_renderedRevealRow = -1;
    int                     m_renderedRevealCol = -1;

    // Ink-under-head flag for the printer audio (see GetPacedReveal): true when
    // the pin band at the paced reveal carries ink just behind the sweep column.
    // Recomputed each render from the span raster; false on a blank sheet.
    bool                    m_revealInk        = false;

    // Rendered-span cache: scrolling shifts the visible window over UNCHANGED
    // strip content, so the expensive ink render is reused row-for-row and
    // only newly exposed rows are rendered. Invalidated whenever new bytes
    // land (contentDirty) or the span geometry stops lining up. The
    // generation counter bumps on full re-renders so the canvas cache below
    // knows when content pixels changed under it.
    RgbaImage               m_spanImg;
    int                     m_spanImgFirstAbsRow = 0;
    bool                    m_spanImgValid       = false;
    uint64_t                m_spanImgGen         = 0;

    // Composed-canvas cache: scroll steps memmove the finished canvas and
    // rebuild only the newly exposed rows; reveal-sweep frames rebuild only
    // the live pin band. Anything else falls back to a full compose.
    std::vector<uint32_t>   m_canvas;
    int                     m_canvasTopAbs    = 0;
    int                     m_canvasRevealTop = -2;
    int                     m_canvasRevealLo  = -1;
    int                     m_canvasRevealHi  = -1;
    uint64_t                m_canvasSpanGen   = 0;
    bool                    m_canvasHasContent = false;
    bool                    m_canvasValid      = false;

    // 3D presentation (FR-032): the ImageWriter + curled-paper scene, drawn
    // from the window's before-present hook into m_paperRectPx (which the
    // panel's Paint therefore does NOT fill). Null = flat PrinterPaperView
    // fallback. unique_ptr so this header stays free of the scene's D3D types.
    std::unique_ptr<Printer3DScene>   m_scene;
    RECT                              m_paperRectPx = {};

    RECT                    m_hintRect         = {};
    float                   m_hintFontPx       = 12.0f;
};
