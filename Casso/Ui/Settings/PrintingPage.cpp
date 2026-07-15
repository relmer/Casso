#include "Pch.h"

#include "PrintingPage.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 130;
    constexpr int    s_kDropdownWidthDp = 220;
    constexpr int    s_kSectionGapDp    = 14;
    constexpr int    s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }

    // Dot-style token <-> dropdown index. The dpi dropdown maps index 0 -> 288,
    // 1 -> 576.
    int  DotStyleToIndex (const std::string & s) { return s == "plain" ? 1 : 0; }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::PrintingPage
//
//  Registers each member widget in the page's child tree (non-owning Adopt),
//  like DiskPage; Layout positions them and Rebuild wires their callbacks.
//
////////////////////////////////////////////////////////////////////////////////

PrintingPage::PrintingPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_dpiLabel);
    Adopt (m_dpi);
    Adopt (m_styleLabel);
    Adopt (m_dotStyle);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::SetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::SetPrefs (GlobalUserPrefs * prefs)
{
    m_prefs = prefs;
    Rebuild ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::SetPopupHost (DxuiHwndSource * host)
{
    m_dpi.SetPopupHost      (host);
    m_dotStyle.SetPopupHost (host);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi        = scaler.Dpi ();
    int  pad        = scaler.Px (s_kPagePadDp);
    int  rowHeight  = scaler.Px (s_kRowHeightDp);
    int  labelWidth = scaler.Px (s_kLabelWidthDp);
    int  dropWidth  = scaler.Px (s_kDropdownWidthDp);
    int  sectionGap = scaler.Px (s_kSectionGapDp);
    int  x          = rect.left + pad;
    int  y          = rect.top  + pad;
    int  controlsX  = x + labelWidth;


    m_dpiLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_dpiLabel.SetText  (L"Output resolution:");
    m_dpi.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_dpi.SetItems ({ L"288 dpi (draft)", L"576 dpi (high)" });
    y += rowHeight + sectionGap;

    m_styleLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_styleLabel.SetText  (L"Dot style:");
    m_dotStyle.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_dotStyle.SetItems ({ L"Ink (round dots)", L"Plain (square)" });
    y += rowHeight + sectionGap;

    m_dpiLabel.SetDpi    (dpi);
    m_dpi.SetDpi         (dpi);
    m_styleLabel.SetDpi  (dpi);
    m_dotStyle.SetDpi    (dpi);

    DxuiPanel::SetBounds (rect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::Rebuild
//
//  Syncs widgets to GlobalUserPrefs and wires each change back into it. Edits
//  have no live effect; the apply controller persists / reverts them.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::Rebuild ()
{
    GlobalUserPrefs *  prefs = m_prefs;


    if (prefs == nullptr)
    {
        return;
    }

    m_dpi.SetSelected      (prefs->printOutputDpi == 576 ? 1 : 0);
    m_dotStyle.SetSelected (DotStyleToIndex (prefs->printDotStyle));

    m_dpi.SetSelect ([this, prefs] (int idx)
    {
        prefs->printOutputDpi = (idx == 1) ? 576 : 288;
        MarkDirty ();
    });

    m_dotStyle.SetSelect ([this, prefs] (int idx)
    {
        prefs->printDotStyle = (idx == 1) ? "plain" : "ink";
        MarkDirty ();
    });
}
