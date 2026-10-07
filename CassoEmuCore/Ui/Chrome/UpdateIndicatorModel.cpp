#include "Pch.h"

#include "Ui/Chrome/UpdateIndicatorModel.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kIndicatorLines
//
//  The title-bar indicator's text, one per session. Short enough to sit
//  beside the caption buttons; {0} is the new version and {1} an em dash.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kIndicatorLines[] =
{
    L"Shiny new Casso!",
    L"Psst{1}update me",
    L"Fresh bits available",
    L"Casso {0} is out",
    L"New toys await",
    L"You're missing out",
    L"{0} is calling",
    L"Update me, maybe?",
    L"Upgrade o'clock",
    L"{0} has landed",
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> UpdateIndicatorModel::GetLines (const std::string & version)
{
    std::vector<std::wstring>  lines;
    std::wstring               wide (version.begin(), version.end());
    wchar_t                    dash = s_kchEmDash;



    for (LPCWSTR pattern : s_kIndicatorLines)
    {
        lines.push_back (std::vformat (pattern, std::make_wformat_args (wide, dash)));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::PickLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateIndicatorModel::PickLine (const std::string & version, const RandomIndexFn & randomIndex)
{
    std::vector<std::wstring>  lines = GetLines (version);
    size_t                     index = randomIndex ? randomIndex (lines.size()) : 0;



    return lines[std::min (index, lines.size() - 1)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::EstimateTextWidthDip
//
//  Laid out before any renderer can measure, so an average glyph width
//  stands in. It errs wide; the text is right-aligned in its box, so the
//  error is a little air on the left, never a clipped word.
//
////////////////////////////////////////////////////////////////////////////////

int UpdateIndicatorModel::EstimateTextWidthDip (std::wstring_view text)
{
    return (int) std::ceil ((float) text.size() * kFontDip * kEstGlyphEm);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::Fit
//
//  `reservedDip` is everything else on the caption: the app icon, the
//  system buttons, the padding. The title gets what is left, but never less
//  than kMinTitleDip while the indicator still shows text; below that the
//  indicator drops to the arrow alone, one button column wide.
//
////////////////////////////////////////////////////////////////////////////////

IndicatorFit UpdateIndicatorModel::Fit (int captionWidthDip, int reservedDip, std::wstring_view text)
{
    IndicatorFit  fit;
    int           withText  = kGlyphColumnDip + EstimateTextWidthDip (text) + kTextPadDip;
    int           available = captionWidthDip - reservedDip - kMinTitleDip;



    fit.showsText = !text.empty() && withText <= available;
    fit.widthDip  = fit.showsText ? withText : kArrowOnlyDip;

    return fit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetSweepProgress
//
//  How far through its shimmer sweep the indicator is, 0 at the start and
//  approaching 1 at the end, or nothing between sweeps. The first sweep
//  comes kFirstSweepMs after the indicator appears, then one every
//  kSweepPeriodMs.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<float> UpdateIndicatorModel::GetSweepProgress (int64_t elapsedMs)
{
    std::optional<float>  progress;
    int64_t               intoPeriod = 0;



    if (elapsedMs >= kFirstSweepMs)
    {
        intoPeriod = (elapsedMs - kFirstSweepMs) % kSweepPeriodMs;

        if (intoPeriod < kSweepMs)
        {
            progress = (float) intoPeriod / (float) kSweepMs;
        }
    }

    return progress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetMsUntilSweep
//
//  How long the idle loop may sleep before the next sweep starts: 0 during
//  one, so a frame is due now.
//
////////////////////////////////////////////////////////////////////////////////

int64_t UpdateIndicatorModel::GetMsUntilSweep (int64_t elapsedMs)
{
    int64_t  until      = 0;
    int64_t  intoPeriod = 0;



    if (elapsedMs < kFirstSweepMs)
    {
        until = kFirstSweepMs - elapsedMs;
    }
    else
    {
        intoPeriod = (elapsedMs - kFirstSweepMs) % kSweepPeriodMs;
        until      = (intoPeriod < kSweepMs) ? 0 : kSweepPeriodMs - intoPeriod;
    }

    return until;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetSweepPhase
//
//  A sweep is a lead pass, then the band, then a short tail while the last
//  glint finishes its twinkle: for the first kLeadMs the glints run across
//  the text ahead of the band, then for kBandMs the band sweeps. During the
//  tail neither is running. `sweepProgress` is the whole sweep's.
//
////////////////////////////////////////////////////////////////////////////////

SweepPhase UpdateIndicatorModel::GetSweepPhase (float sweepProgress)
{
    SweepPhase  phase;
    float       ms     = sweepProgress * (float) kSweepMs;



    if (ms < (float) kLeadMs)
    {
        phase.lead = ms / (float) kLeadMs;
    }
    else if (ms < (float) (kLeadMs + kBandMs))
    {
        phase.band = (ms - (float) kLeadMs) / (float) kBandMs;
    }

    return phase;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetTwinkle
//
//  One glint's twinkle, `sinceStartMs` after it began: smoothstep up to full
//  over kTwinkleRiseMs, held for kTwinkleHoldMs, smoothstep back to nothing
//  over kTwinkleFallMs. Zero before and after, and continuous throughout,
//  so a twinkle fades rather than switches. It is timed in milliseconds,
//  not as a share of a pass, so a fast pass cannot shorten it to a blink.
//
////////////////////////////////////////////////////////////////////////////////

float UpdateIndicatorModel::GetTwinkle (float sinceStartMs)
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
//  UpdateIndicatorModel::GetGlints
//
//  The sparkles, `sweepMs` into a sweep, where `layout` puts them. Each
//  twinkles twice a sweep -- starting as the lead pass reaches its x, and
//  again as the band does -- so, the layout being sorted left to right,
//  they run across ahead of the band and then follow it. Every glint is
//  inside [leftPx, leftPx + widthPx] and on `topPx` or `bottomPx`, which the
//  caller keeps inside the caption.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IndicatorGlint> UpdateIndicatorModel::GetGlints (
    float                 sweepMs,
    const GlintLayout   & layout,
    float                 leftPx,
    float                 widthPx,
    float                 topPx,
    float                 bottomPx)
{
    std::vector<IndicatorGlint>  glints;
    IndicatorGlint               glint;
    float                        at        = 0.0f;
    float                        leadStart = 0.0f;
    float                        bandStart = 0.0f;
    int                          i         = 0;



    for (i = 0; i < kGlintCount; i++)
    {
        at              = layout.at[(size_t) i];
        leadStart       = at * (float) kLeadMs;
        bandStart       = (float) kLeadMs + at * (float) kBandMs;
        glint.x         = leftPx + widthPx * at;
        glint.y         = layout.isTop[(size_t) i] ? topPx : bottomPx;
        glint.intensity = std::max (GetTwinkle (sweepMs - leadStart), GetTwinkle (sweepMs - bandStart));
        glints.push_back (glint);
    }

    return glints;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::MakeEvenGlintLayout
//
//  Evenly spread, alternating top and bottom: the layout before any random
//  source exists, and the fallback when random draws keep crowding.
//
////////////////////////////////////////////////////////////////////////////////

GlintLayout UpdateIndicatorModel::MakeEvenGlintLayout()
{
    GlintLayout  layout;
    float        span   = 1.0f - 2.0f * kGlintEdge;
    int          i      = 0;



    for (i = 0; i < kGlintCount; i++)
    {
        layout.at[(size_t) i]    = kGlintEdge + span * ((float) i + 0.5f) / (float) kGlintCount;
        layout.isTop[(size_t) i] = i % 2 == 0;
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::MakeGlintLayout
//
//  A fresh random layout for one sweep: positions drawn inside the text,
//  away from its ends, sorted left to right, and kept at least
//  kGlintSpacing apart; each on the top or bottom edge at random. Draws
//  that crowd are drawn again, up to kGlintTries times, then the even
//  layout is used, so a source that keeps answering alike cannot stack the
//  glints on one spot.
//
////////////////////////////////////////////////////////////////////////////////

GlintLayout UpdateIndicatorModel::MakeGlintLayout (const RandomIndexFn & randomIndex)
{
    GlintLayout  layout;
    float        span     = 1.0f - 2.0f * kGlintEdge;
    bool         isSpaced = false;
    int          attempt  = 0;
    int          i        = 0;



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
//  UpdateIndicatorModel::GetHoverClockStart
//
//  The shimmer clock start that puts a sweep's beginning at `nowMs`: a hover
//  restarts the schedule rather than slipping an extra sweep into it, so the
//  next periodic sweep comes a full period after the hover's.
//
////////////////////////////////////////////////////////////////////////////////

int64_t UpdateIndicatorModel::GetHoverClockStart (int64_t nowMs)
{
    return nowMs - kFirstSweepMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetBandWeight
//
//  How white the shimmer makes the text at `offsetPx` from the band's
//  center: 1 at the center, 0 at the edges (`halfWidthPx` away) and beyond,
//  falling off by smoothstep between, so the blend has no visible edge --
//  its slope is zero where it meets the plain accent text.
//
////////////////////////////////////////////////////////////////////////////////

float UpdateIndicatorModel::GetBandWeight (float offsetPx, float halfWidthPx)
{
    float  t = 0.0f;



    if (halfWidthPx > 0.0f)
    {
        t = std::clamp (1.0f - std::abs (offsetPx) / halfWidthPx, 0.0f, 1.0f);
    }

    return t * t * (3.0f - 2.0f * t);
}