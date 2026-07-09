#include "Pch.h"

#include "PrinterPanel.h"

#include "CassoTheme.h"
#include "Render/IDxuiPainter.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/RgbaImage.h"




namespace
{
    constexpr wchar_t   s_kpszTitle     [] = L"Casso Printer";
    constexpr wchar_t   s_kpszClassName [] = L"CassoPrinterPanel";

    constexpr int       s_kPreferredWidthDip  = 560;
    constexpr int       s_kPreferredHeightDip = 680;

    // Preview render: cap the rendered strip height so a long fanfold banner
    // stays within GPU texture limits (the paper view scales it down to fit
    // the window anyway).
    constexpr int       s_kPreviewMaxHeightPx = 8000;
    constexpr int       s_kPreviewDpi         = 120;
    constexpr int       s_kNativeRowsPerInch  = 144;
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
//  PrinterPanel::Layout
//
//  Paper view fills the client above a bottom toolbar; Finish / Copy / Discard
//  run left-to-right and Refresh anchors to the right.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.Px (10);
    int   toolbarH = scaler.Px (46);
    int   btnH     = scaler.Px (30);
    int   btnW     = scaler.Px (92);
    int   by       = boundsDip.bottom - toolbarH + (toolbarH - btnH) / 2;
    int   bx       = boundsDip.left + pad;

    // Our override replaces DxuiPanel::Layout, which would otherwise record the
    // panel's own bounds; set them so Paint's backdrop fills the whole client.
    SetBounds (boundsDip);

    if (m_paper != nullptr)
    {
        RECT  paperR = { boundsDip.left + pad, boundsDip.top + pad,
                         boundsDip.right - pad, boundsDip.bottom - toolbarH };
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
//  Fill the client with the device-bezel backdrop, then let the base pump paint
//  the paper view and toolbar buttons on top. Without an explicit backdrop the
//  toolbar band would show whatever the swap chain last cleared to.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  b = Bounds ();

    painter.FillRect ((float) b.left,
                      (float) b.top,
                      (float) (b.right  - b.left),
                      (float) (b.bottom - b.top),
                      0xFF33363B);

    DxuiPanel::Paint (painter, text, theme);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::SetStrip
//
//  Render the strip (PaperRenderer -- core) at a capped preview DPI, convert to
//  premultiplied BGRA, and hand it to the paper view. Empty strip clears it.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::SetStrip (const PrintRaster & raster)
{
    HRESULT                  hr    = S_OK;
    int                      rows  = raster.RowsUsed ();
    int                      dpi   = s_kPreviewDpi;
    long long                outH  = 0;
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;
    RgbaImage                img;
    std::vector<uint32_t>    bgra;

    if (m_paper == nullptr)
    {
        return;
    }

    if (rows <= 0)
    {
        m_paper->Clear ();
        return;
    }

    // Scale the render DPI down so the whole strip fits a safe texture height.
    outH = (long long) rows * dpi / s_kNativeRowsPerInch;

    if (outH > s_kPreviewMaxHeightPx)
    {
        dpi = (int) ((long long) s_kPreviewMaxHeightPx * s_kNativeRowsPerInch / rows);
    }

    if (dpi < 8)   { dpi = 8; }
    if (dpi > 144) { dpi = 144; }

    opt.outputDpi = dpi;
    opt.style     = DotStyle::Ink;

    hr = renderer.Render (raster, 0, rows - 1, opt, img);

    if (FAILED (hr) || img.width <= 0 || img.height <= 0)
    {
        m_paper->Clear ();
        return;
    }

    bgra.resize ((size_t) img.width * img.height);

    for (size_t i = 0; i < bgra.size (); i++)
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

        bgra[i] = (a << 24) | (pr << 16) | (pg << 8) | pb;
    }

    m_paper->SetImage (std::move (bgra), img.width, img.height);
}
