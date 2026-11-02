#include "Pch.h"

#include "DxuiMarquee.h"
#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMarquee::GetPhasePx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiMarquee::GetPhasePx (float dashPx, int64_t nowMs)
{
    int64_t  cycle = std::max<int64_t> (1, (int64_t) std::ceil (std::max (dashPx, 1.0f) * 2.0f));



    return (float) ((std::max<int64_t> (0, nowMs) / kStepMs) % cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMarquee::DrawRect
//
//  Each side is cut where a dash ends, the dash's color chosen by whether
//  it is an even or an odd dash along the outline.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMarquee::DrawRect (IDxuiPainter & painter, float x, float y, float w, float h, float dashPx, float thicknessPx, int64_t nowMs)
{
    const std::array<std::array<float, 4>, 4>  sides =
    {{
        { x,     y,     x + w, y     },
        { x + w, y,     x + w, y + h },
        { x + w, y + h, x,     y + h },
        { x,     y + h, x,     y     },
    }};
    float  dash   = std::max (dashPx, 1.0f);
    float  phase  = GetPhasePx (dash, nowMs);
    float  start  = 0.0f;
    int    dashes = 0;



    for (const std::array<float, 4> & side : sides)
    {
        float  length = std::hypot (side[2] - side[0], side[3] - side[1]);
        float  s      = start;

        while (length > 0.0f && s < start + length)
        {
            float  k    = std::floor ((s + phase) / dash);
            float  next = std::min ((k + 1.0f) * dash - phase, start + length);
            float  from = (s - start) / length;
            float  to   = (next - start) / length;

            painter.DrawLine (side[0] + (side[2] - side[0]) * from, side[1] + (side[3] - side[1]) * from,
                              side[0] + (side[2] - side[0]) * to,   side[1] + (side[3] - side[1]) * to,
                              thicknessPx, (std::abs ((int64_t) k) % 2 == 0) ? kLightArgb : kDarkArgb);

            s = next;
            dashes++;
        }

        start += length;
    }

    return dashes;
}
