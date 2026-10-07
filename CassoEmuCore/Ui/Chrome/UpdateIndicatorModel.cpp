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
//  A sweep is a lead pass with the band starting under it, then a tail while
//  the last glint finishes its twinkle: for the first kLeadMs the glints run
//  across the text, the band starts kBandStartMs in -- the lead pass half way
//  across -- and sweeps for kBandMs. The two overlap, so both can be running.
//  During the tail neither is. `sweepProgress` is the whole sweep's.
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

    if (ms >= (float) kBandStartMs && ms < (float) (kBandStartMs + kBandMs))
    {
        phase.band = (ms - (float) kBandStartMs) / (float) kBandMs;
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
//  UpdateIndicatorModel::GetLeadStartMs
//
//  When, into a sweep, the lead pass reaches `at` (a fraction of the text's
//  width) and a lead glint there starts to twinkle.
//
////////////////////////////////////////////////////////////////////////////////

float UpdateIndicatorModel::GetLeadStartMs (float at)
{
    return at * (float) kLeadMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetBandStartMs
//
//  When, into a sweep, the band reaches `at` and a band glint there starts
//  to twinkle.
//
////////////////////////////////////////////////////////////////////////////////

float UpdateIndicatorModel::GetBandStartMs (float at)
{
    return (float) kBandStartMs + at * (float) kBandMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel::GetGlints
//
//  The sparkles, `sweepMs` into a sweep: the lead glints first, each
//  twinkling as the lead pass reaches its x, then the band glints, each
//  twinkling as the band does. Each layout being sorted left to right, both
//  sets run across in order. Every glint is inside [leftPx, leftPx +
//  widthPx] and on `topPx` or `bottomPx`, which the caller keeps inside the
//  caption.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IndicatorGlint> UpdateIndicatorModel::GetGlints (
    float                 sweepMs,
    const SweepGlints   & sweepGlints,
    float                 leftPx,
    float                 widthPx,
    float                 topPx,
    float                 bottomPx)
{
    std::vector<IndicatorGlint>  glints;
    IndicatorGlint               glint;
    float                        at     = 0.0f;
    int                          i      = 0;



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
//  UpdateIndicatorModel::DoesBandGlintClash
//
//  True when band glint `j` and some lead glint would be lit at once on the
//  same edge closer than kGlintSpacing, so their stars would run into each
//  other.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateIndicatorModel::DoesBandGlintClash (const SweepGlints & sweepGlints, int j)
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
//  UpdateIndicatorModel::DoGlintsClash
//
//  True when any band glint clashes with a lead glint.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateIndicatorModel::DoGlintsClash (const SweepGlints & sweepGlints)
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
//  UpdateIndicatorModel::MakeEvenSweepGlints
//
//  The even spread for the lead pass, and the same spots on the opposite
//  edges for the band, which can never clash: a band glint shares its x
//  only with a lead glint on the other edge, and every other lead glint is
//  kGlintSpacing away or more.
//
////////////////////////////////////////////////////////////////////////////////

SweepGlints UpdateIndicatorModel::MakeEvenSweepGlints()
{
    SweepGlints  sweepGlints;
    int          i           = 0;



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
//  UpdateIndicatorModel::MakeSweepGlints
//
//  Fresh layouts for one sweep, one per pass, each drawn by
//  MakeGlintLayout. A band glint that would clash with a lead one moves to
//  the other edge; if it clashes there too, the band layout is redrawn, up
//  to kGlintTries times; after that the band takes the lead's
//  spots on the opposite edges, which cannot clash.
//
////////////////////////////////////////////////////////////////////////////////

SweepGlints UpdateIndicatorModel::MakeSweepGlints (const RandomIndexFn & randomIndex)
{
    SweepGlints  sweepGlints;
    bool         isClear     = false;
    int          attempt     = 0;
    int          i           = 0;



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