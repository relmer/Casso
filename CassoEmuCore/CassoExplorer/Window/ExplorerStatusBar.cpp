#include "Pch.h"

#include "CassoExplorer/Window/ExplorerStatusBar.h"
#include "CassoExplorer/CassoExplorerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::ExplorerStatusBar
//
////////////////////////////////////////////////////////////////////////////////

ExplorerStatusBar::ExplorerStatusBar (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::Create
//
//  Explorer's status bar runs on from the list above it, with no lines.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerStatusBar::Create()
{
    m_status = m_window.CreateChild<DxuiStatusBar>();

    m_status->SetDividers (false);
    m_status->SetLeadDip  (kStatusLeadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerStatusBar::Configure()
{
    m_status->SetFields ({ { L"", 0, false, -1, true }, { L"", 0, false, -1, true }, { L"", 0, true },
                           { L"", kStatusFreeDip, false }, { L"", kStatusDetailDip, false }, { L"", kStatusZoomDip, false } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::OnZoomClick
//
//  A click on the zoom level puts it back to 100%.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerStatusBar::OnZoomClick (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Up || ev.button != DxuiMouseButton::Left || !CassoExplorerWindow::Contains (m_status->GetFieldRect (kStatusZoom), ev.positionDip))
    {
        return false;
    }

    m_window.m_dispatch.Dispatch (CassoExplorerCommands::kZoomReset);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::OnActivityClick
//
//  The CassoLink line opens the whole activity log.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerStatusBar::OnActivityClick (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left
        || !CassoExplorerWindow::Contains (m_status->GetFieldRect (kStatusFill), ev.positionDip) || m_window.m_links.GetLatestActivity().empty())
    {
        return false;
    }

    m_window.m_links.ShowActivityLog();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::Fill
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerStatusBar::Fill()
{
    const CassoExplorerBrowser::Status &  status = m_window.m_browser.GetStatus();
    std::wstring                          detail = status.detail;



    //  Bytes or characters selected in the preview take the detail section.
    m_window.m_previewPane.TryGetSelectionDetail (detail);

    //  A link operation's progress has the field while one is pending, so a
    //  refill on a hover or a selection does not replace it between its
    //  updates.
    if (m_window.m_context.links != nullptr && !m_window.m_context.links->GetPendingJobs().empty())
    {
        detail                   = CassoLinkConnections::DescribeJobs (m_window.m_context.links->GetPendingJobs());
        m_window.m_links.MarkJobsShown();
    }

    m_status->SetText (kStatusCount,    status.selection);
    m_status->SetText (kStatusSelected, status.selected);
    m_status->SetText (kStatusFree,     status.freeSpace);
    m_status->SetText (kStatusDetail,   detail);
    m_status->SetText (kStatusZoom,     std::format (L"{}%", m_window.m_prefs.previewZoom));
    m_status->SetText (kStatusFill,     m_window.m_links.GetLatestActivity());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::Layout
//
//  The item count stretches, and free space follows it. The preview's two
//  fields together are as wide as the preview pane, so they begin at its
//  left edge; with the preview hidden they keep a fixed width.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerStatusBar::Layout (const RECT & band)
{
    bool   preview = m_window.m_prefs.previewVisible && (m_window.m_previewPane.GetRect().right > m_window.m_previewPane.GetRect().left);
    RECT   sash    = m_window.m_previewSplitter->GetSashRect();
    int    seam    = (int) (DxuiSplitter::GetSeam (sash.left, sash.right, m_window.m_scaler) + DxuiSplitter::GetLinePx (m_window.m_scaler));
    int    zoomPx  = m_window.m_scaler.ToPx (kStatusZoomDip);
    int    detail  = preview ? (std::max) ((int) band.right - seam - zoomPx, 0) : m_window.m_scaler.ToPx (kStatusDetailDip);



    //  The detail field starts on the splitter's visible line: the lighter of
    //  the two it draws in the middle of its sash, since the darker one is the
    //  color of the panes beside it. The two dividers then meet.
    m_status->SetFields ({ { L"", 0, false, -1, true },
                           { L"", 0, false, -1, true },
                           { L"", 0, true },
                           { L"", kStatusFreeDip, false },
                           { L"", 0, false, detail },
                           { L"", kStatusZoomDip, false } });
    m_status->Layout (band, m_window.m_scaler);

    Fill();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::UpdateTip
//
//  The free space of a disk image says which disk it counts.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerStatusBar::UpdateTip (const DxuiMouseEvent & ev, POINT point)
{
    const std::wstring &  tip      = m_window.m_browser.GetStatus().freeSpaceTip;
    RECT                  anchor   = m_status->GetFieldRect (kStatusFree);
    RECT                  activity = m_status->GetFieldRect (kStatusFill);



    //  The CassoLink line's tip has the latest entries of the log.
    if (ev.kind == DxuiMouseEventKind::Move && CassoExplorerWindow::Contains (activity, point) && !m_window.m_links.GetLatestActivity().empty())
    {
        m_activityTip = m_window.m_links.FormatActivity (kActivityTipLines) + L"\nClick for the whole log.";
        m_window.m_tips.ShowHoverTip (ExplorerTips::TipOwner::Status, activity, m_activityTip.c_str(), kActivityTipWidthDip);
        return;
    }

    if (ev.kind != DxuiMouseEventKind::Move || tip.empty() || !CassoExplorerWindow::Contains (anchor, point))
    {
        m_window.m_tips.HideHoverTip (ExplorerTips::TipOwner::Status);
        return;
    }

    m_window.m_tips.ShowHoverTip (ExplorerTips::TipOwner::Status, anchor, tip.c_str());
}
