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
//  With a gap or margin set, the layout divides the area inside the margin,
//  and each group then gives up its share of the gap at every split, so the
//  splits themselves stay where the layout puts them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Arrange()
{
    std::vector<DxuiPaneLayout::GroupRect>    groups;
    std::unordered_set<const IDxuiControl *>  placed;
    long                                      margin  = m_scaler.ToPx (m_marginDip);
    long                                      gap     = m_scaler.ToPx (m_gapDip);
    DxuiPaneLayout::MinSizeFn                 minSize = GetMinSizeWithGap();
    auto                                      slid    = m_panes.end();



    if (m_boundsDip.right <= m_boundsDip.left || m_boundsDip.bottom <= m_boundsDip.top)
    {
        return;
    }

    m_dockedArea = GetDockedArea();
    m_paneArea   = RECT { m_dockedArea.left + margin, m_dockedArea.top + margin, m_dockedArea.right - margin, m_dockedArea.bottom - margin };

    m_paneArea.right  = std::max (m_paneArea.right,  m_paneArea.left);
    m_paneArea.bottom = std::max (m_paneArea.bottom, m_paneArea.top);

    //  A slid-out pane lies over the docked panes, which keep their places.
    ArrangeEdges (m_dockedArea, m_paneArea);

    groups      = m_layout.Arrange (m_paneArea, m_shown, minSize);
    m_splits    = m_layout.ArrangeSplits (m_paneArea, m_shown, minSize);
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
        RECT            rect     = {};

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
            group->SetTitleExtra  (found->second.content, found->second.titleExtraDip);
            placed.insert (found->second.content);
        }

        //  A floating window's title bar is its pane's, so every group there
        //  is a tool window.
        document = document && m_onDock == nullptr;

        rect = GetInsetForGap (groups[i].rect, m_paneArea, gap);

        group->SetKind          (document ? DxuiTabGroup::Kind::Document : DxuiTabGroup::Kind::ToolWindow);
        group->SetFocusedLook   (focused);
        group->SetStripForced   (carried);
        group->SetActive        (active);
        group->SetWindowCorners (GetWindowCorners (rect), m_cornerDip);
        group->Layout           (rect, m_scaler);
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

    group->SetOnDragStart ([this, group] (int index, POINT pointDip)
    {
        std::wstring  pane = GetPaneOf (group->GetContent (index));



        if (!TearOff (group, pane, pointDip, true))
        {
            BeginDrag (pane);
            m_pointerDrag = IsDragging();
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
        m_pointerDrag = IsDragging();
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
//  DxuiDockSite::GetWindowCorners
//
//  The window's corners are the site's own, which a group touches where it
//  meets both of the site's edges there: in a floating window, with no
//  margin around its panes, a lone group touches all four.
//
////////////////////////////////////////////////////////////////////////////////

UINT DxuiDockSite::GetWindowCorners (const RECT & group) const
{
    bool  isRounded = m_cornerDip > 0;
    bool  isLeft    = isRounded && group.left   == m_boundsDip.left;
    bool  isTop     = isRounded && group.top    == m_boundsDip.top;
    bool  isRight   = isRounded && group.right  == m_boundsDip.right;
    bool  isBottom  = isRounded && group.bottom == m_boundsDip.bottom;
    UINT  corners   = 0;



    corners |= (isLeft  && isTop)    ? DxuiPaneFrame::kCornerTopLeft     : 0;
    corners |= (isRight && isTop)    ? DxuiPaneFrame::kCornerTopRight    : 0;
    corners |= (isLeft  && isBottom) ? DxuiPaneFrame::kCornerBottomLeft  : 0;
    corners |= (isRight && isBottom) ? DxuiPaneFrame::kCornerBottomRight : 0;

    return corners;
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
//  DxuiDockSite::GetTitleButtonTipAt
//
//  The pin's tip gives what pressing it does in OnTitleButton: docks the
//  pane or hides it against its edge. A tab's pin and close button are its
//  pane's title bar buttons.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetTitleButtonTipAt (POINT pointDip, RECT & button) const
{
    static constexpr DxuiTabGroup::TitleButton  kButtons[] = { DxuiTabGroup::TitleButton::Menu, DxuiTabGroup::TitleButton::Pin,
                                                               DxuiTabGroup::TitleButton::Close };
    auto                                        pinTip     = [this] (const std::wstring & pane)
    {
        return (m_onDock != nullptr || m_layout.IsAutoHidden (pane)) ? L"Dock" : L"Auto hide";
    };



    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        std::wstring               pane;
        DxuiTabGroup::TitleButton  onTab = DxuiTabGroup::TitleButton::Close;
        int                        index = -1;

        if (group->IsVisible() && group->TryGetTabButtonAt (pointDip, onTab, index, button))
        {
            pane = GetPaneOf (group->GetContent (index));
            return (onTab == DxuiTabGroup::TitleButton::Pin) ? pinTip (pane) : L"Close";
        }

        if (!group->IsVisible() || !Contains (group->GetTitleRect(), pointDip))
        {
            continue;
        }

        pane = GetPaneOf (group->GetActiveContent());

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

    if (!m_slidPane.empty() && m_slidPane != pane)
    {
        SlideIn();
        return;
    }

    m_slidGroup.SetFocusedLook (!m_slidPane.empty() && m_slidPane == pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetEdgeShare
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetEdgeShare (DxuiDockSide edge, long thickness, long start, long end)
{
    m_shareOn    = true;
    m_shareEdge  = edge;
    m_shareDepth = thickness;
    m_shareStart = start;
    m_shareEnd   = end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ClearEdgeShare
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ClearEdgeShare()
{
    m_shareOn = false;
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
    RECT  area     = m_boundsDip;
    bool  used[4]  = {};
    long  depth[4] = {};



    for (const DxuiPaneLayout::AutoHidden & hidden : m_layout.GetAutoHidden())
    {
        if (!m_shown || m_shown (hidden.pane))
        {
            used[(size_t) hidden.edge] = true;
        }
    }

    //  Every edge's strip is one tab high: along a side the tabs run down it,
    //  their titles turned to read top to bottom.
    //  A shared edge is as deep as the deeper of its tabs and what shares it.
    for (size_t edge = 0; edge < 4; edge++)
    {
        depth[edge] = used[edge] ? m_scaler.ToPx (kEdgeStripDip) : 0;

        if (m_shareOn && (size_t) m_shareEdge == edge)
        {
            depth[edge] = std::max (depth[edge], m_shareDepth);
        }
    }

    area.left   += depth[(size_t) DxuiDockSide::Left];
    area.top    += depth[(size_t) DxuiDockSide::Top];
    area.right  -= depth[(size_t) DxuiDockSide::Right];
    area.bottom -= depth[(size_t) DxuiDockSide::Bottom];

    area.right  = std::max (area.right,  area.left);
    area.bottom = std::max (area.bottom, area.top);

    return area;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ArrangeEdges
//
//  The edge tabs, in the order the panes were hidden, between the site's edge
//  and the docked area, and the area a slid-out pane covers: a third of the
//  pane area from its edge, and never less than it needs to be usable. The
//  pane area is the docked area less its margin, so the slid-out pane lines
//  up with the docked ones.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ArrangeEdges (const RECT & dockedArea, const RECT & paneArea)
{
    long          along[4]  = { dockedArea.top, dockedArea.left, dockedArea.top, dockedArea.left };
    long          minSlide  = m_scaler.ToPx (kSlideMinDip);
    long          width     = paneArea.right - paneArea.left;
    long          height    = paneArea.bottom - paneArea.top;
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

        //  A tab that would run into the shared stretch goes on past it.
        if (m_shareOn && hidden.edge == m_shareEdge && at < m_shareEnd && at + length > m_shareStart)
        {
            at = m_shareEnd;
        }

        switch (hidden.edge)
        {
        case DxuiDockSide::Left:   tab.rect = RECT { m_boundsDip.left,  at, dockedArea.left,   at + length };  at += length;  break;
        case DxuiDockSide::Right:  tab.rect = RECT { dockedArea.right,  at, m_boundsDip.right, at + length };  at += length;  break;
        case DxuiDockSide::Top:    tab.rect = RECT { at, m_boundsDip.top, at + length, dockedArea.top };       at += length;  break;
        case DxuiDockSide::Bottom: tab.rect = RECT { at, dockedArea.bottom, at + length, m_boundsDip.bottom }; at += length;  break;
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
    case DxuiDockSide::Left:   m_slidRect = RECT { paneArea.left, paneArea.top, paneArea.left + std::min (width, std::max (minSlide, width / 3)), paneArea.bottom };       break;
    case DxuiDockSide::Right:  m_slidRect = RECT { paneArea.right - std::min (width, std::max (minSlide, width / 3)), paneArea.top, paneArea.right, paneArea.bottom };     break;
    case DxuiDockSide::Top:    m_slidRect = RECT { paneArea.left, paneArea.top, paneArea.right, paneArea.top + std::min (height, std::max (minSlide, height / 3)) };       break;
    case DxuiDockSide::Bottom: m_slidRect = RECT { paneArea.left, paneArea.bottom - std::min (height, std::max (minSlide, height / 3)), paneArea.right, paneArea.bottom }; break;
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
//  The edge strips and their tabs. The slid-out pane itself is drawn by
//  PaintSlidUnder and PaintSlidOver.
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
//  Its title bar and its frame, which follows its focused look as every
//  other group's does. The frame's corner pieces round off the square
//  background PaintSlidUnder laid.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintSlidOver (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (!m_slidGroup.IsVisible())
    {
        return;
    }

    m_slidGroup.Paint      (painter, text, theme);
    m_slidGroup.PaintFrame (painter, theme);
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
//  DxuiDockSite::SetTitleExtra
//
//  Kept with the pane as it moves between groups, and given to the group
//  that holds it now, so the room shows without a new arrangement.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTitleExtra (const std::wstring & pane, int widthDip)
{
    auto  found = m_panes.find (pane);



    if (found == m_panes.end() || found->second.titleExtraDip == widthDip)
    {
        return;
    }

    found->second.titleExtraDip = widthDip;

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetTitleExtra (found->second.content, widthDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TryGetTitleExtraRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const
{
    auto  found = m_panes.find (pane);



    rect = {};

    if (found == m_panes.end() || found->second.content == nullptr)
    {
        return false;
    }

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        if (group->IsVisible() && group->GetActiveContent() == found->second.content)
        {
            rect = group->GetTitleExtraRect();
            break;
        }
    }

    return rect.right > rect.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetTabAt
//
//  Over a tab's pin or close button the tab gives no tip of its own; the
//  button's is GetTitleButtonTipAt's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDockSite::GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const
{
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        int  index = group->IsVisible() ? group->HitTestTab (pointDip) : -1;

        if (index >= 0)
        {
            std::wstring               pane     = GetPaneOf (group->GetContent (index));
            auto                       found    = m_panes.find (pane);
            DxuiTabGroup::TitleButton  button   = DxuiTabGroup::TitleButton::Close;
            int                        onButton = -1;
            RECT                       rect     = {};
            bool                       isButton = group->TryGetTabButtonAt (pointDip, button, onButton, rect);

            tab = group->GetTabRect (index);
            tip = (found != m_panes.end() && !isButton) ? found->second.tip : std::wstring();
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
//  would put it back where it is. The edge guides lie along the docked
//  area, the margin around the panes included, and each target's shade is
//  worked out now, while the layout is as the drag found it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::BeginGroupDrag (const std::vector<std::wstring> & panes, const std::wstring & active)
{
    std::vector<DxuiPaneLayout::GroupRect>  groups;
    long                                    gap     = m_scaler.ToPx (m_gapDip);
    auto                                    dragged = [&panes] (const std::wstring & pane)
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

    //  The groups as Arrange lays them out, each less its share of the gaps,
    //  so a group's cross sits on the middle of the pane as it is drawn.
    groups = m_layout.Arrange (GetPaneArea(), m_shown, GetMinSizeWithGap());

    for (DxuiPaneLayout::GroupRect & group : groups)
    {
        group.rect = GetInsetForGap (group.rect, GetPaneArea(), gap);
    }

    m_dragPane     = active;
    m_dragPanes    = panes;
    m_zones        = DxuiDockDropZones::Build (groups, GetDockedArea(), active, m_scaler,
                                               [this] (const DxuiPaneLayout::GroupRect & group) { return IsDocumentGroup (group.panes); });
    m_hoverZone    = -1;
    m_compassGroup = -1;
    m_pointerDrag  = false;

    if (panes.size() > 1)
    {
        std::erase_if (m_zones, [&] (const DxuiDockDropZone & zone) { return dragged (zone.targetPane); });
    }

    for (DxuiDockDropZone & zone : m_zones)
    {
        SetDropPreview (zone);
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
//  new group, in order. A drop is made on the site's own layout, and on a
//  copy of it to see where the panes would go.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::DropOnZone (
    const DxuiDockDropZone  & zone,
    DxuiPaneLayout          & layout) const
{
    std::vector<std::wstring>  panes;
    int                        at      = 0;



    if (m_dragPanes.empty() || !DxuiDockDropZones::Apply (zone, layout, m_dragPanes.front(),
                                                                  [this] (const std::vector<std::wstring> & panes) { return IsDocumentGroup (panes); }))
    {
        return false;
    }

    for (size_t i = 1; i < m_dragPanes.size(); i++)
    {
        panes   = layout.GetGroup (m_dragPanes.front());
        at      = (int) (std::find (panes.begin(), panes.end(), m_dragPanes[i - 1]) - panes.begin()) + 1;
        (void) layout.TabWithAt (m_dragPanes[i], m_dragPanes.front(), at);
    }

    (void) layout.Activate (m_dragPane);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetDropPreview
//
//  What a target shades while the pointer is on it: what the drop would give
//  the dragged panes, as Visual Studio shows it. A tab drop is the target
//  group with its new tab (SetTabDropPreview). Any other drop is the group
//  the panes would form, as the layout would lay it out with the drop made,
//  less its share of the gaps; a window edge's also reaches over the margin
//  to the docked area's edges. A drop the layout cannot make keeps the shade
//  DxuiDockDropZones gave it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetDropPreview (DxuiDockDropZone & zone) const
{
    DxuiPaneLayout                          layout = m_layout;
    std::vector<DxuiPaneLayout::GroupRect>  groups;
    RECT                                    area   = GetPaneArea();
    RECT                                    docked = GetDockedArea();
    long                                    gap    = m_scaler.ToPx (m_gapDip);
    bool                                    isEdge = zone.kind == DxuiDockDropZone::Kind::Edge;



    if (zone.kind == DxuiDockDropZone::Kind::Tab)
    {
        SetTabDropPreview (zone);
        return;
    }

    if (!DropOnZone (zone, layout))
    {
        return;
    }

    groups = layout.Arrange (area, m_shown, GetMinSizeWithGap());

    for (const DxuiPaneLayout::GroupRect & group : groups)
    {
        const RECT  & r = group.rect;

        if (std::find (group.panes.begin(), group.panes.end(), m_dragPanes.front()) == group.panes.end())
        {
            continue;
        }

        zone.preview = GetInsetForGap (r, area, gap);

        if (isEdge)
        {
            zone.preview.left   = (r.left   <= area.left)   ? docked.left   : zone.preview.left;
            zone.preview.top    = (r.top    <= area.top)    ? docked.top    : zone.preview.top;
            zone.preview.right  = (r.right  >= area.right)  ? docked.right  : zone.preview.right;
            zone.preview.bottom = (r.bottom >= area.bottom) ? docked.bottom : zone.preview.bottom;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::SetTabDropPreview
//
//  A tab drop's shade, as Visual Studio draws it: the target pane up to its
//  tabs, short of the line between them, and a tab kPreviewTabDip wide at
//  the start of the band, over the band and that line. The rows are where a
//  group of the target's kind puts its tabs, so a tool window showing no
//  tabs yet shows the strip the drop gives it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetTabDropPreview (DxuiDockDropZone & zone) const
{
    const DxuiTabGroup  * target = FindGroupOf (zone.targetPane);
    DxuiTabGroup          probe;
    RECT                  pane   = {};
    RECT                  band   = {};
    long                  line   = DxuiPaneMetrics::GetLinePx (m_scaler);
    long                  tabEnd = 0;
    long                  edge   = 0;



    if (target == nullptr)
    {
        return;
    }

    pane   = target->GetBounds();
    tabEnd = std::min (pane.right, pane.left + (long) m_scaler.ToPx (kPreviewTabDip));

    probe.SetKind        (target->GetKind());
    probe.SetStripForced (true);
    probe.Layout         (pane, m_scaler);
    band = probe.GetStripRect();

    if (target->GetKind() == DxuiTabGroup::Kind::Document)
    {
        edge            = std::min (pane.bottom, band.bottom + line);
        zone.preview    = RECT { pane.left, edge,     pane.right, pane.bottom };
        zone.previewTab = RECT { pane.left, pane.top, tabEnd,     edge        };
    }
    else
    {
        edge            = std::max (pane.top, band.top - line);
        zone.preview    = RECT { pane.left, pane.top, pane.right, edge        };
        zone.previewTab = RECT { pane.left, edge,     tabEnd,     pane.bottom };
    }
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
    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->CancelPress();
    }

    m_slidGroup.CancelPress();
    ClearStripTarget();
    m_dragPane.clear();
    m_dragPanes.clear();
    m_zones.clear();
    m_hoverZone    = -1;
    m_compassGroup = -1;
    m_pointerDrag  = false;
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
//  DxuiDockSite::UpdateCompass
//
//  The one cross a drag shows is the cross of the group under the pointer.
//  It stays while the pointer is anywhere on it, so a cross wider than its
//  group's pane can still be used to its ends; off it, the group under the
//  pointer takes the cross, and outside every group none shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::UpdateCompass (POINT pointDip)
{
    DxuiDockGuideKind  kind   = DxuiDockGuideKind::SmallCross;
    POINT              origin = {};



    if (TryGetCompass (m_compassGroup, kind, origin) && DxuiDockGuide::IsInside (kind, origin, pointDip, m_scaler))
    {
        return;
    }

    m_compassGroup = -1;

    for (const DxuiDockDropZone & zone : m_zones)
    {
        if (zone.group >= 0 && Contains (zone.groupRect, pointDip))
        {
            m_compassGroup = zone.group;
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TrackDrag
//
//  What a drag targets with the pointer at a point: a button of a guide on
//  show, or else a group's tabs or title bar. Off the docked area -- over a
//  menu, a toolbar or a status bar, or out of the window -- the drag targets
//  nothing, so the cross and the shade go at once and only the edge guides
//  stay, even where a cross reaches past the area.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::TrackDrag (POINT pointDip)
{
    if (!Contains (GetDockedArea(), pointDip))
    {
        ClearDragTarget();
        return;
    }

    UpdateCompass (pointDip);
    m_hoverZone = HitTestShown (pointDip);

    if (GetHoveredZone() != nullptr)
    {
        ClearStripTarget();
    }
    else
    {
        UpdateStripTarget (pointDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::ClearDragTarget
//
//  The cross, the hovered button and the strip under the pointer, gone; the
//  edge guides stay for as long as the drag does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::ClearDragTarget()
{
    m_compassGroup = -1;
    m_hoverZone    = -1;
    ClearStripTarget();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::HitTestShown
//
//  As DxuiDockDropZones::HitTest, the last zone holding the point, but only
//  among the zones on show: the edge guides and the one cross.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockSite::HitTestShown (POINT pointDip) const
{
    int  hit = -1;



    for (size_t i = 0; i < m_zones.size(); i++)
    {
        const DxuiDockDropZone  & zone  = m_zones[i];
        bool                      shown = zone.kind == DxuiDockDropZone::Kind::Edge || (m_compassGroup >= 0 && zone.group == m_compassGroup);

        hit = (shown && Contains (zone.target, pointDip)) ? (int) i : hit;
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::TryGetCompass
//
//  A group's cross, as Build placed it: large when the group has split
//  zones, centered on the group. False when the group has no zones.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::TryGetCompass (int group, DxuiDockGuideKind & kind, POINT & origin) const
{
    const DxuiDockDropZone  * found = nullptr;
    bool                      split = false;



    for (const DxuiDockDropZone & zone : m_zones)
    {
        if (group >= 0 && zone.group == group)
        {
            found = &zone;
            split = split || zone.kind == DxuiDockDropZone::Kind::Split;
        }
    }

    if (found == nullptr)
    {
        return false;
    }

    kind   = split ? DxuiDockGuideKind::LargeCross : DxuiDockGuideKind::SmallCross;
    origin = DxuiDockGuide::GetOrigin (kind, POINT { (found->groupRect.left + found->groupRect.right) / 2,
                                                     (found->groupRect.top + found->groupRect.bottom) / 2 }, m_scaler);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetGuideImage
//
//  Drawn once for each look of each guide and kept, so the frames of a drag
//  draw the same buffers and a renderer's bitmap cache stays warm. The cache
//  starts over once it fills, which takes a change of theme or DPI.
//
//  Where `shades` lie over the guide, the drop's shade is laid over the
//  picture, so the guide looks to lie under the shade even though it is
//  drawn after it.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DxuiDockSite::GetGuideImage (
    DxuiDockGuideKind                       kind,
    DxuiDockSide                            edge,
    int                                     hovered,
    const std::vector<DxuiCoverageRect>   & shades,
    const IDxuiTheme                      & theme) const
{
    GuideImage     wanted;
    DxuiIconImage  image;



    wanted.key.kind                = kind;
    wanted.key.edge                = edge;
    wanted.key.hovered             = hovered;
    wanted.key.dpi                 = m_scaler.GetDpi();
    wanted.key.colors.border       = theme.DockGuideBorder();
    wanted.key.colors.fill         = theme.DockGuideFill();
    wanted.key.colors.buttonBorder = theme.DockGuideButtonBorder();
    wanted.key.colors.buttonFill   = theme.DockGuideButtonFill();
    wanted.key.colors.glyph        = theme.DockGuideGlyph();
    wanted.key.colors.arrow        = theme.DockGuideArrow();
    wanted.key.shades              = shades;
    wanted.key.shadeArgb           = theme.DockPreview();

    for (const GuideImage & cached : m_guideImages)
    {
        if (cached.key == wanted.key)
        {
            return cached.image;
        }
    }

    if (m_guideImages.size() >= kGuideCacheMax)
    {
        m_guideImages.clear();
    }

    image = DxuiDockGuide::Render (kind, edge, hovered, wanted.key.colors, m_scaler);

    for (const DxuiCoverageRect & shade : shades)
    {
        DxuiCoverageRaster::TintCovered (image, shade, wanted.key.shadeArgb);
    }

    wanted.image = std::make_shared<const DxuiIconImage> (std::move (image));
    m_guideImages.push_back (wanted);

    return wanted.image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::EndDrag
//
//  A drop on a zone is that zone's operation; a drop outside the site asks
//  for the pane to float there; a drop anywhere else changes nothing. The
//  zone is found as a move to the drop point would have shown it, so a drop
//  with no move before it still lands in the group under the point.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockSite::EndDrag (POINT pointDip)
{
    const DxuiDockDropZone           * hit     = nullptr;
    std::optional<DxuiDockDropZone>    zone;
    std::wstring                       pane    = m_dragPane;
    bool                               changed = false;
    int                                strip   = -1;
    int                                index   = -1;



    TrackDrag (pointDip);
    hit = GetHoveredZone();

    //  Copied before the list is cleared, since hit points into it.
    if (hit != nullptr)
    {
        zone = *hit;
    }
    else
    {
        strip = m_stripGroup;
        index = m_stripIndex;
    }

    ClearStripTarget();
    m_zones.clear();
    m_hoverZone    = -1;
    m_compassGroup = -1;
    m_pointerDrag  = false;

    if (pane.empty())
    {
        m_dragPanes.clear();
        return false;
    }

    //  A group dropped outside floats only the pane shown: a floating window
    //  holds one pane.
    if (zone.has_value())
    {
        changed = DropOnZone (*zone, m_layout);
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
//  DxuiDockSite::SetPaneGap
//
//  The gap between neighboring panes and the margin between the outer panes
//  and the docked area's edges, in DIP.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::SetPaneGap (int gapDip, int marginDip)
{
    m_gapDip    = gapDip;
    m_marginDip = marginDip;

    //  Before the site has bounds, its first layout takes these up.
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetInsetForGap
//
//  A group's rect less its share of the gaps. A side inside the pane area
//  moves in by half the gap, a left or top side by the larger half, so the
//  neighbors at a split are exactly the gap apart; a side on the pane area's
//  edge stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetInsetForGap (const RECT & rect, const RECT & paneArea, long gapPx)
{
    long  lo    = gapPx / 2;
    long  hi    = gapPx - lo;
    RECT  inset = rect;



    if (gapPx <= 0)
    {
        return rect;
    }

    if (rect.left > paneArea.left)
    {
        inset.left += hi;
    }

    if (rect.top > paneArea.top)
    {
        inset.top += hi;
    }

    if (rect.right < paneArea.right)
    {
        inset.right -= lo;
    }

    if (rect.bottom < paneArea.bottom)
    {
        inset.bottom -= lo;
    }

    inset.right  = std::max (inset.right,  inset.left);
    inset.bottom = std::max (inset.bottom, inset.top);
    return inset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetMinSizeWithGap
//
//  Each pane's minimum size as the layout takes it. A pane loses up to the
//  gap on each axis, so while a gap is set it needs that much more.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout::MinSizeFn DxuiDockSite::GetMinSizeWithGap() const
{
    long                       gap     = m_scaler.ToPx (m_gapDip);
    DxuiPaneLayout::MinSizeFn  minSize = m_minSize;



    if (gap > 0 && m_minSize)
    {
        minSize = [base = m_minSize, gap] (const std::wstring & pane)
        {
            SIZE  size = base (pane);

            return SIZE { size.cx + gap, size.cy + gap };
        };
    }

    return minSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::PaintGaps
//
//  The margin around the panes and the gap at every split, in the theme's
//  DockGap. None of it lies under a group: the main window's site paints
//  after the pane controls, so a fill there would cover a pane.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintGaps (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    uint32_t  argb      = theme.DockGap();
    RECT      margins[] =
    {
        { m_dockedArea.left,  m_dockedArea.top,  m_dockedArea.right, m_paneArea.top       },
        { m_dockedArea.left,  m_paneArea.bottom, m_dockedArea.right, m_dockedArea.bottom  },
        { m_dockedArea.left,  m_paneArea.top,    m_paneArea.left,    m_paneArea.bottom    },
        { m_paneArea.right,   m_paneArea.top,    m_dockedArea.right, m_paneArea.bottom    },
    };
    auto      fill      = [&painter, argb] (const RECT & r)
    {
        if (r.right > r.left && r.bottom > r.top)
        {
            painter.FillRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top), argb);
        }
    };



    if (m_gapDip == 0 && m_marginDip == 0)
    {
        return;
    }

    for (const RECT & margin : margins)
    {
        fill (margin);
    }

    if (m_gapDip <= 0)
    {
        return;
    }

    for (const DxuiPaneLayout::SplitRect & split : m_splits)
    {
        fill (GetSashRect (split));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite::GetSashRect
//
//  With a gap, exactly the gap, so a press anywhere between two panes drags
//  their split and a press on a pane never does; without one, a band
//  centered on the split.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockSite::GetSashRect (const DxuiPaneLayout::SplitRect & split) const
{
    long  gap = m_scaler.ToPx (m_gapDip);
    long  lo  = (m_gapDip > 0) ? gap / 2 : m_scaler.ToPx (kSashDip) / 2;
    long  hi  = (m_gapDip > 0) ? gap - lo : lo;



    return split.horizontal ? RECT { split.position - lo, split.area.top,  split.position + hi, split.area.bottom }
                            : RECT { split.area.left, split.position - lo, split.area.right, split.position + hi };
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
            TrackDrag (ev.positionDip);
        }
        else if (ev.kind == DxuiMouseEventKind::Leave)
        {
            ClearDragTarget();
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
//  The shade over what the hovered target would give the dragged panes, in
//  the theme's DockPreview, and a hovered strip's group tinted with the gap
//  its tab will take; then the guides, one at the middle of each window edge
//  and the cross of the group under the pointer.
//
//  As Visual Studio stacks them, the shade lies over the guide holding the
//  button under the pointer, and under every other guide. That guide comes
//  first among the guides, with the shade laid over its picture where the
//  two meet, so it shows under the shade whichever way the marks are drawn.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockDragMark> DxuiDockSite::GetDragMarks (const IDxuiTheme & theme) const
{
    constexpr uint32_t              kColor   = 0x00FFFFFFu;
    constexpr uint32_t              kGapTint = 0xA0000000u;
    std::vector<DxuiDockDragMark>   marks;
    std::vector<DxuiDockDragMark>   guides;
    std::vector<RECT>               shades;
    const DxuiDockDropZone        * hover    = GetHoveredZone();
    uint32_t                        preview  = theme.DockPreview();
    DxuiDockGuideKind               kind     = DxuiDockGuideKind::SmallCross;
    DxuiDockGuideButton             button   = DxuiDockGuideButton::Center;
    POINT                           origin   = {};
    int                             hovered  = -1;
    auto                            addGuide = [&] (DxuiDockGuideKind guide, DxuiDockSide edge, POINT at, int lit)
    {
        DxuiDockDragMark               mark;
        SIZE                           size = DxuiDockGuide::GetSizePx (guide, m_scaler);
        std::vector<DxuiCoverageRect>  over;

        for (size_t i = 0; lit >= 0 && i < shades.size(); i++)
        {
            over.push_back (DxuiCoverageRect { (float) (shades[i].left - at.x),  (float) (shades[i].top - at.y),
                                               (float) (shades[i].right - at.x), (float) (shades[i].bottom - at.y) });
        }

        mark.rect  = RECT { at.x, at.y, at.x + size.cx, at.y + size.cy };
        mark.image = GetGuideImage (guide, edge, lit, over, theme);
        guides.insert ((lit >= 0) ? guides.begin() : guides.end(), mark);
    };



    if (!IsDragging())
    {
        return marks;
    }

    if (hover != nullptr)
    {
        for (const RECT & shade : { hover->preview, hover->previewTab })
        {
            if (shade.right > shade.left && shade.bottom > shade.top)
            {
                shades.push_back (shade);
                marks.push_back ({ shade, preview, 0 });
            }
        }
    }

    if (m_stripGroup >= 0 && m_stripGroup < (int) m_groups.size())
    {
        marks.push_back ({ m_groups[(size_t) m_stripGroup]->GetBodyRect(),      preview,                                0 });
        marks.push_back ({ m_groups[(size_t) m_stripGroup]->GetInsertGapRect(), (theme.Accent() & kColor) | kGapTint, 0 });
    }

    for (const DxuiDockDropZone & zone : m_zones)
    {
        if (zone.kind != DxuiDockDropZone::Kind::Edge)
        {
            continue;
        }

        button  = DxuiDockGuide::GetDockButton (zone.side);
        hovered = (&zone == hover) ? (int) button : -1;
        addGuide (DxuiDockGuideKind::Edge, zone.side, DxuiDockGuide::GetOriginOfButton (DxuiDockGuideKind::Edge, button, zone.target, m_scaler), hovered);
    }

    if (TryGetCompass (m_compassGroup, kind, origin))
    {
        hovered = (hover != nullptr && hover->group == m_compassGroup) ? (int) DxuiDockDropZones::GetGuideButton (*hover) : -1;
        addGuide (kind, DxuiDockSide::Left, origin, hovered);
    }

    marks.insert (marks.end(), guides.begin(), guides.end());
    return marks;
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
    PaintGaps (painter, theme);

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->Paint (painter, text, theme);
    }

    PaintEdges (painter, text, theme);
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

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->PaintFrame (painter, theme);
    }
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
    return (m_gapDip != 0 || m_marginDip != 0) ? m_paneArea : GetDockedArea();
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
//  the shade covers has the shade drawn into its picture.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::PaintDragLayer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme) const
{
    HRESULT  hr      = S_OK;
    bool     isShown = HasDragLayer();
    auto     fill    = [&] (const RECT & r, uint32_t argb)
    {
        painter.FillRect ((float) r.left, (float) r.top,
                          (float) (r.right - r.left), (float) (r.bottom - r.top), argb);
    };



    BAIL_OUT_IF (!isShown, S_OK);

    for (const DxuiDockDragMark & mark : GetDragMarks (theme))
    {
        if (mark.image != nullptr)
        {
            hr = text.DrawIconBitmap (mark.image->bgraPremul.data(), mark.image->width, mark.image->height,
                                      (float) mark.rect.left, (float) mark.rect.top,
                                      (float) mark.image->width, (float) mark.image->height);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
        else if (mark.outlinePx == 0)
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

Error:
    return;
}





