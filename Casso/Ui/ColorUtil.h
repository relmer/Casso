#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ColorMonitorTextMode
//
//  User-selected text color when the Color monitor is active. White is the
//  default; Green / Amber mimic the monochrome phosphors without tinting
//  the rest of the (color) image; Custom uses a user-picked RGB value.
//
////////////////////////////////////////////////////////////////////////////////

enum class ColorMonitorTextMode
{
    White  = 0,
    Green  = 1,
    Amber  = 2,
    Custom = 3,
};




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil
//
//  Pure color helpers shared by the settings color picker, the prefs
//  layer, and the framebuffer text-color wiring. Colors are packed
//  0xAARRGGBB (matching the framebuffer's BGRA8 pixel as a little-endian
//  uint32); all outputs force an opaque alpha. No Win32, no I/O -- unit
//  tested in ColorUtilTests.
//
////////////////////////////////////////////////////////////////////////////////

class ColorUtil
{
public:
    // Canonical resolved colors for the non-custom modes.
    static constexpr uint32_t  kWhiteArgb = 0xFFFFFFFF;
    static constexpr uint32_t  kGreenArgb = 0xFF00FF00;
    static constexpr uint32_t  kAmberArgb = 0xFFFFB000;

    static uint32_t      HsvToArgb     (float h, float s, float v);
    static void          ArgbToHsv     (uint32_t argb, float & outH, float & outS, float & outV);
    static bool          ParseHexColor (const std::wstring & text, uint32_t & outArgb);
    static std::wstring  FormatHexColor (uint32_t argb);
    static uint32_t      ResolveColorMonitorTextArgb (ColorMonitorTextMode mode, uint32_t customArgb);

private:
    // Value 0..15 of a single hex digit, or -1 when not a hex character.
    static int           HexValue      (wchar_t ch);
};
