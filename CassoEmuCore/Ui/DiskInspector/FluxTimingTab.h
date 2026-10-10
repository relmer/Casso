#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/StripGeometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab
//
//  A flux track's timing (FR-044 to FR-046). Above, each transition's
//  interval plotted at its recorded time against lines at one, two and three
//  nominal cells, with the index and each sector marked, pairs that fall
//  within one cell marked, and the selection's transitions highlighted; it
//  follows the strip's zoom and pan and changes them as the strip does.
//  Below, a histogram of the intervals over the whole track or over the
//  selection. A bit track shows a note in their place.
//
////////////////////////////////////////////////////////////////////////////////

enum class HistogramScope
{
    WholeTrack,
    Selection,
};


class FluxTimingTab : public InspectorView
{
public:
    static constexpr double  kPlotCells = 3.6;

    explicit FluxTimingTab (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    HistogramScope  GetScope () const { return m_scope; }

    //  The clusters of a histogram worth a count over them: each bin that
    //  is the most in its neighborhood and holds a tenth of the peak.
    static vector<int>  FindPeaks (const FluxHistogram & histogram);

private:
    void  PaintPlot      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track);
    void  PaintMarks     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g);
    void  PaintHistogram (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track);
    void  PaintBars      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const FluxHistogram & histogram);
    void  PaintScope     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const FluxHistogram & histogram);

    void           Update        (const TrackAnalysis & track) const;
    FluxHistogram  GetHistogram  (const TrackAnalysis & track, bool & outHasSelection) const;
    bool           IsSelected    (const FluxInterval & interval) const;
    StripGeometry  MakeGeometry  () const;
    RECT           GetPlotRect   () const;
    RECT           GetScopeRect  () const;
    RECT           GetBarsRect   () const;
    RECT           GetScopeOption (HistogramScope scope) const;
    float          GetPlotY      (double ticks) const;
    int            HitInterval   (POINT pointPx) const;
    int            HitBin        (POINT pointPx) const;

    HistogramScope                 m_scope       = HistogramScope::WholeTrack;
    mutable const TrackAnalysis *  m_of          = nullptr;
    mutable vector<FluxInterval>   m_intervals;
    mutable vector<double>         m_cellTurns;
    mutable FluxHistogram          m_whole;
    mutable vector<uint8_t>        m_occupied;
    mutable uint32_t               m_selFirst    = 0;
    mutable uint32_t               m_selEnd      = 0;
    mutable bool                   m_hasSel      = false;
    bool                           m_isPressed   = false;
    bool                           m_isPanning   = false;
    POINT                          m_pressAt     = {};
    POINT                          m_lastAt      = {};
};
