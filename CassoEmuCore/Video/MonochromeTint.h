#pragma once

#include "Pch.h"

#include "Video/PixelFormat.h"

////////////////////////////////////////////////////////////////////////////////
//
//  MonochromeTint
//
//  Pure-function color-mode tinting for the framebuffer. Lives in
//  CassoEmuCore so the conversion can be unit-tested without
//  Win32; the shell calls these per-pixel inside its display
//  pipeline (see EmulatorShell::ComposeFrame).
//
//  All functions assume input pixels are in the framebuffer's
//  documented byte order (B8G8R8A8 — see Video/PixelFormat.h)
//  and emit pixels in the same byte order.
//
//  The luminance coefficients (0.299/0.587/0.114) are the standard
//  Rec.601 weights used everywhere in NTSC-era display work.
//
////////////////////////////////////////////////////////////////////////////////

namespace Casso::Video
{
    inline uint8_t Luminance (uint32_t pixel)
    {
        return static_cast<uint8_t> (
            0.299f * ExtractR (pixel) +
            0.587f * ExtractG (pixel) +
            0.114f * ExtractB (pixel));
    }

    inline uint32_t TintGreenMono (uint32_t pixel)
    {
        uint8_t  l = Luminance (pixel);
        return MakePixel (0, l, 0);
    }

    inline uint32_t TintAmberMono (uint32_t pixel)
    {
        uint8_t  l = Luminance (pixel);
        return MakePixel (l, static_cast<uint8_t> (l * 0.75f), 0);
    }

    inline uint32_t TintWhiteMono (uint32_t pixel)
    {
        uint8_t  l = Luminance (pixel);
        return MakePixel (l, l, l);
    }
}
