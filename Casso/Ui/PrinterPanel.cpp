#include "Pch.h"

#include "PrinterPanel.h"

#include "CassoTheme.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/RgbaImage.h"
#include "Print/PrinterWorker.h"




namespace
{
    constexpr wchar_t   s_kpszTitle     [] = L"Casso Printer";
    constexpr wchar_t   s_kpszClassName [] = L"CassoPrinterPanel";

    constexpr int       s_kPreferredWidthDip  = 560;
    constexpr int       s_kPreferredHeightDip = 680;

    // Viewport render: 144 dpi maps native rows 1:1 to pixels, so the visible
    // ~1-page span is a fixed 1152x1584 image regardless of strip length --
    // bounded memory, stable scale-to-fit, and delta-friendly row alignment.
    constexpr int       s_kPreviewDpi = PrinterGrid::kRowsPerInch;

    // Minimum interval between live re-renders while bytes stream (~60 Hz).
    // Viewport motion (scroll / snap) bypasses it so input feels immediate.
    constexpr int64_t   s_kMinRenderIntervalMs = 16;

    // Scroll step sizes in native rows (144 rows/inch).
    constexpr int       s_kWheelRowsPerNotch = 96;    // 2/3" per wheel notch
    constexpr int       s_kArrowScrollRows   = 48;    // 1/3" per key press

    constexpr wchar_t   s_kpszScrollHint [] =
        L"Scroll wheel or Up/Down to review earlier pages \u2022 snaps back to the live row when idle";
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

    if (IsCreated ())
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

    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetTheme (m_theme);
    Show ();

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::OnCreate ()
{
    m_paper   = CreateChild<PrinterPaperView> ();
    m_finish  = CreateChild<DxuiButton> (L"Finish");
    m_copy    = CreateChild<DxuiButton> (L"Copy");
    m_discard = CreateChild<DxuiButton> (L"Discard");
    m_refresh = CreateChild<DxuiButton> (L"Refresh");

    m_finish->SetOnClick  ([this] () { if (m_onFinish)  { m_onFinish  (); } });
    m_copy->SetOnClick    ([this] () { if (m_onCopy)    { m_onCopy    (); } });
    m_discard->SetOnClick ([this] () { if (m_onDiscard) { m_onDiscard (); } });
    m_refresh->SetOnClick ([this] () { if (m_onRefresh) { m_onRefresh (); } });
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

HRESULT PrinterPanel::RenderFrame ()
{
    if (!IsCreated ())
    {
        return S_OK;
    }

    Invalidate ();
    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::NowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t PrinterPanel::NowMs ()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now ().time_since_epoch ()).count ();
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
    int                     rows     = worker.RowsUsed ();
    uint64_t                activity = worker.ActivityCount ();
    PrinterViewport::Span   span;
    bool                    moved    = false;
    PrintRaster             spanRaster;

    if (m_paper == nullptr)
    {
        return;
    }

    // A shrunk strip means eject/discard tore the paper off: rewind the view
    // to the fresh sheet instead of staring past its end.
    if (rows - 1 < m_viewport.LiveRow ())
    {
        m_viewport.Reset ();
    }

    if (rows > 0)
    {
        m_viewport.Advance (rows - 1);
    }
    m_viewport.Tick (nowMs);

    span  = m_viewport.VisibleSpan ();
    moved = (span.firstRow != m_renderedSpan.firstRow || span.lastRow != m_renderedSpan.lastRow);

    if (!force && !moved && m_hasRendered && activity == m_renderedActivity)
    {
        return;   // nothing changed: zero work this frame
    }

    if (!force && !moved && nowMs - m_lastRenderMs < s_kMinRenderIntervalMs)
    {
        return;   // pace streaming re-renders; the change lands next frame
    }

    if (rows <= 0)
    {
        ShowBlankSheet ();
    }
    else if (worker.SnapshotStripSpan (span.firstRow, span.lastRow, spanRaster))
    {
        RenderSpan (spanRaster);
    }
    else
    {
        ShowBlankSheet ();   // no active job: fresh paper in the platen
    }

    m_renderedSpan     = span;
    m_renderedActivity = activity;
    m_lastRenderMs     = nowMs;
    m_hasRendered      = true;
    Invalidate ();
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
    int                     rows = raster.RowsUsed ();
    PrinterViewport::Span   span;
    PrintRaster             spanRaster;

    if (m_paper == nullptr)
    {
        return;
    }

    if (rows - 1 < m_viewport.LiveRow ())
    {
        m_viewport.Reset ();
    }

    if (rows <= 0)
    {
        ShowBlankSheet ();
        m_hasRendered = true;
        return;
    }

    m_viewport.Advance (rows - 1);

    span = m_viewport.VisibleSpan ();
    raster.CopyRowSpan (span.firstRow, span.lastRow, spanRaster);
    RenderSpan (spanRaster);

    m_renderedSpan = span;
    m_hasRendered  = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::ShowBlankSheet
//
//  A blank sheet rather than an empty window, so the preview always reads as
//  "paper loaded, nothing printed yet." Full viewport height at the fixed
//  preview dpi keeps the image dimensions identical to the printed case.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::ShowBlankSheet ()
{
    int                     w = 8 * s_kPreviewDpi;
    int                     h = m_viewport.ViewportRows ();   // 1 native row == 1 px at 144 dpi
    std::vector<uint32_t>   blank ((size_t) w * h, 0xFFFFFFFFu);

    m_paper->SetImage (std::move (blank), w, h);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RenderSpan
//
//  Render the (rebased) span raster and bottom-anchor it on a fixed
//  full-viewport canvas: the live row sits at the bottom -- where the platen
//  will be -- and paper white fills upward until content has fed that far.
//  Constant canvas size = stable texture and scale-to-fit (no zoom jumps).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::RenderSpan (const PrintRaster & spanRaster)
{
    HRESULT                  hr       = S_OK;
    int                      rows     = spanRaster.RowsUsed ();
    int                      canvasH  = m_viewport.ViewportRows ();   // px == rows at 144 dpi
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;
    RgbaImage                img;
    std::vector<uint32_t>    bgra;
    size_t                   dstBase  = 0;

    if (rows <= 0)
    {
        ShowBlankSheet ();
        return;
    }

    opt.outputDpi = s_kPreviewDpi;
    opt.style     = DotStyle::Ink;

    hr = renderer.Render (spanRaster, 0, rows - 1, opt, img);

    if (FAILED (hr) || img.width <= 0 || img.height <= 0 || img.height > canvasH)
    {
        return;   // keep the previous frame rather than flash a bad one
    }

    bgra.assign ((size_t) img.width * canvasH, 0xFFFFFFFFu);   // paper white
    dstBase = (size_t) (canvasH - img.height) * img.width;     // bottom-anchor

    for (size_t i = 0; i < (size_t) img.width * img.height; i++)
    {
        uint32_t  r = img.rgba[i * 4 + 0];
        uint32_t  g = img.rgba[i * 4 + 1];
        uint32_t  b = img.rgba[i * 4 + 2];
        uint32_t  a = img.rgba[i * 4 + 3];

        // Premultiply so the GPU blit composites correctly (paper is opaque, so
        // this is a no-op there, but the margins/anti-aliased dots carry alpha).
        uint32_t  pr = r * a / 255;
        uint32_t  pg = g * a / 255;
        uint32_t  pb = b * a / 255;

        bgra[dstBase + i] = (a << 24) | (pr << 16) | (pg << 8) | pb;
    }

    m_paper->SetImage (std::move (bgra), img.width, canvasH);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnMouse
//
//  Vertical wheel scrolls the viewport (wheel up = back toward earlier pages,
//  like every scrolling surface); everything else -- including the toolbar
//  buttons' clicks -- flows through the base dispatch untouched.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && ev.wheelDelta != 0.0f)
    {
        m_viewport.Scroll ((int) (-ev.wheelDelta * s_kWheelRowsPerNotch), NowMs ());
        return true;   // next RefreshLive renders the moved span immediately
    }

    return DxuiWindow::OnMouse (ev);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnKey
//
//  Up/Down and PageUp/PageDown scroll the viewport (FR-033); Escape hides the
//  preview (its close-box does the same). Everything else falls through to
//  the base, which fans the key to the child controls.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Down)
    {
        switch (ev.vk)
        {
            case VK_ESCAPE:
                Hide ();
                return true;

            case VK_UP:
                m_viewport.Scroll (-s_kArrowScrollRows, NowMs ());
                return true;

            case VK_DOWN:
                m_viewport.Scroll (+s_kArrowScrollRows, NowMs ());
                return true;

            case VK_PRIOR:
                m_viewport.Scroll (-PrinterGrid::kPageRows, NowMs ());
                return true;

            case VK_NEXT:
                m_viewport.Scroll (+PrinterGrid::kPageRows, NowMs ());
                return true;
        }
    }

    return DxuiWindow::OnKey (ev);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Layout
//
//  Paper view fills the client above a hint strip and bottom toolbar; Finish /
//  Copy / Discard run left-to-right and Refresh anchors to the right.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.Px (10);
    int   captionH = CaptionHeightPx ();
    int   toolbarH = scaler.Px (46);
    int   hintH    = scaler.Px (20);
    int   btnH     = scaler.Px (30);
    int   btnW     = scaler.Px (92);
    int   by       = boundsDip.bottom - toolbarH + (toolbarH - btnH) / 2;
    int   bx       = boundsDip.left + pad;

    // Our override replaces DxuiPanel::Layout, which would otherwise record the
    // panel's own bounds; set them so Paint's backdrop fills the whole client.
    SetBounds (boundsDip);

    m_hintFontPx = scaler.Pxf (11.0f);
    m_hintRect   = { boundsDip.left + pad,
                     boundsDip.bottom - toolbarH - hintH,
                     boundsDip.right - pad,
                     boundsDip.bottom - toolbarH };

    if (m_paper != nullptr)
    {
        // Reserve the caption band at the top (the content root spans the full
        // client, so without this the paper would draw up over the title bar).
        RECT  paperR = { boundsDip.left + pad, boundsDip.top + captionH + pad,
                         boundsDip.right - pad, boundsDip.bottom - toolbarH - hintH };
        m_paper->Layout (paperR, scaler);
    }

    for (DxuiButton * btn : { m_finish, m_copy, m_discard })
    {
        if (btn != nullptr)
        {
            RECT  r = { bx, by, bx + btnW, by + btnH };
            btn->Layout (r, scaler);
            bx += btnW + pad;
        }
    }

    if (m_refresh != nullptr)
    {
        RECT  r = { boundsDip.right - pad - btnW, by, boundsDip.right - pad, by + btnH };
        m_refresh->Layout (r, scaler);
    }
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
    RECT     b  = Bounds ();

    painter.FillRect ((float) b.left,
                      (float) b.top,
                      (float) (b.right  - b.left),
                      (float) (b.bottom - b.top),
                      0xFF33363B);

    DxuiPanel::Paint (painter, text, theme);

    {
        DxuiFontHandle  bf = theme.BodyFont ();

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
