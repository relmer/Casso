#include "Pch.h"

#include "Widgets/DxuiTabStrip.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelected
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::SetSelected (int index)
{
    if (m_tabs.empty())
    {
        m_selected = -1;
        return;
    }

    if (index < 0) { index = 0; }
    if (index >= (int) m_tabs.size()) { index = (int) m_tabs.size() - 1; }

    m_selected = index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::HitTest (int x, int y) const
{
    int     i = 0;
    size_t  n = m_tabs.size();



    if (!m_enabled)
    {
        return -1;
    }

    for (i = 0; i < (int) n; ++i)
    {
        const RECT & r = m_tabs[(size_t) i].rect;

        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
        {
            return i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);

    if (m_pressed >= 0 && m_pressed != m_hover)
    {
        m_pressed = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnLButtonDown (int x, int y)
{
    int  hit = HitTest (x, y);



    if (hit < 0)
    {
        return false;
    }

    m_pressed = hit;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnLButtonUp (int x, int y)
{
    int   hit      = HitTest (x, y);
    bool  consumed = (m_pressed >= 0) && (hit == m_pressed);



    m_pressed = -1;

    if (consumed)
    {
        Commit (hit);
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnKey (WPARAM vk)
{
    int     next = m_selected;
    size_t  n    = m_tabs.size();



    if (!m_enabled || !m_focused || n == 0)
    {
        return false;
    }

    if (vk == VK_LEFT)
    {
        next = (m_selected <= 0) ? (int) (n - 1) : m_selected - 1;
        Commit (next);
        return true;
    }

    if (vk == VK_RIGHT)
    {
        next = (m_selected < 0 || m_selected >= (int) n - 1) ? 0 : m_selected + 1;
        Commit (next);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Commit (int newIndex)
{
    bool  changed = (newIndex != m_selected);



    m_selected = newIndex;

    if (changed && m_change)
    {
        m_change (newIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr uint32_t  s_kTabIdle     = 0xFF2A3445;
    constexpr uint32_t  s_kTabHover    = 0xFF38465E;
    constexpr uint32_t  s_kTabSelected = 0xFF4C6480;
    constexpr uint32_t  s_kTextArgb    = 0xFFE8EEF4;
    constexpr uint32_t  s_kFocusRing   = 0xFFAACCFF;

    PaintInternal (painter, text, s_kTabIdle, s_kTabHover, s_kTabSelected, s_kTextArgb, s_kFocusRing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintInternal
//
//  Renders the tab row with caller-supplied colors so the themed and
//  fallback Paint entry points share one body.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                  uint32_t idleArgb, uint32_t hoverArgb, uint32_t selectedArgb,
                                  uint32_t textArgb, uint32_t focusArgb) const
{
    constexpr float     s_kFontDip       = 13.0f;
    constexpr float     s_kFocusThickDip = 1.0f;
    constexpr float     s_kFocusInsetDip = 1.0f;
    constexpr float     s_kPadXDp        = 8.0f;
    constexpr float     s_kPadYDp        = 4.0f;
    constexpr float     s_kPressedScale  = 0.82f;   // armed-tab tint, a touch darker than hover

    HRESULT  hr          = S_OK;
    int      i           = 0;
    size_t   n           = m_tabs.size();
    float    focusThick  = m_scaler.Pxf (s_kFocusThickDip);
    float    focusInset  = m_scaler.Pxf (s_kFocusInsetDip);
    float    padX        = m_scaler.Pxf (s_kPadXDp);
    float    padY        = m_scaler.Pxf (s_kPadYDp);
    float    fontDip     = m_scaler.Pxf (s_kFontDip);
    uint32_t pressedArgb = DxuiColor::Darken (hoverArgb, s_kPressedScale);



    for (i = 0; i < (int) n; ++i)
    {
        const Tab & t        = m_tabs[(size_t) i];
        uint32_t    fillArgb = (i == m_selected)               ? selectedArgb
                                : (i == m_pressed && i == m_hover) ? pressedArgb
                                : (i == m_hover)                ? hoverArgb
                                :                                  idleArgb;

        painter.FillRect ((float) t.rect.left,
                          (float) t.rect.top,
                          (float) (t.rect.right  - t.rect.left),
                          (float) (t.rect.bottom - t.rect.top),
                          fillArgb);

        if (m_focused && i == m_selected)
        {
            painter.OutlineRect ((float) t.rect.left + focusInset,
                                 (float) t.rect.top  + focusInset,
                                 (float) (t.rect.right  - t.rect.left) - focusInset * 2.0f,
                                 (float) (t.rect.bottom - t.rect.top)  - focusInset * 2.0f,
                                 focusThick, focusArgb);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (t.label.c_str(),
                                                  (float) t.rect.left + padX,
                                                  (float) t.rect.top  + padY,
                                                  (float) (t.rect.right  - t.rect.left) - padX * 2.0f,
                                                  (float) (t.rect.bottom - t.rect.top)  - padY * 2.0f,
                                                  textArgb,
                                                  fontDip,
                                                  L"Segoe UI",
                                                  DxuiTextHAlign::Center,
                                                  DxuiTextVAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::Layout  (IDxuiControl override)
//
//  Per-tab rects are populated by the caller via SetTabs; the override
//  records the group bounds for IDxuiControl::Bounds() consumers.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  s_kIdleScale     = 0.6f;
    constexpr float  s_kSelectedScale = 1.45f;

    uint32_t  hover = theme.HoverBackground();



    // Selected tab is the lightest (brightened hover); idle is a darker
    // tint of the same hue, so the strip tracks the theme accent.
    PaintInternal (painter, text,
                   DxuiColor::Scale (hover, s_kIdleScale),
                   hover,
                   DxuiColor::Scale (hover, s_kSelectedScale),
                   theme.Foreground(),
                   theme.FocusRing());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouse (const DxuiMouseEvent & ev)
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
//  DxuiTabStrip::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::AccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected tab (or empty if none).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTabStrip::AccessibleName () const
{
    if (m_selected < 0 || m_selected >= (int) m_tabs.size())
    {
        return L"";
    }

    return m_tabs[(size_t) m_selected].label;
}
