#include "Pch.h"

#include "ColorUtil.h"




static constexpr float  s_kHueMax        = 360.0f;
static constexpr float  s_kHueSextant    = 60.0f;
static constexpr float  s_kChannelMax    = 255.0f;
static constexpr int    s_kHexDigitCount = 6;




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil::HexValue
//
//  Returns the 0..15 value of a single hex digit, or -1 if not hex.
//
////////////////////////////////////////////////////////////////////////////////

int ColorUtil::HexValue (wchar_t ch)
{
    int  result = -1;

    if (ch >= L'0' && ch <= L'9')
    {
        result = ch - L'0';
    }
    else if (ch >= L'a' && ch <= L'f')
    {
        result = 10 + (ch - L'a');
    }
    else if (ch >= L'A' && ch <= L'F')
    {
        result = 10 + (ch - L'A');
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil::HsvToArgb
//
//  Converts HSV (h in [0,360), s / v in [0,1]) to an opaque 0xAARRGGBB
//  color. Inputs are clamped to their valid ranges first.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ColorUtil::HsvToArgb (float h, float s, float v)
{
    float     hh = h;
    float     c  = 0.0f;
    float     x  = 0.0f;
    float     m  = 0.0f;
    float     r1 = 0.0f;
    float     g1 = 0.0f;
    float     b1 = 0.0f;
    int       seg = 0;
    uint32_t  r  = 0;
    uint32_t  g  = 0;
    uint32_t  b  = 0;



    s = std::clamp (s, 0.0f, 1.0f);
    v = std::clamp (v, 0.0f, 1.0f);

    hh = std::fmod (h, s_kHueMax);
    if (hh < 0.0f)
    {
        hh += s_kHueMax;
    }

    c   = v * s;
    seg = (int) (hh / s_kHueSextant);
    x   = c * (1.0f - std::fabs (std::fmod (hh / s_kHueSextant, 2.0f) - 1.0f));
    m   = v - c;

    switch (seg)
    {
        case 0:  r1 = c; g1 = x; b1 = 0.0f; break;
        case 1:  r1 = x; g1 = c; b1 = 0.0f; break;
        case 2:  r1 = 0.0f; g1 = c; b1 = x; break;
        case 3:  r1 = 0.0f; g1 = x; b1 = c; break;
        case 4:  r1 = x; g1 = 0.0f; b1 = c; break;
        default: r1 = c; g1 = 0.0f; b1 = x; break;
    }

    r = (uint32_t) ((r1 + m) * s_kChannelMax + 0.5f);
    g = (uint32_t) ((g1 + m) * s_kChannelMax + 0.5f);
    b = (uint32_t) ((b1 + m) * s_kChannelMax + 0.5f);

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil::ArgbToHsv
//
//  Converts an 0xAARRGGBB color to HSV (h in [0,360), s / v in [0,1]).
//
////////////////////////////////////////////////////////////////////////////////

void ColorUtil::ArgbToHsv (uint32_t argb, float & outH, float & outS, float & outV)
{
    float  r     = (float) ((argb >> 16) & 0xFF) / s_kChannelMax;
    float  g     = (float) ((argb >> 8)  & 0xFF) / s_kChannelMax;
    float  b     = (float) (argb         & 0xFF) / s_kChannelMax;
    float  maxC  = std::max (r, std::max (g, b));
    float  minC  = std::min (r, std::min (g, b));
    float  delta = maxC - minC;
    float  h     = 0.0f;



    if (delta > 0.0f)
    {
        if (maxC == r)
        {
            h = s_kHueSextant * std::fmod ((g - b) / delta, 6.0f);
        }
        else if (maxC == g)
        {
            h = s_kHueSextant * (((b - r) / delta) + 2.0f);
        }
        else
        {
            h = s_kHueSextant * (((r - g) / delta) + 4.0f);
        }
    }

    if (h < 0.0f)
    {
        h += s_kHueMax;
    }

    outH = h;
    outS = (maxC <= 0.0f) ? 0.0f : (delta / maxC);
    outV = maxC;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil::ParseHexColor
//
//  Parses "#RRGGBB" or "RRGGBB" (case-insensitive, surrounding spaces
//  tolerated) into an opaque 0xAARRGGBB color. Returns false for any
//  malformed input, leaving outArgb untouched.
//
////////////////////////////////////////////////////////////////////////////////

bool ColorUtil::ParseHexColor (const std::wstring & text, uint32_t & outArgb)
{
    HRESULT       hr    = S_OK;
    bool          ok    = false;
    size_t        first = text.find_first_not_of (L" \t");
    size_t        last  = text.find_last_not_of (L" \t");
    std::wstring  body;
    uint32_t      rgb   = 0;
    int           i     = 0;



    CBR (first != std::wstring::npos);

    body = text.substr (first, last - first + 1);

    if (!body.empty() && body[0] == L'#')
    {
        body.erase (0, 1);
    }

    CBR (body.size() == (size_t) s_kHexDigitCount);

    for (i = 0; i < s_kHexDigitCount; i++)
    {
        int  nibble = ColorUtil::HexValue (body[(size_t) i]);

        CBR (nibble >= 0);
        rgb = (rgb << 4) | (uint32_t) nibble;
    }

    outArgb = 0xFF000000u | rgb;
    ok      = true;

Error:
    return ok;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil::FormatHexColor
//
//  Formats the RGB channels of an 0xAARRGGBB color as "#RRGGBB" (upper
//  case). The alpha channel is ignored.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ColorUtil::FormatHexColor (uint32_t argb)
{
    wchar_t  buf[8] = {};

    swprintf_s (buf, _countof (buf), L"#%06X", argb & 0x00FFFFFFu);
    return std::wstring (buf);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtil::ResolveColorMonitorTextArgb
//
//  Maps a ColorMonitorTextMode (plus the stored custom color) to the
//  concrete 0xAARRGGBB the framebuffer text renderer should use.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ColorUtil::ResolveColorMonitorTextArgb (ColorMonitorTextMode mode, uint32_t customArgb)
{
    uint32_t  result = kWhiteArgb;

    switch (mode)
    {
        case ColorMonitorTextMode::Green:
            result = kGreenArgb;
            break;

        case ColorMonitorTextMode::Amber:
            result = kAmberArgb;
            break;

        case ColorMonitorTextMode::Custom:
            result = 0xFF000000u | (customArgb & 0x00FFFFFFu);
            break;

        case ColorMonitorTextMode::White:
        default:
            result = kWhiteArgb;
            break;
    }

    return result;
}
