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
    constexpr int    s_kBrowseWidthDp   = 96;
    constexpr int    s_kGapDp           = 8;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }

    // Destination / dot-style tokens <-> dropdown index. The dpi dropdown maps
    // index 0 -> 288, 1 -> 576.
    int  DestinationToIndex (const std::string & s) { return s == "windowsPrinter" ? 1 : 0; }
    int  DotStyleToIndex    (const std::string & s) { return s == "plain"          ? 1 : 0; }
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
    Adopt (m_destLabel);
    Adopt (m_destination);
    Adopt (m_dpiLabel);
    Adopt (m_dpi);
    Adopt (m_styleLabel);
    Adopt (m_dotStyle);
    Adopt (m_folderLabel);
    Adopt (m_folderValue);
    Adopt (m_browse);
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
    m_destination.SetPopupHost (host);
    m_dpi.SetPopupHost         (host);
    m_dotStyle.SetPopupHost    (host);
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
    int  browseW    = scaler.Px (s_kBrowseWidthDp);
    int  gap        = scaler.Px (s_kGapDp);
    int  childIndent = scaler.Px (18);          // matches DxuiTreeView indent
    int  x           = rect.left + pad;
    int  y           = rect.top  + pad;
    int  controlsX   = x + labelWidth;
    int  rightEdge   = rect.right - pad;


    m_destLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_destLabel.SetText  (L"Send output to:");
    m_destination.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_destination.SetItems ({ L"PNG file", L"Windows printer" });
    y += rowHeight + sectionGap;

    // Folder is a child of the PNG-file destination: indent it and sit it
    // directly under the destination row (dimmed when the printer is chosen).
    m_folderLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_folderLabel.SetText (L"PNG folder:");
    {
        int  valueW = rightEdge - browseW - gap - controlsX;

        if (valueW < scaler.Px (80)) { valueW = scaler.Px (80); }

        m_folderValue.SetRect (MakeRect (controlsX, y, valueW, rowHeight));
        m_browse.SetLabel     (L"Browse...");
        m_browse.Layout       (MakeRect (controlsX + valueW + gap, y, browseW, rowHeight));
    }
    y += rowHeight + sectionGap;

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

    m_destLabel.SetDpi   (dpi);
    m_destination.SetDpi (dpi);
    m_dpiLabel.SetDpi    (dpi);
    m_dpi.SetDpi         (dpi);
    m_styleLabel.SetDpi  (dpi);
    m_dotStyle.SetDpi    (dpi);
    m_folderLabel.SetDpi (dpi);
    m_folderValue.SetDpi (dpi);
    m_browse.SetDpi      (dpi);

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

    m_destination.SetSelected (DestinationToIndex (prefs->printDestination));
    m_dpi.SetSelected         (prefs->printOutputDpi == 576 ? 1 : 0);
    m_dotStyle.SetSelected    (DotStyleToIndex (prefs->printDotStyle));

    m_destination.SetSelect ([this, prefs] (int idx)
    {
        prefs->printDestination = (idx == 1) ? "windowsPrinter" : "pngFile";
        ApplyDestinationEnabled (idx == 0);
        MarkDirty ();
    });

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

    m_browse.SetOnClick ([this]
    {
        std::wstring  picked;

        if (m_browseFolder && m_browseFolder (picked) && !picked.empty ())
        {
            if (m_prefs != nullptr)
            {
                m_prefs->printPngFolder = fs::path (picked).string ();
            }
            RefreshFolderValue ();
            MarkDirty ();
        }
    });

    RefreshFolderValue ();
    ApplyDestinationEnabled (DestinationToIndex (prefs->printDestination) == 0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::RefreshFolderValue
//
//  Shows the user's PNG folder, or the resolved default when unset.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::RefreshFolderValue ()
{
    std::wstring  shown = m_defaultPngFolder;

    if (m_prefs != nullptr && !m_prefs->printPngFolder.empty ())
    {
        shown = fs::path (m_prefs->printPngFolder).wstring ();
    }

    m_folderValue.SetText (shown);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ApplyDestinationEnabled
//
//  The PNG folder only matters for the PNG-file destination; dim it when the
//  Windows printer is selected.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ApplyDestinationEnabled (bool pngSelected)
{
    DxuiTextRole  role = pngSelected ? DxuiTextRole::Body : DxuiTextRole::Disabled;

    m_browse.SetEnabled     (pngSelected);
    m_folderLabel.SetTextRole (role);
    m_folderValue.SetTextRole (role);
}
