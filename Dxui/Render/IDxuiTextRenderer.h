#pragma once

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

    virtual HRESULT  MeasureString (const wchar_t  * text,
                                    float            fontSizeDip,
                                    const wchar_t  * fontFamily,
                                    float          & outWidthDip,
                                    float          & outHeightDip)              = 0;

    // Word-wrapped measurement inside maxWidthDip: outWidthDip is the widest
    // wrapped line, outHeightDip the stacked line height -- the box a
    // wrapping DrawString of the same text needs. The default forwards to
    // the single-line measure so implementations without wrapping keep
    // their existing behavior.
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
    virtual float  GetGlobalAlpha () const                                    { return 1.0f; }
};
