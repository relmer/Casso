#include "Pch.h"

#include "Ui/Debugger/Panes/MeterBar.h"

#include "Core/TextEncoding.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeterBar::GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int MeterBar::GetPreferredHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx ((kRowDip + kGapDip) * (int) m_meters.levels.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeterBar::GetFillWidth
//
////////////////////////////////////////////////////////////////////////////////

float MeterBar::GetFillWidth (float level) const
{
    float  bar = std::max (0.0f, (float) (m_boundsDip.right - m_boundsDip.left) - m_scaler.ToPxf ((float) kLabelDip));



    return bar * std::clamp (level, 0.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeterBar::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MeterBar::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeterBar::Paint
//
//  Rows that do not fit are left out rather than squeezed.
//
////////////////////////////////////////////////////////////////////////////////

void MeterBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font  = theme.MonospaceFont();
    float           row   = m_scaler.ToPxf ((float) kRowDip);
    float           step  = m_scaler.ToPxf ((float) (kRowDip + kGapDip));
    float           label = m_scaler.ToPxf ((float) kLabelDip);
    float           left  = (float) m_boundsDip.left;
    float           y     = (float) m_boundsDip.top;
    float           bar   = std::max (0.0f, (float) (m_boundsDip.right - m_boundsDip.left) - label);
    std::wstring    name;
    HRESULT         hr    = S_OK;



    if (!m_visible)
    {
        return;
    }

    for (const DiagnosticsMeters::Level & level : m_meters.levels)
    {
        if (y + row > (float) m_boundsDip.bottom)
        {
            break;
        }

        painter.FillRect (left + label, y, bar,                         row, theme.ControlBackground());
        painter.FillRect (left + label, y, GetFillWidth (level.level),  row, theme.Accent());

        name = TextEncoding::NarrowToWide (level.name);
        hr   = text.DrawString (name.c_str(), left, y, label, row, theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                                DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        y += step;
    }
}
