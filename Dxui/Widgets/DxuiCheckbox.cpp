#include "Pch.h"

#include "Widgets/DxuiCheckbox.h"

#include "UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCheckbox::HitTest (int x, int y) const
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

void DxuiCheckbox::SetMouseHover (int x, int y)
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

bool DxuiCheckbox::OnLButtonDown (int x, int y)
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

bool DxuiCheckbox::OnLButtonUp (int x, int y)
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

bool DxuiCheckbox::OnKey (WPARAM vk)
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
//  DxuiToggle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCheckbox::Toggle ()
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

void DxuiCheckbox::Paint (DxuiPainter & painter, DxuiTextRenderer & text) const
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
    constexpr float     s_kBoxSizeDip    = 16.0f;
    constexpr float     s_kFocusInsetDip = -2.0f;
    constexpr float     s_kFocusThickDip = 1.0f;
    constexpr float     s_kLabelGapDip   = 6.0f;
    constexpr float     s_kFontDip      = 13.0f;

    HRESULT  hr          = S_OK;
    float    boxSize     = m_scaler.Pxf (s_kBoxSizeDip);
    float    focusInset  = m_scaler.Pxf (s_kFocusInsetDip);
    float    focusThick  = m_scaler.Pxf (s_kFocusThickDip);
    float    labelGap    = m_scaler.Pxf (s_kLabelGapDip);
    float    fontDip     = m_scaler.Pxf (s_kFontDip);
    float    boxLeft     = (float) m_rect.left;
    float    boxTop      = (float) m_rect.top + ((float) (m_rect.bottom - m_rect.top) - boxSize) * 0.5f;
    uint32_t boxColor    = m_enabled
                            ? (m_pressed ? s_kBoxPressed : (m_hover ? s_kBoxHover : s_kBoxIdle))
                            : s_kBoxDisabled;
    uint32_t glyphColor  = m_enabled ? s_kCheck : s_kCheckDisabled;
    uint32_t textColor   = m_enabled ? s_kTextIdle : s_kTextDisabled;



    painter.FillRect (boxLeft, boxTop, boxSize, boxSize, boxColor);

    if (m_checked)
    {
        IGNORE_RETURN_VALUE (hr, text.DrawString (s_kpszCheckMark,
                                                  boxLeft,
                                                  boxTop,
                                                  boxSize,
                                                  boxSize,
                                                  glyphColor,
                                                  boxSize * 0.95f,
                                                  L"Segoe UI Symbol",
                                                  DxuiTextRenderer::HAlign::Center,
                                                  DxuiTextRenderer::VAlign::Center));
    }

    if (m_focused)
    {
        painter.OutlineRect (boxLeft + focusInset,
                             boxTop  + focusInset,
                             boxSize - focusInset * 2.0f,
                             boxSize - focusInset * 2.0f,
                             focusThick,
                             s_kFocusRing);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                              boxLeft + boxSize + labelGap,
                                              (float) m_rect.top,
                                              (float) (m_rect.right - m_rect.left) - boxSize - labelGap,
                                              (float) (m_rect.bottom - m_rect.top),
                                              textColor,
                                              fontDip,
                                              L"Segoe UI",
                                              DxuiTextRenderer::HAlign::Left,
                                              DxuiTextRenderer::VAlign::Center));
}
