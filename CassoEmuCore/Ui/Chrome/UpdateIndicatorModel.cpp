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
//  UpdateIndicatorModel::GetGlints
//
//  The sparkles that follow the shimmer: kGlintCount of them, spread evenly
//  across the text and alternating between its top edge and its bottom
//  edge. Each lights as the sweep reaches its x -- `progress` runs across
//  the text from 0 to 1 -- rising to full and falling back to nothing over
//  kTwinkleSpan of the sweep, so they flare one after another behind the
//  band. Every glint is inside [leftPx, leftPx + widthPx] and on `topPx` or
//  `bottomPx`, which the caller keeps inside the caption.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IndicatorGlint> UpdateIndicatorModel::GetGlints (float progress, float leftPx, float widthPx, float topPx, float bottomPx)
{
    std::vector<IndicatorGlint>  glints;
    IndicatorGlint               glint;
    float                        at       = 0.0f;
    float                        distance = 0.0f;
    int                          i        = 0;



    for (i = 0; i < kGlintCount; i++)
    {
        at              = ((float) i + 0.5f) / (float) kGlintCount;
        distance        = std::abs (progress - at);
        glint.x         = leftPx + widthPx * at;
        glint.y         = (i % 2 == 0) ? topPx : bottomPx;
        glint.intensity = std::max (0.0f, 1.0f - distance / (kTwinkleSpan * 0.5f));
        glints.push_back (glint);
    }

    return glints;
}