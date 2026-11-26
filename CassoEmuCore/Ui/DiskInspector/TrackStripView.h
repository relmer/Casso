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

    //  The tip for the nibble at a column of the bar, for a comparison's tip
    //  that has both strips' nibbles at the shared cursor.
    bool  BuildTipAtX        (int xPx, InspectorTip & outTip) const;

    void          ZoomAboutCenter (double factor);
    void          StepZoom        (int steps);
    void          PanBy           (double fractionOfView);
    void          SelectFieldAt   (const TrackAnalysis & track, float x);
    void          ShowWholeTrack  ();
    std::wstring  GetReadout      () const;
    double        GetMinSpan      () const;

    //  Which side of a comparison the strip shows: 0 for A, 1 for B.
    void          SetSide         (int side) { m_side = side; }

    //  The rows the shimmer crosses; the strip's bar when top equals end.
    void          SetLightRows    (LONG top, LONG end) { m_lightTop = top; m_lightEnd = end; }

private:
    void           PaintTrack   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track);
    bool           BuildTip     (POINT pointPx, InspectorTip & outTip) const;
    void           StepZoomAt   (int steps, double fraction);
    StripGeometry  MakeGeometry () const;
    RECT           GetBarRect   () const;

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
    LONG                           m_lightTop     = 0;
    LONG                           m_lightEnd     = 0;
};
