#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IndicatorFit
//
//  How the title-bar indicator fits the caption: with its text, or as the
//  arrow alone, and how wide it is in DIPs.
//
////////////////////////////////////////////////////////////////////////////////

struct IndicatorFit
{
    bool  showsText = false;
    int   widthDip  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModel
//
//  The title-bar update indicator's decisions, as pure functions: which
//  short line it shows this session, whether that line fits beside the
//  caption buttons (the window title gives way first, down to a minimum),
//  and when its shimmer sweeps. All widths are DIPs and times milliseconds.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateIndicatorModel
{
public:
    using RandomIndexFn = std::function<size_t (size_t count)>;

    static constexpr int      kGlyphColumnDip   = 22;     // the arrow, before the text
    static constexpr int      kTextPadDip       = 10;     // after the text, before minimize
    static constexpr int      kArrowOnlyDip     = 46;     // one caption-button column
    static constexpr int      kMinTitleDip      = 80;     // what the title keeps before the text goes
    static constexpr float    kFontDip          = 13.0f;
    static constexpr float    kGlyphFontDip     = 14.0f;
    static constexpr float    kEstGlyphEm       = 0.56f;  // average Segoe UI glyph, generous
    static constexpr int64_t  kFirstSweepMs     = 2000;
    static constexpr int64_t  kSweepPeriodMs    = 8000;
    static constexpr int64_t  kSweepMs          = 1000;

    static std::vector<std::wstring>  GetLines             (const std::string & version);
    static std::wstring               PickLine             (const std::string & version, const RandomIndexFn & randomIndex);
    static int                        EstimateTextWidthDip (std::wstring_view text);
    static IndicatorFit               Fit                  (int captionWidthDip, int reservedDip, std::wstring_view text);
    static std::optional<float>       GetSweepProgress     (int64_t elapsedMs);
    static int64_t                    GetMsUntilSweep      (int64_t elapsedMs);
};
