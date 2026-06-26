#include "Pch.h"

#include "Widgets/DxuiToggle.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr uint32_t  s_kDefaultAccentArgb = 0xFF2D7CDB;   // "on" pill
    constexpr uint32_t  s_kDefaultFocusArgb  = 0xFFAACCFF;   // focus ring
    constexpr float     s_kHoverLighten      = 1.15f;        // "on" pill hover brighten


    //  Lighten each RGB channel toward white by factor f (>1 brightens),
    //  preserving alpha. Used for the hovered "on" pill shade.
    uint32_t LightenRgb (uint32_t argb, float f)
    {
        uint32_t  r = (uint32_t) std::min (255.0f, (float) ((argb >> 16) & 0xFFu) * f);
        uint32_t  g = (uint32_t) std::min (255.0f, (float) ((argb >>  8) & 0xFFu) * f);
        uint32_t  b = (uint32_t) std::min (255.0f, (float) ( argb        & 0xFFu) * f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::HitTest (int x, int y) const
{
    if (!m_enabled)
    {
        return false;
    }
    return x >= m_boundsDip.left && x < m_boundsDip.right && y >= m_boundsDip.top && y < m_boundsDip.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::SetMouseHover (int x, int y)
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

bool DxuiToggle::OnLButtonDown (int x, int y)
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

bool DxuiToggle::OnLButtonUp (int x, int y)
{
    bool  consumed = m_pressed && HitTest (x, y);



    m_pressed = false;

    if (consumed)
    {
        Flip();
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnKey (WPARAM vk)
{
    if (!m_enabled || !m_focused)
    {
        return false;
    }

    if (vk == VK_SPACE || vk == VK_RETURN)
    {
        Flip();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Flip ()
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
//  Pill body painted as one rect plus two end-cap circles. Thumb is a
//  circle that sits inset from the pill edges and slides between the
//  off (left) and on (right) end-caps.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t accentArgb, uint32_t focusArgb) const
{
    constexpr uint32_t  s_kPillOff      = 0xFF4A5260;
    constexpr uint32_t  s_kPillOffHover = 0xFF5A6271;
    constexpr uint32_t  s_kPillDisabled = 0xFF2A2F38;
    constexpr uint32_t  s_kThumb        = 0xFFFFFFFF;
    constexpr uint32_t  s_kThumbDisabled= 0xFF707070;
    constexpr uint32_t  s_kTextIdle     = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled = 0xFF707070;
    constexpr float     s_kPillWidthDip  = 36.0f;
    constexpr float     s_kPillHeightDip = 18.0f;
    constexpr float     s_kThumbInsetDip = 3.0f;
    constexpr float     s_kFocusInsetDip = -2.0f;
    constexpr float     s_kFocusThickDip = 1.0f;
    constexpr float     s_kLabelGapDip   = 8.0f;
    constexpr float     s_kFontDip      = 13.0f;

    HRESULT  hr         = S_OK;
    float    pillW      = m_scaler.Pxf (s_kPillWidthDip);
    float    pillH      = m_scaler.Pxf (s_kPillHeightDip);
    float    thumbInset = m_scaler.Pxf (s_kThumbInsetDip);
    float    focusInset = m_scaler.Pxf (s_kFocusInsetDip);
    float    focusThick = m_scaler.Pxf (s_kFocusThickDip);
    float    labelGap   = m_scaler.Pxf (s_kLabelGapDip);
    float    fontDip    = m_scaler.Pxf (s_kFontDip);
    float    pillLeft   = (float) m_boundsDip.left;
    float    pillTop    = (float) m_boundsDip.top + ((float) (m_boundsDip.bottom - m_boundsDip.top) - pillH) * 0.5f;
    float    capR       = pillH * 0.5f;
    float    leftCx     = pillLeft + capR;
    float    rightCx    = pillLeft + pillW - capR;
    float    cy         = pillTop  + capR;
    float    thumbR     = capR - thumbInset;
    float    thumbCx    = m_checked ? rightCx : leftCx;
    uint32_t pillColor;
    uint32_t thumbColor = m_enabled ? s_kThumb : s_kThumbDisabled;
    uint32_t textColor  = m_enabled ? s_kTextIdle : s_kTextDisabled;



    if (!m_enabled)
    {
        pillColor = s_kPillDisabled;
    }
    else if (m_checked)
    {
        pillColor = m_hover ? LightenRgb (accentArgb, s_kHoverLighten) : accentArgb;
    }
    else
    {
        pillColor = m_hover ? s_kPillOffHover : s_kPillOff;
    }

    painter.FillRect         (leftCx,  pillTop, pillW - pillH, pillH, pillColor);
    painter.FillCircleApprox (leftCx,  cy,      capR,          pillColor);
    painter.FillCircleApprox (rightCx, cy,      capR,          pillColor);
    painter.FillCircleApprox (thumbCx, cy,      thumbR,        thumbColor);

    if (m_focused)
    {
        painter.OutlineRect (pillLeft + focusInset,
                             pillTop  + focusInset,
                             pillW    - focusInset * 2.0f,
                             pillH    - focusInset * 2.0f,
                             focusThick,
                             focusArgb);
    }

    if (!m_label.empty())
    {
        IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                                  pillLeft + pillW + labelGap,
                                                  (float) m_boundsDip.top,
                                                  (float) (m_boundsDip.right - m_boundsDip.left) - pillW - labelGap,
                                                  (float) (m_boundsDip.bottom - m_boundsDip.top),
                                                  textColor,
                                                  fontDip,
                                                  L"Segoe UI",
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
    else
    {
        const wchar_t * stateText = m_checked ? L"On" : L"Off";

        IGNORE_RETURN_VALUE (hr, text.DrawString (stateText,
                                                  pillLeft + pillW + labelGap,
                                                  (float) m_boundsDip.top,
                                                  (float) (m_boundsDip.right - m_boundsDip.left) - pillW - labelGap,
                                                  (float) (m_boundsDip.bottom - m_boundsDip.top),
                                                  textColor,
                                                  fontDip,
                                                  L"Segoe UI",
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::Paint
//
//  Non-themed overload keeps the default blue accent; the IDxuiControl
//  themed override tints the "on" pill from theme.Accent() and the focus
//  ring from theme.FocusRing().
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    PaintInternal (painter, text, s_kDefaultAccentArgb, s_kDefaultFocusArgb);
}




void DxuiToggle::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    PaintInternal (painter, text, theme.Accent(), theme.FocusRing());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        return false;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}
