#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph
//
//  The images a code view's glyph margin draws: a breakpoint's dot, filled or
//  a ring, and the PC's arrow beside it. The dot and the arrow are each
//  kSizePx square; the PC's line has an image twice as wide, the dot's square
//  on the left, empty on a line with no breakpoint, and the arrow's on the
//  right, so the arrow is never drawn over the dot. The dot fills 0.7 of its
//  square across, and the arrow is as tall as the dot.
//
////////////////////////////////////////////////////////////////////////////////

class GutterGlyph
{
public:
    static constexpr int    kSizePx      = 48;
    static constexpr float  kDotRadiusPx = 16.8f;
    static constexpr float  kRingPx      = 4.0f;

    static std::shared_ptr<DxuiIconImage>  MakeDot   (uint32_t argb, bool filled);
    static std::shared_ptr<DxuiIconImage>  MakeArrow (uint32_t argb);
    static std::shared_ptr<DxuiIconImage>  MakeOver  (const DxuiIconImage & under, const DxuiIconImage & over);

    //  The PC's line's image: the line's dot, or nothing, and the arrow to
    //  its right.
    static std::shared_ptr<const DxuiIconImage>  MakeMarked (const std::shared_ptr<const DxuiIconImage> & dot,
                                                             const std::shared_ptr<const DxuiIconImage> & arrow);

    //  Whether a point of the image, in its pixels, is inside the arrow.
    static bool  IsInArrow (float x, float y);

private:
    static constexpr int  kSamples = 4;    // to a side, in each pixel of the arrow

    static uint32_t  MakePixel        (uint32_t argb, float coverage);
    static float     GetArrowCoverage (int x, int y);
    static uint32_t  BlendOver        (uint32_t under, uint32_t over);
};
