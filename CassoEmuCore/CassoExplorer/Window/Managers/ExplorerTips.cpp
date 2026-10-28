#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/CassoExplorerShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::ExplorerTips
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTips::ExplorerTips (ExplorerContext & context)
    : m_context (context)
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
    m_tooltip.SetMaxTextWidthDip (maxWidthDip);
    m_tooltip.RequestShow (anchor, text, CassoExplorerWindow::GetNowMs());
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
        m_tooltip.RequestHide (CassoExplorerWindow::GetNowMs());
        m_tipOwner = TipOwner::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::Tick
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::Tick (int64_t nowMs)
{
    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (nowMs);
    }
}
