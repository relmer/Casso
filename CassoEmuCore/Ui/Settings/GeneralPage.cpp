#include "Pch.h"

#include "GeneralPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::GeneralPage
//
//  Registers the checkbox in the page's child tree (non-owning Adopt) and
//  forwards its changes to the callback the sheet installs.
//
////////////////////////////////////////////////////////////////////////////////

GeneralPage::GeneralPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_autoUpdateCheckbox);

    m_autoUpdateCheckbox.SetLabel (L"Check for updates automatically");
    m_autoUpdateCheckbox.SetSingleLineLabel (true);
    m_autoUpdateCheckbox.SetOnChange ([this] (bool checked)
    {
        if (m_onAutoUpdateToggled)
        {
            m_onAutoUpdateToggled (checked);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::Layout
//
//  One row at the top left, inside the page pad. The row is clamped to the
//  page's inner width so a narrow window cannot push it past the right edge.
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    int   pad       = scaler.ToPx (kPagePadDp);
    int   rowHeight = scaler.ToPx (kRowHeightDp);
    int   x         = rect.left + pad;
    int   y         = rect.top  + pad;
    int   innerW    = std::max (0, (int) (rect.right - rect.left) - pad * 2);
    int   checkW    = std::min (scaler.ToPx (kCheckWidthDp), innerW);
    RECT  row       = { x, y, x + checkW, y + rowHeight };



    m_autoUpdateCheckbox.Layout (row, scaler);
    m_autoUpdateCheckbox.SetDpi (scaler.GetDpi());

    DxuiPanel::SetBounds (rect);

    // The one control is a fixed height, so it is where the content ends.
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}
