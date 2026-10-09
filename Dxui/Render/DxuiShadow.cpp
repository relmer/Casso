#include "Pch.h"

#include "Render/DxuiShadow.h"
#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadow::GetMarginDip
//
//  The shadow's reach below the card, which is its farthest, plus a little
//  slack for the last layer's antialiased edge.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiShadow::GetMarginDip (const Style & style)
{
    constexpr float  kSlackDip = 2.0f;



    return style.blurDip + style.offsetYDip + kSlackDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadow::Paint
//
//  Draws the shadow for a card at the given rect and radius. The card itself
//  is the caller's to draw afterwards, over the part of the shadow beneath it.
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
    Paint (painter, xPx, yPx, widthPx, heightPx, radiusPx, scale, 0, Style());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadow::Paint (in a color)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiShadow::Paint (
    IDxuiPainter & painter,
    float          xPx,
    float          yPx,
    float          widthPx,
    float          heightPx,
    float          radiusPx,
    float          scale,
    uint32_t       rgb)
{
    Paint (painter, xPx, yPx, widthPx, heightPx, radiusPx, scale, rgb, Style());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadow::Paint (in a color and a style)
//
//  Layer k of N reaches blur * (1 - sqrt(k/N)) past a card style.insetDip
//  inside the real one, so the number of layers covering a point at distance
//  d past that card is about N * (1 - d/blur)^2. Every layer has the alpha
//  that makes N of them accumulate to the style's opacity under
//  premultiplied source-over, so the edge lands on it whatever N is. A card
//  inset puts the darkest layers under the card, so the shadow beside a
//  small card stays faint.
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
    float          scale,
    uint32_t       rgb,
    const Style  & style)
{
    constexpr int    kLayers  = 12;
    float            blur     = style.blurDip    * scale;
    float            offsetY  = style.offsetYDip * scale;
    float            inset    = style.insetDip   * scale;
    float            layerA   = 1.0f - powf (1.0f - style.opacity, 1.0f / (float) kLayers);
    uint32_t         argb     = (((uint32_t) (layerA * 255.0f + 0.5f)) << 24) | (rgb & 0x00FFFFFFu);
    int              k        = 0;



    for (k = 1; k <= kLayers; k++)
    {
        float  spread = blur * (1.0f - sqrtf ((float) k / (float) kLayers)) - inset;

        painter.FillRoundedRect (xPx      - spread,
                                 yPx      - spread + offsetY,
                                 widthPx  + spread * 2.0f,
                                 heightPx + spread * 2.0f,
                                 (std::max) (0.0f, radiusPx + spread),
                                 argb);
    }
}
