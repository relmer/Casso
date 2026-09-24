#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





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

    ArrangeEdges (area);

    //  Text is drawn after every fill, so one pane cannot be painted over
    //  another: while a pane is slid out, the docked panes take the room
    //  beside it rather than lying under it.
    if (!m_slidPane.empty())
    {
        switch (m_slidEdge)
        {
        case DxuiDockSide::Left:   area.left   = m_slidRect.right;  break;
        case DxuiDockSide::Right:  area.right  = m_slidRect.left;   break;
        case DxuiDockSide::Top:    area.top    = m_slidRect.bottom; break;
        case DxuiDockSide::Bottom: area.bottom = m_slidRect.top;    break;
        }
    }

    groups      = m_layout.Arrange (area, m_shown, m_minSize);
    m_splits    = m_layout.ArrangeSplits (area, m_shown, m_minSize);
    m_arranging = true;

    while (m_groups.size() < groups.size())
    {
        std::unique_ptr<DxuiTabGroup>  group = std::make_unique<DxuiTabGroup>();
        DxuiTabGroup                 * raw   = group.get();

        raw->SetOnActivated ([this, raw] (int index)
        {
            if (!m_arranging && m_layout.Activate (GetPaneOf (raw->GetContent (index))))
            {
                NotifyChanged();
            }
        });

        raw->SetOnDragStart ([this, raw] (int index, POINT)
        {
            BeginDrag (GetPaneOf (raw->GetContent (index)));
        });

        raw->SetNewTab (m_newTabShown, m_newTab);

        m_groups.push_back (std::move (group));
    }

    m_groups.resize (groups.size());

    for (size_t i = 0; i < groups.size(); i++)
    {
        DxuiTabGroup  * group  = m_groups[i].get();
        int             active = 0;

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

            active = (pane == groups[i].active) ? (int) group->GetTabCount() : active;
            group->AddTab (found->second.title, found->second.content);
            group->SetLeadingMark (found->second.content, found->second.leadMark);
            placed.insert (found->second.content);
        }

        group->SetActive (active);
        group->Layout    (groups[i].rect, m_scaler);
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

    if (slid != m_panes.end() && slid->second.content != nullptr)
    {
        slid->second.content->SetVisible (true);
        slid->second.content->Layout (m_slidRect, m_scaler);
    }

    m_arranging = false;
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

    area.left   += used[(size_t) DxuiDockSide::Left]   ? m_scaler.ToPx (kSideStripDip)            : 0;
    area.top    += used[(size_t) DxuiDockSide::Top]    ? m_scaler.ToPx (DxuiTabGroup::kStripDip)  : 0;
    area.right  -= used[(size_t) DxuiDockSide::Right]  ? m_scaler.ToPx (kSideStripDip)            : 0;
    area.bottom -= used[(size_t) DxuiDockSide::Bottom] ? m_scaler.ToPx (DxuiTabGroup::kStripDip)  : 0;

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
    long          stripH    = m_scaler.ToPx (DxuiTabGroup::kStripDip);
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
        case DxuiDockSide::Left:   tab.rect = RECT { m_boundsDip.left, at, area.left,         at + stripH }; at += stripH;  break;
        case DxuiDockSide::Right:  tab.rect = RECT { area.right,       at, m_boundsDip.right, at + stripH }; at += stripH;  break;
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
    DxuiFontHandle  font = theme.BodyFont();
    float           line = (float) std::max (1L, std::lround (m_scaler.ToPxf (1.0f)));
    float           pad  = m_scaler.ToPxf ((float) DxuiTabGroup::kTabPadDip / 2);
    auto            slid = m_slidPane.empty() ? m_panes.end() : m_panes.find (m_slidPane);
    HRESULT         hr   = S_OK;



    for (const EdgeTab & tab : m_edgeTabs)
    {
        const RECT  & r      = tab.rect;
        bool          active = (tab.pane == m_slidPane);
        auto          found  = m_panes.find (tab.pane);

        painter.FillRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top),
                          active ? theme.ContentBackground() : theme.Background());
        painter.OutlineRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top),
                             line, active ? theme.Accent() : theme.Divider());

        hr = text.DrawString (GetTitle (tab.pane).c_str(), (float) r.left + pad, (float) r.top,
                              (float) (r.right - r.left) - 2 * pad, (float) (r.bottom - r.top),
                              active ? theme.Foreground() : theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (found != m_panes.end() && found->second.indicator)
        {
            painter.FillCircle ((float) r.right - pad, (float) r.top + pad, m_scaler.ToPxf (3.0f), theme.Accent());
        }
    }

    if (slid == m_panes.end() || slid->second.content == nullptr)
    {
        return;
    }

    painter.OutlineRect ((float) m_slidRect.left, (float) m_slidRect.top, (float) (m_slidRect.right - m_slidRect.left),
                         (float) (m_slidRect.bottom - m_slidRect.top), line, theme.Accent());
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
    static const wchar_t * const   kNames[]    = { L"Dock Left", L"Dock Top", L"Dock Right", L"Dock Bottom" };
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

        //  Against the edge the pane's group lies nearest; between two it
        //  touches, the one along its long side, as a column hides to a side
        //  and a row to the top or bottom.
        for (size_t i = 1; i < std::size (gaps); i++)
        {
            bool  sideways = (i % 2) == 0;
            bool  tall     = (group.rect.bottom - group.rect.top) >= (group.rect.right - group.rect.left);

            nearest = (gaps[i] < gaps[nearest] || (gaps[i] == gaps[nearest] && sideways == tall && (nearest % 2 == 0) != tall)) ? i : nearest;
        }

        items.push_back ({ L"Auto Hide", [this, pane, nearest, commit] { return commit (m_layout.AutoHide (pane, kSides[nearest])); } });
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
//  DxuiDockSite::BeginDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::BeginDrag (const std::wstring & pane)
{
    if (pane.empty())
    {
        return;
    }

    m_dragPane  = pane;
    m_zones     = DxuiDockDropZones::Build (m_layout.Arrange (GetDockedArea(), m_shown, m_minSize), GetDockedArea(), pane);
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



    //  Copied before the list is cleared, since hit points into it.
    if (hit != nullptr)
    {
        zone = *hit;
    }

    m_dragPane.clear();
    m_zones.clear();
    m_hoverZone = -1;

    if (pane.empty())
    {
        return false;
    }

    if (zone.has_value())
    {
        changed = DxuiDockDropZones::Apply (*zone, m_layout, pane);
    }
    else if (!Contains (m_boundsDip, pointDip) && m_onFloat)
    {
        m_onFloat (pane, pointDip);
    }

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



    if (IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move)
        {
            zone        = DxuiDockDropZones::HitTest (m_zones, ev.positionDip);
            m_hoverZone = (zone != nullptr) ? (int) (zone - m_zones.data()) : -1;
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
            {
                (void) group->OnMouse (ev);
            }

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

    //  An edge tab slides its pane out on a hover or a press, since a click
    //  arrives after the hover that already opened it; a press anywhere else
    //  slides a slid-out pane back.
    tab      = HitTestEdgeTab (ev.positionDip);
    edgePane = (tab >= 0) ? m_edgeTabs[(size_t) tab].pane : std::wstring();

    if (tab >= 0 && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        SlideOut (edgePane);
        return true;
    }

    if (tab >= 0 && ev.kind == DxuiMouseEventKind::Move && edgePane != m_slidPane)
    {
        SlideOut (edgePane);
    }

    if (ev.kind == DxuiMouseEventKind::Down && !m_slidPane.empty() && !Contains (m_slidRect, ev.positionDip))
    {
        SlideIn();
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
            RECT  strip = group->GetBounds();

            strip.bottom = std::min (strip.bottom, strip.top + (long) m_scaler.ToPx (DxuiTabGroup::kStripDip));

            if (Contains (strip, ev.positionDip))
            {
                (void) group->OnMouse (ev);
                return true;
            }
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up)
    {
        for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
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
//  DxuiDockSite::Paint
//
//  The strips, a seam down each sash, and during a drag the drop zones with
//  the hovered one's area shaded in the accent color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float                      line   = (float) std::max (1L, std::lround (m_scaler.ToPxf (1.0f)));
    const DxuiDockDropZone   * hover  = GetHoveredZone();
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

    if (!IsDragging())
    {
        return;
    }

    if (hover != nullptr)
    {
        fill (hover->preview, (theme.Accent() & 0x00FFFFFFu) | 0x50000000u);
    }

    for (const DxuiDockDropZone & zone : m_zones)
    {
        fill (zone.target, theme.ControlBackground());
        painter.OutlineRect ((float) zone.target.left, (float) zone.target.top,
                             (float) (zone.target.right - zone.target.left),
                             (float) (zone.target.bottom - zone.target.top),
                             line, (&zone == hover) ? theme.Accent() : theme.Border());
    }
}
