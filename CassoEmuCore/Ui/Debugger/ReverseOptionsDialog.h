#pragma once

#include "Pch.h"
#include "Ui/Debugger/BreakpointDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptions
//
//  Reverse execution's settings: whether history is recorded, the memory
//  its snapshots may hold, and how many video frames apart they are taken.
//
////////////////////////////////////////////////////////////////////////////////

struct ReverseOptions
{
    static constexpr int  kDefaultBudgetMb       = 64;
    static constexpr int  kDefaultIntervalFrames = 10;

    bool  isRecording    = true;
    int   budgetMb       = kDefaultBudgetMb;
    int   intervalFrames = kDefaultIntervalFrames;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog
//
//  Tools > Options in the debugger: reverse execution's settings, with the
//  history the memory budget is expected to hold. The settings are read when
//  Casso starts, so a change takes effect then.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseOptionsDialog : public DxuiDialogWindow
{
public:
    //  The options the two boxes give, or empty when either is not a whole
    //  number in its range.
    static std::optional<ReverseOptions>  TryParse (bool isRecording, const std::wstring & budgetText, const std::wstring & intervalText);

    //  Minutes of history a budget holds at an interval: low for a busy
    //  program, high for a quiet one, from measured recordings.
    static void          EstimateMinutes (int budgetMb, int intervalFrames, double & outLow, double & outHigh);

    //  The line under the boxes: the estimate, or what the boxes need.
    static std::wstring  GetEstimateText (const std::wstring & budgetText, const std::wstring & intervalText);

    //  Runs the dialog modally over `owner`; the new options when OK.
    static std::optional<ReverseOptions>  Ask (HWND owner, const IDxuiTheme * theme, const ReverseOptions & current);

protected:
    void  OnCreate() override;

private:
    //  Minutes 64 MB held at the default interval, measured: a game, the
    //  busiest workload, and a program drawing hi-res, the quietest.
    static constexpr double  kBusyMinutesPer64Mb  = 24.1;
    static constexpr double  kQuietMinutesPer64Mb = 43.8;
    static constexpr double  kMeasuredBudgetMb    = 64.0;

    static bool  TryParseWhole (const std::wstring & text, int low, int high, int & outValue);
    void         UpdateEstimate ();

    const IDxuiTheme               * m_theme       = nullptr;
    ReverseOptions                   m_current;
    std::optional<ReverseOptions>    m_chosen;
    DxuiLabel                        m_recordLabel;
    DxuiLabel                        m_budgetLabel;
    DxuiLabel                        m_intervalLabel;
    DxuiLabel                        m_estimateLabel;
    DxuiLabel                        m_noteLabel;
    DxuiLabel                        m_estimate;
    DxuiLabel                        m_note;
    DxuiCheckbox                     m_record;
    DxuiTextInput                    m_budget;
    DxuiTextInput                    m_interval;
    BreakpointDialogPanel          * m_body        = nullptr;
};
