#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDock
//
//  Where the debugger's command bar sits: against one edge of the window, at
//  an offset along it from that edge's start (its left end across the top or
//  bottom, its top end down a side). The offset is in DIPs, so a saved place
//  holds across a DPI change.
//
//  A drop picks the edge nearest the pointer, and the offset that keeps the
//  bar where the pointer grabbed it.
//
////////////////////////////////////////////////////////////////////////////////

struct CommandBarDock
{
    enum class Edge
    {
        Top,
        Bottom,
        Left,
        Right,
    };

    Edge  edge      = Edge::Top;
    int   offsetDip = 0;

    bool  IsVertical () const { return edge == Edge::Left || edge == Edge::Right; }

    bool  operator== (const CommandBarDock & other) const = default;

    //  "top 0", "left 120": the edge, a space, then the offset.
    std::wstring           ToText    () const;

    //  Text that does not read back gives the default place, across the top
    //  at its start.
    static CommandBarDock  FromText  (const std::wstring & text);

    //  The place a drop at `pointer` gives, all in client pixels: the edge
    //  of `area` nearest it, the region the bar docks around (below the menu
    //  bar). `grab` is where the pointer was within the bar when the drag
    //  began, so the bar does not jump under it. The offset runs from the
    //  area's start and is never negative; the layout clamps its far end.
    static CommandBarDock  PickForDrop (POINT pointer, POINT grab, const RECT & area, int dpi);

    //  The offset clamped so a bar of `barLength` fits an edge of
    //  `edgeLength`, both in pixels.
    static int             ClampOffset (int offsetPx, int edgeLength, int barLength);
};
