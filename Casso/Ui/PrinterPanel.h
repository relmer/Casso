#pragma once

#include "Window/DxuiWindow.h"
#include "Widgets/DxuiButton.h"

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

    // Per-frame live update (FR-033): advance the viewport to the worker's
    // newest row, tick the snap-back clock, and re-render the visible span
    // when something changed (new bytes, viewport motion, or `force`). Cost
    // is bounded by the viewport, never the strip length.
    void     RefreshLive (PrinterWorker & worker, int64_t nowMs, bool force = false);

    // Direct raster push for paths with no live worker (e.g. no printer card:
    // an empty raster shows the blank sheet). Renders the given raster's
    // visible span through the same viewport.
    void     SetStrip (const PrintRaster & raster);

    void     SetOnFinish   (ActionFn fn) { m_onFinish   = std::move (fn); }
    void     SetOnCopy     (ActionFn fn) { m_onCopy     = std::move (fn); }
    void     SetOnDiscard  (ActionFn fn) { m_onDiscard  = std::move (fn); }
    void     SetOnFormFeed (ActionFn fn) { m_onFormFeed = std::move (fn); }
    void     SetOnRefresh  (ActionFn fn) { m_onRefresh  = std::move (fn); }

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
    void     RenderSpan     (const PrintRaster & spanRaster, int firstAbsRow, int lastAbsRow,
                             bool contentDirty, int revealBandTopAbs, int revealColDots);
    void     ComposeCanvas  (const RgbaImage * content, int contentFirstAbsRow, int bottomAbsRow,
                             int revealBandTopAbs, int revealColDots);

    static int64_t  NowMs ();

    const CassoTheme  * m_theme   = nullptr;

    PrinterPaperView  * m_paper    = nullptr;
    DxuiButton        * m_finish   = nullptr;
    DxuiButton        * m_copy     = nullptr;
    DxuiButton        * m_discard  = nullptr;
    DxuiButton        * m_formFeed = nullptr;
    DxuiButton        * m_refresh  = nullptr;

    ActionFn            m_onFinish;
    ActionFn            m_onCopy;
    ActionFn            m_onDiscard;
    ActionFn            m_onFormFeed;
    ActionFn            m_onRefresh;

    // Live-viewport state (FR-033). m_renderedSpan/-Activity detect "something
    // changed" so an idle panel does zero render work per frame.
    PrinterViewport         m_viewport;
    PrinterViewport::Span   m_renderedSpan     = {};
    uint64_t                m_renderedActivity = 0;
    int64_t                 m_lastRenderMs     = 0;
    bool                    m_hasRendered      = false;

    // Smooth scrolling: the DISPLAYED window glides toward the viewport's
    // logical position at frame rate, so discrete key/wheel steps (and the
    // snap back to live) read as continuous paper motion. < 0 = unseeded.
    double                  m_smoothBottom     = -1.0;
    int64_t                 m_smoothLastMs     = 0;

    // Head-timing ink reveal (FR-034): pacing chases the worker's head
    // position at ImageWriter speed; the rendered-reveal pair detects sweep
    // motion so the panel keeps animating between byte arrivals. Primed
    // caught-up on first refresh so a restored strip never replays history.
    PrinterPacing           m_pacing;
    bool                    m_pacingPrimed     = false;
    int                     m_renderedRevealRow = -1;
    int                     m_renderedRevealCol = -1;

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
    int                     m_canvasRevealCol = -1;
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
