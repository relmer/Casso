#pragma once

#include "Pch.h"
#include "Widgets/DxuiToolbarDock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands
//
//  The rows docked toolbars stand in along the edges of a window, as Visual
//  Studio's do. Each edge holds bands counted in from the edge: band 0 runs
//  along the edge itself, band 1 inside it, and so on. Bands across the top
//  and bottom run the window's whole width, less a margin at each end, and
//  bands down the sides run between them. A band is as thick as the thickest
//  toolbar in it, and each toolbar in it is centered across it.
//
//  The toolbars of a band stand side by side in order along it. One that
//  does not fill keeps its natural length, at its offset where there is
//  room; one that fills takes the length the others leave, and two that
//  fill in one band split that length equally. A band with a toolbar that
//  fills packs its toolbars end to end from the band's start.
//
//  All pure geometry, in pixels, for the dock group to lay out and to place
//  a drop with.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiToolbarBands
{
    using  Edge = DxuiToolbarDock::Edge;

    static constexpr size_t  kEdgeCount = 4;

    //  One toolbar: its place, natural length and thickness, and whether it
    //  fills. The order key, when there is one, is where along the band the
    //  toolbar is reckoned to be for its order among the others, measured
    //  from the edge's start like the offset; without one the offset is.
    struct Bar
    {
        Edge  edge        = Edge::Top;
        int   band        = 0;
        bool  floating    = false;
        int   offsetPx    = 0;
        int   lengthPx    = 0;
        int   thicknessPx = 0;
        bool  fills       = false;
        bool  hasOrderKey = false;
        int   orderKeyPx  = 0;
    };

    //  A band as laid out: its whole stretch along the edge, and the bars in
    //  it, as indexes into the bars given, in order along it.
    struct Band
    {
        Edge                 edge        = Edge::Top;
        int                  index       = 0;
        RECT                 rect        = {};
        int                  thicknessPx = 0;
        bool                 hasFill     = false;
        bool                 isInnermost = false;
        std::vector<size_t>  bars;
    };

    //  Every bar's rectangle (empty for one floating), every band, edge by
    //  edge from the edge in, how deep each edge's bands run, and the area
    //  they leave.
    struct Placement
    {
        std::vector<RECT>  bars;
        std::vector<Band>  bands;
        int                depthPx[kEdgeCount] = {};
        RECT               inner               = {};
    };

    //  Where a drop goes on an edge: into band `band`, or into a new band
    //  made at that index, which moves the bands from it on one further in.
    struct Target
    {
        int   band      = 0;
        bool  isNewBand = false;
    };

    //  Gives every docked place without a band one of its own inside the
    //  bands its edge already has, in the order given, so places saved
    //  before bands existed stand one inside the other as they did; then
    //  numbers each edge's bands 0, 1, 2 with no gaps, keeping their order.
    //  True when any place changed.
    static bool       AssignBands (const std::vector<DxuiToolbarDock *> & docks);

    //  Moves every docked place on `edge` in band `band` or further in one
    //  band further in, making room for a new band there.
    static void       InsertBand  (const std::vector<DxuiToolbarDock *> & docks, Edge edge, int band);

    //  Lays the bars out in bands along the edges of `area`: the top and
    //  bottom first, `marginPx` in from either end, then the sides between
    //  them. The bars must have bands.
    static Placement  Arrange     (const std::vector<Bar> & bars, const RECT & area, int marginPx);

    //  Where a pointer `acrossPx` in from an edge drops a toolbar, given how
    //  thick that edge's bands are, from the edge in. Within `splitPx` of a
    //  boundary between two bands, or of the outer or inner side of them all,
    //  it makes a new band there; anywhere else it joins the band it is over.
    //  A toolbar already alone in band `ownBand` stays there rather than
    //  making a new band either side of its own.
    static Target     PickTarget  (int acrossPx, const std::vector<int> & thicknessesPx, int ownBand, bool isOwnAlone, int splitPx);

    //  How far in from `edge` of `area` a point is; negative outside it.
    static int        GetAcrossPx (POINT pointer, Edge edge, const RECT & area);

    //  The edge a pointer inside `area` is near enough to dock on: within
    //  `reachPx` past the inner side of that edge's bands. Of two, the one
    //  whose bands it is nearer the inner side of, then the one it is
    //  nearer, then the top, bottom, left and right in that order.
    static bool       TryPickEdge (POINT pointer, const RECT & area, const int (&depthPx)[kEdgeCount], int reachPx, Edge & outEdge);

    //  The area an edge's offsets run in: the whole area for the top and
    //  bottom, and for the sides the stretch between the top and bottom
    //  bands.
    static RECT       GetEdgeArea (Edge edge, const RECT & area, const int (&depthPx)[kEdgeCount]);

    //  The thickness of each of an edge's bands, from the edge in.
    static std::vector<int>  GetThicknesses (const Placement & placement, Edge edge);

private:
    static void  ArrangeEdge (const std::vector<Bar> & bars, Edge edge, int marginPx, Placement & placement);
    static void  PlaceAlong  (const std::vector<Bar> & bars, const std::vector<size_t> & members, int lengthPx, std::vector<int> & outPos, std::vector<int> & outLength);
    static RECT  GetBandRect (Edge edge, const RECT & inner, int acrossPx, int thicknessPx, int startPx, int lengthPx);
};
