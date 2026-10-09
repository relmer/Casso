#include "Pch.h"

#include "Core/DxuiDockDropZones.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::Build
//
//  The window's edge guides first, so a point near the middle of an edge
//  over a group's cross still finds the cross: HitTest takes the last zone
//  that holds the point. Each edge guide's box lies kEdgeDip in from its
//  edge of the area, centered along it, rounding down.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockDropZone> DxuiDockDropZones::Build (const std::vector<DxuiPaneLayout::GroupRect> & groups,
                                                        const RECT & area, const std::wstring & pane,
                                                        const DxuiDpiScaler & scaler,
                                                        const GroupTestFn & isDocument)
{
    static constexpr DxuiDockSide  kSides[] = { DxuiDockSide::Left, DxuiDockSide::Top, DxuiDockSide::Right, DxuiDockSide::Bottom };
    std::vector<DxuiDockDropZone>  zones;
    long                           midX     = (area.left + area.right) / 2;
    long                           midY     = (area.top + area.bottom) / 2;
    long                           inset    = scaler.ToPx (kEdgeDip);
    SIZE                           box      = DxuiDockGuide::GetSizePx (DxuiDockGuideKind::Edge, scaler);
    RECT                           well     = {};
    bool                           hasWell  = false;



    //  The document well: every document group's area together.
    for (const DxuiPaneLayout::GroupRect & group : groups)
    {
        if (isDocument && isDocument (group))
        {
            well    = hasWell ? RECT { std::min (well.left, group.rect.left), std::min (well.top, group.rect.top),
                                       std::max (well.right, group.rect.right), std::max (well.bottom, group.rect.bottom) }
                              : group.rect;
            hasWell = true;
        }
    }

    for (DxuiDockSide side : kSides)
    {
        DxuiDockDropZone  zone;
        POINT             origin = { (side == DxuiDockSide::Left)   ? area.left   + inset
                                   : (side == DxuiDockSide::Right)  ? area.right  - inset - box.cx : midX - box.cx / 2,
                                     (side == DxuiDockSide::Top)    ? area.top    + inset
                                   : (side == DxuiDockSide::Bottom) ? area.bottom - inset - box.cy : midY - box.cy / 2 };

        zone.kind    = DxuiDockDropZone::Kind::Edge;
        zone.side    = side;
        zone.target  = DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::Edge, DxuiDockGuide::GetDockButton (side), origin, scaler);
        zone.preview = GetQuarter (area, side);
        zones.push_back (zone);
    }

    for (size_t index = 0; index < groups.size(); index++)
    {
        const DxuiPaneLayout::GroupRect  & group    = groups[index];
        bool                               document = isDocument && isDocument (group);
        DxuiDockGuideKind                  kind     = document ? DxuiDockGuideKind::LargeCross : DxuiDockGuideKind::SmallCross;
        POINT                              center   = { (group.rect.left + group.rect.right) / 2, (group.rect.top + group.rect.bottom) / 2 };
        POINT                              origin   = DxuiDockGuide::GetOrigin (kind, center, scaler);
        DxuiDockDropZone                   base;
        DxuiDockDropZone                   tab;

        if (group.panes.size() == 1 && group.panes[0] == pane)
        {
            continue;
        }

        base.targetPane = group.active;
        base.group      = (int) index;
        base.groupRect  = group.rect;

        tab         = base;
        tab.kind    = DxuiDockDropZone::Kind::Tab;
        tab.target  = DxuiDockGuide::GetButtonRect (kind, DxuiDockGuideButton::Center, origin, scaler);
        tab.preview = group.rect;
        zones.push_back (tab);

        for (DxuiDockSide side : kSides)
        {
            DxuiDockDropZone  zone  = base;
            DxuiDockDropZone  split = base;

            if (document)
            {
                split.kind    = DxuiDockDropZone::Kind::Split;
                split.side    = side;
                split.target  = DxuiDockGuide::GetButtonRect (kind, DxuiDockGuide::GetSplitButton (side), origin, scaler);
                split.preview = GetHalf (group.rect, side);
                zones.push_back (split);
            }

            zone.kind       = DxuiDockDropZone::Kind::Side;
            zone.side       = side;
            zone.target     = DxuiDockGuide::GetButtonRect (kind, DxuiDockGuide::GetDockButton (side), origin, scaler);
            zone.preview    = GetHalf (document ? well : group.rect, side);
            zone.besideWell = document;
            zones.push_back (zone);
        }
    }

    return zones;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::HitTest
//
////////////////////////////////////////////////////////////////////////////////

const DxuiDockDropZone * DxuiDockDropZones::HitTest (const std::vector<DxuiDockDropZone> & zones, POINT point)
{
    const DxuiDockDropZone  * hit = nullptr;



    for (const DxuiDockDropZone & zone : zones)
    {
        hit = Contains (zone.target, point) ? &zone : hit;
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::GetGuideButton
//
//  A tab drop is a cross's center button, a split its split button on that
//  side, and a side or an edge drop the dock button on that side.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockGuideButton DxuiDockDropZones::GetGuideButton (const DxuiDockDropZone & zone)
{
    switch (zone.kind)
    {
    case DxuiDockDropZone::Kind::Split: return DxuiDockGuide::GetSplitButton (zone.side);
    case DxuiDockDropZone::Kind::Side:  return DxuiDockGuide::GetDockButton  (zone.side);
    case DxuiDockDropZone::Kind::Edge:  return DxuiDockGuide::GetDockButton  (zone.side);
    default:                            return DxuiDockGuideButton::Center;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::Apply
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDropZones::Apply (const DxuiDockDropZone & zone, DxuiPaneLayout & layout, const std::wstring & pane,
                               const DxuiPaneLayout::GroupFn & isDocument)
{
    switch (zone.kind)
    {
    case DxuiDockDropZone::Kind::Side:
        return zone.besideWell ? layout.DockBesideWell (pane, zone.targetPane, zone.side, isDocument)
                               : layout.DockToSide     (pane, zone.targetPane, zone.side);

    case DxuiDockDropZone::Kind::Split:
        return layout.DockToSide (pane, zone.targetPane, zone.side);

    case DxuiDockDropZone::Kind::Tab:
        return layout.TabWith (pane, zone.targetPane);

    case DxuiDockDropZone::Kind::Edge:
        return layout.DockToEdge (pane, zone.side);

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::GetHalf
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockDropZones::GetHalf (const RECT & rect, DxuiDockSide side)
{
    RECT  half = rect;
    long  midX = (rect.left + rect.right) / 2;
    long  midY = (rect.top + rect.bottom) / 2;



    switch (side)
    {
    case DxuiDockSide::Left:   half.right  = midX; break;
    case DxuiDockSide::Right:  half.left   = midX; break;
    case DxuiDockSide::Top:    half.bottom = midY; break;
    case DxuiDockSide::Bottom: half.top    = midY; break;
    default:                                       break;
    }

    return half;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::GetQuarter
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockDropZones::GetQuarter (const RECT & rect, DxuiDockSide side)
{
    RECT  quarter = rect;
    long  width   = (rect.right - rect.left) / 4;
    long  height  = (rect.bottom - rect.top) / 4;



    switch (side)
    {
    case DxuiDockSide::Left:   quarter.right  = rect.left   + width;  break;
    case DxuiDockSide::Right:  quarter.left   = rect.right  - width;  break;
    case DxuiDockSide::Top:    quarter.bottom = rect.top    + height; break;
    case DxuiDockSide::Bottom: quarter.top    = rect.bottom - height; break;
    default:                                                          break;
    }

    return quarter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDropZones::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}
