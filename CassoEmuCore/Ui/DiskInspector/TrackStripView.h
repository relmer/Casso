#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/StripGeometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView
//
//  The selected track unrolled: each nibble a run of its kind's color, as
//  wide as its cells, with its value once it fits, each sector's hex number
//  over its address prologue in its state's color and symbol, and the
//  selected sector outlined from its address prologue to the end of its data
//  epilogue, in two parts across the index (FR-033, FR-036). The wheel zooms
//  about the pointer, a drag or the horizontal wheel pans, and a click
//  selects the nibble and its sector (FR-037).
//
////////////////////////////////////////////////////////////////////////////////

class TrackStripView : public InspectorView
{
public:
    static constexpr double  kWheelZoomStep = 1.25;

    explicit TrackStripView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

private:
    void           PaintTrack   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track);
    void           PaintLabels  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void           UpdateTurns  (const TrackAnalysis & track) const;
    StripGeometry  MakeGeometry () const;
    RECT           GetBarRect   () const;
    int            GetNibbleAtX (const TrackAnalysis & track, float xPx) const;
    uint32_t       GetNibbleColor (const TrackAnalysis & track, int nibble) const;

    mutable const TrackAnalysis *  m_turnsOf    = nullptr;
    mutable vector<double>         m_turns;
    bool                           m_isPressed  = false;
    bool                           m_isPanning  = false;
    POINT                          m_pressAt    = {};
    POINT                          m_lastAt     = {};
};
