#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/SectorAccess.h"

class InspectorMarquee;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterOverlays
//
//  What the platter draws over its rings: the Files overlay and the selected
//  file's marquee, the differences, Find's hits, the reads, the alignment
//  marks, and the selected sector's arc (FR-031, FR-090, FR-121, FR-056). It
//  draws through its view's placement and ring timing.
//
////////////////////////////////////////////////////////////////////////////////

class PlatterOverlays
{
public:
    //  A data field of the selected file on a whole track's rings, between
    //  two turns; `b` passes 1 when the field crosses the index.
    struct FileArc
    {
        int     wholeTrack = 0;
        double  a          = 0;
        double  b          = 0;
    };

    explicit PlatterOverlays (PlatterView & view) : m_view (view) {}

    void           PaintFiles       (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintDiffs       (IDxuiPainter & painter, const PlatterPlacement & view);
    void           PaintHits        (IDxuiPainter & painter, const PlatterPlacement & view);
    void           PaintAlignment   (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintAccess      (IDxuiPainter & painter, const PlatterPlacement & view, AccessKind kind) const;
    void           DimForAccess     (IDxuiPainter & painter, const PlatterPlacement & view) const;
    static uint32_t                      GetHeatAlpha (uint32_t count, uint32_t most);
    static const DiskAccessLog::Spans &  GetSpans     (const DriveAccess & access, AccessKind kind);
    void           PaintSelectedArc (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view) const;
    void           GetFileArcs      (vector<FileArc> & outArcs) const;
    void           DimOutsideFile   (IDxuiPainter & painter, const PlatterPlacement & view, const vector<FileArc> & arcs) const;
    void           PaintMarquee     (IDxuiPainter & painter, const PlatterPlacement & view, const vector<FileArc> & arcs) const;
    bool           IsOverSelection  (const PlatterPlacement & view, POINT pointPx) const;

private:
    void           PaintArcMark     (IDxuiPainter & painter, const PlatterPlacement & view, int quarterTrack, double start, double end, bool isSelected) const;
    void           GetMarkBand      (const PlatterPlacement & view, int quarterTrack, double & outR0, double & outR1) const;
    RECT           GetArcBox        (const PlatterPlacement & view, double start, double end, double r0, double r1) const;
    void           GetVisibleRadii  (const PlatterPlacement & view, double r0, double r1, double & outNear, double & outFar) const;
    static void    GetDiffTurns     (const Difference & difference, const TrackAnalysis & track, const PlatterView::RingTurns & turns, double & outA, double & outB);
    void           ShadeRingArc     (IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r0, double r1, uint32_t argb) const;
    vector<std::pair<double, double>>  GetVisibleParts (const PlatterPlacement & view, double a, double b) const;
    bool           IsBandVisible    (const PlatterPlacement & view, double r0, double r1) const;
    void           DrawMarqueeArc   (const InspectorMarquee & marquee, IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r) const;
    void           DrawMarqueeClear (const InspectorMarquee & marquee, IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r) const;
    double         GetClearTurn     (const PlatterPlacement & view, double turn, double inner, double outer) const;
    bool           GetSelectionBorder (const PlatterPlacement & view, double & outStart, double & outEnd, double & outR0, double & outR1) const;

    PlatterView  & m_view;
};
