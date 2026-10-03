#include "Pch.h"

#include "Ui/Debugger/ReverseOptionsDialog.h"
#include "Config/GlobalUserPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::TryParse
//
//  The ranges are the ones the saved settings are held to.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ReverseOptions> ReverseOptionsDialog::TryParse (
    bool                  isRecording,
    const std::wstring  & budgetText,
    const std::wstring  & intervalText)
{
    ReverseOptions                 options;
    std::optional<ReverseOptions>  parsed;
    bool                           isBudgetOk   = false;
    bool                           isIntervalOk = false;



    options.isRecording = isRecording;

    isBudgetOk   = TryParseWhole (budgetText,   GlobalUserPrefs::kMinReverseBudgetMb, GlobalUserPrefs::kMaxReverseBudgetMb,       options.budgetMb);
    isIntervalOk = TryParseWhole (intervalText, 1,                                    GlobalUserPrefs::kMaxReverseIntervalFrames, options.intervalFrames);

    if (isBudgetOk && isIntervalOk)
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

std::wstring ReverseOptionsDialog::GetEstimateText (
    const std::wstring  & budgetText,
    const std::wstring  & intervalText)
{
    std::optional<ReverseOptions>  parsed = TryParse (true, budgetText, intervalText);
    double                         low    = 0.0;
    double                         high   = 0.0;



    if (!parsed.has_value())
    {
        return std::format (L"The memory is {} to {} MB, and the interval {} to {} frames.",
                            GlobalUserPrefs::kMinReverseBudgetMb, GlobalUserPrefs::kMaxReverseBudgetMb,
                            1, GlobalUserPrefs::kMaxReverseIntervalFrames);
    }

    EstimateMinutes (parsed->budgetMb, parsed->intervalFrames, low, high);

    return std::format (L"Holds about {:.0f} to {:.0f} minutes of history; a busy program fills it sooner.", low, high);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialog::OnCreate
//
//  OK reads the boxes and keeps the dialog open while they hold anything out
//  of range, which the estimate line says.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseOptionsDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    m_recordLabel.SetText   (L"History");
    m_budgetLabel.SetText   (L"Memory (MB)");
    m_intervalLabel.SetText (L"Snapshot every (frames)");
    m_estimateLabel.SetText (L"");
    m_noteLabel.SetText     (L"");
    m_note.SetText          (L"Changes take effect the next time Casso starts.");

    m_record.SetLabel   (L"Record history for stepping back");
    m_record.SetChecked (m_current.isRecording);

    for (DxuiTextInput * box : { &m_budget, &m_interval })
    {
        box->SetTheme        (m_theme);
        box->SetHwnd         (GetHwnd());
        box->SetTextRenderer (GetTextRenderer());
        box->SetOnChange     ([this] (const std::wstring &) { UpdateEstimate(); Invalidate(); });
    }

    m_budget.SetText   (std::to_wstring (m_current.budgetMb));
    m_interval.SetText (std::to_wstring (m_current.intervalFrames));
    UpdateEstimate();

    m_body = CreateDialogContent<BreakpointDialogPanel>();
    m_body->Add (m_recordLabel,   m_record);
    m_body->Add (m_budgetLabel,   m_budget);
    m_body->Add (m_intervalLabel, m_interval);
    m_body->Add (m_estimateLabel, m_estimate);
    m_body->Add (m_noteLabel,     m_note);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_chosen = TryParse (m_record.IsChecked(), m_budget.GetText(), m_interval.GetText());

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
    m_estimate.SetText (GetEstimateText (m_budget.GetText(), m_interval.GetText()));
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
    params.initialSizeDip           = { 640, 300 };
    params.minSizeDip               = { 560, 300 };
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





