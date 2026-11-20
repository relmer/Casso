#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/StripHitTest.h"


class InspectorTip;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView
//
//  The selected track unrolled: each nibble a run of its kind's color, as
//  wide as its cells, with its value once it fits, each sector bracketed
//  from its address prologue to the end of its data epilogue and labeled in
//  its state's color and symbol, and the selected sector outlined, in two
//  parts across the index (FR-033, FR-036). The wheel zooms
//  about the pointer; a drag selects nibbles, as a drag does elsewhere, and
//  Shift and a drag or the horizontal wheel pans; a click selects the nibble
//  and its sector (FR-037). A click on a sector's bracket selects the
//  sector, and the platter flies to it, as it does to a range dragged out
//  on the strip (FR-165).
//
////////////////////////////////////////////////////////////////////////////////

class TrackStripView : public InspectorView
{
public:

    explicit TrackStripView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint              (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  GetTooltip         (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    void          ZoomAboutCenter (double factor);
    void          StepZoom        (int steps);
    void          PanBy           (double fractionOfView);
    void          SelectFieldAt   (const TrackAnalysis & track, float x);
    void          ShowWholeTrack  ();
    std::wstring  GetReadout      () const;

    //  Which side of a comparison the strip shows: 0 for A, 1 for B.
    void          SetSide         (int side) { m_side = side; }

private:
    void            PaintTrack         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track);
    void            PaintCells         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void            PaintTimingLine    (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, float top, float bottom);
    void            PaintFiles         (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void            DimOutsideFile     (IDxuiPainter & painter, const vector<StripSegment> & parts) const;
    void            PaintMarquee       (IDxuiPainter & painter, const vector<StripSegment> & parts) const;
    void            PaintSelection     (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g);
    void            PaintShimmer       (IDxuiPainter & painter, const StripGeometry & g) const;
    void            PaintDiffs         (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g);
    void            PaintHits          (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g);
    void            PaintHead          (IDxuiPainter & painter, const StripGeometry & g) const;
    bool            IsHeadOnTrack      () const;
    static LPCWSTR  GetHeadLabel       (HeadActivity activity);
    void            PaintDividers      (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g, float top, float height);
    void            PaintSeam          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void            PaintLabels        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void            PaintStretchLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, bool isInLabelRow);
    float          GetLabelRowClearance (IDxuiTextRenderer & text, const TrackAnalysis & track, const StripGeometry & g, const NibbleSection & section,
                                         float x0, float x1) const;
    float                GetSectorLabel     (IDxuiTextRenderer & text, const AnalyzedSector & sector, float widthPx, std::wstring & outLabel) const;
    static std::wstring  GetNibbleKindLabel (NibbleKind kind);
    bool                 BuildTip           (POINT pointPx, InspectorTip & outTip) const;
    void                 StepZoomAt         (int steps, double fraction);
    StripGeometry        MakeGeometry       () const;
    RECT                 GetBarRect         () const;

    mutable StripHitTest           m_hits;
    bool                           m_isPressed    = false;
    bool                           m_isPanning    = false;
    bool                           m_isSelecting  = false;
    bool                           m_isShiftPress = false;
    POINT                          m_pressAt      = {};
    POINT                          m_lastAt       = {};
    int64_t                        m_lastClickMs  = 0;
    int                            m_side         = 0;
    float                          m_digitsW      = 0;
};
