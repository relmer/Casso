#pragma once

#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors
//
//  The colors the debugger draws text in, for a theme's background: Visual
//  Studio's code colors for the background's darkness, the theme's own text
//  and result colors, and a changed value's red. Every one is moved toward
//  black on a light background, or toward white on a dark one, until it
//  reaches the WCAG AA ratio for text, so no theme leaves text too faint to
//  read.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerTextColors
{
public:
    static constexpr float  s_kMinTextContrast     = 4.5f;

    //  On a light page: how far the darkest row tint is toward black, and how
    //  far plain text is moved toward the page so it is not stark black.
    static constexpr float  s_kLightRowShade       = 0.15f;
    static constexpr float  s_kLightTextSoftening  = 0.25f;

    struct Set
    {
        SourceSyntax::Colors  syntax;
        uint32_t              annotation = 0;
        uint32_t              changed    = 0;
        uint32_t              result     = 0;
        uint32_t              muted      = 0;
    };

    //  `resultText` of zero means the theme gives no result color.
    static Set       Make        (uint32_t background,
                                  uint32_t foreground,
                                  uint32_t muted,
                                  uint32_t resultText);

    static bool      IsDark      (uint32_t background);

    //  The darkest a row is tinted on a light page, which every
    //  color must read against.
    static uint32_t  GetRowShade (uint32_t background);

    //  `argb`, or the nearest color along the way to black or white that
    //  reaches s_kMinTextContrast against `background`.
    static uint32_t  GetReadable (uint32_t argb, uint32_t background);
};
