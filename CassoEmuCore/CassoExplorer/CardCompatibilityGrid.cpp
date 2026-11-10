#include "Pch.h"

#include "CassoExplorer/CardCompatibilityGrid.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardCompatibilityGrid::MeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int CardCompatibilityGrid::MeasureHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    std::vector<CardLayoutModel::Row>  rows;
    size_t                             most = 0;



    UNREFERENCED_PARAMETER (widthPx);

    for (int position = 1; position <= m_model.GetPositionCount(); position++)
    {
        m_model.GetRows (position, m_adapter, rows);
        most = std::max (most, rows.size());
    }

    return (int) std::ceil (scaler.ToPxf (kLineDip * 2.0f + kGapDip + kRowDip * (float) most));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardCompatibilityGrid::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CardCompatibilityGrid::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler = scaler;

    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardCompatibilityGrid::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CardCompatibilityGrid::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT                               bounds = GetBounds();
    float                              left   = (float) bounds.left;
    float                              width  = (float) (bounds.right - bounds.left);
    float                              line   = m_scaler.ToPxf (kLineDip);
    float                              row    = m_scaler.ToPxf (kRowDip);
    float                              icon   = m_scaler.ToPxf (kIconDip);
    float                              font   = m_scaler.ToPxf (kFontDip);
    float                              nameW  = width * kAdapterShare;
    float                              noteX  = left + nameW + icon + m_scaler.ToPxf (kGapDip);
    float                              y      = (float) bounds.top;
    std::vector<CardLayoutModel::Row>  rows;



    text.DrawString (m_model.DescribeSpace (m_position).c_str(), left, y, width, line * 2.0f, theme.ForegroundMuted(), font, DxuiTheme::GetUiFace(),
                     DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);

    y += line * 2.0f + m_scaler.ToPxf (kGapDip);

    m_model.GetRows (m_position, m_adapter, rows);

    for (const CardLayoutModel::Row & each : rows)
    {
        painter.FillRect (left, y, width, 1.0f, theme.Divider());

        text.DrawString (each.adapter.c_str(), left, y + 2.0f, nameW, row, theme.Foreground(), font, DxuiTheme::GetUiFace(),
                         DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        text.DrawString (each.note.c_str(), noteX, y + 2.0f, left + width - noteX, row, theme.ForegroundMuted(), font, DxuiTheme::GetUiFace(),
                         DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);

        PaintIcon (painter, text, theme, each.support, left + nameW + icon * 0.5f, y + line * 0.5f);

        y += row;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardCompatibilityGrid::PaintIcon
//
////////////////////////////////////////////////////////////////////////////////

void CardCompatibilityGrid::PaintIcon (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, CardLayoutModel::Support support, float cx, float cy)
{
    float  r    = m_scaler.ToPxf (kIconDip) * 0.33f;
    float  font = m_scaler.ToPxf (kFontDip);



    switch (support)
    {
        case CardLayoutModel::Support::Works:
            painter.FillCircle (cx, cy, r, s_kWorksArgb);
            painter.DrawLine (cx - r * 0.5f, cy, cx - r * 0.1f, cy + r * 0.4f, 2.0f, 0xFFFFFFFF);
            painter.DrawLine (cx - r * 0.1f, cy + r * 0.4f, cx + r * 0.55f, cy - r * 0.4f, 2.0f, 0xFFFFFFFF);
            break;

        case CardLayoutModel::Support::Step:
            painter.FillConvexQuad (cx, cy - r, cx + r * 1.1f, cy + r * 0.85f, cx - r * 1.1f, cy + r * 0.85f, cx, cy - r, s_kStepArgb);
            painter.DrawLine (cx, cy - r * 0.35f, cx, cy + r * 0.3f, 2.0f, s_kWarningMarkArgb);
            painter.FillCircle (cx, cy + r * 0.6f, 1.2f, s_kWarningMarkArgb);
            break;

        case CardLayoutModel::Support::Fails:
            painter.FillCircle (cx, cy, r, s_kFailsArgb);
            painter.DrawLine (cx - r * 0.45f, cy - r * 0.45f, cx + r * 0.45f, cy + r * 0.45f, 2.0f, 0xFFFFFFFF);
            painter.DrawLine (cx - r * 0.45f, cy + r * 0.45f, cx + r * 0.45f, cy - r * 0.45f, 2.0f, 0xFFFFFFFF);
            break;

        default:
            painter.FillCircle (cx, cy, r, theme.ForegroundDisabled());
            text.DrawString (L"?", cx - r, cy - r, r * 2.0f, r * 2.0f, 0xFFFFFFFF, font, DxuiTheme::GetUiFace(),
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            break;
    }
}
