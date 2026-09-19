#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZone
//
//  One place a dragged pane can be dropped: a side of a group, the group's
//  tab strip, or an edge of the window. `target` is where the guide square
//  is drawn and hit-tested; `preview` is the area the pane would take, which
//  is what the overlay shades while the pointer is over the square.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDockDropZone
{
    enum class Kind { Side, Tab, Edge };

    Kind          kind       = Kind::Tab;
    DxuiDockSide  side       = DxuiDockSide::Left;
    std::wstring  targetPane;
    RECT          target     = {};
    RECT          preview    = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones
//
//  The drop targets for one drag over one window, as Visual Studio shows
//  them: a compass of five squares at the middle of each group (four sides
//  and the tab), and a square at the middle of each window edge.
//
//  A group that holds only the pane being dragged offers no compass, since
//  every drop there would put the pane back where it is. Every zone's
//  meaning is a DxuiPaneLayout operation, applied by Apply.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockDropZones
{
public:
    static constexpr int  kSquareDip = 32;
    static constexpr int  kGapDip    = 4;
    static constexpr int  kEdgeDip   = 8;

    //  The zones for dragging `pane` over a window of `area` laid out as
    //  `groups`.
    static std::vector<DxuiDockDropZone>  Build (const std::vector<DxuiPaneLayout::GroupRect> & groups,
                                                 const RECT & area, const std::wstring & pane);

    //  The zone whose square holds the point, or none.
    static const DxuiDockDropZone *  HitTest (const std::vector<DxuiDockDropZone> & zones, POINT point);

    //  Carries the drop out; false when it changed nothing.
    static bool  Apply (const DxuiDockDropZone & zone, DxuiPaneLayout & layout, const std::wstring & pane);

private:
    static RECT  MakeSquare  (long centerX, long centerY);
    static RECT  GetHalf     (const RECT & rect, DxuiDockSide side);
    static RECT  GetQuarter  (const RECT & rect, DxuiDockSide side);
    static bool  Contains    (const RECT & rect, POINT point);
};
