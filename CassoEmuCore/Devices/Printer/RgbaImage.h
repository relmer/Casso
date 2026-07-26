#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  RgbaImage
//
//  Plain 8-bit RGBA raster, bytes interleaved R,G,B,A row-major. The printer
//  paper renderer produces one of these; the shell hands it to WIC for PNG
//  encoding or to the clipboard. Kept free of any system dependency so the
//  renderer stays unit-testable.
//
////////////////////////////////////////////////////////////////////////////////

struct RgbaImage
{
    int             width  = 0;
    int             height = 0;
    vector<Byte>    rgba;                 // width * height * 4, order R,G,B,A

    void Allocate (int w, int h, Byte r, Byte g, Byte b)
    {
        size_t   count = (size_t) w * h;
        size_t   i     = 0;

        width  = w;
        height = h;
        rgba.resize (count * 4);

        for (i = 0; i < count; i++)
        {
            rgba[i * 4 + 0] = r;
            rgba[i * 4 + 1] = g;
            rgba[i * 4 + 2] = b;
            rgba[i * 4 + 3] = 0xFF;
        }
    }

    Byte * PixelAt (int x, int y)
    {
        return &rgba[((size_t) y * width + x) * 4];
    }

    const Byte * PixelAt (int x, int y) const
    {
        return &rgba[((size_t) y * width + x) * 4];
    }
};
