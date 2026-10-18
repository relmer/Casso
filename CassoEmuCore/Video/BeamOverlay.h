#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlay
//
//  Marks where the video beam is on a rendered frame, for the debugger. The
//  beam's scanline is a line across the picture at three quarters strength,
//  edged with a darkened row on each side so it is visible over a busy
//  picture, and the cycle it is on is a solid bar across that line, three
//  scanlines above and below it, in contrast with the line. A beam in
//  horizontal blank has its block at the picture's left edge, where the line
//  starts drawing; a beam in vertical blank is a line along the bottom edge.
//
//  The frame is the 192 visible scanlines scaled to the buffer, each
//  scanline drawing 40 cycles across its width after 25 cycles of blanking.
//
////////////////////////////////////////////////////////////////////////////////

class BeamOverlay
{
public:
    static constexpr uint32_t  kVisibleScanlines = 192;
    static constexpr uint32_t  kVisibleCycles    = 40;
    static constexpr uint32_t  kBlankCycles      = 25;

    static void      Draw  (uint32_t * pixels, int width, int height, uint32_t scanline, uint32_t cycle, uint32_t color);

    //  A quarter of the pixel and three quarters of the color, opaque; and the
    //  pixel at a quarter of its brightness, opaque.
    static uint32_t  Blend  (uint32_t pixel, uint32_t color);
    static uint32_t  Darken (uint32_t pixel);

private:
    static void      DrawLine (uint32_t * pixels, int width, int height, int top, int bottom, uint32_t color);
    static void      ApplyRow (uint32_t * pixels, int width, int row, const std::function<uint32_t (uint32_t)> & change);
    static void      FillBox  (uint32_t * pixels, int width, int left, int top, int right, int bottom, uint32_t color);
};
