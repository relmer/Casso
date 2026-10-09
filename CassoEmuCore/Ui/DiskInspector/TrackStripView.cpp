#include "Pch.h"

#include "Ui/DiskInspector/TrackStripView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





static constexpr int    s_kLabelRowDip     = 18;
static constexpr int    s_kDragThresholdPx = 4;
static constexpr float  s_kOutlineDip      = 2.0f;
static constexpr float  s_kValueMinPx      = 18.0f;
static constexpr float  s_kLabelDip        = 11.0f;





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
//  Only the nibbles in view are drawn, found by searching the turns.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintTrack (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track)
{
    StripGeometry                g      = MakeGeometry();
    RECT                         bar    = GetBarRect();
    std::array<StripSegment, 2>  parts  = {};
    float                        top    = static_cast<float> (bar.top);
    float                        height = static_cast<float> (bar.bottom - bar.top);
    size_t                       i      = 0;
    int                          count  = 0;
    int                          k      = 0;
    uint32_t                     color  = 0;
    float                        textPx = m_scaler.ToPxf (kSmallDip);



    UpdateTurns (track);

    for (i = 0; i + 1 < m_turns.size(); i++)
    {
        count = g.GetSegments (m_turns[i], m_turns[i + 1] - m_turns[i], parts);
        color = GetNibbleColor (track, static_cast<int> (i));

        for (k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, top, std::max (parts[k].x1 - parts[k].x0, 1.0f), height, color);

            if (parts[k].x1 - parts[k].x0 >= m_scaler.ToPxf (s_kValueMinPx))
            {
                text.DrawString (std::format (L"{:02X}", track.framed.nibbles[i].value).c_str(), parts[k].x0, top, parts[k].x1 - parts[k].x0, height,
                                 DiskInspectorPalette::GetTextColorOn (color), textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                                 DxuiFontWeight::Normal, false);
            }
        }
    }

    for (const RandomRegion & region : track.framed.randomRegions)
    {
        double  a = TrackAnalyzer::GetAngle (track, region.startCell);
        double  b = TrackAnalyzer::GetAngle (track, region.startCell + region.cellCount);

        count = g.GetSegments (a, (b > a) ? b - a : b + 1.0 - a, parts);

        for (k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, top, std::max (parts[k].x1 - parts[k].x0, 1.0f), height, m_context.palette.GetKindColor (PlatterKind::RandomBits));
        }
    }

    PaintLabels (painter, text, theme, track, g);
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
            if (isInside && ev.button == DxuiMouseButton::Left)
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
                m_context.model->SetStrip (m_context.model->GetStripStart() - (p.x - m_lastAt.x) / std::max (GetWidth(), 1.0f) * span, span);
                m_lastAt = p;
            }

            isHandled = isInside || m_isPressed;
            break;

        case DxuiMouseEventKind::Up:
            isHandled = m_isPressed;

            if (m_isPressed && !m_isPanning)
            {
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
        auto  after = std::upper_bound (m_turns.begin(), m_turns.end() - 1, turn);

        nibble = (after == m_turns.begin()) ? static_cast<int> (m_turns.size()) - 2 : static_cast<int> (after - m_turns.begin()) - 1;
    }

    (void) track;

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetNibbleColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t TrackStripView::GetNibbleColor (const TrackAnalysis & track, int nibble) const
{
    bool  isFailed = nibble < static_cast<int> (track.isFailedChecksum.size()) && track.isFailedChecksum[nibble] != 0;



    return m_context.palette.GetKindColor (isFailed ? PlatterKind::FailedChecksum : PlatterCells::GetKindOf (track.nibbleKinds[nibble]));
}
