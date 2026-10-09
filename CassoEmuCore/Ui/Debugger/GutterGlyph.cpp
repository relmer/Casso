#include "Pch.h"

#include "Ui/Debugger/GutterGlyph.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::MakeDot
//
//  A breakpoint's dot in a color, filled or a ring, drawn as an image so it
//  can be larger than the text beside it. Drawn in a 16-DIP box, the dot is
//  11.2 DIP across, the 14 pixels Visual Studio's breakpoint is at 125%.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> GutterGlyph::MakeDot (uint32_t argb, bool filled)
{
    auto  image = std::make_shared<DxuiIconImage>();



    image->width  = kSizePx;
    image->height = kSizePx;
    image->bgraPremul.assign ((size_t) (kSizePx * kSizePx), 0u);

    for (int y = 0; y < kSizePx; y++)
    {
        for (int x = 0; x < kSizePx; x++)
        {
            float  d     = std::hypot (x + 0.5f - kSizePx * 0.5f, y + 0.5f - kSizePx * 0.5f);
            float  outer = std::clamp (kDotRadiusPx - d + 0.5f, 0.0f, 1.0f);
            float  inner = filled ? 0.0f : std::clamp (kDotRadiusPx - kRingPx - d + 0.5f, 0.0f, 1.0f);

            image->bgraPremul[(size_t) (y * kSizePx + x)] = MakePixel (argb, outer - inner);
        }
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::MakeArrow
//
//  The PC's arrow in a color: a triangle pointing right, its left side on
//  the dot's left edge and as tall as the dot, its edges smoothed by
//  sampling each pixel kSamples times to a side.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> GutterGlyph::MakeArrow (uint32_t argb)
{
    auto  image = std::make_shared<DxuiIconImage>();



    image->width  = kSizePx;
    image->height = kSizePx;
    image->bgraPremul.assign ((size_t) (kSizePx * kSizePx), 0u);

    for (int y = 0; y < kSizePx; y++)
    {
        for (int x = 0; x < kSizePx; x++)
        {
            image->bgraPremul[(size_t) (y * kSizePx + x)] = MakePixel (argb, GetArrowCoverage (x, y));
        }
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::MakeOver
//
//  One image drawn over another, as a line with both a breakpoint and the PC
//  shows the PC's arrow over the dot. The result is the size of `under`; the
//  part of `over` outside it is left out.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> GutterGlyph::MakeOver (const DxuiIconImage & under, const DxuiIconImage & over)
{
    auto  image = std::make_shared<DxuiIconImage> (under);



    for (int y = 0; y < under.height && y < over.height; y++)
    {
        for (int x = 0; x < under.width && x < over.width; x++)
        {
            size_t  at = (size_t) (y * under.width + x);

            image->bgraPremul[at] = BlendOver (under.bgraPremul[at], over.bgraPremul[(size_t) (y * over.width + x)]);
        }
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::MakeMarked
//
//  The line the PC is on shows the arrow over its dot, as Visual Studio
//  draws its arrow over a breakpoint; a line with no dot shows the arrow
//  itself, the same image rather than a copy.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> GutterGlyph::MakeMarked (
    const std::shared_ptr<const DxuiIconImage>  & dot,
    const std::shared_ptr<const DxuiIconImage>  & arrow)
{
    std::shared_ptr<const DxuiIconImage>  marked = arrow;



    if (dot != nullptr && arrow != nullptr)
    {
        marked = MakeOver (*dot, *arrow);
    }

    return marked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::IsInArrow
//
//  The arrow is an equilateral triangle pointing right: its upright side on
//  the dot's left edge, spanning the dot's height, and its tip on the image's
//  middle row.
//
////////////////////////////////////////////////////////////////////////////////

bool GutterGlyph::IsInArrow (float x, float y)
{
    float  middle = kSizePx * 0.5f;
    float  left   = middle - kDotRadiusPx;
    float  width  = kDotRadiusPx * std::numbers::sqrt3_v<float>;
    float  toTip  = left + width - x;



    return x >= left && toTip >= 0.0f && std::abs (y - middle) <= kDotRadiusPx * toTip / width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::MakePixel
//
//  A color at a coverage, premultiplied, as DxuiIconImage holds its pixels.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t GutterGlyph::MakePixel (uint32_t argb, float coverage)
{
    constexpr float  kFull = 255.0f;
    auto             scale = [coverage] (uint32_t channel) { return (uint32_t) std::lround ((float) (channel & 0xFF) * coverage); };



    return ((uint32_t) std::lround (coverage * kFull) << 24) | (scale (argb >> 16) << 16) | (scale (argb >> 8) << 8) | scale (argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::GetArrowCoverage
//
//  How much of a pixel the arrow covers, from kSamples to a side.
//
////////////////////////////////////////////////////////////////////////////////

float GutterGlyph::GetArrowCoverage (int x, int y)
{
    int  inside = 0;



    for (int sy = 0; sy < kSamples; sy++)
    {
        for (int sx = 0; sx < kSamples; sx++)
        {
            inside += IsInArrow ((float) x + (sx + 0.5f) / kSamples, (float) y + (sy + 0.5f) / kSamples) ? 1 : 0;
        }
    }

    return (float) inside / (float) (kSamples * kSamples);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutterGlyph::BlendOver
//
//  A premultiplied pixel drawn over another: each channel is the top one's
//  plus the bottom one's, by how much of it the top one leaves showing.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t GutterGlyph::BlendOver (uint32_t under, uint32_t over)
{
    constexpr uint32_t  kFull            = 0xFF;
    constexpr int       kChannelShifts[] = { 0, 8, 16, 24 };
    uint32_t            showing          = kFull - (over >> 24);
    uint32_t            blended          = 0;



    for (int shift : kChannelShifts)
    {
        uint32_t  top    = (over  >> shift) & kFull;
        uint32_t  bottom = (under >> shift) & kFull;

        blended |= (std::min) (kFull, top + (bottom * showing + kFull / 2) / kFull) << shift;
    }

    return blended;
}





