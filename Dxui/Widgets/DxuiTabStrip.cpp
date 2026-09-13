#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTabStrip.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiTextElide.h"
#include "Core/UnicodeSymbols.h"





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
    ScrollIntoView (index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::HitTest (int x, int y) const
{
    int     i        = 0;
    size_t  n        = m_tabs.size();
    int     hit      = -1;
    int     contentX = x + m_scrollPx - GetArrowWidthPx();
    bool    inView   = false;



    //  Tab rects are in strip coordinates before scrolling, and only the part
    //  of the strip between the arrows shows tabs.
    inView = !HasBounds() || (x >= GetViewLeft() && x < GetViewRight());

    if (m_enabled && inView)
    {
        for (i = 0; i < (int) n && hit < 0; ++i)
        {
            const RECT & r = m_tabs[(size_t) i].rect;

            if (contentX >= r.left && contentX < r.right && y >= r.top && y < r.bottom)
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
    m_hover      = HitTest (x, y);
    m_hoverArrow = GetArrowAt (x, y);

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
    int   arrow  = GetArrowAt (x, y);
    int   hit    = HitTest (x, y);
    bool  wasHit = (arrow != 0 || hit >= 0);



    if (arrow != 0)
    {
        m_pressedArrow = arrow;
    }
    else if (hit >= 0)
    {
        m_pressed  = hit;
        m_pressX   = x;
        m_dragging = false;
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
    int   pressed  = m_pressed;
    int   arrow    = m_pressedArrow;
    bool  consumed = (pressed >= 0) && (m_dragging || hit == pressed);



    //  A drag ends on the tab it carried, wherever the pointer is.
    m_pressed      = -1;
    m_dragging     = false;
    m_pressedArrow = 0;

    if (arrow != 0)
    {
        consumed = true;

        if (GetArrowAt (x, y) == arrow)
        {
            ScrollByTab (arrow);
        }
    }
    else if (consumed)
    {
        Commit (pressed);
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
//  OnMouseMove
//
//  With no button held a move is hover. A held press that travels past the
//  threshold becomes a drag: the pressed tab follows the pointer, taking the
//  place of each neighbor it crosses, and held past either end of the strip
//  it scrolls the tabs toward the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouseMove (int x, int y)
{
    int   threshold = m_scaler.ToPx (s_kDragThresholdDip);
    int   step      = m_scaler.ToPx (s_kDragScrollDip);
    bool  handled   = false;



    if (m_pressed < 0)
    {
        SetMouseHover (x, y);
    }
    else
    {
        if (!m_dragging && std::abs (x - m_pressX) > threshold)
        {
            m_dragging = true;
        }

        if (m_dragging)
        {
            if (HasBounds() && x >= GetViewRight())
            {
                m_scrollPx += step;
            }
            else if (HasBounds() && x < GetViewLeft())
            {
                m_scrollPx -= step;
            }

            ClampScroll();
            MoveDraggedTab (GetDropIndex (x));
            ScrollIntoView (m_pressed);

            m_hover = m_pressed;
            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnWheel
//
//  Scrolls the tabs; reports whether they moved.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnWheel (float notches)
{
    int  before = m_scrollPx;



    m_scrollPx -= (int) (notches * (float) m_scaler.ToPx (s_kWheelStepDip));
    ClampScroll();

    return m_scrollPx != before;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTabs
//
//  A drag in progress keeps its tab as long as the tab is still there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::SetTabs (std::vector<Tab> tabs)
{
    m_tabs = std::move (tabs);

    if (m_pressed >= (int) m_tabs.size())
    {
        m_pressed  = -1;
        m_dragging = false;
    }

    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxScrollPx
//
//  How far the last tab reaches past the right arrow.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetMaxScrollPx() const
{
    int  maxScroll = 0;



    if (IsOverflowing())
    {
        maxScroll = (std::max) (0, (int) (m_tabs.back().rect.right - m_boundsDip.right) + GetArrowWidthPx() * 2);
    }

    return maxScroll;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDropIndex
//
//  The tab a drag at `x` lands on: the one under the pointer, held to the
//  strip, or the first or last beyond them.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetDropIndex (int x) const
{
    int  contentX = x;
    int  index    = 0;



    if (HasBounds())
    {
        contentX = std::clamp (x, GetViewLeft(), GetViewRight() - 1);
    }

    contentX += m_scrollPx - GetArrowWidthPx();

    while (index + 1 < (int) m_tabs.size() && contentX >= m_tabs[(size_t) index].rect.right)
    {
        index++;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampScroll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::ClampScroll()
{
    m_scrollPx = std::clamp (m_scrollPx, 0, GetMaxScrollPx());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollIntoView
//
//  Scrolls the least distance that shows all of tab `index`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::ScrollIntoView (int index)
{
    int  arrow = GetArrowWidthPx();



    if (HasBounds() && index >= 0 && index < (int) m_tabs.size())
    {
        const RECT & r = m_tabs[(size_t) index].rect;

        if (r.left - m_scrollPx + arrow < GetViewLeft())
        {
            m_scrollPx = r.left + arrow - GetViewLeft();
        }
        else if (r.right - m_scrollPx + arrow > GetViewRight())
        {
            m_scrollPx = r.right + arrow - GetViewRight();
        }
    }

    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveDraggedTab
//
//  Moves the dragged tab to `to`. Every tab keeps its width and the row is
//  packed again from the first tab's left edge; the selection stays on the
//  tab it was on.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::MoveDraggedTab (int to)
{
    int   from = m_pressed;
    LONG  left = 0;



    if (from < 0 || to < 0 || to == from || to >= (int) m_tabs.size())
    {
        return;
    }

    left = m_tabs.front().rect.left;

    if (from < to)
    {
        std::rotate (m_tabs.begin() + from, m_tabs.begin() + from + 1, m_tabs.begin() + to + 1);
    }
    else
    {
        std::rotate (m_tabs.begin() + to, m_tabs.begin() + from, m_tabs.begin() + from + 1);
    }

    for (Tab & tab : m_tabs)
    {
        LONG  width = tab.rect.right - tab.rect.left;

        tab.rect.left  = left;
        tab.rect.right = left + width;
        left          += width;
    }

    if (m_selected == from)
    {
        m_selected = to;
    }
    else if (from < m_selected && m_selected <= to)
    {
        m_selected--;
    }
    else if (to <= m_selected && m_selected < from)
    {
        m_selected++;
    }

    m_pressed = to;

    if (m_move)
    {
        m_move (from, to);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOverflowing
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsOverflowing() const
{
    return HasBounds() && !m_tabs.empty() && m_tabs.back().rect.right > m_boundsDip.right;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetArrowWidthPx
//
//  Zero while the tabs fit, so the tabs start at the strip's left edge.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetArrowWidthPx() const
{
    return IsOverflowing() ? m_scaler.ToPx (s_kArrowWidthDip) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetArrowAt
//
//  -1 over the left arrow, +1 over the right, 0 elsewhere or with no arrows.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetArrowAt (int x, int y) const
{
    int   arrow  = GetArrowWidthPx();
    int   result = 0;
    bool  inRow  = y >= m_boundsDip.top && y < m_boundsDip.bottom;



    if (arrow > 0 && inRow && x >= m_boundsDip.left && x < m_boundsDip.left + arrow)
    {
        result = -1;
    }
    else if (arrow > 0 && inRow && x >= m_boundsDip.right - arrow && x < m_boundsDip.right)
    {
        result = 1;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanScroll
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::CanScroll (int direction) const
{
    return (direction < 0) ? m_scrollPx > 0 : m_scrollPx < GetMaxScrollPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollByTab
//
//  An arrow click brings the next tab cut off on that side fully into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::ScrollByTab (int direction)
{
    int   arrow = GetArrowWidthPx();
    int   i     = 0;
    bool  found = false;



    if (direction < 0)
    {
        for (i = (int) m_tabs.size() - 1; i >= 0 && !found; i--)
        {
            const RECT & r = m_tabs[(size_t) i].rect;

            if (r.left - m_scrollPx + arrow < GetViewLeft())
            {
                m_scrollPx = r.left + arrow - GetViewLeft();
                found      = true;
            }
        }
    }
    else
    {
        for (i = 0; i < (int) m_tabs.size() && !found; i++)
        {
            const RECT & r = m_tabs[(size_t) i].rect;

            if (r.right - m_scrollPx + arrow > GetViewRight())
            {
                m_scrollPx = r.right + arrow - GetViewRight();
                found      = true;
            }
        }
    }

    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintArrow
//
//  A triangle, dimmed when there is nothing further to scroll to, with the
//  hover fill only when it can act.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintArrow (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t hoverArgb, uint32_t textArgb) const
{
    constexpr float  s_kArrowFontDip   = 9.0f;
    constexpr float  s_kArrowIdleScale = 0.35f;
    constexpr float  s_kPressedScale   = 0.82f;



    HRESULT   hr      = S_OK;
    int       arrow   = GetArrowWidthPx();
    float     left    = (float) ((direction < 0) ? m_boundsDip.left : m_boundsDip.right - arrow);
    float     top     = (float) m_boundsDip.top;
    float     height  = (float) (m_boundsDip.bottom - m_boundsDip.top);
    bool      enabled = CanScroll (direction);
    uint32_t  color   = enabled ? textArgb : DxuiColor::Scale (textArgb, s_kArrowIdleScale);



    if (enabled && m_hoverArrow == direction)
    {
        painter.FillRect (left, top, (float) arrow, height,
                          (m_pressedArrow == direction) ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
    }

    hr = text.DrawString ((direction < 0) ? s_kpszTriangleLeft : s_kpszTriangleRight,
                          left, top, (float) arrow, height,
                          color,
                          m_scaler.ToPxf (s_kArrowFontDip),
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
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
    ScrollIntoView (newIndex);

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



    constexpr float  s_kUnderlineDip   = 3.0f;   // thick active-tab underline
    constexpr float  s_kMutedTextScale = 0.62f;   // dim inactive labels

    HRESULT  hr          = S_OK;
    int      i           = 0;
    size_t   n           = m_tabs.size();
    float    focusThick  = m_scaler.ToPxf (s_kFocusThickDip);
    float    focusInset  = m_scaler.ToPxf (s_kFocusInsetDip);
    float    padX        = m_scaler.ToPxf (s_kPadXDp);
    float    padY        = m_scaler.ToPxf (s_kPadYDp);
    float    fontDip     = m_scaler.ToPxf (s_kFontDip);
    float    underline   = m_scaler.ToPxf (s_kUnderlineDip);
    uint32_t mutedText   = DxuiColor::Scale (textArgb, s_kMutedTextScale);
    int      arrow       = GetArrowWidthPx();

    UNREFERENCED_PARAMETER (idleArgb);   // idle + selected tabs blend with the page



    // Modern connected-tab look: the active tab shares the page background (no
    // chip fill) and is marked by a thick accent underline flush with the page
    // edge; inactive tabs are unfilled with dimmed labels, and a hovered /
    // armed inactive tab gets a subtle fill hint.
    if (HasBounds())
    {
        hr = text.PushClipRect ((float) GetViewLeft(), (float) m_boundsDip.top,
                                (float) (GetViewRight() - GetViewLeft()), (float) (m_boundsDip.bottom - m_boundsDip.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (i = 0; i < (int) n; ++i)
    {
        const Tab  & t       = m_tabs[(size_t) i];
        RECT         r       = { t.rect.left - m_scrollPx + arrow, t.rect.top, t.rect.right - m_scrollPx + arrow, t.rect.bottom };
        bool         isSel   = (i == m_selected);
        bool         isHover = (i == m_hover);
        bool         isArmed = (i == m_pressed && i == m_hover);
        float        labelW  = (float) (r.right - r.left) - padX * 2.0f;
        std::wstring shown;

        if (!isSel && (isHover || isArmed))
        {
            painter.FillRect ((float) r.left,
                              (float) r.top,
                              (float) (r.right  - r.left),
                              (float) (r.bottom - r.top),
                              isArmed ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
        }

        if (isSel)
        {
            painter.FillRect ((float) r.left,
                              (float) r.bottom - underline,
                              (float) (r.right - r.left),
                              underline,
                              selectedArgb);
        }

        if (m_focused && isSel)
        {
            painter.OutlineRect ((float) r.left + focusInset,
                                 (float) r.top  + focusInset,
                                 (float) (r.right  - r.left) - focusInset * 2.0f,
                                 (float) (r.bottom - r.top)  - focusInset * 2.0f,
                                 focusThick, focusArgb);
        }

        //  A label wider than its tab is cut off with an ellipsis, never wrapped.
        shown = DxuiTextElide::ToWidth (text, t.label, fontDip, DxuiTheme::kBodyFace, labelW, DxuiElide::Tail);

        hr = text.DrawString (shown.c_str(),
                              (float) r.left + padX,
                              (float) r.top  + padY,
                              (float) (r.right  - r.left) - padX * 2.0f,
                              (float) (r.bottom - r.top)  - padY * 2.0f,
                              isSel ? textArgb : mutedText,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (HasBounds())
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (arrow > 0)
    {
        PaintArrow (painter, text, -1, hoverArgb, textArgb);
        PaintArrow (painter, text,  1, hoverArgb, textArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::Layout  (IDxuiControl override)
//
//  Per-tab rects are populated by the caller via SetTabs; the override
//  records the group bounds for IDxuiControl::GetBounds() consumers.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
    ClampScroll();
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



    // Underline-style strip: the "selected" slot is the accent color for the
    // active-tab underline, hover is a subtle fill, idle is unused (idle tabs
    // match the page). It uses the accent, not the selection fill: the two
    // were the same color until the Windows themes started reading the accent
    // from the system, and row selection is intentionally neutral.
    PaintInternal (painter, text,
                   DxuiColor::Scale (hover, s_kIdleScale),
                   hover,
                   theme.Accent(),
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
//  crossing the strip does not consume moves other widgets want, unless it
//  is dragging a tab.
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
        handled = OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        break;
    case DxuiMouseEventKind::Wheel:
        //  A horizontal wheel's positive notch is rightward, a vertical one's
        //  is up, which moves the tabs the other way.
        handled = OnWheel (ev.wheelHorizontal ? -ev.wheelDelta : ev.wheelDelta);
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
//  DxuiTabStrip::GetAccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected tab (or empty if none).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTabStrip::GetAccessibleName() const
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
