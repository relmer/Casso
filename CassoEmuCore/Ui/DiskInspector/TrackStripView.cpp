#include "Pch.h"

#include "Ui/DiskInspector/TrackStripView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





static constexpr int    s_kLabelRowDip     = 18;
static constexpr float  s_kOutlineDip      = 2.0f;
static constexpr float  s_kValueMinPx      = 18.0f;
static constexpr float  s_kLabelDip        = 11.0f;
static constexpr float  s_kPadDip          = 6.0f;
static constexpr float  s_kCellDigitDip    = 9.0f;
static constexpr float  s_kCellTimingDip   = 30.0f;
static constexpr float  s_kValueShare      = 0.4f;
static constexpr float  s_kLineInsetDip    = 3.0f;
static constexpr float  s_kLineStepDip     = 2.0f;
static constexpr float  s_kLineDip         = 1.5f;
static constexpr float  s_kSeamRuleDip     = 3.0f;
static constexpr uint32_t  s_kBandAlpha    = 0xB0000000u;
static constexpr LPCWSTR   s_kpszSeamLabel = L"Write seam";
static constexpr uint32_t  s_kSelectionAlpha = 0x60000000u;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *  track = m_context.GetTrack();
    RECT                   bar   = GetBarRect();



    painter.FillRect (static_cast<float> (bar.left), static_cast<float> (bar.top), static_cast<float> (bar.right - bar.left), static_cast<float> (bar.bottom - bar.top),
                      m_context.palette.colors.nothingRecorded);

    if (track != nullptr && track->framed.cellCount > 0 && !track->framed.nibbles.empty())
    {
        PaintTrack (painter, text, theme, *track);
    }
    else if (m_context.hasDisk)
    {
        text.DrawString (L"Nothing recorded on this quarter track", static_cast<float> (bar.left), static_cast<float> (bar.top),
                         static_cast<float> (bar.right - bar.left), static_cast<float> (bar.bottom - bar.top), theme.ForegroundMuted(),
                         m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintTrack
//
//  Each nibble in its kind's color, or in Timing mode on a flux track its
//  cells' mean timing, with its value and then its kind once they fit and a
//  divider before it once its value fits (FR-033, FR-035). Zoomed in far
//  enough, the cells, the timing line on a flux track, then the write seam
//  and the sector labels.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintTrack (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track)
{
    const FramedTrack &          framed    = track.framed;
    StripGeometry                g         = MakeGeometry();
    RECT                         bar       = GetBarRect();
    std::array<StripSegment, 2>  parts     = {};
    float                        top       = static_cast<float> (bar.top);
    float                        height    = static_cast<float> (bar.bottom - bar.top);
    float                        textPx    = m_scaler.ToPxf (kSmallDip);
    bool                         isTimed   = IsTimed (track);
    bool                         showCells = GetCellPx (track) >= m_scaler.ToPxf (s_kCellDigitDip);
    float                        valueH    = showCells ? height * s_kValueShare : height;
    size_t                       i         = 0;
    int                          count     = 0;
    int                          k         = 0;
    uint32_t                     color     = 0;
    float                        w         = 0;
    float                        textW     = 0;
    float                        textH     = 0;
    std::wstring                 label;



    UpdateTurns (track);

    for (i = 0; i + 1 < m_turns.size(); i++)
    {
        count = g.GetSegments (m_turns[i], m_turns[i + 1] - m_turns[i], parts);
        color = m_context.palette.GetNibbleColor (track, static_cast<int> (i), m_context.isTimingMode, m_context.timingRange);

        for (k = 0; k < count; k++)
        {
            w = parts[k].x1 - parts[k].x0;
            painter.FillRect (parts[k].x0, top, std::max (w, 1.0f), height, color);

            if (w >= m_scaler.ToPxf (s_kValueMinPx))
            {
                painter.FillRect (parts[k].x0, top, 1.0f, height, theme.Background());

                label = std::format (L"{:02X} ", framed.nibbles[i].value)
                      + InspectorText::FormatNibbleKind (track.nibbleKinds[i], i < track.isFailedChecksum.size() && track.isFailedChecksum[i] != 0);
                text.MeasureString (label.c_str(), textPx, DxuiTheme::kMonoFace, textW, textH);

                if (textW > w - m_scaler.ToPxf (s_kPadDip))
                {
                    label = std::format (L"{:02X}", framed.nibbles[i].value);
                }

                text.DrawString (label.c_str(), parts[k].x0, top, w, valueH, DiskInspectorPalette::GetTextColorOn (color), textPx, DxuiTheme::kMonoFace,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            }
        }
    }

    for (const RandomRegion & region : framed.randomRegions)
    {
        double  ra = TrackAnalyzer::GetAngle (track, region.startCell);
        double  rb = TrackAnalyzer::GetAngle (track, region.startCell + region.cellCount);

        count = g.GetSegments (ra, (rb > ra) ? rb - ra : rb + 1.0 - ra, parts);

        for (k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, top, std::max (parts[k].x1 - parts[k].x0, 1.0f), height, m_context.palette.GetKindColor (PlatterKind::RandomBits));
        }
    }

    if (showCells)
    {
        PaintCells (painter, text, theme, track, g);
    }

    //  Zoomed in to cells, the line takes the cell band's empty upper row,
    //  until each cell's own timing fills that row in its place.
    if (isTimed && !showCells)
    {
        PaintTimingLine (painter, theme, track, g, top, top + height);
    }
    else if (isTimed && GetCellPx (track) < m_scaler.ToPxf (s_kCellTimingDip))
    {
        PaintTimingLine (painter, theme, track, g, top + valueH, top + valueH + (height - valueH) / 2);
    }

    PaintSelection (painter, theme, g);
    PaintSeam      (painter, text, theme, track, g);
    PaintLabels    (painter, text, theme, track, g);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintCells
//
//  Under the values, a band holding each cell in view: its 1 or 0 with a
//  pulse over each 1, and on a flux track its own timing once that fits
//  (FR-035).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintCells (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    const FramedTrack &          framed     = track.framed;
    RECT                         bar        = GetBarRect();
    std::array<StripSegment, 2>  parts      = {};
    float                        height     = static_cast<float> (bar.bottom - bar.top);
    float                        bandTop    = static_cast<float> (bar.top) + height * s_kValueShare;
    float                        bandH      = static_cast<float> (bar.bottom) - bandTop;
    float                        rowH       = bandH / 2;
    float                        textPx     = m_scaler.ToPxf (kSmallDip);
    bool                         showTiming = IsTimed (track) && GetCellPx (track) >= m_scaler.ToPxf (s_kCellTimingDip);
    uint32_t                     first      = FluxTiming::GetCellAt (m_cellTurns, g.GetTurn (static_cast<float> (bar.left)));
    uint32_t                     inView     = static_cast<uint32_t> (GetWidth() / std::max (GetCellPx (track), 1.0f)) + 2;
    uint32_t                     n          = 0;
    uint32_t                     cell       = 0;
    int                          count      = 0;
    int                          k          = 0;
    float                        cx         = 0;
    uint32_t                     band       = (theme.Background() & 0x00FFFFFFu) | s_kBandAlpha;



    painter.FillRect (static_cast<float> (bar.left), bandTop, GetWidth(), bandH, band);

    for (n = 0; n < inView && n < framed.cellCount; n++)
    {
        cell  = (first + n) % framed.cellCount;
        count = g.GetSegments (m_cellTurns[cell], m_cellTurns[cell + 1] - m_cellTurns[cell], parts);

        for (k = 0; k < count; k++)
        {
            cx = (parts[k].x0 + parts[k].x1) / 2;

            if (framed.cells[cell] != 0)
            {
                painter.DrawLine (cx, bandTop + m_scaler.ToPxf (2.0f), cx, bandTop + rowH, m_scaler.ToPxf (1.5f), theme.Foreground());
            }

            text.DrawString (framed.cells[cell] != 0 ? L"1" : L"0", parts[k].x0, bandTop + rowH, parts[k].x1 - parts[k].x0, rowH, theme.Foreground(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

            if (showTiming)
            {
                text.DrawString (InspectorFormat::FormatPercent (FluxTiming::GetDeviation (framed.cellTicks[cell])).c_str(), parts[k].x0, bandTop,
                                 parts[k].x1 - parts[k].x0, rowH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                                 DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintTimingLine
//
//  On a flux track, the mean cell timing under each few pixels, nominal in
//  the middle of the band from top to bottom, slow above and fast below,
//  with the timing range filling it (FR-033).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintTimingLine (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, float top, float bottom)
{
    RECT      bar    = GetBarRect();
    float     inset  = m_scaler.ToPxf (s_kLineInsetDip);
    float     mid    = (top + bottom) / 2.0f;
    float     half   = std::max ((bottom - top) / 2.0f - inset, 1.0f);
    float     step   = m_scaler.ToPxf (s_kLineStepDip);
    float     thick  = m_scaler.ToPxf (s_kLineDip);
    float     x      = static_cast<float> (bar.left);
    float     prevX  = 0;
    float     prevY  = 0;
    float     y      = 0;
    uint32_t  a      = 0;
    uint32_t  b      = 0;
    double    t      = 0;
    bool      isPrev = false;



    painter.DrawLine (static_cast<float> (bar.left), mid, static_cast<float> (bar.right), mid, 1.0f, theme.Border());

    for (x = static_cast<float> (bar.left); x < bar.right; x += step)
    {
        a = FluxTiming::GetCellAt (m_cellTurns, g.GetTurn (x));
        b = FluxTiming::GetCellAt (m_cellTurns, g.GetTurn (std::min (x + step, static_cast<float> (bar.right))));
        t = std::clamp (FluxTiming::GetMeanDeviation (track, a, (b == a) ? a + 1 : b) / std::max (m_context.timingRange, 1e-6), -1.0, 1.0);
        y = mid - static_cast<float> (t) * half;

        if (isPrev)
        {
            painter.DrawLine (prevX, prevY, x + step / 2, y, thick, theme.Foreground());
        }

        prevX  = x + step / 2;
        prevY  = y;
        isPrev = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintSeam
//
//  The longest sync run, the likely write seam (FR-033): a rule along the
//  bottom of the label row, with its label over it where that fits.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintSeam (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    RECT                         bar    = GetBarRect();
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = GetSeamSegments (track, g, parts);
    float                        rule   = m_scaler.ToPxf (s_kSeamRuleDip);
    float                        textPx = m_scaler.ToPxf (s_kLabelDip);
    float                        textW  = 0;
    float                        textH  = 0;
    int                          k      = 0;



    text.MeasureString (s_kpszSeamLabel, textPx, DxuiTheme::kBodyFace, textW, textH);

    for (k = 0; k < count; k++)
    {
        painter.FillRect (parts[k].x0, static_cast<float> (bar.top) - rule - 1.0f, std::max (parts[k].x1 - parts[k].x0, 1.0f), rule, theme.ForegroundMuted());

        if (parts[k].x1 - parts[k].x0 >= textW + m_scaler.ToPxf (s_kPadDip))
        {
            text.DrawString (s_kpszSeamLabel, parts[k].x0, static_cast<float> (m_boundsDip.top), parts[k].x1 - parts[k].x0,
                             static_cast<float> (bar.top - m_boundsDip.top) - rule, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetSeamSegments
//
////////////////////////////////////////////////////////////////////////////////

int TrackStripView::GetSeamSegments (const TrackAnalysis & track, const StripGeometry & g, std::array<StripSegment, 2> & outParts) const
{
    const SyncRun &  seam  = track.measurements.longestSync;
    size_t           n     = track.framed.nibbles.size();
    double           a     = 0;
    double           b     = 0;
    int              count = 0;



    //  A run's width in cells is each sync nibble's, so its extent comes from
    //  where its first nibble starts and where the nibble after it starts.
    if (seam.count > 0 && n > 0 && m_turns.size() == n + 1)
    {
        a     = m_turns[static_cast<size_t> (seam.firstNibble) % n];
        b     = m_turns[static_cast<size_t> (seam.firstNibble + seam.count) % n];
        count = g.GetSegments (a, b - a - std::floor (b - a), outParts);
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintLabels
//
//  Each sector's number over its address prologue, and the selected sector
//  outlined.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintLabels (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    RECT                         bar      = GetBarRect();
    std::array<StripSegment, 2>  parts    = {};
    int                          count    = 0;
    int                          k        = 0;
    size_t                       s        = 0;
    float                        rowTop   = static_cast<float> (m_boundsDip.top);
    float                        rowH     = static_cast<float> (bar.top - m_boundsDip.top);
    float                        thick    = m_scaler.ToPxf (s_kOutlineDip);
    double                       a        = 0;
    double                       b        = 0;
    std::wstring                 label;



    for (s = 0; s < track.sectors.size(); s++)
    {
        const AnalyzedSector &  sector = track.sectors[s];
        const LocatedField &    first  = track.fields[sector.addressField];
        const LocatedField &    last   = track.fields[sector.dataField >= 0 ? sector.dataField : sector.addressField];

        a     = m_turns[first.firstNibble];
        b     = (last.firstNibble + last.nibbleCount < static_cast<int> (m_turns.size())) ? m_turns[last.firstNibble + last.nibbleCount] : m_turns.back();
        label = std::wstring (DiskInspectorPalette::GetStateSymbol (sector.state)) + L" " + InspectorFormat::FormatSector (sector.sector).substr (1);

        count = g.GetSegments (a, 0.0001, parts);

        for (k = 0; k < count; k++)
        {
            text.DrawString (label.c_str(), parts[k].x0, rowTop, m_scaler.ToPxf (60), rowH, m_context.palette.GetStateColor (sector.state),
                             m_scaler.ToPxf (s_kLabelDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        }

        if (static_cast<int> (s) == m_context.model->GetSectorIndex())
        {
            count = g.GetSegments (a, b - a - std::floor (b - a), parts);

            for (k = 0; k < count; k++)
            {
                painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), parts[k].x1 - parts[k].x0, static_cast<float> (bar.bottom - bar.top), thick, theme.Accent());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::OnMouse (const DxuiMouseEvent & ev)
{
    const TrackAnalysis *  track     = m_context.GetTrack();
    POINT                  p         = ev.positionDip;
    bool                   isInside  = PtInRect (&m_boundsDip, p) != FALSE;
    bool                   isHandled = false;
    double                 span      = m_context.model->GetStripSpan();
    double                 fraction  = (p.x - m_boundsDip.left) / std::max (GetWidth(), 1.0f);
    double                 newSpan   = 0;
    int                    nibble    = -1;
    int64_t                now       = static_cast<int64_t> (GetTickCount64());



    switch (track != nullptr ? ev.kind : DxuiMouseEventKind::Leave)
    {
        case DxuiMouseEventKind::Wheel:
            if (isInside && !ev.wheelHorizontal && !ev.shift)
            {
                newSpan = std::clamp (span / std::pow (kWheelZoomStep, ev.wheelDelta), StripGeometry::kMinSpan, 1.0);
                m_context.model->SetStrip (StripGeometry::GetStartForZoom (m_context.model->GetStripStart(), span, newSpan, fraction), newSpan);
                isHandled = true;
            }
            else if (isInside)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - ev.wheelDelta * span / 8.0, span);
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Down:
            if (isInside && ev.button == DxuiMouseButton::Left && ev.shift)
            {
                UpdateTurns (*track);
                nibble        = GetNibbleAtX (*track, static_cast<float> (p.x));
                m_isSelecting = nibble >= 0;
                isHandled     = true;

                if (nibble >= 0 && m_context.model->GetNibbleCount() > 0)
                {
                    m_context.model->ExtendNibbles (nibble);
                }
                else if (nibble >= 0)
                {
                    m_context.model->SelectNibbles (m_context.model->GetQuarterTrack(), nibble, 1);
                }
            }
            else if (isInside && ev.button == DxuiMouseButton::Left)
            {
                m_isPressed = true;
                m_isPanning = false;
                m_pressAt   = p;
                m_lastAt    = p;
                isHandled   = true;
            }

            break;

        case DxuiMouseEventKind::Move:
            if (m_isSelecting)
            {
                UpdateTurns (*track);
                nibble = GetNibbleAtX (*track, std::clamp (static_cast<float> (p.x), static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.right - 1)));

                if (nibble >= 0)
                {
                    m_context.model->ExtendNibbles (nibble);
                }
            }

            if (m_isPressed && !m_isPanning && std::abs (p.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG) / 2)
            {
                m_isPanning = true;
            }

            if (m_isPanning)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - (p.x - m_lastAt.x) / std::max (GetWidth(), 1.0f) * span, span);
                m_lastAt = p;
            }

            isHandled = isInside || m_isPressed;
            break;

        case DxuiMouseEventKind::Up:
            isHandled = m_isPressed || m_isSelecting;

            if (m_isSelecting)
            {
                m_isSelecting = false;
                NotifySelection();
            }

            if (m_isPressed && !m_isPanning && now - m_lastClickMs <= static_cast<int64_t> (GetDoubleClickTime()))
            {
                ShowWholeTrack();
                m_lastClickMs = 0;
            }
            else if (m_isPressed && !m_isPanning)
            {
                m_lastClickMs = now;
                UpdateTurns (*track);
                nibble = GetNibbleAtX (*track, static_cast<float> (p.x));

                if (nibble >= 0)
                {
                    m_context.model->SelectNibbles (m_context.model->GetQuarterTrack(), nibble, 1);
                    NotifySelection();
                }
            }

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
//  TrackStripView::GetTooltip
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    RECT                   bar    = GetBarRect();
    int                    nibble = -1;



    if (track != nullptr && !m_isPanning && PtInRect (&bar, pointPx))
    {
        UpdateTurns (*track);
        nibble = GetNibbleAtX (*track, static_cast<float> (pointPx.x));
    }

    if (nibble >= 0)
    {
        outText     = InspectorText::FormatNibbleTooltip (*track, nibble);
        outAnchorPx = { pointPx.x, bar.top, pointPx.x + 1, bar.bottom };
    }
    else if (track != nullptr && !m_isPanning && pointPx.y >= m_boundsDip.top && pointPx.y < bar.top && IsOverSeam (*track, static_cast<float> (pointPx.x)))
    {
        outText     = std::format (L"Longest sync run, {} nibbles at cell {}\nThe likely write seam", track->measurements.longestSync.count,
                                   InspectorFormat::FormatCount (track->measurements.longestSync.startCell));
        outAnchorPx = { pointPx.x, m_boundsDip.top, pointPx.x + 1, bar.top };
        nibble      = 0;
    }

    return nibble >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::UpdateTurns
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::UpdateTurns (const TrackAnalysis & track) const
{
    if (m_turnsOf != &track)
    {
        StripGeometry::BuildNibbleTurns (track, m_turns);
        FluxTiming::BuildCellTurns (track, m_cellTurns);
        m_turnsOf = &track;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::MakeGeometry
//
////////////////////////////////////////////////////////////////////////////////

StripGeometry TrackStripView::MakeGeometry() const
{
    return StripGeometry (m_context.model->GetStripStart(), m_context.model->GetStripSpan(), static_cast<float> (m_boundsDip.left), GetWidth());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetBarRect
//
//  The colored bar under the row of sector labels.
//
////////////////////////////////////////////////////////////////////////////////

RECT TrackStripView::GetBarRect() const
{
    RECT  bar = m_boundsDip;



    bar.top = std::min (bar.bottom, bar.top + m_scaler.ToPx (s_kLabelRowDip));

    return bar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetNibbleAtX
//
////////////////////////////////////////////////////////////////////////////////

int TrackStripView::GetNibbleAtX (const TrackAnalysis & track, float xPx) const
{
    double  turn   = MakeGeometry().GetTurn (xPx);
    int     nibble = -1;



    if (m_turns.size() > 1)
    {
        //  The turns rise from the first nibble's, which can be just before
        //  the index, so the turn is brought into the same lap.
        turn -= std::floor (turn);
        turn += (turn < m_turns.front()) ? 1.0 : 0.0;

        auto  after = std::upper_bound (m_turns.begin(), m_turns.end() - 1, turn);

        nibble = (after == m_turns.begin()) ? static_cast<int> (m_turns.size()) - 2 : static_cast<int> (after - m_turns.begin()) - 1;
    }

    (void) track;

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::IsTimed
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::IsTimed (const TrackAnalysis & track)
{
    return track.framed.isFlux && track.framed.turnTicks > 0 && track.framed.cellTicks.size() >= track.framed.cellCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetCellPx
//
//  How wide a cell of the mean length is at the strip's zoom.
//
////////////////////////////////////////////////////////////////////////////////

float TrackStripView::GetCellPx (const TrackAnalysis & track) const
{
    return GetWidth() / static_cast<float> (m_context.model->GetStripSpan() * std::max<uint32_t> (track.framed.cellCount, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::IsOverSeam
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::IsOverSeam (const TrackAnalysis & track, float xPx) const
{
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = GetSeamSegments (track, MakeGeometry(), parts);
    int                          k      = 0;
    bool                         isOver = false;



    for (k = 0; k < count; k++)
    {
        isOver = isOver || (xPx >= parts[k].x0 && xPx < parts[k].x1);
    }

    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::ZoomAboutCenter
//
//  For the strip's buttons and keys; the zoom keeps the middle of the view
//  where it is.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::ZoomAboutCenter (double factor)
{
    double  span    = m_context.model->GetStripSpan();
    double  newSpan = std::clamp (span / factor, StripGeometry::kMinSpan, 1.0);



    m_context.model->SetStrip (StripGeometry::GetStartForZoom (m_context.model->GetStripStart(), span, newSpan, 0.5), newSpan);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PanBy
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PanBy (double fractionOfView)
{
    double  span = m_context.model->GetStripSpan();



    m_context.model->SetStrip (m_context.model->GetStripStart() + fractionOfView * span, span);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::ShowWholeTrack
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::ShowWholeTrack()
{
    m_context.model->SetStrip (0.0, 1.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetReadout
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackStripView::GetReadout() const
{
    const TrackAnalysis *  track = m_context.GetTrack();
    double                 start = m_context.model->GetStripStart();
    double                 span  = m_context.model->GetStripSpan();
    uint32_t               first = 0;
    uint32_t               last  = 0;



    if (track != nullptr && track->framed.cellCount > 0)
    {
        UpdateTurns (*track);
        first = (span >= 1.0) ? 0 : FluxTiming::GetCellAt (m_cellTurns, start);
        last  = (span >= 1.0) ? track->framed.cellCount - 1 : FluxTiming::GetCellAt (m_cellTurns, start + span);
    }

    return (track != nullptr && track->framed.cellCount > 0) ? InspectorText::FormatStripReadout (span, first, last) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintSelection
//
//  A run of selected nibbles shaded across the bar (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintSelection (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g)
{
    RECT                         bar   = GetBarRect();
    std::array<StripSegment, 2>  parts = {};
    int                          first = m_context.model->GetFirstNibble();
    int                          end   = first + m_context.model->GetNibbleCount();
    int                          count = 0;
    int                          k     = 0;



    if (first >= 0 && end > first && end < static_cast<int> (m_turns.size()))
    {
        count = g.GetSegments (m_turns[first], m_turns[end] - m_turns[first], parts);
    }

    for (k = 0; k < count; k++)
    {
        painter.FillRect (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 1.0f), static_cast<float> (bar.bottom - bar.top),
                          (theme.Accent() & 0x00FFFFFFu) | s_kSelectionAlpha);
    }
}
