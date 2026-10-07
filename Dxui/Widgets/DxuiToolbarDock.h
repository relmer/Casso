#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDock
//
//  Where a dockable toolbar sits: against one edge of its window, in one of
//  the bands along that edge (band 0 against the edge itself, band 1 inside
//  it, and so on), at an offset along it from that edge's start (its left
//  end across the top or bottom, its top end down a side). The offset is in
//  DIPs, so a saved place holds across a DPI change. A place with no band,
//  as every place saved before bands existed, is given one when it is laid
//  out: a band of its own inside any the edge already has.
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

    static constexpr int  kNoBand = -1;

    Edge   edge           = Edge::Top;
    int    offsetDip      = 0;
    int    band           = kNoBand;
    bool   floating       = false;
    POINT  floatPx        = {};
    bool   floatVertical  = false;
    int    floatLengthDip = 0;      // a floating bar's length when it keeps one; 0 for its natural length

    bool  IsVertical () const { return !floating && (edge == Edge::Left || edge == Edge::Right); }
    bool  HasBand    () const { return band >= 0; }

    bool  operator== (const DxuiToolbarDock & other) const
    {
        return floating == other.floating &&
               (floating ? floatPx.x == other.floatPx.x && floatPx.y == other.floatPx.y && floatVertical == other.floatVertical &&
                           floatLengthDip == other.floatLengthDip
                         : edge == other.edge && offsetDip == other.offsetDip && band == other.band);
    }

    //  "top 0", "left 120": the edge, a space, then the offset, then
    //  " band 1" when the place has a band. Floating, "float 300 -40": the
    //  window's screen position, then " vertical" when it stands on end,
    //  then " length 640" when it keeps a length.
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

    //  The same drop's place on a given edge: the offset alone follows the
    //  pointer, and the band is left for the caller to give.
    static DxuiToolbarDock  PickForDropOn (Edge edge, POINT pointer, POINT grab, const RECT & area, int dpi);

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


    //  A docked place on `edge`, `offsetPx` from the start of that edge.
    static DxuiToolbarDock  MakeDocked (Edge edge, int offsetPx, int dpi);

private:
    static DxuiToolbarDock  ReadFloating (const std::wstring & text);
    static bool             TryReadBand  (const wchar_t * text, int & outBand);
};
