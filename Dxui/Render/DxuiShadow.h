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
//  every side to hold it without clipping, or GetMarginDip of a smaller one.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiShadow
{
public:
    static constexpr float  kBlurDip    = 16.0f;
    static constexpr float  kOffsetYDip = 4.0f;
    static constexpr float  kOpacity    = 0.32f;
    static constexpr float  kMarginDip  = 22.0f;   // kBlurDip + kOffsetYDip, plus slack

    //  How far a shadow reaches past its card, how far down it drops, how far
    //  inside the card it starts, and how dark it is where it starts. The
    //  defaults are a menu's or a flyout's.
    struct Style
    {
        float  blurDip    = kBlurDip;
        float  offsetYDip = kOffsetYDip;
        float  insetDip   = 0.0f;
        float  opacity    = kOpacity;
    };

    //  The room a surface needs on every side to hold `style`'s shadow.
    static float  GetMarginDip (const Style & style);

    static void  Paint (IDxuiPainter & painter,
                        float          xPx,
                        float          yPx,
                        float          widthPx,
                        float          heightPx,
                        float          radiusPx,
                        float          scale);

    //  The same shadow in `rgb`'s color; its alpha is the shadow's own.
    static void  Paint (IDxuiPainter & painter,
                        float          xPx,
                        float          yPx,
                        float          widthPx,
                        float          heightPx,
                        float          radiusPx,
                        float          scale,
                        uint32_t       rgb);

    //  A shadow of `style`'s reach and darkness, in `rgb`'s color.
    static void  Paint (IDxuiPainter & painter,
                        float          xPx,
                        float          yPx,
                        float          widthPx,
                        float          heightPx,
                        float          radiusPx,
                        float          scale,
                        uint32_t       rgb,
                        const Style  & style);
};
