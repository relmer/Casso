#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DwriteTextRenderer
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

class DwriteTextRenderer
{
public:
    DwriteTextRenderer  () = default;
    ~DwriteTextRenderer ();

    HRESULT  Initialize       (ID3D11Device * pDevice);
    void     Shutdown         ();

    HRESULT  BindBackBuffer   (IDXGISurface * pBackBufferSurface,
                               UINT           dpiX,
                               UINT           dpiY);
    void     UnbindBackBuffer ();

    HRESULT  BeginDraw        ();
    HRESULT  EndDraw          ();

    enum class HAlign
    {
        Left   = 0,
        Center = 1,
        Right  = 2,
    };

    enum class VAlign
    {
        Top                = 0,
        // Centers the LINE BOX vertically. For typical Latin fonts
        // the line box extends further above cap-height than below
        // baseline, so the visible glyph cluster sits ABOVE the
        // rect's geometric midline. Cheap, but mis-aligns text
        // against icons sized to row geometry.
        Center             = 1,
        Bottom             = 2,
        // Centers cap-height midline at the rect's vertical center.
        // Uses the font face's metrics (ascent / descent / cap
        // height) to compute the offset, so the visible center of
        // the rendered ASCII text matches the geometric center of
        // the rect. Use this when aligning text against icons or
        // other geometry-centered widgets in the same row.
        CenterOnCapHeight  = 3,
    };

    HRESULT  DrawString       (const wchar_t * text,
                               float           xDip,
                               float           yDip,
                               float           widthDip,
                               float           heightDip,
                               uint32_t        argbColor,
                               float           fontSizeDip,
                               const wchar_t * fontFamily,
                               HAlign          hAlign = HAlign::Left,
                               VAlign          vAlign = VAlign::Top,
                               DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                               bool            wrap   = true);

    // Push an axis-aligned clip rect onto the d2d context. All
    // subsequent DrawString / FillRect calls are clipped to the
    // intersection of currently-active clips until the matching
    // PopClipRect. Used by single-line text inputs to clip their
    // scrolling text content to the visible inner rect.
    HRESULT  PushClipRect     (float xDip, float yDip, float widthDip, float heightDip);
    HRESULT  PopClipRect      ();

    HRESULT  FillRect         (float    xDip,
                               float    yDip,
                               float    widthDip,
                               float    heightDip,
                               uint32_t argbColor);

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
                               float            destHeightDip);

    HRESULT  MeasureString    (const wchar_t * text,
                               float           fontSizeDip,
                               const wchar_t * fontFamily,
                               float         & outWidthDip,
                               float         & outHeightDip);

    HRESULT  OnDeviceLost     ();
    HRESULT  OnDeviceRestored (ID3D11Device * pDevice);

    bool     IsTargetBound    () const { return m_targetBound; }

    // Global alpha multiplier (matches DxUiPainter::SetGlobalAlpha).
    // Pre-multiplied into every brush's alpha channel and into the
    // opacity arg of DrawBitmap so a single switch fades all text,
    // filled rects, and the framebuffer preview uniformly.
    void     SetGlobalAlpha   (float alpha) { m_globalAlpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha; }
    float    GlobalAlpha      () const      { return m_globalAlpha; }

private:
    struct TextFormatKey
    {
        std::wstring        family;
        float               sizeDip = 0.0f;
        DWRITE_FONT_WEIGHT  weight  = DWRITE_FONT_WEIGHT_NORMAL;

        bool operator < (const TextFormatKey & other) const
        {
            if (family != other.family) { return family < other.family; }
            if (sizeDip != other.sizeDip) { return sizeDip < other.sizeDip; }
            return weight < other.weight;
        }
    };


    HRESULT  EnsureTextFormat (const wchar_t                * family,
                               float                          fontSizeDip,
                               DWRITE_FONT_WEIGHT             weight,
                               IDWriteTextFormat           ** outFormat);


    ComPtr<ID2D1Factory1>             m_d2dFactory;
    ComPtr<ID2D1Device>               m_d2dDevice;
    ComPtr<ID2D1DeviceContext>        m_d2dContext;
    ComPtr<ID2D1Bitmap1>              m_target;
    ComPtr<ID2D1Bitmap>               m_framebufferBitmap;
    int                               m_framebufferBitmapW = 0;
    int                               m_framebufferBitmapH = 0;
    ComPtr<ID2D1Bitmap>               m_iconBitmap;
    int                               m_iconBitmapW = 0;
    int                               m_iconBitmapH = 0;

    ComPtr<IDWriteFactory>            m_dwriteFactory;

    std::map<TextFormatKey,
             ComPtr<IDWriteTextFormat>>  m_formatCache;

    bool                              m_targetBound = false;
    bool                              m_drawing     = false;
    float                             m_globalAlpha = 1.0f;
};
