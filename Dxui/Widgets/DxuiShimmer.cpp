#include "Pch.h"

#include "Widgets/DxuiShimmer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::GetSweepPhase
//
//  A sweep is a lead pass with the band starting under it, then a tail while
//  the last glint finishes its twinkle: for the first kLeadMs the glints run
//  across the text, the band starts kBandStartMs in -- the lead pass half way
//  across -- and sweeps for kBandMs. The two overlap, so both can be running.
//  During the tail neither is. `sweepProgress` is the whole sweep's.
//
////////////////////////////////////////////////////////////////////////////////

DxuiSweepPhase DxuiShimmer::GetSweepPhase (float sweepProgress)
{
    DxuiSweepPhase  phase;
    float           ms    = sweepProgress * (float) kSweepMs;



    if (ms < (float) kLeadMs)
    {
        phase.lead = ms / (float) kLeadMs;
    }

    if (ms >= (float) kBandStartMs && ms < (float) (kBandStartMs + kBandMs))
    {
        phase.band = (ms - (float) kBandStartMs) / (float) kBandMs;
    }

    return phase;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::GetTwinkle
//
//  One glint's twinkle, `sinceStartMs` after it began: smoothstep up to full
//  over kTwinkleRiseMs, held for kTwinkleHoldMs, smoothstep back to nothing
//  over kTwinkleFallMs. Zero before and after, and continuous throughout,
//  so a twinkle fades rather than switches. It is timed in milliseconds,
//  not as a share of a pass, so a fast pass cannot shorten it to a blink.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiShimmer::GetTwinkle (float sinceStartMs)
{
    float  t        = 0.0f;
    float  twinkle  = 0.0f;
    float  holdEnd  = (float) (kTwinkleRiseMs + kTwinkleHoldMs);



    if (sinceStartMs > 0.0f && sinceStartMs < (float) kTwinkleRiseMs)
    {
        t       = sinceStartMs / (float) kTwinkleRiseMs;
        twinkle = t * t * (3.0f - 2.0f * t);
    }
    else if (sinceStartMs >= (float) kTwinkleRiseMs && sinceStartMs <= holdEnd)
    {
        twinkle = 1.0f;
    }
    else if (sinceStartMs > holdEnd && sinceStartMs < (float) kTwinkleMs)
    {
        t       = 1.0f - (sinceStartMs - holdEnd) / (float) kTwinkleFallMs;
        twinkle = t * t * (3.0f - 2.0f * t);
    }

    return twinkle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::GetLeadStartMs
//
//  When, into a sweep, the lead pass reaches `at` (a fraction of the text's
//  width) and a lead glint there starts to twinkle.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiShimmer::GetLeadStartMs (float at)
{
    return at * (float) kLeadMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::GetBandStartMs
//
//  When, into a sweep, the band reaches `at` and a band glint there starts
//  to twinkle.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiShimmer::GetBandStartMs (float at)
{
    return (float) kBandStartMs + at * (float) kBandMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::GetGlints
//
//  The sparkles, `sweepMs` into a sweep: the lead glints first, each
//  twinkling as the lead pass reaches its x, then the band glints, each
//  twinkling as the band does. Each layout being sorted left to right, both
//  sets run across in order. Every glint is inside [leftPx, leftPx +
//  widthPx] and on `topPx` or `bottomPx`, which the caller keeps inside the
//  caption.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiShimmerGlint> DxuiShimmer::GetGlints (
    float                 sweepMs,
    const DxuiSweepGlints   & sweepGlints,
    float                 leftPx,
    float                 widthPx,
    float                 topPx,
    float                 bottomPx)
{
    std::vector<DxuiShimmerGlint>  glints;
    DxuiShimmerGlint               glint;
    float                          at     = 0.0f;
    int                            i      = 0;



    for (i = 0; i < kGlintCount; i++)
    {
        at              = sweepGlints.lead.at[(size_t) i];
        glint.x         = leftPx + widthPx * at;
        glint.y         = sweepGlints.lead.isTop[(size_t) i] ? topPx : bottomPx;
        glint.intensity = GetTwinkle (sweepMs - GetLeadStartMs (at));
        glints.push_back (glint);
    }

    for (i = 0; i < kGlintCount; i++)
    {
        at              = sweepGlints.band.at[(size_t) i];
        glint.x         = leftPx + widthPx * at;
        glint.y         = sweepGlints.band.isTop[(size_t) i] ? topPx : bottomPx;
        glint.intensity = GetTwinkle (sweepMs - GetBandStartMs (at));
        glints.push_back (glint);
    }

    return glints;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::MakeEvenGlintLayout
//
//  Evenly spread, alternating top and bottom: the layout before any random
//  source exists, and the fallback when random draws keep crowding.
//
////////////////////////////////////////////////////////////////////////////////

DxuiGlintLayout DxuiShimmer::MakeEvenGlintLayout()
{
    DxuiGlintLayout  layout;
    float            span   = 1.0f - 2.0f * kGlintEdge;
    int              i      = 0;



    for (i = 0; i < kGlintCount; i++)
    {
        layout.at[(size_t) i]    = kGlintEdge + span * ((float) i + 0.5f) / (float) kGlintCount;
        layout.isTop[(size_t) i] = i % 2 == 0;
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::MakeGlintLayout
//
//  A fresh random layout for one sweep: positions drawn inside the text,
//  away from its ends, sorted left to right, and kept at least
//  kGlintSpacing apart; each on the top or bottom edge at random. Draws
//  that crowd are drawn again, up to kGlintTries times, then the even
//  layout is used, so a source that keeps answering alike cannot stack the
//  glints on one spot.
//
////////////////////////////////////////////////////////////////////////////////

DxuiGlintLayout DxuiShimmer::MakeGlintLayout (const RandomIndexFn & randomIndex)
{
    DxuiGlintLayout  layout;
    float            span     = 1.0f - 2.0f * kGlintEdge;
    bool             isSpaced = false;
    int              attempt  = 0;
    int              i        = 0;



    for (attempt = 0; randomIndex && !isSpaced && attempt < kGlintTries; attempt++)
    {
        for (i = 0; i < kGlintCount; i++)
        {
            layout.at[(size_t) i]    = kGlintEdge + span * (float) std::min (randomIndex (kGlintSteps), kGlintSteps - 1) / (float) (kGlintSteps - 1);
            layout.isTop[(size_t) i] = randomIndex (2) == 0;
        }

        std::sort (layout.at.begin(), layout.at.end());

        isSpaced = true;

        for (i = 1; i < kGlintCount; i++)
        {
            isSpaced = isSpaced && layout.at[(size_t) i] - layout.at[(size_t) i - 1] >= kGlintSpacing;
        }
    }

    if (!isSpaced)
    {
        layout = MakeEvenGlintLayout();
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::DoesBandGlintClash
//
//  True when band glint `j` and some lead glint would be lit at once on the
//  same edge closer than kGlintSpacing, so their stars would run into each
//  other.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiShimmer::DoesBandGlintClash (const DxuiSweepGlints & sweepGlints, int j)
{
    bool   isClash   = false;
    float  leadAt    = 0.0f;
    float  bandAt    = sweepGlints.band.at[(size_t) j];
    float  leadStart = 0.0f;
    float  bandStart = GetBandStartMs (bandAt);
    int    i         = 0;



    for (i = 0; i < kGlintCount; i++)
    {
        leadAt    = sweepGlints.lead.at[(size_t) i];
        leadStart = GetLeadStartMs (leadAt);

        isClash = isClash ||
                  (sweepGlints.lead.isTop[(size_t) i] == sweepGlints.band.isTop[(size_t) j] &&
                   std::abs (leadAt - bandAt) < kGlintSpacing                                 &&
                   leadStart < bandStart + (float) kTwinkleMs                                 &&
                   bandStart < leadStart + (float) kTwinkleMs);
    }

    return isClash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::DoGlintsClash
//
//  True when any band glint clashes with a lead glint.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiShimmer::DoGlintsClash (const DxuiSweepGlints & sweepGlints)
{
    bool  isClash = false;
    int   j       = 0;



    for (j = 0; j < kGlintCount; j++)
    {
        isClash = isClash || DoesBandGlintClash (sweepGlints, j);
    }

    return isClash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::MakeEvenSweepGlints
//
//  The even spread for the lead pass, and the same spots on the opposite
//  edges for the band, which can never clash: a band glint shares its x
//  only with a lead glint on the other edge, and every other lead glint is
//  kGlintSpacing away or more.
//
////////////////////////////////////////////////////////////////////////////////

DxuiSweepGlints DxuiShimmer::MakeEvenSweepGlints()
{
    DxuiSweepGlints  sweepGlints;
    int              i           = 0;



    sweepGlints.lead = MakeEvenGlintLayout();
    sweepGlints.band = sweepGlints.lead;

    for (i = 0; i < kGlintCount; i++)
    {
        sweepGlints.band.isTop[(size_t) i] = !sweepGlints.lead.isTop[(size_t) i];
    }

    return sweepGlints;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::MakeSweepGlints
//
//  Fresh layouts for one sweep, one per pass, each drawn by
//  MakeGlintLayout. A band glint that would clash with a lead one moves to
//  the other edge; if it clashes there too, the band layout is redrawn, up
//  to kGlintTries times; after that the band takes the lead's
//  spots on the opposite edges, which cannot clash.
//
////////////////////////////////////////////////////////////////////////////////

DxuiSweepGlints DxuiShimmer::MakeSweepGlints (const RandomIndexFn & randomIndex)
{
    DxuiSweepGlints  sweepGlints;
    bool             isClear     = false;
    int              attempt     = 0;
    int              i           = 0;



    sweepGlints.lead = MakeGlintLayout (randomIndex);

    for (attempt = 0; !isClear && attempt < kGlintTries; attempt++)
    {
        sweepGlints.band = MakeGlintLayout (randomIndex);

        // A clashing band glint first tries the other edge.
        for (i = 0; i < kGlintCount; i++)
        {
            if (DoesBandGlintClash (sweepGlints, i))
            {
                sweepGlints.band.isTop[(size_t) i] = !sweepGlints.band.isTop[(size_t) i];
            }
        }

        isClear = !DoGlintsClash (sweepGlints);
    }

    if (!isClear)
    {
        sweepGlints.band = sweepGlints.lead;

        for (i = 0; i < kGlintCount; i++)
        {
            sweepGlints.band.isTop[(size_t) i] = !sweepGlints.lead.isTop[(size_t) i];
        }
    }

    return sweepGlints;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::GetBandWeight
//
//  How white the shimmer makes the text at `offsetPx` from the band's
//  center: 1 at the center, 0 at the edges (`halfWidthPx` away) and beyond,
//  falling off by smoothstep between, so the blend has no visible edge --
//  its slope is zero at the band's edges.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiShimmer::GetBandWeight (float offsetPx, float halfWidthPx)
{
    float  t = 0.0f;



    if (halfWidthPx > 0.0f)
    {
        t = std::clamp (1.0f - std::abs (offsetPx) / halfWidthPx, 0.0f, 1.0f);
    }

    return t * t * (3.0f - 2.0f * t);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer::Paint
//
//  Dxui's text renderer has no gradient brush, so the gradient is built from
//  opacity: the band is cut into thin columns, and in each the content is
//  drawn again in white at the opacity GetBandWeight returns for that column's
//  distance from the band's center, over the content beneath -- its own color
//  at the edges, white at the center, a smooth blend between. The lean comes
//  from horizontal slices, each shifted right by its height above the
//  baseline.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiShimmer::Paint (
    IDxuiTextRenderer      & text,
    float                    x,
    float                    y,
    float                    w,
    float                    h,
    float                    glintLeft,
    float                    glintWidth,
    float                    fontPx,
    float                    glintRadiusPx,
    float                    progress,
    const DxuiSweepGlints  & glints,
    const DrawFn           & drawContent)
{
    constexpr int       kSlices       = 6;
    constexpr int       kColumns      = 14;
    constexpr uint32_t  kHighlight    = 0xFFFFFFFF;
    constexpr float     kEdgeOffsetEm = 0.62f;     // glint centers just off the glyph tops and bottoms
    constexpr float     kAlphaMax     = 255.0f;
    constexpr float     kMinSize      = 0.55f;     // a glint's size at the edges of its twinkle



    float                          bandW     = w * kBandFraction;
    float                          sliceH    = h / (float) kSlices;
    float                          travel    = w + bandW + h;
    DxuiSweepPhase                 phase     = GetSweepPhase (progress);
    float                          left      = x - bandW - h + travel * phase.band.value_or (0.0f);
    float                          sliceLeft = 0.0f;
    float                          columnW   = bandW / (float) kColumns;
    float                          offset    = 0.0f;
    float                          weight    = 0.0f;
    int                            column    = 0;
    float                          mid       = y + h * 0.5f;
    float                          top       = std::max (y + glintRadiusPx * 2.0f,     mid - fontPx * kEdgeOffsetEm);
    float                          bottom    = std::min (y + h - glintRadiusPx * 2.0f, mid + fontPx * kEdgeOffsetEm);
    float                          r         = 0.0f;
    uint32_t                       color     = 0;
    HRESULT                        hr        = S_OK;
    int                            i         = 0;
    DxuiPointF                     star[8]   = {};
    std::vector<DxuiShimmerGlint>  lit;



    // The band runs only in the second half of a sweep; the lead pass before
    // it is glints alone.
    for (i = 0; phase.band.has_value() && i < kSlices; i++)
    {
        sliceLeft = left + (float) (kSlices - 1 - i) * sliceH;

        for (column = 0; column < kColumns; column++)
        {
            offset = ((float) column + 0.5f) * columnW - bandW * 0.5f;
            weight = GetBandWeight (offset, bandW * 0.5f);
            color  = ((uint32_t) (kAlphaMax * weight) << 24) | (kHighlight & 0x00FFFFFF);

            if (weight <= 0.0f)
            {
                continue;
            }

            hr = text.PushClipRect (sliceLeft + columnW * (float) column, y + sliceH * (float) i, columnW, sliceH);
            IGNORE_RETURN_VALUE (hr, S_OK);

            drawContent (color);

            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    lit = GetGlints (progress * (float) kSweepMs, glints, glintLeft, glintWidth, top, bottom);

    for (const DxuiShimmerGlint & glint : lit)
    {
        if (glint.intensity <= 0.0f)
        {
            continue;
        }

        // Opacity follows the twinkle; size only breathes with it, so the glint
        // fades in place instead of shrinking away to a point.
        r     = glintRadiusPx * (kMinSize + (1.0f - kMinSize) * glint.intensity);
        color = ((uint32_t) (kAlphaMax * glint.intensity) << 24) | (kHighlight & 0x00FFFFFF);

        star[0] = { glint.x,             glint.y - r * 2.0f };
        star[1] = { glint.x + r * 0.4f,  glint.y - r * 0.4f };
        star[2] = { glint.x + r * 2.0f,  glint.y };
        star[3] = { glint.x + r * 0.4f,  glint.y + r * 0.4f };
        star[4] = { glint.x,             glint.y + r * 2.0f };
        star[5] = { glint.x - r * 0.4f,  glint.y + r * 0.4f };
        star[6] = { glint.x - r * 2.0f,  glint.y };
        star[7] = { glint.x - r * 0.4f,  glint.y - r * 0.4f };

        hr = text.FillPolygon (star, std::size (star), color);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
