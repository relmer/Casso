#include "Pch.h"

#include "Widgets/DxuiDockGroups.h"
#include "Core/DxuiPaneLayout.h"
#include "Render/IDxuiPainter.h"
#include "Widgets/DxuiDockPanes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGroups::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::IsChromeAt
//
//  A visible group's title bar or tab strip under the point.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGroups::IsChromeAt (POINT pointDip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
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
//  DxuiDockGroups::GetGroupPaneAt
//
//  Over a tab, the tab's pane, so a right click on a tab that isn't active
//  targets that tab; elsewhere on a group, its active pane.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockGroups::GetGroupPaneAt (POINT pointDip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        int  index = group->IsVisible() ? group->HitTestTab (pointDip) : -1;



        if (Contains (group->GetBounds(), pointDip))
        {
            return m_panes.GetPaneOf ((index >= 0) ? group->GetContent (index) : group->GetActiveContent());
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::FindGroupOf
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabGroup * DxuiDockGroups::FindGroupOf (const std::wstring & pane) const
{
    IDxuiControl  * content = m_panes.GetContent (pane);
    DxuiTabGroup  * group   = nullptr;



    if (content == nullptr)
    {
        return nullptr;
    }

    for (const std::unique_ptr<DxuiTabGroup> & candidate : m_groups)
    {
        group = (group == nullptr && candidate->IndexOf (content) >= 0) ? candidate.get() : group;
    }

    return group;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::GetPaneTabRect
//
//  The pane's tab in its group's strip, or the whole group when the pane is
//  alone in it; empty when the pane is not docked.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGroups::GetPaneTabRect (const std::wstring & pane) const
{
    RECT  rect = {};



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        for (int i = 0; group->IsVisible() && i < (int) group->GetTabCount(); i++)
        {
            if (m_panes.GetPaneOf (group->GetContent (i)) == pane)
            {
                rect = (group->GetTabCount() > 1) ? group->GetTabRect (i) : group->GetBounds();
            }
        }
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::GetCarriedTabRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGroups::GetCarriedTabRect (const std::wstring & pane) const
{
    DxuiTabGroup  * group = pane.empty() ? nullptr : FindGroupOf (pane);



    if (group == nullptr)
    {
        return RECT {};
    }

    return group->GetTabRect (group->IndexOf (m_panes.GetContent (pane)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::TryGetPaneGroupRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGroups::TryGetPaneGroupRect (const std::wstring & pane, RECT & rect) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        if (!group->IsVisible())
        {
            continue;
        }

        for (int i = 0; i < (int) group->GetTabCount(); i++)
        {
            if (m_panes.GetPaneOf (group->GetContent (i)) == pane)
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
//  DxuiDockGroups::TryGetTitleExtraRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGroups::TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    rect = {};

    if (content == nullptr)
    {
        return false;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
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
//  DxuiDockGroups::GetTabAt
//
//  Over a tab's pin or close button the tab gives no tip of its own; the
//  button's is GetTitleButtonTipAt's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockGroups::GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        int  index = group->IsVisible() ? group->HitTestTab (pointDip) : -1;

        if (index >= 0)
        {
            std::wstring               pane     = m_panes.GetPaneOf (group->GetContent (index));
            DxuiTabGroup::TitleButton  button   = DxuiTabGroup::TitleButton::Close;
            int                        onButton = -1;
            RECT                       rect     = {};
            bool                       isButton = group->TryGetTabButtonAt (pointDip, button, onButton, rect);

            tab = group->GetTabRect (index);
            tip = !isButton ? m_panes.GetTip (pane) : std::wstring();
            return pane;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::GetTitleButtonTipAt
//
//  The pin's tip gives what pressing it does in OnTitleButton: docks the
//  pane or hides it against its edge. A tab's pin and close button are its
//  pane's title bar buttons.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockGroups::GetTitleButtonTipAt (POINT pointDip, RECT & button, bool isFloatingSite, const DxuiPaneLayout & layout) const
{
    static constexpr DxuiTabGroup::TitleButton  kButtons[] = { DxuiTabGroup::TitleButton::Menu, DxuiTabGroup::TitleButton::Pin,
                                                               DxuiTabGroup::TitleButton::Close };
    auto                                        pinTip     = [isFloatingSite, &layout] (const std::wstring & pane)
    {
        return (isFloatingSite || layout.IsAutoHidden (pane)) ? L"Dock" : L"Auto hide";
    };



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        std::wstring               pane;
        DxuiTabGroup::TitleButton  onTab = DxuiTabGroup::TitleButton::Close;
        int                        index = -1;

        if (group->IsVisible() && group->TryGetTabButtonAt (pointDip, onTab, index, button))
        {
            pane = m_panes.GetPaneOf (group->GetContent (index));
            return (onTab == DxuiTabGroup::TitleButton::Pin) ? pinTip (pane) : L"Close";
        }

        if (!group->IsVisible() || !Contains (group->GetTitleRect(), pointDip))
        {
            continue;
        }

        pane = m_panes.GetPaneOf (group->GetActiveContent());

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
//  DxuiDockGroups::GetTabNeighbor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockGroups::GetTabNeighbor (const std::wstring & pane) const
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        int  index = (content != nullptr && group->IsVisible()) ? group->IndexOf (content) : -1;



        if (index >= 0 && group->GetTabCount() > 1)
        {
            return m_panes.GetPaneOf (group->GetContent ((index + 1 < (int) group->GetTabCount()) ? index + 1 : index - 1));
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::ClassifyHit
//
//  In a floating window, a group's title bar off its buttons is the
//  window's caption, so the system moves the window by it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiDockGroups::ClassifyHit (POINT clientDip, bool isFloatingSite) const
{
    static constexpr DxuiTabGroup::TitleButton  kButtons[] = { DxuiTabGroup::TitleButton::Menu, DxuiTabGroup::TitleButton::Pin,
                                                               DxuiTabGroup::TitleButton::Close };



    if (!isFloatingSite)
    {
        return DxuiHitTestKind::Client;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetCount
//
//  Tab groups are reused by position, so after a small change the controls
//  that did not move are left in their groups.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetCount (size_t count, const WireFn & wire)
{
    while (m_groups.size() < count)
    {
        m_groups.push_back (std::make_unique<DxuiTabGroup>());
        wire (m_groups.back().get());
    }

    while (m_groups.size() > count)
    {
        m_retired.push_back (std::move (m_groups.back()));
        m_groups.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetTitle (IDxuiControl * content, const std::wstring & title)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetTitle (content, title);
    }

    m_slid.SetTitle (content, title);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetIndicator
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetIndicator (IDxuiControl * content, bool on)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetIndicator (content, on);
    }

    m_slid.SetIndicator (content, on);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetLeadingMark
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetLeadingMark (IDxuiControl * content, const DxuiTabGroup::LeadingMark & mark)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetLeadingMark (content, mark);
    }

    m_slid.SetLeadingMark (content, mark);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetTitleExtra
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetTitleExtra (IDxuiControl * content, int widthDip)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetTitleExtra (content, widthDip);
    }

    m_slid.SetTitleExtra (content, widthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::CancelPress
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::CancelPress()
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->CancelPress();
    }

    m_slid.CancelPress();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::IsTitlePressed
//
//  A title bar press is armed in some group, which a move past the drag
//  distance turns into a drag.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGroups::IsTitlePressed() const
{
    bool  isTitlePressed = m_slid.IsTitlePressed();



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        isTitlePressed = isTitlePressed || group->IsTitlePressed();
    }

    return isTitlePressed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetNewTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetNewTab (const DxuiTabGroup::NewTabShownFn & shown, const DxuiTabGroup::NewTabFn & add)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetNewTab (shown, add);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::SetFocusedLook
//
//  The group with `content` among its tabs has the focused look, and no
//  other docked group does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::SetFocusedLook (IDxuiControl * content)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetFocusedLook (content != nullptr && group->IndexOf (content) >= 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::PaintFrames
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGroups::PaintFrames (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->PaintFrame (painter, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups::GetActivePanes
//
//  The active pane of each group, in the groups' order; empty for a group with
//  no pane of the site's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DxuiDockGroups::GetActivePanes() const
{
    std::vector<std::wstring>  panes;



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        panes.push_back (m_panes.GetPaneOf (group->GetActiveContent()));
    }

    return panes;
}