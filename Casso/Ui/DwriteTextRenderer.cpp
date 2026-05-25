#include "Pch.h"

#include "DwriteTextRenderer.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")





namespace
{
    constexpr float  s_kByteToUnit = 1.0f / 255.0f;


    inline D2D1_COLOR_F  ColorFromArgb (uint32_t argbColor)
    {
        D2D1_COLOR_F  c;

        c.a = ((argbColor >> 24) & 0xFF) * s_kByteToUnit;
        c.r = ((argbColor >> 16) & 0xFF) * s_kByteToUnit;
        c.g = ((argbColor >>  8) & 0xFF) * s_kByteToUnit;
        c.b = ((argbColor      ) & 0xFF) * s_kByteToUnit;

        return c;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DwriteTextRenderer
//
////////////////////////////////////////////////////////////////////////////////

DwriteTextRenderer::~DwriteTextRenderer ()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Builds the Direct2D + DirectWrite factories and binds a Direct2D
//  device context to the shared D3D11 device's underlying DXGI device.
//  The caller's D3D11 device MUST have been created with
//  D3D11_CREATE_DEVICE_BGRA_SUPPORT or the D2D device-create call
//  returns E_INVALIDARG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::Initialize (ID3D11Device * pDevice)
{
    HRESULT                hr      = S_OK;
    ComPtr<IDXGIDevice>    dxgi;
    D2D1_FACTORY_OPTIONS   options = {};
    IUnknown             * dwriteRaw = nullptr;



    CBRAEx (pDevice, E_INVALIDARG);

#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif

    hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_SINGLE_THREADED,
                            __uuidof (ID2D1Factory1),
                            &options,
                            reinterpret_cast<void **> (m_d2dFactory.GetAddressOf()));
    CHRA (hr);

    hr = pDevice->QueryInterface (__uuidof (IDXGIDevice),
                                  reinterpret_cast<void **> (dxgi.GetAddressOf()));
    CHRA (hr);

    hr = m_d2dFactory->CreateDevice (dxgi.Get(), &m_d2dDevice);
    CHRA (hr);

    hr = m_d2dDevice->CreateDeviceContext (D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    CHRA (hr);

    hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
                              __uuidof (IDWriteFactory),
                              &dwriteRaw);
    CHRA (hr);

    m_dwriteFactory.Attach (static_cast<IDWriteFactory *> (dwriteRaw));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DwriteTextRenderer::Shutdown ()
{
    UnbindBackBuffer();

    m_framebufferBitmap.Reset();
    m_framebufferBitmapW = 0;
    m_framebufferBitmapH = 0;
    m_iconBitmap.Reset();
    m_iconBitmapW = 0;
    m_iconBitmapH = 0;
    m_formatCache.clear();
    m_dwriteFactory.Reset();
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
    m_drawing = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BindBackBuffer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::BindBackBuffer (
    IDXGISurface  * pBackBufferSurface,
    UINT            dpiX,
    UINT            dpiY)
{
    HRESULT                  hr    = S_OK;
    D2D1_BITMAP_PROPERTIES1  props = {};



    CBRA (m_d2dContext);
    CBRAEx (pBackBufferSurface, E_INVALIDARG);

    UnbindBackBuffer();

    props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX                  = (dpiX == 0) ? 96.0f : (float) dpiX;
    props.dpiY                  = (dpiY == 0) ? 96.0f : (float) dpiY;
    props.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = m_d2dContext->CreateBitmapFromDxgiSurface (pBackBufferSurface, &props, &m_target);
    CHRA (hr);

    m_d2dContext->SetTarget (m_target.Get());
    m_targetBound = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnbindBackBuffer
//
////////////////////////////////////////////////////////////////////////////////

void DwriteTextRenderer::UnbindBackBuffer ()
{
    if (m_d2dContext)
    {
        m_d2dContext->SetTarget (nullptr);
    }

    m_target.Reset();
    m_targetBound = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDraw
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::BeginDraw ()
{
    HRESULT  hr = S_OK;



    CBRA (m_d2dContext);
    CBRA (m_targetBound);

    m_d2dContext->BeginDraw();
    m_drawing = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDraw
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::EndDraw ()
{
    HRESULT  hr   = S_OK;
    HRESULT  hrEnd = S_OK;



    CBRA (m_d2dContext);

    if (!m_drawing)
    {
        return S_OK;
    }

    hrEnd = m_d2dContext->EndDraw();
    m_drawing = false;

    if (hrEnd == D2DERR_RECREATE_TARGET)
    {
        // Device-lost path: drop the target so the next BindBackBuffer
        // rebuilds. Surface as S_OK to the caller so the present path
        // continues; the renderer-level recovery handles the rebuild.
        UnbindBackBuffer();
        return S_OK;
    }

    hr = hrEnd;
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureTextFormat
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::EnsureTextFormat (
    const wchar_t      *  family,
    float                 fontSizeDip,
    IDWriteTextFormat  ** outFormat)
{
    HRESULT             hr        = S_OK;
    TextFormatKey       key;
    const wchar_t     * useFamily = (family != nullptr) ? family : L"Segoe UI";



    CBRAEx (outFormat, E_INVALIDARG);
    CBRA (m_dwriteFactory);

    *outFormat = nullptr;

    key.family  = useFamily;
    key.sizeDip = fontSizeDip;

    {
        auto  it = m_formatCache.find (key);

        if (it != m_formatCache.end())
        {
            *outFormat = it->second.Get();
            (*outFormat)->AddRef();
            return S_OK;
        }
    }

    {
        ComPtr<IDWriteTextFormat>  format;

        hr = m_dwriteFactory->CreateTextFormat (useFamily,
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                fontSizeDip,
                                                L"en-us",
                                                &format);
        CHRA (hr);

        m_formatCache[key] = format;
        *outFormat = format.Get();
        (*outFormat)->AddRef();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawString
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::DrawString (
    const wchar_t  * text,
    float            xDip,
    float            yDip,
    float            widthDip,
    float            heightDip,
    uint32_t         argbColor,
    float            fontSizeDip,
    const wchar_t  * fontFamily,
    HAlign           hAlign,
    VAlign           vAlign)
{
    HRESULT                            hr      = S_OK;
    ComPtr<IDWriteTextFormat>          format;
    IDWriteTextFormat                * rawFmt  = nullptr;
    ComPtr<ID2D1SolidColorBrush>       brush;
    D2D1_RECT_F                        layoutRect;
    UINT32                             textLen = 0;
    DWRITE_TEXT_ALIGNMENT              dwH     = DWRITE_TEXT_ALIGNMENT_LEADING;
    DWRITE_PARAGRAPH_ALIGNMENT         dwV     = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;



    CBRA (m_d2dContext);
    CBRA (m_drawing);
    CBRAEx (text, E_INVALIDARG);

    hr = EnsureTextFormat (fontFamily, fontSizeDip, &rawFmt);
    CHRA (hr);

    format.Attach (rawFmt);

    switch (hAlign)
    {
        case HAlign::Left:   dwH = DWRITE_TEXT_ALIGNMENT_LEADING;  break;
        case HAlign::Center: dwH = DWRITE_TEXT_ALIGNMENT_CENTER;   break;
        case HAlign::Right:  dwH = DWRITE_TEXT_ALIGNMENT_TRAILING; break;
    }

    switch (vAlign)
    {
        case VAlign::Top:    dwV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;   break;
        case VAlign::Center: dwV = DWRITE_PARAGRAPH_ALIGNMENT_CENTER; break;
        case VAlign::Bottom: dwV = DWRITE_PARAGRAPH_ALIGNMENT_FAR;    break;
    }

    format->SetTextAlignment      (dwH);
    format->SetParagraphAlignment (dwV);

    hr = m_d2dContext->CreateSolidColorBrush (ColorFromArgb (argbColor), &brush);
    CHRA (hr);
    if (m_globalAlpha < 1.0f)
    {
        D2D1_COLOR_F  scaled = brush->GetColor();
        scaled.a *= m_globalAlpha;
        brush->SetColor (scaled);
    }

    layoutRect.left   = xDip;
    layoutRect.top    = yDip;
    layoutRect.right  = xDip + widthDip;
    layoutRect.bottom = yDip + heightDip;

    textLen = (UINT32) wcslen (text);

    m_d2dContext->DrawText (text,
                            textLen,
                            format.Get(),
                            &layoutRect,
                            brush.Get(),
                            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
                            DWRITE_MEASURING_MODE_NATURAL);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillRect
//
//  Paints a filled axis-aligned rectangle through the D2D context.
//  Useful when a fill needs to composite in submission order against
//  prior DrawString calls (e.g. opaque dropdown menu background that
//  must hide earlier text rendered underneath). DxUiPainter's FillRect
//  goes through D3D and always flushes before any D2D text, so it
//  cannot cover text drawn earlier in the same frame.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::FillRect (
    float    xDip,
    float    yDip,
    float    widthDip,
    float    heightDip,
    uint32_t argbColor)
{
    HRESULT                            hr     = S_OK;
    ComPtr<ID2D1SolidColorBrush>       brush;
    D2D1_RECT_F                        rect   = {};


    CBRA (m_d2dContext);
    CBRA (m_drawing);

    hr = m_d2dContext->CreateSolidColorBrush (ColorFromArgb (argbColor), &brush);
    CHRA (hr);
    if (m_globalAlpha < 1.0f)
    {
        D2D1_COLOR_F  scaled = brush->GetColor();
        scaled.a *= m_globalAlpha;
        brush->SetColor (scaled);
    }

    rect.left   = xDip;
    rect.top    = yDip;
    rect.right  = xDip + widthDip;
    rect.bottom = yDip + heightDip;

    m_d2dContext->FillRectangle (&rect, brush.Get());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawFramebuffer
//
//  Uploads a BGRA8 CPU pixel buffer into a cached ID2D1Bitmap and
//  draws it into the destination DIP rect with linear interpolation.
//  The bitmap is created lazily and recreated when the source
//  dimensions change; otherwise CopyFromMemory rewrites the pixels
//  in place each call. Used by the Settings → Theme preview to show
//  the live emulator framebuffer inside the mock window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::DrawFramebuffer (
    const uint32_t * srcBgraPixels,
    int              srcWidthPx,
    int              srcHeightPx,
    float            destXDip,
    float            destYDip,
    float            destWidthDip,
    float            destHeightDip)
{
    HRESULT       hr     = S_OK;
    D2D1_RECT_F   dest   = {};



    CBRA (m_d2dContext);
    CBRA (m_drawing);
    CBRA (srcBgraPixels);
    CBRA (srcWidthPx > 0 && srcHeightPx > 0);

    if (m_framebufferBitmap == nullptr ||
        m_framebufferBitmapW != srcWidthPx ||
        m_framebufferBitmapH != srcHeightPx)
    {
        D2D1_BITMAP_PROPERTIES  props = {};

        props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        props.dpiX                  = 96.0f;
        props.dpiY                  = 96.0f;

        m_framebufferBitmap.Reset();
        hr = m_d2dContext->CreateBitmap (D2D1::SizeU ((UINT32) srcWidthPx, (UINT32) srcHeightPx),
                                         nullptr, 0, &props, &m_framebufferBitmap);
        CHRA (hr);
        m_framebufferBitmapW = srcWidthPx;
        m_framebufferBitmapH = srcHeightPx;
    }

    {
        D2D1_RECT_U  srcRect = D2D1::RectU (0, 0, (UINT32) srcWidthPx, (UINT32) srcHeightPx);

        hr = m_framebufferBitmap->CopyFromMemory (&srcRect, srcBgraPixels,
                                                  (UINT32) (srcWidthPx * sizeof (uint32_t)));
        CHRA (hr);
    }

    dest.left   = destXDip;
    dest.top    = destYDip;
    dest.right  = destXDip + destWidthDip;
    dest.bottom = destYDip + destHeightDip;

    m_d2dContext->DrawBitmap (m_framebufferBitmap.Get(),
                              &dest, m_globalAlpha,
                              D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                              nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawIconBitmap
//
//  Same upload-and-blit logic as DrawFramebuffer but uses a dedicated
//  cached ID2D1Bitmap so the title-bar app icon (stable size) and the
//  emulator framebuffer (560x384) don't ping-pong recreating the same
//  cache slot every frame. Source pixels are interpreted as
//  premultiplied BGRA8 so the alpha channel composites correctly.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::DrawIconBitmap (
    const uint32_t * srcBgraPremul,
    int              srcWidthPx,
    int              srcHeightPx,
    float            destXDip,
    float            destYDip,
    float            destWidthDip,
    float            destHeightDip)
{
    HRESULT       hr     = S_OK;
    D2D1_RECT_F   dest   = {};



    CBRA (m_d2dContext);
    CBRA (m_drawing);
    CBRA (srcBgraPremul);
    CBRA (srcWidthPx > 0 && srcHeightPx > 0);

    if (m_iconBitmap == nullptr ||
        m_iconBitmapW != srcWidthPx ||
        m_iconBitmapH != srcHeightPx)
    {
        D2D1_BITMAP_PROPERTIES  props = {};

        props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.dpiX                  = 96.0f;
        props.dpiY                  = 96.0f;

        m_iconBitmap.Reset();
        hr = m_d2dContext->CreateBitmap (D2D1::SizeU ((UINT32) srcWidthPx, (UINT32) srcHeightPx),
                                         nullptr, 0, &props, &m_iconBitmap);
        CHRA (hr);
        m_iconBitmapW = srcWidthPx;
        m_iconBitmapH = srcHeightPx;
    }

    {
        D2D1_RECT_U  srcRect = D2D1::RectU (0, 0, (UINT32) srcWidthPx, (UINT32) srcHeightPx);

        hr = m_iconBitmap->CopyFromMemory (&srcRect, srcBgraPremul,
                                           (UINT32) (srcWidthPx * sizeof (uint32_t)));
        CHRA (hr);
    }

    dest.left   = destXDip;
    dest.top    = destYDip;
    dest.right  = destXDip + destWidthDip;
    dest.bottom = destYDip + destHeightDip;

    m_d2dContext->DrawBitmap (m_iconBitmap.Get(),
                              &dest, m_globalAlpha,
                              D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                              nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureString
//
//  Returns the natural pixel extent of `text` in the requested font.
//  Independent of BeginDraw/EndDraw bracketing so chrome layout code
//  can size hit-rects before the first frame is rendered.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::MeasureString (
    const wchar_t  * text,
    float            fontSizeDip,
    const wchar_t  * fontFamily,
    float          & outWidthDip,
    float          & outHeightDip)
{
    HRESULT                            hr      = S_OK;
    ComPtr<IDWriteTextFormat>          format;
    IDWriteTextFormat                * rawFmt  = nullptr;
    ComPtr<IDWriteTextLayout>          layout;
    DWRITE_TEXT_METRICS                metrics = {};
    UINT32                             textLen = 0;



    outWidthDip  = 0.0f;
    outHeightDip = 0.0f;

    CBRAEx (text, E_INVALIDARG);

    // Non-asserting member-state check: callers from chrome layout
    // may invoke MeasureString during window-creation -- before
    // Initialize() has run -- to size a first-pass layout. The
    // chrome falls back to a fixed-width heuristic in that case and
    // re-measures on the next Layout pass once Initialize is done.
    CBR (m_dwriteFactory);

    hr = EnsureTextFormat (fontFamily, fontSizeDip, &rawFmt);
    CHRA (hr);

    format.Attach (rawFmt);

    textLen = (UINT32) wcslen (text);

    hr = m_dwriteFactory->CreateTextLayout (text,
                                            textLen,
                                            format.Get(),
                                            FLT_MAX,
                                            FLT_MAX,
                                            &layout);
    CHRA (hr);

    hr = layout->GetMetrics (&metrics);
    CHRA (hr);

    outWidthDip  = metrics.widthIncludingTrailingWhitespace;
    outHeightDip = metrics.height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceLost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::OnDeviceLost ()
{
    Shutdown();
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceRestored
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DwriteTextRenderer::OnDeviceRestored (ID3D11Device * pDevice)
{
    return Initialize (pDevice);
}
