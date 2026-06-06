#pragma once

#include "Pch.h"



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
                                    DWRITE_FONT_WEIGHT   weight = DWRITE_FONT_WEIGHT_NORMAL,
                                    bool                 wrap   = true)         = 0;

    virtual HRESULT  PushClipRect  (float xDip, float yDip, float widthDip, float heightDip) = 0;
    virtual HRESULT  PopClipRect   ()                                                        = 0;

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
    virtual void   SetGlobalAlpha   (float alpha)                               { (void) alpha; }
    virtual float  GlobalAlpha      () const                                    { return 1.0f; }
};
