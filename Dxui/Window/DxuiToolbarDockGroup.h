#pragma once

#include "Pch.h"
#include "Widgets/DxuiToolbarBands.h"
#include "Widgets/DxuiToolbarDock.h"

class DxuiToolbarHost;
class DxuiDpiScaler;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup
//
//  The dockable toolbars of one window, laid out together in bands along its
//  edges (DxuiToolbarBands), so any of them can share a band with another or
//  stand in a band of its own. A host belongs to a group of its own until it
//  joins its window's; the order hosts join in is the order places saved
//  before bands existed stand in, the first against the edge.
//
//  The owner calls Layout from its own layout, in place of each host's. The
//  hosts ask the group, while one is dragged, where its edge's bands are,
//  so a drop goes into the band under the pointer or makes a new band
//  between two, and once a drag has put a toolbar down the group saves
//  every host's place, since a new band moves its neighbors in.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarDockGroup
{
public:
    using  Edge = DxuiToolbarDock::Edge;

    //  How near a boundary between bands, in DIPs, a drop makes a new band
    //  there rather than joining the band it is over.
    static constexpr int  kSplitDp = 8;

    DxuiToolbarDockGroup  () = default;

    DxuiToolbarDockGroup             (const DxuiToolbarDockGroup &) = delete;
    DxuiToolbarDockGroup & operator= (const DxuiToolbarDockGroup &) = delete;

    void  Add    (DxuiToolbarHost * host);
    void  Remove (DxuiToolbarHost * host);

    //  Lays out every toolbar of the group against `area`, in client pixels,
    //  and returns the area the bands leave. The innermost band on its edge
    //  that holds a toolbar with a dock site, and no toolbar that fills, is
    //  left in the area and shared with that site's auto-hidden tabs.
    RECT  Layout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler);

    //  From the last layout: the area it was given, how deep an edge's bands
    //  run, the area that edge's offsets run in, its bands' thicknesses from
    //  the edge in, a band's whole stretch, whether a host's toolbar is the
    //  only one in its band, and the whole placement.
    const RECT                         &  GetArea            () const { return m_area; }
    int                                   GetDepthPx         (Edge edge) const;
    RECT                                  GetEdgeArea        (Edge edge) const;
    std::vector<int>                      GetBandThicknesses (Edge edge) const;
    RECT                                  GetBandRect        (Edge edge, int band) const;
    bool                                  IsAloneInBand      (const DxuiToolbarHost * host) const;
    const DxuiToolbarBands::Placement  &  GetPlacement       () const { return m_placement; }

    //  A new band at `band` on `edge`: every other host's place there or
    //  further in moves one band in.
    void  InsertBand (Edge edge, int band, const DxuiToolbarHost * mover);

    //  A drag has put `mover` down: the toolbars of its band keep the places
    //  they were laid out at, and every host's place is saved.
    void  Commit  (const DxuiToolbarHost * mover);
    void  SaveAll ();

private:
    //  A host and where its toolbar was laid out last, which gives its order
    //  in its band while another toolbar is dragged along it.
    struct Member
    {
        DxuiToolbarHost  * host   = nullptr;
        bool               placed = false;
        Edge               edge   = Edge::Top;
        int                band   = 0;
        RECT               rect   = {};
    };

    DxuiToolbarBands::Bar  MakeBar        (Member & member, const DxuiDpiScaler & scaler);
    void                   ShareEdge      (RECT & inner);
    int                    GetEdgeStartPx (Edge edge) const;

    std::vector<Member>            m_members;
    DxuiToolbarBands::Placement    m_placement;
    RECT                           m_area     = {};
    int                            m_marginPx = 0;
    int                            m_dpi      = USER_DEFAULT_SCREEN_DPI;
};
