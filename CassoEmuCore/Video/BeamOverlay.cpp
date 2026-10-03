#include "Pch.h"

#include "Video/BeamOverlay.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::Draw
//
////////////////////////////////////////////////////////////////////////////////

void BeamOverlay::Draw (uint32_t * pixels, int width, int height, uint32_t scanline, uint32_t cycle, uint32_t color)
{
    static constexpr int  kEdgeMarkWidth = 2;
    static constexpr int  kMarkReach     = 3;   // scanlines the bar reaches above and below the line
    int                   rowsPerLine    = height / (int) kVisibleScanlines;
    int                   dotsPerCycle   = width  / (int) kVisibleCycles;
    int                   top            = (int) scanline * rowsPerLine;
    int                   left           = 0;
    int                   right          = kEdgeMarkWidth;



    if (pixels == nullptr || rowsPerLine <= 0 || dotsPerCycle <= 0)
    {
        return;
    }

    if (scanline >= kVisibleScanlines)
    {
        BlendRow (pixels, width, height - 1, color);
        BlendRow (pixels, width, height - 2, color);
        return;
    }

    for (int row = top; row < top + rowsPerLine; row++)
    {
        BlendRow (pixels, width, row, color);
    }

    if (cycle >= kBlankCycles)
    {
        left  = (int) (cycle - kBlankCycles) * dotsPerCycle;
        right = left + dotsPerCycle;
    }

    FillBox (pixels, width, left, std::max (top - kMarkReach * rowsPerLine, 0), right,
             std::min (top + (kMarkReach + 1) * rowsPerLine, height), color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::Blend
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BeamOverlay::Blend (uint32_t pixel, uint32_t color)
{
    static constexpr uint32_t  kHalfMask = 0x007F7F7F;
    static constexpr uint32_t  kOpaque   = 0xFF000000;



    return (((pixel >> 1) & kHalfMask) + ((color >> 1) & kHalfMask)) | kOpaque;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::BlendRow
//
////////////////////////////////////////////////////////////////////////////////

void BeamOverlay::BlendRow (uint32_t * pixels, int width, int row, uint32_t color)
{
    uint32_t  * line = pixels + (size_t) row * (size_t) width;



    for (int x = 0; x < width; x++)
    {
        line[x] = Blend (line[x], color);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::FillBox
//
////////////////////////////////////////////////////////////////////////////////

void BeamOverlay::FillBox (uint32_t * pixels, int width, int left, int top, int right, int bottom, uint32_t color)
{
    for (int y = top; y < bottom; y++)
    {
        for (int x = left; x < right && x < width; x++)
        {
            pixels[(size_t) y * (size_t) width + (size_t) x] = color;
        }
    }
}
