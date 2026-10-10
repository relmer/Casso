#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/PlatterGeometry.h"
#include "Ui/DiskInspector/PlatterRenderer.h"





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

    PlatterRenderer &  GetRenderer ()       { return m_renderer; }
    PlatterHit         HitTest     (POINT pointPx) const;
    bool               IsDragging  () const { return m_isPanning; }
    void               ZoomBy      (double factor, POINT anchorPx);
    void               ZoomAboutCenter (double factor);
    double             GetRotation () const { return m_rotation; }
    void               SetRotation (double rotation) { m_rotation = rotation; }
    void               SetAlignmentShown (bool isShown) { m_isAlignmentShown = isShown; }

private:
    struct RingTurns
    {
        const TrackAnalysis *  of = nullptr;
        vector<double>         nibbles;
        vector<double>         cells;
    };

    void           Draw           (const DxuiCustomDrawArgs & args);
    void           PaintNibbles   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintRing      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const PlatterPlacement & view, int quarterTrack,
                                   const TrackAnalysis & track, const RingTurns & turns, double start, double end);
    static void    ShadeArc       (IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r0, double r1, uint32_t argb);
    bool           IsSelectedNibble (int quarterTrack, int nibble) const;
    static void    DrawRadial     (IDxuiPainter & painter, const PlatterPlacement & view, double turn, double r0, double r1, float thicknessPx, uint32_t argb);
    void           DrawUpright    (IDxuiTextRenderer & text, const PlatterPlacement & view, double turn, double radius, const std::wstring & value, float widthPx,
                                   float textPx, uint32_t argb) const;
    bool           IsShowingValues (const PlatterPlacement & view, int quarterTrack) const;
    bool           IsShowingTicks  (const PlatterPlacement & view, int quarterTrack) const;
    const TrackAnalysis *  GetRingTrack (int quarterTrack) const;
    const RingTurns &      GetRingTurns (int slot, const TrackAnalysis & track) const;
    void           PaintDisk      (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
    void           PaintAlignment (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view);
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
    double                     m_rotation         = 0.0;
    mutable vector<RingTurns>  m_ringTurns;
};
