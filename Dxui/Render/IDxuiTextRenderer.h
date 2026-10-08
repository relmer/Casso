#pragma once

#include "Render/DxuiVectorIcon.h"

#include "Pch.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextRenderer
//
//  Pure-virtual interface for text measurement and drawing. Widgets
//  measure / draw through this interface so they remain mockable for
//  unit tests; the concrete DxuiTextRenderer (Direct2D + DirectWrite)
//  implements it for the runtime.
//
//  Horizontal and vertical alignment enums are namespace-scope so
//  consumers can reference them without pulling in the concrete
//  renderer header. DxuiTextRenderer back-aliases them as nested
//  HAlign / VAlign typedefs for source-compatibility with existing
//  Casso call sites.
//
////////////////////////////////////////////////////////////////////////////////



enum class DxuiTextHAlign
{
    Left   = 0,
    Center = 1,
    Right  = 2,
};


// One vertex of a filled polygon, in DIPs.
struct DxuiPointF
{
    float  x = 0.0f;
    float  y = 0.0f;
};


enum class DxuiTextVAlign
{
    Top                = 0,
    Center             = 1,
    Bottom             = 2,
    CenterOnCapHeight  = 3,
};



class IDxuiTextRenderer
{
public:
    virtual ~IDxuiTextRenderer() = default;

    virtual HRESULT  DrawString    (const wchar_t      * text,
                                    float                xDip,
                                    float                yDip,
                                    float                widthDip,
                                    float                heightDip,
                                    uint32_t             argbColor,
                                    float                fontSizeDip,
                                    const wchar_t      * fontFamily,
                                    DxuiTextHAlign       hAlign = DxuiTextHAlign::Left,
                                    DxuiTextVAlign       vAlign = DxuiTextVAlign::Top,
                                    DxuiFontWeight       weight = DxuiFontWeight::Normal,
                                    bool                 wrap   = true)         = 0;

    virtual HRESULT  PushClipRect  (float xDip, float yDip, float widthDip, float heightDip) = 0;
    virtual HRESULT  PopClipRect   ()                                                        = 0;

    // OFF-SCREEN TEXT, for callers that need glyphs as a TEXTURE rather
    // than on the back buffer -- the desk scene's drive label, which is
    // geometry in the scene and has to be sampled by a shader like any
    // other surface.
    //
    // Between these two calls the renderer draws into a fresh transparent
    // target of the given size, at 96 dpi so a DIP is a pixel, and the
    // usual DrawString applies. End hands back a shader resource view over
    // it and restores whatever target was bound before.
    //
    // WHAT to draw is deliberately not decided here. The shadow treatment
    // lives with the widget that owns it, and this is only the surface to
    // put it on; a text renderer that knew about halos would be the wrong
    // place for the next caller that wants something else.
    //
    // Not pure: a mock that never renders has nothing useful to say here.
    virtual HRESULT  BeginDrawToTexture (UINT widthPx, UINT heightPx)
    {
        UNREFERENCED_PARAMETER (widthPx);
        UNREFERENCED_PARAMETER (heightPx);
        return E_NOTIMPL;
    }

    // The off-screen target's ACTUAL size, which is not always the size
    // BeginDrawToTexture was asked for: the texture is grown to fit and never
    // shrunk, so a later, smaller request reuses a larger surface. A caller
    // laying texture coordinates over what it drew has to know how much of
    // the surface that drawing covered, or it samples the stale remainder.
    //
    // Zero for a renderer with no target, which is also what a mock says.
    virtual void  GetDrawToTextureSize (UINT & outWidthPx, UINT & outHeightPx) const
    {
        outWidthPx  = 0;
        outHeightPx = 0;
    }

    virtual HRESULT  EndDrawToTexture (ID3D11ShaderResourceView ** outSrv)
    {
        UNREFERENCED_PARAMETER (outSrv);
        return E_NOTIMPL;
    }

    // Shear subsequently-drawn text so vertical strokes lean right by tanX
    // (the top edge kicks right relative to yPivotDip; nothing shifts at the
    // pivot). Used to render labels at the //c case-switch slant. Defaulted to
    // a no-op so test mocks and simple renderers ignore it; PopTextSkew undoes
    // the most recent push. Not nestable.
    virtual void     PushTextSkew  (float tanX, float yPivotDip) { (void) tanX; (void) yPivotDip; }
    virtual void     PopTextSkew   ()                            {}

    // Draw color-font glyphs (emoji) as their monochrome outlines in the brush
    // color until the matching pop. A shadow pass wants the glyph's SHAPE in
    // black; with color fonts on, the glyph keeps its own palette and the
    // brush only sets alpha. Defaulted to a no-op for mocks. Not nestable.
    virtual void     PushMonochromeGlyphs ()                     {}
    virtual void     PopMonochromeGlyphs  ()                     {}

    // Font-handle convenience overloads: unpack a theme DxuiFontHandle into
    // the face / size / weight triple. Defaulted (not pure) so existing
    // mocks need not implement them.
    HRESULT  DrawString (const wchar_t          * text,
                         float                    xDip,
                         float                    yDip,
                         float                    widthDip,
                         float                    heightDip,
                         uint32_t                 argbColor,
                         const DxuiFontHandle   & font,
                         DxuiTextHAlign           hAlign = DxuiTextHAlign::Left,
                         DxuiTextVAlign           vAlign = DxuiTextVAlign::Top,
                         bool                     wrap   = true)
    {
        return DrawString (text, xDip, yDip, widthDip, heightDip, argbColor,
                           font.sizeDip, font.face, hAlign, vAlign, font.weight, wrap);
    }

    HRESULT  MeasureString (const wchar_t        * text,
                            const DxuiFontHandle & font,
                            float                & outWidthDip,
                            float                & outHeightDip)
    {
        return MeasureString (text, font.sizeDip, font.face, outWidthDip, outHeightDip);
    }

    virtual HRESULT  FillRect      (float    xDip,
                                    float    yDip,
                                    float    widthDip,
                                    float    heightDip,
                                    uint32_t argbColor)                         = 0;

    // Anti-aliased shapes, drawn in submission order with the text like
    // FillRect. The painter's circles are built from horizontal slices, which
    // shows as a staircase on anything larger than a dot. Defaulted to
    // no-ops so a mock renderer compiles unchanged.
    virtual HRESULT  FillEllipse   (float    cxDip,
                                    float    cyDip,
                                    float    radiusXDip,
                                    float    radiusYDip,
                                    uint32_t argbColor)
    { (void) cxDip; (void) cyDip; (void) radiusXDip; (void) radiusYDip; (void) argbColor; return S_OK; }

    //  A two-tone icon from SVG paths, scaled into the square at x, y. A
    //  renderer that draws no paths draws nothing.
    virtual HRESULT  FillVectorIcon (const DxuiVectorIcon & icon,
                                     float                  xDip,
                                     float                  yDip,
                                     float                  sizeDip,
                                     uint32_t               foreground,
                                     uint32_t               accent)
    { (void) icon; (void) xDip; (void) yDip; (void) sizeDip; (void) foreground; (void) accent; return S_OK; }

    virtual HRESULT  DrawEllipse   (float    cxDip,
                                    float    cyDip,
                                    float    radiusXDip,
                                    float    radiusYDip,
                                    float    thicknessDip,
                                    uint32_t argbColor)
    { (void) cxDip; (void) cyDip; (void) radiusXDip; (void) radiusYDip; (void) thicknessDip; (void) argbColor; return S_OK; }

    virtual HRESULT  DrawLine      (float    x0Dip,
                                    float    y0Dip,
                                    float    x1Dip,
                                    float    y1Dip,
                                    float    thicknessDip,
                                    uint32_t argbColor)
    { (void) x0Dip; (void) y0Dip; (void) x1Dip; (void) y1Dip; (void) thicknessDip; (void) argbColor; return S_OK; }

    // A filled, anti-aliased polygon through the points in order, closed back
    // to the first. Fewer than three points draws nothing.
    virtual HRESULT  FillPolygon   (const DxuiPointF * points,
                                    size_t             count,
                                    uint32_t           argbColor)
    { (void) points; (void) count; (void) argbColor; return S_OK; }

    virtual HRESULT  MeasureString (const wchar_t  * text,
                                    float            fontSizeDip,
                                    const wchar_t  * fontFamily,
                                    float          & outWidthDip,
                                    float          & outHeightDip)              = 0;

    // Measurement in a given weight, for text drawn bold: a bold word is
    // wider than the same word measured in the regular face. The default
    // forwards to the regular measure so renderers without weights keep
    // their existing behavior.
    virtual HRESULT  MeasureStringWeighted (const wchar_t  * text,
                                            float            fontSizeDip,
                                            const wchar_t  * fontFamily,
                                            DxuiFontWeight   weight,
                                            float          & outWidthDip,
                                            float          & outHeightDip)
    {
        UNREFERENCED_PARAMETER (weight);

        return MeasureString (text, fontSizeDip, fontFamily, outWidthDip, outHeightDip);
    }

    // Word-wrapped measurement inside maxWidthDip: outWidthDip is the widest
    // wrapped line, outHeightDip the stacked line height -- the box a
    // wrapping DrawString of the same text needs. The default forwards to
    // the single-line measure so implementations without wrapping keep
    // their existing behavior.
    //  An SVG document drawn into a square, scaled from its viewBox, under
    //  the renderer's global alpha. A renderer that cannot draw SVG says so,
    //  and the caller draws something else.
    virtual HRESULT  DrawSvgIcon (const std::string & svg, float xDip, float yDip, float sizeDip)
    {
        UNREFERENCED_PARAMETER (svg);  UNREFERENCED_PARAMETER (xDip);
        UNREFERENCED_PARAMETER (yDip); UNREFERENCED_PARAMETER (sizeDip);

        return E_NOTIMPL;
    }

    //  Where a point falls in text laid out in a box as DrawString lays it,
    //  wrapped: the index of the character boundary nearest it. The point is
    //  relative to the box's top left. A renderer that cannot lay text out
    //  says so.
    virtual HRESULT  HitTestText (const wchar_t  * text,
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
        UNREFERENCED_PARAMETER (text);        UNREFERENCED_PARAMETER (fontSizeDip);
        UNREFERENCED_PARAMETER (fontFamily);  UNREFERENCED_PARAMETER (widthDip);
        UNREFERENCED_PARAMETER (heightDip);   UNREFERENCED_PARAMETER (hAlign);
        UNREFERENCED_PARAMETER (vAlign);      UNREFERENCED_PARAMETER (xDip);
        UNREFERENCED_PARAMETER (yDip);

        outIndex = 0;
        return E_NOTIMPL;
    }

    //  The boxes a run of characters covers in the same layout, one per line
    //  it touches, relative to the box's top left.
    struct TextRangeRect
    {
        float  x      = 0.0f;
        float  y      = 0.0f;
        float  width  = 0.0f;
        float  height = 0.0f;
    };

    virtual HRESULT  GetTextRangeRects (const wchar_t                * text,
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
        UNREFERENCED_PARAMETER (text);        UNREFERENCED_PARAMETER (fontSizeDip);
        UNREFERENCED_PARAMETER (fontFamily);  UNREFERENCED_PARAMETER (widthDip);
        UNREFERENCED_PARAMETER (heightDip);   UNREFERENCED_PARAMETER (hAlign);
        UNREFERENCED_PARAMETER (vAlign);      UNREFERENCED_PARAMETER (start);
        UNREFERENCED_PARAMETER (length);

        outRects.clear();
        return E_NOTIMPL;
    }

    virtual HRESULT  MeasureStringWrapped (const wchar_t  * text,
                                           float            fontSizeDip,
                                           const wchar_t  * fontFamily,
                                           float            maxWidthDip,
                                           float          & outWidthDip,
                                           float          & outHeightDip)
    {
        UNREFERENCED_PARAMETER (maxWidthDip);

        return MeasureString (text, fontSizeDip, fontFamily, outWidthDip, outHeightDip);
    }

    // A string's width by GDI's whole-pixel advances, as the shell's own
    // list view measures a caption it wraps. Where GDI is not at hand, the
    // renderer's own measure.
    virtual HRESULT  MeasureStringGdi (const wchar_t  * text,
                                       float            fontSizePx,
                                       const wchar_t  * fontFamily,
                                       float          & outWidthPx)
    {
        float  height = 0.0f;

        return MeasureString (text, fontSizePx, fontFamily, outWidthPx, height);
    }

    // Lays out and draws text by GDI's whole-pixel advances while on, as the
    // shell's list view draws its names, rather than DirectWrite's own. Returns
    // the setting it replaced, for the caller to put back.
    virtual bool     SetGdiClassicText (bool on)                               { (void) on; return false; }

    // The distance from one line to the next in GDI's measure, as the shell's
    // list view stacks a caption's lines. Where GDI is not at hand, the
    // renderer's own line height.
    virtual HRESULT  GetLineHeightGdi (float            fontSizePx,
                                       const wchar_t  * fontFamily,
                                       float          & outHeightPx)
    {
        float  width = 0.0f;

        return MeasureString (L"Ag", fontSizePx, fontFamily, width, outHeightPx);
    }

    // Blit an opaque BGRA8 frame that changes from call to call (an emulator
    // framebuffer, a picture preview) scaled into the destination rect.
    // Implementations keep the frame in its own cached GPU bitmap, separate
    // from the icon cache, and re-upload the pixels on every call.
    virtual HRESULT  DrawFramebuffer (const uint32_t * srcBgraPixels,
                                      int              srcWidthPx,
                                      int              srcHeightPx,
                                      float            destXDip,
                                      float            destYDip,
                                      float            destWidthDip,
                                      float            destHeightDip)           = 0;

    // Blit a premultiplied BGRA8 bitmap (e.g. the app icon harvested
    // from an HICON) into the target. Implementations cache the source
    // pixels in a GPU bitmap; callers should keep the buffer stable
    // across frames for the cache to remain hot.
    virtual HRESULT  DrawIconBitmap (const uint32_t * srcBgraPremul,
                                     int              srcWidthPx,
                                     int              srcHeightPx,
                                     float            destXDip,
                                     float            destYDip,
                                     float            destWidthDip,
                                     float            destHeightDip)            = 0;

    // Global alpha multiplier (matches IDxuiPainter::SetGlobalAlpha).
    // Defaulted to a no-op on the interface so test mocks don't have to
    // implement alpha tracking; the concrete DxuiTextRenderer overrides
    // these to fade brushes and bitmap opacity uniformly.
    virtual void   SetGlobalAlpha (float alpha)                               { (void) alpha; }

    // Offset added to every position drawn after it, in pixels (matches
    // IDxuiPainter::SetOrigin). Defaulted to a no-op.
    virtual void   SetOrigin (float xPx, float yPx)                           { (void) xPx; (void) yPx; }
    virtual float  GetGlobalAlpha () const                                    { return 1.0f; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNullTextRenderer
//
//  The renderer for a widget that has to lay itself out before any real one
//  exists: it draws nothing and reports that it cannot measure, so every
//  caller takes its own glyph-width fallback rather than dereferencing a
//  null pointer. A menu bar opened by a unit test with no renderer installed
//  is the case that needs it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiNullTextRenderer : public IDxuiTextRenderer
{
public:
    HRESULT  DrawString    (const wchar_t *, float, float, float, float, uint32_t, float,
                            const wchar_t *, DxuiTextHAlign, DxuiTextVAlign, DxuiFontWeight, bool) override
    {
        return S_OK;
    }

    HRESULT  PushClipRect  (float, float, float, float) override             { return S_OK; }
    HRESULT  PopClipRect   () override                                       { return S_OK; }
    HRESULT  FillRect      (float, float, float, float, uint32_t) override   { return S_OK; }

    HRESULT  MeasureString (const wchar_t *, float, const wchar_t *, float & outWidthDip, float & outHeightDip) override
    {
        outWidthDip  = 0.0f;
        outHeightDip = 0.0f;
        return E_NOTIMPL;
    }

    HRESULT  DrawFramebuffer (const uint32_t *, int, int, float, float, float, float) override
    {
        return S_OK;
    }

    HRESULT  DrawIconBitmap (const uint32_t *, int, int, float, float, float, float) override
    {
        return S_OK;
    }
};
