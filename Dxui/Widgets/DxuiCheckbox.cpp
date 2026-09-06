#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiCheckbox.h"

#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCheckbox::HitTest (int x, int y) const
{
    // A disabled checkbox is transparent to the mouse, so whatever is behind
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
    bool  toggles = m_enabled
                    && m_focused
                    && (vk == VK_SPACE || vk == VK_RETURN);



    if (toggles)
    {
        Toggle();
    }

    return toggles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Toggle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCheckbox::Toggle()
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

void DxuiCheckbox::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  s_kBoxSizeDip    = 16.0f;
    constexpr float  s_kFocusInsetDip = -2.0f;
    constexpr float  s_kFocusThickDip = 1.0f;
    constexpr float  s_kLabelGapDip   = 6.0f;
    constexpr float  s_kFontDip       = 13.0f;
    uint32_t         glyphColor       = 0;
    uint32_t         textColor        = 0;



    HRESULT  hr          = S_OK;
    float    boxSize     = m_scaler.ToPxf (s_kBoxSizeDip);
    float    focusInset  = m_scaler.ToPxf (s_kFocusInsetDip);
    float    focusThick  = m_scaler.ToPxf (s_kFocusThickDip);
    float    labelGap    = m_scaler.ToPxf (s_kLabelGapDip);
    float    fontDip     = m_scaler.ToPxf (s_kFontDip);
    float    boxLeft     = (float) m_boundsDip.left;
    float    boxTop      = (float) m_boundsDip.top + ((float) (m_boundsDip.bottom - m_boundsDip.top) - boxSize) * 0.5f;
    uint32_t boxColor    = m_enabled
                            ? (m_pressed ? theme.ButtonPressed() : (m_hover ? theme.ButtonHover() : theme.ButtonIdle()))
                            : theme.PressedBackground();
    glyphColor = m_enabled ? theme.ButtonText()  : theme.ForegroundDisabled();
    textColor = m_enabled ? theme.Foreground()  : theme.ForegroundDisabled();

    // No area means this control has not been laid out yet: OnCreate() builds
    // the tree, but the first Layout() waits on the host sizing the window,
    // and a WM_PAINT can arrive in between. Nothing can render into a
    // zero-area rect, and the label box below is derived by SUBTRACTING fixed
    // insets from the width -- on a {0,0,0,0} rect that goes negative and
    // reaches DWrite as an invalid text-layout extent.
    bool     hasArea     = (m_boundsDip.right > m_boundsDip.left)
                           && (m_boundsDip.bottom > m_boundsDip.top);



    if (hasArea)
    {
        float  labelX = 0.0f;
        float  labelW = 0.0f;

        painter.FillRect (boxLeft, boxTop, boxSize, boxSize, boxColor);

        if (m_checked)
        {
            hr = text.DrawString (s_kpszCheckMark,
                                  boxLeft,
                                  boxTop,
                                  boxSize,
                                  boxSize,
                                  glyphColor,
                                  boxSize * 0.95f,
                                  L"Segoe UI Symbol",
                                  DxuiTextHAlign::Center,
                                  DxuiTextVAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (m_focused)
        {
            painter.OutlineRect (boxLeft + focusInset,
                                 boxTop  + focusInset,
                                 boxSize - focusInset * 2.0f,
                                 boxSize - focusInset * 2.0f,
                                 focusThick,
                                 theme.FocusRing());
        }

        labelX = boxLeft + boxSize + labelGap;
        labelW = (float) (m_boundsDip.right - m_boundsDip.left) - boxSize - labelGap;
        std::wstring  drawn  = DxuiTextElide::ToWidth (text, m_label, fontDip, DxuiTheme::kBodyFace,
                                                       labelW,
                                                       m_singleLineLabel ? DxuiElide::Tail
                                                                         : DxuiElide::None);

        hr = text.DrawString (drawn.c_str(),
                              labelX,
                              (float) m_boundsDip.top,
                              labelW,
                              (float) (m_boundsDip.bottom - m_boundsDip.top),
                              textColor,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              !m_singleLineLabel);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCheckbox::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCheckbox::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCheckbox::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported unhandled, so the pointer
//  crossing the checkbox does not consume moves other widgets want.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCheckbox::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isLeft   = (ev.button == DxuiMouseButton::Left);
    bool  consumed = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        // Hover tracking never claims the event -- a checkbox does not stop
        // a move from reaching whatever else wants to see it.
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
//  DxuiCheckbox::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCheckbox::OnKey (const DxuiKeyEvent & ev)
{
    // Key-up would toggle a second time for the same press.
    return (ev.kind == DxuiKeyEventKind::Down) && OnKey (ev.vk);
}
