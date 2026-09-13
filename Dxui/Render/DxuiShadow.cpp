#include "Pch.h"

#include "Render/DxuiShadow.h"
#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadow::Paint
//
//  Draws the shadow for a card at the given rect and radius. The card itself
//  is the caller's to draw afterwards, over the part of the shadow beneath it.
//
//  Layer k of N reaches blur * (1 - sqrt(k/N)) past the card, so the number of
//  layers covering a point at distance d is about N * (1 - d/blur)^2. Every
//  layer carries the alpha that makes N of them accumulate to kOpacity under
//  premultiplied source-over, so the edge lands on kOpacity whatever N is.
//
//  The whole stack is offset downward: light from above, as a Windows 11
//  flyout's shadow is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiShadow::Paint (
    IDxuiPainter & painter,
    float          xPx,
    float          yPx,
    float          widthPx,
    float          heightPx,
    float          radiusPx,
    float          scale)
{
    constexpr float  kOpacity = 0.32f;
    constexpr int    kLayers  = 12;
    float            blur     = kBlurDip    * scale;
    float            offsetY  = kOffsetYDip * scale;
    float            layerA   = 1.0f - powf (1.0f - kOpacity, 1.0f / (float) kLayers);
    uint32_t         argb     = ((uint32_t) (layerA * 255.0f + 0.5f)) << 24;
    int              k        = 0;



    for (k = 1; k <= kLayers; k++)
    {
        float  spread = blur * (1.0f - sqrtf ((float) k / (float) kLayers));

        painter.FillRoundedRect (xPx      - spread,
                                 yPx      - spread + offsetY,
                                 widthPx  + spread * 2.0f,
                                 heightPx + spread * 2.0f,
                                 radiusPx + spread,
                                 argb);
    }
}
