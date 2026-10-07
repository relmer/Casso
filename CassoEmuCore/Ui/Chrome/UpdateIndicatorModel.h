#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IndicatorGlint
//
//  One sparkle of the shimmer: its center, and how brightly it twinkles,
//  0 unlit to 1 at full.
//
////////////////////////////////////////////////////////////////////////////////

struct IndicatorGlint
{
    float  x         = 0.0f;
    float  y         = 0.0f;
    float  intensity = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SweepPhase
//
//  Where one sweep is: the glints' lead pass across the text, then the band,
//  each as its own progress from 0 to 1, or nothing while it is not running.
//
////////////////////////////////////////////////////////////////////////////////

struct SweepPhase
{
    std::optional<float>  lead;
    std::optional<float>  band;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GlintLayout
//
//  Where a sweep's glints sit: each one's x as a fraction of the text's
//  width, in ascending order (the twinkles follow it left to right), and
//  whether it sits on the top edge or the bottom.
//
////////////////////////////////////////////////////////////////////////////////

struct GlintLayout
{
    static constexpr int  kCount = 4;

    std::array<float, kCount>  at    = {};
    std::array<bool,  kCount>  isTop = {};
};





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
    static constexpr int64_t  kLeadMs           = 1000;   // the glints' lead pass, ahead of the band
    static constexpr int64_t  kBandMs           = 2000;   // the band itself
    static constexpr int64_t  kTwinkleRiseMs    = 120;    // a glint eases in to full,
    static constexpr int64_t  kTwinkleHoldMs    = 60;     // holds,
    static constexpr int64_t  kTwinkleFallMs    = 200;    // and eases out
    static constexpr int64_t  kTwinkleMs        = kTwinkleRiseMs + kTwinkleHoldMs + kTwinkleFallMs;
    static constexpr int64_t  kSweepMs          = kLeadMs + kBandMs + kTwinkleMs;   // until the last twinkle ends: 3.38 s
    static constexpr int      kGlintCount       = GlintLayout::kCount;
    static constexpr float    kBandFraction     = 0.30f;  // shimmer band width, of the text's width
    static constexpr float    kGlintEdge        = 0.06f;  // glints keep this far (of the text width) from its ends
    static constexpr float    kGlintSpacing     = 0.14f;  // and at least this far from each other
    static constexpr size_t   kGlintSteps       = 1000;   // resolution of a random glint position
    static constexpr int      kGlintTries       = 32;     // draws before falling back to an even spread

    static std::vector<std::wstring>  GetLines             (const std::string & version);
    static std::wstring               PickLine             (const std::string & version, const RandomIndexFn & randomIndex);
    static int                        EstimateTextWidthDip (std::wstring_view text);
    static IndicatorFit               Fit                  (int captionWidthDip, int reservedDip, std::wstring_view text);
    static std::optional<float>       GetSweepProgress     (int64_t elapsedMs);
    static int64_t                    GetMsUntilSweep      (int64_t elapsedMs);
    static float                      GetBandWeight        (float offsetPx, float halfWidthPx);
    static SweepPhase                 GetSweepPhase        (float sweepProgress);
    static float                      GetTwinkle           (float sinceStartMs);
    static std::vector<IndicatorGlint>  GetGlints          (float sweepMs, const GlintLayout & layout, float leftPx, float widthPx, float topPx, float bottomPx);
    static GlintLayout                MakeGlintLayout      (const RandomIndexFn & randomIndex);
    static GlintLayout                MakeEvenGlintLayout  ();
    static int64_t                    GetHoverClockStart   (int64_t nowMs);
};
