#include "Pch.h"

#include "Render/DxuiSvgPath.h"
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

    m_d3dDevice = pDevice;

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
//  AddSymbolFont
//
//  Registers an in-memory font and teaches DirectWrite's fallback to reach for
//  it across one codepoint range.
//
//  Fallback rather than a font collection on the format: a format names exactly
//  one family, so putting the symbol font in the collection would mean either a
//  collection holding both it and every system face, or callers switching
//  family mid-sentence and laying the pieces out themselves. Fallback is the
//  mechanism DirectWrite already runs for any character the chosen font lacks,
//  so a private-use codepoint dropped into a Segoe UI string comes back as our
//  glyph, correctly positioned, and MeasureString accounts for it. Emoji in
//  these dialogs already arrive this way, off the system mappings.
//
//  The custom mapping is added BEFORE the system ones, since the builder takes
//  the first match; appending it would let the system's private-use handling
//  answer first.
//
//  The family is read back off the loaded font rather than passed in, so the
//  font file stays the single authority on what it is called.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::AddSymbolFont (
    const void  * pFontBytes,
    size_t        byteCount,
    uint32_t      firstCodepoint,
    uint32_t      lastCodepoint)
{
    HRESULT                                hr              = S_OK;
    IUnknown                             * factoryRaw      = nullptr;
    ComPtr<IDWriteFactory>                 dwrite;
    ComPtr<IDWriteFactory5>                factory;
    ComPtr<IDWriteInMemoryFontFileLoader>  loader;
    ComPtr<IDWriteFontFile>                fontFile;
    ComPtr<IDWriteFontSetBuilder1>         setBuilder;
    ComPtr<IDWriteFontSet>                 fontSet;
    ComPtr<IDWriteFontCollection1>         collection;
    ComPtr<IDWriteFontFamily>              family;
    ComPtr<IDWriteLocalizedStrings>        familyNames;
    ComPtr<IDWriteFontFallbackBuilder>     fallbackBuilder;
    ComPtr<IDWriteFontFallback>            systemFallback;
    ComPtr<IDWriteFontFallback>            fallback;
    DWRITE_UNICODE_RANGE                   range           = {};
    UINT32                                 familyCount     = 0;
    UINT32                                 nameLength      = 0;
    std::wstring                           familyName;
    const wchar_t                        * targetFamily    = nullptr;



    DXUI_ASSERT_UI_THREAD();

    CBRAEx (pFontBytes, E_INVALIDARG);
    CBRAEx (byteCount > 0, E_INVALIDARG);
    CBRAEx (firstCodepoint <= lastCodepoint, E_INVALIDARG);

    // The shared factory hands back the same object every renderer already
    // holds, so no instance is needed to register against it.
    hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
                              __uuidof (IDWriteFactory),
                              &factoryRaw);
    CHRA (hr);

    dwrite.Attach (static_cast<IDWriteFactory *> (factoryRaw));

    hr = dwrite.As (&factory);
    CHRA (hr);

    hr = factory->CreateInMemoryFontFileLoader (&loader);
    CHRA (hr);

    hr = factory->RegisterFontFileLoader (loader.Get());
    CHRA (hr);

    // A null owner means DirectWrite copies the bytes, so the caller's buffer
    // does not have to outlive this call.
    hr = loader->CreateInMemoryFontFileReference (factory.Get(),
                                                 pFontBytes,
                                                 static_cast<UINT32> (byteCount),
                                                 nullptr,
                                                 &fontFile);
    CHRA (hr);

    hr = factory->CreateFontSetBuilder (&setBuilder);
    CHRA (hr);

    hr = setBuilder->AddFontFile (fontFile.Get());
    CHRA (hr);

    hr = setBuilder->CreateFontSet (&fontSet);
    CHRA (hr);

    hr = factory->CreateFontCollectionFromFontSet (fontSet.Get(), &collection);
    CHRA (hr);

    familyCount = collection->GetFontFamilyCount();
    CBRA (familyCount > 0);

    hr = collection->GetFontFamily (0, &family);
    CHRA (hr);

    hr = family->GetFamilyNames (&familyNames);
    CHRA (hr);

    // Sized from the string rather than a fixed buffer: a font whose family
    // name did not fit would fail the whole registration over a name nothing
    // reads but the mapping below.
    hr = familyNames->GetStringLength (0, &nameLength);
    CHRA (hr);

    familyName.resize (static_cast<size_t> (nameLength) + 1);

    hr = familyNames->GetString (0, familyName.data(),
                                 static_cast<UINT32> (familyName.size()));
    CHRA (hr);

    targetFamily = familyName.c_str();

    hr = factory->CreateFontFallbackBuilder (&fallbackBuilder);
    CHRA (hr);

    range.first = firstCodepoint;
    range.last  = lastCodepoint;

    hr = fallbackBuilder->AddMapping (&range,
                                      1,
                                      &targetFamily,
                                      1,
                                      collection.Get(),
                                      nullptr,
                                      nullptr,
                                      1.0f);
    CHRA (hr);

    hr = factory->GetSystemFontFallback (&systemFallback);
    CHRA (hr);

    hr = fallbackBuilder->AddMappings (systemFallback.Get());
    CHRA (hr);

    hr = fallbackBuilder->CreateFontFallback (&fallback);
    CHRA (hr);

    // Published last, together: until this point a failure would have left
    // the collection reading as loaded while no format could ever use it.
    s_symbolFonts  = collection;
    s_fontFallback = fallback;

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

    m_targets.ReleaseOffscreen();
    m_icons.ReleaseBitmaps();
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
//  SetOrigin
//
//  Stores an offset for every position drawn after it, in pixels, and pushes
//  it into the transform at once if a draw is already under way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextRenderer::SetOrigin (float xPx, float yPx)
{
    m_originXPx = xPx;
    m_originYPx = yPx;

    if (m_drawing)
    {
        ApplyOrigin();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyOrigin
//
//  The origin is in pixels and a D2D transform is in DIPs, so the offset is
//  scaled through the context's own DPI rather than assuming 96. The
//  context's DPI is read here, never set here, so whatever it is, this is
//  right for it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextRenderer::ApplyOrigin()
{
    float              dpiX = 0.0f;
    float              dpiY = 0.0f;
    D2D1_MATRIX_3X2_F  m    = {};



    if (!m_d2dContext)
    {
        return;
    }

    m_d2dContext->GetDpi (&dpiX, &dpiY);

    dpiX = (dpiX > 0.0f) ? dpiX : (float) USER_DEFAULT_SCREEN_DPI;
    dpiY = (dpiY > 0.0f) ? dpiY : (float) USER_DEFAULT_SCREEN_DPI;

    m._11 = 1.0f;
    m._22 = 1.0f;
    m._31 = m_originXPx * (float) USER_DEFAULT_SCREEN_DPI / dpiX;
    m._32 = m_originYPx * (float) USER_DEFAULT_SCREEN_DPI / dpiY;

    m_d2dContext->SetTransform (m);
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
    ApplyOrigin();

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
                                                static_cast<DWRITE_FONT_WEIGHT> ((int) weight & 0xFFFF),
                                                (((int) weight & 0x10000) != 0) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                fontSizeDip,
                                                L"en-us",
                                                &format);
        CHRA (hr);

        if (s_fontFallback)
        {
            ComPtr<IDWriteTextFormat1>  format1;

            hr = format.As (&format1);
            CHRA (hr);

            hr = format1->SetFontFallback (s_fontFallback.Get());
            CHRA (hr);
        }

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
    key.gdi     = m_gdiClassic;

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

    if (m_gdiClassic)
    {
        hr = m_dwriteFactory->CreateGdiCompatibleTextLayout (text,
                                                             (UINT32) wcslen (text),
                                                             format.Get(),
                                                             maxWidthDip,
                                                             maxHeightDip,
                                                             1.0f,
                                                             nullptr,
                                                             FALSE,
                                                             &layout);
    }
    else
    {
        hr = m_dwriteFactory->CreateTextLayout (text,
                                                (UINT32) wcslen (text),
                                                format.Get(),
                                                maxWidthDip,
                                                maxHeightDip,
                                                &layout);
    }

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
    float                          kMeasureBox = 4096.0f;
    UINT32                         familyIndex = 0;
    DWRITE_FONT_METRICS            metrics     = {};
    DWRITE_LINE_METRICS            lineMetrics = {};
    UINT32                         lineCount   = 0;
    const wchar_t                * useFamily     = (family != nullptr) ? family : L"Segoe UI";
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
//  A BOX WITH NO AREA DRAWS NOTHING, and says so here rather than at the
//  DirectWrite call. Widgets size themselves from a client rect, and a window
//  that has none yet -- one shown minimized, or mid-resize -- gives every one
//  of them a zero width; a widget that then insets that width by its padding
//  passes a NEGATIVE one down. CreateTextLayout refuses a negative extent with
//  E_INVALIDARG, so without this the first paint of such a window fails an
//  assertion, and it is a paint, so it fails again on the next frame and the
//  next. Nothing is lost by declining: an unwrapped draw is clipped to its box
//  and an empty box shows none of it anyway.
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

    BAIL_OUT_IF (widthDip <= 0.0f || heightDip <= 0.0f, S_OK);

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
        D2D1_DRAW_TEXT_OPTIONS  opts = m_monochromeGlyphs ? D2D1_DRAW_TEXT_OPTIONS_NONE
                                                          : D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;

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
//  FillEllipse
//
//  An anti-aliased filled ellipse through the D2D context.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::FillEllipse (float cxDip, float cyDip, float radiusXDip, float radiusYDip, uint32_t argbColor)
{
    HRESULT                       hr = S_OK;
    ComPtr<ID2D1SolidColorBrush>  brush;



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

    m_d2dContext->FillEllipse (D2D1::Ellipse (D2D1::Point2F (cxDip, cyDip), radiusXDip, radiusYDip), brush.Get());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawEllipse
//
//  An anti-aliased ellipse outline through the D2D context.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::DrawEllipse (float cxDip, float cyDip, float radiusXDip, float radiusYDip, float thicknessDip, uint32_t argbColor)
{
    HRESULT                       hr = S_OK;
    ComPtr<ID2D1SolidColorBrush>  brush;



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

    m_d2dContext->DrawEllipse (D2D1::Ellipse (D2D1::Point2F (cxDip, cyDip), radiusXDip, radiusYDip), brush.Get(), thicknessDip);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawLine
//
//  An anti-aliased line through the D2D context.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::DrawLine (float x0Dip, float y0Dip, float x1Dip, float y1Dip, float thicknessDip, uint32_t argbColor)
{
    HRESULT                       hr = S_OK;
    ComPtr<ID2D1SolidColorBrush>  brush;



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

    m_d2dContext->DrawLine (D2D1::Point2F (x0Dip, y0Dip), D2D1::Point2F (x1Dip, y1Dip), brush.Get(), thicknessDip);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillPolygon
//
//  An anti-aliased filled polygon through the D2D context: one closed figure
//  through the points in order. Fewer than three points is not a polygon and
//  draws nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::FillPolygon (const DxuiPointF * points, size_t count, uint32_t argbColor)
{
    constexpr size_t               kMinVertices = 3;
    HRESULT                        hr           = S_OK;
    ComPtr<ID2D1SolidColorBrush>   brush;
    ComPtr<ID2D1PathGeometry1>     geometry;
    ComPtr<ID2D1GeometrySink>      sink;
    bool                           isPolygon    = points != nullptr && count >= kMinVertices;
    size_t                         i            = 0;



    DXUI_ASSERT_UI_THREAD();

    BAIL_OUT_IF (!isPolygon, S_OK);

    CBRA (m_d2dContext);
    CBRA (m_d2dFactory);
    CBRA (m_drawing);

    hr = m_d2dFactory->CreatePathGeometry (&geometry);
    CHRA (hr);

    hr = geometry->Open (&sink);
    CHRA (hr);

    sink->BeginFigure (D2D1::Point2F (points[0].x, points[0].y), D2D1_FIGURE_BEGIN_FILLED);

    for (i = 1; i < count; i++)
    {
        sink->AddLine (D2D1::Point2F (points[i].x, points[i].y));
    }

    sink->EndFigure (D2D1_FIGURE_END_CLOSED);

    hr = sink->Close();
    CHRA (hr);

    hr = m_d2dContext->CreateSolidColorBrush (ColorFromArgb (argbColor), &brush);
    CHRA (hr);

    if (m_globalAlpha < 1.0f)
    {
        D2D1_COLOR_F  scaled = brush->GetColor();

        scaled.a *= m_globalAlpha;
        brush->SetColor (scaled);
    }

    m_d2dContext->FillGeometry (geometry.Get(), brush.Get());

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
//  EraseRoundedRect
//
//  Clears a rounded rect of the target to transparent, edges antialiased:
//  a copy, not a blend, so what was drawn there is gone rather than covered.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::EraseRoundedRect (float xDip, float yDip, float widthDip, float heightDip, float radiusDip)
{
    HRESULT                        hr      = S_OK;
    ComPtr<ID2D1SolidColorBrush>   brush;
    D2D1_ROUNDED_RECT              rounded = {};



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_d2dContext);
    CBRA (m_drawing);

    hr = m_d2dContext->CreateSolidColorBrush (D2D1::ColorF (0.0f, 0.0f, 0.0f, 0.0f), &brush);
    CHRA (hr);

    rounded.rect    = D2D1::RectF (xDip, yDip, xDip + widthDip, yDip + heightDip);
    rounded.radiusX = radiusDip;
    rounded.radiusY = radiusDip;

    m_d2dContext->SetPrimitiveBlend    (D2D1_PRIMITIVE_BLEND_COPY);
    m_d2dContext->FillRoundedRectangle (&rounded, brush.Get());
    m_d2dContext->SetPrimitiveBlend    (D2D1_PRIMITIVE_BLEND_SOURCE_OVER);

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
    return MeasureStringWeighted (text, fontSizeDip, fontFamily, DxuiFontWeight::Normal, outWidthDip, outHeightDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureStringWeighted
//
//  MeasureString in a given weight, through the same cached layout the draw
//  path uses for that weight, so a bold word measures as wide as it draws.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::MeasureStringWeighted (
    const wchar_t  * text,
    float            fontSizeDip,
    const wchar_t  * fontFamily,
    DxuiFontWeight   weight,
    float          & outWidthDip,
    float          & outHeightDip)
{
    HRESULT                      hr        = S_OK;
    ComPtr<IDWriteTextLayout>    layout;
    IDWriteTextLayout          * rawLayout = nullptr;
    DWRITE_TEXT_METRICS          metrics   = {};
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
    hr = EnsureLayout (text, fontFamily, fontSizeDip, weight,
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
//  HitTestText
//
//  The same cached, shaped layout a wrapping DrawString of this box uses, so
//  a point lands on the character drawn under it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::HitTestText (
    const wchar_t  * text,
    float            fontSizeDip,
    const wchar_t  * fontFamily,
    float            widthDip,
    float            heightDip,
    DxuiTextHAlign   hAlign,
    DxuiTextVAlign   vAlign,
    float            xDip,
    float            yDip,
    size_t         & outIndex)
{
    HRESULT                    hr        = S_OK;
    ComPtr<IDWriteTextLayout>  layout;
    IDWriteTextLayout        * rawLayout = nullptr;
    BOOL                       trailing  = FALSE;
    BOOL                       inside    = FALSE;
    DWRITE_HIT_TEST_METRICS    metrics   = {};



    DXUI_ASSERT_UI_THREAD();

    outIndex = 0;

    CBRAEx (text, E_INVALIDARG);
    CBR (m_dwriteFactory);

    hr = EnsureLayout (text, fontFamily, fontSizeDip, DxuiFontWeight::Normal, hAlign, vAlign, true,
                       widthDip, heightDip, &rawLayout);
    CHRA (hr);

    layout.Attach (rawLayout);

    hr = layout->HitTestPoint (xDip, yDip, &trailing, &inside, &metrics);
    CHRA (hr);

    outIndex = (size_t) metrics.textPosition + (trailing ? metrics.length : 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTextRangeRects
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::GetTextRangeRects (
    const wchar_t                * text,
    float                          fontSizeDip,
    const wchar_t                * fontFamily,
    float                          widthDip,
    float                          heightDip,
    DxuiTextHAlign                 hAlign,
    DxuiTextVAlign                 vAlign,
    size_t                         start,
    size_t                         length,
    std::vector<TextRangeRect>   & outRects)
{
    HRESULT                               hr        = S_OK;
    ComPtr<IDWriteTextLayout>             layout;
    IDWriteTextLayout                   * rawLayout = nullptr;
    UINT32                                count     = 0;
    std::vector<DWRITE_HIT_TEST_METRICS>  metrics;



    DXUI_ASSERT_UI_THREAD();

    outRects.clear();

    CBRAEx (text, E_INVALIDARG);
    CBR (m_dwriteFactory);
    BAIL_OUT_IF (length == 0, S_OK);

    hr = EnsureLayout (text, fontFamily, fontSizeDip, DxuiFontWeight::Normal, hAlign, vAlign, true,
                       widthDip, heightDip, &rawLayout);
    CHRA (hr);

    layout.Attach (rawLayout);

    //  Asked once for the count, then for the boxes.
    hr = layout->HitTestTextRange ((UINT32) start, (UINT32) length, 0.0f, 0.0f, nullptr, 0, &count);
    BAIL_OUT_IF (hr != E_NOT_SUFFICIENT_BUFFER && FAILED (hr), hr);

    metrics.resize (count);

    hr = layout->HitTestTextRange ((UINT32) start, (UINT32) length, 0.0f, 0.0f, metrics.data(), count, &count);
    CHRA (hr);

    for (UINT32 i = 0; i < count; i++)
    {
        outRects.push_back (TextRangeRect { metrics[i].left, metrics[i].top, metrics[i].width, metrics[i].height });
    }

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
    HRESULT                            hr              = S_OK;
    ComPtr<IDWriteTextLayout>          layout;
    IDWriteTextLayout                * rawLayout       = nullptr;
    DWRITE_TEXT_METRICS                metrics         = {};
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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRenderer::IsFontFamilyInstalled
//
//  Through a shared factory of its own, so it can be asked before any window
//  or renderer exists.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextRenderer::IsFontFamilyInstalled (const wchar_t * family)
{
    HRESULT                        hr         = S_OK;
    ComPtr<IDWriteFactory>         factory;
    ComPtr<IDWriteFontCollection>  collection;
    UINT32                         index      = 0;
    BOOL                           found      = FALSE;



    CBRAEx (family != nullptr, E_INVALIDARG);

    hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory),
                              reinterpret_cast<IUnknown **> (factory.GetAddressOf()));
    CHR (hr);

    hr = factory->GetSystemFontCollection (&collection, FALSE);
    CHR (hr);

    hr = collection->FindFamilyName (family, &index, &found);
    CHR (hr);

Error:
    return SUCCEEDED (hr) && found != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRenderer::MeasureWrappedDip
//
//  Through a shared factory of its own, as IsFontFamilyInstalled is. Without
//  the symbol fallback a private-use glyph measures as the system's missing
//  glyph, which is no taller than a line.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTextRenderer::MeasureWrappedDip (const wchar_t * text, float fontSizeDip, const wchar_t * fontFamily, DxuiFontWeight weight,
                                             float widthDip, float & outHeightDip)
{
    HRESULT                     hr      = S_OK;
    ComPtr<IDWriteFactory>      factory;
    ComPtr<IDWriteTextFormat>   format;
    ComPtr<IDWriteTextLayout>   layout;
    DWRITE_TEXT_METRICS         metrics = {};



    outHeightDip = 0.0f;

    CBRAEx (text != nullptr && fontFamily != nullptr, E_INVALIDARG);
    CBRAEx (widthDip > 0.0f, E_INVALIDARG);

    hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory),
                              reinterpret_cast<IUnknown **> (factory.GetAddressOf()));
    CHR (hr);

    hr = factory->CreateTextFormat (fontFamily, nullptr,
                                    static_cast<DWRITE_FONT_WEIGHT> ((int) weight & 0xFFFF),
                                    (((int) weight & 0x10000) != 0) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                    DWRITE_FONT_STRETCH_NORMAL, fontSizeDip, L"en-us", &format);
    CHR (hr);

    hr = factory->CreateTextLayout (text, (UINT32) wcslen (text), format.Get(), widthDip, 1.0e6f, &layout);
    CHR (hr);

    hr = layout->GetMetrics (&metrics);
    CHR (hr);

    outHeightDip = metrics.height;

Error:
    return hr;
}
