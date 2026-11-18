#include "Pch.h"

#include "DxuiDockQuery.h"
#include "IDxuiDockQueryHost.h"
#include "Widgets/DxuiTabGroup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockQuery::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::IsChromeAt
//
//  A visible group's title bar or tab strip under the point.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockQuery::IsChromeAt (POINT pointDip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        if (group != nullptr && group->IsVisible() && (Contains (group->GetTitleRect(), pointDip) || Contains (group->GetStripRect(), pointDip)))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::GetGroupPaneAt
//
//  Over a tab, the tab's pane, so a right click on a tab that isn't active
//  targets that tab; elsewhere on a group, its active pane.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockQuery::GetGroupPaneAt (POINT pointDip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        int  index = group->IsVisible() ? group->HitTestTab (pointDip) : -1;



        if (Contains (group->GetBounds(), pointDip))
        {
            return m_host.GetQueryPaneOf ((index >= 0) ? group->GetContent (index) : group->GetActiveContent());
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::FindGroupOf
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabGroup * DxuiDockQuery::FindGroupOf (const std::wstring & pane) const
{
    IDxuiControl  * content = m_host.GetQueryPaneContent (pane);
    DxuiTabGroup  * group   = nullptr;



    if (content == nullptr)
    {
        return nullptr;
    }

    for (const std::unique_ptr<DxuiTabGroup> & candidate : m_host.GetQueryGroups())
    {
        group = (group == nullptr && candidate->IndexOf (content) >= 0) ? candidate.get() : group;
    }

    return group;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::GetPaneTabRect
//
//  The pane's tab in its group's strip, or the whole group when the pane is
//  alone in it; empty when the pane is not docked.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockQuery::GetPaneTabRect (const std::wstring & pane) const
{
    RECT  rect = {};



    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        for (int i = 0; group->IsVisible() && i < (int) group->GetTabCount(); i++)
        {
            if (m_host.GetQueryPaneOf (group->GetContent (i)) == pane)
            {
                rect = (group->GetTabCount() > 1) ? group->GetTabRect (i) : group->GetBounds();
            }
        }
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::GetCarriedTabRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockQuery::GetCarriedTabRect (const std::wstring & pane) const
{
    DxuiTabGroup  * group = pane.empty() ? nullptr : FindGroupOf (pane);



    if (group == nullptr)
    {
        return RECT {};
    }

    return group->GetTabRect (group->IndexOf (m_host.GetQueryPaneContent (pane)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::TryGetPaneGroupRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockQuery::TryGetPaneGroupRect (const std::wstring & pane, RECT & rect) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        if (!group->IsVisible())
        {
            continue;
        }

        for (int i = 0; i < (int) group->GetTabCount(); i++)
        {
            if (m_host.GetQueryPaneOf (group->GetContent (i)) == pane)
            {
                rect = group->GetBounds();
                return rect.right > rect.left && rect.bottom > rect.top;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::TryGetTitleExtraRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockQuery::TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const
{
    IDxuiControl  * content = m_host.GetQueryPaneContent (pane);



    rect = {};

    if (content == nullptr)
    {
        return false;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        if (group->IsVisible() && group->GetActiveContent() == content)
        {
            rect = group->GetTitleExtraRect();
            break;
        }
    }

    return rect.right > rect.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::GetTabAt
//
//  Over a tab's pin or close button the tab gives no tip of its own; the
//  button's is GetTitleButtonTipAt's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockQuery::GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        int  index = group->IsVisible() ? group->HitTestTab (pointDip) : -1;

        if (index >= 0)
        {
            std::wstring               pane     = m_host.GetQueryPaneOf (group->GetContent (index));
            DxuiTabGroup::TitleButton  button   = DxuiTabGroup::TitleButton::Close;
            int                        onButton = -1;
            RECT                       rect     = {};
            bool                       isButton = group->TryGetTabButtonAt (pointDip, button, onButton, rect);

            tab = group->GetTabRect (index);
            tip = !isButton ? m_host.GetQueryPaneTip (pane) : std::wstring();
            return pane;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::GetTitleButtonTipAt
//
//  The pin's tip gives what pressing it does in OnTitleButton: docks the
//  pane or hides it against its edge. A tab's pin and close button are its
//  pane's title bar buttons.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockQuery::GetTitleButtonTipAt (POINT pointDip, RECT & button) const
{
    static constexpr DxuiTabGroup::TitleButton  kButtons[] = { DxuiTabGroup::TitleButton::Menu, DxuiTabGroup::TitleButton::Pin,
                                                               DxuiTabGroup::TitleButton::Close };
    auto                                        pinTip     = [this] (const std::wstring & pane)
    {
        return (m_host.IsQueryFloatingSite() || m_host.IsQueryAutoHidden (pane)) ? L"Dock" : L"Auto hide";
    };



    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        std::wstring               pane;
        DxuiTabGroup::TitleButton  onTab = DxuiTabGroup::TitleButton::Close;
        int                        index = -1;

        if (group->IsVisible() && group->TryGetTabButtonAt (pointDip, onTab, index, button))
        {
            pane = m_host.GetQueryPaneOf (group->GetContent (index));
            return (onTab == DxuiTabGroup::TitleButton::Pin) ? pinTip (pane) : L"Close";
        }

        if (!group->IsVisible() || !Contains (group->GetTitleRect(), pointDip))
        {
            continue;
        }

        pane = m_host.GetQueryPaneOf (group->GetActiveContent());

        for (DxuiTabGroup::TitleButton kind : kButtons)
        {
            button = group->GetTitleButtonRect (kind);

            if (!Contains (button, pointDip))
            {
                continue;
            }

            switch (kind)
            {
            case DxuiTabGroup::TitleButton::Menu:
                return L"Window position";

            case DxuiTabGroup::TitleButton::Pin:
                return pinTip (pane);

            case DxuiTabGroup::TitleButton::Close:
                return L"Close";
            }
        }
    }

    button = {};
    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::GetTabNeighbor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockQuery::GetTabNeighbor (const std::wstring & pane) const
{
    IDxuiControl  * content = m_host.GetQueryPaneContent (pane);



    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        int  index = (content != nullptr && group->IsVisible()) ? group->IndexOf (content) : -1;



        if (index >= 0 && group->GetTabCount() > 1)
        {
            return m_host.GetQueryPaneOf (group->GetContent ((index + 1 < (int) group->GetTabCount()) ? index + 1 : index - 1));
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery::ClassifyHit
//
//  In a floating window, a group's title bar off its buttons is the
//  window's caption, so the system moves the window by it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiDockQuery::ClassifyHit (POINT clientDip) const
{
    static constexpr DxuiTabGroup::TitleButton  kButtons[] = { DxuiTabGroup::TitleButton::Menu, DxuiTabGroup::TitleButton::Pin,
                                                               DxuiTabGroup::TitleButton::Close };



    if (!m_host.IsQueryFloatingSite())
    {
        return DxuiHitTestKind::Client;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_host.GetQueryGroups())
    {
        if (!group->IsVisible() || !Contains (group->GetTitleRect(), clientDip))
        {
            continue;
        }

        for (DxuiTabGroup::TitleButton button : kButtons)
        {
            if (Contains (group->GetTitleButtonRect (button), clientDip))
            {
                return DxuiHitTestKind::Client;
            }
        }

        //  A pane's own control in the title bar is pressed, not dragged.
        if (Contains (group->GetTitleExtraRect(), clientDip))
        {
            return DxuiHitTestKind::Client;
        }

        return DxuiHitTestKind::Caption;
    }

    return DxuiHitTestKind::Client;
}
