#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/CassoExplorerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::ExplorerStatusBar
//
////////////////////////////////////////////////////////////////////////////////

ExplorerStatusBar::ExplorerStatusBar (ExplorerContext & context)
    : m_context (context)
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
    m_status = m_context.window.CreateChild<DxuiStatusBar>();

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
//  ExplorerStatusBar::HandleZoomClick
//
//  A click on the zoom level puts it back to 100%.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerStatusBar::HandleZoomClick (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Up || ev.button != DxuiMouseButton::Left || !ExplorerLayout::Contains (m_status->GetFieldRect (kStatusZoom), ev.positionDip))
    {
        return false;
    }

    m_context.commandManager.Dispatch (CassoExplorerCommands::kZoomReset);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::HandleActivityClick
//
//  The CassoLink line opens the whole activity log.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerStatusBar::HandleActivityClick (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left
        || !ExplorerLayout::Contains (m_status->GetFieldRect (kStatusFill), ev.positionDip) || m_context.cassoLinkManager.GetLatestActivity().empty())
    {
        return false;
    }

    m_context.cassoLinkManager.ShowActivityLog();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar::Fill
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerStatusBar::Fill()
{
    const CassoExplorerBrowser::Status &  status = m_context.browser.GetStatus();
    std::wstring                          detail = status.detail;



    //  Bytes or characters selected in the preview take the detail section.
    m_context.preview.TryGetSelectionDetail (detail);

    //  A link operation's progress has the field while one is pending, so a
    //  refill on a hover or a selection does not replace it between its
    //  updates.
    if (m_context.app.links != nullptr && !m_context.app.links->GetPendingJobs().empty())
    {
        detail                   = CassoLinkConnections::DescribeJobs (m_context.app.links->GetPendingJobs());
        m_context.cassoLinkManager.MarkJobsShown();
    }

    m_status->SetText (kStatusCount,    status.selection);
    m_status->SetText (kStatusSelected, status.selected);
    m_status->SetText (kStatusFree,     status.freeSpace);
    m_status->SetText (kStatusDetail,   detail);
    m_status->SetText (kStatusZoom,     std::format (L"{}%", m_context.prefs.previewZoom));
    m_status->SetText (kStatusFill,     m_context.cassoLinkManager.GetLatestActivity());
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
    bool   preview = m_context.prefs.previewVisible && (m_context.preview.GetRect().right > m_context.preview.GetRect().left);
    RECT   sash    = m_context.layout.GetPreviewSplitter()->GetSashRect();
    int    seam    = (int) (DxuiSplitter::GetSeam (sash.left, sash.right, m_context.scaler) + DxuiSplitter::GetLinePx (m_context.scaler));
    int    zoomPx  = m_context.scaler.ToPx (kStatusZoomDip);
    int    detail  = preview ? (std::max) ((int) band.right - seam - zoomPx, 0) : m_context.scaler.ToPx (kStatusDetailDip);



    //  The detail field starts on the splitter's visible line: the lighter of
    //  the two it draws in the middle of its sash, since the darker one is the
    //  color of the panes beside it. The two dividers then meet.
    m_status->SetFields ({ { L"", 0, false, -1, true },
                           { L"", 0, false, -1, true },
                           { L"", 0, true },
                           { L"", kStatusFreeDip, false },
                           { L"", 0, false, detail },
                           { L"", kStatusZoomDip, false } });
    m_status->Layout (band, m_context.scaler);

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
    const std::wstring &  tip      = m_context.browser.GetStatus().freeSpaceTip;
    RECT                  anchor   = m_status->GetFieldRect (kStatusFree);
    RECT                  activity = m_status->GetFieldRect (kStatusFill);



    //  The CassoLink line's tip has the latest entries of the log.
    if (ev.kind == DxuiMouseEventKind::Move && ExplorerLayout::Contains (activity, point) && !m_context.cassoLinkManager.GetLatestActivity().empty())
    {
        m_activityTip = m_context.cassoLinkManager.FormatActivity (kActivityTipLines) + L"\nClick for the whole log.";
        m_context.tips.ShowHoverTip (ExplorerTips::TipOwner::Status, activity, m_activityTip.c_str(), kActivityTipWidthDip);
        return;
    }

    if (ev.kind != DxuiMouseEventKind::Move || tip.empty() || !ExplorerLayout::Contains (anchor, point))
    {
        m_context.tips.HideHoverTip (ExplorerTips::TipOwner::Status);
        return;
    }

    m_context.tips.ShowHoverTip (ExplorerTips::TipOwner::Status, anchor, tip.c_str());
}
