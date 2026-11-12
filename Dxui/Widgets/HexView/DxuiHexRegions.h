#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"

class DxuiHexView;
class IDxuiPainter;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions
//
//  The outlines around the source's regions: which region each value is in,
//  the lanes that open between rows where an outline's edge runs, each row's
//  top with those lanes, the label each region's top edge has, and the
//  outlines themselves.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexRegions
{
public:
    explicit DxuiHexRegions (DxuiHexView & view);

    //  A region's top edge segment that has its label: the row whose top it
    //  runs along and its first and last value. The first row's segment when
    //  the label fits it, otherwise the second row's when that one does,
    //  otherwise the longer.
    struct LabelSegment
    {
        uint64_t  row   = 0;
        int       first = 0;
        int       last  = 0;
    };

    //  Edge lanes, the space each side of a label, and the length of edge
    //  drawn before a label that cannot be centered on its segment, in DIPs.
    static constexpr int    kEdgeLaneDip  = 6;
    static constexpr int    kLabelPadDip  = 4;
    static constexpr int    kCornerDip    = 4;
    static constexpr int    kLabelLeadDip = 6;

    //  The label's size against the values', and how far the outline's color
    //  is taken from the background toward the region's.
    static constexpr float  kLabelScale  = 0.8f;
    static constexpr float  kOutlineMix  = 0.6f;

    bool  IsShowing () const { return m_showRegions; }
    void  SetShow   (bool show);
    bool  IsDrawn   () const;

    //  The space above a row that the outlines need, in the bounds' units:
    //  none, an edge's lane, or a lane a label fits in.
    int   GetLaneAbove (uint64_t row) const;

    //  Where a row's values begin, measured down from the bounds' top, and the
    //  row a point's height falls in, lanes counted with the row below them.
    int       GetRowTop (uint64_t row) const;
    uint64_t  GetRowAtY (int clientY) const;

    //  The segment the label of the region that starts at a row's value goes
    //  on. False when no region starts there.
    bool  TryGetLabelSegment (uint64_t row, int unit, LabelSegment & out) const;

    //  A label's left: centered on its segment, but at least lead past the
    //  segment's left, and past the row's left when the row's right moves it
    //  back. It can extend past the segment's right, never past the row's
    //  right while the row is wide enough for it.
    static float  PlaceLabel (float left, float right, float width, float lead, float rowLeft, float rowRight);

    //  The rows that fit with the lanes above each and the one below the last.
    int       GetRowCapWithLanes    () const;
    uint64_t  GetMaxTopRowWithLanes () const;

    //  Each row's top, found again; painting does this every frame, since the
    //  source's regions can change under the view.
    void  RefreshLanes () const;

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);

private:
    //  A unit is one value of the hex column; a region is read at each value's
    //  first byte. Rows past either end are in no region.
    int       GetUnitsPerRow        () const;
    uint16_t  GetUnitRegion         (int64_t row, int unit) const;
    uint16_t  GetNearestKnownRegion (uint64_t offset) const;
    bool      IsRegionStart         (int64_t row, int unit) const;
    float     GetUnitLeft           (int unit) const;
    float     GetUnitRight          (int unit) const;
    float     GetEdgeY              (int64_t row) const;
    int       GetLabelLane          () const;
    int       GetLabelWidth         (uint16_t region) const;
    void      PaintRun       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme,
                              int64_t firstRow, int firstUnit, int64_t lastRow, int lastUnit, uint16_t region);
    void      DrawOutline    (IDxuiPainter & painter, const std::vector<DxuiPointF> & points, uint32_t argb) const;

    DxuiHexView  & m_view;

    bool  m_showRegions = false;

    //  Each row's top, down from the bounds' top, from the top row on, as the
    //  last refresh of the lanes found them.
    mutable std::vector<int>  m_laneTops;
    mutable uint64_t          m_lanesTopRow = UINT64_MAX;
    mutable int               m_lanesUnits  = 0;
    mutable int               m_lanesCellH  = 0;
    mutable int               m_lanesHeight = 0;
};
