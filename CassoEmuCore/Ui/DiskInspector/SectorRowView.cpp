#include "Pch.h"

#include "Ui/DiskInspector/SectorRowView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr LPCWSTR  s_kpszLabel  = L"Sectors, in the order they pass the head";
static constexpr float    s_kHitDotDip = 3.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SectorRowView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *  track    = m_context.GetTrack();
    float                  x        = static_cast<float> (m_boundsDip.left);
    float                  y        = static_cast<float> (m_boundsDip.top);
    vector<int>            order;
    size_t                 i        = 0;
    RECT                   r        = {};
    std::wstring           caption;
    uint32_t               color    = 0;
    bool                   selected = false;



    if (m_context.hasDisk && m_context.analysis != nullptr)
    {
        order = (track != nullptr) ? GetPassingOrder (*track) : vector<int>();

        if (order.empty())
        {
            caption = InspectorText::FormatNoSectors (m_context.analysis->entries[m_context.model->GetQuarterTrack()]);
            text.DrawString (caption.c_str(), x, y, GetWidth(), m_scaler.ToPxf (static_cast<float> (kRowDip)), theme.Foreground(),
                             m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        for (i = 0; i < order.size(); i++)
        {
            const AnalyzedSector &  sector = track->sectors[order[i]];

            r        = GetButtonRect (static_cast<int> (i));
            color    = m_context.palette.GetStateColor (sector.state);
            selected = order[i] == m_context.model->GetSectorIndex();
            caption  = std::wstring (DiskInspectorPalette::GetStateSymbol (sector.state)) + InspectorFormat::FormatSector (sector.sector).substr (1);

            painter.FillRoundedRect (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                                     DxuiTheme::kCornerRadiusDip, selected ? theme.SelectionBackground() : (m_hover == static_cast<int> (i) ? theme.ButtonHover() : theme.ButtonIdle()));
            painter.OutlineRoundedRect (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                                        DxuiTheme::kCornerRadiusDip, m_scaler.ToPxf (selected ? 2.0f : 1.0f), selected ? theme.Accent() : color);
            text.DrawString (caption.c_str(), static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                             color, m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

            //  A corner mark at the top left of a sector with pending edits (FR-097).
            if (m_context.edits != nullptr && m_context.edits->HasChanges (track->slot, sector.sector))
            {
                painter.FillConvexQuad (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.left) + m_scaler.ToPxf (3.0f * s_kHitDotDip), static_cast<float> (r.top),
                                        static_cast<float> (r.left), static_cast<float> (r.top) + m_scaler.ToPxf (3.0f * s_kHitDotDip),
                                        static_cast<float> (r.left), static_cast<float> (r.top) + m_scaler.ToPxf (3.0f * s_kHitDotDip), m_context.palette.colors.pendingEdit);
            }

            //  A dot in the top right corner of a sector with find hits (FR-056).
            if (m_context.findMarks != nullptr && m_context.findMarks->GetSectorHits (track, order[i]) > 0)
            {
                painter.FillCircle (static_cast<float> (r.right) - m_scaler.ToPxf (2.0f * s_kHitDotDip), static_cast<float> (r.top) + m_scaler.ToPxf (2.0f * s_kHitDotDip),
                                    m_scaler.ToPxf (s_kHitDotDip + 1.0f), DiskInspectorPalette::GetTextColorOn (m_context.palette.colors.findHit));
                painter.FillCircle (static_cast<float> (r.right) - m_scaler.ToPxf (2.0f * s_kHitDotDip), static_cast<float> (r.top) + m_scaler.ToPxf (2.0f * s_kHitDotDip),
                                    m_scaler.ToPxf (s_kHitDotDip), m_context.palette.colors.findHit);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool SectorRowView::OnMouse (const DxuiMouseEvent & ev)
{
    const TrackAnalysis *  track     = m_context.GetTrack();
    bool                   isInside  = PtInRect (&m_boundsDip, ev.positionDip) != FALSE;
    bool                   isHandled = false;
    int                    position  = (track != nullptr && isInside) ? HitTest (ev.positionDip, *track) : -1;



    if (ev.kind == DxuiMouseEventKind::Move)
    {
        m_hover   = position;
        isHandled = isInside;
    }
    else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && position >= 0)
    {
        m_context.model->SelectSector (m_context.model->GetQuarterTrack(), GetPassingOrder (*track)[position]);
        NotifySelection();
        isHandled = true;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::GetTooltip
//
////////////////////////////////////////////////////////////////////////////////

bool SectorRowView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track    = m_context.GetTrack();
    int                    position = (track != nullptr) ? HitTest (pointPx, *track) : -1;



    bool                   hasTip   = position >= 0 || (track != nullptr && PtInRect (&m_boundsDip, pointPx) != FALSE);
    int                    hits     = 0;



    if (position >= 0)
    {
        outText     = InspectorText::FormatSectorTooltip (track->sectors[GetPassingOrder (*track)[position]], *track);
        outAnchorPx = GetButtonRect (position);
        hits        = (m_context.findMarks != nullptr) ? m_context.findMarks->GetSectorHits (track, GetPassingOrder (*track)[position]) : 0;
        outText    += (hits > 0) ? L"\n" + m_context.findMarks->FormatHitLine (hits) : L"";
    }
    else if (hasTip)
    {
        outText     = s_kpszLabel;
        outAnchorPx = m_boundsDip;
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::GetTooltipSwatches
//
//  The find hit color beside the line counting a sector's hits, its last.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorRowView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    const TrackAnalysis *  track    = m_context.GetTrack();
    int                    position = (track != nullptr) ? HitTest (pointPx, *track) : -1;
    std::wstring           text;
    RECT                   anchor   = {};
    bool                   hasHits  = position >= 0 && m_context.findMarks != nullptr && m_context.findMarks->GetSectorHits (track, GetPassingOrder (*track)[position]) > 0;



    if (hasHits && GetTooltip (pointPx, text, anchor))
    {
        outSwatches = { { static_cast<int> (std::count (text.begin(), text.end(), L'\n')), m_context.palette.colors.findHit } };
    }

    return hasHits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::GetPassingOrder
//
////////////////////////////////////////////////////////////////////////////////

vector<int> SectorRowView::GetPassingOrder (const TrackAnalysis & track)
{
    vector<int>  order (track.sectors.size());
    size_t       i     = 0;



    for (i = 0; i < order.size(); i++)
    {
        order[i] = static_cast<int> (i);
    }

    std::stable_sort (order.begin(), order.end(), [&track] (int a, int b) { return track.sectors[a].passingIndex < track.sectors[b].passingIndex; });

    return order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::GetRowCount
//
//  Buttons as wide as the width allows for one row each, down to a minimum,
//  then the rest on further rows.
//
////////////////////////////////////////////////////////////////////////////////

int SectorRowView::GetRowCount (int widthPx, const DxuiDpiScaler & scaler) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    count  = (track != nullptr) ? std::max (static_cast<int> (track->sectors.size()), 1) : 1;
    int                    gap    = scaler.ToPx (kGapDip);
    int                    w      = std::clamp ((widthPx + gap) / count - gap, scaler.ToPx (kMinButtonDip), scaler.ToPx (kButtonDip));
    int                    perRow = std::max (1, (widthPx + gap) / (w + gap));



    return (count + perRow - 1) / perRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::GetButtonRect
//
//  Buttons run left to right and wrap onto further rows.
//
////////////////////////////////////////////////////////////////////////////////

RECT SectorRowView::GetButtonRect (int position) const
{
    const TrackAnalysis *  track   = m_context.GetTrack();
    int                    count   = (track != nullptr) ? std::max (static_cast<int> (track->sectors.size()), 1) : 1;
    int                    h       = m_scaler.ToPx (kRowDip);
    int                    gap     = m_scaler.ToPx (kGapDip);
    int                    w       = std::clamp (static_cast<int> ((GetWidth() + gap) / count) - gap, m_scaler.ToPx (kMinButtonDip), m_scaler.ToPx (kButtonDip));
    int                    perRow  = std::max (1, static_cast<int> ((GetWidth() + gap) / (w + gap)));
    int                    col     = position % perRow;
    int                    row     = position / perRow;
    int                    left    = m_boundsDip.left + col * (w + gap);
    int                    top     = m_boundsDip.top + row * (h + gap);



    return { left, top, left + w, top + h };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView::HitTest
//
////////////////////////////////////////////////////////////////////////////////

int SectorRowView::HitTest (POINT pointPx, const TrackAnalysis & track) const
{
    int   position = -1;
    int   i        = 0;
    RECT  r        = {};



    for (i = 0; i < static_cast<int> (track.sectors.size()) && position < 0; i++)
    {
        r = GetButtonRect (i);

        if (PtInRect (&r, pointPx))
        {
            position = i;
        }
    }

    return position;
}
