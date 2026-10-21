#pragma once

#include "Pch.h"

#include "Render/IDxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmerGlint
//
//  One sparkle of the shimmer: its center, and how brightly it twinkles,
//  0 unlit to 1 at full.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiShimmerGlint
{
    float  x         = 0.0f;
    float  y         = 0.0f;
    float  intensity = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSweepPhase
//
//  Where one sweep is: the glints' lead pass across the text, then the band,
//  each as its own progress from 0 to 1, or nothing while it is not running.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiSweepPhase
{
    std::optional<float>  lead;
    std::optional<float>  band;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGlintLayout
//
//  Where a sweep's glints sit: each one's x as a fraction of the text's
//  width, in ascending order (the twinkles follow it left to right), and
//  whether it sits on the top edge or the bottom.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiGlintLayout
{
    static constexpr int  kCount = 4;

    std::array<float, kCount>  at    = {};
    std::array<bool,  kCount>  isTop = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSweepGlints
//
//  A sweep's two sets of glints, each laid out on its own: one twinkles as
//  the lead pass reaches it, the other as the band does.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiSweepGlints
{
    DxuiGlintLayout  lead;
    DxuiGlintLayout  band;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmer
//
//  A shimmer over text, as pure functions and one paint: a soft bright band
//  sweeping left to right, leaning 45 degrees like light off glass, and small
//  four-point glints on the text's top and bottom edges that twinkle in turn.
//  One sweep lasts kSweepMs; the caller starts each sweep and passes its
//  progress, 0 to 1. Times are milliseconds.
//
//  The caller draws the content the band lights, in the color passed to it;
//  the band is that content drawn again in white, column by column, at an
//  opacity set by each column's distance from the band's center.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiShimmer
{
public:
    using RandomIndexFn = std::function<size_t (size_t count)>;
    using DrawFn        = std::function<void (uint32_t argb)>;

    static constexpr int64_t  kLeadMs        = 1000;   // the glints' lead pass, ahead of the band
    static constexpr int64_t  kBandMs        = 2000;   // the band itself
    static constexpr int64_t  kBandStartMs   = kLeadMs / 2;   // the band starts with the lead pass half way across
    static constexpr int64_t  kTwinkleRiseMs = 500;    // a glint eases in to full,
    static constexpr int64_t  kTwinkleHoldMs = 80;     // holds,
    static constexpr int64_t  kTwinkleFallMs = 500;    // and eases out
    static constexpr int64_t  kTwinkleMs     = kTwinkleRiseMs + kTwinkleHoldMs + kTwinkleFallMs;
    static constexpr int64_t  kSweepMs       = kBandStartMs + kBandMs + kTwinkleMs;   // the later of the band's end and the last twinkle's: 3.58 s
    static constexpr int      kGlintCount    = DxuiGlintLayout::kCount;
    static constexpr float    kBandFraction  = 0.30f;  // shimmer band width, of the text's width
    static constexpr float    kGlintEdge     = 0.06f;  // glints keep this far (of the text width) from its ends
    static constexpr float    kGlintSpacing  = 0.14f;  // and at least this far from each other
    static constexpr size_t   kGlintSteps    = 1000;   // resolution of a random glint position
    static constexpr int      kGlintTries    = 32;     // draws before falling back to a fixed spread

    static float                          GetBandWeight       (float offsetPx, float halfWidthPx);
    static DxuiSweepPhase                 GetSweepPhase       (float sweepProgress);
    static float                          GetTwinkle          (float sinceStartMs);
    static std::vector<DxuiShimmerGlint>  GetGlints           (float sweepMs, const DxuiSweepGlints & sweepGlints, float leftPx, float widthPx, float topPx, float bottomPx);
    static DxuiGlintLayout                MakeGlintLayout     (const RandomIndexFn & randomIndex);
    static DxuiGlintLayout                MakeEvenGlintLayout ();
    static DxuiSweepGlints                MakeSweepGlints     (const RandomIndexFn & randomIndex);
    static DxuiSweepGlints                MakeEvenSweepGlints ();
    static float                          GetLeadStartMs      (float at);
    static float                          GetBandStartMs      (float at);
    static bool                           DoesBandGlintClash  (const DxuiSweepGlints & sweepGlints, int j);
    static bool                           DoGlintsClash       (const DxuiSweepGlints & sweepGlints);

    // The band sweeps the box (x, y, w, h). Glints sit across
    // [glintLeft, glintLeft + glintWidth), just off the top and bottom of
    // text `fontPx` tall centered in the box, and lie inside the box.
    static void  Paint (IDxuiTextRenderer       & text,
                        float                     x,
                        float                     y,
                        float                     w,
                        float                     h,
                        float                     glintLeft,
                        float                     glintWidth,
                        float                     fontPx,
                        float                     glintRadiusPx,
                        float                     progress,
                        const DxuiSweepGlints   & glints,
                        const DrawFn            & drawContent);
};
