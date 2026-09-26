#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiColor
//
//  Stateless packed-ARGB color math shared by Dxui widgets: WCAG
//  relative-luminance / contrast-ratio helpers plus accent darkening and
//  lighten / darken / scale tints. Exposed as static methods (no free
//  functions) so any widget can derive theme-accurate, accessible colors
//  from a single IDxuiTheme accent without re-deriving the math locally.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiColor
{
public:
    //
    //  WCAG relative luminance of a packed ARGB color (alpha ignored).
    //
    static float ComputeRelativeLuminance (uint32_t argb)
    {
        float  r = ChannelToLinear (argb >> 16);
        float  g = ChannelToLinear (argb >> 8);
        float  b = ChannelToLinear (argb);

        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }


    //
    //  WCAG contrast ratio between two packed ARGB colors (1.0 .. 21.0).
    //
    static float ComputeContrastRatio (uint32_t a, uint32_t b)
    {
        float  la = ComputeRelativeLuminance (a);
        float  lb = ComputeRelativeLuminance (b);
        float  hi = (la > lb) ? la : lb;
        float  lo = (la > lb) ? lb : la;

        return (hi + 0.05f) / (lo + 0.05f);
    }


    //
    //  Darkens `accent` in fixed steps until it clears `minRatio` against
    //  white, so a bright accent stays legible behind white labels / thumbs.
    //
    static uint32_t ComputeAccentForWhiteContrast (uint32_t accent, float minRatio)
    {
        constexpr uint32_t  s_kWhite    = 0xFFFFFFFFu;
        constexpr int       s_kMaxSteps = 32;
        constexpr float     s_kStepMul  = 0.9f;

        uint32_t  cur = accent;
        int       i   = 0;

        for (i = 0; i < s_kMaxSteps && ComputeContrastRatio (cur, s_kWhite) < minRatio; ++i)
        {
            cur = Scale (cur, s_kStepMul);
        }

        return cur;
    }


    //
    //  Returns a subtle tint of `background` guaranteed to reach at least
    //  `minRatio` WCAG contrast against it — lightening a dark background or
    //  darkening a light one in fixed steps. Used for subtle-but-visible
    //  surfaces (inactive slider track, disabled checkbox fill) that must
    //  stand off the panel regardless of theme.
    //
    static uint32_t ComputeTintForContrast (uint32_t background, float minRatio)
    {
        constexpr int    s_kMaxSteps  = 24;
        constexpr float  s_kLightStep = 0.08f;
        constexpr float  s_kDarkMul   = 0.92f;

        bool      lighten = ComputeRelativeLuminance (background) < 0.5f;
        uint32_t  cur     = background;
        int       i       = 0;

        for (i = 0; i < s_kMaxSteps && ComputeContrastRatio (cur, background) < minRatio; ++i)
        {
            cur = lighten ? Lighten (cur, s_kLightStep) : Scale (cur, s_kDarkMul);
        }

        return (background & 0xFF000000u) | (cur & 0x00FFFFFFu);
    }


    //
    //  Lightens a color toward white by fraction `f` (0 = unchanged, 1 = white).
    //
    static uint32_t Lighten (uint32_t argb, float f)
    {
        uint32_t  r = (uint32_t) (((argb >> 16) & 0xFFu) + (255 - ((argb >> 16) & 0xFFu)) * f);
        uint32_t  g = (uint32_t) (((argb >>  8) & 0xFFu) + (255 - ((argb >>  8) & 0xFFu)) * f);
        uint32_t  b = (uint32_t) (( argb        & 0xFFu) + (255 - ( argb        & 0xFFu)) * f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }


    //
    //  Darkens a color toward black by multiplying each channel by `f`
    //  (0 = black, 1 = unchanged).
    //
    static uint32_t Darken (uint32_t argb, float f)
    {
        return Scale (argb, f);
    }


    //
    //  Multiplies each RGB channel by `f` (clamped to 255) preserving alpha;
    //  `f` may exceed 1.0 to brighten a color toward white.
    //
    static uint32_t Scale (uint32_t argb, float f)
    {
        uint32_t  r = ScaleChannel ((argb >> 16) & 0xFFu, f);
        uint32_t  g = ScaleChannel ((argb >>  8) & 0xFFu, f);
        uint32_t  b = ScaleChannel ( argb        & 0xFFu, f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }


    //
    //  Multiplies the alpha channel by `f` (clamped to 255) preserving RGB,
    //  e.g. to fade a color in or out without disturbing its hue.
    //
    static uint32_t ScaleAlpha (uint32_t argb, float f)
    {
        uint32_t  a = ScaleChannel ((argb >> 24) & 0xFFu, f);

        return (a << 24) | (argb & 0x00FFFFFFu);
    }


    //
    //  The color fraction `t` of the way from `from` to `to` (0 = from,
    //  1 = to), channel by channel, with `to`'s alpha.
    //
    static uint32_t Mix (uint32_t from, uint32_t to, float t)
    {
        uint32_t  r = (uint32_t) ((float) ((from >> 16) & 0xFFu) + ((float) ((to >> 16) & 0xFFu) - (float) ((from >> 16) & 0xFFu)) * t);
        uint32_t  g = (uint32_t) ((float) ((from >>  8) & 0xFFu) + ((float) ((to >>  8) & 0xFFu) - (float) ((from >>  8) & 0xFFu)) * t);
        uint32_t  b = (uint32_t) ((float) ( from        & 0xFFu) + ((float) ( to        & 0xFFu) - (float) ( from        & 0xFFu)) * t);

        return (to & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }


    //
    //  The accent that marks what has focus. A blue accent on a blue-tinted
    //  background reads as more of the same, not as a highlight, so where the
    //  background carries a hue near the accent's this is the accent's
    //  complement -- orange for blue -- made vivid, since a pale accent's
    //  complement is a pastel that a one-pixel line loses. On a gray
    //  background, or one tinted another way, it is the accent itself.
    //
    static uint32_t ComputeFocusAccent (uint32_t accent, uint32_t background)
    {
        constexpr float  s_kMinTintSat   = 0.15f;    // below this a background is gray
        constexpr float  s_kSameHueDeg   = 60.0f;    // hues this close read as one color
        constexpr float  s_kHalfTurnDeg  = 180.0f;
        constexpr float  s_kFullTurnDeg  = 360.0f;
        constexpr float  s_kVividSat     = 0.8f;     // the least saturation of a complement
        constexpr float  s_kVividValue   = 0.95f;    // and the least brightness

        float  ah    = 0.0f;
        float  as    = 0.0f;
        float  av    = 0.0f;
        float  bh    = 0.0f;
        float  bs    = 0.0f;
        float  bv    = 0.0f;
        float  apart = 0.0f;

        ToHsv (accent,     ah, as, av);
        ToHsv (background, bh, bs, bv);

        apart = std::fabs (ah - bh);
        apart = (apart > s_kHalfTurnDeg) ? s_kFullTurnDeg - apart : apart;

        if (bs < s_kMinTintSat || apart > s_kSameHueDeg)
        {
            return accent;
        }

        return FromHsv (std::fmod (ah + s_kHalfTurnDeg, s_kFullTurnDeg), (std::max) (as, s_kVividSat), (std::max) (av, s_kVividValue), accent & 0xFF000000u);
    }


private:
    //  Hue in degrees, saturation and value 0..1.
    static void ToHsv (uint32_t argb, float & h, float & s, float & v)
    {
        constexpr float  s_kSectorDeg = 60.0f;

        float  r    = (float) ((argb >> 16) & 0xFFu) / 255.0f;
        float  g    = (float) ((argb >>  8) & 0xFFu) / 255.0f;
        float  b    = (float) ( argb        & 0xFFu) / 255.0f;
        float  hi   = (std::max) ({ r, g, b });
        float  lo   = (std::min) ({ r, g, b });
        float  span = hi - lo;

        v = hi;
        s = (hi > 0.0f) ? span / hi : 0.0f;
        h = 0.0f;

        if (span <= 0.0f)
        {
            return;
        }

        if (hi == r)      { h = s_kSectorDeg * std::fmod ((g - b) / span, 6.0f); }
        else if (hi == g) { h = s_kSectorDeg * ((b - r) / span + 2.0f); }
        else              { h = s_kSectorDeg * ((r - g) / span + 4.0f); }

        h = (h < 0.0f) ? h + 360.0f : h;
    }


    static uint32_t FromHsv (float h, float s, float v, uint32_t alpha)
    {
        constexpr float  s_kSectorDeg = 60.0f;

        float  c      = v * s;
        float  x      = c * (1.0f - std::fabs (std::fmod (h / s_kSectorDeg, 2.0f) - 1.0f));
        float  m      = v - c;
        float  r      = 0.0f;
        float  g      = 0.0f;
        float  b      = 0.0f;
        int    sector = (int) (h / s_kSectorDeg) % 6;

        switch (sector)
        {
        case 0:  r = c; g = x; break;
        case 1:  r = x; g = c; break;
        case 2:  g = c; b = x; break;
        case 3:  g = x; b = c; break;
        case 4:  r = x; b = c; break;
        default: r = c; b = x; break;
        }

        return alpha | ((uint32_t) std::lround ((r + m) * 255.0f) << 16) | ((uint32_t) std::lround ((g + m) * 255.0f) << 8) | (uint32_t) std::lround ((b + m) * 255.0f);
    }



    static float ChannelToLinear (uint32_t c8)
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
