#pragma once



class IDxuiPainter;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadow
//
//  The soft shadow under a floating surface -- a popup's card, or a flyout
//  drawn inside the window -- painted with ordinary rounded fills, since the
//  painter has no blur.
//
//  The shadow is a stack of rounded rects around the card, each carrying the
//  same small alpha. Coverage at a distance from the card is the number of
//  layers reaching that far, so the SPACING of the layers sets how opacity
//  falls off. Evenly spaced layers give a straight-line ramp, which over a dozen
//  pixels changes too little per pixel to see and reads as one flat band.
//  Spacing by 1 - sqrt(k/N) puts most layers close to the card and a few far
//  out, so opacity falls off quadratically: dense at the edge, a long faint
//  tail -- the way a real shadow looks.
//
//  A surface drawing a shadow into its own buffer needs kMarginDip of room on
//  every side to hold it without clipping.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiShadow
{
public:
    static constexpr float  kBlurDip    = 16.0f;
    static constexpr float  kOffsetYDip = 4.0f;
    static constexpr float  kMarginDip  = 22.0f;   // kBlurDip + kOffsetYDip, plus slack

    static void  Paint (IDxuiPainter & painter,
                        float          xPx,
                        float          yPx,
                        float          widthPx,
                        float          heightPx,
                        float          radiusPx,
                        float          scale);
};
