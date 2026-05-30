#include "Pch.h"

#include "DialogPrimitiveRenderer.h"
#include "../Chrome/ChromeTheme.h"
#include "../WindowsThemeColors.h"
#include "../../resource.h"


static constexpr UINT      s_kSwapBufferCount        = 2;
static constexpr float     s_kBodyFontDp              = 13.0f;
static constexpr float     s_kTitleFontDp             = 13.0f;
static constexpr float     s_kTitlePaddingDp          = 12.0f;
static constexpr float     s_kUnderlineHeightPx       = 1.0f;
static constexpr float     s_kIconRadiusFraction      = 0.40f;
static constexpr float     s_kCenterDivisor           = 2.0f;
static constexpr float     s_kBorderThicknessPx       = 1.0f;
static constexpr float     s_kTitleSeparatorHeightPx  = 1.0f;
static constexpr float     s_kClearR                  = 0.0f;
static constexpr float     s_kClearG                  = 0.0f;
static constexpr float     s_kClearB                  = 0.0f;
static constexpr float     s_kClearA                  = 1.0f;
static constexpr uint32_t  s_kIconColorInfo           = 0xFF4A9EDB;
static constexpr uint32_t  s_kIconColorWarning        = 0xFFF5A623;
static constexpr uint32_t  s_kIconColorError          = 0xFFE5424D;
static constexpr int       s_kBgraBitCount            = 32;
static constexpr int       s_kAlphaShift              = 24;
static constexpr int       s_kRedShift                = 16;
static constexpr int       s_kGreenShift              = 8;
static constexpr int       s_kByteMask                = 0xFF;
static constexpr int       s_kByteMax                 = 255;
static constexpr float     s_kCloseButtonWidthDp      = 46.0f;
static constexpr float     s_kCloseGlyphFontDp        = 10.0f;
static constexpr wchar_t   s_kpszCloseGlyph[]         = L"\xE8BB";
static constexpr wchar_t   s_kpszMdl2Family[]         = L"Segoe MDL2 Assets";




////////////////////////////////////////////////////////////////////////////////
//
//  LoadIconAsPremulBgra
//
//  Local copy of the SettingsWindow/EmulatorShell helper. Loads an
//  HICON resource into a CPU-side premultiplied BGRA8 pixel buffer
//  suitable for DwriteTextRenderer::DrawIconBitmap.
//
////////////////////////////////////////////////////////////////////////////////

static bool LoadIconAsPremulBgra (
    HINSTANCE               hInstance,
    int                     iconResourceId,
    int                     sizePx,
    std::vector<uint32_t> & outPixels,
    int                   & outW,
    int                   & outH)
{
    HICON       hIcon       = nullptr;
    HDC         screenDc    = nullptr;
    HDC         memDc       = nullptr;
    HBITMAP     dib         = nullptr;
    HBITMAP     oldBitmap   = nullptr;
    void      * dibBits     = nullptr;
    BITMAPINFO  bmi         = {};
    bool        success     = false;
    size_t      pixelCount  = (size_t) sizePx * (size_t) sizePx;
    size_t      i           = 0;



    hIcon = (HICON) LoadImageW (hInstance,
                                MAKEINTRESOURCEW (iconResourceId),
                                IMAGE_ICON,
                                sizePx, sizePx,
                                LR_DEFAULTCOLOR);
    if (hIcon == nullptr)
    {
        return false;
    }

    screenDc = GetDC (nullptr);
    memDc    = CreateCompatibleDC (screenDc);

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sizePx;
    bmi.bmiHeader.biHeight      = -sizePx;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = s_kBgraBitCount;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib = CreateDIBSection (memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);

    if (dib != nullptr && dibBits != nullptr)
    {
        oldBitmap = (HBITMAP) SelectObject (memDc, dib);
        memset (dibBits, 0, pixelCount * sizeof (uint32_t));

        if (DrawIconEx (memDc, 0, 0, hIcon, sizePx, sizePx, 0, nullptr, DI_NORMAL))
        {
            uint32_t  * src = (uint32_t *) dibBits;

            outPixels.assign (pixelCount, 0);

            for (i = 0; i < pixelCount; i++)
            {
                uint32_t  px = src[i];
                uint8_t   a  = (uint8_t) ((px >> s_kAlphaShift) & s_kByteMask);
                uint8_t   r  = (uint8_t) ((px >> s_kRedShift)   & s_kByteMask);
                uint8_t   g  = (uint8_t) ((px >> s_kGreenShift) & s_kByteMask);
                uint8_t   b  = (uint8_t) ( px                   & s_kByteMask);

                r = (uint8_t) ((r * a) / s_kByteMax);
                g = (uint8_t) ((g * a) / s_kByteMax);
                b = (uint8_t) ((b * a) / s_kByteMax);

                outPixels[i] = ((uint32_t) a << s_kAlphaShift) |
                               ((uint32_t) r << s_kRedShift)   |
                               ((uint32_t) g << s_kGreenShift) |
                                (uint32_t) b;
            }

            outW    = sizePx;
            outH    = sizePx;
            success = true;
        }

        SelectObject (memDc, oldBitmap);
    }

    if (dib != nullptr)      { DeleteObject (dib); }
    if (memDc != nullptr)    { DeleteDC (memDc); }
    if (screenDc != nullptr) { ReleaseDC (nullptr, screenDc); }
    DestroyIcon (hIcon);

    return success;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ~DialogPrimitiveRenderer
//
////////////////////////////////////////////////////////////////////////////////

DialogPrimitiveRenderer::~DialogPrimitiveRenderer()
{
    Shutdown();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Creates a non-DComp swap chain (CreateSwapChainForHwnd, alpha mode
//  IGNORE), render target view, geometry painter, and text renderer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitiveRenderer::Initialize (
    HWND                   hwnd,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    int                    widthPx,
    int                    heightPx,
    UINT                   dpi)
{
    HRESULT                hr          = S_OK;
    ComPtr<IDXGIDevice>    dxgiDevice;
    ComPtr<IDXGIAdapter>   dxgiAdapter;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1  desc        = {};



    CBRAEx (hwnd,    E_INVALIDARG);
    CBRAEx (device,  E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

    m_hwnd     = hwnd;
    m_device   = device;
    m_context  = context;
    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_scaler.SetDpi (dpi);

    hr = m_painter.Initialize (m_device, m_context);
    CHRA (hr);

    hr = m_text.Initialize (m_device);
    CHRA (hr);

    hr = m_device->QueryInterface (IID_PPV_ARGS (&dxgiDevice));
    CHRA (hr);

    hr = dxgiDevice->GetAdapter (&dxgiAdapter);
    CHRA (hr);

    hr = dxgiAdapter->GetParent (IID_PPV_ARGS (&dxgiFactory));
    CHRA (hr);

    desc.Width            = static_cast<UINT> (m_widthPx);
    desc.Height           = static_cast<UINT> (m_heightPx);
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo           = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = s_kSwapBufferCount;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags            = 0;

    hr = dxgiFactory->CreateSwapChainForHwnd (m_device,
                                              m_hwnd,
                                              &desc,
                                              nullptr,
                                              nullptr,
                                              &m_swapChain);
    CHRA (hr);

    hr = CreateBackBufferTarget();
    CHRA (hr);

    m_initialized = true;

Error:
    if (FAILED (hr))
    {
        Shutdown();
    }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::Shutdown()
{
    m_text.Shutdown();
    m_painter.Shutdown();

    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets (0, nullptr, nullptr);
    }

    m_rtv.Reset();
    m_swapChain.Reset();

    m_appIconPixels.clear();
    m_appIconW     = 0;
    m_appIconH     = 0;
    m_appIconResId = 0;

    m_context     = nullptr;
    m_device      = nullptr;
    m_hwnd        = nullptr;
    m_widthPx     = 0;
    m_heightPx    = 0;
    m_initialized = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Resize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitiveRenderer::Resize (int widthPx, int heightPx, UINT dpi)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_initialized || m_swapChain == nullptr, S_OK);
    BAIL_OUT_IF (widthPx <= 0 || heightPx <= 0, S_OK);

    m_widthPx  = widthPx;
    m_heightPx = heightPx;
    m_scaler.SetDpi (dpi);

    m_text.UnbindBackBuffer();
    m_context->OMSetRenderTargets (0, nullptr, nullptr);
    m_rtv.Reset();

    hr = m_swapChain->ResizeBuffers (s_kSwapBufferCount,
                                     static_cast<UINT> (m_widthPx),
                                     static_cast<UINT> (m_heightPx),
                                     DXGI_FORMAT_UNKNOWN,
                                     0);
    CHRA (hr);

    hr = CreateBackBufferTarget();
    CHRA (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Full-frame render. Clears the back buffer, paints the title bar
//  gradient, dialog background, icon, body text, custom body, and
//  buttons; then presents via Present(1, 0).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitiveRenderer::Render (
    const DialogDefinition   & def,
    const DialogLayoutResult & layout,
    const ChromeTheme        & theme,
    int                        titleHeightPx,
    std::vector<Button>      & buttons,
    size_t                     focusedHyperlinkRunIdx,
    size_t                     hoveredHyperlinkRunIdx,
    bool                       closeHovered,
    bool                       closePressed)
{
    HRESULT        hr            = S_OK;
    D3D11_VIEWPORT viewport      = {};
    float          clearColor[4] = { s_kClearR, s_kClearG, s_kClearB, s_kClearA };



    BAIL_OUT_IF (!m_initialized || m_rtv == nullptr, S_OK);

    if (!m_text.IsTargetBound())
    {
        hr = BindTextTarget();
        CHRA (hr);
    }

    viewport.Width    = static_cast<float> (m_widthPx);
    viewport.Height   = static_cast<float> (m_heightPx);
    viewport.MaxDepth = 1.0f;

    m_context->RSSetViewports (1, &viewport);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDraw();
    CHRA (hr);

    PaintBackground (theme, titleHeightPx);
    PaintTitle      (def, theme, titleHeightPx, closeHovered, closePressed);
    PaintIcon       (def, layout, titleHeightPx);
    PaintBody       (def, layout, theme, titleHeightPx,
                     focusedHyperlinkRunIdx, hoveredHyperlinkRunIdx);
    PaintCustomBody (def, layout, theme, titleHeightPx);
    PaintButtons    (buttons, theme);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  MeasureText
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitiveRenderer::MeasureText (
    const wchar_t * text,
    float           fontSizePx,
    float         & outWidthPx)
{
    HRESULT  hr = S_OK;
    float    w  = 0.0f;
    float    h  = 0.0f;



    outWidthPx = 0.0f;

    hr = m_text.MeasureString (text, fontSizePx, L"Segoe UI", w, h);
    CHRA (hr);

    outWidthPx = w;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferTarget
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitiveRenderer::CreateBackBufferTarget()
{
    HRESULT                  hr = S_OK;
    ComPtr<ID3D11Texture2D>  backBuffer;



    CBRA (m_device);
    CBRA (m_swapChain);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

    hr = BindTextTarget();
    CHRA (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  BindTextTarget
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitiveRenderer::BindTextTarget()
{
    HRESULT                  hr = S_OK;
    ComPtr<ID3D11Texture2D>  backBuffer;
    ComPtr<IDXGISurface>     surface;



    CBRA (m_swapChain);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = backBuffer.As (&surface);
    CHRA (hr);

    hr = m_text.BindBackBuffer (surface.Get(), m_scaler.Dpi(), m_scaler.Dpi());
    CHRA (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintBackground
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintBackground (const ChromeTheme & theme, int titleHeightPx)
{
    float  contentY = static_cast<float> (titleHeightPx);
    float  contentH = static_cast<float> (m_heightPx) - contentY;
    float  totalW   = static_cast<float> (m_widthPx);
    float  totalH   = static_cast<float> (m_heightPx);



    m_painter.FillRect     (0.0f, contentY, totalW, contentH,                 theme.dropdownBgArgb);
    m_painter.FillRect     (0.0f, contentY, totalW, s_kTitleSeparatorHeightPx, theme.navStripArgb);
    m_painter.OutlineRect  (0.0f, 0.0f,     totalW, totalH,                   s_kBorderThicknessPx, theme.navStripArgb);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintTitle
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintTitle (
    const DialogDefinition & def,
    const ChromeTheme      & theme,
    int                      titleHeightPx,
    bool                     closeHovered,
    bool                     closePressed)
{
    HRESULT                    hr             = S_OK;
    float                      titlePaddingPx = m_scaler.Pxf (s_kTitlePaddingDp);
    float                      titleH         = static_cast<float> (titleHeightPx);
    float                      fontPx         = m_scaler.Pxf (s_kTitleFontDp);
    float                      closeWPx       = m_scaler.Pxf (s_kCloseButtonWidthDp);
    float                      closeGlyphPx   = m_scaler.Pxf (s_kCloseGlyphFontDp);
    float                      closeLeftPx    = static_cast<float> (m_widthPx) - closeWPx;
    float                      textX          = titlePaddingPx;
    float                      textW          = closeLeftPx - titlePaddingPx * s_kCenterDivisor;
    const WindowsThemeColors & sys            = WindowsThemeColors::Instance();
    uint32_t                   fillArgb       = 0;
    uint32_t                   glyphArgb      = sys.CaptionButtonForegroundArgb();



    m_painter.FillGradientRect (0.0f, 0.0f, static_cast<float> (m_widthPx), titleH,
                                theme.titleBarTopArgb, theme.titleBarBottomArgb);

    if (!def.title.empty())
    {
        IGNORE_RETURN_VALUE (hr, m_text.DrawString (def.title.c_str(),
                                                    textX, 0.0f, textW, titleH,
                                                    theme.titleTextArgb,
                                                    fontPx, L"Segoe UI",
                                                    DwriteTextRenderer::HAlign::Left,
                                                    DwriteTextRenderer::VAlign::Center));
    }

    if (closePressed)
    {
        fillArgb  = sys.CloseButtonPressedArgb();
        glyphArgb = sys.CloseButtonGlyphPressedArgb();
    }
    else if (closeHovered)
    {
        fillArgb  = sys.CloseButtonHoverArgb();
        glyphArgb = sys.CloseButtonGlyphHoverArgb();
    }

    if (fillArgb != 0)
    {
        m_painter.FillRect (closeLeftPx, 0.0f, closeWPx, titleH, fillArgb);
    }

    IGNORE_RETURN_VALUE (hr, m_text.DrawString (s_kpszCloseGlyph,
                                                closeLeftPx, 0.0f, closeWPx, titleH,
                                                glyphArgb,
                                                closeGlyphPx, s_kpszMdl2Family,
                                                DwriteTextRenderer::HAlign::Center,
                                                DwriteTextRenderer::VAlign::Center));
}




////////////////////////////////////////////////////////////////////////////////
//
//  EnsureAppIconLoaded
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::EnsureAppIconLoaded (int iconResourceId, int sizePx)
{
    HINSTANCE  hInstance = nullptr;

    if (!m_appIconPixels.empty() && m_appIconResId == iconResourceId && m_appIconW == sizePx)
    {
        return;
    }

    if (m_hwnd == nullptr)
    {
        return;
    }

    hInstance = (HINSTANCE) GetWindowLongPtrW (m_hwnd, GWLP_HINSTANCE);
    if (hInstance == nullptr)
    {
        return;
    }

    if (LoadIconAsPremulBgra (hInstance, iconResourceId, sizePx,
                              m_appIconPixels, m_appIconW, m_appIconH))
    {
        m_appIconResId = iconResourceId;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintIcon
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintIcon (
    const DialogDefinition   & def,
    const DialogLayoutResult & layout,
    int                        titleHeightPx)
{
    HRESULT   hr           = S_OK;
    float     iconL        = 0.0f;
    float     iconT        = 0.0f;
    float     iconR        = 0.0f;
    float     iconB        = 0.0f;
    float     iconW        = 0.0f;
    float     cx           = 0.0f;
    float     cy           = 0.0f;
    float     radius       = 0.0f;
    uint32_t  color        = 0;
    int       appResId     = 0;



    if (def.icon == DialogIcon::None)
    {
        return;
    }

    iconL  = (float) layout.iconRectPx.left;
    iconT  = (float) layout.iconRectPx.top    + (float) titleHeightPx;
    iconR  = (float) layout.iconRectPx.right;
    iconB  = (float) layout.iconRectPx.bottom + (float) titleHeightPx;
    iconW  = iconR - iconL;
    cx     = (iconL + iconR) / s_kCenterDivisor;
    cy     = (iconT + iconB) / s_kCenterDivisor;
    radius = iconW * s_kIconRadiusFraction;

    if (def.icon == DialogIcon::AppPhotoreal || def.icon == DialogIcon::AppFlat)
    {
        appResId = (def.icon == DialogIcon::AppPhotoreal) ? IDI_CASSO_PHOTOREAL : IDI_CASSO;
        EnsureAppIconLoaded (appResId, (int) iconW);

        if (!m_appIconPixels.empty() && m_appIconW > 0 && m_appIconH > 0)
        {
            IGNORE_RETURN_VALUE (hr, m_text.DrawIconBitmap (m_appIconPixels.data(),
                                                            m_appIconW, m_appIconH,
                                                            iconL, iconT,
                                                            iconW, iconB - iconT));
            return;
        }
    }

    switch (def.icon)
    {
        case DialogIcon::Info:    color = s_kIconColorInfo;    break;
        case DialogIcon::Warning: color = s_kIconColorWarning; break;
        case DialogIcon::Error:   color = s_kIconColorError;   break;
        default:                                                break;
    }

    if (color != 0)
    {
        m_painter.FillCircleApprox (cx, cy, radius, color);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintBody
//
//  Iterates per-line wrapped pieces produced by DialogLayout, drawing
//  each substring at its exact pixel rect. This avoids letting DWrite
//  re-wrap inside the unioned per-run bounding rect (which would
//  disagree with FindWrapBoundary at the edges and cause spurious
//  mid-sentence breaks).
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintBody (
    const DialogDefinition   & def,
    const DialogLayoutResult & layout,
    const ChromeTheme        & theme,
    int                        titleHeightPx,
    size_t                     focusedHyperlinkRunIdx,
    size_t                     hoveredHyperlinkRunIdx)
{
    HRESULT  hr         = S_OK;
    float    fontPx     = m_scaler.Pxf (s_kBodyFontDp);
    float    titleH     = (float) titleHeightPx;
    float    lineH      = (layout.bodyLineHeightPx > 0.0f) ? layout.bodyLineHeightPx
                                                           : fontPx;
    size_t   pi         = 0;



    for (pi = 0; pi < layout.wrappedPiecesPx.size(); pi++)
    {
        const DialogWrappedPiece &  p = layout.wrappedPiecesPx[pi];

        if (p.count == 0 || p.runIndex >= def.body.size())
        {
            continue;
        }

        const DialogTextRun &  run        = def.body[p.runIndex];
        std::wstring           piece      (run.text.data() + p.start, p.count);
        bool                   isFocused  = run.isHyperlink && (p.runIndex == focusedHyperlinkRunIdx);
        bool                   isHovered  = run.isHyperlink && (p.runIndex == hoveredHyperlinkRunIdx);
        uint32_t               linkColor  = (isFocused || isHovered) ? theme.linkHoverArgb
                                                                     : theme.linkArgb;
        uint32_t               textColor  = run.isHyperlink ? linkColor
                                                            : theme.dropdownItemTextArgb;
        float                  x          = p.xPx;
        float                  y          = p.yPx + titleH;
        float                  w          = p.widthPx;

        IGNORE_RETURN_VALUE (hr, m_text.DrawString (piece.c_str(),
                                                    x, y, w, lineH,
                                                    textColor,
                                                    fontPx, L"Segoe UI",
                                                    DwriteTextRenderer::HAlign::Left,
                                                    DwriteTextRenderer::VAlign::Top));

        if (run.isHyperlink)
        {
            m_painter.FillRect (x, y + lineH - s_kUnderlineHeightPx,
                                w, s_kUnderlineHeightPx, linkColor);
        }

        if (isFocused)
        {
            float  rx = x   - 2.0f;
            float  ry = y   - 1.0f;
            float  rw = w   + 4.0f;
            float  rh = lineH + 2.0f;

            m_painter.FillRect (rx,            ry,            rw,   1.0f, theme.linkHoverArgb);
            m_painter.FillRect (rx,            ry + rh - 1.0f, rw,  1.0f, theme.linkHoverArgb);
            m_painter.FillRect (rx,            ry,            1.0f, rh,   theme.linkHoverArgb);
            m_painter.FillRect (rx + rw - 1.0f, ry,           1.0f, rh,   theme.linkHoverArgb);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintCustomBody
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintCustomBody (
    const DialogDefinition   & def,
    const DialogLayoutResult & layout,
    const ChromeTheme        & theme,
    int                        titleHeightPx)
{
    DialogPaintContext  ctx          = {};
    RECT                adjustedRect = layout.customBodyRectPx;



    if (!def.onPaintCustomBody)
    {
        return;
    }

    adjustedRect.top    += titleHeightPx;
    adjustedRect.bottom += titleHeightPx;

    ctx.painter        = &m_painter;
    ctx.text           = &m_text;
    ctx.theme          = &theme;
    ctx.customBodyRect = adjustedRect;
    ctx.dpiScale       = static_cast<float> (m_scaler.Dpi()) / static_cast<float> (DpiScaler::kBaseDpi);

    def.onPaintCustomBody (ctx);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintButtons
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintButtons (std::vector<Button> & buttons, const ChromeTheme & theme)
{
    for (Button & btn : buttons)
    {
        btn.Paint (m_painter, m_text, theme);
    }
}
