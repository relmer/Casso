#include "Pch.h"

#include "Checkbox.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool Checkbox::HitTest (int x, int y) const
{
    if (!m_enabled)
    {
        return false;
    }
    return x >= m_rect.left && x < m_rect.right && y >= m_rect.top && y < m_rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void Checkbox::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
    if (!m_hover)
    {
        m_pressed = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool Checkbox::OnLButtonDown (int x, int y)
{
    if (!HitTest (x, y))
    {
        return false;
    }

    m_pressed = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool Checkbox::OnLButtonUp (int x, int y)
{
    bool  consumed = m_pressed && HitTest (x, y);



    m_pressed = false;

    if (consumed)
    {
        Toggle();
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool Checkbox::OnKey (WPARAM vk)
{
    if (!m_enabled || !m_focused)
    {
        return false;
    }

    if (vk == VK_SPACE || vk == VK_RETURN)
    {
        Toggle();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Toggle
//
////////////////////////////////////////////////////////////////////////////////

void Checkbox::Toggle ()
{
    m_checked = !m_checked;

    if (m_change)
    {
        m_change (m_checked);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Hard-coded neutral palette for now. Slice C / P4 wires real theme
//  swatches in once the settings-panel theme surface lands.
//
////////////////////////////////////////////////////////////////////////////////

void Checkbox::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    constexpr uint32_t  s_kBoxIdle      = 0xFF606060;
    constexpr uint32_t  s_kBoxHover     = 0xFF808080;
    constexpr uint32_t  s_kBoxPressed   = 0xFF404040;
    constexpr uint32_t  s_kBoxDisabled  = 0xFF303030;
    constexpr uint32_t  s_kCheck        = 0xFFFFFFFF;
    constexpr uint32_t  s_kCheckDisabled = 0xFF707070;
    constexpr uint32_t  s_kFocusRing    = 0xFFAACCFF;
    constexpr uint32_t  s_kTextIdle     = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled = 0xFF707070;
    constexpr float     s_kBoxSize      = 16.0f;
    constexpr float     s_kCheckInset   = 3.0f;
    constexpr float     s_kFocusInset   = -2.0f;
    constexpr float     s_kFocusThick   = 1.0f;
    constexpr float     s_kLabelGap     = 6.0f;
    constexpr float     s_kFontDip      = 13.0f;

    HRESULT  hr        = S_OK;
    float    boxLeft   = (float) m_rect.left;
    float    boxTop    = (float) m_rect.top + ((float) (m_rect.bottom - m_rect.top) - s_kBoxSize) * 0.5f;
    uint32_t boxColor  = m_enabled
                            ? (m_pressed ? s_kBoxPressed : (m_hover ? s_kBoxHover : s_kBoxIdle))
                            : s_kBoxDisabled;
    uint32_t glyphColor = m_enabled ? s_kCheck : s_kCheckDisabled;
    uint32_t textColor  = m_enabled ? s_kTextIdle : s_kTextDisabled;



    painter.FillRect (boxLeft, boxTop, s_kBoxSize, s_kBoxSize, boxColor);

    if (m_checked)
    {
        painter.FillRect (boxLeft   + s_kCheckInset,
                          boxTop    + s_kCheckInset,
                          s_kBoxSize - s_kCheckInset * 2.0f,
                          s_kBoxSize - s_kCheckInset * 2.0f,
                          glyphColor);
    }

    if (m_focused)
    {
        painter.OutlineRect (boxLeft + s_kFocusInset,
                             boxTop  + s_kFocusInset,
                             s_kBoxSize - s_kFocusInset * 2.0f,
                             s_kBoxSize - s_kFocusInset * 2.0f,
                             s_kFocusThick,
                             s_kFocusRing);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                              boxLeft + s_kBoxSize + s_kLabelGap,
                                              (float) m_rect.top,
                                              (float) (m_rect.right - m_rect.left) - s_kBoxSize - s_kLabelGap,
                                              (float) (m_rect.bottom - m_rect.top),
                                              textColor,
                                              s_kFontDip,
                                              L"Segoe UI"));
}
