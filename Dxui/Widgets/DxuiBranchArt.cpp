#include "Pch.h"

#include "DxuiBranchArt.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiBranchArt::Draw
//
//  Each pixel's coverage is the share of its samples within half a stroke of
//  either line, and its color the ink premultiplied by that share.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiBranchArt::Draw (
    int                      sizePx,
    uint32_t                 inkArgb,
    std::vector<uint32_t>  & outPixels)
{
    constexpr float     kTotal     = (float) (kSamples * kSamples);
    std::vector<Point>  mainLine;
    std::vector<Point>  branch;
    float               size       = (float) std::max (sizePx, 1);
    float               halfStroke = std::max (size * kStrokeRatio, kMinHalfStroke);
    uint32_t            red        = (inkArgb >> 16) & 0xFF;
    uint32_t            green      = (inkArgb >> 8)  & 0xFF;
    uint32_t            blue       = inkArgb         & 0xFF;
    Point               sample;
    int                 covered    = 0;
    uint32_t            alpha      = 0;



    BuildPath (size, mainLine, branch);

    outPixels.assign ((size_t) sizePx * (size_t) sizePx, 0);

    for (int y = 0; y < sizePx; y++)
    {
        for (int x = 0; x < sizePx; x++)
        {
            covered = 0;

            for (int sy = 0; sy < kSamples; sy++)
            {
                for (int sx = 0; sx < kSamples; sx++)
                {
                    sample.x = (float) x + ((float) sx + 0.5f) / (float) kSamples;
                    sample.y = (float) y + ((float) sy + 0.5f) / (float) kSamples;

                    if (GetDistanceToPath (mainLine, sample) <= halfStroke || GetDistanceToPath (branch, sample) <= halfStroke)
                    {
                        covered++;
                    }
                }
            }

            alpha = (uint32_t) std::lround (255.0f * (float) covered / kTotal);

            outPixels[(size_t) y * (size_t) sizePx + (size_t) x] = (alpha << 24) | ((red * alpha / 255) << 16) | ((green * alpha / 255) << 8) | (blue * alpha / 255);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiBranchArt::BuildPath
//
//  The main line across the full width, and the branch: a cubic curve from
//  the main line up to the branch's height, flattened into segments, then
//  straight on to the right edge.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiBranchArt::BuildPath (
    float                 sizePx,
    std::vector<Point>  & outMain,
    std::vector<Point>  & outBranch)
{
    Point  p0 = { kForkX * sizePx,                       kMainY   * sizePx };
    Point  p1 = { (kForkX + kRiseX) * 0.5f * sizePx,     kMainY   * sizePx };
    Point  p2 = { (kForkX + kRiseX) * 0.5f * sizePx,     kBranchY * sizePx };
    Point  p3 = { kRiseX * sizePx,                       kBranchY * sizePx };
    float  t  = 0.0f;
    float  u  = 0.0f;



    outMain   = { { 0.0f, kMainY * sizePx }, { sizePx, kMainY * sizePx } };
    outBranch.clear();

    for (int i = 0; i <= kCurveSegments; i++)
    {
        t = (float) i / (float) kCurveSegments;
        u = 1.0f - t;

        outBranch.push_back (Point { u * u * u * p0.x + 3.0f * u * u * t * p1.x + 3.0f * u * t * t * p2.x + t * t * t * p3.x,
                                     u * u * u * p0.y + 3.0f * u * u * t * p1.y + 3.0f * u * t * t * p2.y + t * t * t * p3.y });
    }

    outBranch.push_back (Point { sizePx, kBranchY * sizePx });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiBranchArt::GetDistanceToPath
//
////////////////////////////////////////////////////////////////////////////////

float DxuiBranchArt::GetDistanceToPath (
    const std::vector<Point>  & path,
    Point                       p)
{
    float  nearest = FLT_MAX;



    for (size_t i = 1; i < path.size(); i++)
    {
        nearest = std::min (nearest, GetDistanceToSegment (path[i - 1], path[i], p));
    }

    return nearest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiBranchArt::GetDistanceToSegment
//
////////////////////////////////////////////////////////////////////////////////

float DxuiBranchArt::GetDistanceToSegment (
    Point  a,
    Point  b,
    Point  p)
{
    float  dx     = b.x - a.x;
    float  dy     = b.y - a.y;
    float  length = dx * dx + dy * dy;
    float  t      = 0.0f;



    if (length > 0.0f)
    {
        t = std::clamp (((p.x - a.x) * dx + (p.y - a.y) * dy) / length, 0.0f, 1.0f);
    }

    return std::hypot (p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}
