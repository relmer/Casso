#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





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
    m_slidGroup.SetKind    (DxuiTabGroup::Kind::ToolWindow);
    m_slidGroup.SetVisible (false);
    WireGroup (&m_slidGroup);
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

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetNewTab (m_newTabShown, m_newTab);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::AddPane
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::AddPane (const std::wstring & pane, const std::wstring & title, IDxuiControl * content)
{
    m_panes[pane] = Pane { title, content };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTitle (const std::wstring & pane, const std::wstring & title)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end())
    {
        return;
    }

    found->second.title = title;

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetTitle (found->second.content, title);
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
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Arrange()
{
    std::vector<DxuiPaneLayout::GroupRect>    groups;
    std::unordered_set<const IDxuiControl *>  placed;
    RECT                                      area = GetDockedArea();
    auto                                      slid = m_panes.end();



    if (m_boundsDip.right <= m_boundsDip.left || m_boundsDip.bottom <= m_boundsDip.top)
    {
        return;
    }

    //  A slid-out pane lies over the docked panes, which keep their places.
    ArrangeEdges (area);

    groups      = m_layout.Arrange (area, m_shown, m_minSize);
    m_splits    = m_layout.ArrangeSplits (area, m_shown, m_minSize);
    m_arranging = true;

    while (m_groups.size() < groups.size())
    {
        m_groups.push_back (std::make_unique<DxuiTabGroup>());
        WireGroup (m_groups.back().get());
    }

    while (m_groups.size() > groups.size())
    {
        m_retired.push_back (std::move (m_groups.back()));
        m_groups.pop_back();
    }

    for (size_t i = 0; i < groups.size(); i++)
    {
        DxuiTabGroup  * group    = m_groups[i].get();
        int             active   = 0;
        bool            document = !m_isDocument;
        bool            focused  = false;
        bool            carried  = false;

        while (group->GetTabCount() > 0)
        {
            group->RemoveTab (group->GetContent (0));
        }

        for (const std::wstring & pane : groups[i].panes)
        {
            auto  found = m_panes.find (pane);

            if (found == m_panes.end() || found->second.content == nullptr)
            {
                continue;
            }

            active    = (pane == groups[i].active) ? (int) group->GetTabCount() : active;
            document  = document || m_isDocument (pane);
            focused   = focused || pane == m_focusedPane;
            carried   = carried || (!m_carriedPane.empty() && pane == m_carriedPane);

            group->AddTab         (found->second.title, found->second.content);
            group->SetLeadingMark (found->second.content, found->second.leadMark);
            group->SetTabTip      (found->second.content, found->second.tip);
            placed.insert (found->second.content);
        }

        //  A floating window's title bar is its pane's, so every group there
        //  is a tool window.
        document = document && m_onDock == nullptr;

        group->SetKind        (document ? DxuiTabGroup::Kind::Document : DxuiTabGroup::Kind::ToolWindow);
        group->SetFocusedLook (focused);
        group->SetStripForced (carried);
        group->SetActive      (active);
        group->Layout         (groups[i].rect, m_scaler);
    }

    //  A floating pane's controls are in another window, which shows them.
    for (const auto & pane : m_panes)
    {
        if (pane.second.content != nullptr && !placed.contains (pane.second.content) && !m_layout.IsFloating (pane.first))
        {
            pane.second.content->SetVisible (false);
        }
    }

    slid = m_slidPane.empty() ? m_panes.end() : m_panes.find (m_slidPane);

    while (m_slidGroup.GetTabCount() > 0)
    {
        m_slidGroup.RemoveTab (m_slidGroup.GetContent (0));
    }

    m_slidGroup.SetVisible (slid != m_panes.end() && slid->second.content != nullptr);

    if (m_slidGroup.IsVisible())
    {
        m_slidGroup.AddTab         (slid->second.title, slid->second.content);
        m_slidGroup.SetFocusedLook (m_slidPane == m_focusedPane);
        m_slidGroup.Layout         (m_slidRect, m_scaler);
    }

    m_arranging = false;

    if (m_slidNotified != m_slidPane)
    {
        m_slidNotified = m_slidPane;

        if (m_onSlid)
        {
            m_onSlid (m_slidPane);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::WireGroup
//
//  A group's handlers find their pane when they run, since the site refills
//  its groups on every arrangement.
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

    group->SetOnDragStart ([this, group] (int index, POINT pointDip)
    {
        std::wstring  pane = GetPaneOf (group->GetContent (index));



        if (!TearOff (group, pane, pointDip, true))
        {
            BeginDrag (pane);
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

        BeginGroupDrag (panes, GetPaneOf (group->GetContent (index)));
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
            if (m_layout.DockBack (pane))
            {
                Arrange();
                NotifyChanged();
            }

            break;
        }

        for (const MenuItem & item : GetDockToMenu (pane))
        {
            if (item.label == kAutoHideLabel)
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
//  DxuiDockSite::SetOnClosePane
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetOnClosePane (PaneFn fn, PaneTestFn canClose)
{
    m_onClosePane  = std::move (fn);
    m_canClosePane = std::move (canClose);

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        WireGroup (group.get());
    }

    WireGroup (&m_slidGroup);
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
//  DxuiDockSite::ClassifyHit
//
//  In a floating window, a group's title bar off its buttons is the
//  window's caption, so the system moves the window by it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiDockSite::ClassifyHit (POINT clientDip) const
{
    static constexpr DxuiTabGroup::TitleButton  kButtons[] = { DxuiTabGroup::TitleButton::Menu, DxuiTabGroup::TitleButton::Pin,
                                                               DxuiTabGroup::TitleButton::Close };



    if (m_onDock == nullptr)
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

        return DxuiHitTestKind::Caption;
    }

    return DxuiHitTestKind::Client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetFocusedPane
//
//  Only the looks change, so nothing is laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetFocusedPane (const std::wstring & pane)
{
    auto  found = m_panes.find (pane);



    if (pane == m_focusedPane)
    {
        return;
    }

    m_focusedPane = pane;

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetFocusedLook (found != m_panes.end() && group->IndexOf (found->second.content) >= 0);
    }

    m_slidGroup.SetFocusedLook (!m_slidPane.empty() && m_slidPane == pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetDockedArea
//
//  The site less a strip along each edge that holds an auto-hidden pane.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetDockedArea() const
{
    RECT  area    = m_boundsDip;
    bool  used[4] = {};



    for (const DxuiPaneLayout::AutoHidden & hidden : m_layout.GetAutoHidden())
    {
        if (!m_shown || m_shown (hidden.pane))
        {
            used[(size_t) hidden.edge] = true;
        }
    }

    //  Every edge's strip is one tab high: along a side the tabs run down it,
    //  their titles turned to read top to bottom.
    area.left   += used[(size_t) DxuiDockSide::Left]   ? m_scaler.ToPx (DxuiTabGroup::kStripDip) : 0;
    area.top    += used[(size_t) DxuiDockSide::Top]    ? m_scaler.ToPx (DxuiTabGroup::kStripDip) : 0;
    area.right  -= used[(size_t) DxuiDockSide::Right]  ? m_scaler.ToPx (DxuiTabGroup::kStripDip) : 0;
    area.bottom -= used[(size_t) DxuiDockSide::Bottom] ? m_scaler.ToPx (DxuiTabGroup::kStripDip) : 0;

    area.right  = std::max (area.right,  area.left);
    area.bottom = std::max (area.bottom, area.top);

    return area;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ArrangeEdges
//
//  The edge tabs, in the order the panes were hidden, and the area a slid-out
//  pane covers: a third of the docked area from its edge, and never less
//  than it needs to be usable.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ArrangeEdges (const RECT & area)
{
    long          along[4]  = { area.top, area.left, area.top, area.left };
    long          minSlide  = m_scaler.ToPx (kSlideMinDip);
    long          width     = area.right - area.left;
    long          height    = area.bottom - area.top;
    bool          slidFound = false;
    DxuiDockSide  slidEdge  = DxuiDockSide::Left;



    m_edgeTabs.clear();

    for (const DxuiPaneLayout::AutoHidden & hidden : m_layout.GetAutoHidden())
    {
        EdgeTab  tab;
        long     length = m_scaler.ToPx (2 * DxuiTabGroup::kTabPadDip + (int) GetTitle (hidden.pane).size() * DxuiTabGroup::kCharDip);
        long   & at     = along[(size_t) hidden.edge];

        if (m_shown && !m_shown (hidden.pane))
        {
            continue;
        }

        tab.pane = hidden.pane;
        tab.edge = hidden.edge;

        switch (hidden.edge)
        {
        case DxuiDockSide::Left:   tab.rect = RECT { m_boundsDip.left, at, area.left,         at + length }; at += length;  break;
        case DxuiDockSide::Right:  tab.rect = RECT { area.right,       at, m_boundsDip.right, at + length }; at += length;  break;
        case DxuiDockSide::Top:    tab.rect = RECT { at, m_boundsDip.top, at + length, area.top };           at += length;  break;
        case DxuiDockSide::Bottom: tab.rect = RECT { at, area.bottom, at + length, m_boundsDip.bottom };     at += length;  break;
        }

        m_edgeTabs.push_back (tab);

        if (tab.pane == m_slidPane)
        {
            slidFound = true;
            slidEdge  = tab.edge;
        }
    }

    if (!slidFound)
    {
        m_slidPane.clear();
        m_slidRect = RECT {};
        return;
    }

    m_slidEdge = slidEdge;

    switch (slidEdge)
    {
    case DxuiDockSide::Left:   m_slidRect = RECT { area.left, area.top, area.left + std::min (width, std::max (minSlide, width / 3)), area.bottom };      break;
    case DxuiDockSide::Right:  m_slidRect = RECT { area.right - std::min (width, std::max (minSlide, width / 3)), area.top, area.right, area.bottom };    break;
    case DxuiDockSide::Top:    m_slidRect = RECT { area.left, area.top, area.right, area.top + std::min (height, std::max (minSlide, height / 3)) };      break;
    case DxuiDockSide::Bottom: m_slidRect = RECT { area.left, area.bottom - std::min (height, std::max (minSlide, height / 3)), area.right, area.bottom }; break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::HitTestEdgeTab
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockSite::HitTestEdgeTab (POINT pointDip) const
{
    for (size_t i = 0; i < m_edgeTabs.size(); i++)
    {
        if (Contains (m_edgeTabs[i].rect, pointDip))
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetEdgeTabRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetEdgeTabRect (const std::wstring & pane) const
{
    for (const EdgeTab & tab : m_edgeTabs)
    {
        if (tab.pane == pane)
        {
            return tab.rect;
        }
    }

    return RECT {};
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
    auto  found = m_panes.find (pane);



    if (!m_layout.IsAutoHidden (pane) || found == m_panes.end())
    {
        return;
    }

    found->second.indicator = false;
    m_slidPane              = pane;
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SlideIn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SlideIn()
{
    if (m_slidPane.empty())
    {
        return;
    }

    m_slidPane.clear();
    Arrange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintEdges
//
//  The edge strips and their tabs, the slid-out one's in the accent color,
//  and an outline around the slid-out pane.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintEdges (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           pad   = m_scaler.ToPxf ((float) DxuiTabGroup::kTabPadDip / 2);
    float           bar   = m_scaler.ToPxf ((float) kEdgeBarDip);
    float           inset = m_scaler.ToPxf ((float) kEdgeBarInsetDip);
    HRESULT         hr    = S_OK;



    //  As Visual Studio draws them: no box, only the title and a bar above
    //  it, gray at rest and in the focus accent while the pointer is over the
    //  tab or its pane is out.
    for (const EdgeTab & tab : m_edgeTabs)
    {
        const RECT  & r        = tab.rect;
        bool          lit      = (tab.pane == m_slidPane) || (tab.pane == m_hoverEdge);
        auto          found    = m_panes.find (tab.pane);
        bool          sideways = tab.edge == DxuiDockSide::Left || tab.edge == DxuiDockSide::Right;
        float         cx       = (float) (r.left + r.right) / 2;
        float         cy       = (float) (r.top + r.bottom) / 2;
        float         along    = (float) (sideways ? r.bottom - r.top : r.right - r.left);
        float         across   = (float) (sideways ? r.right - r.left : r.bottom - r.top);
        uint32_t      barArgb  = lit ? theme.FocusAccent() : theme.Border();

        painter.FillRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top), theme.Background());

        //  Against the window's outer edge, as Visual Studio draws it: left
        //  of a tab on the left edge, below one on the bottom edge.
        switch (tab.edge)
        {
        case DxuiDockSide::Left:
            painter.FillRect ((float) r.left, (float) r.top + inset, bar, along - 2 * inset, barArgb);
            break;

        case DxuiDockSide::Right:
            painter.FillRect ((float) r.right - bar, (float) r.top + inset, bar, along - 2 * inset, barArgb);
            break;

        case DxuiDockSide::Top:
            painter.FillRect ((float) r.left + inset, (float) r.top, along - 2 * inset, bar, barArgb);
            break;

        case DxuiDockSide::Bottom:
            painter.FillRect ((float) r.left + inset, (float) r.bottom - bar, along - 2 * inset, bar, barArgb);
            break;
        }

        //  A side tab is laid out level about its center and turned a
        //  quarter clockwise, so its title reads down the edge.
        if (sideways)
        {
            text.PushTextRotation (90.0f, cx, cy);
        }

        hr = text.DrawString (GetTitle (tab.pane).c_str(), cx - along / 2 + pad, cy - across / 2,
                              along - 2 * pad, across,
                              lit ? theme.Foreground() : theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (sideways)
        {
            text.PopTextRotation();
        }

        if (found != m_panes.end() && found->second.indicator)
        {
            painter.FillCircle ((float) r.right - pad, (float) r.top + pad, m_scaler.ToPxf (3.0f), theme.Accent());
        }
    }
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
    if (!m_slidGroup.IsVisible())
    {
        return;
    }

    painter.FillRect ((float) m_slidRect.left, (float) m_slidRect.top, (float) (m_slidRect.right - m_slidRect.left),
                      (float) (m_slidRect.bottom - m_slidRect.top), theme.ContentBackground());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintSlidOver
//
//  Its title bar and an outline in the accent color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintSlidOver (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  line = (float) std::max (1L, std::lround (m_scaler.ToPxf (1.0f)));



    if (!m_slidGroup.IsVisible())
    {
        return;
    }

    m_slidGroup.Paint (painter, text, theme);

    painter.OutlineRect ((float) m_slidRect.left, (float) m_slidRect.top, (float) (m_slidRect.right - m_slidRect.left),
                         (float) (m_slidRect.bottom - m_slidRect.top), line, theme.FocusAccent());
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
    auto  found = m_panes.find (pane);



    if (found == m_panes.end())
    {
        return;
    }

    found->second.indicator = on;

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetIndicator (found->second.content, on);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetLeadingMark
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetLeadingMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end() || found->second.leadMark == mark)
    {
        return;
    }

    found->second.leadMark = mark;

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetLeadingMark (found->second.content, mark);
    }

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetTabTip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTabTip (const std::wstring & pane, const std::wstring & tip)
{
    auto  found = m_panes.find (pane);



    if (found != m_panes.end())
    {
        found->second.tip = tip;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetTabAt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        int  index = group->IsVisible() ? group->HitTestTab (pointDip) : -1;

        if (index >= 0)
        {
            std::wstring  pane  = GetPaneOf (group->GetContent (index));
            auto          found = m_panes.find (pane);

            tab = group->GetTabRect (index);
            tip = (found != m_panes.end()) ? found->second.tip : std::wstring();
            return pane;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetPaneOf
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetPaneOf (const IDxuiControl * content) const
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
//  DxuiDockSite::GetPaneAt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetPaneAt (POINT pointDip) const
{
    int  tab = HitTestEdgeTab (pointDip);



    if (!m_slidPane.empty() && Contains (m_slidRect, pointDip))
    {
        return m_slidPane;
    }

    if (tab >= 0)
    {
        return m_edgeTabs[(size_t) tab].pane;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        if (Contains (group->GetBounds(), pointDip))
        {
            return GetPaneOf (group->GetActiveContent());
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetTitle (const std::wstring & pane) const
{
    auto  found = m_panes.find (pane);



    return (found != m_panes.end()) ? found->second.title : pane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetDockToMenu
//
//  Against each edge of the site, into each other shown group, and out into
//  a window of its own; or back, for a pane that is floating or hidden.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockSite::MenuItem> DxuiDockSite::GetDockToMenu (const std::wstring & pane)
{
    static constexpr DxuiDockSide  kSides[]    = { DxuiDockSide::Left, DxuiDockSide::Top, DxuiDockSide::Right, DxuiDockSide::Bottom };
    static const wchar_t * const   kNames[]    = { L"Dock left", L"Dock top", L"Dock right", L"Dock bottom" };
    std::vector<MenuItem>          items;
    std::vector<std::wstring>      mine        = m_layout.GetGroup (pane);
    auto                           commit      = [this] (bool changed)
    {
        if (changed)
        {
            Arrange();
            NotifyChanged();
        }

        return changed;
    };



    if (!m_layout.Contains (pane))
    {
        return items;
    }

    if (!m_layout.IsDocked (pane))
    {
        items.push_back ({ L"Dock", [this, pane, commit] { return commit (m_layout.DockBack (pane)); } });
        return items;
    }

    for (size_t i = 0; i < std::size (kSides); i++)
    {
        DxuiDockSide  side = kSides[i];

        items.push_back ({ kNames[i], [this, pane, side, commit] { return commit (m_layout.DockToEdge (pane, side)); } });
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        std::wstring  target = GetPaneOf (group->GetActiveContent());

        if (target.empty() || std::find (mine.begin(), mine.end(), target) != mine.end())
        {
            continue;
        }

        items.push_back ({ L"Tab with " + GetTitle (target),
                           [this, pane, target, commit] { return commit (m_layout.TabWith (pane, target)); } });
    }

    for (const DxuiPaneLayout::GroupRect & group : m_layout.Arrange (GetDockedArea(), m_shown, m_minSize))
    {
        RECT          area  = GetDockedArea();
        long          gaps[] = { group.rect.left - area.left, group.rect.top - area.top,
                                 area.right - group.rect.right, area.bottom - group.rect.bottom };
        size_t        nearest = 0;

        if (std::find (group.panes.begin(), group.panes.end(), pane) == group.panes.end())
        {
            continue;
        }

        //  Against the edge the pane's group lies nearest. Between a side and
        //  the top or bottom it touches, a group no wider than half the
        //  window is in a side bar and hides to its side; a wider one hides
        //  to the top or bottom. So the top of the right-hand column hides
        //  right, and the memory row under the code hides to the bottom.
        bool   sideBar = 2 * (group.rect.right - group.rect.left) <= (area.right - area.left);

        for (size_t i = 1; i < std::size (gaps); i++)
        {
            bool  sideways = (i % 2) == 0;

            nearest = (gaps[i] < gaps[nearest] || (gaps[i] == gaps[nearest] && sideways == sideBar)) ? i : nearest;
        }

        items.push_back ({ kAutoHideLabel, [this, pane, nearest, commit] { return commit (m_layout.AutoHide (pane, kSides[nearest])); } });
    }

    if (m_onFloat)
    {
        items.push_back ({ L"Float", [this, pane]
        {
            RECT  area = m_boundsDip;

            m_onFloat (pane, POINT { (area.left + area.right) / 2, (area.top + area.bottom) / 2 });
            return true;
        } });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetPaneMenu
//
//  Dock returns a floating or hidden pane to its place. Dock in tab group
//  tabs a tool window in with the documents. Auto hide pins the pane to the
//  edge its pin would. Move to new window floats the pane; All to new
//  window floats its group, which a floating window can hold only when the
//  group is the pane alone. Close is the application's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockSite::MenuItem> DxuiDockSite::GetPaneMenu (const std::wstring & pane, bool fromTab)
{
    std::vector<MenuItem>      items;
    std::vector<MenuItem>      dockTo     = GetDockToMenu (pane);
    std::vector<std::wstring>  mine       = m_layout.GetGroup (pane);
    bool                       docked     = m_layout.IsDocked (pane);
    bool                       canFloat   = docked && m_onFloat != nullptr && !IsFloatingSite();
    bool                       canClose   = m_onClosePane && (!m_canClosePane || m_canClosePane (pane));
    std::wstring               documents;
    MenuItem                   autoHide   = { kAutoHideLabel, nullptr, false };
    MenuItem                   dock       = { L"Dock", nullptr, false };
    auto                       commit     = [this] (bool changed)
    {
        if (changed)
        {
            Arrange();
            NotifyChanged();
        }

        return changed;
    };
    auto                       floatPane  = [this, pane]
    {
        RECT  area = m_boundsDip;

        m_onFloat (pane, POINT { (area.left + area.right) / 2, (area.top + area.bottom) / 2 });
        return true;
    };



    if (!m_layout.Contains (pane))
    {
        return items;
    }

    for (const MenuItem & item : dockTo)
    {
        autoHide = (item.label == kAutoHideLabel) ? item : autoHide;
        dock     = (!docked && item.label == L"Dock") ? item : dock;
    }

    //  The documents' group, when this pane is a tool window outside it.
    for (const DxuiPaneLayout::GroupRect & group : m_layout.Arrange (GetDockedArea(), m_shown, m_minSize))
    {
        bool  holdsDocument = m_isDocument && std::any_of (group.panes.begin(), group.panes.end(), m_isDocument);
        bool  holdsMe       = std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end();

        documents = (documents.empty() && holdsDocument && !holdsMe) ? group.active : documents;
    }

    documents = (m_isDocument && !m_isDocument (pane)) ? documents : std::wstring();

    items.push_back ({ L"Dock", dock.action, dock.action != nullptr });
    items.push_back ({ L"Dock in tab group", [this, pane, documents, commit] { return commit (m_layout.TabWith (pane, documents)); },
                       !documents.empty() && !IsFloatingSite() });
    items.push_back ({ kAutoHideLabel, autoHide.action, autoHide.action != nullptr });

    if (fromTab)
    {
        items.push_back ({ L"Move to new window", floatPane, canFloat });
    }

    items.push_back ({ L"All to new window", floatPane, canFloat && mine.size() == 1 });
    items.push_back ({ L"", nullptr, false });
    items.push_back ({ L"Close", [this, pane] { m_onClosePane (pane); return true; }, canClose, L"Shift+Esc" });

    return items;
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
//  DxuiDockSite::GetCarriedTabRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetCarriedTabRect() const
{
    DxuiTabGroup  * group = m_carriedPane.empty() ? nullptr : FindGroupOf (m_carriedPane);
    auto            found = m_panes.find (m_carriedPane);



    if (group == nullptr || found == m_panes.end())
    {
        return RECT {};
    }

    return group->GetTabRect (group->IndexOf (found->second.content));
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
//  DxuiDockSite::FindGroupOf
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabGroup * DxuiDockSite::FindGroupOf (const std::wstring & pane) const
{
    auto            found = m_panes.find (pane);
    DxuiTabGroup  * group = nullptr;



    if (found == m_panes.end() || found->second.content == nullptr)
    {
        return nullptr;
    }

    for (const std::unique_ptr<DxuiTabGroup> & candidate : m_groups)
    {
        group = (group == nullptr && candidate->IndexOf (found->second.content) >= 0) ? candidate.get() : group;
    }

    return group;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::AddDottedHalf
//
//  A split square's picture: a dotted outline of the half of the square on
//  its side, where the new tab group would go.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::AddDottedHalf (std::vector<DxuiDockDragMark> & marks, const DxuiDockDropZone & zone, uint32_t argb, int line) const
{
    RECT  r     = zone.target;
    long  inset = m_scaler.ToPx (6);
    long  dot   = std::max (1L, (long) line);
    long  midX  = (r.left + r.right) / 2;
    long  midY  = (r.top + r.bottom) / 2;



    r = RECT { r.left + inset, r.top + inset, r.right - inset, r.bottom - inset };

    switch (zone.side)
    {
    case DxuiDockSide::Left:   r.right  = midX; break;
    case DxuiDockSide::Right:  r.left   = midX; break;
    case DxuiDockSide::Top:    r.bottom = midY; break;
    case DxuiDockSide::Bottom: r.top    = midY; break;
    default:                                    break;
    }

    marks.push_back ({ r, argb, (int) dot, true });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetOutlineStrips
//
//  The filled rectangles that draw an outlined mark: four strips along the
//  inside of its rectangle, or for a dotted one a square dot every other
//  dot's width along each side.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> DxuiDockSite::GetOutlineStrips (const DxuiDockDragMark & mark)
{
    const RECT &       r = mark.rect;
    long               t = mark.outlinePx;
    std::vector<RECT>  strips;



    if (t <= 0)
    {
        return strips;
    }

    if (!mark.dotted)
    {
        strips.push_back (RECT { r.left,      r.top,        r.right,    r.top + t    });
        strips.push_back (RECT { r.left,      r.bottom - t, r.right,    r.bottom     });
        strips.push_back (RECT { r.left,      r.top + t,    r.left + t, r.bottom - t });
        strips.push_back (RECT { r.right - t, r.top + t,    r.right,    r.bottom - t });
        return strips;
    }

    for (long x = r.left; x + t <= r.right; x += 2 * t)
    {
        strips.push_back (RECT { x, r.top,        x + t, r.top + t });
        strips.push_back (RECT { x, r.bottom - t, x + t, r.bottom  });
    }

    for (long y = r.top + 2 * t; y + t <= r.bottom - t; y += 2 * t)
    {
        strips.push_back (RECT { r.left,      y, r.left + t, y + t });
        strips.push_back (RECT { r.right - t, y, r.right,    y + t });
    }

    return strips;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TearOff
//
//  The group forgets its press, since the floating window the application
//  makes takes the button's release; a slid-out pane slides back first.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::TearOff (DxuiTabGroup * group, const std::wstring & pane, POINT pointDip, bool fromTab)
{
    if (!m_onTearOff || IsFloatingSite() || pane.empty())
    {
        return false;
    }

    m_tearGrab    = group->GetGrabOffset (fromTab);
    m_tearFromTab = fromTab;

    group->CancelPress();

    if (pane == m_slidPane)
    {
        m_slidPane.clear();
        Arrange();
    }

    m_onTearOff (pane, pointDip);
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
//  A group dragged whole offers no zone on itself, since every drop there
//  would put it back where it is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::BeginGroupDrag (const std::vector<std::wstring> & panes, const std::wstring & active)
{
    auto  dragged = [&panes] (const std::wstring & pane)
    {
        return std::find (panes.begin(), panes.end(), pane) != panes.end();
    };



    if (active.empty() || !dragged (active))
    {
        return;
    }

    //  A slid-out pane slides back as its drag starts, so the drop zones
    //  beneath it show; it stays hidden against its edge unless dropped.
    if (active == m_slidPane)
    {
        m_slidPane.clear();
        Arrange();
    }

    m_dragPane  = active;
    m_dragPanes = panes;
    m_zones     = DxuiDockDropZones::Build (m_layout.Arrange (GetDockedArea(), m_shown, m_minSize), GetDockedArea(), active,
                                            [this] (const DxuiPaneLayout::GroupRect & group) { return IsDocumentGroup (group.panes); });
    m_hoverZone = -1;

    if (panes.size() > 1)
    {
        std::erase_if (m_zones, [&] (const DxuiDockDropZone & zone) { return dragged (zone.targetPane); });
    }

    //  A tab drop shades the pane's body, never its tabs, so the tabs stay in
    //  view where the dropped one will join them.
    for (DxuiDockDropZone & zone : m_zones)
    {
        DxuiTabGroup  * group = (zone.kind == DxuiDockDropZone::Kind::Tab) ? FindGroupOf (zone.targetPane) : nullptr;

        zone.preview = (group != nullptr) ? group->GetBodyRect() : zone.preview;
    }

    ClearStripTarget();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::UpdateStripTarget
//
//  The group whose tabs or title bar lie under the pointer takes the drop,
//  opening a gap where it would land. A group the drag would leave as it is
//  -- the dragged group itself, or a pane's own group of one -- takes none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::UpdateStripTarget (POINT pointDip)
{
    int  found = -1;
    int  index = -1;



    for (size_t i = 0; i < m_groups.size() && found < 0; i++)
    {
        DxuiTabGroup  * group  = m_groups[i].get();
        size_t          inside = 0;

        if (!group->IsVisible() || !group->IsChromeAt (pointDip))
        {
            continue;
        }

        for (int t = 0; t < (int) group->GetTabCount(); t++)
        {
            inside += std::find (m_dragPanes.begin(), m_dragPanes.end(), GetPaneOf (group->GetContent (t))) != m_dragPanes.end() ? 1 : 0;
        }

        if (inside == group->GetTabCount() || (inside > 0 && m_dragPanes.size() > 1))
        {
            break;
        }

        found = (int) i;
        index = group->GetInsertIndexAt (pointDip);
    }

    if (found == m_stripGroup && index == m_stripIndex)
    {
        return;
    }

    ClearStripTarget();
    m_stripGroup = found;
    m_stripIndex = index;

    if (found >= 0)
    {
        m_groups[(size_t) found]->SetInsertGap (index, m_scaler.ToPx (kInsertGapDip));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ClearStripTarget
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ClearStripTarget()
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetInsertGap (-1, 0);
    }

    m_stripGroup = -1;
    m_stripIndex = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::DropOnStrip
//
//  The dragged panes go into the group ahead of the tab at `index`, in their
//  order, and the pane dragged is the one shown.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::DropOnStrip (int group, int index)
{
    DxuiTabGroup               * target  = (group >= 0 && group < (int) m_groups.size()) ? m_groups[(size_t) group].get() : nullptr;
    std::wstring                 anchor;
    std::vector<std::wstring>    panes;
    bool                         changed = false;
    int                          at      = index;



    if (target == nullptr)
    {
        return false;
    }

    for (int t = 0; t < (int) target->GetTabCount() && anchor.empty(); t++)
    {
        std::wstring  pane = GetPaneOf (target->GetContent (t));

        if (std::find (m_dragPanes.begin(), m_dragPanes.end(), pane) == m_dragPanes.end())
        {
            anchor = pane;
        }
    }

    if (anchor.empty())
    {
        return false;
    }

    for (const std::wstring & pane : m_dragPanes)
    {
        changed = m_layout.TabWithAt (pane, anchor, at) || changed;
        panes   = m_layout.GetGroup (anchor);
        at      = (int) (std::find (panes.begin(), panes.end(), pane) - panes.begin()) + 1;
    }

    return m_layout.Activate (m_dragPane) || changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::DropOnZone
//
//  The zone's operation moves the first pane; the rest follow it into its
//  new group, in order.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::DropOnZone (const DxuiDockDropZone & zone)
{
    std::vector<std::wstring>  panes;
    int                        at      = 0;



    if (m_dragPanes.empty() || !DxuiDockDropZones::Apply (zone, m_layout, m_dragPanes.front()))
    {
        return false;
    }

    for (size_t i = 1; i < m_dragPanes.size(); i++)
    {
        panes   = m_layout.GetGroup (m_dragPanes.front());
        at      = (int) (std::find (panes.begin(), panes.end(), m_dragPanes[i - 1]) - panes.begin()) + 1;
        (void) m_layout.TabWithAt (m_dragPanes[i], m_dragPanes.front(), at);
    }

    (void) m_layout.Activate (m_dragPane);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::CancelDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::CancelDrag()
{
    m_dragPane.clear();
    m_zones.clear();
    m_hoverZone = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::EndDrag
//
//  A drop on a zone is that zone's operation; a drop outside the site asks
//  for the pane to float there; a drop anywhere else changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::EndDrag (POINT pointDip)
{
    const DxuiDockDropZone           * hit     = DxuiDockDropZones::HitTest (m_zones, pointDip);
    std::optional<DxuiDockDropZone>    zone;
    std::wstring                       pane    = m_dragPane;
    bool                               changed = false;
    int                                strip   = -1;
    int                                index   = -1;



    //  Copied before the list is cleared, since hit points into it.
    if (hit != nullptr)
    {
        zone = *hit;
    }
    else
    {
        UpdateStripTarget (pointDip);
        strip = m_stripGroup;
        index = m_stripIndex;
    }

    ClearStripTarget();
    m_zones.clear();
    m_hoverZone = -1;

    if (pane.empty())
    {
        m_dragPanes.clear();
        return false;
    }

    //  A group dropped outside floats only the pane shown: a floating window
    //  holds one pane.
    if (zone.has_value())
    {
        changed = DropOnZone (*zone);
    }
    else if (strip >= 0)
    {
        changed = DropOnStrip (strip, index);
    }
    else if (!Contains (m_boundsDip, pointDip) && m_onFloat)
    {
        m_onFloat (pane, pointDip);
    }

    m_dragPane.clear();
    m_dragPanes.clear();

    if (changed)
    {
        Arrange();
        NotifyChanged();
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetHoveredZone
//
////////////////////////////////////////////////////////////////////////////////

const DxuiDockDropZone * DxuiDockSite::GetHoveredZone() const
{
    return (m_hoverZone >= 0 && m_hoverZone < (int) m_zones.size()) ? &m_zones[(size_t) m_hoverZone] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetSashRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetSashRect (const DxuiPaneLayout::SplitRect & split) const
{
    long  half = m_scaler.ToPx (kSashDip) / 2;



    return split.horizontal ? RECT { split.position - half, split.area.top,  split.position + half, split.area.bottom }
                            : RECT { split.area.left, split.position - half, split.area.right, split.position + half };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::HitTestSash
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockSite::HitTestSash (POINT pointDip) const
{
    for (size_t i = 0; i < m_splits.size(); i++)
    {
        if (Contains (GetSashRect (m_splits[i]), pointDip))
        {
            return (int) i;
        }
    }

    return -1;
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
    const DxuiPaneLayout::SplitRect  * split   = nullptr;
    const DxuiDockDropZone           * zone    = nullptr;
    bool                               handled = false;
    long                               total   = 0;
    long                               offset  = 0;
    int                                tab     = -1;
    std::wstring                       edgePane;



    //  Whatever handler retired these has returned by now.
    m_retired.clear();

    if (IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move)
        {
            zone        = DxuiDockDropZones::HitTest (m_zones, ev.positionDip);
            m_hoverZone = (zone != nullptr) ? (int) (zone - m_zones.data()) : -1;

            if (zone != nullptr)
            {
                ClearStripTarget();
            }
            else
            {
                UpdateStripTarget (ev.positionDip);
            }
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
            {
                (void) group->OnMouse (ev);
            }

            //  The slid group, hidden as its title bar's drag began, still
            //  holds that press.
            (void) m_slidGroup.OnMouse (ev);
            EndDrag (ev.positionDip);
        }

        return true;
    }

    if (m_sashDrag >= 0 && m_sashDrag < (int) m_splits.size())
    {
        split = &m_splits[(size_t) m_sashDrag];

        if (ev.kind == DxuiMouseEventKind::Move)
        {
            total  = split->horizontal ? (split->area.right - split->area.left) : (split->area.bottom - split->area.top);
            offset = split->horizontal ? (ev.positionDip.x - split->area.left)  : (ev.positionDip.y - split->area.top);

            if (total > 0 && m_layout.SetRatio (split->path, (float) offset / (float) total))
            {
                Arrange();
            }
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            m_sashDrag = -1;
            NotifyChanged();
        }

        return true;
    }

    //  An edge tab is a button: a press slides its pane out, and a press on
    //  the same tab again slides it back. A hover does nothing. A press
    //  anywhere else slides a slid-out pane back too.
    tab      = HitTestEdgeTab (ev.positionDip);
    edgePane = (tab >= 0) ? m_edgeTabs[(size_t) tab].pane : std::wstring();

    //  The tab under the pointer lights its bar.
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hoverEdge = (ev.kind == DxuiMouseEventKind::Move) ? edgePane : std::wstring();
    }

    if (tab >= 0 && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        if (edgePane == m_slidPane)
        {
            SlideIn();
        }
        else
        {
            SlideOut (edgePane);
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && !m_slidPane.empty() && !Contains (m_slidRect, ev.positionDip))
    {
        SlideIn();
    }

    //  The wheel over a group's tabs scrolls them when they overflow. Over the
    //  chrome it means nothing to the pane below, so it stops here either way.
    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        if (m_slidGroup.IsVisible() && m_slidGroup.IsChromeAt (ev.positionDip))
        {
            (void) m_slidGroup.OnMouse (ev);
            return true;
        }

        for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
        {
            if (group->IsVisible() && group->IsChromeAt (ev.positionDip) && !Contains (m_slidRect, ev.positionDip))
            {
                (void) group->OnMouse (ev);
                return true;
            }
        }
    }

    //  The slid pane's title bar is the site's; its controls are the window's.
    if (ev.kind == DxuiMouseEventKind::Down && m_slidGroup.IsVisible() && m_slidGroup.IsChromeAt (ev.positionDip))
    {
        (void) m_slidGroup.OnMouse (ev);
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && !Contains (m_slidRect, ev.positionDip))
    {
        m_sashDrag = HitTestSash (ev.positionDip);

        if (m_sashDrag >= 0)
        {
            return true;
        }

        for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
        {
            if (group->IsChromeAt (ev.positionDip))
            {
                (void) group->OnMouse (ev);
                return true;
            }
        }

        return false;
    }

    //  A button or tab released here can change the layout, which refills
    //  and retires groups, so the groups are taken before any of them acts.
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up)
    {
        std::vector<DxuiTabGroup *>  groups;

        for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
        {
            groups.push_back (group.get());
        }

        groups.push_back (&m_slidGroup);

        for (DxuiTabGroup * group : groups)
        {
            handled = group->OnMouse (ev) || handled;
        }
    }

    return handled || IsDragging();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiDockSite::GetCursorForPoint (POINT clientPx) const
{
    int  sash = (m_sashDrag >= 0) ? m_sashDrag : HitTestSash (clientPx);



    if (sash < 0 || sash >= (int) m_splits.size())
    {
        return nullptr;
    }

    return m_splits[(size_t) sash].horizontal ? IDC_SIZEWE : IDC_SIZENS;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetDragMarks
//
//  The hovered target's area shaded in the accent color, a hovered strip's
//  group tinted with the gap its tab will take, then every target square,
//  the hovered one outlined in the accent color.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockDragMark> DxuiDockSite::GetDragMarks (const IDxuiTheme & theme) const
{
    std::vector<DxuiDockDragMark>   marks;
    const DxuiDockDropZone        * hover = GetHoveredZone();
    int                             line  = std::max (1, (int) std::lround (m_scaler.ToPxf (1.0f)));
    uint32_t                        tint  = (theme.Accent() & 0x00FFFFFFu) | 0x50000000u;



    if (!IsDragging())
    {
        return marks;
    }

    if (hover != nullptr)
    {
        marks.push_back ({ hover->preview, tint, 0 });
    }

    if (m_stripGroup >= 0 && m_stripGroup < (int) m_groups.size())
    {
        marks.push_back ({ m_groups[(size_t) m_stripGroup]->GetBodyRect(),      tint,                                         0 });
        marks.push_back ({ m_groups[(size_t) m_stripGroup]->GetInsertGapRect(), (theme.Accent() & 0x00FFFFFFu) | 0xA0000000u, 0 });
    }

    for (const DxuiDockDropZone & zone : m_zones)
    {
        marks.push_back ({ zone.target, theme.ControlBackground(),                         0    });
        marks.push_back ({ zone.target, (&zone == hover) ? theme.Accent() : theme.Border(), line });

        if (zone.kind == DxuiDockDropZone::Kind::Split)
        {
            AddDottedHalf (marks, zone, theme.Border(), line);
        }
    }

    return marks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::Paint
//
//  The strips, a seam down each sash, and during a drag the drop zones with
//  the hovered one's area shaded in the accent color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float                      line   = (float) std::max (1L, std::lround (m_scaler.ToPxf (1.0f)));
    auto                       fill   = [&] (const RECT & r, uint32_t argb)
    {
        painter.FillRect ((float) r.left, (float) r.top,
                          (float) (r.right - r.left), (float) (r.bottom - r.top), argb);
    };



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->Paint (painter, text, theme);
    }

    for (const DxuiPaneLayout::SplitRect & split : m_splits)
    {
        if (split.horizontal)
        {
            painter.FillRect ((float) split.position - line / 2, (float) split.area.top,
                              line, (float) (split.area.bottom - split.area.top), theme.Divider());
        }
        else
        {
            painter.FillRect ((float) split.area.left, (float) split.position - line / 2,
                              (float) (split.area.right - split.area.left), line, theme.Divider());
        }
    }

    PaintEdges (painter, text, theme);

    if (!IsDragging() || m_marksElsewhere)
    {
        return;
    }

    for (const DxuiDockDragMark & mark : GetDragMarks (theme))
    {
        if (mark.outlinePx == 0)
        {
            fill (mark.rect, mark.argb);
        }
        else if (mark.dotted)
        {
            for (const RECT & dot : GetOutlineStrips (mark))
            {
                fill (dot, mark.argb);
            }
        }
        else
        {
            painter.OutlineRect ((float) mark.rect.left, (float) mark.rect.top,
                                 (float) (mark.rect.right - mark.rect.left),
                                 (float) (mark.rect.bottom - mark.rect.top),
                                 (float) mark.outlinePx, mark.argb);
        }
    }
}
