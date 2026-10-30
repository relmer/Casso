#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/NibbleSections.h"
#include "Ui/DiskInspector/StripGeometry.h"


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

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    void          ZoomAboutCenter (double factor);
    void          StepZoom        (int steps);
    void          PanBy           (double fractionOfView);
    void          ShowWholeTrack  ();
    std::wstring  GetReadout      () const;

    //  Which side of a comparison the strip shows: 0 for A, 1 for B.
    void          SetSide         (int side) { m_side = side; }

private:
    void           PaintTrack   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track);
    void           PaintCells      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void           PaintTimingLine (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, float top, float bottom);
    void           PaintFiles      (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void           GetFileParts    (const TrackAnalysis & track, const StripGeometry & g, vector<StripSegment> & outParts) const;
    void           DimOutsideFile  (IDxuiPainter & painter, const vector<StripSegment> & parts) const;
    void           PaintMarquee    (IDxuiPainter & painter, const vector<StripSegment> & parts) const;
    void           PaintSelection  (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g);
    void           PaintShimmer    (IDxuiPainter & painter, const StripGeometry & g) const;
    void           PaintDiffs      (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g);
    void           PaintHits       (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g);
    void           PaintHead       (IDxuiPainter & painter, const StripGeometry & g) const;
    bool           IsHeadOnTrack   () const;
    static LPCWSTR GetHeadLabel    (HeadActivity activity);
    void           PaintDividers   (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g, float top, float height);
    void           PaintSeam       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void           PaintLabels  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void           PaintStretchLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, bool isInLabelRow);
    float          GetLabelRowClearance (IDxuiTextRenderer & text, const TrackAnalysis & track, const StripGeometry & g, const NibbleSection & section,
                                         float x0, float x1) const;
    int            GetSectorParts  (const TrackAnalysis & track, const StripGeometry & g, int sectorIndex, double & outStart, double & outEnd,
                                    std::array<StripSegment, 2> & outParts) const;
    int            GetSectorAtX    (const TrackAnalysis & track, float xPx) const;
    float          GetSectorLabel  (IDxuiTextRenderer & text, const AnalyzedSector & sector, float widthPx, std::wstring & outLabel) const;
    static std::wstring  GetNibbleKindLabel (NibbleKind kind);
    int            GetSeamSegments (const TrackAnalysis & track, const StripGeometry & g, std::array<StripSegment, 2> & outParts) const;
    bool           IsOverSeam      (const TrackAnalysis & track, float xPx) const;
    bool           BuildTip        (POINT pointPx, InspectorTip & outTip) const;
    float          GetCellPx       (const TrackAnalysis & track) const;
    vector<double> GetZoomStops    (const TrackAnalysis & track) const;
    void           StepZoomAt      (int steps, double fraction);
    static bool    IsTimed         (const TrackAnalysis & track);
    void           UpdateTurns  (const TrackAnalysis & track) const;
    StripGeometry  MakeGeometry () const;
    RECT           GetBarRect   () const;
    int            GetNibbleAtX (const TrackAnalysis & track, float xPx) const;
    int            GetCellAtX   (const TrackAnalysis & track, float xPx) const;

    mutable const TrackAnalysis  * m_turnsOf      = nullptr;
    mutable vector<double>         m_turns;
    mutable vector<NibbleSection>  m_sections;
    mutable const TrackAnalysis  * m_sectionsOf   = nullptr;
    mutable vector<double>         m_cellTurns;
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
