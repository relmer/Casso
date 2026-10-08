#pragma once

#include "Pch.h"
#include "Core/DxuiDockGuide.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZone
//
//  One place a dragged pane can be dropped: a side of a group, the group's
//  tab strip, a new tab group beside a document group, or an edge of the
//  window. `target` is the guide button that is drawn and hit-tested;
//  `preview` is the area the pane would take, which is what the overlay
//  shades while the pointer is over the button.
//
//  A zone of a group's cross keeps that group's index in the list Build was
//  given, in `group`, and its area, in `groupRect`, so the site can show the
//  cross of the group under the pointer alone. An edge zone has no group.
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
    int           group      = -1;
    RECT          groupRect  = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZones
//
//  The drop targets for one drag over one window, as Visual Studio shows
//  them: a small cross of five buttons at the middle of each tool window
//  group (four sides and the tab), and a guide of one button at the middle
//  of each window edge. A document group's cross is larger: between its tab
//  button and its four side buttons is an inner ring of four split buttons,
//  each making a new tab group on that side of the documents. Each target
//  is the button DxuiDockGuide draws, so what is hit is what is seen.
//
//  A group that holds only the pane being dragged offers no cross, since
//  every drop there would put the pane back where it is. Every zone's
//  meaning is a DxuiPaneLayout operation, applied by Apply.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockDropZones
{
public:
    //  How far an edge guide's button lies in from the window's edge.
    static constexpr int  kEdgeDip = 8;

    using GroupTestFn = std::function<bool (const DxuiPaneLayout::GroupRect & group)>;

    //  The zones for dragging `pane` over a window of `area` laid out as
    //  `groups`; `isDocument` picks the groups that get the larger cross.
    static std::vector<DxuiDockDropZone>  Build (const std::vector<DxuiPaneLayout::GroupRect> & groups,
                                                 const RECT & area, const std::wstring & pane,
                                                 const DxuiDpiScaler & scaler,
                                                 const GroupTestFn & isDocument = nullptr);

    //  The zone whose button holds the point, or none.
    static const DxuiDockDropZone *  HitTest (const std::vector<DxuiDockDropZone> & zones, POINT point);

    //  The guide button a zone's target is.
    static DxuiDockGuideButton  GetGuideButton (const DxuiDockDropZone & zone);

    //  Makes the drop; false when it changed nothing.
    //  A side button of a document group's outer ring docks beside the whole
    //  document well, which isDocument picks out; an inner split button
    //  makes a new tab group beside the one group.
    static bool  Apply (const DxuiDockDropZone & zone, DxuiPaneLayout & layout, const std::wstring & pane,
                        const DxuiPaneLayout::GroupFn & isDocument = nullptr);

private:
    static RECT  GetHalf     (const RECT & rect, DxuiDockSide side);
    static RECT  GetQuarter  (const RECT & rect, DxuiDockSide side);
    static bool  Contains    (const RECT & rect, POINT point);
};
