#pragma once

#include "Pch.h"

class IDxuiTextRenderer;
class SettingsSheet;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle
//
//  A tab appearing among the others is easy to miss, so when turning a card
//  on adds its tab, Start makes the tab's label shimmer as the update
//  indicator's text does, one sweep every kPeriodMs, until the tab is opened
//  or hidden again. Not with animations turned off. Keyed by page index; any
//  number of tabs can sparkle at once.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsTabSparkle
{
public:
    static constexpr UINT     kTickMs   = 16;
    static constexpr int64_t  kPeriodMs = 5000;

    static_assert (DxuiShimmer::kSweepMs < kPeriodMs, "a sweep ends before the next starts");

    explicit SettingsTabSparkle (SettingsSheet & sheet) : m_sheet (sheet) {}

    void  Start         (int pageIndex);
    void  Update        ();
    bool  IsAnySweeping () const;
    void  PaintAll      (IDxuiTextRenderer & text);

private:
    std::optional<float>  GetProgress (int pageIndex) const;
    void                  Paint       (IDxuiTextRenderer & text, int pageIndex, float progress);

    SettingsSheet           & m_sheet;
    std::map<int, int64_t>    m_startMs;
    DxuiSweepGlints           m_glints = DxuiShimmer::MakeEvenSweepGlints();
};
