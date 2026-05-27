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


bool Button::OnKey (WPARAM vk)
{
    if (!m_focused)
    {
        return false;
    }

    if (vk == VK_RETURN || vk == VK_SPACE)
    {
        Click();
        return true;
    }

    return false;
}


void Button::Paint (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme)
{
    constexpr uint32_t  s_kFocusRingArgb = 0xFFAACCFF;
    constexpr float     s_kFocusRingPx   = 1.5f;
    constexpr float     s_kFocusInsetPx  = -2.0f;

    HRESULT  hr        = S_OK;
    uint32_t themeIdle    = m_useOverrides ? m_idleOverride    : theme.sysButtonIdleArgb;
    uint32_t themeHover   = m_useOverrides ? m_hoverOverride   : theme.sysButtonHoverArgb;
    uint32_t themePressed = m_useOverrides ? m_pressedOverride : theme.sysButtonPressedArgb;
    uint32_t color        = m_pressed ? themePressed : (m_hover ? themeHover : themeIdle);
    uint32_t textColor    = m_useTextOverride ? m_textOverride : theme.navItemTextArgb;
    float    fontDip      = m_scaler.Pxf (13.0f);



    painter.FillRect ((float) m_rect.left,
                      (float) m_rect.top,
                      (float) (m_rect.right  - m_rect.left),
                      (float) (m_rect.bottom - m_rect.top),
                      color);

    if (m_outlineThick > 0.0f)
    {
        painter.OutlineRect ((float) m_rect.left,
                             (float) m_rect.top,
                             (float) (m_rect.right  - m_rect.left),
                             (float) (m_rect.bottom - m_rect.top),
                             m_outlineThick,
                             m_outlineArgb);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                              (float) m_rect.left,
                                              (float) m_rect.top,
                                              (float) (m_rect.right  - m_rect.left),
                                              (float) (m_rect.bottom - m_rect.top),
                                              textColor,
                                              fontDip,
                                              L"Segoe UI",
                                              DwriteTextRenderer::HAlign::Center,
                                              DwriteTextRenderer::VAlign::Center));

    if (m_focused)
    {
        float  focusInset = m_scaler.Pxf (s_kFocusInsetPx);
        float  focusThick = m_scaler.Pxf (s_kFocusRingPx);

        painter.OutlineRect ((float) m_rect.left + focusInset,
                             (float) m_rect.top  + focusInset,
                             (float) (m_rect.right  - m_rect.left) - focusInset * 2.0f,
                             (float) (m_rect.bottom - m_rect.top)  - focusInset * 2.0f,
                             focusThick,
                             s_kFocusRingArgb);
    }
}
