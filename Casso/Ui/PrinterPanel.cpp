#include "Pch.h"

#include "PrinterPanel.h"

#include "CassoTheme.h"
#include "Printer3DScene.h"
#include "../resource.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/RgbaImage.h"
#include "Print/PrinterWorker.h"




static constexpr wchar_t   s_kpszTitle     [] = L"Casso Printer";
static constexpr wchar_t   s_kpszClassName [] = L"CassoPrinterPanel";

static constexpr int       s_kPreferredWidthDip  = 560;
static constexpr int       s_kPreferredHeightDip = 680;

// Viewport render: 144 dpi maps native rows 1:1 to pixels, so the visible
// ~1-page span is a fixed 1152x1584 image regardless of strip length --
// bounded memory, stable scale-to-fit, and delta-friendly row alignment.
static constexpr int       s_kPreviewDpi = PrinterGrid::kRowsPerInch;

// Minimum interval between live re-renders while bytes stream (~60 Hz).
// Viewport motion (scroll / snap) bypasses it so input feels immediate.
static constexpr int64_t   s_kMinRenderIntervalMs = 16;

// Scroll step sizes in native rows (144 rows/inch).
static constexpr int       s_kWheelRowsPerNotch = 144;   // 1" per wheel notch
static constexpr int       s_kArrowScrollRows   = 48;    // 1/3" per key press

// Guest-activity gap after which the print counts as finished: Form Feed
// arms, matching the shell-side gate on the same signal.
static constexpr int64_t   s_kPrintIdleMs = 1200;

// Zoom: each button press / wheel notch scales by this factor, clamped to
// [min,max]. 1 = fit-to-window; the scene camera does the magnifying.
static constexpr float     s_kZoomStep = 1.25f;
static constexpr float     s_kZoomMin  = 1.0f;
static constexpr float     s_kZoomMax  = 4.0f;

// Framing reach: a plain (1 - 1/Z) can only center the content EDGE, but the
// status LEDs + switches sit just inside the lower-right corner and need a
// touch more camera travel to bring to the middle at ~300%. Boost the reach
// so the corners are framable by 3x, clamped to 1 so the printer can't be
// panned clear out of the view.
static constexpr float     s_kFramingReach = 1.5f;

// Fanfold paper furniture (FR-032; panel-only per FR-027), all in px at the
// fixed 144 dpi preview scale. Real continuous-form stock: 9.5" wide with
// 0.5" tractor strips both sides (tear width 8.5"), 5/32" sprocket holes on
// a 1/2" pitch, and the 8" printable area centered between the strips.
static constexpr int       s_kStockWidthPx   = (19 * PrinterGrid::kRowsPerInch) / 2;   // 9.5" = 1368
static constexpr int       s_kStripWidthPx   = PrinterGrid::kRowsPerInch / 2;          // 0.5" =   72
static constexpr int       s_kContentXPx     = s_kStripWidthPx + PrinterGrid::kRowsPerInch / 4;   // 0.75" = 108
static constexpr int       s_kHoleRadiusPx   = 11;                                     // ~5/32" dia
static constexpr int       s_kHolePitchPx    = PrinterGrid::kRowsPerInch / 2;          // 0.5" =   72
static constexpr uint32_t  s_kArgbHoleRim    = 0xFFB8B8B8;   // sprocket hole edge

// The live pin band (FR-034): a head pass strikes 8 pins spaced 1/72",
// i.e. 2 native rows each -- 16 rows below the paper row. The reveal mask
// clips this band at the paced head column; rows above it are complete. MUST
// equal PrinterPacing's rowsPerSweep so a swept band tiles exactly onto the next.
static constexpr int       s_kPinBandRows = PrinterGrid::kPinBandRows;

// Bidirectional reveal lag: the carriage prints alternate lines right-to-left
// (real ImageWriter), but our interpreter fills each line's dots left-to-
// right, so a right-to-left reveal is only correct on a COMPLETED line. Hold
// the reveal this many rows behind the guest's head while it prints so the
// line being swept is always fully in the raster -- comfortably more than a
// pin band, ~1/6" (one text line). Released to the live head once idle.
static constexpr int       s_kBidiLagRows = 24;

static constexpr wchar_t   s_kpszScrollHint [] =
    L"Scroll wheel or Up/Down to review \u2022 scroll past the end to lift the last page \u2022 rejoins a live print when idle";




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::FloorMod
//
//  Floor modulus: hole / perforation phase stays continuous for rows above the
//  top of the strip (the leading fanfold paper), where absRow < 0.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPanel::FloorMod (int a, int m)
{
    return ((a % m) + m) % m;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::DarkenPerf
//
//  Perforation dash: a slight darkening of whatever it crosses -- light gray on
//  paper white, a shade darker on ink -- like a real perf cut, instead of
//  stamping gray over (and visually erasing) printed content.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::DarkenPerf (uint32_t & px)
{
    uint32_t  a = px & 0xFF000000u;
    uint32_t  r = ((px >> 16) & 0xFF) * 210 / 255;
    uint32_t  g = ((px >>  8) & 0xFF) * 210 / 255;
    uint32_t  b = ( px        & 0xFF) * 210 / 255;

    px = a | (r << 16) | (g << 8) | b;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::LoadTextResource
//
//  Read an embedded RCDATA resource (the user's ImageWriter CAD model) into a
//  string. Returns empty on any failure -- the scene then keeps its procedural
//  body, so a missing model never blanks the panel.
//
////////////////////////////////////////////////////////////////////////////////

std::string PrinterPanel::LoadTextResource (int resourceId)
{
    HINSTANCE   hInstance = GetModuleHandleW (nullptr);
    HRSRC       hRes      = nullptr;
    HGLOBAL     hMem      = nullptr;
    DWORD       cbData    = 0;
    void      * pData     = nullptr;

    hRes = FindResourceW (hInstance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
    if (hRes == nullptr) { return {}; }

    cbData = SizeofResource (hInstance, hRes);
    hMem   = LoadResource (hInstance, hRes);
    if (cbData == 0 || hMem == nullptr) { return {}; }

    pData = LockResource (hMem);
    if (pData == nullptr) { return {}; }

    return std::string ((const char *) pData, cbData);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel ctor / dtor
//
//  Defined here (not defaulted in the header) so unique_ptr<Printer3DScene>
//  destroys against the complete type.
//
////////////////////////////////////////////////////////////////////////////////

PrinterPanel::PrinterPanel()
    : m_panZoom (PanZoomConfig())
{
}

PrinterPanel::~PrinterPanel() = default;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::PanZoomConfig
//
//  Tunes the reusable controller for the printer preview: zoom range + step
//  match the old toolbar, a wheel notch scrolls 2/3" (96 native rows) or pans
//  96 content px horizontally, pan glides to preserve the smooth-scroll feel,
//  and zoom stays instant (FR-027 preview chrome the user already liked).
//
////////////////////////////////////////////////////////////////////////////////

DxuiPanZoom::Config PrinterPanel::PanZoomConfig()
{
    DxuiPanZoom::Config   cfg;

    cfg.zoomMin        = s_kZoomMin;
    cfg.zoomMax        = s_kZoomMax;
    cfg.zoomStep       = s_kZoomStep;
    cfg.wheelPanY      = (float) s_kWheelRowsPerNotch;   // native rows / notch
    cfg.wheelPanX      = (float) s_kWheelRowsPerNotch;   // content px / notch
    cfg.easeTauSec     = 0.08;    // glide used ONLY for the follow snap-back
    cfg.zoomEaseTauSec = 0.0;     // instant zoom (fit-to-window chrome)
    cfg.userPanInstant = true;    // wheel / drag track the paper 1:1, no lag

    return cfg;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const CassoTheme     * theme)
{
    HRESULT                    hr = S_OK;
    DxuiWindow::CreateParams   params;

    UNREFERENCED_PARAMETER (device);
    UNREFERENCED_PARAMETER (context);

    if (IsCreated())
    {
        return S_OK;
    }

    m_theme = theme;

    params.title             = s_kpszTitle;
    params.hInstance         = hInstance;
    params.ownerHwnd         = hwndOwner;
    params.initialSizeDip    = { s_kPreferredWidthDip, s_kPreferredHeightDip };
    params.minSizeDip        = { 360, 420 };
    params.resizable         = true;
    params.captionStyle      = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride = s_kpszClassName;

    // The preview animates (paper feed, head sweep, smooth scroll) on the
    // same UI thread that presents the vsynced main window; presenting this
    // window unsynced keeps the pair from stacking two vblank waits per
    // frame and halving everyone's frame rate. DWM composes at vsync.
    params.presentSyncInterval = 0;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetTheme (m_theme);

    // The shell's UI loop repaints this panel every frame (RenderFrame), so the
    // per-wheel-message auto-invalidate is redundant here -- and a precision
    // touchpad's wheel-message flood would otherwise spawn a synchronous 3D
    // repaint per message and starve the loop's own paint pump, freezing the
    // view mid-scroll. Let the loop own paint pacing.
    if (PopupHost() != nullptr)
    {
        PopupHost()->SetSuppressInputInvalidate (true);
    }

    m_tooltip.SetPopupHost (PopupHost());

    // 3D presentation (FR-032): build the scene on THIS window's own device
    // (its swap chain does not live on the emulator renderer's device) and
    // draw it from the before-present hook -- under the panel chrome, which
    // deliberately leaves the paper rect unfilled. Failure falls back to the
    // flat PrinterPaperView silently.
    {
        std::unique_ptr<Printer3DScene>   scene = std::make_unique<Printer3DScene> ();

        if (PopupHost() != nullptr &&
            SUCCEEDED (scene->Initialize (PopupHost()->GetDevice(), PopupHost()->GetContext())))
        {
            m_scene = std::move (scene);

            // The user's ImageWriter CAD model, embedded as OBJ+MTL. Failure
            // silently keeps the procedural body.
            {
                std::string   obj = LoadTextResource (IDR_MODEL_IMAGEWRITER_OBJ);
                std::string   mtl = LoadTextResource (IDR_MODEL_IMAGEWRITER_MTL);

                if (!obj.empty())
                {
                    IGNORE_RETURN_VALUE (hr, m_scene->SetModel (obj, mtl));
                }
            }

            PopupHost()->SetBeforePresentHook ([this] ()
            {
                if (m_scene != nullptr && m_paperRectPx.right > m_paperRectPx.left)
                {
                    m_scene->Render (m_paperRectPx);
                }
            });

            if (m_paper != nullptr)
            {
                m_paper->SetVisible (false);   // the scene presents the content now
            }
        }
    }

    Show();

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::OnCreate()
{
    m_paper     = CreateChild<PrinterPaperView> ();

    // Top toolbar. The ellipsis on Print / Save signals a dialog follows.
    m_print     = CreateChild<DxuiButton> (L"Print\u2026");
    m_saveAs    = CreateChild<DxuiButton> (L"Save\u2026");
    m_copy      = CreateChild<DxuiButton> (L"Copy");
    m_zoomOut   = CreateChild<DxuiButton> (L"\u2212");   // minus sign
    m_zoomReset = CreateChild<DxuiButton> (L"100%");
    m_zoomIn    = CreateChild<DxuiButton> (L"+");

    // Bottom row.
    m_formFeed  = CreateChild<DxuiButton> (L"Form Feed");
    m_discard   = CreateChild<DxuiButton> (L"Discard");

    m_print->SetOnClick     ([this] () { if (m_onPrint)    { m_onPrint    (); } });
    m_saveAs->SetOnClick    ([this] () { if (m_onSaveAs)   { m_onSaveAs   (); } });
    m_copy->SetOnClick      ([this] () { if (m_onCopy)     { m_onCopy     (); } });
    m_formFeed->SetOnClick  ([this] () { if (m_onFormFeed) { m_onFormFeed(); } });
    m_discard->SetOnClick   ([this] () { if (m_onDiscard)  { m_onDiscard  (); } });

    m_zoomOut->SetOnClick   ([this] () { m_panZoom.ZoomOut(); });
    m_zoomReset->SetOnClick ([this] () { m_panZoom.ResetZoom(); });
    m_zoomIn->SetOnClick    ([this] () { m_panZoom.ZoomIn(); });

    // A genuine user pan drops the viewport out of follow mode so the
    // scrollback holds where the user parks it (RefreshLive stops chasing the
    // live row). Deliberately NO OnChange->Invalidate: RenderFrame already
    // repaints the panel every UI-loop frame, so repainting per input event
    // would fire a synchronous 3D redraw for every one of the message flood a
    // trackpad scroll produces -- clogging the message pump and freezing the
    // view until the fingers stop (the paint pacing stays owned by the loop).
    m_panZoom.SetOnUserPanY ([this] () { m_viewport.NotifyUserScroll (NowMs()); });

    // A freshly opened panel has nothing on the paper yet, so the delivery
    // actions start disabled; RefreshLive enables them the moment content
    // appears. (Without this, a preview opened over a machine with no printer
    // card -- which never runs RefreshLive -- would show them wrongly live.)
    m_print->SetEnabled   (false);
    m_saveAs->SetEnabled  (false);
    m_copy->SetEnabled    (false);
    m_discard->SetEnabled (false);

    // Establish the zoom label ("100%") and disable [-] at the low end.
    SyncTransform();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::SetTheme (const CassoTheme * theme)
{
    m_theme = theme;
    DxuiWindow::SetTheme (theme);   // implicit upcast CassoTheme -> DxuiTheme -> IDxuiTheme
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RenderFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterPanel::RenderFrame()
{
    if (!IsCreated())
    {
        return S_OK;
    }

    m_tooltip.Tick (NowMs());

    // Advance the pan/zoom glide and push the transform to the scene every
    // frame (runs even with no printer card, so zooming a blank sheet still
    // animates). RefreshLive layers the follow-mode panY target on top. The
    // Tick return keeps the frame cadence hot while a glide is still in flight.
    m_panZoomEasing = m_panZoom.Tick ((double) NowMs() / 1000.0);
    SyncTransform();

    Invalidate();
    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::UpdateTooltip
//
//  Hover help for the toolbar. A disabled button's tip says WHY it is
//  disabled instead of what it would do.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::UpdateTooltip (int x, int y)
{
    int64_t   now = NowMs();

    if (m_print != nullptr && m_print->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_print->Bounds(),
            m_print->Enabled()
                ? L"Send the printout to a Windows printer (the paper stays in the printer, so you can also save or copy it)"
                : L"Print (nothing has been printed yet)",
            now);
        return;
    }

    if (m_saveAs != nullptr && m_saveAs->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_saveAs->Bounds(),
            m_saveAs->Enabled()
                ? L"Save the printout as a PNG image file (the paper stays in the printer)"
                : L"Save (nothing has been printed yet)",
            now);
        return;
    }

    if (m_copy != nullptr && m_copy->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_copy->Bounds(),
            m_copy->Enabled()
                ? L"Copy the whole printout to the clipboard (the paper stays in the printer)"
                : L"Copy to clipboard (nothing has been printed yet)",
            now);
        return;
    }

    if (m_zoomOut != nullptr && m_zoomOut->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_zoomOut->Bounds(),
            m_zoomOut->Enabled() ? L"Zoom out" : L"Zoom out (already at fit-to-window)", now);
        return;
    }

    if (m_zoomReset != nullptr && m_zoomReset->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_zoomReset->Bounds(), L"Reset the zoom to fit the window", now);
        return;
    }

    if (m_zoomIn != nullptr && m_zoomIn->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_zoomIn->Bounds(),
            m_zoomIn->Enabled() ? L"Zoom in" : L"Zoom in (already at maximum)", now);
        return;
    }

    if (m_formFeed != nullptr && m_formFeed->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_formFeed->Bounds(),
            m_formFeed->Enabled()
                ? L"Feed the paper to the top of the next page"
                : L"Form feed (waiting for the current print to finish)",
            now);
        return;
    }

    if (m_discard != nullptr && m_discard->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_discard->Bounds(),
            m_discard->Enabled()
                ? L"Tear off the printout and throw it away, loading a fresh sheet"
                : L"Discard (nothing has been printed yet)",
            now);
        return;
    }

    m_tooltip.RequestHide (now);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::SyncTransform
//
//  Per-frame bridge from the pan/zoom controller to the 3D scene and toolbar.
//  Pushes the eased zoom + horizontal pan to the camera, refreshes the
//  zoom-dependent pan bounds and drag scale, and (only when the zoom actually
//  changed) relabels the reset button and re-enables the +/- ends. Zoom is
//  preview-only chrome (FR-027): it never touches the raster or delivery.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::SyncTransform()
{
    float   zoom = m_panZoom.Zoom();

    // Framing is only possible once zoomed in. Horizontal: at zoom Z the paper
    // is Z times wider than the view, so panX may slide +/- half the hidden
    // width (content px; the scene takes a normalized -1..1 paper edge).
    // Vertical camera framing spans a normalized +/-(1 - 1/Z): zero at fit,
    // approaching +/-1 (full up/down reach) as the zoom climbs.
    {
        float   f         = (std::min) (1.0f, s_kFramingReach * (1.0f - 1.0f / zoom));
        float   halfRange = (float) s_kStockWidthPx * 0.5f * f;
        float   camRange  = f;

        m_panZoom.SetPanXBounds (-halfRange, halfRange);
        m_panZoom.SetPanYCamBounds (-camRange, camRange);
    }

    // Drag scale: a screen-pixel drag moves this much. panX is content px (paper
    // width shrinks with zoom -> fewer content px per dragged pixel). panYCam is
    // the normalized camera framing -- a fixed fraction per pixel so a partial
    // drag reaches the up/down limit at any zoom (it clamps to the bounds above).
    {
        int     pw = m_paperRectPx.right - m_paperRectPx.left;
        int     ph = m_paperRectPx.bottom - m_paperRectPx.top;
        float   perPxX = (pw > 0) ? (float) s_kStockWidthPx / ((float) pw * zoom) : 1.0f;
        float   perPxY = (ph > 0) ? 2.2f / (float) ph : 1.0f;

        m_panZoom.SetDragScale (perPxX, perPxY);
    }

    // Cursor-anchored zoom needs the paper rect's center in the same space as
    // event positions (PaperHit compares them directly, so m_paperRectPx is it).
    m_panZoom.SetViewCenter ((float) (m_paperRectPx.left + m_paperRectPx.right) * 0.5f,
                             (float) (m_paperRectPx.top  + m_paperRectPx.bottom) * 0.5f);

    // World overscroll: once the paper is pinned at a scroll limit, up to half a
    // viewport of further pan slides the whole 3D world to a hard stop instead
    // of hitting a wall. The scene takes a normalized -1..1 (edge = stop).
    float   overMax = (float) m_viewport.ViewportRows() * 0.5f;
    m_panZoom.SetOverscrollYMax (overMax);

    if (m_scene != nullptr)
    {
        m_scene->SetZoom (zoom);
        m_scene->SetPanX (m_panZoom.PanX() / ((float) s_kStockWidthPx * 0.5f));
        m_scene->SetCameraPanY (m_panZoom.PanYCam());
        m_scene->SetWorldPanY ((overMax > 0.0f) ? (m_panZoom.OverscrollY() / overMax) : 0.0f);
    }

    // Zoom chrome changes rarely; refresh it only when the target moves.
    if (m_panZoom.ZoomTarget() != m_zoomChromeSynced)
    {
        m_zoomChromeSynced = m_panZoom.ZoomTarget();

        if (m_zoomReset != nullptr)
        {
            wchar_t   label[16];
            swprintf_s (label, L"%d%%", (int) std::lround (m_zoomChromeSynced * 100.0f));
            m_zoomReset->SetLabel (label);
        }

        if (m_zoomOut != nullptr) { m_zoomOut->SetEnabled (m_zoomChromeSynced > s_kZoomMin + 1e-3f); }
        if (m_zoomIn  != nullptr) { m_zoomIn->SetEnabled  (m_zoomChromeSynced < s_kZoomMax - 1e-3f); }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::PaperHit
//
//  True when (x,y) DIP lands on the paper area (the 3D scene / paper view),
//  where a left-press begins a pan-drag. Toolbar bands sit outside this rect,
//  so their buttons keep their clicks.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::PaperHit (int x, int y) const
{
    return x >= m_paperRectPx.left && x < m_paperRectPx.right &&
           y >= m_paperRectPx.top  && y < m_paperRectPx.bottom;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::NowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t PrinterPanel::NowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RefreshLive
//
//  The per-frame heartbeat: sync the viewport to the worker's newest row and
//  re-render the visible span only when something changed -- new bytes landed,
//  the viewport moved (scroll or snap-back), or the caller forced it. The
//  span snapshot and render are both bounded by the viewport (~1 page), so a
//  60-page banner costs the same per frame as a receipt (SC-010).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::RefreshLive (PrinterWorker & worker, int64_t nowMs, bool force)
{
    int                     rows        = worker.RowsUsed();
    uint64_t                activity    = worker.ActivityCount();
    double                  nowSec      = (double) nowMs / 1000.0;
    int                     headRow     = 0;
    int                     headCol     = 0;
    int                     revealRow   = 0;
    int                     revealCol   = 0;
    int                     revealLo    = 0;
    int                     revealHi    = 0;
    bool                    sweepLtr    = true;
    int                     bandBottom  = 0;
    bool                    moved       = false;
    bool                    revealMoved = false;
    PrinterViewport::Span   span;
    PrintRaster             spanRaster;

    if (m_paper == nullptr)
    {
        return;
    }

    worker.HeadPosition (headRow, headCol);

    // Adopt the worker's current activity count on the first refresh so that
    // opening the panel over a restored / pending strip does NOT read the
    // initial 0 -> N sync as a fresh "receiving" event -- which would flash the
    // status LEDs bright for a full idle window before settling back to dim.
    if (!m_activityPrimed)
    {
        m_renderedActivity = activity;
        m_activityPrimed   = true;
    }

    // Toolbar validity: the delivery actions need a printout on the paper;
    // Form Feed arms only once the guest print idles (feeding mid-print
    // would interleave a page break into its stream).
    if (activity != m_renderedActivity)
    {
        m_lastActivityChangeMs = nowMs;
    }

    {
        bool   hasContent = rows > 0;
        bool   printing   = (m_lastActivityChangeMs != 0)
                            && (nowMs - m_lastActivityChangeMs < s_kPrintIdleMs);

        // Held for NeedsAnimationFrame so the present cadence stays hot across
        // the whole active print, not just the strict sweep (avoids the coarse
        // idle tick jerking the carriage at every line boundary).
        m_printingActive = printing;

        if (m_print    != nullptr) { m_print->SetEnabled    (hasContent); }
        if (m_saveAs   != nullptr) { m_saveAs->SetEnabled   (hasContent); }
        if (m_copy     != nullptr) { m_copy->SetEnabled     (hasContent); }
        if (m_discard  != nullptr) { m_discard->SetEnabled  (hasContent); }
        if (m_formFeed != nullptr) { m_formFeed->SetEnabled (!printing);  }
    }

    // A shrunk strip means eject/discard tore the paper off: rewind the view
    // and the reveal to the fresh sheet instead of staring past its end.
    if (rows - 1 < m_viewport.LiveRow())
    {
        m_viewport.Reset();
        m_pacing.Reset (nowSec, 0);
        m_spanImgValid = false;
        m_panYSeeded   = false;   // reseed onto the fresh sheet, don't glide across the tear
    }

    // First refresh starts caught up at the head, so opening the panel over a
    // restored pending strip never replays its history (only NEW ink animates).
    if (!m_pacingPrimed)
    {
        m_pacing.Reset (nowSec, headRow);
        m_pacingPrimed = true;
    }

    // The FR-034 reveal: pacing chases the head at ImageWriter speed. At
    // authentic guest speed it stays caught up (the sweep IS the guest's own
    // timing); at max speed it animates behind, jump-cutting past big backlogs.
    //
    // Bidirectional carriage: the real ImageWriter prints each line in the
    // opposite direction. Our interpreter lays every line's dots left-to-right,
    // so a right-to-left REVEAL only looks correct once the line is COMPLETE in
    // the raster -- hold the reveal one line behind the guest while it is
    // actively printing (released to the live head the moment the print idles).
    // The sweep runs the full carriage width in the alternating direction; the
    // ink-gate keeps the buzz silent over the blank overtravel of a short line.
    int  laggedRow = m_printingActive ? (std::max) (0, headRow - s_kBidiLagRows) : headRow;

    m_pacing.SetTargetPosition (laggedRow, PrinterGrid::kDotsPerRow);
    m_pacing.Advance (nowSec);

    revealRow  = m_pacing.RevealedRows();
    bandBottom = (std::min) (revealRow + s_kPinBandRows - 1, rows - 1);
    sweepLtr   = m_pacing.SweepLeftToRight();

    {
        // The head sweeps the live band whether or not it has caught the guest:
        // revealing ink IS the sweep now (PrinterPacing), so a backlog drains as a
        // visible carriage pass, not a full-width snap. progress == full width when
        // caught up, so the live band then shows complete. Mirror for a R->L pass.
        int  progress = m_pacing.RevealedColDots();   // 0..kDotsPerRow sweep distance

        revealCol = sweepLtr ? progress : (PrinterGrid::kDotsPerRow - progress);
        revealLo  = sweepLtr ? 0 : revealCol;
        revealHi  = sweepLtr ? revealCol : PrinterGrid::kDotsPerRow;
    }

    // The carriage is animating whenever the reveal trails the lagged target or a
    // sweep is mid-flight; m_printingActive additionally holds the present
    // cadence hot across line boundaries (see NeedsAnimationFrame).
    m_sweeping = (revealRow < laggedRow) || (revealLo > 0) || (revealHi < PrinterGrid::kDotsPerRow);

    if (m_scene != nullptr)
    {
        m_scene->SetHeadColumn01 ((float) revealCol / (float) PrinterGrid::kDotsPerRow);

        // Front-panel status lamps carry fixed per-lamp meanings (see
        // Printer3DScene::LampRole): Power + Select sit steady-lit while the
        // emulated printer is powered + online, while Print Quality (draft) and
        // the red fault lamp stay dark. They no longer pulse together with the
        // receive activity -- the carriage motion and sound convey that.
        m_scene->SetLeds (/*online*/ true, /*error*/ false);
    }

    // The viewport follows the REVEALED edge, not the raster's -- so a paced
    // reveal happens on-screen instead of scrolling past unseen.
    if (rows > 0)
    {
        m_viewport.Advance ((std::max) (bandBottom, 0));
    }
    m_viewport.Tick (nowMs);

    // The scroll position lives in m_panZoom now. Follow mode drives its panY
    // target to the live row each frame; a user pan (which fired
    // NotifyUserScroll) instead leaves it parked where they put it. panZoom
    // clamps to the viewport's legal bounds and eases the position (glided in
    // RenderFrame's Tick), and the eased bottom row is what we render.
    m_panZoom.SetPanYBounds ((float) m_viewport.MinBottomRow(),
                             (float) m_viewport.MaxBottomRow());

    if (m_viewport.FollowingLive())
    {
        m_panZoom.SetPanYTarget ((float) m_viewport.LiveRow());
    }

    // Seed the eased position onto the target on the first content frame (and
    // after a tear), so opening over a restored strip lands on the paper
    // instead of scrolling down to it from row 0.
    if (!m_panYSeeded && rows > 0)
    {
        m_panZoom.SnapPanY (m_panZoom.PanYTarget());
        m_panYSeeded = true;
    }

    span.lastRow  = (int) std::lround (m_panZoom.PanY());
    span.firstRow = (std::max) (0, span.lastRow - m_viewport.ViewportRows() + 1);

    moved       = (span.firstRow != m_renderedSpan.firstRow || span.lastRow != m_renderedSpan.lastRow);
    revealMoved = (revealRow != m_renderedRevealRow || revealCol != m_renderedRevealCol);

    if (!force && !moved && !revealMoved && m_hasRendered && activity == m_renderedActivity)
    {
        return;   // nothing changed: zero work this frame
    }

    if (!force && !moved && nowMs - m_lastRenderMs < s_kMinRenderIntervalMs)
    {
        return;   // pace streaming re-renders; the change lands next frame
    }

    if (rows <= 0)
    {
        ShowBlankSheet();
        m_revealInk = false;
    }
    else if (worker.SnapshotStripSpan (span.firstRow, span.lastRow, spanRaster))
    {
        bool   contentDirty = (activity != m_renderedActivity) || !m_hasRendered;

        RenderSpan (spanRaster, span.firstRow, span.lastRow, contentDirty, revealRow, revealLo, revealHi);

        // Tell the audio whether the head is on ink (buzz) vs feeding (silent).
        // Sample just BEHIND the head in the sweep direction -- the sweep is now
        // always the reveal (backlog included), so there is a meaningful head
        // column every frame; behind-head keeps a word buzzing as one but goes
        // silent over a wide blank margin.
        {
            constexpr int  kInkLookbackDots = (PrinterGrid::kDotsPerInchH * 3) / 10;   // 0.3"
            int  sampleLo;
            int  sampleHi;

            if (sweepLtr)
            {
                sampleLo = (std::max) (0, revealCol - kInkLookbackDots);
                sampleHi = (std::min) (PrinterGrid::kDotsPerRow - 1, revealCol);
            }
            else
            {
                sampleLo = (std::max) (0, revealCol);
                sampleHi = (std::min) (PrinterGrid::kDotsPerRow - 1, revealCol + kInkLookbackDots);
            }

            m_revealInk = RevealBandHasInk (spanRaster, span.firstRow, revealRow, sampleLo, sampleHi);
        }
    }
    else
    {
        ShowBlankSheet();   // no active job: fresh paper in the platen
        m_revealInk = false;
    }

    m_renderedSpan      = span;
    m_renderedActivity  = activity;
    m_renderedRevealRow = revealRow;
    m_renderedRevealCol = revealCol;
    m_lastRenderMs      = nowMs;
    m_hasRendered       = true;
    Invalidate();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RevealBandHasInk
//
//  Whether the pin band at the reveal row carries any ink within the column
//  range [sampleLoCol, sampleHiCol], sampled from the span raster. The caller
//  picks the range: a short window just behind the head in the sweep direction
//  while printing a live line (bridges inter-character gaps so a word buzzes as
//  one, but goes silent over a wide margin), or the whole row while rows are
//  still feeding. Drives the audio buzz gate -- inked rows buzz, blank feed /
//  form-feed rows stay silent.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::RevealBandHasInk (const PrintRaster & spanRaster, int spanFirstRow,
                                     int revealRow, int sampleLoCol, int sampleHiCol) const
{
    int   topRow = (std::max) (0, revealRow - spanFirstRow);   // span-relative
    int   botRow = revealRow - spanFirstRow + s_kPinBandRows - 1;

    for (int r = topRow; r <= botRow; r++)
    {
        for (int c = sampleLoCol; c <= sampleHiCol; c++)
        {
            if (spanRaster.CellAt (c, r) != 0)
            {
                return true;
            }
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::SetStrip
//
//  Direct push for worker-less paths (no printer card / tests): same viewport
//  and span render, sourced from the supplied raster instead of the worker.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::SetStrip (const PrintRaster & raster)
{
    int                     rows = raster.RowsUsed();
    PrinterViewport::Span   span;
    PrintRaster             spanRaster;

    if (m_paper == nullptr)
    {
        return;
    }

    if (rows - 1 < m_viewport.LiveRow())
    {
        m_viewport.Reset();
    }

    if (rows <= 0)
    {
        ShowBlankSheet();
        m_hasRendered = true;
        return;
    }

    m_viewport.Advance (rows - 1);

    // One-shot push: place panZoom's eased position on the follow target (no
    // glide) and render that bottom-anchored span.
    m_panZoom.SetPanYBounds ((float) m_viewport.MinBottomRow(), (float) m_viewport.MaxBottomRow());
    if (m_viewport.FollowingLive())
    {
        m_panZoom.SetPanYTarget ((float) m_viewport.LiveRow());
    }
    m_panZoom.SnapPanY (m_panZoom.PanYTarget());
    m_panYSeeded = true;

    span.lastRow  = (int) std::lround (m_panZoom.PanY());
    span.firstRow = (std::max) (0, span.lastRow - m_viewport.ViewportRows() + 1);
    raster.CopyRowSpan (span.firstRow, span.lastRow, spanRaster);
    RenderSpan (spanRaster, span.firstRow, span.lastRow, true, -1, 0, 0);   // no live head: show everything

    m_renderedSpan = span;
    m_hasRendered  = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::ShowBlankSheet
//
//  A blank sheet rather than an empty window, so the preview always reads as
//  "paper loaded, nothing printed yet" -- same fanfold canvas, no content.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::ShowBlankSheet()
{
    ComposeCanvas (nullptr, 0, 0, -1, 0, 0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RenderSpan
//
//  Render the (rebased) span raster, then compose it onto the fanfold canvas.
//  `firstAbsRow`/`lastAbsRow` are the span's bounds in strip-absolute terms:
//  the canvas bottom is the span's LAST row (the live row at the platen), and
//  the sprocket-hole / perforation phase keys off the same frame so the
//  furniture scrolls WITH the paper instead of sitting still while content
//  slides past it. The reveal pair (band top + head column, FR-034) clips
//  the live line; -1 disables.
//
//  The ink render is the expensive step, so it is cached by absolute row:
//  scrolling shifts the window over UNCHANGED content, and only the newly
//  exposed rows are rendered (the rest memmove within the cache). New bytes
//  (`contentDirty`) or a span that stops lining up rebuild the whole image.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::RenderSpan (const PrintRaster & spanRaster, int firstAbsRow, int lastAbsRow,
                               bool contentDirty, int revealBandTopAbs, int revealLoDots, int revealHiDots)
{
    HRESULT                  hr       = S_OK;
    int                      spanRows = lastAbsRow - firstAbsRow + 1;
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;

    if (spanRows <= 0)
    {
        // Degenerate span: blank paper, furniture still tracking the scroll.
        m_spanImgValid = false;
        ComposeCanvas (nullptr, 0, lastAbsRow, -1, 0, 0);
        return;
    }

    opt.outputDpi = s_kPreviewDpi;
    opt.style     = DotStyle::Ink;

    bool   haveImg = false;

    if (!contentDirty && m_spanImgValid && m_spanImg.height == spanRows)
    {
        int   delta = firstAbsRow - m_spanImgFirstAbsRow;   // + = scrolled toward live

        if (delta == 0)
        {
            haveImg = true;   // reveal-only update: the ink image is current
        }
        else if (std::abs (delta) < spanRows)
        {
            // Pure scroll: shift the overlap, render only the exposed edge.
            int         keepRows = spanRows - std::abs (delta);
            size_t      rowBytes = (size_t) m_spanImg.width * 4;
            int         newFirst = (delta > 0) ? spanRows - delta : 0;   // exposed block, span-relative
            int         newCount = std::abs (delta);
            RgbaImage   edge;

            if (delta > 0)
            {
                memmove (m_spanImg.PixelAt (0, 0), m_spanImg.PixelAt (0, delta), rowBytes * keepRows);
            }
            else
            {
                memmove (m_spanImg.PixelAt (0, -delta), m_spanImg.PixelAt (0, 0), rowBytes * keepRows);
            }

            hr = renderer.Render (spanRaster, newFirst, newFirst + newCount - 1, opt, edge);

            if (SUCCEEDED (hr) && edge.width == m_spanImg.width && edge.height == newCount)
            {
                memcpy (m_spanImg.PixelAt (0, newFirst), edge.PixelAt (0, 0), rowBytes * newCount);
                m_spanImgFirstAbsRow = firstAbsRow;
                haveImg              = true;
            }
        }
    }

    if (!haveImg)
    {
        hr = renderer.Render (spanRaster, 0, spanRows - 1, opt, m_spanImg);

        if (FAILED (hr) || m_spanImg.width <= 0 || m_spanImg.height != spanRows)
        {
            m_spanImgValid = false;
            return;   // keep the previous frame rather than flash a bad one
        }

        m_spanImgFirstAbsRow = firstAbsRow;
        m_spanImgValid       = true;
        m_spanImgGen++;      // content pixels changed: the canvas cache must rebuild
    }

    if (m_spanImg.height > m_viewport.ViewportRows())
    {
        return;   // span larger than the canvas: keep the previous frame
    }

    ComposeCanvas (&m_spanImg, firstAbsRow, lastAbsRow, revealBandTopAbs, revealLoDots, revealHiDots);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::ComposeCanvas
//
//  Builds the fanfold-paper view (FR-032): a fixed full-viewport canvas at
//  9.5" stock width -- tractor strips with sprocket holes down both edges,
//  light vertical perforations where the strips tear off, cross perforations
//  at every 11" page boundary. The canvas bottom is `bottomAbsRow` (the
//  span's live row, where the platen will be) and content rows land at their
//  true strip positions above it -- ink that ends before the live row keeps
//  its trailing gap instead of being dragged down to the platen. Hole and
//  perforation phase is strip-absolute, so the furniture feeds upward with
//  the paper. Constant canvas size = stable texture and scale-to-fit (no
//  zoom jumps). Holes are punched transparent so the dark mat shows through
//  them.
//
//  The FR-034 reveal mask clips the live pin band: rows at or below
//  `revealBandTopAbs` show ink only left of the paced head column, so the
//  head visibly lays ink as it sweeps. -1 disables the mask.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::ComposeCanvas (const RgbaImage * content, int contentFirstAbsRow, int bottomAbsRow,
                                  int revealBandTopAbs, int revealLoDots, int revealHiDots)
{
    HRESULT   hr        = S_OK;
    int       canvasW   = s_kStockWidthPx;
    int       canvasH   = m_viewport.ViewportRows();   // px == rows at 144 dpi
    int       topAbsRow = bottomAbsRow - canvasH + 1;   // canvas bottom = span's live row
    int       holeR     = s_kHoleRadiusPx;

    if (m_canvas.size() != (size_t) canvasW * canvasH)
    {
        m_canvas.assign ((size_t) canvasW * canvasH, 0xFFFFFFFFu);
        m_canvasValid = false;
    }

    // Rebuild canvas rows [rowFirst..rowLast] (canvas-relative, inclusive)
    // from scratch: paper white, content blit with the FR-034 reveal clip,
    // perforations, then the sprocket holes whose circles reach the range.
    // Everything is a function of absolute row phase, so a range rebuild is
    // bit-identical to the same rows of a full rebuild.
    auto RebuildRows = [&] (int rowFirst, int rowLast)
    {
        rowFirst = (std::max) (rowFirst, 0);
        rowLast  = (std::min) (rowLast, canvasH - 1);

        if (rowLast < rowFirst)
        {
            return;
        }

        std::fill (m_canvas.begin() + (size_t) rowFirst * canvasW,
                   m_canvas.begin() + ((size_t) rowLast + 1) * canvasW, 0xFFFFFFFFu);

        // Content, bottom-anchored in the printable area, premultiplied for
        // the GPU blit (paper is opaque, but anti-aliased dot edges carry
        // alpha). Rows in the live pin band blit only up to the paced head
        // column; ink to its right stays paper white until the sweep gets
        // there (FR-034).
        if (content != nullptr)
        {
            int   yTop = (contentFirstAbsRow - topAbsRow);

            for (int y = 0; y < content->height; y++)
            {
                if (yTop + y < rowFirst || yTop + y > rowLast)
                {
                    continue;
                }

                uint32_t *     dst    = &m_canvas[(size_t) (yTop + y) * canvasW + s_kContentXPx];
                const Byte *   src    = content->PixelAt (0, y);
                int            xStart = 0;
                int            xEnd   = content->width;

                // Rows in the live pin band reveal only the swept column span in
                // the carriage's current direction: [0, head] left-to-right, or
                // [head, width] on a right-to-left pass (FR-034, bidirectional).
                if (revealBandTopAbs >= 0 && contentFirstAbsRow + y >= revealBandTopAbs)
                {
                    xStart = std::clamp (revealLoDots * content->width / PrinterGrid::kDotsPerRow,
                                         0, content->width);
                    xEnd   = std::clamp (revealHiDots * content->width / PrinterGrid::kDotsPerRow,
                                         0, content->width);
                }

                for (int x = xStart; x < xEnd; x++)
                {
                    uint32_t  r = src[x * 4 + 0];
                    uint32_t  g = src[x * 4 + 1];
                    uint32_t  b = src[x * 4 + 2];
                    uint32_t  a = src[x * 4 + 3];

                    dst[x] = (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
                }
            }
        }

        // Vertical tear-off perforations where the tractor strips meet the
        // sheet: dotted 1-px columns, phase locked to the paper.
        for (int y = rowFirst; y <= rowLast; y++)
        {
            if (FloorMod (topAbsRow + y, 8) < 4)
            {
                DarkenPerf (m_canvas[(size_t) y * canvasW + s_kStripWidthPx - 1]);
                DarkenPerf (m_canvas[(size_t) y * canvasW + canvasW - s_kStripWidthPx]);
            }
        }

        // Cross perforations at every page boundary (11" pitch).
        for (int y = rowFirst; y <= rowLast; y++)
        {
            if (FloorMod (topAbsRow + y, PrinterGrid::kPageRows) == 0)
            {
                uint32_t *   row = &m_canvas[(size_t) y * canvasW];

                for (int x = 0; x < canvasW; x++)
                {
                    if (x % 8 < 4)
                    {
                        DarkenPerf (row[x]);
                    }
                }
            }
        }

        // Sprocket holes: punched transparent (alpha 0 -- the mat shows
        // through) with a soft rim, centered in each strip on the 1/2"
        // pitch. Center rows just outside the range still reach into it, so
        // scan wider and clip the writes to the range.
        {
            int   xL = s_kStripWidthPx / 2;
            int   xR = canvasW - s_kStripWidthPx / 2;

            for (int y = rowFirst - holeR - 1; y <= rowLast + holeR + 1; y++)
            {
                if (FloorMod (topAbsRow + y, s_kHolePitchPx) != s_kHolePitchPx / 2)
                {
                    continue;   // y is not a hole-center row
                }

                for (int dy = -holeR - 1; dy <= holeR + 1; dy++)
                {
                    int   py = y + dy;

                    if (py < rowFirst || py > rowLast)
                    {
                        continue;
                    }

                    for (int dx = -holeR - 1; dx <= holeR + 1; dx++)
                    {
                        int   d2 = dx * dx + dy * dy;

                        for (int cx : { xL, xR })
                        {
                            uint32_t &   px = m_canvas[(size_t) py * canvasW + cx + dx];

                            if      (d2 <= holeR * holeR)             { px = 0x00000000u;   }
                            else if (d2 <= (holeR + 1) * (holeR + 1)) { px = s_kArgbHoleRim; }
                        }
                    }
                }
            }
        }
    };

    // Fast paths apply only while the SAME full-height content sits aligned
    // under the canvas (no strip-start clamp, no new ink since the cache was
    // composed): a scroll step memmoves and rebuilds the exposed edge; a
    // reveal-sweep frame rebuilds just the pin-band rows.
    bool   aligned = m_canvasValid
                     && content != nullptr && m_canvasHasContent
                     && m_canvasSpanGen == m_spanImgGen
                     && content->height == canvasH
                     && contentFirstAbsRow == topAbsRow;
    bool   revealSame = (revealBandTopAbs == m_canvasRevealTop
                         && revealLoDots == m_canvasRevealLo
                         && revealHiDots == m_canvasRevealHi);
    int    delta      = topAbsRow - m_canvasTopAbs;

    if (aligned && delta == 0 && revealSame)
    {
        // Bit-identical frame; the caller's change detection normally
        // prevents this, so just re-upload.
    }
    else if (aligned && delta == 0)
    {
        // Reveal sweep only: rebuild from the older of the two band tops
        // down to the canvas bottom (the bands live in the last rows).
        int   oldTop = (m_canvasRevealTop >= 0) ? m_canvasRevealTop - topAbsRow : canvasH;
        int   newTop = (revealBandTopAbs  >= 0) ? revealBandTopAbs  - topAbsRow : canvasH;

        RebuildRows ((std::min) (oldTop, newTop), canvasH - 1);
    }
    else if (aligned && revealSame && std::abs (delta) < canvasH)
    {
        // Scroll step: shift the finished canvas and rebuild the exposed
        // edge (padded by the hole radius so straddling holes stay round).
        int      keepRows = canvasH - std::abs (delta);
        size_t   rowBytes = (size_t) canvasW * 4;

        if (delta > 0)
        {
            memmove (m_canvas.data(), m_canvas.data() + (size_t) delta * canvasW, rowBytes * keepRows);
            RebuildRows (canvasH - delta - holeR - 1, canvasH - 1);
        }
        else
        {
            memmove (m_canvas.data() + (size_t) (-delta) * canvasW, m_canvas.data(), rowBytes * keepRows);
            RebuildRows (0, -delta + holeR);
        }
    }
    else
    {
        RebuildRows (0, canvasH - 1);
    }

    m_canvasTopAbs     = topAbsRow;
    m_canvasRevealTop  = revealBandTopAbs;
    m_canvasRevealLo   = revealLoDots;
    m_canvasRevealHi   = revealHiDots;
    m_canvasSpanGen    = m_spanImgGen;
    m_canvasHasContent = (content != nullptr);
    m_canvasValid      = true;

    if (m_scene != nullptr)
    {
        // The sheet above the head only exists as far as paper has fed past
        // it: bottomAbsRow IS that feed (rows 0..bottom have passed) -- no
        // phantom blank page on a fresh sheet. A short leader always shows
        // (a loaded fanfold's edge sits just past the head; with nothing
        // visible the printer reads as out of paper).
        constexpr int   kLeaderRows = 48;   // ~1/3" of leading edge

        m_scene->SetPaperFeed01 ((float) std::clamp ((std::max) (bottomAbsRow, kLeaderRows), 0, canvasH)
                                 / (float) canvasH);
        IGNORE_RETURN_VALUE (hr, m_scene->SetContent (m_canvas.data(), canvasW, canvasH));
    }
    else
    {
        std::vector<uint32_t>   copy = m_canvas;

        m_paper->SetImage (std::move (copy), canvasW, canvasH);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnMouse
//
//  The paper area is a pan/zoom surface driven by the reusable controller:
//  vertical wheel scrolls (wheel up = back toward earlier pages), horizontal
//  wheel / two-finger pan slides sideways when zoomed, Ctrl+wheel (and touchpad
//  pinch) zooms, and a left-drag on the paper pans. The toolbar bands sit
//  outside the paper rect, so their button clicks flow through the base
//  dispatch untouched.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::OnMouse (const DxuiMouseEvent & ev)
{
    // Wheel gestures (scroll / pan / zoom) always belong to the controller.
    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        if (m_panZoom.OnMouse (ev))
        {
            return true;
        }
    }

    // A left-press on the paper begins a pan-drag; presses on the toolbar fall
    // through so the buttons get their clicks.
    if (ev.kind == DxuiMouseEventKind::Down &&
        ev.button == DxuiMouseButton::Left &&
        PaperHit (ev.positionDip.x, ev.positionDip.y))
    {
        m_tooltip.RequestHide (NowMs());
        m_panZoom.OnMouse (ev);
        return true;
    }

    // Continue / end an active drag. OnMouse returns true only while a drag is
    // in progress, so non-drag moves fall through to the hover tooltip below.
    if ((ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up) &&
        m_panZoom.OnMouse (ev))
    {
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (ev.positionDip.x, ev.positionDip.y);
    }
    else if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_tooltip.RequestHide (NowMs());
    }

    return DxuiWindow::OnMouse (ev);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnKey
//
//  Ctrl +/-/0 zoom via the controller (also the touchpad pinch path); Up/Down
//  and PageUp/PageDown scroll (FR-033); Escape hides the preview (its close-box
//  does the same). Everything else falls through to the base, which fans the
//  key to the child controls.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Down)
    {
        // Ctrl +/=, Ctrl -, Ctrl 0 -> zoom in / out / reset.
        if (ev.ctrl && m_panZoom.OnKey (ev))
        {
            return true;
        }

        switch (ev.vk)
        {
            case VK_ESCAPE:
                Hide();
                return true;

            case VK_UP:
                m_panZoom.PanByUser (0.0f, -(float) s_kArrowScrollRows);
                return true;

            case VK_DOWN:
                m_panZoom.PanByUser (0.0f, +(float) s_kArrowScrollRows);
                return true;

            case VK_PRIOR:
                m_panZoom.PanByUser (0.0f, -(float) PrinterGrid::kPageRows);
                return true;

            case VK_NEXT:
                m_panZoom.PanByUser (0.0f, +(float) PrinterGrid::kPageRows);
                return true;
        }
    }

    return DxuiWindow::OnKey (ev);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Layout
//
//  Two toolbars sandwich the paper. The TOP band carries the document actions
//  (Print / Save / Copy, left) and the zoom cluster ([-] [nnn%] [+], right).
//  The paper view fills the middle, above a hint strip. The BOTTOM band
//  carries paper handling (Form Feed, then Discard).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.Px (10);
    int   gap      = scaler.Px (6);
    int   captionH = CaptionHeightPx();
    int   toolbarH = scaler.Px (46);
    int   hintH    = scaler.Px (20);
    int   btnH     = scaler.Px (30);
    int   btnW     = scaler.Px (84);    // Print... / Save... / Copy / Form Feed / Discard
    int   zoomW    = scaler.Px (42);    // [-] and [+]
    int   zoomResetW = scaler.Px (54);  // [nnn%]

    // Our override replaces DxuiPanel::Layout, which would otherwise record the
    // panel's own bounds; set them so Paint's backdrop fills the whole client.
    SetBounds (boundsDip);

    int   topBandTop = boundsDip.top + captionH;
    int   topBy      = topBandTop + (toolbarH - btnH) / 2;
    int   botBandTop = boundsDip.bottom - toolbarH;
    int   botBy      = botBandTop + (toolbarH - btnH) / 2;

    // Top band: document actions run left-to-right from the left pad.
    {
        int   bx = boundsDip.left + pad;

        for (DxuiButton * btn : { m_print, m_saveAs, m_copy })
        {
            if (btn != nullptr)
            {
                RECT  r = { bx, topBy, bx + btnW, topBy + btnH };
                btn->Layout (r, scaler);
                bx += btnW + gap;
            }
        }
    }

    // Top band: zoom cluster [-] [nnn%] [+] hugs the right edge.
    {
        int   clusterW = zoomW + gap + zoomResetW + gap + zoomW;
        int   zx       = boundsDip.right - pad - clusterW;

        struct { DxuiButton * btn; int w; }   zoomBtns[] =
        {
            { m_zoomOut,   zoomW      },
            { m_zoomReset, zoomResetW },
            { m_zoomIn,    zoomW      },
        };

        for (auto & z : zoomBtns)
        {
            if (z.btn != nullptr)
            {
                RECT  r = { zx, topBy, zx + z.w, topBy + btnH };
                z.btn->Layout (r, scaler);
            }
            zx += z.w + gap;
        }
    }

    m_hintFontPx = scaler.Pxf (11.0f);
    m_hintRect   = { boundsDip.left + pad,
                     botBandTop - hintH,
                     boundsDip.right - pad,
                     botBandTop };

    {
        // The paper fills the middle, between the two toolbar bands (the top
        // band already reserves the caption, so the paper never rides up over
        // the title bar).
        RECT  paperR = { boundsDip.left + pad, topBandTop + toolbarH,
                         boundsDip.right - pad, botBandTop - hintH };

        m_paperRectPx = paperR;   // the 3D scene's viewport (before-present hook)

        if (m_paper != nullptr)
        {
            m_paper->Layout (paperR, scaler);
        }
    }

    // Bottom band: paper handling, left-to-right.
    {
        int   bx = boundsDip.left + pad;

        for (DxuiButton * btn : { m_formFeed, m_discard })
        {
            if (btn != nullptr)
            {
                RECT  r = { bx, botBy, bx + btnW, botBy + btnH };
                btn->Layout (r, scaler);
                bx += btnW + gap;
            }
        }
    }

    m_tooltip.SetViewportSize (boundsDip.right - boundsDip.left,
                               boundsDip.bottom - boundsDip.top);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Paint
//
//  Fill the client with the device-bezel backdrop, let the base pump paint the
//  paper view and toolbar buttons, then draw the scroll hint in its strip
//  between them (FR-033's discoverability line).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr = S_OK;
    RECT     b  = Bounds();

    // Matches the 3D scene's mat color (Printer3DScene s_kArgbMat) so the
    // frame and the scene backdrop read as one surface.
    constexpr uint32_t   kArgbFrame = 0xFF4A505A;

    if (m_scene != nullptr && m_paperRectPx.right > m_paperRectPx.left)
    {
        // The 3D scene owns the paper rect (drawn from the before-present
        // hook, UNDER this painter flush) -- fill only the frame around it,
        // or the backdrop would cover the scene.
        RECT   p = m_paperRectPx;

        painter.FillRect ((float) b.left, (float) b.top,
                          (float) (b.right - b.left), (float) (p.top - b.top), kArgbFrame);
        painter.FillRect ((float) b.left, (float) p.bottom,
                          (float) (b.right - b.left), (float) (b.bottom - p.bottom), kArgbFrame);
        painter.FillRect ((float) b.left, (float) p.top,
                          (float) (p.left - b.left), (float) (p.bottom - p.top), kArgbFrame);
        painter.FillRect ((float) p.right, (float) p.top,
                          (float) (b.right - p.right), (float) (p.bottom - p.top), kArgbFrame);
    }
    else
    {
        painter.FillRect ((float) b.left,
                          (float) b.top,
                          (float) (b.right  - b.left),
                          (float) (b.bottom - b.top),
                          kArgbFrame);
    }

    DxuiPanel::Paint (painter, text, theme);

    {
        DxuiFontHandle  bf = theme.BodyFont();

        IGNORE_RETURN_VALUE (hr, text.DrawString (
            s_kpszScrollHint,
            (float) m_hintRect.left,
            (float) m_hintRect.top,
            (float) (m_hintRect.right  - m_hintRect.left),
            (float) (m_hintRect.bottom - m_hintRect.top),
            0xFF8A8F98,
            m_hintFontPx,
            bf.face,
            DxuiTextHAlign::Center,
            DxuiTextVAlign::Center,
            DxuiFontWeight::Normal,
            false));
    }
}
