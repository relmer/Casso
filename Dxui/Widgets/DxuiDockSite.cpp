#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





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
//  and showing its active one; every pane not in a shown group is hidden.
//  Tab groups are reused by position, so arranging again after a small change
//  keeps the controls that did not move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockSite::Arrange()
{
    std::vector<DxuiPaneLayout::GroupRect>    groups;
    std::unordered_set<const IDxuiControl *>  placed;



    if (m_boundsDip.right <= m_boundsDip.left || m_boundsDip.bottom <= m_boundsDip.top)
    {
        return;
    }

    groups      = m_layout.Arrange (m_boundsDip, m_shown, m_minSize);
    m_splits    = m_layout.ArrangeSplits (m_boundsDip, m_shown, m_minSize);
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
            placed.insert (found->second.content);
        }

        group->SetActive (active);
        group->Layout    (groups[i].rect, m_scaler);
    }

    for (const auto & pane : m_panes)
    {
        if (pane.second.content != nullptr && !placed.contains (pane.second.content))
        {
            pane.second.content->SetVisible (false);
        }
    }

    m_arranging = false;
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

    for (const std::unique_ptr<DxuiTabGroup> & group : m_groups)
    {
        group->SetIndicator (found->second.content, on);
    }
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
    bool  moved = m_layout.MoveByArrow (pane, direction, m_boundsDip, m_shown, m_minSize);



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
    m_zones     = DxuiDockDropZones::Build (m_layout.Arrange (m_boundsDip, m_shown, m_minSize), m_boundsDip, pane);
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

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
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
