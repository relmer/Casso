#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/PlatterGeometry.h"
#include "Ui/DiskInspector/PlatterRenderer.h"


class InspectorMarquee;
class InspectorTip;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView
//
//  The platter: its rings drawn on the GPU by PlatterRenderer, then over
//  them the hub, the index mark, the line at the head's limit, and outlines
//  around the ring under the pointer and, more strongly, the selected ring
//  (FR-022, FR-023, FR-028). The wheel zooms about the pointer, a drag pans,
//  a click selects the quarter track and the sector whose field is under
//  the pointer, and a double-click returns to fit (FR-025).
//
////////////////////////////////////////////////////////////////////////////////

class PlatterView : public InspectorView
{
public:
    static constexpr double  kWheelZoomStep = 1.25;

    explicit PlatterView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    PlatterRenderer &  GetRenderer ()       { return m_renderer; }
    PlatterHit         HitTest     (POINT pointPx) const;
    bool               IsDragging  () const { return m_isPanning; }
    void               ZoomBy      (double factor, POINT anchorPx);
    void               ZoomAboutCenter (double factor);
    //  Where the head's bullseye is drawn, or false when there is no head.
    bool               GetHeadPoint (POINT & outPointPx) const;
    void               SetAlignmentShown (bool isShown) { m_isAlignmentShown = isShown; }

private:
    struct RingTurns
    {
        const TrackAnalysis *  of = nullptr;
        vector<double>         nibbles;
        vector<double>         cells;
    };

    //  A data field of the selected file on a whole track's rings, between
    //  two turns; `b` passes 1 when the field crosses the index.
    struct FileArc
    {
        int     wholeTrack = 0;
        double  a          = 0;
        double  b          = 0;
    };

    void           Draw           (const DxuiCustomDrawArgs & args);
    void           PaintNibbles   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintRing      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const PlatterPlacement & view, int quarterTrack,
                                   const TrackAnalysis & track, const RingTurns & turns, double start, double end);
    void           PaintFiles     (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintDiffs     (IDxuiPainter & painter, const PlatterPlacement & view);
    void           PaintHits      (IDxuiPainter & painter, const PlatterPlacement & view);
    void           GetFileArcs    (vector<FileArc> & outArcs) const;
    void           PaintSelectedArc (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view) const;
    void           PaintArcMark     (IDxuiPainter & painter, const PlatterPlacement & view, int quarterTrack, double start, double end, bool isSelected) const;
    void           GetMarkBand      (const PlatterPlacement & view, int quarterTrack, double & outR0, double & outR1) const;
    RECT           GetArcBox        (const PlatterPlacement & view, double start, double end, double r0, double r1) const;
    void           GetVisibleRadii  (const PlatterPlacement & view, double r0, double r1, double & outNear, double & outFar) const;
    bool           IsOverSelection  (const PlatterPlacement & view, POINT pointPx) const;
    void           DimOutsideFile (IDxuiPainter & painter, const PlatterPlacement & view, const vector<FileArc> & arcs) const;
    void           PaintMarquee   (IDxuiPainter & painter, const PlatterPlacement & view, const vector<FileArc> & arcs) const;
    static void    GetDiffTurns   (const Difference & difference, const TrackAnalysis & track, const RingTurns & turns, double & outA, double & outB);
    void           ShadeRingArc   (IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r0, double r1, uint32_t argb) const;
    vector<std::pair<double, double>>  GetVisibleParts (const PlatterPlacement & view, double a, double b) const;
    bool           IsBandVisible  (const PlatterPlacement & view, double r0, double r1) const;
    void           DrawMarqueeArc (const InspectorMarquee & marquee, IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r) const;
    void           DrawMarqueeClear (const InspectorMarquee & marquee, IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r) const;
    double         GetClearTurn     (const PlatterPlacement & view, double turn, double inner, double outer) const;
    bool           GetSelectionBorder (const PlatterPlacement & view, double & outStart, double & outEnd, double & outR0, double & outR1) const;
    static void    ShadeArc       (IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r0, double r1, uint32_t argb);
    bool           IsSelectedNibble (int quarterTrack, int nibble) const;
    static void    DrawRadial     (IDxuiPainter & painter, const PlatterPlacement & view, double turn, double r0, double r1, float thicknessPx, uint32_t argb);
    void           DrawUpright    (IDxuiTextRenderer & text, const PlatterPlacement & view, double turn, double radius, const std::wstring & value, float widthPx,
                                   float textPx, uint32_t argb) const;
    bool           BuildTip       (POINT pointPx, InspectorTip & outTip) const;
    bool           IsShowingValues (const PlatterPlacement & view, int quarterTrack) const;
    bool           IsShowingTicks  (const PlatterPlacement & view, int quarterTrack) const;
    const TrackAnalysis *  GetRingTrack (int quarterTrack) const;
    const RingTurns &      GetRingTurns (int slot, const TrackAnalysis & track) const;
    void           PaintDisk      (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintAlignment (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintHead      (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view) const;
    void           PaintReads     (IDxuiPainter & painter, const PlatterPlacement & view) const;
    void           OutlineRing    (IDxuiPainter & painter, const PlatterPlacement & view, int quarterTrack, float thicknessPx, uint32_t argb) const;
    static void    OutlineCircle  (IDxuiPainter & painter, float cx, float cy, float radius, float thicknessPx, uint32_t argb);
    PlatterPlacement  MakeView       () const;
    InspectorViewModel::Point  ToViewPoint (POINT pointPx) const;

    PlatterRenderer            m_renderer;
    bool                       m_isRendererReady  = false;
    bool                       m_isAlignmentShown = false;
    bool                       m_isPressed        = false;
    bool                       m_isPanning        = false;
    POINT                      m_pressAt          = {};
    POINT                      m_lastAt           = {};
    int64_t                    m_lastClickMs      = 0;
    int                        m_hoverRing        = -1;
    mutable vector<RingTurns>  m_ringTurns;
};
