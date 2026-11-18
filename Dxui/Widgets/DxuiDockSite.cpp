#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiPaneFrame.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiTip.h"
#include "Core/DxuiPaneMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::DxuiDockSite
//
//  A slid-out pane is a tool window of one pane, whatever it was docked as,
//  and keeps its group for as long as the site lives.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockSite::DxuiDockSite()
{
    m_groups.GetSlid().SetKind    (DxuiTabGroup::Kind::ToolWindow);
    m_groups.GetSlid().SetVisible (false);
    WireGroup (&m_groups.GetSlid());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetNewTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetNewTab (DxuiTabGroup::NewTabShownFn shown, DxuiTabGroup::NewTabFn add)
{
    m_newTabShown = std::move (shown);
    m_newTab      = std::move (add);

    m_groups.SetNewTab (m_newTabShown, m_newTab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::AddPane
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::AddPane (const std::wstring & pane, const std::wstring & title, IDxuiControl * content)
{
    m_panes.Add (pane, title, content);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTitle (const std::wstring & pane, const std::wstring & title)
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    if (!m_panes.SetTitle (pane, title))
    {
        return;
    }

    m_groups.SetTitle (content, title);

    //  A hidden pane's edge tab is as long as its title.
    if (m_layout.IsAutoHidden (pane))
    {
        Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetPaneLayout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetPaneLayout (const DxuiPaneLayout & layout)
{
    m_layout = layout;
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::Relayout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Relayout()
{
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler = scaler;
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::Arrange
//
//  One tab group per group the layout shows, filled with its panes in order
//  and showing its active one; every docked pane not in a shown group is
//  hidden.
//  Tab groups are reused by position, so arranging again after a small change
//  keeps the controls that did not move.
//
//  With a gap or margin set, the layout divides the area inside the margin,
//  and each group then gives up its share of the gap at every split, so the
//  splits themselves stay where the layout puts them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Arrange()
{
    std::vector<DxuiPaneLayout::GroupRect>    groups;
    std::unordered_set<const IDxuiControl *>  placed;
    long                                      margin  = m_scaler.ToPx (m_gaps.GetMarginDip());
    DxuiPaneLayout::MinSizeFn                 minSize = GetMinSizeWithGap();



    if (m_boundsDip.right <= m_boundsDip.left || m_boundsDip.bottom <= m_boundsDip.top)
    {
        return;
    }

    m_dockedArea = GetDockedArea();
    m_paneArea   = RECT { m_dockedArea.left + margin, m_dockedArea.top + margin, m_dockedArea.right - margin, m_dockedArea.bottom - margin };

    m_paneArea.right  = std::max (m_paneArea.right,  m_paneArea.left);
    m_paneArea.bottom = std::max (m_paneArea.bottom, m_paneArea.top);

    //  A slid-out pane lies over the docked panes, which keep their places.
    m_edges.Arrange (m_dockedArea, m_paneArea);

    groups      = m_layout.Arrange (m_paneArea, m_shown, minSize);
    m_gaps.SetSplits (m_layout.ArrangeSplits (m_paneArea, m_shown, minSize));
    m_arranging = true;

    m_groups.SetCount (groups.size(), [this] (DxuiTabGroup * group) { WireGroup (group); });

    for (size_t i = 0; i < groups.size(); i++)
    {
        ArrangeGroup (m_groups.Get (i), groups[i], placed);
    }

    //  A floating pane's controls are in another window, which shows them.
    for (const auto & pane : m_panes.GetAll())
    {
        if (pane.second.content != nullptr && !placed.contains (pane.second.content) && !m_layout.IsFloating (pane.first))
        {
            pane.second.content->SetVisible (false);
        }
    }

    ArrangeSlid();

    m_arranging = false;

    if (m_slidNotified != m_edges.GetSlidPane())
    {
        m_slidNotified = m_edges.GetSlidPane();

        if (m_onSlid)
        {
            m_onSlid (m_edges.GetSlidPane());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ArrangeGroup
//
//  One tab group filled with an arranged group's panes, its tabs' marks
//  set, and laid out in the group's share of the pane area. Each pane
//  placed is added to `placed`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ArrangeGroup (
    DxuiTabGroup                              * group,
    const DxuiPaneLayout::GroupRect           & arranged,
    std::unordered_set<const IDxuiControl *>  & placed)
{
    long                                                  gap      = m_scaler.ToPx (m_gaps.GetGapDip());
    int                                                   active   = 0;
    bool                                                  document = !m_isDocument;
    bool                                                  focused  = false;
    bool                                                  carried  = false;
    RECT                                                  rect     = {};
    std::vector<std::pair<std::wstring, IDxuiControl *>>  tabs;



    //  Every tab at once, so a press on a tab survives the refill: an
    //  arrangement runs when a pressed tab is activated, and the press
    //  may go on to drag it.
    for (const std::wstring & pane : arranged.panes)
    {
        const DxuiDockPane  * found = m_panes.Find (pane);

        if (found == nullptr || found->content == nullptr)
        {
            continue;
        }

        active    = (pane == arranged.active) ? (int) tabs.size() : active;
        document  = document || m_isDocument (pane);
        focused   = focused || pane == m_focusedPane;
        carried   = carried || (!m_carriedPane.empty() && pane == m_carriedPane);

        tabs.push_back ({ found->title, found->content });
        placed.insert (found->content);
    }

    group->ReplaceTabs (tabs, active);

    for (size_t t = 0; t < tabs.size(); t++)
    {
        std::wstring          name  = m_panes.GetPaneOf (tabs[t].second);
        const DxuiDockPane  & entry = *m_panes.Find (name);

        //  The tab in front has no mark, which is for a change out of
        //  sight; a pane moved to another group takes its mark with it.
        m_panes.SetIndicator (name, entry.indicator && (int) t != active);

        group->SetLeadingMark (tabs[t].second, entry.leadMark);
        group->SetTabTip      (tabs[t].second, entry.tip);
        group->SetTitleExtra  (tabs[t].second, entry.titleExtraDip);
        group->SetIndicator   (tabs[t].second, entry.indicator);
    }

    //  A floating window's title bar is its pane's, so every group there
    //  is a tool window.
    document = document && m_onDock == nullptr;

    rect = GetInsetForGap (arranged.rect, m_paneArea, gap);

    group->SetKind          (document ? DxuiTabGroup::Kind::Document : DxuiTabGroup::Kind::ToolWindow);
    group->SetFocusedLook   (focused);
    group->SetStripForced   (carried);
    group->SetActive        (active);
    group->SetWindowCorners ((m_cornerDip > 0) ? DxuiPaneFrame::GetWindowCorners (rect, m_boundsDip) : 0, m_cornerDip);
    group->Layout           (rect, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ArrangeSlid
//
//  The slid-out pane's group, refilled with that pane alone over its slid
//  rect, or hidden while no pane is slid out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ArrangeSlid()
{
    DxuiTabGroup        & slidGroup = m_groups.GetSlid();
    const DxuiDockPane  * slid      = m_edges.GetSlidPane().empty() ? nullptr : m_panes.Find (m_edges.GetSlidPane());
    bool                  isShown   = slid != nullptr && slid->content != nullptr;



    while (slidGroup.GetTabCount() > 0)
    {
        slidGroup.RemoveTab (slidGroup.GetContent (0));
    }

    slidGroup.SetVisible (isShown);

    if (isShown)
    {
        slidGroup.AddTab         (slid->title, slid->content);
        slidGroup.SetFocusedLook (m_edges.GetSlidPane() == m_focusedPane);
        slidGroup.Layout         (m_edges.GetSlidRect(), m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::WireGroup
//
//  A group's handlers find their pane when they run, since the site refills
//  its groups on every arrangement. A drag one of them starts is the
//  pointer's, which HasPointerDrag reports.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::WireGroup (DxuiTabGroup * group)
{
    group->SetOnActivated ([this, group] (int index)
    {
        if (!m_arranging && m_layout.Activate (GetPaneOf (group->GetContent (index))))
        {
            NotifyChanged();
        }
    });

    group->SetOnTitlePress ([this, group] (int index)
    {
        std::wstring  pane = GetPaneOf (group->GetContent (index));



        SetFocusedPane (pane);

        if (m_onFocusRequested)
        {
            m_onFocusRequested (pane);
        }
    });

    group->SetOnDragStart ([this, group] (int index, POINT pointDip)
    {
        std::wstring  pane = GetPaneOf (group->GetContent (index));



        if (!TearOff (group, pane, pointDip, true))
        {
            BeginDrag (pane);
            m_drag.SetPointerDrag (IsDragging());
        }
    });

    //  A tab dragged along its strip moves within the group, among every pane
    //  in the group, visible or not: ahead of the tab now after it, or after
    //  the tab now before it.
    group->SetOnMove ([this, group] (int from, int to)
    {
        std::wstring               pane  = GetPaneOf (group->GetContent (to));
        std::wstring               after = (to + 1 < (int) group->GetTabCount()) ? GetPaneOf (group->GetContent (to + 1)) : std::wstring();
        std::wstring               ahead = (to > 0) ? GetPaneOf (group->GetContent (to - 1)) : std::wstring();
        std::vector<std::wstring>  all   = m_layout.GetGroup (pane);
        std::wstring               other = !after.empty() ? after : ahead;
        int                        index = 0;



        (void) from;

        if (other.empty())
        {
            return;
        }

        index = (int) (std::find (all.begin(), all.end(), other) - all.begin()) + (after.empty() ? 1 : 0);

        if (m_layout.TabWithAt (pane, other, index))
        {
            NotifyChanged();
        }
    });

    group->SetOnTitleDragStart ([this, group] (int index, POINT pointDip)
    {
        std::vector<std::wstring>  panes;



        for (int i = 0; i < (int) group->GetTabCount(); i++)
        {
            panes.push_back (GetPaneOf (group->GetContent (i)));
        }

        if (panes.size() == 1 && TearOff (group, panes.front(), pointDip, false))
        {
            return;
        }

        //  FR-205: a group's title bar tears off the whole group.
        if (panes.size() > 1 && TearOffGroup (group, panes, GetPaneOf (group->GetContent (index)), pointDip))
        {
            return;
        }

        BeginGroupDrag (panes, GetPaneOf (group->GetContent (index)));
        m_drag.SetPointerDrag (IsDragging());
    });

    group->SetOnTitleButton ([this, group] (DxuiTabGroup::TitleButton button, int index, POINT pointDip)
    {
        OnTitleButton (button, GetPaneOf (group->GetContent (index)), pointDip);
    });

    group->SetOnCloseTab (m_onClosePane ? DxuiTabGroup::CloseTabFn ([this, group] (int index)
    {
        OnTitleButton (DxuiTabGroup::TitleButton::Close, GetPaneOf (group->GetContent (index)), POINT {});
    }) : nullptr);

    group->SetCanClose ([this, group] (int index)
    {
        return m_onClosePane && (!m_canClosePane || m_canClosePane (GetPaneOf (group->GetContent (index))));
    });

    group->SetNewTab (m_newTabShown, m_newTab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnTitleButton
//
//  The menu is the application's to show; the pin hides the pane against the
//  edge its Dock To menu would; a close is the application's to carry out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::OnTitleButton (DxuiTabGroup::TitleButton button, const std::wstring & pane, POINT pointDip)
{
    if (pane.empty())
    {
        return;
    }

    switch (button)
    {
    case DxuiTabGroup::TitleButton::Menu:
        if (m_onPaneMenu)
        {
            m_onPaneMenu (pane, pointDip);
        }

        break;

    case DxuiTabGroup::TitleButton::Pin:
        //  In a floating window the pin docks the pane back into the window
        //  it came from.
        if (m_onDock)
        {
            m_onDock (pane);
            break;
        }

        //  On a slid-out pane the pin docks it back where it came from.
        if (m_layout.IsAutoHidden (pane))
        {
            (void) UnpinPane (pane);
            break;
        }

        for (const MenuItem & item : GetDockToMenu (pane))
        {
            if (item.label == DxuiDockMenus::kAutoHideLabel)
            {
                (void) item.action();
                break;
            }
        }

        break;

    case DxuiTabGroup::TitleButton::Close:
        if (m_onClosePane && (!m_canClosePane || m_canClosePane (pane)))
        {
            m_onClosePane (pane);
        }

        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PinPane
//
//  Hides the pane against an edge, then shrinks a rect into its edge tab
//  from the pane's tab, or from its group when it is alone there.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::PinPane (const std::wstring & pane, DxuiDockSide edge)
{
    RECT  from    = GetPaneTabRect (pane);
    bool  changed = false;



    changed = m_layout.AutoHide (pane, edge);

    if (changed)
    {
        Arrange();
        StartMorph (from, GetEdgeTabRect (pane));
        NotifyChanged();
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::UnpinPane
//
//  Docks a hidden or floating pane back where it came from, as its group's
//  active tab. A hidden one then has a rect move from its edge tab, or from
//  the slid-out pane while it is out, to the tab it takes, or to its group
//  when it is alone there. The pane is then focused, as a press on its tab
//  focuses it, and its group is drawn with the focus outline.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::UnpinPane (const std::wstring & pane)
{
    bool  wasHidden = m_layout.IsAutoHidden (pane);
    RECT  from      = !wasHidden ? RECT {} : (pane == m_edges.GetSlidPane()) ? m_edges.GetSlidRect() : GetEdgeTabRect (pane);
    bool  changed   = false;



    changed = m_layout.DockBack (pane);

    if (changed)
    {
        Arrange();
        StartMorph (from, GetPaneTabRect (pane));
        NotifyChanged();
    }

    if (changed && wasHidden)
    {
        SetFocusedPane (pane);

        if (m_onFocusRequested)
        {
            m_onFocusRequested (pane);
        }
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetOnClosePane
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetOnClosePane (PaneFn fn, PaneTestFn canClose)
{
    m_onClosePane  = std::move (fn);
    m_canClosePane = std::move (canClose);

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups.GetAll())
    {
        WireGroup (group.get());
    }

    WireGroup (&m_groups.GetSlid());
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetFloating
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetFloating (PaneFn dock)
{
    m_onDock = std::move (dock);
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetWindowCornerDip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetWindowCornerDip (int radiusDip)
{
    m_cornerDip = radiusDip;
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetFocusedPane
//
//  Only the looks change, so nothing is laid out again, unless the focus
//  leaves a slid-out pane for another: as in Visual Studio, that slides it
//  back. A pane slid out while another held the focus stays out until the
//  focus moves.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetFocusedPane (const std::wstring & pane)
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    if (pane == m_focusedPane)
    {
        return;
    }

    m_focusedPane = pane;

    m_groups.SetFocusedLook (content);

    if (!m_edges.GetSlidPane().empty() && m_edges.GetSlidPane() != pane)
    {
        SlideIn();
        return;
    }

    m_groups.GetSlid().SetFocusedLook (!m_edges.GetSlidPane().empty() && m_edges.GetSlidPane() == pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SlideOut
//
//  Showing a pane clears its indicator: whatever changed is now in view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SlideOut (const std::wstring & pane)
{
    if (!m_layout.IsAutoHidden (pane) || m_panes.Find (pane) == nullptr)
    {
        return;
    }

    m_panes.SetIndicator (pane, false);
    m_edges.SetSlidPane (pane);
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SlideIn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SlideIn()
{
    if (m_edges.GetSlidPane().empty())
    {
        return;
    }

    m_edges.ClearSlidPane();
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintSlidUnder
//
//  The slid-out pane's background, so nothing of the panes it covers shows
//  where its controls leave gaps.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintSlidUnder (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    if (!m_groups.GetSlid().IsVisible())
    {
        return;
    }

    painter.FillRect ((float) m_edges.GetSlidRect().left, (float) m_edges.GetSlidRect().top, (float) (m_edges.GetSlidRect().right - m_edges.GetSlidRect().left),
                      (float) (m_edges.GetSlidRect().bottom - m_edges.GetSlidRect().top), theme.ContentBackground());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintSlidOver
//
//  Its title bar and its frame, which follows its focused look as every
//  other group's does. The frame's corner pieces round off the square
//  background PaintSlidUnder laid.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintSlidOver (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (!m_groups.GetSlid().IsVisible())
    {
        return;
    }

    m_groups.GetSlid().Paint      (painter, text, theme);
    m_groups.GetSlid().PaintFrame (painter, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::NotifyChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::NotifyChanged()
{
    if (m_onChanged)
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ActivatePane
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ActivatePane (const std::wstring & pane)
{
    if (m_layout.Activate (pane))
    {
        Arrange();
        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetIndicator
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetIndicator (const std::wstring & pane, bool on)
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    if (!m_panes.SetIndicator (pane, on))
    {
        return;
    }

    m_groups.SetIndicator (content, on);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetLeadingMark
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetLeadingMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark)
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    if (!m_panes.SetLeadingMark (pane, mark))
    {
        return;
    }

    m_groups.SetLeadingMark (content, mark);

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetTabTip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTabTip (const std::wstring & pane, const std::wstring & tip)
{
    (void) m_panes.SetTabTip (pane, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetTitleExtra
//
//  Kept with the pane as it moves between groups, and given to the group
//  that holds it now, so the room shows without a new arrangement.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTitleExtra (const std::wstring & pane, int widthDip)
{
    IDxuiControl  * content = m_panes.GetContent (pane);



    if (!m_panes.SetTitleExtra (pane, widthDip))
    {
        return;
    }

    m_groups.SetTitleExtra (content, widthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::IsHitAt
//
//  The site lies over the whole of its panes, but only its chrome is under
//  the pointer: the title bars, the tab strips, the edges' tabs and the
//  sashes. A pane slid out from an edge lies over the panes under it, so all
//  of it is the site's but for the slid pane's controls, which the window's
//  top layer finds first.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::IsHitAt (POINT pointPx) const
{
    if (!IsVisible())
    {
        return false;
    }

    if (!m_edges.GetSlidPane().empty() && Contains (m_edges.GetSlidRect(), pointPx))
    {
        return true;
    }

    return HitTestEdgeTab (pointPx) >= 0 || HitTestSash (pointPx) >= 0 || m_groups.IsChromeAt (pointPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TryGetTip
//
//  A tab's tip, or what the title-bar button under the point does.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::TryGetTip (POINT pointPx, DxuiTip & tip) const
{
    RECT          rect = {};
    std::wstring  text;



    if (!IsVisible())
    {
        return false;
    }

    if (GetTabAt (pointPx, rect, text).empty() || text.empty())
    {
        text = GetTitleButtonTipAt (pointPx, rect);
    }

    if (text.empty())
    {
        return IDxuiControl::TryGetTip (pointPx, tip);
    }

    tip        = DxuiTip();
    tip.text   = text;
    tip.anchor = rect;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetPaneAt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetPaneAt (POINT pointDip) const
{
    int  tab = HitTestEdgeTab (pointDip);



    if (!m_edges.GetSlidPane().empty() && Contains (m_edges.GetSlidRect(), pointDip))
    {
        return m_edges.GetSlidPane();
    }

    if (tab >= 0)
    {
        return m_edges.GetTabPane (tab);
    }

    return m_groups.GetGroupPaneAt (pointDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::MovePaneByArrow
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::MovePaneByArrow (const std::wstring & pane, DxuiDockSide direction)
{
    bool  moved = m_layout.MoveByArrow (pane, direction, GetDockedArea(), m_shown, m_minSize);



    if (moved)
    {
        Arrange();
        NotifyChanged();
    }

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetCarriedPane
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetCarriedPane (const std::wstring & pane)
{
    if (pane == m_carriedPane)
    {
        return;
    }

    m_carriedPane = pane;
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::IsDocumentGroup
//
//  As Arrange decides a group's kind: with no predicate every group is a
//  document group, and a floating window holds none.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::IsDocumentGroup (const std::vector<std::wstring> & panes) const
{
    bool  document = !m_isDocument;



    for (const std::wstring & pane : panes)
    {
        document = document || m_isDocument (pane);
    }

    return document && m_onDock == nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TearOff
//
//  The group forgets its press, since the floating window the application
//  makes takes the button's release; a slid-out pane slides back first. In a
//  floating window a tab tears off only from a group of several: a lone
//  pane is the window, and dragging it moves the window.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::TearOff (DxuiTabGroup * group, const std::wstring & pane, POINT pointDip, bool fromTab)
{
    if (!m_onTearOff || pane.empty() || (IsFloatingSite() && m_layout.GetGroup (pane).size() < 2))
    {
        return false;
    }

    m_tearGrab    = group->GetGrabOffset (fromTab);
    m_tearFromTab = fromTab;

    group->CancelPress();

    if (pane == m_edges.GetSlidPane())
    {
        m_edges.ClearSlidPane();
        Arrange();
    }

    m_onTearOff (pane, pointDip);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TearOffGroup
//
//  As TearOff, for every pane of a group pulled by its title bar.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::TearOffGroup (DxuiTabGroup * group, const std::vector<std::wstring> & panes, const std::wstring & active, POINT pointDip)
{
    if (!m_onTearOffGroup || IsFloatingSite() || panes.empty() || active.empty())
    {
        return false;
    }

    m_tearGrab    = group->GetGrabOffset (false);
    m_tearFromTab = false;

    group->CancelPress();

    if (std::ranges::find (panes, m_edges.GetSlidPane()) != panes.end())
    {
        m_edges.ClearSlidPane();
        Arrange();
    }

    m_onTearOffGroup (panes, active, pointDip);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::BeginDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::BeginDrag (const std::wstring & pane)
{
    BeginGroupDrag (std::vector<std::wstring> { pane }, pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::BeginGroupDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::BeginGroupDrag (const std::vector<std::wstring> & panes, const std::wstring & active)
{
    m_drag.Begin (panes, active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::CancelDrag
//
//  The drop targets go, and so does the gap a hovered strip opened for the
//  tab; the layout stays as the drag found it. The press that started the
//  drag is canceled in its group too, so the release that follows, or the
//  next move, is not taken for a press still under way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::CancelDrag()
{
    m_groups.CancelPress();
    m_drag.Cancel();

    //  A sash moved as it was dragged, so it stays where it is.
    if (m_sashDrag >= 0)
    {
        m_sashDrag = -1;
        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnDragKey
//
//  Escape cancels a drag the pointer started, as in Visual Studio, with the
//  button still down; the release that follows changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::OnDragKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down || ev.vk != VK_ESCAPE || !HasPointerDrag())
    {
        return false;
    }

    CancelDrag();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnDragMouseLost
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::OnDragMouseLost (bool isButtonDown)
{
    if (isButtonDown && HasPointerDrag())
    {
        CancelDrag();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::HasPointerDrag
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::HasPointerDrag() const
{
    return (IsDragging() && m_drag.IsPointerDrag()) || m_sashDrag >= 0 || m_groups.IsTitlePressed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetPaneGap
//
//  The gap between neighboring panes and the margin between the outer panes
//  and the docked area's edges, in DIP.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetPaneGap (int gapDip, int marginDip)
{
    m_gaps.Set (gapDip, marginDip);

    //  Before the site has bounds, its first layout takes these up.
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnMouse
//
//  A drag in progress and a sash being dragged take every event until the
//  button comes up. Otherwise a press on a sash starts a sash drag, a press
//  on a strip goes to its tab group, and anything else is left for the panes.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::OnMouse (const DxuiMouseEvent & ev)
{
    bool          handled = false;
    int           tab     = -1;
    std::wstring  edgePane;



    //  Whatever handler retired these has returned by now.
    m_groups.ClearRetired();

    if (IsDragging())
    {
        OnDragMouse (ev);
        return true;
    }

    if (m_sashDrag >= 0 && m_sashDrag < (int) m_gaps.GetSplits().size())
    {
        OnSashMouse (ev);
        return true;
    }

    //  An edge tab is a button: a press slides its pane out, and a press on
    //  the same tab again slides it back. A hover does nothing. A press
    //  anywhere else slides a slid-out pane back too.
    tab      = HitTestEdgeTab (ev.positionDip);
    edgePane = (tab >= 0) ? m_edges.GetTabPane (tab) : std::wstring();

    //  The tab under the pointer lights its bar.
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_edges.SetHoveredTab ((ev.kind == DxuiMouseEventKind::Move) ? edgePane : std::wstring());
    }

    if (tab >= 0 && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        if (edgePane == m_edges.GetSlidPane())
        {
            SlideIn();
        }
        else
        {
            SlideOut (edgePane);
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && !m_edges.GetSlidPane().empty() && !Contains (m_edges.GetSlidRect(), ev.positionDip))
    {
        SlideIn();
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && OnChromeWheel (ev))
    {
        return true;
    }

    //  The slid pane's title bar is the site's; its controls are the window's.
    if (ev.kind == DxuiMouseEventKind::Down && m_groups.GetSlid().IsVisible() && m_groups.GetSlid().IsChromeAt (ev.positionDip))
    {
        (void) m_groups.GetSlid().OnMouse (ev);
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && !Contains (m_edges.GetSlidRect(), ev.positionDip))
    {
        return OnChromePress (ev);
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up)
    {
        handled = OnGroupsMouse (ev);
    }

    return handled || IsDragging();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnDragMouse
//
//  A pane's drag tracks the pointer and drops where the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::OnDragMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Move)
    {
        m_drag.Track (ev.positionDip);
    }
    else if (ev.kind == DxuiMouseEventKind::Leave)
    {
        m_drag.ClearTarget();
    }
    else if (ev.kind == DxuiMouseEventKind::Up)
    {
        for (const std::unique_ptr<DxuiTabGroup> & group : m_groups.GetAll())
        {
            (void) group->OnMouse (ev);
        }

        //  The slid group, hidden as its title bar's drag began, still
        //  holds that press.
        (void) m_groups.GetSlid().OnMouse (ev);
        EndDrag (ev.positionDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnSashMouse
//
//  A sash's drag sets its split's ratio as the pointer moves, and the
//  change is reported once the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::OnSashMouse (const DxuiMouseEvent & ev)
{
    const DxuiPaneLayout::SplitRect  & split  = m_gaps.GetSplits()[(size_t) m_sashDrag];
    long                               total  = 0;
    long                               offset = 0;



    if (ev.kind == DxuiMouseEventKind::Move)
    {
        total  = split.horizontal ? (split.area.right - split.area.left) : (split.area.bottom - split.area.top);
        offset = split.horizontal ? (ev.positionDip.x - split.area.left)  : (ev.positionDip.y - split.area.top);

        if (total > 0 && m_layout.SetRatio (split.path, (float) offset / (float) total))
        {
            Arrange();
        }
    }
    else if (ev.kind == DxuiMouseEventKind::Up)
    {
        m_sashDrag = -1;
        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnChromeWheel
//
//  The wheel over a group's tabs scrolls them when they overflow. Over the
//  chrome it means nothing to the pane below, so it stops here either way.
//  False off every group's chrome.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::OnChromeWheel (const DxuiMouseEvent & ev)
{
    if (m_groups.GetSlid().IsVisible() && m_groups.GetSlid().IsChromeAt (ev.positionDip))
    {
        (void) m_groups.GetSlid().OnMouse (ev);
        return true;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups.GetAll())
    {
        if (group->IsVisible() && group->IsChromeAt (ev.positionDip) && !Contains (m_edges.GetSlidRect(), ev.positionDip))
        {
            (void) group->OnMouse (ev);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnChromePress
//
//  A left press off the slid-out pane: on a sash it starts the sash's drag,
//  and on a group's chrome it goes to the group. False elsewhere, which is
//  the panes'.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::OnChromePress (const DxuiMouseEvent & ev)
{
    m_sashDrag = HitTestSash (ev.positionDip);

    if (m_sashDrag >= 0)
    {
        return true;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups.GetAll())
    {
        if (group->IsChromeAt (ev.positionDip))
        {
            (void) group->OnMouse (ev);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::OnGroupsMouse
//
//  A move or release offered to every group, the slid-out pane's last.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::OnGroupsMouse (const DxuiMouseEvent & ev)
{
    std::vector<DxuiTabGroup *>  groups;
    bool                         handled = false;



    //  A button or tab released here can change the layout, which refills
    //  and retires groups, so the groups are taken before any of them acts.
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups.GetAll())
    {
        groups.push_back (group.get());
    }

    groups.push_back (&m_groups.GetSlid());

    for (DxuiTabGroup * group : groups)
    {
        handled = group->OnMouse (ev) || handled;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiDockSite::GetCursorForPoint (POINT clientPx) const
{
    int  sash = (m_sashDrag >= 0) ? m_sashDrag : HitTestSash (clientPx);



    if (sash < 0 || sash >= (int) m_gaps.GetSplits().size())
    {
        return nullptr;
    }

    return m_gaps.GetSplits()[(size_t) sash].horizontal ? IDC_SIZEWE : IDC_SIZENS;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SlideInForDrag
//
//  A slid-out pane slides back as its drag starts, so the drop zones beneath
//  it show; it stays hidden against its edge unless dropped.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SlideInForDrag (const std::wstring & pane)
{
    if (pane == m_edges.GetSlidPane())
    {
        m_edges.ClearSlidPane();
        Arrange();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::RequestDragFloat
//
//  A drop outside the site floats the pane there, through the application.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::RequestDragFloat (const std::wstring & pane, POINT pointDip)
{
    if (!Contains (m_boundsDip, pointDip) && m_onFloat)
    {
        m_onFloat (pane, pointDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::CommitChange
//
//  A menu item that changed the layout lays the site out again and reports
//  the change.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::CommitChange (bool changed)
{
    if (changed)
    {
        Arrange();
        NotifyChanged();
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::FloatPane
//
//  The pane floats through the application, centered on the site.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::FloatPane (const std::wstring & pane)
{
    RECT  area = m_boundsDip;



    m_onFloat (pane, POINT { (area.left + area.right) / 2, (area.top + area.bottom) / 2 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::FloatGroup
//
//  Every pane of a group floats through the application in one window,
//  centered on the site, with `active` its selected tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::FloatGroup (const std::vector<std::wstring> & panes, const std::wstring & active)
{
    RECT  area = m_boundsDip;



    m_onFloatGroup (panes, active, POINT { (area.left + area.right) / 2, (area.top + area.bottom) / 2 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetMenuState
//
//  The layout arranged in the docked area, as the site would arrange it,
//  and what the site's handlers allow for the pane.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockMenus::SiteState DxuiDockSite::GetMenuState (const std::wstring & pane) const
{
    DxuiDockMenus::SiteState  state;



    state.arranged       = m_layout.Arrange (GetDockedArea(), m_shown, m_minSize);
    state.dockedArea     = GetDockedArea();
    state.isDocument     = m_isDocument;
    state.canFloat       = m_onFloat != nullptr;
    state.canFloatGroup  = m_onFloatGroup != nullptr;
    state.canClose       = m_onClosePane && (!m_canClosePane || m_canClosePane (pane));
    state.isFloatingSite = IsFloatingSite();

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::Paint
//
//  The gaps and margin, the groups, and the edge strips. With no gap, two
//  neighbors' outlines meet at their split. A drag's marks are drawn in a
//  layer of their own, in PaintDragLayer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_gaps.Paint (painter, theme, m_dockedArea, m_paneArea, m_scaler);
    m_groups.Paint (painter, text, theme);

    m_edges.Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintAfterSiblings
//
//  Every group's frame -- its outline and corner caps. This pass draws over
//  every pane control whatever the child order, which a floating window
//  needs: its site is its first child, so the panes paint after the site's
//  own Paint. A drag's marks are not drawn here but in PaintDragLayer: a
//  pane's pictures, drawn after every fill of the page, would cover them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintAfterSiblings (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (text);

    m_groups.PaintFrames (painter, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetPaneArea
//
//  The area the panes are laid out in.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetPaneArea() const
{
    return m_gaps.IsSet() ? m_paneArea : GetDockedArea();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintDragLayer
//
//  Each mark of GetDragMarks, top to bottom: a fill, a dotted outline drawn
//  as its strips, a solid outline, or a guide's picture. The window flushes
//  this layer after the page, so even the shade's fills lie over the panes'
//  text and pictures. The guides' pictures go through the layer's text
//  pass, which draws after its fills, so they lie over the shade; the guide
//  the shade covers has the shade drawn into its picture. The marks are left
//  out while an overlay draws them; a pinned or unpinned pane's rect is not.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintDragLayer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme) const
{
    HRESULT                        hr      = S_OK;
    bool                           isShown = HasDragLayer();
    std::vector<DxuiDockDragMark>  marks;
    RECT                           morph   = GetMorphRect();



    BAIL_OUT_IF (!isShown, S_OK);

    if (!m_marksElsewhere)
    {
        marks = GetDragMarks (theme);
    }

    //  A pane being pinned or unpinned: the drop preview's shade, outlined in
    //  the focus accent.
    if (morph.right > morph.left && morph.bottom > morph.top)
    {
        marks.push_back ({ morph, theme.DockPreview(), 0 });
        marks.push_back ({ morph, theme.FocusAccent(), DxuiPaneMetrics::GetLinePx (m_scaler) });
    }

    DxuiDockDrag::PaintMarks (painter, text, marks);

Error:
    return;
}





