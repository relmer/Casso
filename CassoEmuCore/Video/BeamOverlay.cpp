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
        DrawLine (pixels, width, height, height - 2, height, color);
        return;
    }

    DrawLine (pixels, width, height, top, top + rowsPerLine, color);

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
    static constexpr uint32_t  kQuarterMask = 0x003F3F3F;
    static constexpr uint32_t  kOpaque      = 0xFF000000;



    return (((pixel >> 2) & kQuarterMask) + 3 * ((color >> 2) & kQuarterMask)) | kOpaque;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::Darken
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BeamOverlay::Darken (uint32_t pixel)
{
    static constexpr uint32_t  kQuarterMask = 0x003F3F3F;
    static constexpr uint32_t  kOpaque      = 0xFF000000;



    return ((pixel >> 2) & kQuarterMask) | kOpaque;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::DrawLine
//
//  The rows from top up to bottom in the color, and the row on each side of
//  them darkened, so the line is visible over any picture under it.
//
////////////////////////////////////////////////////////////////////////////////

void BeamOverlay::DrawLine (uint32_t * pixels, int width, int height, int top, int bottom, uint32_t color)
{
    for (int row = top; row < bottom; row++)
    {
        ApplyRow (pixels, width, row, [color] (uint32_t pixel) { return Blend (pixel, color); });
    }

    for (int row : { top - 1, bottom })
    {
        if (row >= 0 && row < height)
        {
            ApplyRow (pixels, width, row, [] (uint32_t pixel) { return Darken (pixel); });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay::ApplyRow
//
////////////////////////////////////////////////////////////////////////////////

void BeamOverlay::ApplyRow (uint32_t * pixels, int width, int row, const std::function<uint32_t (uint32_t)> & change)
{
    uint32_t  * line = pixels + (size_t) row * (size_t) width;



    for (int x = 0; x < width; x++)
    {
        line[x] = change (line[x]);
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
