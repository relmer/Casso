#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTabStrip.h"
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
    int     i   = 0;
    size_t  n   = m_tabs.size();
    int     hit = -1;



    if (m_enabled)
    {
        for (i = 0; i < (int) n && hit < 0; ++i)
        {
            const RECT & r = m_tabs[(size_t) i].rect;

            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            {
                hit = i;
            }
        }
    }

    return hit;
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
    int   hit    = HitTest (x, y);
    bool  wasHit = (hit >= 0);



    if (wasHit)
    {
        m_pressed = hit;
    }

    return wasHit;
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
//  Left / Right arrow navigation between tabs.
//
//  Only the horizontal axis is bound, unlike the radio group's four -- a tab
//  strip is always laid out horizontally, so Up and Down belong to whatever
//  the tab is displaying.
//
//  Selection WRAPS at both ends, matching the platform convention for tabs.
//
//  Moving the selection COMMITS it, so arrowing through tabs switches pages as
//  it goes. That is what a tab strip does; there is no separate activation
//  step to require.
//
//  An unselected strip enters at the first tab going right and the last going
//  left, so the first key press always lands somewhere sensible.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnKey (WPARAM vk)
{
    int     next     = m_selected;
    size_t  n        = m_tabs.size();
    bool    isActive = false;
    bool    handled  = false;



    isActive = m_enabled && m_focused && n != 0;

    if (isActive && vk == VK_LEFT)
    {
        next    = (m_selected <= 0) ? (int) (n - 1) : m_selected - 1;
        handled = true;
    }
    else if (isActive && vk == VK_RIGHT)
    {
        next    = (m_selected < 0 || m_selected >= (int) n - 1) ? 0 : m_selected + 1;
        handled = true;
    }

    if (handled)
    {
        Commit (next);
    }

    return handled;
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

    constexpr float     s_kUnderlineDip  = 3.0f;   // thick active-tab underline
    constexpr float     s_kMutedTextScale = 0.62f; // dim inactive labels

    HRESULT  hr          = S_OK;
    int      i           = 0;
    size_t   n           = m_tabs.size();
    float    focusThick  = m_scaler.Pxf (s_kFocusThickDip);
    float    focusInset  = m_scaler.Pxf (s_kFocusInsetDip);
    float    padX        = m_scaler.Pxf (s_kPadXDp);
    float    padY        = m_scaler.Pxf (s_kPadYDp);
    float    fontDip     = m_scaler.Pxf (s_kFontDip);
    float    underline   = m_scaler.Pxf (s_kUnderlineDip);
    uint32_t mutedText   = DxuiColor::Scale (textArgb, s_kMutedTextScale);

    UNREFERENCED_PARAMETER (idleArgb);   // idle + selected tabs blend with the page



    // Modern connected-tab look: the active tab shares the page background (no
    // chip fill) and is marked by a thick accent underline flush with the page
    // edge; inactive tabs are unfilled with dimmed labels, and a hovered /
    // armed inactive tab gets a subtle fill hint.
    for (i = 0; i < (int) n; ++i)
    {
        const Tab & t       = m_tabs[(size_t) i];
        bool        isSel    = (i == m_selected);
        bool        isHover  = (i == m_hover);
        bool        isArmed  = (i == m_pressed && i == m_hover);

        if (!isSel && (isHover || isArmed))
        {
            painter.FillRect ((float) t.rect.left,
                              (float) t.rect.top,
                              (float) (t.rect.right  - t.rect.left),
                              (float) (t.rect.bottom - t.rect.top),
                              isArmed ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
        }

        if (isSel)
        {
            painter.FillRect ((float) t.rect.left,
                              (float) t.rect.bottom - underline,
                              (float) (t.rect.right - t.rect.left),
                              underline,
                              selectedArgb);
        }

        if (m_focused && isSel)
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
                                                  isSel ? textArgb : mutedText,
                                                  fontDip,
                                                  DxuiTheme::kBodyFace,
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
    constexpr float  s_kIdleScale = 0.6f;



    uint32_t  hover = theme.HoverBackground();



    // Underline-style strip: the "selected" slot carries the accent used for
    // the active-tab underline (the theme selection color), hover is a subtle
    // fill hint, idle is unused (idle tabs blend with the page).
    PaintInternal (painter, text,
                   DxuiColor::Scale (hover, s_kIdleScale),
                   hover,
                   theme.SelectionBackground(),
                   theme.Foreground(),
                   theme.FocusRing());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported unhandled, so the pointer
//  crossing the strip does not consume moves other widgets want.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        break;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        break;
    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::AccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected tab (or empty if none).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTabStrip::AccessibleName() const
{
    std::wstring  name;
    bool          hasSelection = false;



    hasSelection = m_selected >= 0 && m_selected < (int) m_tabs.size();

    if (hasSelection)
    {
        name = m_tabs[(size_t) m_selected].label;
    }

    return name;
}
