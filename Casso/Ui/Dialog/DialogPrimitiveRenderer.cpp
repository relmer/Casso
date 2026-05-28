#include "Pch.h"

#include "DialogPrimitiveRenderer.h"
#include "../Chrome/ChromeTheme.h"


static constexpr UINT      s_kSwapBufferCount        = 2;
static constexpr float     s_kBodyFontDp              = 11.0f;
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
static constexpr uint32_t  s_kIconColorApp            = 0xFF8899AA;




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
    std::vector<Button>      & buttons)
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
    PaintTitle      (def, theme, titleHeightPx);
    PaintIcon       (def, layout, titleHeightPx);
    PaintBody       (def, layout, theme, titleHeightPx);
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
    int                      titleHeightPx)
{
    HRESULT  hr             = S_OK;
    float    titlePaddingPx = m_scaler.Pxf (s_kTitlePaddingDp);
    float    titleH         = static_cast<float> (titleHeightPx);
    float    fontPx         = m_scaler.Pxf (s_kTitleFontDp);
    float    textX          = titlePaddingPx;
    float    textW          = static_cast<float> (m_widthPx) - titlePaddingPx * s_kCenterDivisor;



    m_painter.FillGradientRect (0.0f, 0.0f, static_cast<float> (m_widthPx), titleH,
                                theme.titleBarTopArgb, theme.titleBarBottomArgb);

    CBR (!def.title.empty());

    IGNORE_RETURN_VALUE (hr, m_text.DrawString (def.title.c_str(),
                                                textX, 0.0f, textW, titleH,
                                                theme.titleTextArgb,
                                                fontPx, L"Segoe UI",
                                                DwriteTextRenderer::HAlign::Left,
                                                DwriteTextRenderer::VAlign::Center));
Error:
    return;
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
    float     iconL   = 0.0f;
    float     iconT   = 0.0f;
    float     iconR   = 0.0f;
    float     iconB   = 0.0f;
    float     iconW   = 0.0f;
    float     cx      = 0.0f;
    float     cy      = 0.0f;
    float     radius  = 0.0f;
    uint32_t  color   = 0;



    if (def.icon == DialogIcon::None)
    {
        return;
    }

    iconL  = static_cast<float> (layout.iconRectPx.left);
    iconT  = static_cast<float> (layout.iconRectPx.top)    + static_cast<float> (titleHeightPx);
    iconR  = static_cast<float> (layout.iconRectPx.right);
    iconB  = static_cast<float> (layout.iconRectPx.bottom) + static_cast<float> (titleHeightPx);
    iconW  = iconR - iconL;
    cx     = (iconL + iconR) / s_kCenterDivisor;
    cy     = (iconT + iconB) / s_kCenterDivisor;
    radius = iconW * s_kIconRadiusFraction;

    switch (def.icon)
    {
        case DialogIcon::Info:         color = s_kIconColorInfo;    break;
        case DialogIcon::Warning:      color = s_kIconColorWarning; break;
        case DialogIcon::Error:        color = s_kIconColorError;   break;
        case DialogIcon::AppPhotoreal: color = s_kIconColorApp;     break;
        case DialogIcon::AppFlat:      color = s_kIconColorApp;     break;
        default:                                                     break;
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
//  Iterates body runs in definition order: normal text in body text
//  color, hyperlinks in accent color with a single-pixel underline.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitiveRenderer::PaintBody (
    const DialogDefinition   & def,
    const DialogLayoutResult & layout,
    const ChromeTheme        & theme,
    int                        titleHeightPx)
{
    HRESULT  hr    = S_OK;
    float    fontPx = m_scaler.Pxf (s_kBodyFontDp);
    float    titleH = static_cast<float> (titleHeightPx);
    size_t   count  = std::min (def.body.size(), layout.bodyRunRectsPx.size());



    for (size_t i = 0; i < count; ++i)
    {
        const DialogTextRun  & run       = def.body[i];
        const RECT           & rect      = layout.bodyRunRectsPx[i];
        float                  x         = static_cast<float> (rect.left);
        float                  y         = static_cast<float> (rect.top)  + titleH;
        float                  w         = static_cast<float> (rect.right  - rect.left);
        float                  h         = static_cast<float> (rect.bottom - rect.top);
        uint32_t               textColor = run.isHyperlink ? theme.navHoverArgb : theme.dropdownItemTextArgb;

        IGNORE_RETURN_VALUE (hr, m_text.DrawString (run.text.c_str(),
                                                    x, y, w, h,
                                                    textColor,
                                                    fontPx, L"Segoe UI",
                                                    DwriteTextRenderer::HAlign::Left,
                                                    DwriteTextRenderer::VAlign::Top));

        if (run.isHyperlink)
        {
            m_painter.FillRect (x, y + h - s_kUnderlineHeightPx, w, s_kUnderlineHeightPx, theme.navHoverArgb);
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
