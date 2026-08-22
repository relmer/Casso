#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiToggle.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

// The palette constants are private members of DxuiToggle.





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::HitTest (int x, int y) const
{
    // A disabled toggle is transparent to the mouse, so whatever is behind
    // it gets the click.
    return m_enabled
           && x >= m_boundsDip.left && x < m_boundsDip.right
           && y >= m_boundsDip.top  && y < m_boundsDip.bottom;
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
    bool  hit = HitTest (x, y);



    if (hit)
    {
        m_pressed = true;
    }

    return hit;
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
    bool  flips = m_enabled
                  && m_focused
                  && (vk == VK_SPACE || vk == VK_RETURN);



    if (flips)
    {
        Flip();
    }

    return flips;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Flip()
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
    constexpr uint32_t  s_kPillOff       = 0xFF4A5260;
    constexpr uint32_t  s_kPillOffHover  = 0xFF5A6271;
    constexpr uint32_t  s_kPillDisabled  = 0xFF2A2F38;
    constexpr uint32_t  s_kThumb         = 0xFFFFFFFF;
    constexpr uint32_t  s_kThumbDisabled = 0xFF707070;
    constexpr uint32_t  s_kTextIdle      = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled  = 0xFF707070;
    constexpr float     s_kPillWidthDip  = 36.0f;
    constexpr float     s_kPillHeightDip = 18.0f;
    constexpr float     s_kThumbInsetDip = 3.0f;
    constexpr float     s_kFocusInsetDip = -2.0f;
    constexpr float     s_kFocusThickDip = 1.0f;
    constexpr float     s_kLabelGapDip   = 8.0f;
    constexpr float     s_kFontDip       = 13.0f;
    constexpr float     s_kPillRatio     = 3.0f;    // WCAG 1.4.11 min contrast of pill vs white thumb



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
    uint32_t accentBase = DxuiColor::AccentForWhiteContrast (accentArgb, s_kPillRatio);
    uint32_t thumbColor = m_enabled ? s_kThumb : s_kThumbDisabled;
    uint32_t textColor  = m_enabled ? s_kTextIdle : s_kTextDisabled;

    // Same rule as DxuiCheckbox: no area means the control has not been laid
    // out yet (a WM_PAINT can land between OnCreate and the first Layout), and
    // the label box below is the width MINUS the pill and gap -- on a
    // {0,0,0,0} rect that is negative, which DWrite rejects outright.
    bool     hasArea    = (m_boundsDip.right > m_boundsDip.left)
                          && (m_boundsDip.bottom > m_boundsDip.top);



    if (hasArea)
    {
        if (!m_enabled)
        {
            pillColor = s_kPillDisabled;
        }
        else if (m_checked)
        {
            pillColor = m_hover ? DxuiColor::Scale (accentBase, kHoverLighten) : accentBase;
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

        // An unlabeled toggle narrates its own state instead, so the pill is
        // never left with nothing beside it.
        hr = text.DrawString (m_label.empty() ? (m_checked ? L"On" : L"Off")
                                              : m_label.c_str(),
                              pillLeft + pillW + labelGap,
                              (float) m_boundsDip.top,
                              (float) (m_boundsDip.right - m_boundsDip.left) - pillW - labelGap,
                              (float) (m_boundsDip.bottom - m_boundsDip.top),
                              textColor,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
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
    PaintInternal (painter, text, kDefaultAccentArgb, kDefaultFocusArgb);
}




void DxuiToggle::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    PaintInternal (painter, text, theme.Accent(), theme.FocusRing());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported unhandled, so the pointer
//  crossing the toggle does not consume moves other widgets want.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isLeft   = (ev.button == DxuiMouseButton::Left);
    bool  consumed = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        // Hover tracking never claims the event -- a toggle does not stop a
        // move from reaching whatever else wants to see it.
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Down:
        consumed = isLeft && OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Up:
        consumed = isLeft && OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        break;

    default:
        break;
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnKey (const DxuiKeyEvent & ev)
{
    // Key-up would flip a second time for the same press.
    return (ev.kind == DxuiKeyEventKind::Down) && OnKey (ev.vk);
}

