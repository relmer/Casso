#include "Pch.h"

#include "Ui/Debugger/HistoryOptionsPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage::HistoryOptionsPage
//
////////////////////////////////////////////////////////////////////////////////

HistoryOptionsPage::HistoryOptionsPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_record);
    Adopt (m_budgetLabel);
    Adopt (m_budget);
    Adopt (m_estimate);
    Adopt (m_intervalLabel);
    Adopt (m_interval);

    m_record.SetLabel           (L"Record history for stepping back");
    m_record.SetSingleLineLabel (true);
    m_budgetLabel.SetText       (L"Memory for history");
    m_estimate.SetTextRole      (DxuiTextRole::Muted);

    m_budget.SetStep           (1.0f);
    m_budget.SetDecimalPlaces  (0);
    m_budget.SetSuffix         (L" MB");
    m_budget.SetShowTicks      (true);
    m_budget.SetOnChange ([this] (float) { UpdateEstimate(); });

    m_intervalLabel.SetText     (L"Frames between keyframes, from the next start of history");
    m_interval.SetStep          (1.0f);
    m_interval.SetDecimalPlaces (0);
    m_interval.SetSuffix        (L" frames");
    m_interval.SetShowTicks     (true);
    m_interval.SetRange         (1.0f, (float) kMaxSliderFrames);
    m_interval.SetTickInterval  ((float) kFrameTick);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage::SetOptions
//
//  Ten ticks across the range; arrow keys move 1 MB, Page Up and Page Down
//  the range's page.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryOptionsPage::SetOptions (const ReverseOptions & options, const ReverseBudgetRange & range, const HistoryStatus & status)
{
    m_status        = status;
    m_savedBudgetMb = options.budgetMb;
    m_placedMb      = ReverseBudget::PlaceOnRange (options.budgetMb, range);

    m_record.SetChecked     (options.isRecording);
    m_budget.SetRange       ((float) range.minMb, (float) range.maxMb);
    m_budget.SetPageStep    ((float) range.pageMb);
    m_budget.SetTickInterval ((float) std::max ((range.maxMb - range.minMb) / ReverseBudget::kTickCount, 1));
    m_budget.SetValue       ((float) m_placedMb);

    m_savedFrames  = options.intervalFrames;
    m_placedFrames = std::clamp (options.intervalFrames, 1, kMaxSliderFrames);

    m_interval.SetValue ((float) m_placedFrames);

    UpdateEstimate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage::GetOptions
//
////////////////////////////////////////////////////////////////////////////////

ReverseOptions HistoryOptionsPage::GetOptions() const
{
    ReverseOptions  options;
    int             valueMb = (int) std::lround (m_budget.GetValue());
    int             frames  = (int) std::lround (m_interval.GetValue());



    options.isRecording    = m_record.IsChecked();
    options.budgetMb       = (valueMb == m_placedMb)    ? m_savedBudgetMb : valueMb;
    options.intervalFrames = (frames  == m_placedFrames) ? m_savedFrames   : frames;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage::UpdateEstimate
//
////////////////////////////////////////////////////////////////////////////////

void HistoryOptionsPage::UpdateEstimate()
{
    m_estimate.SetText (ReverseBudget::GetEstimateText (GetOptions().budgetMb, m_status));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HistoryOptionsPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryOptionsPage::Layout
//
//  The check box, then the memory label, its slider and the estimate under
//  it, then the interval label and its slider, down the left inside the page
//  pad, each clamped to the page's width.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryOptionsPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi        = scaler.GetDpi();
    int   pad        = scaler.ToPx (kPagePadDp);
    int   rowHeight  = scaler.ToPx (kRowHeightDp);
    int   rowStep    = rowHeight + scaler.ToPx (kRowGapDp);
    int   sectionGap = scaler.ToPx (kSectionGapDp);
    int   x          = rect.left + pad;
    int   y          = rect.top  + pad;
    int   innerW     = std::max (0, (int) (rect.right - rect.left) - pad * 2);
    int   sliderW    = std::min (scaler.ToPx (kSliderWidthDp), innerW);



    m_record.Layout (MakeRect (x, y, innerW, rowHeight), scaler);
    y += rowStep + sectionGap;

    m_budgetLabel.SetRect (MakeRect (x, y, innerW, rowHeight));
    y += rowStep;

    m_budget.SetRect (MakeRect (x, y, sliderW, rowHeight));
    y += rowStep;

    m_estimate.SetRect (MakeRect (x, y, innerW, rowHeight));
    y += rowStep + sectionGap;

    m_intervalLabel.SetRect (MakeRect (x, y, innerW, rowHeight));
    y += rowStep;

    m_interval.SetRect (MakeRect (x, y, sliderW, rowHeight));

    m_record.SetDpi        (dpi);
    m_budgetLabel.SetDpi   (dpi);
    m_budget.SetDpi        (dpi);
    m_estimate.SetDpi      (dpi);
    m_intervalLabel.SetDpi (dpi);
    m_interval.SetDpi      (dpi);

    DxuiPanel::SetBounds (rect);

    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





