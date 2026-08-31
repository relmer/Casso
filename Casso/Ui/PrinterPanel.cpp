#include "Pch.h"

#include "PrinterPanel.h"

#include "CassoTheme.h"
#include "Printer3DScene.h"
#include "../resource.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/PrinterPreviewModel.h"
#include "Devices/Printer/RgbaImage.h"
#include "Print/PrinterWorker.h"




static constexpr wchar_t   s_kpszTitle     [] = L"Casso Printer";
static constexpr wchar_t   s_kpszClassName [] = L"CassoPrinterPanel";

// The chrome band geometry and the sizes derived from it are private members
// of PrinterPanel.

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
// clips this band at the live head column; rows above it are complete.
static constexpr int       s_kPinBandRows = PrinterGrid::kPinBandRows;

// Overtravel past the live band's rightmost ink (logic seeking): the real
// carriage coasts a touch beyond the last printed dot before reversing.
static constexpr int       s_kSweepOvertravelDots = PrinterGrid::kDotsPerInchH / 4;   // 0.25"

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
//  ToolbarTip
//
//  One toolbar button's hover help. Two strings because a disabled button
//  explains why rather than what. See PrinterPanel::UpdateTooltip.
//
////////////////////////////////////////////////////////////////////////////////

struct ToolbarTip
{
    const DxuiButton *  button;
    const wchar_t    *  enabledText;
    const wchar_t    *  disabledText;
};





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
    HINSTANCE    hInstance = GetModuleHandleW (nullptr);
    HRSRC        hRes      = nullptr;
    HGLOBAL      hMem      = nullptr;
    DWORD        cbData    = 0;
    void       * pData     = nullptr;
    std::string  text;



    // Each step needs the one before it to have succeeded, so this is a
    // ladder rather than four independent guards. An empty return at any
    // rung leaves the scene on its procedural body.
    hRes = FindResourceW (hInstance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);

    if (hRes != nullptr)
    {
        cbData = SizeofResource (hInstance, hRes);
        hMem   = LoadResource (hInstance, hRes);
    }

    if (cbData != 0 && hMem != nullptr)
    {
        pData = LockResource (hMem);
    }

    if (pData != nullptr)
    {
        text.assign ((const char *) pData, cbData);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::LoadBinaryResource
//
//  As above, without the copy. RCDATA is mapped into the image, so the
//  returned view stays valid for the life of the process and the caller
//  reads a baked mesh straight out of the executable.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const uint8_t> PrinterPanel::LoadBinaryResource (int resourceId)
{
    HINSTANCE    hInstance = GetModuleHandleW (nullptr);
    HRSRC        hRes      = nullptr;
    HGLOBAL      hMem      = nullptr;
    DWORD        cbData    = 0;
    void       * pData     = nullptr;



    hRes = FindResourceW (hInstance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);

    if (hRes != nullptr)
    {
        cbData = SizeofResource (hInstance, hRes);
        hMem   = LoadResource (hInstance, hRes);
    }

    if (cbData != 0 && hMem != nullptr)
    {
        pData = LockResource (hMem);
    }

    if (pData == nullptr)
    {
        return {};
    }

    return std::span<const uint8_t> (static_cast<const uint8_t *> (pData), cbData);
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
    : m_panZoom (GetPanZoomConfig())
{
}

PrinterPanel::~PrinterPanel() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::GetPanZoomConfig
//
//  Tunes the reusable controller for the printer preview: zoom range + step
//  match the old toolbar, a wheel notch scrolls 2/3" (96 native rows) or pans
//  96 content px horizontally, pan glides to preserve the smooth-scroll feel,
//  and zoom stays instant (FR-027 preview chrome the user already liked).
//
////////////////////////////////////////////////////////////////////////////////

DxuiPanZoom::Config PrinterPanel::GetPanZoomConfig()
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
//  Creates the print-preview window, with three window parameters that are
//  each load-bearing.
//
//  MINIMUM SIZE. The toolbar is the only thing in the panel that cannot adapt,
//  so it alone sets the floor. Its top band packs a fixed left group against a
//  fixed zoom group hugging the right edge and nothing reflows, so below a
//  certain width the two overlap. The chosen minimum leaves enough space that
//  "document actions" and "zoom" still read as separate clusters rather than
//  one undifferentiated run of six buttons. Everything else yields instead of
//  dictating -- the 3D scene widens its FOV to stay whole, and the hint wraps
//  onto its reserved second line.
//
//  UNSYNCED PRESENT. The preview animates on the same UI thread that presents
//  the vsynced main window. Syncing this one too would stack two vblank waits
//  per frame and halve everyone's frame rate; DWM composes at vsync regardless,
//  so nothing tears.
//
//  NO-ACTIVATE CREATION. This window auto-opens mid-print, often mid-KEYSTROKE
//  -- typing PR#1 and pressing Enter is what opens it. Stealing activation
//  means the guest's Enter key-UP lands here instead, leaving the key latched
//  and auto-repeating forever. The menu path still focuses it, with an
//  activating Show immediately after creation.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const CassoTheme     * theme)
{
    HRESULT                    hr        = S_OK;
    DxuiWindow::CreateParams   params;
    bool                       isCreated = false;



    UNREFERENCED_PARAMETER (device);
    UNREFERENCED_PARAMETER (context);

    isCreated = IsCreated();

    BAIL_OUT_IF (isCreated, S_OK);

    m_theme = theme;

    params.title             = s_kpszTitle;
    params.hInstance         = hInstance;
    params.ownerHwnd         = hwndOwner;
    params.initialSizeDip    = { kPreferredWidthDip, kPreferredHeightDip };

    // Minimum IS the preferred size: this layout has no smaller valid form.
    // The top band packs a fixed-width left cluster (Print / Save / Copy,
    // ending at pad + 3*btnW + 2*gap = 274dip) against a fixed-width zoom
    // cluster hugging the right edge (starting at width - 160dip), so below
    // 434dip the two collide -- and nothing reflows to prevent it. Vertically
    // the caption, both 46dip toolbars and the 20dip hint strip are fixed, so
    // every dip lost comes out of the printer scene and then the hint clips.
    // Matches Disk2DebugPanel / InputDebugPanel, which already do this.
    // The toolbar is the ONLY thing that cannot adapt, so it alone sets the
    // floor. Its top band packs a fixed left group (Print / Save / Copy,
    // ending at pad + 3*btnW + 2*gap = 274dip) against a fixed zoom group
    // hugging the right edge (starting at width - 160dip); nothing reflows, so
    // below 434dip they overlap. 460 leaves 26dip between the two groups --
    // 440 would technically clear them, but only by the same 6dip gap used
    // WITHIN the left group, so the six buttons read as one undifferentiated
    // run. The extra 20dip keeps "document actions" and "zoom" legible as
    // separate clusters.
    //
    // Everything else in the panel now yields instead of dictating: the 3D
    // scene widens its FOV to stay whole (Printer3DScene, "contain rather than
    // crop"), and the hint wraps onto its reserved second line. Height is
    // likewise unconstrained by content -- only the caption, the two 46dip
    // toolbars and the hint strip are fixed.
    params.minSizeDip        = { kMinWidthDip, kMinHeightDip };
    params.resizable         = true;
    params.captionStyle      = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride = s_kpszClassName;

    // The preview animates (paper feed, head sweep, smooth scroll) on the
    // same UI thread that presents the vsynced main window; presenting this
    // window unsynced keeps the pair from stacking two vblank waits per
    // frame and halving everyone's frame rate. DWM composes at vsync.
    params.presentSyncInterval = 0;

    // This window auto-opens mid-print, often mid-KEYSTROKE (PR#1 + Enter):
    // creation must not steal activation, or the guest's Enter key-up lands
    // here and the latched key auto-repeats "]" forever. The menu-open path
    // still focuses it via an activating Show() right after creation.
    params.createNoActivate = true;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetTheme (m_theme);

    // The shell's UI loop repaints this panel every frame (RenderFrame), so the
    // per-wheel-message auto-invalidate is redundant here -- and a precision
    // touchpad's wheel-message flood would otherwise spawn a synchronous 3D
    // repaint per message and starve the loop's own paint pump, freezing the
    // view mid-scroll. Let the loop own paint pacing.
    if (GetPopupHost() != nullptr)
    {
        GetPopupHost()->SetSuppressInputInvalidate (true);
    }

    m_tooltip.SetPopupHost (GetPopupHost());

    // 3D presentation (FR-032): build the scene on THIS window's own device
    // (its swap chain does not live on the emulator renderer's device) and
    // draw it from the before-present hook -- under the panel chrome, which
    // deliberately leaves the paper rect unfilled. Failure falls back to the
    // flat PrinterPaperView silently.
    {
        std::unique_ptr<Printer3DScene>   scene   = std::make_unique<Printer3DScene> ();
        HRESULT                           hrScene = E_FAIL;

        if (GetPopupHost() != nullptr)
        {
            hrScene = scene->Initialize (GetPopupHost()->GetDevice(), GetPopupHost()->GetContext());
        }

        if (SUCCEEDED (hrScene))
        {
            m_scene = std::move (scene);

            // The user's ImageWriter CAD model, embedded as a baked mesh.
            // Failure silently keeps the procedural body.
            {
                std::span<const uint8_t>   mesh = LoadBinaryResource (IDR_MODEL_IMAGEWRITER_MESH);

                if (!mesh.empty())
                {
                    hr = m_scene->SetModel (mesh);
                    IGNORE_RETURN_VALUE (hr, S_OK);
                }
            }

            GetPopupHost()->SetBeforePresentHook ([this] ()
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

    // Deliberately NOT shown here: showing (and whether to activate) is the
    // caller's decision. This Create used to end with an activating Show(),
    // which yanked foreground+focus off the main window even on the no-steal
    // auto-open path -- a caller's later Show(false) came too late, the guest's
    // in-flight key-up was already lost to this window, and the latched key
    // auto-repeated into the guest forever (endless "]" lines after PR#1).

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnCreate
//
//  Builds the panel's children and wires their callbacks.
//
//  The delivery actions -- Print, Save, Copy, Discard -- start DISABLED. A
//  freshly opened panel has nothing on the paper, and RefreshLive enables them
//  as soon as content appears. Starting enabled would show them wrongly live
//  on a machine with no printer card, where RefreshLive never runs at all.
//
//  Print and Save carry an ellipsis because a dialog follows; Copy does not,
//  because it completes immediately.
//
//  The pan callback drops the viewport out of follow mode, so once a user
//  scrolls the scrollback holds where they parked it instead of being dragged
//  back to the live row.
//
//  There is deliberately NO OnChange to Invalidate. RenderFrame already
//  repaints the panel every UI-loop frame, so repainting per input event would
//  fire a synchronous 3D redraw for every message in the flood a trackpad
//  scroll produces -- clogging the message pump and freezing the view until the
//  fingers stop. Paint pacing stays owned by the loop.
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
    m_panZoom.SetOnUserPanY ([this] () { m_viewport.NotifyUserScroll (GetNowMs()); });

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
    HRESULT  hr        = S_OK;
    bool     isCreated = false;



    isCreated = IsCreated();

    BAIL_OUT_IF (!isCreated, S_OK);

    m_tooltip.Tick (GetNowMs());

    // Advance the pan/zoom glide and push the transform to the scene every
    // frame (runs even with no printer card, so zooming a blank sheet still
    // animates). RefreshLive layers the follow-mode panY target on top. The
    // Tick return keeps the frame cadence hot while a glide is still in flight.
    m_panZoomEasing = m_panZoom.Tick ((double) GetNowMs() / 1000.0);
    SyncTransform();

    Invalidate();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::UpdateTooltip
//
//  Hover help for the toolbar. A disabled button's tip says WHY it is
//  disabled instead of what it would do, which is the whole reason each
//  row carries two strings.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::UpdateTooltip (int x, int y)
{
    int64_t   now   = GetNowMs();
    bool      shown = false;



    // Toolbar order, left to right. m_zoomReset is the one button that is
    // always enabled, so its two strings are the same.
    const ToolbarTip  tips[] =
    {
        { m_print,
          L"Send the printout to a Windows printer (the paper stays in the printer, so you can also save or copy it)",
          L"Print (nothing has been printed yet)" },

        { m_saveAs,
          L"Save the printout as a PNG image file (the paper stays in the printer)",
          L"Save (nothing has been printed yet)" },

        { m_copy,
          L"Copy the whole printout to the clipboard (the paper stays in the printer)",
          L"Copy to clipboard (nothing has been printed yet)" },

        { m_zoomOut,
          L"Zoom out",
          L"Zoom out (already at fit-to-window)" },

        { m_zoomReset,
          L"Reset the zoom to fit the window",
          L"Reset the zoom to fit the window" },

        { m_zoomIn,
          L"Zoom in",
          L"Zoom in (already at maximum)" },

        { m_formFeed,
          L"Feed the paper to the top of the next page",
          L"Form feed (waiting for the current print to finish)" },

        { m_discard,
          L"Tear off the printout and throw it away, loading a fresh sheet",
          L"Discard (nothing has been printed yet)" },
    };

    for (const ToolbarTip & tip : tips)
    {
        if (!shown && tip.button != nullptr && tip.button->HitTest (x, y))
        {
            m_tooltip.RequestShow (tip.button->GetBounds(),
                                   tip.button->IsEnabled() ? tip.enabledText : tip.disabledText,
                                   now);
            shown = true;
        }
    }

    // Pointer is over the panel but not over any button.
    if (!shown)
    {
        m_tooltip.RequestHide (now);
    }
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
    float  zoom    = m_panZoom.Zoom();
    float  overMax = 0.0f;



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
        int    pw     = m_paperRectPx.right - m_paperRectPx.left;
        int    ph     = m_paperRectPx.bottom - m_paperRectPx.top;
        float  perPxX = (pw > 0) ? (float) s_kStockWidthPx / ((float) pw * zoom) : 1.0f;
        float  perPxY = (ph > 0) ? 2.2f / (float) ph : 1.0f;

        m_panZoom.SetDragScale (perPxX, perPxY);
    }

    // Cursor-anchored zoom needs the paper rect's center in the same space as
    // event positions (IsPaperHit compares them directly, so m_paperRectPx is it).
    m_panZoom.SetViewCenter ((float) (m_paperRectPx.left + m_paperRectPx.right) * 0.5f,
                             (float) (m_paperRectPx.top  + m_paperRectPx.bottom) * 0.5f);

    // The scroll bounds are hard-locked -- the bottom pinned to the last printed
    // row, the top extended just enough to clear the curl -- so there is no
    // world overscroll: hitting a scroll limit stops rather than sliding the 3D
    // world past it. (Cursor zoom / drag pan still move the camera freely.)
    m_panZoom.SetOverscrollYMax (overMax);

    if (m_scene != nullptr)
    {
        m_scene->SetZoom (zoom);
        m_scene->SetPanX (m_panZoom.GetPanX() / ((float) s_kStockWidthPx * 0.5f));
        m_scene->SetCameraPanY (m_panZoom.GetPanYCam());
        m_scene->SetWorldPanY ((overMax > 0.0f) ? (m_panZoom.GetOverscrollY() / overMax) : 0.0f);
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
//  PrinterPanel::IsPaperHit
//
//  True when (x,y) DIP lands on the paper area (the 3D scene / paper view),
//  where a left-press begins a pan-drag. Toolbar bands sit outside this rect,
//  so their buttons keep their clicks.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::IsPaperHit (int x, int y) const
{
    return x >= m_paperRectPx.left && x < m_paperRectPx.right &&
           y >= m_paperRectPx.top  && y < m_paperRectPx.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t PrinterPanel::GetNowMs()
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
    int                    rows         = worker.GetRowsUsed();
    uint64_t               activity     = worker.GetActivityCount();
    double                 nowSec       = (double) nowMs / 1000.0;
    int                    headRow      = 0;
    int                    headCol      = 0;
    int                    revealRow    = 0;
    int                    revealCol    = 0;
    int                    revealLo     = 0;
    int                    revealHi     = 0;
    bool                   sweepLtr     = true;
    int                    bandBottom   = 0;
    bool                   moved        = false;
    bool                   revealMoved  = false;
    bool                   revealBehind = false;
    bool                   urgent       = false;
    bool                   changed      = false;
    bool                   paced        = false;
    PrinterViewport::Span  span;
    PrintRaster            spanRaster;



    // No paper surface yet (the panel is constructed before its first layout),
    // so there is nothing to sync a viewport to.
    if (m_paper != nullptr)
    {
        int  platenRow = 0;

        worker.GetHeadPosition (headRow, headCol);

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

        // A shrunk strip means eject/discard tore the paper off: rewind the view to
        // the fresh sheet instead of staring past its end.
        if (PrinterPreviewModel::StripTornOff (rows, m_viewport.GetLiveRow()))
        {
            m_viewport.Reset();
            m_spanImgValid = false;
            m_panYSeeded   = false;   // reseed onto the fresh sheet, don't glide across the tear
        }

        // The worker replays the interpreter's real carriage timeline off the guest
        // clock (a pass per printed line, a feed per paper advance) at draft speed.
        // It publishes two rows: the PLATEN (GetHeadPosition row), where the head sits --
        // it slews down through a feed, so the viewport follows it and the paper
        // scrolls -- and the reveal FRONTIER (GetRevealBandTop), the line being laid,
        // which holds one band back through a feed so freshly fed paper reads blank
        // even though the raster already holds the next line (drained ahead to keep
        // the buffer full). Ink at or below the frontier shows only within the swept
        // column span; between passes the mask column is 0, so nothing below it shows.
        //
        // The ImageWriter II prints BIDIRECTIONALLY: one pin band left-to-right, feed,
        // the next right-to-left. The worker owns direction and publishes the physical
        // carriage column already mirrored around the LINE's printed width, so a short
        // text line sweeps back over its own ink, not off to the right margin. The ink
        // reveal itself is the wet-ink presented layer; the column here only drives the
        // audio (which ink the head just crossed) and the re-render change detection.
        platenRow = (std::max) (0, headRow);

        sweepLtr     = worker.IsHeadSweepLtr();
        revealRow    = (std::max) (0, worker.GetRevealBandTop());   // print frontier (change-detect)
        revealBehind = false;
        bandBottom   = (std::min) (platenRow + s_kPinBandRows - 1, rows - 1);   // viewport follows the platen

        revealCol = worker.GetCarriageCol();   // physical carriage column, over the actual ink

        {
            PrinterPreviewModel::RevealSpan   rs = PrinterPreviewModel::RevealColumnSpan (sweepLtr, revealCol);

            revealLo = rs.loDots;
            revealHi = rs.hiDots;
        }

        // Audio follows the PRESENTATION (what is seen): the platen row and the head's
        // column (which wraps to 0 each new line -- the line-feed clack). The buzz gate
        // is whether the band under the head carries ink, so a blank paper feed slews
        // silent under its own feed one-shot instead of buzzing.
        m_liveRevealRow     = platenRow;
        m_liveRevealColDots = headCol;
        m_revealInk = worker.GetSpanInkExtent (platenRow, platenRow + s_kPinBandRows - 1) > 0;

        // Keep requesting animation frames while the carriage sweeps or the paper
        // feeds (IsHeadMoving covers a host Form Feed, which does not bump activity);
        // m_printingActive holds the cadence hot across the guest's brief byte gaps.
        m_sweeping = m_printingActive || worker.IsHeadMoving();

        if (m_scene != nullptr)
        {
            // The head glyph rides the physical carriage column, which tracks the
            // sweep during a pass and PARKS where it finished between passes -- so it
            // never snaps back to the left margin during a feed (the reveal mask does
            // close to 0 there, to blank the paper scrolling in, but the carriage must
            // not follow it).
            m_scene->SetHeadColumn01 ((float) worker.GetCarriageCol() / (float) PrinterGrid::kDotsPerRow);

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
        m_panZoom.SetPanYBounds ((float) m_viewport.GetMinBottomRow(),
                                 (float) m_viewport.GetMaxBottomRow());

        if (m_viewport.IsFollowingLive())
        {
            m_panZoom.SetPanYTarget ((float) m_viewport.GetLiveRow());
        }

        // Seed the eased position onto the target on the first content frame (and
        // after a tear), so opening over a restored strip lands on the paper
        // instead of scrolling down to it from row 0.
        if (!m_panYSeeded && rows > 0)
        {
            m_panZoom.SnapPanY (m_panZoom.GetPanYTarget());
            m_panYSeeded = true;
        }

        span.lastRow  = (int) std::lround (m_panZoom.GetPanY());
        span.firstRow = (std::max) (0, span.lastRow - m_viewport.GetViewportRows() + 1);

        // The eased viewport pan lags a fast print (text catch-up), so the live band
        // can sit BELOW the snapshotted span -- there the span-sample ink gate reads
        // blank paper and drops the buzz (the missing CATALOG ink). When the head is
        // ahead of the snapshot, keep the pacing block's worker-raster gate instead.
        revealBehind = PrinterPreviewModel::IsLiveBandOutsideSpan (platenRow, span.firstRow, span.lastRow);

        moved       = PrinterPreviewModel::HasSpanMoved (span.firstRow, span.lastRow,
                                                         m_renderedSpan.firstRow, m_renderedSpan.lastRow);
        revealMoved = PrinterPreviewModel::RevealMoved (revealRow, revealCol,
                                                        m_renderedRevealRow, m_renderedRevealCol);

        // Two ways to skip a frame, sharing the force/moved override:
        //   - nothing changed at all, or
        //   - something did, but the last render was too recent to pace another
        //     (the change lands next frame).
        // As one expression: render when urgent, else only when a real change has
        // waited out the interval.
        urgent  = force || moved;
        changed = revealMoved || !m_hasRendered || (activity != m_renderedActivity);
        paced   = (nowMs - m_lastRenderMs) >= s_kMinRenderIntervalMs;

        if (urgent || (changed && paced))
        {
            if (rows <= 0)
            {
                ShowBlankSheet();
                m_revealInk = false;
            }
            else if (worker.TrySnapshotPresentedSpan (span.firstRow, span.lastRow, spanRaster))
            {
                // Ink only ever accretes in the live pin band(s) at the print frontier;
                // every row above the band that was live at the last render is final.
                // So the ink image and canvas need refreshing only from here down --
                // the rest is reused via memmove -- which keeps per-frame render cost
                // flat instead of O(page). The full-page redraw every frame (activity
                // ticks each frame, forcing a whole-span re-render + recompose) was what
                // made later rows print progressively slower as the sheet grew. A fresh
                // panel (nothing rendered yet) marks everything dirty for a full render.
                // The presented layer changed everywhere the head painted since the LAST
                // render -- normally a band or two at the platen, but a whole run of rows
                // if the UI was frozen (a modal disk picker blocks compositing while the
                // background worker keeps printing). Dirty from the last-rendered platen,
                // not just a fixed window, or those frozen-through rows keep their stale
                // pixels (an overprint that finished during the freeze reads as its first
                // pass only -- green shows as yellow). The reveal is baked into the
                // presented pixels, so RenderSpan's mask stays off (-1).
                int   dirtyFromAbs = PrinterPreviewModel::GetDirtyFromRow (m_hasRendered, platenRow, m_renderedPlaten);

                RenderSpan (spanRaster, span.firstRow, span.lastRow, dirtyFromAbs, -1, revealLo, revealHi);

                // Tell the audio whether the head passed over ink (buzz) vs blank feed
                // (silent) -- CAUGHT-UP frames only. While the reveal trails the guest,
                // the pacing block above already set m_revealInk from the live band's
                // WORKER-raster extent: this span snapshot cannot serve there, because
                // at catch-up speed the reveal races ahead of the EASED viewport pan,
                // the live band falls outside the snapshot, and GetCell reads blank --
                // the missing CATALOG buzz. Caught up, the span IS the live region, so
                // sample the FULL column span the head swept SINCE THE LAST FRAME, not
                // a fixed lookback: at carriage speed the head advances more dots per
                // frame than a short window is wide, so a thin feature crossed between
                // two frames (a border edge, a lone glyph) used to fall between the
                // samples and drop the buzz. A small bridge behind the leading edge
                // keeps a word buzzing across the blank gaps between glyphs (the audio
                // hold bridges the rest). On a line wrap the column jumps margin to
                // margin -- no contiguous swept span exists, so sample the entire band:
                // any ink means the pass the head just ran is a printing pass (buzz);
                // a blank band (line / form feed) stays silent.
                if (!revealBehind)
                {
                    PrinterPreviewModel::InkSample   sample = PrinterPreviewModel::GetAudioSampleWindow (
                        sweepLtr, m_renderedRevealCol, revealCol, revealRow, m_renderedRevealRow);

                    // Sample where the HEAD physically is (platenRow), not the reveal
                    // frontier: an overprint pass (Print Shop lays a color band L>R then
                    // re-strikes the SAME row R>L in the next primary) sits at row R while
                    // the monotonic frontier already advanced a band past it, so sampling
                    // the frontier would read the blank row below and drop the buzz.
                    m_revealInk = PrinterPreviewModel::HasBandInk (spanRaster, span.firstRow, platenRow,
                                                                   sample.loCol, sample.hiCol);
                }
            }
            else
            {
                ShowBlankSheet();   // no active job: fresh paper in the platen
                m_revealInk = false;
            }

            m_renderedSpan      = span;
            m_renderedActivity  = activity;
            m_renderedRows      = rows;
            m_renderedRevealRow = revealRow;
            m_renderedRevealCol = revealCol;
            m_renderedPlaten    = platenRow;
            m_lastRenderMs      = nowMs;
            m_hasRendered       = true;
            Invalidate();
        }
    }
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
    int                     rows = raster.GetRowsUsed();
    PrinterViewport::Span   span;
    PrintRaster             spanRaster;



    // Same no-paper guard as RefreshLive: nothing to render onto yet.
    if (m_paper != nullptr)
    {
        // A shorter raster than last time means the strip was replaced, not
        // appended to, so the viewport's live row no longer exists.
        if (rows - 1 < m_viewport.GetLiveRow())
        {
            m_viewport.Reset();
        }

        if (rows <= 0)
        {
            ShowBlankSheet();
        }
        else
        {
            m_viewport.Advance (rows - 1);

            // One-shot push: place panZoom's eased position on the follow
            // target (no glide) and render that bottom-anchored span.
            m_panZoom.SetPanYBounds ((float) m_viewport.GetMinBottomRow(), (float) m_viewport.GetMaxBottomRow());

            if (m_viewport.IsFollowingLive())
            {
                m_panZoom.SetPanYTarget ((float) m_viewport.GetLiveRow());
            }

            m_panZoom.SnapPanY (m_panZoom.GetPanYTarget());
            m_panYSeeded = true;

            span.lastRow  = (int) std::lround (m_panZoom.GetPanY());
            span.firstRow = (std::max) (0, span.lastRow - m_viewport.GetViewportRows() + 1);
            raster.CopyRowSpan (span.firstRow, span.lastRow, spanRaster);
            RenderSpan (spanRaster, span.firstRow, span.lastRow, -1, -1, 0, 0);   // dirtyFromAbs -1: full render, no live head

            m_renderedSpan = span;
            m_renderedRows = rows;
        }

        // Both paths rendered something, blank sheet included.
        m_hasRendered = true;
    }
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
    ComposeCanvas (nullptr, 0, 0, -1, 0, 0, -1);
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
//  scrolling shifts the window over UNCHANGED content and only the newly
//  exposed rows are rendered (the rest memmove within the cache), while ink
//  still accreting in the live band re-renders only from `dirtyFromAbs` down
//  -- rows above are final. A span that stops lining up, or `dirtyFromAbs` at
//  or above the span top (e.g. -1), rebuilds the whole image.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::RenderSpan (const PrintRaster & spanRaster, int firstAbsRow, int lastAbsRow,
                               int dirtyFromAbs, int revealBandTopAbs, int revealLoDots, int revealHiDots)
{
    HRESULT                  hr       = S_OK;
    int                      spanRows = lastAbsRow - firstAbsRow + 1;
    bool                     haveImg  = false;
    bool                     composes = true;
    size_t                   rowBytes = 0;
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;



    if (spanRows <= 0)
    {
        // Degenerate span: blank paper, furniture still tracking the scroll.
        // This composes its own frame, so the one at the bottom is skipped.
        m_spanImgValid = false;
        ComposeCanvas (nullptr, 0, lastAbsRow, -1, 0, 0, -1);
        composes = false;
    }
    else
    {
        opt.outputDpi = s_kPreviewDpi;
        opt.style     = DotStyle::Ink;
        rowBytes      = (size_t) m_spanImg.width * 4;   // previous frame's width, deliberately

        // Incremental update, keyed by absolute row. The cache already holds the
        // ink for every row still on-screen; only two kinds of row need work: those
        // scrolled newly into view, and the live pin band at the frontier
        // (`dirtyFromAbs` down) whose ink is still accreting. Everything above is
        // final and reused via memmove, so the per-frame render stays flat instead
        // of O(page) -- the whole-span redraw every frame is what made later rows
        // print progressively slower. `dirtyFromAbs` at/above the span top (e.g. -1)
        // marks the whole span dirty, collapsing to a full re-render.
        if (m_spanImgValid && m_spanImg.height == spanRows)
        {
            int   delta = firstAbsRow - m_spanImgFirstAbsRow;   // + = scrolled toward live

            if (delta >= 0 && delta < spanRows)
            {
                int  dirtyFirst   = 0;
                int  exposedFirst = 0;
                int  renderFirst  = 0;

                if (delta > 0)
                {
                    memmove (m_spanImg.GetPixel (0, 0), m_spanImg.GetPixel (0, delta),
                             rowBytes * (spanRows - delta));   // shift retained rows up
                }

                dirtyFirst = (dirtyFromAbs > firstAbsRow) ? (dirtyFromAbs - firstAbsRow) : 0;
                exposedFirst = spanRows - delta; // rows scrolled into view (spanRows when delta==0)
                renderFirst = (std::min) (dirtyFirst, exposedFirst);

                if (renderFirst >= spanRows)
                {
                    // Reveal-only frame (no scroll, nothing accreting on-screen):
                    // the cached ink is current; ComposeCanvas sweeps the reveal.
                    m_spanImgFirstAbsRow = firstAbsRow;
                    haveImg              = true;
                }
                else
                {
                    int         tailRows = spanRows - renderFirst;
                    RgbaImage   tail;

                    hr = renderer.Render (spanRaster, renderFirst, spanRows - 1, opt, tail);

                    if (SUCCEEDED (hr) && tail.width == m_spanImg.width && tail.height == tailRows)
                    {
                        memcpy (m_spanImg.GetPixel (0, renderFirst), tail.GetPixel (0, 0), rowBytes * tailRows);
                        m_spanImgFirstAbsRow = firstAbsRow;
                        haveImg              = true;
                    }
                }
            }
            else if (delta < 0 && -delta < spanRows && dirtyFromAbs > lastAbsRow)
            {
                // Pure scroll back over finished content (user scrolled up; the
                // accreting frontier is below the view, so nothing on-screen
                // changes). Shift down and render the newly exposed top edge.
                int         newCount = -delta;
                RgbaImage   edge;

                memmove (m_spanImg.GetPixel (0, newCount), m_spanImg.GetPixel (0, 0), rowBytes * (spanRows - newCount));

                hr = renderer.Render (spanRaster, 0, newCount - 1, opt, edge);

                if (SUCCEEDED (hr) && edge.width == m_spanImg.width && edge.height == newCount)
                {
                    memcpy (m_spanImg.GetPixel (0, 0), edge.GetPixel (0, 0), rowBytes * newCount);
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
                composes       = false;   // keep the previous frame rather than flash a bad one
            }
            else
            {
                m_spanImgFirstAbsRow = firstAbsRow;
                m_spanImgValid       = true;
                m_spanImgGen++;      // full rebuild: force the canvas cache to rebuild wholesale
            }
        }

        // Span larger than the canvas: also keep the previous frame.
        if (m_spanImg.height > m_viewport.GetViewportRows())
        {
            composes = false;
        }

        if (composes)
        {
            ComposeCanvas (&m_spanImg, firstAbsRow, lastAbsRow, revealBandTopAbs, revealLoDots, revealHiDots, dirtyFromAbs);
        }
    }
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
                                  int revealBandTopAbs, int revealLoDots, int revealHiDots, int contentDirtyFromAbs)
{
    HRESULT  hr        = S_OK;
    int      canvasW   = s_kStockWidthPx;
    int      canvasH   = m_viewport.GetViewportRows();   // px == rows at 144 dpi
    int      topAbsRow = bottomAbsRow - canvasH + 1;   // canvas bottom = span's live row
    int      holeR     = s_kHoleRadiusPx;
    int      delta     = 0;



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
                uint32_t    * dst    = nullptr;
                const Byte  * src    = nullptr;
                int           xStart = 0;
                int           xEnd   = 0;

                if (yTop + y < rowFirst || yTop + y > rowLast)
                {
                    continue;
                }

                dst = &m_canvas[(size_t) (yTop + y) * canvasW + s_kContentXPx];
                src = content->GetPixel (0, y);
                xEnd = content->width;

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

    // Fast path: the SAME full-height content still sits aligned under the
    // canvas (no strip-start clamp, no wholesale ink re-render). Everything
    // that can change this frame lives in the BOTTOM rows -- the scroll-exposed
    // edge, the reveal band (old + new position), and the accreting ink tail
    // (`contentDirtyFromAbs` down) -- so shift the retained rows and rebuild
    // just the union of those. Rows above are reused. A whole-canvas rebuild
    // every frame is what made later rows of a tall page compose ever slower.
    bool   aligned = m_canvasValid
                     && content != nullptr && m_canvasHasContent
                     && m_canvasSpanGen == m_spanImgGen
                     && content->height == canvasH
                     && contentFirstAbsRow == topAbsRow;
    delta = topAbsRow - m_canvasTopAbs;

    if (aligned && std::abs (delta) < canvasH)
    {
        size_t  rowBytes     = (size_t) canvasW * 4;
        int     oldRevealTop = 0;
        int     newRevealTop = 0;
        int     contentTop   = 0;
        int     rebuildTop   = 0;

        if (delta > 0)
        {
            memmove (m_canvas.data(), m_canvas.data() + (size_t) delta * canvasW, rowBytes * (canvasH - delta));
        }
        else if (delta < 0)
        {
            memmove (m_canvas.data() + (size_t) (-delta) * canvasW, m_canvas.data(), rowBytes * (canvasH + delta));
        }

        // Union of the dirty regions, all anchored at the canvas bottom.
        oldRevealTop = (m_canvasRevealTop >= 0) ? m_canvasRevealTop - topAbsRow : canvasH;
        newRevealTop = (revealBandTopAbs  >= 0) ? revealBandTopAbs  - topAbsRow : canvasH;
        contentTop = (contentDirtyFromAbs > topAbsRow) ? (contentDirtyFromAbs - topAbsRow) : 0;
        rebuildTop = (std::min) (canvasH - 1, (std::min) (oldRevealTop, newRevealTop));

        rebuildTop = (std::min) (rebuildTop, contentTop);

        if (delta > 0)
        {
            rebuildTop = (std::min) (rebuildTop, canvasH - delta - holeR - 1);
        }

        RebuildRows (rebuildTop, canvasH - 1);

        if (delta < 0)
        {
            RebuildRows (0, -delta + holeR);   // scroll-up exposes the top edge too
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
        hr = m_scene->SetContent (m_canvas.data(), canvasW, canvasH);
        IGNORE_RETURN_VALUE (hr, S_OK);
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
    bool  isMove    = (ev.kind == DxuiMouseEventKind::Move);
    bool  isDown    = (ev.kind == DxuiMouseEventKind::Down);
    bool  handled   = false;
    bool  isPaperLb = isDown
                      && ev.button == DxuiMouseButton::Left
                      && IsPaperHit (ev.positionDip.x, ev.positionDip.y);



    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        // Wheel gestures (scroll / pan / zoom) always belong to the controller.
        handled = m_panZoom.OnMouse (ev);
    }
    else if (isPaperLb)
    {
        // A left-press on the paper begins a pan-drag; presses on the toolbar
        // fall through so the buttons get their clicks.
        m_tooltip.RequestHide (GetNowMs());
        m_panZoom.OnMouse (ev);
        handled = true;
    }
    else if (isMove || ev.kind == DxuiMouseEventKind::Up)
    {
        // Continue / end an active drag. OnMouse returns true only WHILE a drag
        // is in progress, so a plain hover move falls through to the tooltip.
        handled = m_panZoom.OnMouse (ev);
    }

    if (!handled)
    {
        if (isMove)
        {
            UpdateTooltip (ev.positionDip.x, ev.positionDip.y);
        }
        else if (isDown)
        {
            m_tooltip.RequestHide (GetNowMs());
        }

        handled = DxuiWindow::OnMouse (ev);
    }

    return handled;
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
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        // Ctrl +/=, Ctrl -, Ctrl 0 -> zoom in / out / reset.
        handled = ev.ctrl && m_panZoom.OnKey (ev);
    }

    if (!handled && ev.kind == DxuiKeyEventKind::Down)
    {
        handled = true;   // every case below claims the key; default takes it back

        switch (ev.vk)
        {
            case VK_ESCAPE:
                Hide();
                break;

            case VK_UP:
                m_panZoom.PanByUser (0.0f, -(float) s_kArrowScrollRows);
                break;

            case VK_DOWN:
                m_panZoom.PanByUser (0.0f, +(float) s_kArrowScrollRows);
                break;

            case VK_PRIOR:
                m_panZoom.PanByUser (0.0f, -(float) PrinterGrid::kPageRows);
                break;

            case VK_NEXT:
                m_panZoom.PanByUser (0.0f, +(float) PrinterGrid::kPageRows);
                break;

            default:
                handled = false;
                break;
        }
    }

    // Anything we did not claim fans out to the child controls.
    if (!handled)
    {
        handled = DxuiWindow::OnKey (ev);
    }

    return handled;
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
    int  pad        = scaler.ToPx (kPadDip);
    int  gap        = scaler.ToPx (6);
    int  captionH   = GetCaptionHeightPx();
    int  toolbarH   = scaler.ToPx (kToolbarHDip);
    int  topBandTop = 0;
    int  topBy      = 0;
    int  botBandTop = 0;
    int  botBy      = 0;
    // Two lines' worth. The hint wraps rather than clipping (see Paint), so the
    // strip has to reserve the second line up front -- Layout has no text
    // renderer to measure with, and sizing it per-frame would make the paper
    // area jump as the window crosses the wrap threshold. A single line simply
    // centers in the taller box.
    int  hintH      = scaler.ToPx (kHintHDip);
    int  btnH       = scaler.ToPx (30);
    int  btnW       = scaler.ToPx (84);   // Print... / Save... / Copy / Form Feed / Discard
    int  zoomW      = scaler.ToPx (42);   // [-] and [+]
    int  zoomResetW = scaler.ToPx (54);   // [nnn%]



    // Our override replaces DxuiPanel::Layout, which would otherwise record the
    // panel's own bounds; set them so Paint's backdrop fills the whole client.
    SetBounds (boundsDip);

    topBandTop = boundsDip.top + captionH;
    topBy = topBandTop + (toolbarH - btnH) / 2;
    botBandTop = boundsDip.bottom - toolbarH;
    botBy = botBandTop + (toolbarH - btnH) / 2;

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

    m_hintFontPx = scaler.ToPxf (11.0f);
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
    RECT     b  = GetBounds();



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

        hr = text.DrawString (
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
// Wrap: the hint must never be the thing that sets the window's
// minimum width. The strip is laid out two lines tall, so a narrow
// panel spills onto the second line instead of losing both ends,
// and a wide one centers a single line in the same box.
true);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
