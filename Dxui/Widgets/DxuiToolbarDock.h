#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock
//
//  Where a dockable toolbar sits: against one edge of its window, at
//  an offset along it from that edge's start (its left end across the top or
//  bottom, its top end down a side). The offset is in DIPs, so a saved place
//  holds across a DPI change.
//
//  A drop picks the edge nearest the pointer, and the offset that keeps the
//  bar where the pointer grabbed it. Or the bar floats in a window of its
//  own, at a place on the screen, in pixels.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiToolbarDock
{
    enum class Edge
    {
        Top,
        Bottom,
        Left,
        Right,
    };

    Edge   edge           = Edge::Top;
    int    offsetDip      = 0;
    bool   floating       = false;
    POINT  floatPx        = {};
    bool   floatVertical  = false;
    int    floatLengthDip = 0;      // a floating bar's length when it keeps one; 0 for its natural length

    bool  IsVertical () const { return !floating && (edge == Edge::Left || edge == Edge::Right); }

    bool  operator== (const DxuiToolbarDock & other) const
    {
        return floating == other.floating &&
               (floating ? floatPx.x == other.floatPx.x && floatPx.y == other.floatPx.y && floatVertical == other.floatVertical &&
                           floatLengthDip == other.floatLengthDip
                         : edge == other.edge && offsetDip == other.offsetDip);
    }

    //  "top 0", "left 120": the edge, a space, then the offset. Floating,
    //  "float 300 -40": the window's screen position, then " vertical"
    //  when it stands on end, then " length 640" when it keeps a length.
    std::wstring           ToText    () const;

    //  Text that does not read back gives the default place, across the top
    //  at its start.
    static DxuiToolbarDock  FromText  (const std::wstring & text);

    //  The place a drop at `pointer` gives, all in client pixels: the edge
    //  of `area` nearest it, the region the bar docks around (below the menu
    //  bar). `grab` is where the pointer was within the bar when the drag
    //  began, so the bar does not jump under it. The offset runs from the
    //  area's start and is never negative; the layout clamps its far end.
    static DxuiToolbarDock  PickForDrop (POINT pointer, POINT grab, const RECT & area, int dpi);

    //  The offset clamped so a bar of `barLength` fits an edge of
    //  `edgeLength`, both in pixels.
    static int             ClampOffset (int offsetPx, int edgeLength, int barLength);

    //  Whether a pointer in client pixels is close enough to an edge of
    //  `area` to dock there: inside it and within `bandPx` of an edge. A drag
    //  that leaves the band tears the bar off to float, and a floating bar
    //  dropped inside it docks.
    static bool            IsInDockBand (POINT pointer, const RECT & area, int bandPx);

    //  A drag of a docked bar: it stays on the edge it is on, however near
    //  another edge the pointer comes, and only the offset follows the
    //  pointer.
    static DxuiToolbarDock  SlideAlong (POINT pointer, POINT grab, const DxuiToolbarDock & current, const RECT & area, int dpi);

    //  Whether a drag of a bar docked on dge has pulled far enough away
    //  to tear it off: more than `bandPx + pullPx` in from that edge, or
    //  more than `pullPx` outside it or past either end of it.
    static bool            IsPulledOut (POINT pointer, Edge edge, const RECT & area, int bandPx, int pullPx);

    //  A floating bar's orientation as it is dragged: vertical within
    //  `bandPx` of a side of `area`, horizontal within it of the top or
    //  bottom, and `current` anywhere else.
    static bool            PickFloatVertical (POINT pointer, const RECT & area, int bandPx, bool current);

    //  Where a floating bar was grabbed, from its window's top left, as the
    //  grab a drag of the docked bar keeps: the distance along the bar, plus
    //  marginPx across the top or bottom, where the docked bar starts that
    //  far in from the window's edge. A bar standing on end measures down.
    static POINT           GrabForDocking (POINT grabPx, bool vertical, int marginPx);

    //  The edge the far end of a floating bar, the end away from its grab
    //  handle, is being dragged up to: the right edge of `area` for a bar
    //  lying down, the bottom for one standing up. It is when that end, at
    //  `toolbar` now and at `previous` a moment before, has moved toward the
    //  edge and come within `bandPx` of it from inside, with the bar across
    //  from that edge. A long bar held by its handle reaches an edge with
    //  that end long before the pointer could.
    static bool            TryGetFarEndEdge (const RECT & toolbar, const RECT & previous, bool vertical, const RECT & area, int bandPx, Edge & outEdge);

    //  A docked place on `edge`, `offsetPx` from the start of that edge.
    static DxuiToolbarDock  MakeDocked (Edge edge, int offsetPx, int dpi);

private:
    static DxuiToolbarDock  ReadFloating (const std::wstring & text);
};
