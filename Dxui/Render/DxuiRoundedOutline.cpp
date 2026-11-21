#include "Pch.h"

#include "Render/DxuiRoundedOutline.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRoundedOutline::GetSegments
//
//  Fewer than three points have no outline.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiRoundedOutline::Segment> DxuiRoundedOutline::GetSegments (const std::vector<DxuiPointF> & points, float radius)
{
    constexpr int            kArcSteps = 4;
    size_t                   count     = points.size();
    std::vector<DxuiPointF>  enters (count);
    std::vector<DxuiPointF>  leaves (count);
    std::vector<Segment>     segments;



    if (count < 3)
    {
        return segments;
    }

    for (size_t index = 0; index < count; index++)
    {
        const DxuiPointF  & corner = points[index];
        const DxuiPointF  & prev   = points[(index + count - 1) % count];
        const DxuiPointF  & next   = points[(index + 1) % count];
        float               toPrev = std::hypot (prev.x - corner.x, prev.y - corner.y);
        float               toNext = std::hypot (next.x - corner.x, next.y - corner.y);
        float               r      = (std::min) ({ radius, toPrev / 2.0f, toNext / 2.0f });

        enters[index] = { corner.x + (toPrev > 0.0f ? (prev.x - corner.x) * r / toPrev : 0.0f), corner.y + (toPrev > 0.0f ? (prev.y - corner.y) * r / toPrev : 0.0f) };
        leaves[index] = { corner.x + (toNext > 0.0f ? (next.x - corner.x) * r / toNext : 0.0f), corner.y + (toNext > 0.0f ? (next.y - corner.y) * r / toNext : 0.0f) };
    }

    for (size_t index = 0; index < count; index++)
    {
        const DxuiPointF  & corner = points[index];
        DxuiPointF          from   = enters[index];

        //  The curve through the corner, then the side on to the next one.
        for (int step = 1; step <= kArcSteps; step++)
        {
            float       t  = (float) step / (float) kArcSteps;
            float       u  = 1.0f - t;
            DxuiPointF  to = { (u * u * enters[index].x) + (2.0f * u * t * corner.x) + (t * t * leaves[index].x),
                               (u * u * enters[index].y) + (2.0f * u * t * corner.y) + (t * t * leaves[index].y) };

            segments.push_back ({ from, to });
            from = to;
        }

        segments.push_back ({ leaves[index], enters[(index + 1) % count] });
    }

    return segments;
}
