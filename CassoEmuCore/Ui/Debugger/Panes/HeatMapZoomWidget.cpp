#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapZoomWidget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetPercent
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapZoomWidget::GetPercent (int cellPx, int startCellPx)
{
    return (int) std::lround ((double) cellPx * kPercent / (double) std::max (1, startCellPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetPercentLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapZoomWidget::GetPercentLabel (int cellPx, int startCellPx)
{
    return std::format (L"{}%", GetPercent (cellPx, startCellPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetPosition
//
//  The cell size's logarithm, from one pixel's at the left to the largest's
//  at the right.
//
////////////////////////////////////////////////////////////////////////////////

float HeatMapZoomWidget::GetPosition (int cellPx, int maxCellPx)
{
    double  span = std::log ((double) std::max (2, maxCellPx));



    return (float) std::clamp (std::log ((double) std::max (1, cellPx)) / span, 0.0, 1.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetCellPxAtPosition
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapZoomWidget::GetCellPxAtPosition (float position, int maxCellPx)
{
    double  span = std::log ((double) std::max (2, maxCellPx));



    return std::clamp ((int) std::lround (std::exp (std::clamp ((double) position, 0.0, 1.0) * span)), 1, std::max (1, maxCellPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::Place
//
//  The button sits a margin in from the corner's bottom right.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapZoomWidget::Place (const RECT & corner, const RECT & bounds, const DxuiDpiScaler & scaler)
{
    long  margin = scaler.ToPx (kMarginDip);



    m_scaler = scaler;
    m_bounds = bounds;

    m_button.right  = corner.right  - margin;
    m_button.bottom = corner.bottom - margin;
    m_button.left   = m_button.right  - scaler.ToPx (kButtonWidthDip);
    m_button.top    = m_button.bottom - scaler.ToPx (kButtonHeightDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetPanelRect
//
//  Above the button, its right edge on the button's, and kept below the
//  view's top; empty while closed.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapZoomWidget::GetPanelRect() const
{
    RECT  panel = {};



    if (!m_isOpen)
    {
        return panel;
    }

    panel.right  = m_button.right;
    panel.left   = panel.right - m_scaler.ToPx (kPanelWidthDip);
    panel.bottom = m_button.top - m_scaler.ToPx (kMarginDip);
    panel.top    = std::max (m_bounds.top, panel.bottom - (long) m_scaler.ToPx (kPanelHeightDip));

    return panel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetResetRect
//
//  The panel's top row, at its right.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapZoomWidget::GetResetRect() const
{
    RECT  panel = GetPanelRect();
    long  pad   = m_scaler.ToPx (kPadDip);



    if (panel.right <= panel.left)
    {
        return {};
    }

    return { panel.right - pad - m_scaler.ToPx (kResetWidthDip), panel.top + pad / 2, panel.right - pad, panel.top + pad / 2 + m_scaler.ToPx (kRowDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetTrackRect
//
//  The panel's bottom row, inside its padding: the thumb's center runs from
//  the left edge to the right.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapZoomWidget::GetTrackRect() const
{
    RECT  panel = GetPanelRect();
    long  pad   = m_scaler.ToPx (kPadDip);
    long  row   = m_scaler.ToPx (kRowDip);



    if (panel.right <= panel.left)
    {
        return {};
    }

    return { panel.left + pad, panel.bottom - pad / 2 - row, panel.right - pad, panel.bottom - pad / 2 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::SetOpen
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapZoomWidget::SetOpen (bool open)
{
    m_isOpen = open;

    if (!open)
    {
        m_isDragging = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapZoomWidget::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::HitTest
//
////////////////////////////////////////////////////////////////////////////////

HeatMapZoomWidget::Hit HeatMapZoomWidget::HitTest (POINT point) const
{
    if (Contains (m_button, point))
    {
        return Hit::Button;
    }

    if (!m_isOpen || !Contains (GetPanelRect(), point))
    {
        return Hit::None;
    }

    if (Contains (GetResetRect(), point))
    {
        return Hit::Reset;
    }

    if (Contains (GetTrackRect(), point))
    {
        return Hit::Track;
    }

    return Hit::Panel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::GetCellPxAt
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapZoomWidget::GetCellPxAt (POINT point, int maxCellPx) const
{
    RECT   track = GetTrackRect();
    long   width = std::max (1L, track.right - track.left - 1);
    float  along = (float) (point.x - track.left) / (float) width;



    return GetCellPxAtPosition (along, maxCellPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::PaintBox
//
//  A filled rect with a one-pixel edge, all through the text renderer, so it
//  lands over the map's picture rather than under it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapZoomWidget::PaintBox (IDxuiTextRenderer & text, const RECT & rect, uint32_t fill, uint32_t edge)
{
    HRESULT  hr     = S_OK;
    float    left   = (float) rect.left;
    float    top    = (float) rect.top;
    float    right  = (float) rect.right;
    float    bottom = (float) rect.bottom;



    hr = text.FillRect (left, top, right - left, bottom - top, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillRect (left + 1.0f, top + 1.0f, right - left - 2.0f, bottom - top - 2.0f, fill);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget::Paint
//
//  The button with the percentage; open, the panel above it with the zoom
//  again, Reset, and the track with its thumb at the zoom.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapZoomWidget::Paint (IDxuiTextRenderer & text, const IDxuiTheme & theme, int cellPx, int startCellPx, int maxCellPx) const
{
    constexpr float  kTrackDip = 2.0f;
    HRESULT          hr        = S_OK;
    DxuiFontHandle   font      = theme.BodyFont();
    float            sizePx    = m_scaler.ToPxf (font.sizeDip);
    std::wstring     percent   = GetPercentLabel (cellPx, startCellPx);
    uint32_t         fill      = theme.BackgroundElevated();
    uint32_t         edge      = theme.Border();
    uint32_t         ink       = theme.Foreground();
    RECT             panel     = GetPanelRect();
    RECT             reset     = GetResetRect();
    RECT             track     = GetTrackRect();
    float            middle    = 0.0f;
    float            thumbX    = 0.0f;
    float            thumb     = m_scaler.ToPxf ((float) kThumbDip);



    if (m_button.right <= m_button.left)
    {
        return;
    }

    PaintBox (text, m_button, m_isOpen ? theme.PressedBackground() : fill, edge);

    hr = text.DrawString (percent.c_str(), (float) m_button.left, (float) m_button.top,
                          (float) (m_button.right - m_button.left), (float) (m_button.bottom - m_button.top),
                          ink, sizePx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!m_isOpen || panel.right <= panel.left)
    {
        return;
    }

    PaintBox (text, panel, fill, edge);

    hr = text.DrawString ((L"Zoom: " + percent).c_str(), (float) track.left, (float) reset.top,
                          (float) (reset.left - track.left), (float) (reset.bottom - reset.top),
                          ink, sizePx, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    PaintBox (text, reset, theme.ButtonIdle(), theme.ButtonBorder());

    hr = text.DrawString (L"Reset", (float) reset.left, (float) reset.top,
                          (float) (reset.right - reset.left), (float) (reset.bottom - reset.top),
                          theme.ButtonText(), sizePx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    middle = (float) (track.top + track.bottom) / 2.0f;
    thumbX = (float) track.left + GetPosition (cellPx, maxCellPx) * (float) (track.right - track.left - 1);

    hr = text.DrawLine ((float) track.left, middle, (float) track.right, middle, m_scaler.ToPxf (kTrackDip), theme.ForegroundMuted());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine ((float) track.left, middle, thumbX, middle, m_scaler.ToPxf (kTrackDip), theme.Accent());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillEllipse (thumbX, middle, thumb, thumb, theme.Accent());
    IGNORE_RETURN_VALUE (hr, S_OK);
}
