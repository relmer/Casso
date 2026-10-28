#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiBranchArt
//
//  A horizontal branch for a timeline mark, drawn here because the glyph
//  font lacks one: a line across the bottom of a square, and a second line
//  that curves up from it and runs on to the right edge above it. Drawn into
//  premultiplied BGRA8 pixels at the size it is painted, with each pixel's
//  coverage sampled on a grid, for IDxuiTextRenderer::DrawIconBitmap.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiBranchArt
{
public:
    // sizePx by sizePx pixels of inkArgb, whose alpha is ignored.
    static void  Draw (int sizePx, uint32_t inkArgb, std::vector<uint32_t> & outPixels);

private:
    struct Point
    {
        float  x = 0.0f;
        float  y = 0.0f;
    };

    static constexpr int    kSamples       = 4;      // per pixel, along each axis
    static constexpr int    kCurveSegments = 12;
    static constexpr float  kMainY         = 0.78f;  // the lines, as fractions of the size
    static constexpr float  kBranchY       = 0.24f;
    static constexpr float  kForkX         = 0.16f;
    static constexpr float  kRiseX         = 0.68f;
    static constexpr float  kStrokeRatio   = 0.075f; // half the stroke width
    static constexpr float  kMinHalfStroke = 0.55f;

    static void   BuildPath           (float sizePx, std::vector<Point> & outMain, std::vector<Point> & outBranch);
    static float  GetDistanceToPath   (const std::vector<Point> & path, Point p);
    static float  GetDistanceToSegment (Point a, Point b, Point p);
};
