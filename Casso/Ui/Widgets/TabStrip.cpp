#include "Pch.h"

#include "TabStrip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelected
//
////////////////////////////////////////////////////////////////////////////////

void TabStrip::SetSelected (int index)
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

int TabStrip::HitTest (int x, int y) const
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

void TabStrip::SetMouseHover (int x, int y)
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

bool TabStrip::OnLButtonDown (int x, int y)
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

bool TabStrip::OnLButtonUp (int x, int y)
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

bool TabStrip::OnKey (WPARAM vk)
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

void TabStrip::Commit (int newIndex)
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

void TabStrip::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    constexpr uint32_t  s_kTabIdle      = 0xFF2A3445;
    constexpr uint32_t  s_kTabHover     = 0xFF38465E;
    constexpr uint32_t  s_kTabSelected  = 0xFF4C6480;
    constexpr uint32_t  s_kTextArgb     = 0xFFE8EEF4;
    constexpr uint32_t  s_kFocusRing    = 0xFFAACCFF;
    constexpr float     s_kFontDip      = 13.0f;
    constexpr float     s_kFocusThickDp = 1.0f;
    constexpr float     s_kFocusInsetDp = 1.0f;
    constexpr float     s_kPadXDp       = 8.0f;
    constexpr float     s_kPadYDp       = 4.0f;

    // When themed, the selected tab is a brightened tint of the hover
    // color so it stays the lightest of the three states across themes.
    auto  ScaleRgb = [] (uint32_t argb, float f) -> uint32_t
    {
        uint32_t  r = (uint32_t) std::min (255.0f, ((argb >> 16) & 0xFFu) * f);
        uint32_t  g = (uint32_t) std::min (255.0f, ((argb >>  8) & 0xFFu) * f);
        uint32_t  b = (uint32_t) std::min (255.0f, ( argb        & 0xFFu) * f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    };

    uint32_t  tabIdle     = m_theme ? m_theme->navStripArgb            : s_kTabIdle;
    uint32_t  tabHover    = m_theme ? m_theme->navHoverArgb            : s_kTabHover;
    uint32_t  tabSelected = m_theme ? ScaleRgb (m_theme->navHoverArgb, 1.45f) : s_kTabSelected;
    uint32_t  textArgb    = m_theme ? m_theme->navItemTextArgb         : s_kTextArgb;
    uint32_t  focusRing   = m_theme ? m_theme->linkArgb                : s_kFocusRing;

    HRESULT  hr         = S_OK;
    int      i          = 0;
    size_t   n          = m_tabs.size();
    float    focusThick = m_scaler.Pxf (s_kFocusThickDp);
    float    focusInset = m_scaler.Pxf (s_kFocusInsetDp);
    float    padX       = m_scaler.Pxf (s_kPadXDp);
    float    padY       = m_scaler.Pxf (s_kPadYDp);
    float    fontDip    = m_scaler.Pxf (s_kFontDip);



    for (i = 0; i < (int) n; ++i)
    {
        const Tab & t        = m_tabs[(size_t) i];
        uint32_t    fillArgb = (i == m_selected) ? tabSelected
                                : (i == m_hover ? tabHover : tabIdle);

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
                                 focusThick, focusRing);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (t.label.c_str(),
                                                  (float) t.rect.left + padX,
                                                  (float) t.rect.top  + padY,
                                                  (float) (t.rect.right  - t.rect.left) - padX * 2.0f,
                                                  (float) (t.rect.bottom - t.rect.top)  - padY * 2.0f,
                                                  textArgb,
                                                  fontDip,
                                                  L"Segoe UI",
                                                  DwriteTextRenderer::HAlign::Center,
                                                  DwriteTextRenderer::VAlign::Center));
    }
}
