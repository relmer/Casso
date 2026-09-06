#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple II Color Palette -- the single source of truth
//
//  Stored in B8G8R8A8 byte layout to match the swap-chain format
//  (DXGI_FORMAT_B8G8R8A8_UNORM in D3DRenderer.cpp). Byte 0 = B, byte 1
//  = G, byte 2 = R, byte 3 = A. The little-endian uint32_t hex literal
//  therefore reads as 0xAARRGGBB -- the "ARGB-looking" form most
//  Windows pixel tooling documents.
//
//  See Video/PixelFormat.h for the project-wide byte-order convention.
//
//  ---------------------------------------------------------------------------
//  Why ONE table
//
//  The machine has sixteen colors, and every mode draws from that one set.
//  Lo-res names them directly, DHGR indexes them by color number, and HGR
//  reaches four of them by NTSC artifact -- but HGR violet IS color 3, HGR
//  green IS color 12, HGR blue IS color 6 and HGR orange IS color 9. Each
//  pair is the same subcarrier phase at the same amplitude; the hardware has
//  no way to make them differ, so neither may this table.
//
//  They used to differ. HGR carried four NTSC-derived constants here while
//  lo-res and DHGR carried sixteen nominal ones, duplicated verbatim in
//  AppleLoResMode.cpp and AppleDoubleHiResMode.cpp. The two families
//  disagreed on all four shared colors, worst at color 3, where HGR drew a
//  bright violet and lo-res drew a crimson 41 degrees of hue away under the
//  name "Purple". Draw HGR violet beside lo-res 3 and they did not match.
//
//  So the sixteen live here, HGR's names are aliases into them, and the two
//  duplicate tables are gone. Adding a mode means indexing this array, not
//  copying it.
//
//  The four shared entries took HGR's values rather than lo-res's, because
//  those were the NTSC-derived ones and because "Purple" rendering as crimson
//  was the actual error. That choice moves lo-res and DHGR colors 3, 6, 9 and
//  12 on screen; the golden-hash tests in VideoRenderTests.cpp moved with it.
//
//  History: pre-2026-05-13 these were written as 0xAARRGGBB but the
//  surface format was R8G8B8A8_UNORM, so R and B were silently
//  swapped on screen -- most visible as Blue<->Orange in HGR output.
//  Violet and Green happened to look correct by accident because
//  their R/B components are roughly symmetric. That bug was fixed
//  by re-encoding the constants in RGBA byte order. On 2026-05-14
//  we instead switched the surface format to BGRA (matching every
//  Windows pixel surface) and put the literals back into the
//  human-natural 0xAARRGGBB form. The on-screen colors are identical;
//  the difference is that image-export paths (clipboard, BMP, WIC)
//  no longer need to swizzle.
//
////////////////////////////////////////////////////////////////////////////////

// Indexed by Apple II color number: the lo-res COLOR= value, and the DHGR
// color a cell's four dots decode to.
inline constexpr uint32_t kAppleColors[16] =
{
    0xFF000000,   //  0: Black        RGB(  0,  0,  0)
    0xFFDD2266,   //  1: Magenta      RGB(221, 34,102)
    0xFF000099,   //  2: Dark Blue    RGB(  0,  0,153)
    0xFFFF44FD,   //  3: Purple       RGB(255, 68,253)  = HGR violet
    0xFF002200,   //  4: Dark Green   RGB(  0, 34,  0)
    0xFF555555,   //  5: Gray 1       RGB( 85, 85, 85)
    0xFF14CFFF,   //  6: Medium Blue  RGB( 20,207,255)  = HGR blue
    0xFF66AAFF,   //  7: Light Blue   RGB(102,170,255)
    0xFF885500,   //  8: Brown        RGB(136, 85,  0)
    0xFFFF6A3C,   //  9: Orange       RGB(255,106, 60)  = HGR orange
    0xFFAAAAAA,   // 10: Gray 2       RGB(170,170,170)
    0xFFFF8888,   // 11: Pink         RGB(255,136,136)
    0xFF14F53C,   // 12: Light Green  RGB( 20,245, 60)  = HGR green
    0xFFFFFF00,   // 13: Yellow       RGB(255,255,  0)
    0xFF44FFDD,   // 14: Aquamarine   RGB( 68,255,221)
    0xFFFFFFFF,   // 15: White        RGB(255,255,255)
};


// HGR's artifact colors, by the names the hi-res decode uses. These are not a
// separate palette -- they are four of the sixteen above, plus black and
// white. The column/palette notes say which dot phase reaches each one.
inline constexpr uint32_t kNtscBlack   = kAppleColors[0];
inline constexpr uint32_t kNtscWhite   = kAppleColors[15];
inline constexpr uint32_t kNtscViolet  = kAppleColors[3];   // even col, palette 0
inline constexpr uint32_t kNtscGreen   = kAppleColors[12];  // odd  col, palette 0
inline constexpr uint32_t kNtscBlue    = kAppleColors[6];   // even col, palette 1
inline constexpr uint32_t kNtscOrange  = kAppleColors[9];   // odd  col, palette 1
