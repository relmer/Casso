#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph
//
//  The images a code view's glyph margin draws, as Visual Studio's glyph
//  margin does: a breakpoint's dot, filled or a ring, the PC's arrow, and the
//  arrow over a dot on a line that has both. Each is kSizePx square, drawn in
//  the margin's icon box. The dot fills 0.7 of the image across, and the
//  arrow starts at the dot's left edge and is as tall as the dot, so both
//  sit the same distance inside the view.
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

    //  The PC's arrow over a line's dot, or the arrow alone for a line with
    //  no dot.
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
