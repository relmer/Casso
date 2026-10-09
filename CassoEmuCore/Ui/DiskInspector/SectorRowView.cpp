#include "Pch.h"

#include "Ui/DiskInspector/SectorRowView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr LPCWSTR  s_kpszLabel = L"Sectors, in the order they pass the head";





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
    float                  labelH   = m_scaler.ToPxf (static_cast<float> (kLabelDip));
    vector<int>            order;
    size_t                 i        = 0;
    RECT                   r        = {};
    std::wstring           caption;
    uint32_t               color    = 0;
    bool                   selected = false;



    if (m_context.hasDisk && m_context.analysis != nullptr)
    {
        text.DrawString (s_kpszLabel, x, y, GetWidth(), labelH, theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        order = (track != nullptr) ? GetPassingOrder (*track) : vector<int>();

        if (order.empty())
        {
            caption = InspectorText::FormatNoSectors (m_context.analysis->entries[m_context.model->GetQuarterTrack()]);
            text.DrawString (caption.c_str(), x, y + labelH, GetWidth(), m_scaler.ToPxf (static_cast<float> (kRowDip)), theme.Foreground(),
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



    if (position >= 0)
    {
        outText     = InspectorText::FormatSectorTooltip (track->sectors[GetPassingOrder (*track)[position]], *track);
        outAnchorPx = GetButtonRect (position);
    }

    return position >= 0;
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
//  SectorRowView::GetButtonRect
//
//  Buttons run left to right and wrap onto further rows.
//
////////////////////////////////////////////////////////////////////////////////

RECT SectorRowView::GetButtonRect (int position) const
{
    int  w       = m_scaler.ToPx (kButtonDip);
    int  h       = m_scaler.ToPx (kRowDip);
    int  gap     = m_scaler.ToPx (kGapDip);
    int  perRow  = std::max (1, static_cast<int> ((GetWidth() + gap) / (w + gap)));
    int  col     = position % perRow;
    int  row     = position / perRow;
    int  left    = m_boundsDip.left + col * (w + gap);
    int  top     = m_boundsDip.top + m_scaler.ToPx (kLabelDip) + row * (h + gap);



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
