#include "Pch.h"

#include "Widgets/DxuiDropdown.h"





namespace
{
    constexpr uint32_t  s_kFocusRingArgb   = 0xFFAACCFF;
    constexpr float     s_kFocusRingPx     = 1.5f;
    constexpr float     s_kFocusInsetPx    = -2.0f;
    constexpr int       s_kRowHeightDip     = 28;
    constexpr int       s_kTextInsetDip     = 8;
    constexpr int       s_kChevronWidthDip  = 10;
    constexpr int       s_kChevronHeightDip = 5;
    constexpr int       s_kChevronRightDip  = 10;
    constexpr uint32_t  s_kBoxIdleArgb     = 0xFF263241;
    constexpr uint32_t  s_kBoxHoverArgb    = 0xFF33475C;
    constexpr uint32_t  s_kBoxPressedArgb  = 0xFF1E2733;
    constexpr uint32_t  s_kBoxDisabledArgb = 0xFF1C242F;
    constexpr uint32_t  s_kMenuArgb        = 0xFF202A35;
    constexpr uint32_t  s_kMenuHoverArgb   = 0xFF34475F;
    constexpr uint32_t  s_kTextArgb        = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabledArgb = 0xFF6A7585;
    constexpr uint32_t  s_kEdgeArgb        = 0xFF5C7088;
    constexpr uint32_t  s_kEdgeDisabledArgb = 0xFF364252;
    constexpr float     s_kEdgePx          = 1.0f;
    constexpr float     s_kFontDip         = 13.0f;
    constexpr wchar_t   s_kFontFamily[]    = L"Segoe UI";


    bool RectContains (const RECT & rect, int x, int y)
    {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetItems
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetItems (const std::vector<std::wstring> & items)
{
    m_items = items;

    if (m_selected >= (int) m_items.size())
    {
        m_selected = m_items.empty() ? -1 : 0;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetSelected
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetSelected (int index)
{
    if (index < 0 || index >= (int) m_items.size())
    {
        m_selected = m_items.empty() ? -1 : 0;
        return;
    }

    m_selected = index;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Open
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Open()
{
    m_open      = true;
    m_highlight = (m_selected >= 0) ? m_selected : (m_items.empty() ? -1 : 0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::HitTest (int x, int y) const
{
    return RectContains (m_rect, x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ItemHitTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDropdown::ItemHitTest (int x, int y) const
{
    RECT  menuRect   = m_rect;
    int   index      = -1;
    int   rowHeight  = m_scaler.Px (s_kRowHeightDip);



    menuRect.top    = m_rect.bottom;
    menuRect.bottom = m_rect.bottom + (int) m_items.size() * rowHeight;

    if (!m_open || !RectContains (menuRect, x, y))
    {
        return -1;
    }

    index = (y - menuRect.top) / rowHeight;
    if (index < 0 || index >= (int) m_items.size())
    {
        return -1;
    }

    return index;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetMouseHover (int x, int y)
{
    int  item = ItemHitTest (x, y);



    if (!m_enabled)
    {
        m_hover   = false;
        m_pressed = false;
        return;
    }

    m_hover = HitTest (x, y);

    if (item >= 0 && item != m_highlight)
    {
        m_highlight = item;
        if (m_highlightChange)
        {
            m_highlightChange (m_highlight);
        }
    }

    if (!m_hover && item < 0)
    {
        m_pressed = false;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnLButtonDown (int x, int y)
{
    if (!m_enabled)
    {
        return false;
    }

    if (HitTest (x, y))
    {
        m_pressed = true;
        return true;
    }

    if (ItemHitTest (x, y) >= 0)
    {
        return true;
    }

    if (m_open)
    {
        Close();
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnLButtonUp (int x, int y)
{
    int   item       = ItemHitTest (x, y);
    bool  wasPressed = m_pressed;



    if (!m_enabled)
    {
        return false;
    }

    m_pressed = false;

    if (item >= 0)
    {
        Commit (item);
        Close();
        return true;
    }

    if (wasPressed && HitTest (x, y))
    {
        if (m_open)
        {
            Close();
        }
        else
        {
            Open();
        }
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::HandleKey (WPARAM vk)
{
    int  count = (int) m_items.size();



    if (!m_enabled || count <= 0)
    {
        return false;
    }

    if (!m_open)
    {
        if (!m_focused)
        {
            return false;
        }

        if (vk == VK_RETURN || vk == VK_SPACE || vk == VK_DOWN)
        {
            Open();
            return true;
        }

        return false;
    }

    if (vk == VK_DOWN)
    {
        m_highlight = (m_highlight + 1) % count;
        if (m_highlightChange) { m_highlightChange (m_highlight); }
        return true;
    }

    if (vk == VK_UP)
    {
        m_highlight = (m_highlight + count - 1) % count;
        if (m_highlightChange) { m_highlightChange (m_highlight); }
        return true;
    }

    if (vk == VK_RETURN)
    {
        Commit (m_highlight);
        Close();
        return true;
    }

    if (vk == VK_ESCAPE)
    {
        Close();
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Commit (int index)
{
    bool  changed = index != m_selected;



    if (index < 0 || index >= (int) m_items.size())
    {
        return;
    }

    m_selected = index;

    if (changed && m_select)
    {
        m_select (index);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Convenience that paints both the base box and (if open) the popup
//  menu in a single call. Callers that need correct z-order with
//  multiple dropdowns should use PaintBase / PaintMenu separately.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    PaintBase (painter, text);
    PaintMenu (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintBase
//
//  Paints the closed-box portion of the dropdown: background fill,
//  border, selected-item text, and a chevron glyph on the right that
//  signals click-to-open.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::PaintBase (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT      hr            = S_OK;
    uint32_t     boxColor      = m_enabled
                                     ? (m_pressed ? s_kBoxPressedArgb : (m_hover ? s_kBoxHoverArgb : s_kBoxIdleArgb))
                                     : s_kBoxDisabledArgb;
    uint32_t     textColor     = m_enabled ? s_kTextArgb : s_kTextDisabledArgb;
    uint32_t     edgeColor     = m_enabled ? s_kEdgeArgb : s_kEdgeDisabledArgb;
    std::wstring label;
    float        edgePx        = m_scaler.Pxf (s_kEdgePx);
    float        fontDip       = m_scaler.Pxf (s_kFontDip);
    int          textInset     = m_scaler.Px (s_kTextInsetDip);
    int          chevronW      = m_scaler.Px (s_kChevronWidthDip);
    int          chevronH      = m_scaler.Px (s_kChevronHeightDip);
    int          chevronRight  = m_scaler.Px (s_kChevronRightDip);
    int          chevronX      = m_rect.right - chevronRight - chevronW;
    int          chevronY      = (m_rect.top + m_rect.bottom) / 2 - chevronH / 2;
    int          textWidth     = (m_rect.right - m_rect.left) - textInset - (chevronRight + chevronW);



    if (m_selected >= 0 && m_selected < (int) m_items.size())
    {
        label = m_items[(size_t) m_selected];
    }

    if (textWidth < 0)
    {
        textWidth = 0;
    }

    painter.FillRect    ((float) m_rect.left,
                         (float) m_rect.top,
                         (float) (m_rect.right - m_rect.left),
                         (float) (m_rect.bottom - m_rect.top),
                         boxColor);
    painter.OutlineRect ((float) m_rect.left,
                         (float) m_rect.top,
                         (float) (m_rect.right - m_rect.left),
                         (float) (m_rect.bottom - m_rect.top),
                         edgePx,
                         edgeColor);
    IGNORE_RETURN_VALUE (hr, text.DrawString (label.c_str(),
                                              (float) (m_rect.left + textInset),
                                              (float) m_rect.top,
                                              (float) textWidth,
                                              (float) (m_rect.bottom - m_rect.top),
                                              textColor,
                                              fontDip,
                                              s_kFontFamily,
                                              DxuiTextHAlign::Left,
                                              DxuiTextVAlign::Center));

    // Chevron: stack of horizontal rects forming a downward triangle.
    for (int row = 0; row < chevronH; row++)
    {
        int  inset = (row * chevronW) / (2 * chevronH);
        int  w     = chevronW - inset * 2;

        if (w <= 0) break;

        painter.FillRect ((float) (chevronX + inset),
                          (float) (chevronY + row),
                          (float) w,
                          1.0f,
                          textColor);
    }

    if (m_focused)
    {
        float  focusInset = m_scaler.Pxf (s_kFocusInsetPx);
        float  focusThick = m_scaler.Pxf (s_kFocusRingPx);

        painter.OutlineRect ((float) m_rect.left + focusInset,
                             (float) m_rect.top  + focusInset,
                             (float) (m_rect.right  - m_rect.left) - focusInset * 2.0f,
                             (float) (m_rect.bottom - m_rect.top)  - focusInset * 2.0f,
                             focusThick,
                             s_kFocusRingArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintMenu
//
//  Paints the popup list when the dropdown is open. Pages that host
//  several dropdowns should call this AFTER painting every other
//  widget so the open menu draws on top of them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::PaintMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT  hr        = S_OK;
    int      i         = 0;
    int      rowHeight = m_scaler.Px (s_kRowHeightDip);
    int      textInset = m_scaler.Px (s_kTextInsetDip);
    float    fontDip   = m_scaler.Pxf (s_kFontDip);


    (void) painter;

    if (!m_open)
    {
        return;
    }

    for (i = 0; i < (int) m_items.size(); i++)
    {
        RECT      row   = { m_rect.left, m_rect.bottom + i * rowHeight, m_rect.right, m_rect.bottom + (i + 1) * rowHeight };
        uint32_t  color = (i == m_highlight) ? s_kMenuHoverArgb : s_kMenuArgb;

        // D2D fill (not D3D painter) so the menu background composites
        // in submission order with prior text and hides sibling text
        // underneath the open menu.
        IGNORE_RETURN_VALUE (hr, text.FillRect ((float) row.left,
                                                (float) row.top,
                                                (float) (row.right - row.left),
                                                (float) (row.bottom - row.top),
                                                color));
        IGNORE_RETURN_VALUE (hr, text.DrawString (m_items[(size_t) i].c_str(),
                                                  (float) (row.left + textInset),
                                                  (float) row.top,
                                                  (float) (row.right - row.left - textInset),
                                                  (float) (row.bottom - row.top),
                                                  s_kTextArgb,
                                                  fontDip,
                                                  s_kFontFamily,
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}
