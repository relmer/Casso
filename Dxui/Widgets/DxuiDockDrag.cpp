#include "Pch.h"

#include "DxuiDockDrag.h"
#include "IDxuiDockDragHost.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiPaneMetrics.h"
#include "Render/DxuiCoverageRaster.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiDockGaps.h"
#include "Widgets/DxuiDockGroups.h"
#include "Widgets/DxuiDockPanes.h"
#include "Widgets/DxuiTabGroup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDrag::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::Cancel
//
//  The drop targets go, and so does the gap a hovered strip opened for the
//  tab; the layout stays as the drag found it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::Cancel()
{
    ClearStripTarget();
    m_pane.clear();
    m_panes.clear();
    m_zones.clear();
    m_hoverZone     = -1;
    m_compassGroup  = -1;
    m_isPointerDrag = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::Begin
//
//  A group dragged whole offers no zone on itself, since every drop there
//  would put it back where it is. The edge guides lie along the docked
//  area, the margin around the panes included, and each target's shade is
//  worked out now, while the layout is as the drag found it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::Begin (const std::vector<std::wstring> & panes, const std::wstring & active)
{
    std::vector<DxuiPaneLayout::GroupRect>  groups;
    long                                    gap     = m_host.GetDragGapPx();
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
    m_host.SlideInForDrag (active);

    //  The groups as Arrange lays them out, each less its share of the gaps,
    //  so a group's cross sits on the middle of the pane as it is drawn.
    groups = m_host.ArrangeForDrag (m_layout);

    for (DxuiPaneLayout::GroupRect & group : groups)
    {
        group.rect = DxuiDockGaps::GetInsetForGap (group.rect, m_host.GetDragPaneArea(), gap);
    }

    m_pane     = active;
    m_panes    = panes;
    m_zones        = DxuiDockDropZones::Build (groups, m_host.GetDragDockedArea(), active, m_scaler,
                                               [this] (const DxuiPaneLayout::GroupRect & group) { return m_host.IsDragDocumentGroup (group.panes); });
    m_hoverZone    = -1;
    m_compassGroup = -1;
    m_isPointerDrag  = false;

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
//  DxuiDockDrag::UpdateStripTarget
//
//  The group whose tabs or title bar lie under the pointer takes the drop,
//  opening a gap where it would land. A group the drag would leave as it is
//  -- the dragged group itself, or a pane's own group of one -- takes none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::UpdateStripTarget (POINT pointDip)
{
    int  found = -1;
    int  index = -1;



    for (size_t i = 0; i < m_groups.GetCount() && found < 0; i++)
    {
        DxuiTabGroup  * group  = m_groups.Get (i);
        size_t          inside = 0;

        if (!group->IsVisible() || !group->IsChromeAt (pointDip))
        {
            continue;
        }

        for (int t = 0; t < (int) group->GetTabCount(); t++)
        {
            inside += std::find (m_panes.begin(), m_panes.end(), m_sitePanes.GetPaneOf (group->GetContent (t))) != m_panes.end() ? 1 : 0;
        }

        if (inside == group->GetTabCount() || (inside > 0 && m_panes.size() > 1))
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
        m_groups.Get ((size_t) found)->SetInsertGap (index, m_scaler.ToPx (kInsertGapDip));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::ClearStripTarget
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::ClearStripTarget()
{
    for (size_t i = 0; i < m_groups.GetCount(); i++)
    {
        m_groups.Get (i)->SetInsertGap (-1, 0);
    }

    m_stripGroup = -1;
    m_stripIndex = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::DropOnStrip
//
//  The dragged panes go into the group ahead of the tab at `index`, in their
//  order, and the pane dragged is made the active tab. `index` counts the
//  visible tabs, while the layout's group also has the hidden panes, so the
//  drop goes ahead of that tab's pane, or after the last tab's, wherever it
//  is in the whole group.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDrag::DropOnStrip (int group, int index)
{
    DxuiTabGroup               * target  = (group >= 0 && group < (int) m_groups.GetCount()) ? m_groups.Get ((size_t) group) : nullptr;
    DxuiPaneLayout             & layout  = m_layout;
    std::wstring                 anchor;
    std::wstring                 edge;
    std::vector<std::wstring>    panes;
    bool                         changed = false;
    int                          tabs    = 0;
    int                          at      = 0;



    if (target == nullptr)
    {
        return false;
    }

    for (int t = 0; t < (int) target->GetTabCount() && anchor.empty(); t++)
    {
        std::wstring  pane = m_sitePanes.GetPaneOf (target->GetContent (t));

        if (std::find (m_panes.begin(), m_panes.end(), pane) == m_panes.end())
        {
            anchor = pane;
        }
    }

    if (anchor.empty())
    {
        return false;
    }

    tabs  = (int) target->GetTabCount();
    edge  = m_sitePanes.GetPaneOf (target->GetContent (std::clamp (index, 0, tabs - 1)));
    panes = layout.GetGroup (anchor);
    at    = (int) (std::find (panes.begin(), panes.end(), edge) - panes.begin()) + ((index >= tabs) ? 1 : 0);

    for (const std::wstring & pane : m_panes)
    {
        changed = layout.TabWithAt (pane, anchor, at) || changed;
        panes   = layout.GetGroup (anchor);
        at      = (int) (std::find (panes.begin(), panes.end(), pane) - panes.begin()) + 1;
    }

    return layout.Activate (m_pane) || changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::DropOnZone
//
//  The zone's operation moves the first pane; the rest follow it into its
//  new group, in order. A drop is made on the site's own layout, and on a
//  copy of it to see where the panes would go.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDrag::DropOnZone (
    const DxuiDockDropZone  & zone,
    DxuiPaneLayout          & layout) const
{
    std::vector<std::wstring>  panes;
    int                        at      = 0;



    if (m_panes.empty() || !DxuiDockDropZones::Apply (zone, layout, m_panes.front(),
                                                                  [this] (const std::vector<std::wstring> & panes) { return m_host.IsDragDocumentGroup (panes); }))
    {
        return false;
    }

    for (size_t i = 1; i < m_panes.size(); i++)
    {
        panes   = layout.GetGroup (m_panes.front());
        at      = (int) (std::find (panes.begin(), panes.end(), m_panes[i - 1]) - panes.begin()) + 1;
        (void) layout.TabWithAt (m_panes[i], m_panes.front(), at);
    }

    (void) layout.Activate (m_pane);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::SetDropPreview
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

void DxuiDockDrag::SetDropPreview (DxuiDockDropZone & zone) const
{
    DxuiPaneLayout                          layout = m_layout;
    std::vector<DxuiPaneLayout::GroupRect>  groups;
    RECT                                    area   = m_host.GetDragPaneArea();
    RECT                                    docked = m_host.GetDragDockedArea();
    long                                    gap    = m_host.GetDragGapPx();
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

    groups = m_host.ArrangeForDrag (layout);

    for (const DxuiPaneLayout::GroupRect & group : groups)
    {
        const RECT  & r = group.rect;

        if (std::find (group.panes.begin(), group.panes.end(), m_panes.front()) == group.panes.end())
        {
            continue;
        }

        zone.preview = DxuiDockGaps::GetInsetForGap (r, area, gap);

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
//  DxuiDockDrag::SetTabDropPreview
//
//  A tab drop's shade, as Visual Studio draws it: the target pane up to its
//  tabs, short of the line between them, and a tab kPreviewTabDip wide at
//  the start of the band, over the band and that line. The rows are where a
//  group of the target's kind puts its tabs, so a tool window showing no
//  tabs yet shows the strip the drop gives it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::SetTabDropPreview (DxuiDockDropZone & zone) const
{
    const DxuiTabGroup  * target = m_groups.FindGroupOf (zone.targetPane);
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
//  DxuiDockDrag::UpdateCompass
//
//  The one cross a drag shows is the cross of the group under the pointer.
//  It stays while the pointer is anywhere on it, so a cross wider than its
//  group's pane can still be used to its ends; off it, the group under the
//  pointer takes the cross, and outside every group none shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::UpdateCompass (POINT pointDip)
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
//  DxuiDockDrag::Track
//
//  What a drag targets with the pointer at a point: a button of a guide on
//  show, or else a group's tabs or title bar. Off the docked area -- over a
//  menu, a toolbar or a status bar, or out of the window -- the drag targets
//  nothing, so the cross and the shade go at once and only the edge guides
//  stay, even where a cross reaches past the area.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::Track (POINT pointDip)
{
    if (!Contains (m_host.GetDragDockedArea(), pointDip))
    {
        ClearTarget();
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
//  DxuiDockDrag::ClearTarget
//
//  The cross, the hovered button and the strip under the pointer, gone; the
//  edge guides stay for as long as the drag does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::ClearTarget()
{
    m_compassGroup = -1;
    m_hoverZone    = -1;
    ClearStripTarget();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::HitTestShown
//
//  As DxuiDockDropZones::HitTest, the last zone holding the point, but only
//  among the zones on show: the edge guides and the one cross.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockDrag::HitTestShown (POINT pointDip) const
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
//  DxuiDockDrag::TryGetCompass
//
//  A group's cross, as Build placed it: large when the group has split
//  zones, centered on the group. False when the group has no zones.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDrag::TryGetCompass (int group, DxuiDockGuideKind & kind, POINT & origin) const
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
//  DxuiDockDrag::GetGuideImage
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

std::shared_ptr<const DxuiIconImage> DxuiDockDrag::GetGuideImage (
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
//  DxuiDockDrag::End
//
//  A drop on a zone is that zone's operation; a drop outside the site asks
//  for the pane to float there; a drop anywhere else changes nothing. The
//  zone is found as a move to the drop point would have shown it, so a drop
//  with no move before it still lands in the group under the point.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDrag::End (POINT pointDip)
{
    const DxuiDockDropZone           * hit     = nullptr;
    std::optional<DxuiDockDropZone>    zone;
    std::wstring                       pane    = m_pane;
    bool                               changed = false;
    int                                strip   = -1;
    int                                index   = -1;



    Track (pointDip);
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
    m_isPointerDrag  = false;

    if (pane.empty())
    {
        m_panes.clear();
        return false;
    }

    //  A group dropped outside floats only the pane shown. With a group
    //  tear-off handler a group's title bar floats the whole group before
    //  any drag starts here (TearOffGroup), so this is the drag of a site
    //  without one.
    if (zone.has_value())
    {
        changed = DropOnZone (*zone, m_layout);
    }
    else if (strip >= 0)
    {
        changed = DropOnStrip (strip, index);
    }
    else
    {
        m_host.RequestDragFloat (pane, pointDip);
    }

    m_pane.clear();
    m_panes.clear();

    if (changed)
    {
        m_host.CommitDragDrop();
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::GetHoveredZone
//
////////////////////////////////////////////////////////////////////////////////

const DxuiDockDropZone * DxuiDockDrag::GetHoveredZone() const
{
    return (m_hoverZone >= 0 && m_hoverZone < (int) m_zones.size()) ? &m_zones[(size_t) m_hoverZone] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag::GetMarks
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

std::vector<DxuiDockDragMark> DxuiDockDrag::GetMarks (const IDxuiTheme & theme) const
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

    if (m_stripGroup >= 0 && m_stripGroup < (int) m_groups.GetCount())
    {
        marks.push_back ({ m_groups.Get ((size_t) m_stripGroup)->GetBodyRect(),      preview,                                0 });
        marks.push_back ({ m_groups.Get ((size_t) m_stripGroup)->GetInsertGapRect(), (theme.Accent() & kColor) | kGapTint, 0 });
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
//  DxuiDockDrag::GetOutlineStrips
//
//  The filled rectangles that draw an outlined mark: four strips along the
//  inside of its rectangle, or for a dotted one a square dot every other
//  dot's width along each side.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> DxuiDockDrag::GetOutlineStrips (const DxuiDockDragMark & mark)
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
//  DxuiDockDrag::PaintMarks
//
//  Each mark, first to last: a fill, a dotted outline drawn as its strips,
//  a solid outline, or a guide's picture.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockDrag::PaintMarks (IDxuiPainter & painter, IDxuiTextRenderer & text, const std::vector<DxuiDockDragMark> & marks)
{
    HRESULT  hr   = S_OK;
    auto     fill = [&] (const RECT & r, uint32_t argb)
    {
        painter.FillRect ((float) r.left, (float) r.top,
                          (float) (r.right - r.left), (float) (r.bottom - r.top), argb);
    };



    for (const DxuiDockDragMark & mark : marks)
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
}