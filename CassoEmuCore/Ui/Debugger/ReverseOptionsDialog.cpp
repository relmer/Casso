#include "Pch.h"

#include "Ui/Debugger/ReverseOptionsDialog.h"
#include "Config/GlobalUserPrefs.h"
#include "Ui/Debugger/BreakpointDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::TryParse
//
//  The range is the one the saved budget is held to.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ReverseOptions> ReverseOptionsDialog::TryParse (
    bool                  isRecording,
    const std::wstring  & budgetText)
{
    ReverseOptions                 options;
    std::optional<ReverseOptions>  parsed;
    bool                           isBudgetOk = false;



    options.isRecording = isRecording;

    isBudgetOk = TryParseWhole (budgetText, GlobalUserPrefs::kMinReverseBudgetMb, GlobalUserPrefs::kMaxReverseBudgetMb, options.budgetMb);

    if (isBudgetOk)
    {
        parsed = options;
    }

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::TryParseWhole
//
//  Decimal digits only, surrounding spaces allowed, within low..high.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseOptionsDialog::TryParseWhole (
    const std::wstring  & text,
    int                   low,
    int                   high,
    int                 & outValue)
{
    constexpr int  kBase = 10;
    size_t         first = text.find_first_not_of (L' ');
    size_t         last  = text.find_last_not_of (L' ');
    int            value = 0;
    size_t         i     = 0;



    if (first == std::wstring::npos)
    {
        return false;
    }

    for (i = first; i <= last; i++)
    {
        if (text[i] < L'0' || text[i] > L'9' || value > high)
        {
            return false;
        }

        value = value * kBase + (text[i] - L'0');
    }

    if (value < low || value > high)
    {
        return false;
    }

    outValue = value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::EstimateMinutes
//
//  History grows with the snapshots taken, so a longer interval stretches
//  the same memory over proportionally more time. That holds roughly: a
//  snapshot further from the one before differs from it in more bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseOptionsDialog::EstimateMinutes (
    int       budgetMb,
    int       intervalFrames,
    double  & outLow,
    double  & outHigh)
{
    double  scale = (double) budgetMb / kMeasuredBudgetMb * (double) intervalFrames / (double) ReverseOptions::kDefaultIntervalFrames;



    outLow  = kBusyMinutesPer64Mb  * scale;
    outHigh = kQuietMinutesPer64Mb * scale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::GetEstimateText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ReverseOptionsDialog::GetEstimateText (const std::wstring & budgetText)
{
    std::optional<ReverseOptions>  parsed = TryParse (true, budgetText);
    double                         low    = 0.0;
    double                         high   = 0.0;



    if (!parsed.has_value())
    {
        return std::format (L"The memory is {} to {} MB.", GlobalUserPrefs::kMinReverseBudgetMb, GlobalUserPrefs::kMaxReverseBudgetMb);
    }

    EstimateMinutes (parsed->budgetMb, ReverseOptions::kDefaultIntervalFrames, low, high);

    return std::format (L"Holds about {:.0f} to {:.0f} minutes of history; a busy program fills it sooner.", low, high);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::OnCreate
//
//  OK reads the check box and the budget, and keeps the dialog open while the
//  budget is out of range, which the estimate line says.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseOptionsDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    m_recordLabel.SetText   (L"History");
    m_budgetLabel.SetText   (L"Memory (MB)");
    m_estimateLabel.SetText (L"");

    m_record.SetLabel   (L"Record history for stepping back");
    m_record.SetChecked (m_current.isRecording);

    m_budget.SetTheme        (m_theme);
    m_budget.SetHwnd         (GetHwnd());
    m_budget.SetTextRenderer (GetTextRenderer());
    m_budget.SetOnChange     ([this] (const std::wstring &) { UpdateEstimate(); Invalidate(); });
    m_budget.SetText         (std::to_wstring (m_current.budgetMb));

    UpdateEstimate();

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->Add (m_recordLabel,   m_record);
    m_body->Add (m_budgetLabel,   m_budget);
    m_body->Add (m_estimateLabel, m_estimate);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_chosen = TryParse (m_record.IsChecked(), m_budget.GetText());

        if (m_chosen.has_value())
        {
            EndDialog (IDOK);
        }
    });

    SetInitialFocus (&m_record);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::UpdateEstimate
//
////////////////////////////////////////////////////////////////////////////////

void ReverseOptionsDialog::UpdateEstimate()
{
    m_estimate.SetText (GetEstimateText (m_budget.GetText()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ReverseOptions> ReverseOptionsDialog::Ask (HWND owner, const IDxuiTheme * theme, const ReverseOptions & current)
{
    HRESULT                   hr     = S_OK;
    ReverseOptionsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme   = theme;
    dialog.m_current = current;

    params.title                    = L"Options";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 640, 240 };
    params.minSizeDip               = { 560, 240 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHR (hr);

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

Error:
    return dialog.m_chosen;
}





