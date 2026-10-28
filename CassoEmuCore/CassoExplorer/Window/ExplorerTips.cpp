#include "Pch.h"

#include "CassoExplorer/Window/ExplorerTips.h"
#include "CassoExplorer/CassoExplorerShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::ExplorerTips
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTips::ExplorerTips (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::ShowHoverTip
//
//  One tooltip serves every part of the window. Each part hides only a tip it
//  showed itself, so a move that one part has no tip for does not take down
//  another's.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::ShowHoverTip (TipOwner owner, const RECT & anchor, const std::wstring & text, float maxWidthDip)
{
    m_tipOwner = owner;
    m_window.m_tooltip.SetMaxTextWidthDip (maxWidthDip);
    m_window.m_tooltip.RequestShow (anchor, text, CassoExplorerWindow::GetNowMs());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::HideHoverTip
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::HideHoverTip (TipOwner owner)
{
    if (m_tipOwner == owner)
    {
        m_window.m_tooltip.RequestHide (CassoExplorerWindow::GetNowMs());
        m_tipOwner = TipOwner::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::UpdateStatusTip
//
//  The free space of a disk image says which disk it counts.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::UpdateStatusTip (const DxuiMouseEvent & ev, POINT point)
{
    const std::wstring &  tip      = m_window.m_browser.GetStatus().freeSpaceTip;
    RECT                  anchor   = m_window.m_status->GetFieldRect (ExplorerLayout::kStatusFree);
    RECT                  activity = m_window.m_status->GetFieldRect (ExplorerLayout::kStatusFill);



    //  The CassoLink line's tip has the latest entries of the log.
    if (ev.kind == DxuiMouseEventKind::Move && CassoExplorerWindow::Contains (activity, point) && !m_window.m_links.GetLatestActivity().empty())
    {
        m_activityTip = m_window.m_links.FormatActivity (kActivityTipLines) + L"\nClick for the whole log.";
        ShowHoverTip (TipOwner::Status, activity, m_activityTip.c_str(), kActivityTipWidthDip);
        return;
    }

    if (ev.kind != DxuiMouseEventKind::Move || tip.empty() || !CassoExplorerWindow::Contains (anchor, point))
    {
        HideHoverTip (TipOwner::Status);
        return;
    }

    ShowHoverTip (TipOwner::Status, anchor, tip.c_str());
}
