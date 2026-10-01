#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZone
//
//  One place a dragged pane can be dropped: a side of a group, the group's
//  tab strip, a new tab group beside a document group, or an edge of the
//  window. `target` is where the guide square
//  is drawn and hit-tested; `preview` is the area the pane would take, which
//  is what the overlay shades while the pointer is over the square.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDockDropZone
{
    enum class Kind { Side, Tab, Edge, Split };

    Kind          kind       = Kind::Tab;
    DxuiDockSide  side       = DxuiDockSide::Left;
    std::wstring  targetPane;
    RECT          target     = {};
    RECT          preview    = {};
    bool          besideWell = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones
//
//  The drop targets for one drag over one window, as Visual Studio shows
//  them: a compass of five squares at the middle of each tool window group
//  (four sides and the tab), and a square at the middle of each window edge.
//  A document group's compass is larger: between its tab square and its
//  four side squares is an inner ring of four split squares, drawn dotted,
//  each making a new tab group on that side of the documents.
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

    using GroupTestFn = std::function<bool (const DxuiPaneLayout::GroupRect & group)>;

    //  The zones for dragging `pane` over a window of `area` laid out as
    //  `groups`; `isDocument` picks the groups that get the larger compass.
    static std::vector<DxuiDockDropZone>  Build (const std::vector<DxuiPaneLayout::GroupRect> & groups,
                                                 const RECT & area, const std::wstring & pane,
                                                 const GroupTestFn & isDocument = nullptr);

    //  The zone whose square holds the point, or none.
    static const DxuiDockDropZone *  HitTest (const std::vector<DxuiDockDropZone> & zones, POINT point);

    //  Carries the drop out; false when it changed nothing.
    //  A side square of a document group's outer ring docks beside the whole
    //  document well, which isDocument picks out; an inner split square
    //  makes a new tab group beside the one group.
    static bool  Apply (const DxuiDockDropZone & zone, DxuiPaneLayout & layout, const std::wstring & pane,
                        const DxuiPaneLayout::GroupFn & isDocument = nullptr);

private:
    static RECT  MakeSquare  (long centerX, long centerY);
    static RECT  GetHalf     (const RECT & rect, DxuiDockSide side);
    static RECT  GetQuarter  (const RECT & rect, DxuiDockSide side);
    static bool  Contains    (const RECT & rect, POINT point);
};
