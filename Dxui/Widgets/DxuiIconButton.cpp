#include "Pch.h"

#include "DxuiIconButton.h"




namespace
{
    constexpr float    s_kGlyphFontDip  = 12.0f;
    constexpr float    s_kFocusRingPx   = 1.5f;
    constexpr float    s_kFocusInsetPx  = -2.0f;
    constexpr float    s_kDoubleInset   = 2.0f;
    constexpr wchar_t  s_kMdl2Family[]  = L"Segoe MDL2 Assets";
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconButton
//
////////////////////////////////////////////////////////////////////////////////

DxuiIconButton::DxuiIconButton()
{
    m_focusable = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiIconButton::HitTest (int x, int y) const
{
    return m_visible && m_enabled &&
           x >= m_boundsDip.left && x < m_boundsDip.right &&
           y >= m_boundsDip.top  && y < m_boundsDip.bottom;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetMouse
//
////////////////////////////////////////////////////////////////////////////////

void DxuiIconButton::SetMouse (int x, int y, bool down)
{
    if (!m_visible || !m_enabled)
    {
        m_hover   = false;
        m_pressed = false;
    }
    else
    {
        m_hover   = HitTest (x, y);
        m_pressed = m_hover && down;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Click
//
////////////////////////////////////////////////////////////////////////////////

void DxuiIconButton::Click ()
{
    if (m_visible && m_enabled && m_click)
    {
        m_click();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiIconButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiIconButton::OnMouse (const DxuiMouseEvent & ev)
{
    bool  prevHover   = m_hover;
    bool  prevPressed = m_pressed;
    bool  leftBtn     = (ev.button == DxuiMouseButton::Left);
    bool  handled     = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouse (ev.positionDip.x, ev.positionDip.y, m_pressed);
        handled = (m_hover != prevHover) || (m_pressed != prevPressed);
        break;

    case DxuiMouseEventKind::Down:
        if (leftBtn)
        {
            SetMouse (ev.positionDip.x, ev.positionDip.y, true);
            handled = m_pressed;
        }
        break;

    case DxuiMouseEventKind::Up:
        if (leftBtn)
        {
            handled = m_pressed && HitTest (ev.positionDip.x, ev.positionDip.y);
            SetMouse (ev.positionDip.x, ev.positionDip.y, false);
            if (handled)
            {
                Click();
            }
        }
        break;

    default:
        break;
    }

    return handled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiIconButton::OnKey (const DxuiKeyEvent & ev)
{
    if (!m_visible || !m_enabled || !m_focused)
    {
        return false;
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    if (ev.vk == VK_SPACE || ev.vk == VK_RETURN)
    {
        Click();
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiIconButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT   hr         = S_OK;
    float     x          = (float) m_boundsDip.left;
    float     y          = (float) m_boundsDip.top;
    float     w          = (float) (m_boundsDip.right  - m_boundsDip.left);
    float     h          = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float     glyphDip   = m_scaler.Pxf (s_kGlyphFontDip);
    float     focusInset = m_scaler.Pxf (s_kFocusInsetPx);
    float     focusThick = m_scaler.Pxf (s_kFocusRingPx);
    uint32_t  glyphArgb  = m_enabled ? theme.ForegroundMuted() : theme.ForegroundDisabled();



    BAIL_OUT_IF (!m_visible, S_OK);

    if (m_enabled && (m_hover || m_pressed))
    {
        painter.FillRect (x, y, w, h, theme.HoverBackground());
        glyphArgb = theme.Accent();
    }

    hr = text.DrawString (m_glyph,
                          x,
                          y,
                          w,
                          h,
                          glyphArgb,
                          glyphDip,
                          s_kMdl2Family,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_focused)
    {
        painter.OutlineRect (x + focusInset,
                             y + focusInset,
                             w - focusInset * s_kDoubleInset,
                             h - focusInset * s_kDoubleInset,
                             focusThick,
                             theme.FocusRing());
    }

Error:
    return;
}
