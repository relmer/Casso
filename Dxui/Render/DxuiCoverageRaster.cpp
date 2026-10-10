#include "Pch.h"

#include "Render/DxuiCoverageRaster.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::MakeImage
//
////////////////////////////////////////////////////////////////////////////////

DxuiIconImage DxuiCoverageRaster::MakeImage (int width, int height)
{
    DxuiIconImage  image;



    image.width  = std::max (0, width);
    image.height = std::max (0, height);
    image.bgraPremul.assign ((size_t) image.width * (size_t) image.height, 0u);

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::FillPolygon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::FillPolygon (DxuiIconImage & image, const std::vector<DxuiPointF> & points, uint32_t argb)
{
    auto  getSpans = [&points] (float y, std::vector<Span> & spans)
    {
        AddPolygonSpans (points, y, spans);
    };



    Fill (image, GetBounds (points), getSpans, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::FillRoundedRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::FillRoundedRect (DxuiIconImage & image, const DxuiCoverageRect & rect, uint32_t argb)
{
    auto  getSpans = [&rect] (float y, std::vector<Span> & spans)
    {
        AddRoundedSpans (rect, y, spans);
    };



    Fill (image, GetBounds (rect), getSpans, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::FillRects
//
//  Every rectangle at once, so where two overlap the color is laid down only
//  once.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::FillRects (DxuiIconImage & image, const std::vector<DxuiCoverageRect> & rects, uint32_t argb)
{
    RECT  bounds   = { LONG_MAX, LONG_MAX, LONG_MIN, LONG_MIN };
    auto  getSpans = [&rects] (float y, std::vector<Span> & spans)
    {
        for (const DxuiCoverageRect & rect : rects)
        {
            AddRoundedSpans (rect, y, spans);
        }
    };



    if (rects.empty())
    {
        return;
    }

    for (const DxuiCoverageRect & rect : rects)
    {
        RECT  one = GetBounds (rect);

        bounds = RECT { std::min (bounds.left,  one.left),  std::min (bounds.top,    one.top),
                        std::max (bounds.right, one.right), std::max (bounds.bottom, one.bottom) };
    }

    Fill (image, bounds, getSpans, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::FillPolygonRing
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::FillPolygonRing (DxuiIconImage & image, const std::vector<DxuiPointF> & outer, const std::vector<DxuiPointF> & inner, uint32_t argb)
{
    std::vector<Span>  holes;
    auto               getSpans = [&] (float y, std::vector<Span> & spans)
    {
        holes.clear();
        AddPolygonSpans (outer, y, spans);
        AddPolygonSpans (inner, y, holes);
        SubtractSpans   (holes, spans);
    };



    Fill (image, GetBounds (outer), getSpans, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::FillRoundedRing
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::FillRoundedRing (DxuiIconImage & image, const DxuiCoverageRect & outer, const DxuiCoverageRect & inner, uint32_t argb)
{
    std::vector<Span>  holes;
    auto               getSpans = [&] (float y, std::vector<Span> & spans)
    {
        holes.clear();
        AddRoundedSpans (outer, y, spans);
        AddRoundedSpans (inner, y, holes);
        SubtractSpans   (holes, spans);
    };



    Fill (image, GetBounds (outer), getSpans, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::InsetPolygon
//
//  Each corner moves along the bisector of its two edges, as far as keeps
//  both edges `distance` from where they were, so a 45-degree edge stays at
//  45 degrees. Which side is inside follows from the order of the points.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> DxuiCoverageRaster::InsetPolygon (const std::vector<DxuiPointF> & points, float distance)
{
    std::vector<DxuiPointF>  inset;
    size_t                   count = points.size();
    float                    area  = 0.0f;
    float                    turn  = 0.0f;



    for (size_t i = 0; i < count; i++)
    {
        area += points[i].x * points[(i + 1) % count].y - points[(i + 1) % count].x * points[i].y;
    }

    turn = (area >= 0.0f) ? 1.0f : -1.0f;

    for (size_t i = 0; i < count; i++)
    {
        const DxuiPointF  & prev   = points[(i + count - 1) % count];
        const DxuiPointF  & here   = points[i];
        const DxuiPointF  & next   = points[(i + 1) % count];
        float               inX    = here.x - prev.x;
        float               inY    = here.y - prev.y;
        float               outX   = next.x - here.x;
        float               outY   = next.y - here.y;
        float               inLen  = std::hypot (inX, inY);
        float               outLen = std::hypot (outX, outY);
        float               n0x    = 0.0f;
        float               n0y    = 0.0f;
        float               n1x    = 0.0f;
        float               n1y    = 0.0f;
        float               miter  = 0.0f;

        if (inLen <= 0.0f || outLen <= 0.0f)
        {
            inset.push_back (here);
            continue;
        }

        n0x   = -turn * inY  / inLen;
        n0y   =  turn * inX  / inLen;
        n1x   = -turn * outY / outLen;
        n1y   =  turn * outX / outLen;
        miter = 1.0f + n0x * n1x + n0y * n1y;

        inset.push_back (DxuiPointF { here.x + distance * (n0x + n1x) / miter, here.y + distance * (n0y + n1y) / miter });
    }

    return inset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::IsInsidePolygon
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCoverageRaster::IsInsidePolygon (const std::vector<DxuiPointF> & points, float x, float y)
{
    std::vector<Span>  spans;
    bool               inside = false;



    AddPolygonSpans (points, y, spans);

    for (const Span & span : spans)
    {
        inside = inside || (x >= span.left && x < span.right);
    }

    return inside;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::DrawImage
//
//  Source-over of a premultiplied picture the size of the image, every pixel
//  of it scaled by `opacity` first, as a translucent layer lies over what is
//  already drawn. Each channel is worked out whole and rounded once. A
//  picture of another size draws nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::DrawImage (
    DxuiIconImage        & image,
    const DxuiIconImage  & layer,
    float                  opacity)
{
    size_t  pixels = (size_t) image.width * (size_t) image.height;
    bool    isSame = layer.width == image.width && layer.height == image.height &&
                     image.bgraPremul.size() >= pixels && layer.bgraPremul.size() >= pixels;



    if (!isSame)
    {
        return;
    }

    for (size_t i = 0; i < pixels; i++)
    {
        image.bgraPremul[i] = LayPixel (image.bgraPremul[i], layer.bgraPremul[i], opacity);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::FadeImage
//
//  Every pixel, color and alpha alike since they are premultiplied, scaled
//  by `opacity`, so the picture draws that much fainter over anything.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::FadeImage (
    DxuiIconImage  & image,
    float            opacity)
{
    for (uint32_t & pixel : image.bgraPremul)
    {
        pixel = ScalePixel (pixel, opacity);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::TintCovered
//
//  For each pixel whose center lies in the rectangle, the color the picture
//  would show with `argb` laid over it, counted only where the picture
//  covers: a picture drawn over a tint then looks as if the tint lay above
//  it. A pixel the picture covers by `g` takes `g` of the tint's color at
//  the tint's alpha, and keeps the rest of its own; its alpha is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::TintCovered (
    DxuiIconImage           & image,
    const DxuiCoverageRect  & rect,
    uint32_t                  argb)
{
    constexpr float     kChannelMax = 255.0f;
    constexpr float     kPixelMid   = 0.5f;
    constexpr float     kRound      = 0.5f;
    constexpr uint32_t  kChannel    = 0xFFu;
    float               alpha       = (float) (argb >> 24) / kChannelMax;
    float               keep        = 1.0f - alpha;
    long                left        = std::max (0L,                  (long) std::ceil (rect.left   - kPixelMid));
    long                top         = std::max (0L,                  (long) std::ceil (rect.top    - kPixelMid));
    long                right       = std::min ((long) image.width,  (long) std::ceil (rect.right  - kPixelMid));
    long                bottom      = std::min ((long) image.height, (long) std::ceil (rect.bottom - kPixelMid));
    size_t              pixels      = (size_t) image.width * (size_t) image.height;



    if (alpha <= 0.0f || image.bgraPremul.size() < pixels)
    {
        return;
    }

    for (long y = top; y < bottom; y++)
    {
        for (long x = left; x < right; x++)
        {
            uint32_t  & pixel = image.bgraPremul[(size_t) y * (size_t) image.width + (size_t) x];
            float       cover = (float) (pixel >> 24) / kChannelMax;
            uint32_t    r     = (uint32_t) ((float) ((pixel >> 16) & kChannel) * keep + (float) ((argb >> 16) & kChannel) * alpha * cover + kRound);
            uint32_t    g     = (uint32_t) ((float) ((pixel >>  8) & kChannel) * keep + (float) ((argb >>  8) & kChannel) * alpha * cover + kRound);
            uint32_t    b     = (uint32_t) ((float) ( pixel        & kChannel) * keep + (float) ( argb        & kChannel) * alpha * cover + kRound);

            pixel = (pixel & (kChannel << 24)) | (std::min (r, kChannel) << 16) | (std::min (g, kChannel) << 8) | std::min (b, kChannel);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::Fill
//
//  Row by row over the bounds: for each of the 16 samples, where the area
//  lies along that sample's row, and so which pixels of the row have that
//  sample inside it. Sample i, j sits in column i and row j of the 4 by 4
//  grid, and within its cell at offset j across and 3 - i down, which puts
//  every sample in a column and a row of its own.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::Fill (DxuiIconImage & image, const RECT & bounds, const SpanFn & getSpans, uint32_t argb)
{
    constexpr float                            kSampleMid = 0.5f;
    std::array<DxuiPointF, kSampleCount>       samples    = {};
    std::vector<int>                           inside;
    std::vector<Span>                          spans;
    long                                       left       = std::max (0L, bounds.left);
    long                                       top        = std::max (0L, bounds.top);
    long                                       right      = std::min ((long) image.width,  bounds.right);
    long                                       bottom     = std::min ((long) image.height, bounds.bottom);
    size_t                                     pixels     = (size_t) image.width * (size_t) image.height;



    if (right <= left || bottom <= top || image.bgraPremul.size() < pixels)
    {
        return;
    }

    for (int i = 0; i < kSamplesPerAxis; i++)
    {
        for (int j = 0; j < kSamplesPerAxis; j++)
        {
            samples[(size_t) (i * kSamplesPerAxis + j)] = DxuiPointF { ((float) (i * kSamplesPerAxis + j) + kSampleMid) / (float) kSampleCount,
                                                                       ((float) (j * kSamplesPerAxis + kSamplesPerAxis - 1 - i) + kSampleMid) / (float) kSampleCount };
        }
    }

    inside.resize ((size_t) (right - left));

    for (long y = top; y < bottom; y++)
    {
        std::fill (inside.begin(), inside.end(), 0);

        for (const DxuiPointF & sample : samples)
        {
            spans.clear();
            getSpans ((float) y + sample.y, spans);
            MergeSpans (spans);

            for (const Span & span : spans)
            {
                long  first = std::max (left,  (long) std::ceil (span.left  - sample.x));
                long  last  = std::min (right, (long) std::ceil (span.right - sample.x));

                for (long x = first; x < last; x++)
                {
                    inside[(size_t) (x - left)]++;
                }
            }
        }

        for (long x = left; x < right; x++)
        {
            Blend (image.bgraPremul[(size_t) y * (size_t) image.width + (size_t) x], argb, inside[(size_t) (x - left)]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::AddPolygonSpans
//
//  Where the polygon's edges cross the row, in order, taken in pairs. An edge
//  takes its upper end and not its lower one, so a vertex on the row counts
//  once and a level edge never.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::AddPolygonSpans (const std::vector<DxuiPointF> & points, float y, std::vector<Span> & spans)
{
    constexpr size_t     kCrossingsPerSpan = 2;
    std::vector<float>   crossings;
    size_t               count             = points.size();



    for (size_t i = 0; i < count; i++)
    {
        const DxuiPointF  & a = points[i];
        const DxuiPointF  & b = points[(i + 1) % count];

        if ((a.y <= y && y < b.y) || (b.y <= y && y < a.y))
        {
            crossings.push_back (a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y));
        }
    }

    std::sort (crossings.begin(), crossings.end());

    for (size_t i = 0; i + 1 < crossings.size(); i += kCrossingsPerSpan)
    {
        spans.push_back (Span { crossings[i], crossings[i + 1] });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::AddRoundedSpans
//
//  Within a rounded corner's height the row stops short of the side by as
//  much as the corner's circle falls away from it there. A radius larger
//  than half the rectangle's width or height is held to that half.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::AddRoundedSpans (const DxuiCoverageRect & rect, float y, std::vector<Span> & spans)
{
    float  half   = std::min (rect.right - rect.left, rect.bottom - rect.top) / 2.0f;
    float  top    = std::clamp (rect.topRadius,    0.0f, std::max (0.0f, half));
    float  bottom = std::clamp (rect.bottomRadius, 0.0f, std::max (0.0f, half));
    float  rise   = 0.0f;
    float  inset  = 0.0f;



    if (y < rect.top || y >= rect.bottom || rect.right <= rect.left)
    {
        return;
    }

    if (y < rect.top + top)
    {
        rise  = rect.top + top - y;
        inset = top - std::sqrt (std::max (0.0f, top * top - rise * rise));
    }
    else if (y > rect.bottom - bottom)
    {
        rise  = y - (rect.bottom - bottom);
        inset = bottom - std::sqrt (std::max (0.0f, bottom * bottom - rise * rise));
    }

    spans.push_back (Span { rect.left + inset, rect.right - inset });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::MergeSpans
//
//  Sorts the spans and joins any that overlap or touch, so no sample is
//  counted twice.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::MergeSpans (std::vector<Span> & spans)
{
    std::vector<Span>  merged;



    std::sort (spans.begin(), spans.end(), [] (const Span & a, const Span & b) { return a.left < b.left; });

    for (const Span & span : spans)
    {
        if (span.right <= span.left)
        {
            continue;
        }

        if (!merged.empty() && span.left <= merged.back().right)
        {
            merged.back().right = std::max (merged.back().right, span.right);
            continue;
        }

        merged.push_back (span);
    }

    spans = std::move (merged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::SubtractSpans
//
//  Takes the holes, in order, out of each span.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::SubtractSpans (const std::vector<Span> & holes, std::vector<Span> & spans)
{
    std::vector<Span>  kept;



    for (const Span & span : spans)
    {
        float  left = span.left;

        for (const Span & hole : holes)
        {
            if (hole.right <= left || hole.left >= span.right)
            {
                continue;
            }

            if (hole.left > left)
            {
                kept.push_back (Span { left, hole.left });
            }

            left = std::max (left, hole.right);
        }

        if (left < span.right)
        {
            kept.push_back (Span { left, span.right });
        }
    }

    spans = std::move (kept);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::GetBounds
//
//  The whole pixels a polygon touches.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiCoverageRaster::GetBounds (const std::vector<DxuiPointF> & points)
{
    float  left   = FLT_MAX;
    float  top    = FLT_MAX;
    float  right  = -FLT_MAX;
    float  bottom = -FLT_MAX;



    if (points.empty())
    {
        return RECT {};
    }

    for (const DxuiPointF & point : points)
    {
        left   = std::min (left,   point.x);
        top    = std::min (top,    point.y);
        right  = std::max (right,  point.x);
        bottom = std::max (bottom, point.y);
    }

    return RECT { (long) std::floor (left), (long) std::floor (top), (long) std::ceil (right), (long) std::ceil (bottom) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::GetBounds
//
//  The whole pixels a rectangle touches.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiCoverageRaster::GetBounds (const DxuiCoverageRect & rect)
{
    return RECT { (long) std::floor (rect.left), (long) std::floor (rect.top), (long) std::ceil (rect.right), (long) std::ceil (rect.bottom) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::Blend
//
//  Source-over of a straight-alpha color, its alpha scaled by the share of
//  the pixel's samples inside the area, onto a premultiplied pixel.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCoverageRaster::Blend (uint32_t & pixel, uint32_t argb, int samplesInside)
{
    constexpr float     kChannelMax = 255.0f;
    constexpr float     kRound      = 0.5f;
    constexpr uint32_t  kChannel    = 0xFFu;
    float               alpha       = (float) (argb >> 24) / kChannelMax * (float) samplesInside / (float) kSampleCount;
    float               keep        = 1.0f - alpha;
    uint32_t            a           = (uint32_t) (kChannelMax * alpha + (float) (pixel >> 24) * keep + kRound);
    uint32_t            r           = (uint32_t) ((float) ((argb >> 16) & kChannel) * alpha + (float) ((pixel >> 16) & kChannel) * keep + kRound);
    uint32_t            g           = (uint32_t) ((float) ((argb >>  8) & kChannel) * alpha + (float) ((pixel >>  8) & kChannel) * keep + kRound);
    uint32_t            b           = (uint32_t) ((float) ( argb        & kChannel) * alpha + (float) ( pixel        & kChannel) * keep + kRound);



    if (samplesInside <= 0 || alpha <= 0.0f)
    {
        return;
    }

    pixel = (std::min (a, kChannel) << 24) | (std::min (r, kChannel) << 16) | (std::min (g, kChannel) << 8) | std::min (b, kChannel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::ScalePixel
//
//  A premultiplied pixel with every channel scaled, rounded to the nearest
//  level and held to 255.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiCoverageRaster::ScalePixel (
    uint32_t  pixel,
    float     factor)
{
    constexpr float     kRound   = 0.5f;
    constexpr uint32_t  kChannel = 0xFFu;
    uint32_t            a        = (uint32_t) ((float) ( pixel >> 24)             * factor + kRound);
    uint32_t            r        = (uint32_t) ((float) ((pixel >> 16) & kChannel) * factor + kRound);
    uint32_t            g        = (uint32_t) ((float) ((pixel >>  8) & kChannel) * factor + kRound);
    uint32_t            b        = (uint32_t) ((float) ( pixel        & kChannel) * factor + kRound);



    return (std::min (a, kChannel) << 24) | (std::min (r, kChannel) << 16) | (std::min (g, kChannel) << 8) | std::min (b, kChannel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster::LayPixel
//
//  Source-over of one premultiplied pixel, scaled by `opacity`, onto
//  another: the upper one, and what its alpha leaves of the lower, each
//  channel rounded once and held to 255.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiCoverageRaster::LayPixel (
    uint32_t  under,
    uint32_t  over,
    float     opacity)
{
    constexpr float     kChannelMax = 255.0f;
    constexpr float     kRound      = 0.5f;
    constexpr uint32_t  kChannel    = 0xFFu;
    float               keep        = 1.0f - (float) (over >> 24) * opacity / kChannelMax;
    uint32_t            pixel       = 0;



    for (int shift = 0; shift <= 24; shift += 8)
    {
        float     value   = (float) ((over >> shift) & kChannel) * opacity + (float) ((under >> shift) & kChannel) * keep;
        uint32_t  channel = std::min (kChannel, (uint32_t) (value + kRound));

        pixel |= channel << shift;
    }

    return pixel;
}





