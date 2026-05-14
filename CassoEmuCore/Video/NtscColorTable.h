#pragma once

#include "Pch.h"

////////////////////////////////////////////////////////////////////////////////
//
//  NTSC Color Constants
//
//  Stored in B8G8R8A8 byte layout to match the swap-chain format
//  (DXGI_FORMAT_B8G8R8A8_UNORM in D3DRenderer.cpp). Byte 0 = B, byte 1
//  = G, byte 2 = R, byte 3 = A. The little-endian uint32_t hex literal
//  therefore reads as 0xAARRGGBB — the "ARGB-looking" form most
//  Windows pixel tooling documents.
//
//  See Video/PixelFormat.h for the project-wide byte-order convention.
//
//  History: pre-2026-05-13 these were written as 0xAARRGGBB but the
//  surface format was R8G8B8A8_UNORM, so R and B were silently
//  swapped on screen — most visible as Blue<->Orange in HGR output.
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

static constexpr uint32_t kNtscBlack   = 0xFF000000;
static constexpr uint32_t kNtscWhite   = 0xFFFFFFFF;
static constexpr uint32_t kNtscViolet  = 0xFFFF44FD;  // RGB(255, 68,253)  even col, palette 0
static constexpr uint32_t kNtscGreen   = 0xFF14F53C;  // RGB( 20,245, 60)  odd  col, palette 0
static constexpr uint32_t kNtscBlue    = 0xFF14CFFF;  // RGB( 20,207,255)  even col, palette 1
static constexpr uint32_t kNtscOrange  = 0xFFFF6A3C;  // RGB(255,106, 60)  odd  col, palette 1
