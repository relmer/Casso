#include "Pch.h"

#include "DxuiTextRenderer.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")


// DxuiFontWeight mirrors the DirectWrite weight axis 1:1 so the concrete
// renderer maps between them with a plain static_cast; enforce that here.
static_assert ((int) DxuiFontWeight::Normal   == DWRITE_FONT_WEIGHT_NORMAL,    "DxuiFontWeight::Normal weight mismatch");
static_assert ((int) DxuiFontWeight::SemiBold == DWRITE_FONT_WEIGHT_SEMI_BOLD, "DxuiFontWeight::SemiBold weight mismatch");
static_assert ((int) DxuiFontWeight::Bold     == DWRITE_FONT_WEIGHT_BOLD,      "DxuiFontWeight::Bold weight mismatch");




static constexpr float  s_kByteToUnit = 1.0f / 255.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  ColorFromArgb
//
////////////////////////////////////////////////////////////////////////////////

D2D1_COLOR_F  DxuiTextRenderer::ColorFromArgb (uint32_t argbColor)
{
    D2D1_COLOR_F  c;



    c.a = ((argbColor >> 24) & 0xFF) * s_kByteToUnit;
    c.r = ((argbColor >> 16) & 0xFF) * s_kByteToUnit;
    c.g = ((argbColor >>  8) & 0xFF) * s_kByteToUnit;
    c.b = ((argbColor      ) & 0xFF) * s_kByteToUnit;

    return c;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiTextRenderer
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextRenderer::~DxuiTextRenderer()
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

HRESULT DxuiTextRenderer::Initialize (ID3D11Device * pDevice)
{
    HRESULT                 hr        = S_OK;
    ComPtr<IDXGIDevice>     dxgi;
    D2D1_FACTORY_OPTIONS    options   = {};
    IUnknown              * dwriteRaw = nullptr;



    DXUI_ASSERT_UI_THREAD();

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

void DxuiTextRenderer::Shutdown()
{
    DXUI_ASSERT_UI_THREAD();

    UnbindBackBuffer();

    m_offscreen.Reset();
    m_offscreenW = 0;
    m_offscreenH = 0;
    m_framebufferBitmap.Reset();
    m_framebufferBitmapW = 0;
    m_framebufferBitmapH = 0;
    m_iconBitmap.Reset();
    m_iconBitmapW = 0;
    m_iconBitmapH = 0;
    m_brushCache.clear();
    m_layoutCache.clear();
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
//  Wraps the host's DXGI back-buffer surface as a D2D bitmap and makes it the
//  render target, so text draws directly into the same surface D3D just
//  composited into.
//
//  The previous binding is dropped FIRST, unconditionally. A resize calls this
//  with a new surface while the old one is still bound, and D2D would
//  otherwise keep the old back buffer alive.
//
//  DPI is baked into the bitmap rather than applied per draw, which is what
//  lets callers pass DIPs everywhere and get correctly-scaled text on any
//  monitor. A zero DPI falls back to 96 rather than producing a degenerate
//  target -- callers legitimately have no DPI before the first WM_NCCREATE.
//
//  CANNOT_DRAW is set because this bitmap is a TARGET only; it is never
//  sampled as a source, and saying so lets D2D skip readback support.
//
//  PREMULTIPLIED alpha matches both DxuiPainter and the 3D renderer, so
//  everything composites into this surface under one blend convention.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::BindBackBuffer (
    IDXGISurface  * pBackBufferSurface,
    UINT            dpiX,
    UINT            dpiY)
{
    HRESULT                  hr    = S_OK;
    D2D1_BITMAP_PROPERTIES1  props = {};



    DXUI_ASSERT_UI_THREAD();

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

void DxuiTextRenderer::UnbindBackBuffer()
{
    DXUI_ASSERT_UI_THREAD();

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

HRESULT DxuiTextRenderer::BeginDraw()
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();

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
//  Closes the D2D batch and handles device loss.
//
//  D2D reports errors from the whole batch HERE, not at each draw call, so
//  this is the only place a painting failure can surface -- the individual
//  DrawString calls have nothing to report.
//
//  D2DERR_RECREATE_TARGET means the device was lost. It is handled rather than
//  propagated: the target is unbound so the next BindBackBuffer rebuilds it,
//  and the frame is simply dropped. Callers notice through IsTargetBound and
//  skip presenting what would be a half-painted frame. Treating it as an error
//  would fail a frame that a rebind fixes completely.
//
//  Calling this without a matching BeginDraw is a no-op rather than an error,
//  so an early-out paint path does not have to track whether it began.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EndDraw()
{
    HRESULT  hr           = S_OK;
    HRESULT  hrEnd        = S_OK;
    bool     isTargetLost = false;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_d2dContext);
    BAIL_OUT_IF (!m_drawing, S_OK);

    hrEnd        = m_d2dContext->EndDraw();
    m_drawing    = false;
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    if (isTargetLost)
    {
        // Device-lost path: drop the target so the next BindBackBuffer
        // rebuilds. The target is now unbound; callers detect this via
        // IsTargetBound() and skip presenting the half-painted frame.
        DEBUGMSG (L"[Dxui] DxuiTextRenderer::EndDraw target lost (D2DERR_RECREATE_TARGET); frame dropped\n");
        UnbindBackBuffer();
    }

    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDrawOffscreen
//
//  Drop-in alternative to BeginDraw that targets a private offscreen
//  bitmap instead of the swap-chain back-buffer surface. Paired with
//  EndDrawComposite, which flushes the offscreen bitmap and then blits
//  it onto the bound back buffer in a single DrawBitmap.
//
//  Why this exists: on some drivers (observed on ARM64) Direct2D drops
//  the middle band of a frame when the render target IS the flip-model
//  swap-chain surface and that surface is large -- top and bottom
//  survive, the middle silently vanishes despite EndDraw returning
//  S_OK. Rendering into a plain offscreen target and compositing the
//  result sidesteps the limitation, so tall debug panels paint in full.
//
//  The offscreen bitmap is cached and only recreated when the target
//  pixel size changes. It is cleared transparent so the composite
//  alpha-blends cleanly over whatever D3D geometry was drawn earlier
//  in the frame.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::BeginDrawOffscreen()
{
    HRESULT                  hr    = S_OK;
    D2D1_SIZE_U              size  = {};
    D2D1_BITMAP_PROPERTIES1  props = {};



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_d2dContext);
    CBRA (m_targetBound);

    size = m_target->GetPixelSize();

    if (m_offscreen == nullptr || m_offscreenW != size.width || m_offscreenH != size.height)
    {
        m_offscreen.Reset();

        props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.dpiX                  = 96.0f;
        props.dpiY                  = 96.0f;
        props.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET;

        hr = m_d2dContext->CreateBitmap (size, nullptr, 0, &props, &m_offscreen);
        CHRA (hr);

        m_offscreenW = size.width;
        m_offscreenH = size.height;
    }

    m_d2dContext->SetTarget (m_offscreen.Get());
    m_d2dContext->BeginDraw();
    m_d2dContext->Clear (D2D1::ColorF (0.0f, 0.0f, 0.0f, 0.0f));
    m_drawing = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDrawComposite
//
//  Flushes the offscreen bitmap started by BeginDrawOffscreen, then
//  rebinds the back-buffer target and draws the offscreen bitmap onto
//  it 1:1 (nearest-neighbor, no scaling). On D2DERR_RECREATE_TARGET
//  the target is dropped so the next BindBackBuffer rebuilds it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EndDrawComposite()
{
    HRESULT      hr           = S_OK;
    HRESULT      hrEnd        = S_OK;
    D2D1_SIZE_U  size         = {};
    D2D1_RECT_F  dest         = {};
    bool         isTargetLost = false;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_d2dContext);
    BAIL_OUT_IF (!m_drawing, S_OK);

    hrEnd        = m_d2dContext->EndDraw();
    m_drawing    = false;
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    if (isTargetLost)
    {
        UnbindBackBuffer();
        m_offscreen.Reset();
        m_offscreenW = 0;
        m_offscreenH = 0;
    }

    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

    size   = m_target->GetPixelSize();
    dest   = D2D1::RectF (0.0f, 0.0f, (float) size.width, (float) size.height);

    m_d2dContext->SetTarget (m_target.Get());
    m_d2dContext->BeginDraw();
    m_d2dContext->DrawBitmap (m_offscreen.Get(),
                              &dest,
                              1.0f,
                              D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                              &dest);
    hrEnd        = m_d2dContext->EndDraw();
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    if (isTargetLost)
    {
        UnbindBackBuffer();
    }

    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureTextFormat
//
//  Returns a cached IDWriteTextFormat for a (family, size, weight) triple,
//  creating it on first use.
//
//  Caching matters because chrome repaints continuously and the distinct
//  format count is tiny -- a handful of sizes and weights across the whole UI
//  -- while CreateTextFormat is far too expensive to run per draw.
//
//  The cache is keyed on all three properties together, since DirectWrite
//  bakes size and weight into the format object rather than accepting them per
//  draw.
//
//  A null family falls back to Segoe UI so callers can omit it for ordinary
//  body text.
//
//  The returned pointer is AddRef'd whether it was cached or freshly made, so
//  every caller releases unconditionally -- otherwise the two paths would need
//  different cleanup.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EnsureTextFormat (
    const wchar_t      *  family,
    float                 fontSizeDip,
    DxuiFontWeight        weight,
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
    key.weight  = weight;

    {
        auto  it = m_formatCache.find (key);

        if (it != m_formatCache.end())
        {
            *outFormat = it->second.Get();
        }
    }

    if (*outFormat == nullptr)
    {
        ComPtr<IDWriteTextFormat>  format;

        hr = m_dwriteFactory->CreateTextFormat (useFamily,
                                                nullptr,
                                                static_cast<DWRITE_FONT_WEIGHT> (weight),
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                fontSizeDip,
                                                L"en-us",
                                                &format);
        CHRA (hr);

        m_formatCache[key] = format;
        *outFormat         = format.Get();
    }

    (*outFormat)->AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureBrush
//
//  Returns a solid-color brush for `argb`, cached so DrawString does not
//  allocate a D2D brush per call. The caller sets opacity per use (global
//  alpha), so the cached color carries only the ARGB alpha.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EnsureBrush (uint32_t argb, ID2D1SolidColorBrush ** outBrush)
{
    HRESULT  hr = S_OK;



    CBRAEx (outBrush, E_INVALIDARG);
    CBRA (m_d2dContext);

    *outBrush = nullptr;

    {
        auto  it = m_brushCache.find (argb);

        if (it != m_brushCache.end())
        {
            *outBrush = it->second.Get();
        }
    }

    if (*outBrush == nullptr)
    {
        ComPtr<ID2D1SolidColorBrush>  brush;

        hr = m_d2dContext->CreateSolidColorBrush (ColorFromArgb (argb), &brush);
        CHRA (hr);

        m_brushCache[argb] = brush;
        *outBrush          = brush.Get();
    }

    (*outBrush)->AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureLayout
//
//  Returns a shaped DirectWrite text layout for the given text + format +
//  alignment + box, cached so repeated frames of unchanged chrome re-draw via
//  DrawTextLayout with no font-fallback / shaping (the allocation-heavy part).
//  Alignment and wrapping are set on the layout, never on the shared format.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EnsureLayout (
    const wchar_t      *  text,
    const wchar_t      *  family,
    float                 fontSizeDip,
    DxuiFontWeight        weight,
    DxuiTextHAlign        hAlign,
    DxuiTextVAlign        vAlign,
    bool                  wrap,
    float                 maxWidthDip,
    float                 maxHeightDip,
    IDWriteTextLayout  ** outLayout)
{
    IDWriteTextFormat           * rawFmt = nullptr;
    ComPtr<IDWriteTextFormat>     format;
    ComPtr<IDWriteTextLayout>     layout;
    DWRITE_TEXT_ALIGNMENT         dwH    = {};
    DWRITE_PARAGRAPH_ALIGNMENT    dwV    = {};



    // Bound so a scrolling debug panel with many distinct strings cannot grow
    // the cache without limit. Chrome's working set is a few dozen entries; the
    // cap only trips under pathological churn, where a rebuild is cheap.
    static constexpr size_t  s_kMaxLayoutCache = 512;

    HRESULT                    hr        = S_OK;
    LayoutCacheKey             key;
    const wchar_t            * useFamily = (family != nullptr) ? family : L"Segoe UI";
    dwH = DWRITE_TEXT_ALIGNMENT_LEADING;
    dwV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;


    CBRAEx (outLayout, E_INVALIDARG);
    CBRAEx (text, E_INVALIDARG);
    CBRA (m_dwriteFactory);

    *outLayout = nullptr;

    key.text    = text;
    key.family  = useFamily;
    key.sizeDip = fontSizeDip;
    key.weight  = weight;
    key.hAlign  = static_cast<int> (hAlign);
    key.vAlign  = static_cast<int> (vAlign);
    key.wrap    = wrap;
    key.maxW    = maxWidthDip;
    key.maxH    = maxHeightDip;

    {
        auto  it = m_layoutCache.find (key);

        if (it != m_layoutCache.end())
        {
            *outLayout = it->second.Get();
            (*outLayout)->AddRef();
        }
    }

    BAIL_OUT_IF (*outLayout != nullptr, S_OK);

    hr = EnsureTextFormat (useFamily, fontSizeDip, weight, &rawFmt);
    CHRA (hr);
    format.Attach (rawFmt);

    hr = m_dwriteFactory->CreateTextLayout (text,
                                            (UINT32) wcslen (text),
                                            format.Get(),
                                            maxWidthDip,
                                            maxHeightDip,
                                            &layout);
    CHRA (hr);

    switch (hAlign)
    {
        case DxuiTextHAlign::Left:   dwH = DWRITE_TEXT_ALIGNMENT_LEADING;  break;
        case DxuiTextHAlign::Center: dwH = DWRITE_TEXT_ALIGNMENT_CENTER;   break;
        case DxuiTextHAlign::Right:  dwH = DWRITE_TEXT_ALIGNMENT_TRAILING; break;
    }

    switch (vAlign)
    {
        case DxuiTextVAlign::Top:                dwV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;   break;
        case DxuiTextVAlign::Center:             dwV = DWRITE_PARAGRAPH_ALIGNMENT_CENTER; break;
        case DxuiTextVAlign::Bottom:             dwV = DWRITE_PARAGRAPH_ALIGNMENT_FAR;    break;
        case DxuiTextVAlign::CenterOnCapHeight:  dwV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;   break;
    }

    layout->SetTextAlignment      (dwH);
    layout->SetParagraphAlignment (dwV);
    layout->SetWordWrapping       (wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);

    if (m_layoutCache.size() >= s_kMaxLayoutCache)
    {
        m_layoutCache.clear();
    }

    m_layoutCache[key] = layout;
    *outLayout = layout.Get();
    (*outLayout)->AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureCapMidY
//
//  Computes the vertical offset from a line box's top to the MIDDLE OF THE
//  CAPITAL LETTERS, cached per (family, size, weight).
//
//  This is what makes DxuiTextVAlign::Center actually look centered. Centering
//  by line box centers the FONT's box -- ascender to descender -- which
//  includes room for descenders and accents that most UI strings never use, so
//  a label like "Settings" reads visibly high in its button. Centering on the
//  cap midline puts the visual mass of the text where the eye expects it.
//
//  Getting there needs two different pieces of DirectWrite:
//
//    baseline    from a laid-out line's metrics -- where the baseline sits
//                inside the line box, which depends on the layout
//    cap height  from the font FACE's design metrics, in design units, so it
//                must be scaled by size/unitsPerEm to reach DIPs
//
//  The measurement string is arbitrary ("Mg") because neither value depends on
//  the content; it exists only to produce a laid-out line to read metrics
//  from. The oversized measure box keeps that line from wrapping.
//
//  The result is cached on the same key as the text format, since it is a
//  property of the font at that size and weight and never of the string.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EnsureCapMidY (
    const wchar_t      *  family,
    float                 fontSizeDip,
    DxuiFontWeight        weight,
    IDWriteTextFormat  *  format,
    float              &  outCapMidY)
{
    HRESULT                        hr          = S_OK;
    TextFormatKey                  key;
    ComPtr<IDWriteTextLayout>      layout;
    ComPtr<IDWriteFontCollection>  collection;
    ComPtr<IDWriteFontFamily>      familyObj;
    ComPtr<IDWriteFont>            font;
    ComPtr<IDWriteFontFace>        face;
    BOOL                           familyFound = FALSE;
    bool                           isCached    = false;
    float                          kMeasureBox = 0.0f;
    UINT32                         familyIndex = 0;
    DWRITE_FONT_METRICS            metrics     = {};
    DWRITE_LINE_METRICS            lineMetrics = {};
    UINT32                         lineCount   = 0;
    const wchar_t                * useFamily     = (family != nullptr) ? family : L"Segoe UI";
    kMeasureBox = 4096.0f;
    const wchar_t                * kMeasureText  = L"Mg";



    CBRA (m_dwriteFactory);
    CBRAEx (format, E_INVALIDARG);

    key.family  = useFamily;
    key.sizeDip = fontSizeDip;
    key.weight  = weight;

    {
        auto  it = m_capMidCache.find (key);

        if (it != m_capMidCache.end())
        {
            outCapMidY = it->second;
            isCached   = true;
        }
    }

    BAIL_OUT_IF (isCached, S_OK);

    hr = m_dwriteFactory->CreateTextLayout (kMeasureText,
                                            (UINT32) wcslen (kMeasureText),
                                            format,
                                            kMeasureBox,
                                            kMeasureBox,
                                            &layout);
    CHRA (hr);

    hr = layout->GetLineMetrics (&lineMetrics, 1, &lineCount);
    CHRA (hr);

    hr = format->GetFontCollection (&collection);
    CHRA (hr);

    hr = collection->FindFamilyName (useFamily, &familyIndex, &familyFound);
    CHRA (hr);
    CBRA (familyFound);

    hr = collection->GetFontFamily (familyIndex, &familyObj);
    CHRA (hr);

    hr = familyObj->GetFirstMatchingFont (DWRITE_FONT_WEIGHT_NORMAL,
                                          DWRITE_FONT_STRETCH_NORMAL,
                                          DWRITE_FONT_STYLE_NORMAL,
                                          &font);
    CHRA (hr);

    hr = font->CreateFontFace (&face);
    CHRA (hr);
    CBRA (lineCount > 0);

    face->GetMetrics (&metrics);
    {
        float  upem         = (float) metrics.designUnitsPerEm;
        float  capHeightDip = 0.0f;
        float  baselineY    = 0.0f;
        CBRA (upem > 0.0f);

        capHeightDip = (float) metrics.capHeight * (fontSizeDip / upem);
        baselineY = lineMetrics.baseline;

        outCapMidY         = baselineY - capHeightDip * 0.5f;
        m_capMidCache[key] = outCapMidY;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawString
//
//  Draws one string into a rect with the given alignment, weight, and wrap
//  mode -- the workhorse every widget paints text through.
//
//  Three things are cached rather than built per call, because this runs many
//  times per frame: the text format, the brush, and the layout. Alignment and
//  wrapping are configured on the cached LAYOUT, never on the shared format,
//  since the format is reused across callers that align differently.
//
//  Global alpha is applied as brush OPACITY rather than by tinting the color,
//  so it multiplies the ARGB alpha instead of replacing it. It is re-applied
//  every call because the brush is shared and the previous caller may have
//  left a different value.
//
//  CenterOnCapHeight shifts the layout RECT rather than changing the
//  alignment: the text stays NEAR-aligned inside a rect moved so the cap
//  midline lands on the true center. That offset is cached per format because
//  computing it creates roughly six DWrite COM objects, and doing so per cell
//  per frame could intermittently fail under heavy list scrolling and silently
//  drop text. A failed measurement leaves the rect alone and degrades to plain
//  near alignment.
//
//  CLIP is applied only when NOT wrapping. A no-wrap draw is a single line
//  confined to its box, so an over-wide value truncates instead of spilling
//  into the neighboring column; wrapped text keeps the older unclipped
//  behavior, where a caller sizing its own box expects overflow to show.
//
//  Color fonts are enabled, so emoji and multi-color glyphs render in color
//  rather than as flat silhouettes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::DrawString (
    const wchar_t      * text,
    float                xDip,
    float                yDip,
    float                widthDip,
    float                heightDip,
    uint32_t             argbColor,
    float                fontSizeDip,
    const wchar_t      * fontFamily,
    HAlign               hAlign,
    VAlign               vAlign,
    DxuiFontWeight       weight,
    bool                 wrap)
{
    HRESULT                         hr         = S_OK;
    ComPtr<IDWriteTextFormat>       format;
    IDWriteTextFormat             * rawFmt     = nullptr;
    ComPtr<ID2D1SolidColorBrush>    brush;
    D2D1_RECT_F                     layoutRect;
    ComPtr<IDWriteTextLayout>       layout;
    IDWriteTextLayout             * rawLayout  = nullptr;
    ID2D1SolidColorBrush          * rawBrush   = nullptr;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_d2dContext);
    CBRA (m_drawing);
    CBRAEx (text, E_INVALIDARG);

    hr = EnsureTextFormat (fontFamily, fontSizeDip, weight, &rawFmt);
    CHRA (hr);

    format.Attach (rawFmt);

    // Alignment / wrapping are configured on the cached layout (EnsureLayout),
    // never on the shared format.

    hr = EnsureBrush (argbColor, &rawBrush);
    CHRA (hr);
    brush.Attach (rawBrush);

    // Global alpha multiplies the brush's ARGB alpha via opacity, re-applied
    // per use since the brush is shared across draws.
    brush->SetOpacity (m_globalAlpha);

    layoutRect.left   = xDip;
    layoutRect.top    = yDip;
    layoutRect.right  = xDip + widthDip;
    layoutRect.bottom = yDip + heightDip;

    if (vAlign == VAlign::CenterOnCapHeight)
    {
        // Shift the layout rect so the font's cap-height midline lands on
        // the vertical center of the rect. The cap-height midline position
        // (capMidY, measured from the rect top under NEAR alignment) depends
        // only on the font family, size, and weight -- not on the text or
        // the rect dimensions -- so it is computed once and cached per
        // format. Recomputing it per cell created ~6 DWrite COM objects on
        // every cell every frame, which under heavy list scrolling could
        // intermittently fail and drop text. See EnsureCapMidY.
        float    capMidY = 0.0f;
        HRESULT  hrCap   = EnsureCapMidY (fontFamily, fontSizeDip, weight, format.Get(), capMidY);

        if (SUCCEEDED (hrCap))
        {
            float  shift = heightDip * 0.5f - capMidY;
            layoutRect.top    += shift;
            layoutRect.bottom += shift;
        }

        // Silent fallback: if measurement failed we leave the rect in
        // place and use NEAR alignment.
    }

    hr = EnsureLayout (text, fontFamily, fontSizeDip, weight,
                       hAlign, vAlign, wrap, widthDip, heightDip, &rawLayout);
    CHRA (hr);
    layout.Attach (rawLayout);

    {
        // No-wrap means single-line clipped to the layout box, so a value wider
        // than its column is truncated horizontally instead of spilling into
        // the neighbour (CLIP stops the overflow). Wrapped text keeps the
        // legacy unclipped behavior. The layout owns the box (widthDip x
        // heightDip); the origin carries any cap-height vertical shift applied
        // to layoutRect above.
        D2D1_DRAW_TEXT_OPTIONS  opts = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;

        if (!wrap)
        {
            opts |= D2D1_DRAW_TEXT_OPTIONS_CLIP;
        }

        m_d2dContext->DrawTextLayout (D2D1::Point2F (layoutRect.left, layoutRect.top),
                                      layout.Get(),
                                      brush.Get(),
                                      opts);
    }

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
//  must hide earlier text rendered underneath). DxuiPainter's FillRect
//  goes through D3D and always flushes before any D2D text, so it
//  cannot cover text drawn earlier in the same frame.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::FillRect (
    float    xDip,
    float    yDip,
    float    widthDip,
    float    heightDip,
    uint32_t argbColor)
{
    HRESULT                            hr     = S_OK;
    ComPtr<ID2D1SolidColorBrush>       brush;
    D2D1_RECT_F                        rect   = {};



    DXUI_ASSERT_UI_THREAD();



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
//  PushClipRect / PopClipRect
//
//  Forwards directly to ID2D1DeviceContext's clip stack. All drawing
//  ops between Push and Pop are clipped to the rect's intersection
//  with currently-active clips. Caller must balance pushes and pops
//  within a single BeginDraw/EndDraw pair.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::PushClipRect (float xDip, float yDip, float widthDip, float heightDip)
{
    HRESULT      hr   = S_OK;
    D2D1_RECT_F  rect = {};



    DXUI_ASSERT_UI_THREAD();



    CBRA (m_d2dContext);
    CBRA (m_drawing);

    rect.left   = xDip;
    rect.top    = yDip;
    rect.right  = xDip + widthDip;
    rect.bottom = yDip + heightDip;

    m_d2dContext->PushAxisAlignedClip (&rect, D2D1_ANTIALIAS_MODE_ALIASED);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopClipRect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::PopClipRect()
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();



    CBRA (m_d2dContext);
    CBRA (m_drawing);

    m_d2dContext->PopAxisAlignedClip();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushTextSkew / PopTextSkew
//
//  Compose a horizontal shear onto the context transform: a point at height y
//  is pushed right by (yPivot - y) * tanX, so vertical strokes lean right and
//  nothing shifts at the pivot. Saves the prior transform for PopTextSkew.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextRenderer::PushTextSkew (float tanX, float yPivotDip)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_d2dContext == nullptr)
    {
        return;
    }

    m_d2dContext->GetTransform (&m_savedTransform);

    D2D1::Matrix3x2F  shear (1.0f, 0.0f,
                             -tanX, 1.0f,
                             tanX * yPivotDip, 0.0f);

    m_d2dContext->SetTransform (shear * (*D2D1::Matrix3x2F::ReinterpretBaseType (&m_savedTransform)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopTextSkew
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextRenderer::PopTextSkew()
{
    DXUI_ASSERT_UI_THREAD();

    if (m_d2dContext == nullptr)
    {
        return;
    }

    m_d2dContext->SetTransform (m_savedTransform);
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

HRESULT DxuiTextRenderer::DrawFramebuffer (
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



    DXUI_ASSERT_UI_THREAD();

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

HRESULT DxuiTextRenderer::DrawIconBitmap (
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



    DXUI_ASSERT_UI_THREAD();

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

HRESULT DxuiTextRenderer::MeasureString (
    const wchar_t  * text,
    float            fontSizeDip,
    const wchar_t  * fontFamily,
    float          & outWidthDip,
    float          & outHeightDip)
{
    HRESULT                            hr        = S_OK;
    ComPtr<IDWriteTextLayout>          layout;
    IDWriteTextLayout                * rawLayout = nullptr;
    DWRITE_TEXT_METRICS                metrics   = {};
    // A literal FLT_MAX layout box makes DirectWrite report width 0 on
    // some D2D targets; use a large FINITE sentinel instead.
    constexpr float                    s_kUnboundedDip = 1.0e6f;



    DXUI_ASSERT_UI_THREAD();

    outWidthDip  = 0.0f;
    outHeightDip = 0.0f;

    CBRAEx (text, E_INVALIDARG);

    // Non-asserting member-state check: callers from chrome layout
    // may invoke MeasureString during window-creation -- before
    // Initialize() has run -- to size a first-pass layout. The
    // chrome falls back to a fixed-width heuristic in that case and
    // re-measures on the next Layout pass once Initialize is done.
    CBR (m_dwriteFactory);

    // Same cached, shaped layout the draw path uses -- keyed here with an
    // unbounded box and default alignment -- so repeated measurements of an
    // unchanged label do no re-shaping.
    hr = EnsureLayout (text, fontFamily, fontSizeDip, DxuiFontWeight::Normal,
                       DxuiTextHAlign::Left, DxuiTextVAlign::Top, false,
                       s_kUnboundedDip, s_kUnboundedDip, &rawLayout);
    CHRA (hr);

    layout.Attach (rawLayout);

    hr = layout->GetMetrics (&metrics);
    CHRA (hr);

    outWidthDip  = metrics.widthIncludingTrailingWhitespace;
    outHeightDip = metrics.height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureStringWrapped
//
//  Word-wrapped twin of MeasureString: the layout box is capped at
//  maxWidthDip with wrapping on, so the metrics report the widest wrapped
//  line and the stacked height a wrapping DrawString will need.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::MeasureStringWrapped (
    const wchar_t  * text,
    float            fontSizeDip,
    const wchar_t  * fontFamily,
    float            maxWidthDip,
    float          & outWidthDip,
    float          & outHeightDip)
{
    HRESULT                            hr        = S_OK;
    ComPtr<IDWriteTextLayout>          layout;
    IDWriteTextLayout                * rawLayout = nullptr;
    DWRITE_TEXT_METRICS                metrics   = {};
    constexpr float                    s_kUnboundedDip = 1.0e6f;



    DXUI_ASSERT_UI_THREAD();

    outWidthDip  = 0.0f;
    outHeightDip = 0.0f;

    CBRAEx (text, E_INVALIDARG);
    CBRAEx (maxWidthDip > 0.0f, E_INVALIDARG);
    CBR (m_dwriteFactory);

    hr = EnsureLayout (text, fontFamily, fontSizeDip, DxuiFontWeight::Normal,
                       DxuiTextHAlign::Left, DxuiTextVAlign::Top, true,
                       maxWidthDip, s_kUnboundedDip, &rawLayout);
    CHRA (hr);

    layout.Attach (rawLayout);

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

HRESULT DxuiTextRenderer::OnDeviceLost()
{
    DXUI_ASSERT_UI_THREAD();

    Shutdown();
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceRestored
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::OnDeviceRestored (ID3D11Device * pDevice)
{
    DXUI_ASSERT_UI_THREAD();

    return Initialize (pDevice);
}

