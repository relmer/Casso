#pragma once

#include "Pch.h"

////////////////////////////////////////////////////////////////////////////////
//
//  PixelFormat
//
//  Single source of truth for the byte order of pixels in the framebuffer.
//  Everything in Casso that produces or consumes 32-bit pixels must agree
//  on this layout:
//
//      byte 0 = B
//      byte 1 = G
//      byte 2 = R
//      byte 3 = A
//
//  i.e. DXGI_FORMAT_B8G8R8A8_UNORM. This matches every Windows pixel
//  surface (GDI bitmaps, CF_DIB clipboard data, BMP files, WIC default)
//  so image-export paths can memcpy the framebuffer directly with no
//  swizzle.
//
//  In a little-endian uint32_t hex literal, the byte at index 0 is the
//  least-significant nibble pair. Therefore a hex literal of the form
//  0xAARRGGBB reads correctly: A in the high byte, B in byte 0.
//
//  Mnemonic: B8G8R8A8 has B in byte 0, R in byte 2; the hex literal is
//  "ARGB-looking" (0xAARRGGBB), which is what most Windows tooling
//  documents anyway.
//
//  History: pre-2026-05-14 the engine used DXGI_FORMAT_R8G8B8A8_UNORM,
//  which required every Windows-bound pixel consumer (clipboard
//  screenshot, etc.) to swizzle R<->B on the way out. After two
//  separate "the red and the blue are swapped" bugs in palette tables
//  written by humans who don't think in little-endian RGBA byte order,
//  we flipped to BGRA to match Windows conventions everywhere.
//
////////////////////////////////////////////////////////////////////////////////

namespace Casso::Video
{
    static constexpr int kPixelByteIndexB = 0;
    static constexpr int kPixelByteIndexG = 1;
    static constexpr int kPixelByteIndexR = 2;
    static constexpr int kPixelByteIndexA = 3;

    inline constexpr uint8_t ExtractB (uint32_t pixel) { return static_cast<uint8_t> (pixel        & 0xFF); }
    inline constexpr uint8_t ExtractG (uint32_t pixel) { return static_cast<uint8_t> ((pixel >>  8) & 0xFF); }
    inline constexpr uint8_t ExtractR (uint32_t pixel) { return static_cast<uint8_t> ((pixel >> 16) & 0xFF); }
    inline constexpr uint8_t ExtractA (uint32_t pixel) { return static_cast<uint8_t> ((pixel >> 24) & 0xFF); }

    inline constexpr uint32_t MakePixel (uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF)
    {
        return  (static_cast<uint32_t> (a) << 24)
              | (static_cast<uint32_t> (r) << 16)
              | (static_cast<uint32_t> (g) <<  8)
              |  static_cast<uint32_t> (b);
    }
}
