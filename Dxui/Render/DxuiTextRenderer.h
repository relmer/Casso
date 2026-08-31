#pragma once

#include "Render/IDxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRenderer
//
//  Direct2D-on-Direct3D11 text renderer. Owns a Direct2D device +
//  context bound to a back-buffer surface acquired from the swap chain
//  via `IDXGISurface`, plus a DirectWrite factory and a tiny cache of
//  text formats keyed by `(family, weight, size, dpi)`. Geometry
//  emitted between `BeginDraw` and `EndDraw` composites on top of
//  whatever the D3D pipeline drew earlier in the same frame.
//
//  Lifetime: `Initialize` allocates the Direct2D factory + device
//  against the shared `ID3D11Device` (which MUST have been created
//  with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`). `BindBackBuffer` rebinds
//  the target bitmap whenever the swap chain resizes. `OnDeviceLost`
//  drops every D2D resource so a subsequent `OnDeviceRestored` can
//  rebuild against the new device.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextRenderer : public IDxuiTextRenderer
{
public:
    DxuiTextRenderer  () = default;
    ~DxuiTextRenderer ();

    HRESULT  Initialize       (ID3D11Device * pDevice);
    void     Shutdown         ();

    HRESULT  BindBackBuffer   (IDXGISurface * pBackBufferSurface,
                               UINT           dpiX,
                               UINT           dpiY);
    void     UnbindBackBuffer ();

    HRESULT  BeginDraw        ();
    HRESULT  EndDraw          ();

    HRESULT  BeginDrawOffscreen ();
    HRESULT  EndDrawComposite   ();

    // Glyphs as a TEXTURE. See IDxuiTextRenderer for what these are for;
    // the target is created on demand and grown to fit, and the view handed
    // back stays valid until the next BeginDrawToTexture.
    HRESULT  BeginDrawToTexture (UINT widthPx, UINT heightPx) override;
    HRESULT  EndDrawToTexture   (ID3D11ShaderResourceView ** outSrv) override;

    // Source-compatibility aliases for existing call sites that
    // reference `DxuiTextRenderer::HAlign` / `::VAlign`. The canonical
    // names are the namespace-scope enums declared in IDxuiTextRenderer.h.
    using HAlign = DxuiTextHAlign;
    using VAlign = DxuiTextVAlign;

    HRESULT  DrawString       (const wchar_t * text,
                               float           xDip,
                               float           yDip,
                               float           widthDip,
                               float           heightDip,
                               uint32_t        argbColor,
                               float           fontSizeDip,
                               const wchar_t * fontFamily,
                               DxuiTextHAlign  hAlign = DxuiTextHAlign::Left,
                               DxuiTextVAlign  vAlign = DxuiTextVAlign::Top,
                               DxuiFontWeight  weight = DxuiFontWeight::Normal,
                               bool            wrap   = true) override;

    // Push an axis-aligned clip rect onto the d2d context. All
    // subsequent DrawString / FillRect calls are clipped to the
    // intersection of currently-active clips until the matching
    // PopClipRect. Used by single-line text inputs to clip their
    // scrolling text content to the visible inner rect.
    HRESULT  PushClipRect     (float xDip, float yDip, float widthDip, float heightDip) override;
    HRESULT  PopClipRect      () override;

    void     PushTextSkew     (float tanX, float yPivotDip) override;
    void     PopTextSkew      () override;

    HRESULT  FillRect         (float    xDip,
                               float    yDip,
                               float    widthDip,
                               float    heightDip,
                               uint32_t argbColor) override;

    // Uploads a CPU-side BGRA8 framebuffer into a cached ID2D1Bitmap
    // and draws it into the destination rect with linear filtering.
    // Used by the Settings → Theme preview to show the live emulator
    // image inside the mock window. The bitmap is recreated if srcW
    // or srcH changes; otherwise CopyFromMemory uploads the new
    // pixels every call (cheap at 560x384 = 860 KB).
    HRESULT  DrawFramebuffer  (const uint32_t * srcBgraPixels,
                               int              srcWidthPx,
                               int              srcHeightPx,
                               float            destXDip,
                               float            destYDip,
                               float            destWidthDip,
                               float            destHeightDip);

    // Same shape as DrawFramebuffer but uses a SEPARATE cached
    // ID2D1Bitmap so the emulator framebuffer cache (which gets
    // refreshed every theme-preview frame) doesn't thrash against
    // the title-bar icon cache (which is stable for the app's
    // lifetime). Source pixels MUST be premultiplied BGRA8.
    HRESULT  DrawIconBitmap   (const uint32_t * srcBgraPremul,
                               int              srcWidthPx,
                               int              srcHeightPx,
                               float            destXDip,
                               float            destYDip,
                               float            destWidthDip,
                               float            destHeightDip) override;

    HRESULT  MeasureString    (const wchar_t * text,
                               float           fontSizeDip,
                               const wchar_t * fontFamily,
                               float         & outWidthDip,
                               float         & outHeightDip) override;

    HRESULT  MeasureStringWrapped (const wchar_t * text,
                                   float           fontSizeDip,
                                   const wchar_t * fontFamily,
                                   float           maxWidthDip,
                                   float         & outWidthDip,
                                   float         & outHeightDip) override;

    HRESULT  OnDeviceLost     ();
    HRESULT  OnDeviceRestored (ID3D11Device * pDevice);

    bool     IsTargetBound    () const { return m_targetBound; }

    // Global alpha multiplier (matches DxuiPainter::SetGlobalAlpha).
    // Pre-multiplied into every brush's alpha channel and into the
    // opacity arg of DrawBitmap so a single switch fades all text,
    // filled rects, and the framebuffer preview uniformly.
    void     SetGlobalAlpha   (float alpha) override { m_globalAlpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha; }
    float    GetGlobalAlpha   () const      override { return m_globalAlpha; }

private:
    static D2D1_COLOR_F  ColorFromArgb (uint32_t argbColor);

    struct TextFormatKey
    {
        std::wstring        family;
        float               sizeDip = 0.0f;
        DxuiFontWeight      weight  = DxuiFontWeight::Normal;

        bool operator < (const TextFormatKey & other) const
        {
            if (family != other.family) { return family < other.family; }
            if (sizeDip != other.sizeDip) { return sizeDip < other.sizeDip; }
            return weight < other.weight;
        }
    };


    // Key for the shaped-text-layout cache. A DirectWrite text layout owns the
    // font-fallback + shaping work, which is expensive and allocation-heavy;
    // caching one per distinct (text, format, alignment, box) lets repeated
    // frames re-draw unchanged chrome via DrawTextLayout with no re-shaping.
    struct LayoutCacheKey
    {
        std::wstring        text;
        std::wstring        family;
        float               sizeDip = 0.0f;
        DxuiFontWeight      weight  = DxuiFontWeight::Normal;
        int                 hAlign  = 0;
        int                 vAlign  = 0;
        bool                wrap    = false;
        float               maxW    = 0.0f;
        float               maxH    = 0.0f;

        bool operator < (const LayoutCacheKey & o) const
        {
            if (text    != o.text)    { return text    < o.text;    }
            if (family  != o.family)  { return family  < o.family;  }
            if (sizeDip != o.sizeDip) { return sizeDip < o.sizeDip; }
            if (weight  != o.weight)  { return weight  < o.weight;  }
            if (hAlign  != o.hAlign)  { return hAlign  < o.hAlign;  }
            if (vAlign  != o.vAlign)  { return vAlign  < o.vAlign;  }
            if (wrap    != o.wrap)    { return wrap    < o.wrap;    }
            if (maxW    != o.maxW)    { return maxW    < o.maxW;    }
            return maxH < o.maxH;
        }
    };


    HRESULT  EnsureTextFormat (const wchar_t                * family,
                               float                          fontSizeDip,
                               DxuiFontWeight                 weight,
                               IDWriteTextFormat           ** outFormat);


    HRESULT  EnsureCapMidY    (const wchar_t                * family,
                               float                          fontSizeDip,
                               DxuiFontWeight                 weight,
                               IDWriteTextFormat            * format,
                               float                        & outCapMidY);


    // Cached solid-color brush keyed by ARGB (opacity re-applied per use).
    HRESULT  EnsureBrush      (uint32_t                       argb,
                               ID2D1SolidColorBrush        ** outBrush);


    // Cached shaped text layout (see LayoutCacheKey). Alignment / wrapping are
    // set on the layout itself so the shared format is never mutated.
    HRESULT  EnsureLayout     (const wchar_t                * text,
                               const wchar_t                * family,
                               float                          fontSizeDip,
                               DxuiFontWeight                 weight,
                               DxuiTextHAlign                 hAlign,
                               DxuiTextVAlign                 vAlign,
                               bool                           wrap,
                               float                          maxWidthDip,
                               float                          maxHeightDip,
                               IDWriteTextLayout           ** outLayout);


    ComPtr<ID2D1Factory1>       m_d2dFactory;
    ComPtr<ID2D1Device>         m_d2dDevice;
    ComPtr<ID2D1DeviceContext>  m_d2dContext;
    D2D1_MATRIX_3X2_F           m_savedTransform     = D2D1::Matrix3x2F::Identity();
    ComPtr<ID2D1Bitmap1>        m_target;
    ComPtr<ID2D1Bitmap1>        m_offscreen;

    // The text-as-texture target, and the D3D texture behind it that the
    // scene samples. Kept across calls and only recreated when a larger
    // one is asked for, since a label is re-rendered only when it changes.
    // The D3D device the D2D one rides on, kept so the text-as-texture
    // target can be created. Everything else here works through D2D.
    ComPtr<ID3D11Device>              m_d3dDevice;

    ComPtr<ID3D11Texture2D>           m_textureTarget;
    ComPtr<ID3D11ShaderResourceView>  m_textureSrv;
    ComPtr<ID2D1Bitmap1>              m_textureBitmap;
    ComPtr<ID2D1Image>                m_savedTarget;
    UINT                              m_textureW           = 0;
    UINT                              m_textureH           = 0;
    UINT                              m_offscreenW         = 0;
    UINT                              m_offscreenH         = 0;
    ComPtr<ID2D1Bitmap>               m_framebufferBitmap;
    int                               m_framebufferBitmapW = 0;
    int                               m_framebufferBitmapH = 0;
    ComPtr<ID2D1Bitmap>               m_iconBitmap;
    int                               m_iconBitmapW        = 0;
    int                               m_iconBitmapH        = 0;

    ComPtr<IDWriteFactory>            m_dwriteFactory;

    std::map<TextFormatKey,
             ComPtr<IDWriteTextFormat>>  m_formatCache;

    std::map<TextFormatKey, float>       m_capMidCache;

    std::map<uint32_t,
             ComPtr<ID2D1SolidColorBrush>>  m_brushCache;

    std::map<LayoutCacheKey,
             ComPtr<IDWriteTextLayout>>     m_layoutCache;

    bool                              m_targetBound = false;
    bool                              m_drawing     = false;
    float                             m_globalAlpha = 1.0f;
};
