#pragma once

#include "Pch.h"

#include "Ui/Debugger/ReverseBudget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage
//
//  The History tab of the debugger's Options: the check box that turns
//  history recording on, and the memory budget as a slider over
//  ReverseBudget's range, with the estimated minutes of history under it.
//
//  A SAVED BUDGET ABOVE THE RANGE IS NOT CHANGED BY OPENING THE PAGE. The
//  slider is placed at its maximum, and the page returns the saved value while
//  the slider is where it was placed.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryOptionsPage : public DxuiPropertyPage
{
public:
    explicit HistoryOptionsPage (std::wstring title);

    void            SetOptions (const ReverseOptions & options, const ReverseBudgetRange & range);
    ReverseOptions  GetOptions () const;

    void  Layout (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Test accessors.
    DxuiCheckbox  & GetRecordCheckbox ()       { return m_record;   }
    DxuiSlider    & GetBudgetSlider   ()       { return m_budget;   }
    DxuiLabel     & GetEstimateLabel  ()       { return m_estimate; }

private:
    static constexpr int  kPagePadDp     = 16;
    static constexpr int  kRowHeightDp   = 28;
    static constexpr int  kRowGapDp      = 6;
    static constexpr int  kSectionGapDp  = 18;
    static constexpr int  kSliderWidthDp = 420;

    static RECT  MakeRect (int l, int t, int w, int h);
    void         UpdateEstimate ();

    DxuiCheckbox  m_record;
    DxuiLabel     m_budgetLabel;
    DxuiSlider    m_budget;
    DxuiLabel     m_estimate;
    int           m_savedBudgetMb  = ReverseOptions::kDefaultBudgetMb;
    int           m_placedMb       = ReverseOptions::kDefaultBudgetMb;
};
