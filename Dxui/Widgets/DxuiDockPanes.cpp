#include "Pch.h"

#include "Widgets/DxuiDockPanes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::Find
//
////////////////////////////////////////////////////////////////////////////////

const DxuiDockPane * DxuiDockPanes::Find (const std::wstring & pane) const
{
    auto  found = m_panes.find (pane);



    return (found != m_panes.end()) ? &found->second : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockPanes::SetTitle (const std::wstring & pane, const std::wstring & title)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end())
    {
        return false;
    }

    found->second.title = title;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::SetIndicator
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockPanes::SetIndicator (const std::wstring & pane, bool on)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end())
    {
        return false;
    }

    found->second.indicator = on;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::SetLeadingMark
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockPanes::SetLeadingMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end() || found->second.leadMark == mark)
    {
        return false;
    }

    found->second.leadMark = mark;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::SetTabTip
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockPanes::SetTabTip (const std::wstring & pane, const std::wstring & tip)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end())
    {
        return false;
    }

    found->second.tip = tip;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::SetTitleExtra
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockPanes::SetTitleExtra (const std::wstring & pane, int widthDip)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end() || found->second.titleExtraDip == widthDip)
    {
        return false;
    }

    found->second.titleExtraDip = widthDip;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::GetPaneOf
//
//  The pane whose control is `content`, or empty.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockPanes::GetPaneOf (const IDxuiControl * content) const
{
    for (const auto & pane : m_panes)
    {
        if (content != nullptr && pane.second.content == content)
        {
            return pane.first;
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::GetContent
//
//  A pane's control, or null for a pane that was never added.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiDockPanes::GetContent (const std::wstring & pane) const
{
    const DxuiDockPane  * found = Find (pane);



    return (found != nullptr) ? found->content : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::GetTitle
//
//  A pane's title, or its name for a pane that was never added.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockPanes::GetTitle (const std::wstring & pane) const
{
    const DxuiDockPane  * found = Find (pane);



    return (found != nullptr) ? found->title : pane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::GetTip
//
//  A pane's tab tip, empty for a pane that was never added.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockPanes::GetTip (const std::wstring & pane) const
{
    const DxuiDockPane  * found = Find (pane);



    return (found != nullptr) ? found->tip : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes::HasIndicator
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockPanes::HasIndicator (const std::wstring & pane) const
{
    const DxuiDockPane  * found = Find (pane);



    return found != nullptr && found->indicator;
}
