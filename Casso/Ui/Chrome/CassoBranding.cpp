#include "Pch.h"

#include "CassoBranding.h"
#include "Render/IDxuiPainter.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DrawCassowaryRainbow
//
//  Cassowary head + neck silhouette, baked from
//  Resources/Branding/Cassowary.png at 36x54 resolution. Stored as a per-row
//  36-bit bitmask (bit 0 = leftmost column). The bitmask format preserves
//  per-row concavities -- in particular the gap between the underside of the
//  beak and the top of the neck -- that a single (start, end) span per row
//  would erroneously fill.
//
////////////////////////////////////////////////////////////////////////////////

void DrawCassowaryRainbow (IDxuiPainter & painter,
                           float          left,
                           float          top,
                           float          width,
                           float          height,
                           uint32_t       borderArgb)
{
    constexpr int         kGridW       = 36;
    constexpr int         kGridH       = 54;
    constexpr int         kStripeCount = 6;
    static const uint64_t s_kSilhouette[kGridH] = {
        0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL,
        0x000000FE00ULL, 0x000001FF80ULL, 0x000003FFC0ULL, 0x000007FFE0ULL, 0x00000FFFC0ULL,
        0x00000FFFC0ULL, 0x00001FFF80ULL, 0x00001FFF00ULL, 0x00003FFF00ULL, 0x00003FFF00ULL,
        0x00007FFE00ULL, 0x00007FFE00ULL, 0x0000FFFE00ULL, 0x0000FFFC00ULL, 0x0000FFFC00ULL,
        0x0000FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL,
        0x0003FFFE00ULL, 0x0003FFFE00ULL, 0x0003FFFF00ULL, 0x0003FFFF80ULL, 0x0003FFFFC0ULL,
        0x0003FFFFC0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x000FFFFFF0ULL,
        0x001FFFFFF0ULL, 0x003FFFFFF0ULL, 0x007FFFFFF0ULL, 0x00FFFFFFF0ULL, 0x01FFFFFFF8ULL,
        0x01FFFFFFF8ULL, 0x03F007FFF8ULL, 0x038000FFF8ULL, 0x0200007FF8ULL, 0x0000007FF8ULL,
        0x0000007FF8ULL, 0x0000007FF8ULL, 0x000000FFF8ULL, 0x000000FFF8ULL, 0x000001FFF8ULL,
        0x000001FFF8ULL, 0x000001FFF8ULL, 0x000001FFF0ULL, 0x000003FFF0ULL
    };
    static const uint32_t s_kStripeColors[kStripeCount] = {
        0xFF61BB46,  // green
        0xFFFDB827,  // yellow
        0xFFF5821F,  // orange
        0xFFE03A3E,  // red
        0xFF963D97,  // purple
        0xFF009DDC,  // blue
    };

    // Find the first / last non-empty row so the rainbow bands span the actual
    // silhouette extents rather than the full grid (which would waste stripes
    // on the empty rows above).
    int  firstRow = kGridH;
    int  lastRow  = -1;
    int  i        = 0;

    for (i = 0; i < kGridH; i++)
    {
        if (s_kSilhouette[i] != 0)
        {
            if (i < firstRow) { firstRow = i; }
            if (i > lastRow)  { lastRow  = i; }
        }
    }
    if (lastRow < firstRow)
    {
        return;
    }

    int    silhouetteH = lastRow - firstRow + 1;
    float  rowH        = height / (float) kGridH;
    float  colW        = width  / (float) kGridW;

    // One pass over the silhouette, offset by (ox, oy). overrideArgb != 0 paints
    // every run that flat color (the outline pass); 0 uses the rainbow stripes.
    // One FillRect per contiguous bit run so per-row concavities render as
    // actual daylight rather than being span-filled.
    auto drawPass = [&] (float ox, float oy, uint32_t overrideArgb)
    {
        for (int row = firstRow; row <= lastRow; row++)
        {
            uint64_t  bits   = s_kSilhouette[row];
            int       stripe = ((row - firstRow) * kStripeCount) / silhouetteH;
            uint32_t  argb   = (overrideArgb != 0) ? overrideArgb : s_kStripeColors[stripe];
            int       col    = 0;

            while (col < kGridW)
            {
                if ((bits & (1ULL << col)) == 0)
                {
                    col++;
                    continue;
                }

                int  runStart = col;

                while (col < kGridW && (bits & (1ULL << col)) != 0)
                {
                    col++;
                }

                float  x = left + ox + (float) runStart * colW;
                float  y = top  + oy + (float) row      * rowH;
                float  w = (float) (col - runStart) * colW;

                painter.FillRect (x, y, w, rowH + 0.5f, argb);
            }
        }
    };

    // Outline: paint the silhouette in the border color offset one logo-pixel in
    // all eight directions, then lay the rainbow on top -- the border color that
    // pokes past the fill is the ~1px exact-shape outline.
    if (borderArgb != 0)
    {
        float  o = std::max (1.0f, rowH);
        const float  offs[8][2] = {
            { -o, -o }, { 0.0f, -o }, { o, -o },
            { -o, 0.0f },             { o, 0.0f },
            { -o,  o }, { 0.0f,  o }, { o,  o }
        };

        for (const auto & d : offs)
        {
            drawPass (d[0], d[1], borderArgb);
        }
    }

    drawPass (0.0f, 0.0f, 0);
}
