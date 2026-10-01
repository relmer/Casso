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
//  bar where the pointer grabbed it. Or the bar floats in a window of its
//  own, at a place on the screen, in pixels.
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

    Edge   edge      = Edge::Top;
    int    offsetDip = 0;
    bool   floating  = false;
    POINT  floatPx   = {};

    bool  IsVertical () const { return !floating && (edge == Edge::Left || edge == Edge::Right); }

    bool  operator== (const CommandBarDock & other) const
    {
        return floating == other.floating &&
               (floating ? floatPx.x == other.floatPx.x && floatPx.y == other.floatPx.y
                         : edge == other.edge && offsetDip == other.offsetDip);
    }

    //  "top 0", "left 120": the edge, a space, then the offset. Floating,
    //  "float 300 -40": the window's screen position.
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

    //  Whether a pointer in client pixels is close enough to an edge of
    //  `area` to dock there: inside it and within `bandPx` of an edge. A drag
    //  that leaves the band tears the bar off to float, and a floating bar
    //  dropped inside it docks.
    static bool            IsInDockBand (POINT pointer, const RECT & area, int bandPx);

private:
    static CommandBarDock  ReadFloating (const std::wstring & text);
};
