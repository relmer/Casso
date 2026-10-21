#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragModel
//
//  The geometry of a tab dragged along a strip, as File Explorer's tabs move:
//  the dragged tab is lifted out of the row, the other tabs close up, and a
//  gap as wide as the dragged tab opens where it would drop. The tabs never
//  trade places; the gap moves.
//
//  Positions are in the strip's own content coordinates, before scrolling.
//  `widths` lists every tab in order, the dragged one included; `dragged` is
//  its index, or -1 for a tab dragged in from another strip, which has no
//  entry. `originLeft` is where the first tab starts.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabDragModel
{
public:
    //  The slot the gap opens at, counted among the tabs that are not
    //  dragged: the number of them whose center lies left of the dragged
    //  tab's center, with the other tabs closed up. It depends on the
    //  pointer alone, so the gap cannot flicker between two slots.
    static int   GetGapIndex (const std::vector<int> & widths,
                              int                      dragged,
                              int                      originLeft,
                              int                      draggedLeft,
                              int                      draggedWidth);

    //  Where each tab goes with the gap at `gapIndex`, or closed up with -1.
    //  The dragged tab's entry is the gap's left edge (its packed place when
    //  the gap is closed). `outEndRight` is the right edge of the row, gap
    //  included, where the + button follows.
    static void  GetLefts    (const std::vector<int> & widths,
                              int                      dragged,
                              int                      originLeft,
                              int                      gapIndex,
                              int                      gapWidth,
                              std::vector<int>       & outLefts,
                              int                    & outEndRight);

    //  Whether a pointer has left the strip far enough to tear its tab off:
    //  more than `thresholdPx` past any edge.
    static bool  IsTornOff   (const RECT & strip, POINT point, int thresholdPx);
};
