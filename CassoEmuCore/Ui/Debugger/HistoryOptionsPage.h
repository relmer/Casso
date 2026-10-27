#pragma once

#include "Pch.h"

#include "Ui/Debugger/ReverseBudget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage
//
//  The History tab of the debugger's Options and of Settings: the check box
//  that turns history recording on, the memory budget as a slider over
//  ReverseBudget's range, with the estimated time of history under it, at
//  the recording's rate once it has one, and the frames between keyframes,
//  which apply from the next start of history.
//
//  A SAVED BUDGET OR INTERVAL ABOVE ITS RANGE IS NOT CHANGED BY OPENING THE
//  PAGE. The slider is placed at its maximum, and the page returns the saved
//  value while the slider is where it was placed.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryOptionsPage : public DxuiPropertyPage
{
public:
    explicit HistoryOptionsPage (std::wstring title);

    void            SetOptions (const ReverseOptions & options, const ReverseBudgetRange & range, const HistoryStatus & status);
    ReverseOptions  GetOptions () const;

    void  Layout (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Test accessors.
    DxuiCheckbox  & GetRecordCheckbox ()       { return m_record;   }
    DxuiSlider    & GetBudgetSlider   ()       { return m_budget;   }
    DxuiLabel     & GetEstimateLabel  ()       { return m_estimate; }
    DxuiSlider    & GetIntervalSlider ()       { return m_interval; }

private:
    static constexpr int  kPagePadDp     = 16;
    static constexpr int  kRowHeightDp   = 28;
    static constexpr int  kRowGapDp      = 6;
    static constexpr int  kSectionGapDp  = 18;
    static constexpr int  kSliderWidthDp = 420;

    // The interval slider's range and ticks, in video frames; the settings
    // allow more, for a preferences file edited by hand.
    static constexpr int  kMaxSliderFrames = 60;
    static constexpr int  kFrameTick       = 5;

    static RECT  MakeRect (int l, int t, int w, int h);
    void         UpdateEstimate ();

    DxuiCheckbox  m_record;
    DxuiLabel     m_budgetLabel;
    DxuiSlider    m_budget;
    DxuiLabel     m_estimate;
    DxuiLabel     m_intervalLabel;
    DxuiSlider    m_interval;
    int           m_savedBudgetMb  = ReverseOptions::kDefaultBudgetMb;
    int           m_placedMb       = ReverseOptions::kDefaultBudgetMb;
    int           m_savedFrames    = ReverseOptions::kDefaultIntervalFrames;
    int           m_placedFrames   = ReverseOptions::kDefaultIntervalFrames;
    HistoryStatus m_status;
};
