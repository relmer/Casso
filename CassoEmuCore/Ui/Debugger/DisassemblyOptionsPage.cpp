#include "Pch.h"

#include "Ui/Debugger/DisassemblyOptionsPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage::DisassemblyOptionsPage
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyOptionsPage::DisassemblyOptionsPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    int  i = 0;



    for (i = 0; i < DisassemblyOptions::kOptionCount; i++)
    {
        Adopt (m_checks[(size_t) i]);

        m_checks[(size_t) i].SetLabel           (DisassemblyOptions::GetLabel ((DisassemblyOptions::Option) i));
        m_checks[(size_t) i].SetSingleLineLabel (true);
        m_checks[(size_t) i].SetOnChange        ([this] (bool) { m_preview.SetOptions (GetOptions()); });
    }

    Adopt (m_previewLabel);
    Adopt (m_preview);

    m_previewLabel.SetText (L"Preview");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage::SetOptions
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyOptionsPage::SetOptions (const DisassemblyOptions & options)
{
    int  i = 0;



    for (i = 0; i < DisassemblyOptions::kOptionCount; i++)
    {
        m_checks[(size_t) i].SetChecked (options.IsOn ((DisassemblyOptions::Option) i));
    }

    m_preview.SetOptions (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage::GetOptions
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyOptions DisassemblyOptionsPage::GetOptions() const
{
    DisassemblyOptions  options;
    int                 i       = 0;



    for (i = 0; i < DisassemblyOptions::kOptionCount; i++)
    {
        options.Set ((DisassemblyOptions::Option) i, m_checks[(size_t) i].IsChecked());
    }

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DisassemblyOptionsPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage::Layout
//
//  The toolbar's options down the left column and the four after them down
//  the right, then the preview across the page under them. The page's
//  content width is at least the preview's minimum width.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyOptionsPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi        = scaler.GetDpi();
    int   pad        = scaler.ToPx (kPagePadDp);
    int   rowHeight  = scaler.ToPx (kRowHeightDp);
    int   rowStep    = rowHeight + scaler.ToPx (kRowGapDp);
    int   sectionGap = scaler.ToPx (kSectionGapDp);
    int   x          = rect.left + pad;
    int   y          = rect.top  + pad;
    int   innerW     = std::max (0, (int) (rect.right - rect.left) - pad * 2);
    int   columnGap  = scaler.ToPx (kColumnGapDp);
    int   columnW    = std::max (0, (innerW - columnGap) / 2);
    int   top        = y;
    int   bottom     = y;
    int   column     = 0;
    int   row        = 0;
    int   i          = 0;



    for (i = 0; i < DisassemblyOptions::kOptionCount; i++)
    {
        column = (i < DisassemblyOptions::kFirstTextOptionCount) ? 0 : 1;
        row    = (column == 0) ? i : i - DisassemblyOptions::kFirstTextOptionCount;
        y      = top + row * rowStep;

        m_checks[(size_t) i].Layout (MakeRect (x + column * (columnW + columnGap), y, columnW, rowHeight), scaler);
        m_checks[(size_t) i].SetDpi (dpi);

        bottom = std::max (bottom, y + rowStep);
    }

    y = bottom + sectionGap;

    m_previewLabel.SetRect (MakeRect (x, y, innerW, rowHeight));
    m_previewLabel.SetDpi  (dpi);
    y += rowStep;

    m_preview.Layout (MakeRect (x, y, innerW, scaler.ToPx (DisassemblyPreview::kHeightDip)), scaler);

    DxuiPanel::SetBounds (rect);

    SetContentWidthPx  (scaler.ToPx (DisassemblyPreview::kMinWidthDip) + pad * 2);
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





