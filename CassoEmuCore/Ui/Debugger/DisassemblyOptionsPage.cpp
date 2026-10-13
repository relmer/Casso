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
    }
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
//  One check box a row, down the left inside the page pad, with a gap
//  between the toolbar's options and the four after them.
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
    int   i          = 0;



    for (i = 0; i < DisassemblyOptions::kOptionCount; i++)
    {
        if (i == DisassemblyOptions::kFirstTextOptionCount)
        {
            y += sectionGap;
        }

        m_checks[(size_t) i].Layout (MakeRect (x, y, innerW, rowHeight), scaler);
        m_checks[(size_t) i].SetDpi (dpi);
        y += rowStep;
    }

    DxuiPanel::SetBounds (rect);

    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





