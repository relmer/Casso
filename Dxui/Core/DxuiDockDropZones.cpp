#include "Pch.h"

#include "Core/DxuiDockDropZones.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones::Build
//
//  The window's edge squares first, so a point near the middle of an edge
//  over a group's compass still finds the compass: HitTest takes the last
//  zone that holds the point.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockDropZone> DxuiDockDropZones::Build (const std::vector<DxuiPaneLayout::GroupRect> & groups,
                                                        const RECT & area, const std::wstring & pane)
{
    static constexpr DxuiDockSide  kSides[] = { DxuiDockSide::Left, DxuiDockSide::Top, DxuiDockSide::Right, DxuiDockSide::Bottom };
    std::vector<DxuiDockDropZone>  zones;
    long                           midX     = (area.left + area.right) / 2;
    long                           midY     = (area.top + area.bottom) / 2;
    long                           half     = kSquareDip / 2;
    long                           step     = kSquareDip + kGapDip;



    for (DxuiDockSide side : kSides)
    {
        DxuiDockDropZone  zone;
        long              x = (side == DxuiDockSide::Left)  ? area.left  + kEdgeDip + half
                            : (side == DxuiDockSide::Right) ? area.right - kEdgeDip - half : midX;
        long              y = (side == DxuiDockSide::Top)    ? area.top    + kEdgeDip + half
                            : (side == DxuiDockSide::Bottom) ? area.bottom - kEdgeDip - half : midY;

        zone.kind    = DxuiDockDropZone::Kind::Edge;
        zone.side    = side;
        zone.target  = MakeSquare (x, y);
        zone.preview = GetQuarter (area, side);
        zones.push_back (zone);
    }

    for (const DxuiPaneLayout::GroupRect & group : groups)
    {
        long              cx  = (group.rect.left + group.rect.right) / 2;
        long              cy  = (group.rect.top + group.rect.bottom) / 2;
        DxuiDockDropZone  tab;

        if (group.panes.size() == 1 && group.panes[0] == pane)
        {
            continue;
        }

        tab.kind       = DxuiDockDropZone::Kind::Tab;
        tab.targetPane = group.active;
        tab.target     = MakeSquare (cx, cy);
        tab.preview    = group.rect;
        zones.push_back (tab);

        for (DxuiDockSide side : kSides)
        {
            DxuiDockDropZone  zone;
            long              x = cx + ((side == DxuiDockSide::Left) ? -step : (side == DxuiDockSide::Right)  ? step : 0);
            long              y = cy + ((side == DxuiDockSide::Top)  ? -step : (side == DxuiDockSide::Bottom) ? step : 0);

            zone.kind       = DxuiDockDropZone::Kind::Side;
            zone.side       = side;
            zone.targetPane = group.active;
            zone.target     = MakeSquare (x, y);
            zone.preview    = GetHalf (group.rect, side);
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
//  DxuiDockDropZones::Apply
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockDropZones::Apply (const DxuiDockDropZone & zone, DxuiPaneLayout & layout, const std::wstring & pane)
{
    switch (zone.kind)
    {
    case DxuiDockDropZone::Kind::Side:
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
//  DxuiDockDropZones::MakeSquare
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockDropZones::MakeSquare (long centerX, long centerY)
{
    long  half = kSquareDip / 2;



    return RECT { centerX - half, centerY - half, centerX + half, centerY + half };
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
