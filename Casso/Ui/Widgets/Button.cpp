#include "Pch.h"

#include "Button.h"





bool Button::HitTest (int x, int y) const
{
    return x >= m_rect.left && x < m_rect.right && y >= m_rect.top && y < m_rect.bottom;
}


void Button::SetMouse (int x, int y, bool down)
{
    m_hover   = HitTest (x, y);
    m_pressed = m_hover && down;
}


void Button::Click ()
{
    if (m_click)
    {
        m_click();
    }
}


void Button::Paint (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme)
{
    HRESULT  hr        = S_OK;
    uint32_t color     = m_pressed ? theme.sysButtonPressedArgb : (m_hover ? theme.sysButtonHoverArgb : theme.sysButtonIdleArgb);
    float    textPadX  = (float) m_scaler.Px (8);
    float    textPadY  = (float) m_scaler.Px (4);
    float    fontDip   = m_scaler.Pxf (13.0f);



    painter.FillRect ((float) m_rect.left, (float) m_rect.top, (float) (m_rect.right - m_rect.left), (float) (m_rect.bottom - m_rect.top), color);
    IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(), (float) m_rect.left + textPadX, (float) m_rect.top + textPadY, (float) (m_rect.right - m_rect.left), (float) (m_rect.bottom - m_rect.top), theme.navItemTextArgb, fontDip, L"Segoe UI"));
}
