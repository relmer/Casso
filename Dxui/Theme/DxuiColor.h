#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiColor
//
//  Stateless packed-ARGB colour math shared by Dxui widgets: WCAG
//  relative-luminance / contrast-ratio helpers plus accent darkening and
//  lighten / darken / scale tints. Exposed as static methods (no free
//  functions) so any widget can derive theme-accurate, accessible colours
//  from a single IDxuiTheme accent without re-deriving the math locally.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiColor
{
public:
    //
    //  WCAG relative luminance of a packed ARGB colour (alpha ignored).
    //
    static float RelativeLuminance (uint32_t argb)
    {
        float  r = ChannelLinear (argb >> 16);
        float  g = ChannelLinear (argb >> 8);
        float  b = ChannelLinear (argb);

        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }


    //
    //  WCAG contrast ratio between two packed ARGB colours (1.0 .. 21.0).
    //
    static float ContrastRatio (uint32_t a, uint32_t b)
    {
        float  la = RelativeLuminance (a);
        float  lb = RelativeLuminance (b);
        float  hi = (la > lb) ? la : lb;
        float  lo = (la > lb) ? lb : la;

        return (hi + 0.05f) / (lo + 0.05f);
    }


    //
    //  Darkens `accent` in fixed steps until it clears `minRatio` against
    //  white, so a bright accent stays legible behind white labels / thumbs.
    //
    static uint32_t AccentForWhiteContrast (uint32_t accent, float minRatio)
    {
        constexpr uint32_t  s_kWhite    = 0xFFFFFFFFu;
        constexpr int       s_kMaxSteps = 32;
        constexpr float     s_kStepMul  = 0.9f;

        uint32_t  cur = accent;
        int       i   = 0;

        for (i = 0; i < s_kMaxSteps && ContrastRatio (cur, s_kWhite) < minRatio; ++i)
        {
            cur = Scale (cur, s_kStepMul);
        }

        return cur;
    }


    //
    //  Lightens a colour toward white by fraction `f` (0 = unchanged, 1 = white).
    //
    static uint32_t Lighten (uint32_t argb, float f)
    {
        uint32_t  r = (uint32_t) (((argb >> 16) & 0xFFu) + (255 - ((argb >> 16) & 0xFFu)) * f);
        uint32_t  g = (uint32_t) (((argb >>  8) & 0xFFu) + (255 - ((argb >>  8) & 0xFFu)) * f);
        uint32_t  b = (uint32_t) (( argb        & 0xFFu) + (255 - ( argb        & 0xFFu)) * f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }


    //
    //  Darkens a colour toward black by multiplying each channel by `f`
    //  (0 = black, 1 = unchanged).
    //
    static uint32_t Darken (uint32_t argb, float f)
    {
        return Scale (argb, f);
    }


    //
    //  Multiplies each RGB channel by `f` (clamped to 255) preserving alpha;
    //  `f` may exceed 1.0 to brighten a colour toward white.
    //
    static uint32_t Scale (uint32_t argb, float f)
    {
        uint32_t  r = ScaleChannel ((argb >> 16) & 0xFFu, f);
        uint32_t  g = ScaleChannel ((argb >>  8) & 0xFFu, f);
        uint32_t  b = ScaleChannel ( argb        & 0xFFu, f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }


private:
    static float ChannelLinear (uint32_t c8)
    {
        float  s = (float) (c8 & 0xFFu) / 255.0f;

        return (s <= 0.03928f) ? (s / 12.92f)
                               : std::pow ((s + 0.055f) / 1.055f, 2.4f);
    }


    static uint32_t ScaleChannel (uint32_t c8, float f)
    {
        float  v = (float) (c8 & 0xFFu) * f;

        return (v > 255.0f) ? 255u : (uint32_t) v;
    }
};
