#include "Pch.h"

#include "FileTypes/QuickDrawCanvas.h"





//  QuickDraw II's standard palettes, as $0RGB.
static constexpr Word  s_kPalette320[16] =
{
    0x0000, 0x0777, 0x0841, 0x072C, 0x000F, 0x0080, 0x0F70, 0x0D00,
    0x0FA9, 0x0FF0, 0x00E0, 0x04DF, 0x0DAF, 0x078F, 0x0CCC, 0x0FFF,
};

static constexpr Word  s_kPalette640[16] =
{
    0x0000, 0x000F, 0x0FF0, 0x0FFF, 0x0000, 0x0D00, 0x00E0, 0x0FFF,
    0x0000, 0x000F, 0x0FF0, 0x0FFF, 0x0000, 0x0D00, 0x00E0, 0x0FFF,
};





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::QuickDrawCanvas
//
//  A new port: a pen of one pixel, black pen and fill patterns, a white
//  background pattern, and the bitmap erased to it.
//
////////////////////////////////////////////////////////////////////////////////

QuickDrawCanvas::QuickDrawCanvas (const Rect & frame, bool mode640) :
    m_frame   (frame),
    m_mode640 (mode640),
    m_width   (frame.right - frame.left),
    m_height  (frame.bottom - frame.top),
    m_mask    (mode640 ? (Byte) 0x03 : (Byte) 0x0F)
{
    m_penPat  = MakeSolid (0);
    m_fillPat = MakeSolid (0);
    m_bkPat   = MakeSolid (0x0F);

    m_pixels.assign ((size_t) m_width * m_height, m_mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::MakeSolid
//
//  A pattern of one color. In 640 mode a color's four bits are two pixels,
//  repeated across the byte.
//
////////////////////////////////////////////////////////////////////////////////

QuickDrawCanvas::Pattern QuickDrawCanvas::MakeSolid (Byte value)
{
    Pattern  pattern = {};
    Byte     nibble  = (Byte) (value & 0x0F);



    pattern.fill ((Byte) ((nibble << 4) | nibble));

    return pattern;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::GetPatternAt
//
//  A pattern is eight rows of four bytes: eight pixels a row in 320 mode
//  and sixteen in 640 mode, the leftmost in the high bits.
//
////////////////////////////////////////////////////////////////////////////////

Byte QuickDrawCanvas::GetPatternAt (const Pattern & pattern, int h, int v) const
{
    constexpr int  kRowBytes = 4;
    constexpr int  kRows     = 8;



    int   row   = ((v % kRows) + kRows) % kRows;
    int   width = m_mode640 ? kRowBytes * 4 : kRowBytes * 2;
    int   col   = ((h % width) + width) % width;
    Byte  value = 0;



    if (m_mode640)
    {
        value = (Byte) ((pattern[(size_t) (row * kRowBytes + col / 4)] >> (6 - (col % 4) * 2)) & 0x03);
    }
    else
    {
        value = (Byte) ((pattern[(size_t) (row * kRowBytes + col / 2)] >> ((col % 2) ? 0 : 4)) & 0x0F);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::Plot
//
//  One pixel at a point of the port, through the clip and the pen mode.
//  A "not" mode inverts the source first.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::Plot (int h, int v, Byte source, int mode)
{
    int     x     = h - m_origin.h - m_frame.left;
    int     y     = v - m_origin.v - m_frame.top;
    Byte  * pixel = nullptr;



    if (x < 0 || y < 0 || x >= m_width || y >= m_height)
    {
        return;
    }

    if (m_hasClip && !IsInRegion (m_clip, h, v))
    {
        return;
    }

    pixel = &m_pixels[(size_t) y * m_width + x];

    if (mode & kModeNot)
    {
        source = (Byte) (~source & m_mask);
    }

    switch (mode & ~kModeNot)
    {
        case kModeOr:   *pixel = (Byte) (*pixel | source);             break;
        case kModeXor:  *pixel = (Byte) (*pixel ^ source);             break;
        case kModeBic:  *pixel = (Byte) (*pixel & ~source & m_mask);   break;
        default:        *pixel = source;                               break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawLine
//
//  The pen moved from one point to the other. Each pixel is drawn once,
//  where the pen's rectangles overlap, so an XOR line doesn't erase itself.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawLine (Point from, Point to)
{
    std::set<std::pair<int, int>>  covered;
    int                            dh    = std::abs (to.h - from.h);
    int                            dv    = -std::abs (to.v - from.v);
    int                            stepH = from.h < to.h ? 1 : -1;
    int                            stepV = from.v < to.v ? 1 : -1;
    int                            error = dh + dv;
    int                            twice = 0;
    Point                          at    = from;



    if (m_penSize.h <= 0 || m_penSize.v <= 0)
    {
        return;
    }

    for (;;)
    {
        for (int pv = 0; pv < m_penSize.v; pv++)
        {
            for (int ph = 0; ph < m_penSize.h; ph++)
            {
                covered.insert ({ at.v + pv, at.h + ph });
            }
        }

        if (at.h == to.h && at.v == to.v)
        {
            break;
        }

        twice = 2 * error;

        if (twice >= dv)
        {
            error += dv;
            at.h  += stepH;
        }

        if (twice <= dh)
        {
            error += dh;
            at.v  += stepV;
        }
    }

    for (const auto & [v, h] : covered)
    {
        Plot (h, v, GetPatternAt (m_penPat, h, v), m_penMode);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawFigure
//
//  Every pixel of the bounds inside the figure. A frame is the pixels inside
//  the figure and outside the inner figure, the pen's size in from its edge.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawFigure (Verb verb, const Rect & bounds, const Inside & inside, const Inside & inner)
{
    Byte  source = 0;



    for (int v = bounds.top; v < bounds.bottom; v++)
    {
        for (int h = bounds.left; h < bounds.right; h++)
        {
            if (!inside (h, v) || (verb == Verb::Frame && inner (h, v)))
            {
                continue;
            }

            switch (verb)
            {
                case Verb::Frame:
                case Verb::Paint:
                    source = GetPatternAt (m_penPat, h, v);
                    Plot (h, v, source, m_penMode);
                    break;

                case Verb::Erase:
                    Plot (h, v, GetPatternAt (m_bkPat, h, v), kModeCopy);
                    break;

                case Verb::Invert:
                    Plot (h, v, m_mask, kModeXor);
                    break;

                case Verb::Fill:
                    Plot (h, v, GetPatternAt (m_fillPat, h, v), kModeCopy);
                    break;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::Inset
//
////////////////////////////////////////////////////////////////////////////////

QuickDrawCanvas::Rect QuickDrawCanvas::Inset (const Rect & rect) const
{
    return Rect { rect.top + m_penSize.v, rect.left + m_penSize.h, rect.bottom - m_penSize.v, rect.right - m_penSize.h };
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawRect
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawRect (Verb verb, const Rect & rect)
{
    Rect  inner = Inset (rect);



    DrawFigure (verb, rect,
               [&] (int h, int v) { return h >= rect.left  && h < rect.right  && v >= rect.top  && v < rect.bottom; },
               [&] (int h, int v) { return h >= inner.left && h < inner.right && v >= inner.top && v < inner.bottom; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::IsInOval
//
//  A pixel is inside when its center is inside the ellipse the rectangle
//  bounds.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawCanvas::IsInOval (const Rect & rect, int h, int v) const
{
    double  radiusH = (rect.right - rect.left) / 2.0;
    double  radiusV = (rect.bottom - rect.top) / 2.0;
    double  dh      = 0;
    double  dv      = 0;



    if (radiusH <= 0 || radiusV <= 0)
    {
        return false;
    }

    dh = (h + 0.5 - (rect.left + radiusH)) / radiusH;
    dv = (v + 0.5 - (rect.top  + radiusV)) / radiusV;

    return dh * dh + dv * dv <= 1.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::IsInRRect
//
//  Inside the rectangle, and in a corner inside that quarter of an oval
//  of the oval size.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawCanvas::IsInRRect (const Rect & rect, int h, int v) const
{
    double  radiusH = (std::min) (m_ovalSize.h, rect.right - rect.left) / 2.0;
    double  radiusV = (std::min) (m_ovalSize.v, rect.bottom - rect.top) / 2.0;
    double  ph      = h + 0.5;
    double  pv      = v + 0.5;
    double  dh      = 0;
    double  dv      = 0;



    if (h < rect.left || h >= rect.right || v < rect.top || v >= rect.bottom)
    {
        return false;
    }

    if (radiusH <= 0 || radiusV <= 0)
    {
        return true;
    }

    dh = (ph - std::clamp (ph, rect.left + radiusH, rect.right  - radiusH)) / radiusH;
    dv = (pv - std::clamp (pv, rect.top  + radiusV, rect.bottom - radiusV)) / radiusV;

    return dh * dh + dv * dv <= 1.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::IsInArc
//
//  Inside the oval and within the arc's angles: degrees clockwise from the
//  top, taken against the rectangle, so 45 is toward its top right corner.
//  A negative arc angle runs counterclockwise.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawCanvas::IsInArc (const Rect & rect, int startAngle, int arcAngle, int h, int v) const
{
    constexpr double  kFullCircle = 360.0;



    double  radiusH = (rect.right - rect.left) / 2.0;
    double  radiusV = (rect.bottom - rect.top) / 2.0;
    double  dh      = 0;
    double  dv      = 0;
    double  angle   = 0;
    double  start   = startAngle;
    double  arc     = arcAngle;



    if (!IsInOval (rect, h, v))
    {
        return false;
    }

    if (arc < 0)
    {
        start += arc;
        arc    = -arc;
    }

    if (arc >= kFullCircle)
    {
        return true;
    }

    dh    = (h + 0.5 - (rect.left + radiusH)) / radiusH;
    dv    = (v + 0.5 - (rect.top  + radiusV)) / radiusV;
    angle = std::atan2 (dh, -dv) * kFullCircle / (2.0 * std::numbers::pi);
    angle = std::fmod (angle - start + 2.0 * kFullCircle, kFullCircle);

    return angle < arc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::IsInPoly
//
//  Even-odd: a pixel center is inside when a ray to its right crosses the
//  polygon's edges an odd number of times.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawCanvas::IsInPoly (const std::vector<Point> & points, int h, int v) const
{
    double  ph     = h + 0.5;
    double  pv     = v + 0.5;
    bool    inside = false;
    size_t  count  = points.size();



    for (size_t i = 0, j = count - 1; i < count; j = i++)
    {
        const Point &  a = points[i];
        const Point &  b = points[j];

        if ((a.v > pv) != (b.v > pv)
            && ph < a.h + (pv - a.v) * (b.h - a.h) / (double) (b.v - a.v))
        {
            inside = !inside;
        }
    }

    return inside;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::IsInRegion
//
//  A row's switch points pair up as runs from one to the next, so a point
//  is inside when an odd number of them are at or left of it.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawCanvas::IsInRegion (const Region & region, int h, int v)
{
    const Rect &  bounds = region.bounds;
    size_t        count  = 0;



    if (h < bounds.left || h >= bounds.right || v < bounds.top || v >= bounds.bottom)
    {
        return false;
    }

    if (region.rows.empty())
    {
        return true;
    }

    if ((size_t) (v - bounds.top) >= region.rows.size())
    {
        return false;
    }

    for (int at : region.rows[(size_t) (v - bounds.top)])
    {
        count += at <= h ? 1 : 0;
    }

    return (count % 2) == 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawRRect
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawRRect (Verb verb, const Rect & rect)
{
    Rect  inner = Inset (rect);



    DrawFigure (verb, rect,
               [&] (int h, int v) { return IsInRRect (rect,  h, v); },
               [&] (int h, int v) { return IsInRRect (inner, h, v); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawOval
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawOval (Verb verb, const Rect & rect)
{
    Rect  inner = Inset (rect);



    DrawFigure (verb, rect,
               [&] (int h, int v) { return IsInOval (rect,  h, v); },
               [&] (int h, int v) { return IsInOval (inner, h, v); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawArc
//
//  A framed arc is its curved edge alone, the pen's width deep.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawArc (Verb verb, const Rect & rect, int startAngle, int arcAngle)
{
    Rect  inner = Inset (rect);



    DrawFigure (verb, rect,
               [&] (int h, int v) { return IsInArc (rect, startAngle, arcAngle, h, v); },
               [&] (int h, int v) { return IsInOval (inner, h, v); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawPoly
//
//  A framed polygon is the pen drawn from each point to the next, so it is
//  closed only when its last point is its first.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawPoly (Verb verb, const std::vector<Point> & points)
{
    Rect  bounds = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };



    if (points.empty())
    {
        return;
    }

    if (verb == Verb::Frame)
    {
        for (size_t i = 1; i < points.size(); i++)
        {
            DrawLine (points[i - 1], points[i]);
        }

        return;
    }

    for (const Point & point : points)
    {
        bounds.top    = (std::min) (bounds.top,    point.v);
        bounds.left   = (std::min) (bounds.left,   point.h);
        bounds.bottom = (std::max) (bounds.bottom, point.v);
        bounds.right  = (std::max) (bounds.right,  point.h);
    }

    DrawFigure (verb, bounds,
               [&] (int h, int v) { return IsInPoly (points, h, v); },
               [&] (int, int)     { return false; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawRegion
//
//  A framed region is its pixels within the pen's size of a pixel outside
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawRegion (Verb verb, const Region & region)
{
    int  penH = m_penSize.h;
    int  penV = m_penSize.v;



    DrawFigure (verb, region.bounds,
               [&] (int h, int v) { return IsInRegion (region, h, v); },
               [&] (int h, int v)
               {
                   return IsInRegion (region, h - penH, v) && IsInRegion (region, h + penH, v)
                       && IsInRegion (region, h, v - penV) && IsInRegion (region, h, v + penV);
               });
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::DrawPixels
//
//  A pixel image copied from its source rectangle to the destination,
//  scaled when the two differ in size. A pixel of a 640-mode image drawn in
//  320 mode is spread over the sixteen values, and in the reverse a value is
//  cut to its top two bits.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::DrawPixels (std::span<const Byte> pixels, int rowBytes, bool mode640, const Rect & bounds, const Rect & source, const Rect & dest, int mode)
{
    constexpr Byte  kSpread640 = 5;



    int     sourceH = source.right - source.left;
    int     sourceV = source.bottom - source.top;
    int     destH   = dest.right - dest.left;
    int     destV   = dest.bottom - dest.top;
    int     col     = 0;
    int     row     = 0;
    size_t  at      = 0;
    Byte    value   = 0;



    if (sourceH <= 0 || sourceV <= 0 || destH <= 0 || destV <= 0)
    {
        return;
    }

    for (int v = 0; v < destV; v++)
    {
        for (int h = 0; h < destH; h++)
        {
            col = source.left + h * sourceH / destH - bounds.left;
            row = source.top  + v * sourceV / destV - bounds.top;
            at  = (size_t) row * rowBytes + (size_t) (mode640 ? col / 4 : col / 2);

            if (col < 0 || row < 0 || at >= pixels.size())
            {
                continue;
            }

            value = mode640 ? (Byte) ((pixels[at] >> (6 - (col % 4) * 2)) & 0x03)
                            : (Byte) ((pixels[at] >> ((col % 2) ? 0 : 4)) & 0x0F);

            if (mode640 && !m_mode640)
            {
                value = (Byte) (value * kSpread640);
            }
            else if (!mode640 && m_mode640)
            {
                value = (Byte) (value >> 2);
            }

            Plot (dest.left + h, dest.top + v, value, mode);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas::WriteBgra
//
//  The pixels in the standard palette. In 640 mode each column of four
//  takes its colors from a separate quarter of the palette.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawCanvas::WriteBgra (std::vector<uint32_t> & outBgra) const
{
    constexpr uint32_t  kOpaque  = 0xFF000000u;
    constexpr uint32_t  kToByte  = 17;



    Word  color = 0;



    outBgra.assign (m_pixels.size(), kOpaque);

    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            color = m_mode640 ? s_kPalette640[(x % 4) * 4 + GetPixel (x, y)]
                              : s_kPalette320[GetPixel (x, y)];

            outBgra[(size_t) y * m_width + x] = kOpaque
                                              | (((color >> 8) & 0x0Fu) * kToByte) << 16
                                              | (((color >> 4) & 0x0Fu) * kToByte) << 8
                                              |  ((color       & 0x0Fu) * kToByte);
        }
    }
}
