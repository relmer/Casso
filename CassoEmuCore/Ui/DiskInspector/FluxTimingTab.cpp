#include "Pch.h"

#include "Ui/DiskInspector/FluxTimingTab.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr float   s_kAxisDip         = 52.0f;
static constexpr float   s_kGapDip          = 8.0f;
static constexpr float   s_kScopeRowDip     = 24.0f;
static constexpr float   s_kScopeOptionDip  = 100.0f;
static constexpr float   s_kLabelRowDip     = 16.0f;
static constexpr float   s_kPlotShare       = 0.55f;
static constexpr float   s_kPointDip        = 2.0f;
static constexpr float   s_kPairDip         = 4.0f;
static constexpr float   s_kHitDip          = 6.0f;
static constexpr float   s_kUnderlineDip    = 2.0f;
static constexpr int     s_kDragThresholdPx = 4;
static constexpr int     s_kPeakRadius      = 6;
static constexpr int     s_kPeakShare       = 10;
static constexpr int     s_kTickEveryUs     = 4;
static constexpr double  s_kTicksPerUs      = 8.0;
static constexpr double  s_kWheelZoomStep   = 1.25;
static constexpr uint8_t s_kPlain           = 1;
static constexpr uint8_t s_kSelected        = 2;
static constexpr uint8_t s_kPair            = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::Paint
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *  track   = m_context.GetTrack();
    bool                   isTimed = track != nullptr && track->framed.isFlux && track->framed.turnTicks > 0;



    if (isTimed)
    {
        Update (*track);
        PaintPlot      (painter, text, theme, *track);
        PaintHistogram (painter, text, theme, *track);
    }
    else if (track != nullptr && track->framed.cellCount > 0)
    {
        text.DrawString (L"This track is stored as bit cells of one length, so it records no timing.", static_cast<float> (m_boundsDip.left),
                         static_cast<float> (m_boundsDip.top), GetWidth(), m_scaler.ToPxf (s_kScopeRowDip), theme.ForegroundMuted(),
                         m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::PaintPlot
//
//  Each transition at its recorded time across the strip's view, its
//  interval upward; a transition is drawn once per pixel and kind, so the
//  whole track costs no more than the plot has pixels.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::PaintPlot (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track)
{
    RECT           plot    = GetPlotRect();
    StripGeometry  g       = MakeGeometry();
    int            w       = std::max (static_cast<int> (plot.right - plot.left), 1);
    int            h       = std::max (static_cast<int> (plot.bottom - plot.top), 1);
    double         start   = m_context.model->GetStripStart();
    double         span    = m_context.model->GetStripSpan();
    float          textPx  = m_scaler.ToPxf (kSmallDip);
    float          point   = m_scaler.ToPxf (s_kPointDip);
    float          pair    = m_scaler.ToPxf (s_kPairDip);
    float          x       = 0;
    float          y       = 0;
    float          size    = 0;
    double         offset  = 0;
    int            k       = 0;
    int            cell    = 0;
    size_t         at      = 0;
    uint8_t        kind    = 0;
    uint32_t       color   = 0;



    painter.FillRect (static_cast<float> (plot.left), static_cast<float> (plot.top), static_cast<float> (w), static_cast<float> (h), theme.BackgroundElevated());

    for (k = 1; k <= 3; k++)
    {
        y = GetPlotY (k * TrackAnalyzer::kNominalCellTicks);
        painter.DrawLine (static_cast<float> (plot.left), y, static_cast<float> (plot.right), y, 1.0f, theme.Divider());
        text.DrawString (InspectorText::FormatCount (k, L"cell", L"cells").c_str(), static_cast<float> (m_boundsDip.left), y - textPx, m_scaler.ToPxf (s_kAxisDip) - m_scaler.ToPxf (s_kGapDip),
                         2 * textPx, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    PaintMarks (painter, text, theme, track, g);

    m_occupied.assign (static_cast<size_t> (w) * static_cast<size_t> (h), 0);

    for (const FluxInterval & interval : m_intervals)
    {
        offset  = interval.turn - start;
        offset -= std::floor (offset);

        if (offset >= span)
        {
            continue;
        }

        x    = static_cast<float> (plot.left) + static_cast<float> (offset / span) * static_cast<float> (w);
        y    = GetPlotY (interval.ticks);
        kind = interval.isWithinCell ? s_kPair : (IsSelected (interval) ? s_kSelected : s_kPlain);
        cell = std::clamp (static_cast<int> (x - plot.left), 0, w - 1);
        at   = static_cast<size_t> (std::clamp (static_cast<int> (y - plot.top), 0, h - 1)) * w + cell;

        if ((m_occupied[at] & kind) == 0)
        {
            m_occupied[at] |= kind;
            size            = (kind == s_kPair) ? pair : point;
            color           = (kind == s_kPair) ? m_context.palette.colors.failedChecksum : (kind == s_kSelected) ? theme.Accent() : theme.Foreground();
            painter.FillRect (x - size / 2, y - size / 2, size, size, color);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::PaintMarks
//
//  The index and each sector's address prologue, as rules down the plot
//  with their labels along its top.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::PaintMarks (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    const FramedTrack &          framed = track.framed;
    RECT                         plot   = GetPlotRect();
    std::array<StripSegment, 2>  parts  = {};
    float                        textPx = m_scaler.ToPxf (kSmallDip);
    float                        rowH   = m_scaler.ToPxf (s_kLabelRowDip);
    float                        pad    = m_scaler.ToPxf (3.0f);
    int                          count  = 0;
    int                          k      = 0;
    uint32_t                     cell   = 0;
    std::wstring                 label;



    count = g.GetSegments (0.0, 1e-9, parts);

    for (k = 0; k < count; k++)
    {
        painter.DrawLine (parts[k].x0, static_cast<float> (plot.top), parts[k].x0, static_cast<float> (plot.bottom), 1.5f, theme.ForegroundMuted());
        text.DrawString (L"Index", parts[k].x0 + pad, static_cast<float> (plot.top), m_scaler.ToPxf (s_kAxisDip), rowH, theme.ForegroundMuted(), textPx,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    for (const AnalyzedSector & sector : track.sectors)
    {
        cell  = framed.nibbles[track.fields[sector.addressField].firstNibble].startCell % framed.cellCount;
        count = g.GetSegments (m_cellTurns[cell], 1e-9, parts);
        label = std::wstring (DiskInspectorPalette::GetStateSymbol (sector.state)) + L" " + InspectorFormat::FormatSector (sector.sector).substr (1);

        for (k = 0; k < count; k++)
        {
            painter.DrawLine (parts[k].x0, static_cast<float> (plot.top), parts[k].x0, static_cast<float> (plot.bottom), 1.0f, theme.Divider());
            text.DrawString (label.c_str(), parts[k].x0 + pad, static_cast<float> (plot.top) + rowH, m_scaler.ToPxf (s_kAxisDip), rowH,
                             m_context.palette.GetStateColor (sector.state), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                             DxuiFontWeight::SemiBold, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::PaintHistogram
//
//  A bar per 0.125 µs of interval, scaled to the tallest, intervals under
//  half a cell in the color of the pairs they make, with the count over each
//  cluster and rules at one, two and three cells (FR-045).
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::PaintHistogram (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track)
{
    bool           hasSelection = false;
    FluxHistogram  histogram    = GetHistogram (track, hasSelection);
    RECT           bars         = GetBarsRect();
    std::wstring   message;



    PaintScope (painter, text, theme, histogram);

    painter.FillRect (static_cast<float> (bars.left), static_cast<float> (bars.top), static_cast<float> (bars.right - bars.left),
                      static_cast<float> (bars.bottom - bars.top), theme.BackgroundElevated());

    message = (m_scope == HistogramScope::Selection && !hasSelection) ? L"Select nibbles or a sector to see their intervals."
            : (histogram.total == 0)                                   ? L"No transitions in the selection."
            :                                                            L"";

    if (message.empty())
    {
        PaintBars (painter, text, theme, histogram);
    }
    else
    {
        text.DrawString (message.c_str(), static_cast<float> (bars.left), static_cast<float> (bars.top), static_cast<float> (bars.right - bars.left),
                         static_cast<float> (bars.bottom - bars.top), theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::PaintBars
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::PaintBars (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const FluxHistogram & histogram)
{
    RECT   bars    = GetBarsRect();
    float  textPx  = m_scaler.ToPxf (kSmallDip);
    float  rowH    = m_scaler.ToPxf (s_kLabelRowDip);
    float  axis    = m_scaler.ToPxf (s_kAxisDip);
    float  left    = static_cast<float> (bars.left);
    float  w       = static_cast<float> (bars.right - bars.left);
    float  barsTop = static_cast<float> (bars.top) + rowH;
    float  barsH   = std::max (static_cast<float> (bars.bottom) - barsTop, 1.0f);
    float  binW    = w / FluxHistogram::kBinCount;
    float  x       = 0;
    float  hgt     = 0;
    int    b       = 0;
    int    k       = 0;



    for (k = 1; k <= 3; k++)
    {
        x = left + static_cast<float> (k * TrackAnalyzer::kNominalCellTicks / FluxHistogram::kBinCount) * w;
        painter.DrawLine (x, static_cast<float> (bars.top), x, static_cast<float> (bars.bottom), 1.0f, theme.Divider());
    }

    for (b = 0; b < FluxHistogram::kBinCount; b++)
    {
        if (histogram.counts[b] > 0)
        {
            hgt = std::max (barsH * histogram.counts[b] / histogram.peak, 1.0f);
            painter.FillRect (left + b * binW, static_cast<float> (bars.bottom) - hgt, std::max (binW - 1.0f, 1.0f), hgt,
                              (b < TrackAnalyzer::kNominalCellTicks / 2) ? m_context.palette.colors.failedChecksum : theme.Accent());
        }
    }

    for (int peak : FindPeaks (histogram))
    {
        hgt = barsH * histogram.counts[peak] / histogram.peak;
        text.DrawString (InspectorFormat::FormatCount (static_cast<uint64_t> (histogram.counts[peak])).c_str(), left + peak * binW - axis,
                         static_cast<float> (bars.bottom) - hgt - rowH, 2 * axis + binW, rowH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    for (b = s_kTickEveryUs; b * s_kTicksPerUs < FluxHistogram::kBinCount; b += s_kTickEveryUs)
    {
        x = left + static_cast<float> (b * s_kTicksPerUs / FluxHistogram::kBinCount) * w;
        text.DrawString (std::format (L"{} {}s", b, s_kpszMicro).c_str(), x - axis, static_cast<float> (bars.bottom), 2 * axis, rowH, theme.ForegroundMuted(),
                         textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::PaintScope
//
//  The histogram's two scopes as tabs, and the count it holds.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::PaintScope (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const FluxHistogram & histogram)
{
    RECT          row    = GetScopeRect();
    float         textPx = m_scaler.ToPxf (kTextDip);
    float         line   = m_scaler.ToPxf (s_kUnderlineDip);
    std::wstring  total  = InspectorText::FormatCount (histogram.total, L"transition", L"transitions");



    for (HistogramScope scope : { HistogramScope::WholeTrack, HistogramScope::Selection })
    {
        RECT  option     = GetScopeOption (scope);
        bool  isSelected = scope == m_scope;

        text.DrawString (scope == HistogramScope::WholeTrack ? L"Whole track" : L"Selection", static_cast<float> (option.left), static_cast<float> (option.top),
                         static_cast<float> (option.right - option.left), static_cast<float> (option.bottom - option.top),
                         isSelected ? theme.Foreground() : theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                         DxuiFontWeight::Normal, false);

        if (isSelected)
        {
            painter.FillRect (static_cast<float> (option.left), static_cast<float> (option.bottom) - line, static_cast<float> (option.right - option.left), line, theme.Accent());
        }
    }

    text.DrawString (total.c_str(), static_cast<float> (row.left), static_cast<float> (row.top), static_cast<float> (row.right - row.left),
                     static_cast<float> (row.bottom - row.top), theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Right,
                     DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::OnMouse
//
//  A click on a scope switches the histogram. Over the plot the wheel and
//  a drag zoom and pan as they do on the strip, which follows.
//
////////////////////////////////////////////////////////////////////////////////

bool FluxTimingTab::OnMouse (const DxuiMouseEvent & ev)
{
    const TrackAnalysis *  track     = m_context.GetTrack();
    RECT                   plot      = GetPlotRect();
    POINT                  p         = ev.positionDip;
    bool                   isInPlot  = PtInRect (&plot, p) != FALSE;
    bool                   isHandled = false;
    double                 span      = m_context.model->GetStripSpan();
    double                 width     = std::max (static_cast<double> (plot.right - plot.left), 1.0);
    double                 newSpan   = 0;
    RECT                   whole     = GetScopeOption (HistogramScope::WholeTrack);
    RECT                   selection = GetScopeOption (HistogramScope::Selection);



    switch (track != nullptr && track->framed.isFlux ? ev.kind : DxuiMouseEventKind::Leave)
    {
        case DxuiMouseEventKind::Wheel:
            if (isInPlot && !ev.wheelHorizontal && !ev.shift)
            {
                newSpan = std::clamp (span / std::pow (s_kWheelZoomStep, ev.wheelDelta), StripGeometry::kMinSpan, 1.0);
                m_context.model->SetStrip (StripGeometry::GetStartForZoom (m_context.model->GetStripStart(), span, newSpan, (p.x - plot.left) / width), newSpan);
                isHandled = true;
            }
            else if (isInPlot)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - ev.wheelDelta * span / 8.0, span);
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left && (PtInRect (&whole, p) || PtInRect (&selection, p)))
            {
                m_scope   = PtInRect (&whole, p) ? HistogramScope::WholeTrack : HistogramScope::Selection;
                isHandled = true;
            }
            else if (isInPlot && ev.button == DxuiMouseButton::Left)
            {
                m_isPressed = true;
                m_isPanning = false;
                m_pressAt   = p;
                m_lastAt    = p;
                isHandled   = true;
            }

            break;

        case DxuiMouseEventKind::Move:
            if (m_isPressed && !m_isPanning && std::abs (p.x - m_pressAt.x) >= s_kDragThresholdPx)
            {
                m_isPanning = true;
            }

            if (m_isPanning)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - (p.x - m_lastAt.x) / width * span, span);
                m_lastAt = p;
            }

            isHandled = isInPlot || m_isPressed;
            break;

        case DxuiMouseEventKind::Up:
            isHandled   = m_isPressed;
            m_isPressed = false;
            m_isPanning = false;
            break;

        default:
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetTooltip
//
//  A transition's cell and interval in the plot, a bar's range and count
//  in the histogram.
//
////////////////////////////////////////////////////////////////////////////////

bool FluxTimingTab::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track        = m_context.GetTrack();
    bool                   hasSelection = false;
    int                    i            = -1;
    int                    b            = -1;
    FluxHistogram          histogram;



    if (track != nullptr && track->framed.isFlux && !m_isPanning)
    {
        Update (*track);
        i = HitInterval (pointPx);
        b = (i < 0) ? HitBin (pointPx) : -1;
    }

    if (i >= 0)
    {
        const FluxInterval &  interval = m_intervals[i];

        outText = std::format (L"Transition at cell {}\n{} since the one before, {:.2f} cells", InspectorFormat::FormatCount (interval.cell),
                               InspectorFormat::FormatMicroseconds (interval.ticks), interval.ticks / TrackAnalyzer::kNominalCellTicks);

        if (interval.isWithinCell)
        {
            outText += L"\nWithin one cell of the one before, so the drive reads the two as one";
        }
    }
    else if (b >= 0)
    {
        histogram = GetHistogram (*track, hasSelection);
        outText   = std::format (L"{:.3f} to {:.3f} {}s\n{}", b * FluxHistogram::kBinMicroseconds, (b + 1) * FluxHistogram::kBinMicroseconds, s_kpszMicro,
                                 InspectorText::FormatCount (histogram.counts[b], L"transition", L"transitions"));
    }

    outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };

    return i >= 0 || b >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::FindPeaks
//
////////////////////////////////////////////////////////////////////////////////

vector<int> FluxTimingTab::FindPeaks (const FluxHistogram & histogram)
{
    vector<int>  peaks;
    int          b      = 0;
    int          k      = 0;
    bool         isPeak = false;



    for (b = 0; b < FluxHistogram::kBinCount; b++)
    {
        isPeak = histogram.counts[b] > 0 && histogram.counts[b] * s_kPeakShare >= histogram.peak;

        for (k = std::max (b - s_kPeakRadius, 0); isPeak && k <= std::min (b + s_kPeakRadius, FluxHistogram::kBinCount - 1); k++)
        {
            //  Ties go to the first bin, so a flat top gets one count.
            isPeak = (k < b) ? histogram.counts[k] < histogram.counts[b] : histogram.counts[k] <= histogram.counts[b];
        }

        if (isPeak)
        {
            peaks.push_back (b);
        }
    }

    return peaks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::Update
//
//  A track's intervals and whole-track histogram are built once; the
//  selection's cells are found on every paint, since it changes elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTimingTab::Update (const TrackAnalysis & track) const
{
    if (m_of != &track)
    {
        FluxTiming::BuildIntervals (track, m_intervals);
        FluxTiming::BuildCellTurns (track, m_cellTurns);
        m_whole = FluxTiming::BuildHistogram (m_intervals, 0, UINT32_MAX);
        m_of    = &track;
    }

    m_hasSel = FluxTiming::GetSelectionCells (track, m_context.model->GetFirstNibble(), m_context.model->GetNibbleCount(),
                                              m_context.model->GetSectorIndex(), m_selFirst, m_selEnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetHistogram
//
////////////////////////////////////////////////////////////////////////////////

FluxHistogram FluxTimingTab::GetHistogram (const TrackAnalysis & track, bool & outHasSelection) const
{
    FluxHistogram  histogram;



    (void) track;
    outHasSelection = m_hasSel;

    if (m_scope == HistogramScope::WholeTrack)
    {
        histogram = m_whole;
    }
    else if (m_hasSel)
    {
        histogram = FluxTiming::BuildHistogram (m_intervals, m_selFirst, m_selEnd);
    }

    return histogram;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::IsSelected
//
////////////////////////////////////////////////////////////////////////////////

bool FluxTimingTab::IsSelected (const FluxInterval & interval) const
{
    bool  isWrapped = m_selEnd < m_selFirst;



    return m_hasSel && (isWrapped ? (interval.cell >= m_selFirst || interval.cell < m_selEnd) : (interval.cell >= m_selFirst && interval.cell < m_selEnd));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::MakeGeometry
//
////////////////////////////////////////////////////////////////////////////////

StripGeometry FluxTimingTab::MakeGeometry() const
{
    RECT  plot = GetPlotRect();



    return StripGeometry (m_context.model->GetStripStart(), m_context.model->GetStripSpan(), static_cast<float> (plot.left), static_cast<float> (plot.right - plot.left));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetPlotRect
//
//  The plot over the top part of the tab, past a gutter for its labels; the
//  scope row, then the histogram and its axis labels, below it.
//
////////////////////////////////////////////////////////////////////////////////

RECT FluxTimingTab::GetPlotRect() const
{
    RECT  plot = m_boundsDip;



    plot.left   += m_scaler.ToPx (s_kAxisDip);
    plot.bottom  = plot.top + static_cast<LONG> ((m_boundsDip.bottom - m_boundsDip.top - m_scaler.ToPxf (s_kScopeRowDip)) * s_kPlotShare);

    return plot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetScopeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT FluxTimingTab::GetScopeRect() const
{
    RECT  row = GetPlotRect();



    row.top    = row.bottom + m_scaler.ToPx (s_kGapDip);
    row.bottom = row.top + m_scaler.ToPx (s_kScopeRowDip);

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetBarsRect
//
////////////////////////////////////////////////////////////////////////////////

RECT FluxTimingTab::GetBarsRect() const
{
    RECT  bars = GetScopeRect();



    bars.top    = bars.bottom + m_scaler.ToPx (s_kGapDip);
    bars.bottom = std::max (bars.top + 1, static_cast<LONG> (m_boundsDip.bottom - m_scaler.ToPx (s_kLabelRowDip)));

    return bars;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetScopeOption
//
////////////////////////////////////////////////////////////////////////////////

RECT FluxTimingTab::GetScopeOption (HistogramScope scope) const
{
    RECT  option = GetScopeRect();
    int   width  = m_scaler.ToPx (s_kScopeOptionDip);



    option.left  += (scope == HistogramScope::WholeTrack) ? 0 : width;
    option.right  = option.left + width;

    return option;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetPlotY
//
////////////////////////////////////////////////////////////////////////////////

float FluxTimingTab::GetPlotY (double ticks) const
{
    RECT    plot  = GetPlotRect();
    double  share = std::min (ticks / (TrackAnalyzer::kNominalCellTicks * kPlotCells), 1.0);



    return static_cast<float> (plot.bottom) - static_cast<float> (share) * static_cast<float> (plot.bottom - plot.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::HitInterval
//
//  The transition nearest the point within a few pixels, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int FluxTimingTab::HitInterval (POINT pointPx) const
{
    RECT    plot   = GetPlotRect();
    double  start  = m_context.model->GetStripStart();
    double  span   = m_context.model->GetStripSpan();
    float   w      = static_cast<float> (plot.right - plot.left);
    float   reach  = m_scaler.ToPxf (s_kHitDip);
    float   best   = reach * reach;
    float   dx     = 0;
    float   dy     = 0;
    double  offset = 0;
    size_t  i      = 0;
    int     hit    = -1;



    for (i = 0; PtInRect (&plot, pointPx) && i < m_intervals.size(); i++)
    {
        offset  = m_intervals[i].turn - start;
        offset -= std::floor (offset);
        dx      = static_cast<float> (plot.left) + static_cast<float> (offset / span) * w - pointPx.x;
        dy      = GetPlotY (m_intervals[i].ticks) - pointPx.y;

        if (offset < span && dx * dx + dy * dy <= best)
        {
            best = dx * dx + dy * dy;
            hit  = static_cast<int> (i);
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::HitBin
//
////////////////////////////////////////////////////////////////////////////////

int FluxTimingTab::HitBin (POINT pointPx) const
{
    RECT  bars = GetBarsRect();
    int   bin  = -1;



    if (PtInRect (&bars, pointPx))
    {
        bin = std::clamp (static_cast<int> ((pointPx.x - bars.left) * FluxHistogram::kBinCount / std::max (bars.right - bars.left, 1L)), 0,
                          FluxHistogram::kBinCount - 1);
    }

    return bin;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTab::GetHistogramText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FluxTimingTab::GetHistogramText() const
{
    const TrackAnalysis *         track        = m_context.GetTrack();
    bool                          hasSelection = false;
    FluxHistogram                 histogram;
    vector<vector<std::wstring>>  rows;
    int                           b            = 0;



    if (track != nullptr && track->framed.isFlux)
    {
        Update (*track);
        histogram = GetHistogram (*track, hasSelection);
    }

    for (b = 0; b < FluxHistogram::kBinCount; b++)
    {
        if (histogram.counts[b] > 0)
        {
            rows.push_back ({ std::format (L"{:.3f}", b * FluxHistogram::kBinMicroseconds), std::format (L"{:.3f}", (b + 1) * FluxHistogram::kBinMicroseconds),
                              std::to_wstring (histogram.counts[b]) });
        }
    }

    return rows.empty() ? std::wstring() : InspectorClipboard::FormatTable ({ std::format (L"From ({}s)", s_kpszMicro), std::format (L"To ({}s)", s_kpszMicro), L"Transitions" }, rows);
}
