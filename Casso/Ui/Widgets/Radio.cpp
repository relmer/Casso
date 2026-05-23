#include "Pch.h"

#include "Radio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelected
//
////////////////////////////////////////////////////////////////////////////////

void RadioGroup::SetSelected (int index)
{
    if (index < 0 || index >= (int) m_options.size())
    {
        m_selected = -1;
        return;
    }

    m_selected = index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

int RadioGroup::HitTest (int x, int y) const
{
    int     i = 0;
    size_t  n = m_options.size();



    if (!m_enabled)
    {
        return -1;
    }

    for (i = 0; i < (int) n; ++i)
    {
        const RECT & r = m_options[(size_t) i].rect;

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

void RadioGroup::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);

    if (m_pressedIdx >= 0 && m_pressedIdx != m_hover)
    {
        m_pressedIdx = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool RadioGroup::OnLButtonDown (int x, int y)
{
    int  hit = HitTest (x, y);



    if (hit < 0)
    {
        return false;
    }

    m_pressedIdx = hit;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool RadioGroup::OnLButtonUp (int x, int y)
{
    int  hit      = HitTest (x, y);
    bool consumed = m_pressedIdx >= 0 && hit == m_pressedIdx;



    m_pressedIdx = -1;

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

bool RadioGroup::OnKey (WPARAM vk)
{
    int     next = m_selected;
    size_t  n    = m_options.size();



    if (!m_enabled || !m_focused || n == 0)
    {
        return false;
    }

    if (vk == VK_LEFT || vk == VK_UP)
    {
        next = (m_selected <= 0) ? (int) (n - 1) : m_selected - 1;
        Commit (next);
        return true;
    }

    if (vk == VK_RIGHT || vk == VK_DOWN)
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

void RadioGroup::Commit (int newIndex)
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

void RadioGroup::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    constexpr uint32_t  s_kBoxIdle      = 0xFF606060;
    constexpr uint32_t  s_kBoxHover     = 0xFF808080;
    constexpr uint32_t  s_kBoxDisabled  = 0xFF303030;
    constexpr uint32_t  s_kDot          = 0xFFFFFFFF;
    constexpr uint32_t  s_kDotDisabled  = 0xFF707070;
    constexpr uint32_t  s_kFocusRing    = 0xFFAACCFF;
    constexpr uint32_t  s_kTextIdle     = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled = 0xFF707070;
    constexpr float     s_kBoxSize      = 16.0f;
    constexpr float     s_kDotInset     = 4.0f;
    constexpr float     s_kFocusInset   = -2.0f;
    constexpr float     s_kFocusThick   = 1.0f;
    constexpr float     s_kLabelGap     = 6.0f;
    constexpr float     s_kFontDip      = 13.0f;

    HRESULT  hr        = S_OK;
    int      i         = 0;
    size_t   n         = m_options.size();
    uint32_t textColor = m_enabled ? s_kTextIdle : s_kTextDisabled;
    uint32_t dotColor  = m_enabled ? s_kDot      : s_kDotDisabled;



    for (i = 0; i < (int) n; ++i)
    {
        const RadioOption & opt      = m_options[(size_t) i];
        float               boxLeft  = (float) opt.rect.left;
        float               boxTop   = (float) opt.rect.top
                                       + ((float) (opt.rect.bottom - opt.rect.top) - s_kBoxSize) * 0.5f;
        uint32_t            boxColor = m_enabled
                                            ? (m_hover == i ? s_kBoxHover : s_kBoxIdle)
                                            : s_kBoxDisabled;

        painter.FillRect (boxLeft, boxTop, s_kBoxSize, s_kBoxSize, boxColor);

        if (m_selected == i)
        {
            painter.FillRect (boxLeft   + s_kDotInset,
                              boxTop    + s_kDotInset,
                              s_kBoxSize - s_kDotInset * 2.0f,
                              s_kBoxSize - s_kDotInset * 2.0f,
                              dotColor);
        }

        if (m_focused && m_selected == i)
        {
            painter.OutlineRect (boxLeft + s_kFocusInset,
                                 boxTop  + s_kFocusInset,
                                 s_kBoxSize - s_kFocusInset * 2.0f,
                                 s_kBoxSize - s_kFocusInset * 2.0f,
                                 s_kFocusThick,
                                 s_kFocusRing);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (opt.label.c_str(),
                                                  boxLeft + s_kBoxSize + s_kLabelGap,
                                                  (float) opt.rect.top,
                                                  (float) (opt.rect.right - opt.rect.left) - s_kBoxSize - s_kLabelGap,
                                                  (float) (opt.rect.bottom - opt.rect.top),
                                                  textColor,
                                                  s_kFontDip,
                                                  L"Segoe UI"));
    }
}
