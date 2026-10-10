#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTabStrip.h"
#include "Widgets/DxuiPaneFrame.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiTextElide.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/IDxuiTextRenderer.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::Palette
//
//  The colors one paint uses. Explorer's tabs take the first six. The
//  compact styles take `fill`, `hover` for a button's wash and `text` for
//  the arrows and the +, and the rest: `tabHover` under the pointer, on a
//  tab, an arrow or the +, and each label and glyph ink, the `Focused` ones
//  for the selected tab of the pane the user works in and the `Hovered` ones
//  for the tab under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTabStrip::Palette
{
    uint32_t  strip        = 0;
    uint32_t  hover        = 0;
    uint32_t  fill         = 0;
    uint32_t  divider      = 0;
    uint32_t  text         = 0;
    uint32_t  focus        = 0;
    uint32_t  tabHover     = 0;
    uint32_t  label        = 0;
    uint32_t  labelFocused = 0;
    uint32_t  labelHovered = 0;
    uint32_t  glyph        = 0;
    uint32_t  glyphFocused = 0;
    uint32_t  glyphHovered = 0;
};





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
    m_hover       = HitTest (x, y);
    m_hoverArrow  = GetArrowAt (x, y);
    m_hoverNewTab = IsOverNewTab (x, y);
    m_hoverClose  = GetCloseAt (x, y);
    m_hoverPin    = GetPinAt (x, y);

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
    int   arrow   = GetArrowAt (x, y);
    int   hit     = HitTest (x, y);
    int   close   = GetCloseAt (x, y);
    int   pin     = GetPinAt (x, y);
    bool  overNew = IsOverNewTab (x, y);
    bool  wasHit  = (arrow != 0 || hit >= 0 || overNew);



    if (arrow != 0)
    {
        m_pressedArrow = arrow;
    }
    else if (overNew)
    {
        m_pressedNewTab = true;
    }
    else if (close >= 0)
    {
        m_pressedClose = close;
    }
    else if (pin >= 0)
    {
        m_pressedPin = pin;
    }
    else if (hit >= 0)
    {
        m_pressed    = hit;
        m_pressX     = x;
        m_pressY     = y;
        m_dragging   = false;
        m_grabOffset = POINT { x - GetTabScreenRect (hit).left, y - GetTabScreenRect (hit).top };
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
    int   closing  = m_pressedClose;
    int   pinning  = m_pressedPin;
    bool  newTab   = m_pressedNewTab;
    bool  consumed = (pressed >= 0) && (m_dragging || hit == pressed);



    //  A drag ends on the tab it carried, wherever the pointer is.
    m_pressed       = -1;
    m_dragging      = false;
    m_pressedArrow  = 0;
    m_pressedNewTab = false;
    m_pressedClose  = -1;
    m_pressedPin    = -1;

    if (arrow != 0)
    {
        consumed = true;

        if (GetArrowAt (x, y) == arrow)
        {
            ScrollByTab (arrow);
        }
    }
    else if (newTab)
    {
        consumed = true;

        if (IsOverNewTab (x, y))
        {
            m_newTab();
        }
    }
    else if (closing >= 0)
    {
        consumed = true;

        if (GetCloseAt (x, y) == closing)
        {
            m_close (closing);
        }
    }
    else if (pinning >= 0)
    {
        consumed = true;

        if (GetPinAt (x, y) == pinning)
        {
            m_pin (pinning);
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
//  it scrolls the tabs toward the pointer. Dragged past the strip's top or
//  bottom, with a host that asked, the tab is the host's to carry.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouseMove (int x, int y)
{
    int   threshold = m_scaler.ToPx (s_kDragThresholdDip);
    int   step      = m_scaler.ToPx (s_kDragScrollDip);
    bool  handled   = false;
    bool  leftStrip = HasBounds() && (y < m_boundsDip.top - threshold || y >= m_boundsDip.bottom + threshold);
    bool  moved     = std::abs (x - m_pressX) > threshold || std::abs (y - m_pressY) > threshold;
    int   carried   = m_pressed;
    int   prevHover = m_hover;



    //  With no move handler the tabs keep their order, so any drag is the
    //  host's; with one, only a drag off the strip is.
    if (m_pressed >= 0 && m_dragOut && moved && (leftStrip || !m_move))
    {
        m_pressed  = -1;
        m_dragging = false;
        m_dragOut (carried, POINT { x, y });
        return true;
    }

    if (m_pressed < 0)
    {
        SetMouseHover (x, y);

        //  Handled only when the hovered tab changes, as DxuiButton does: that
        //  repaints the window at once rather than on its half-second tick.
        handled = m_hover != prevHover;
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
        maxScroll = (std::max) (0, (int) (m_tabs.back().rect.right - m_boundsDip.right) + GetArrowWidthPx() * 2 + GetNewTabWidthPx());
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
    return HasBounds() && !m_tabs.empty() && m_tabs.back().rect.right > m_boundsDip.right - GetNewTabWidthPx();
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
    else if (arrow > 0 && inRow && x >= GetViewRight() && x < GetViewRight() + arrow)
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



    HRESULT   hr      = S_OK;
    int       arrow   = GetArrowWidthPx();
    float     left    = (float) ((direction < 0) ? (int) m_boundsDip.left : GetViewRight());
    float     top     = (float) m_boundsDip.top;
    float     height  = (float) (m_boundsDip.bottom - m_boundsDip.top);
    bool      enabled = CanScroll (direction);
    uint32_t  color   = enabled ? textArgb : DxuiColor::Scale (textArgb, s_kArrowIdleScale);
    uint32_t  wash    = (m_pressedArrow == direction) ? DxuiColor::Darken (hoverArgb, kPressedScale) : hoverArgb;
    RECT      area    = { (long) left, m_boundsDip.top, (long) left + arrow, m_boundsDip.bottom };



    if (enabled && m_hoverArrow == direction && m_style == Style::Explorer)
    {
        painter.FillRect (left, top, (float) arrow, height, wash);
    }
    else if (enabled && m_hoverArrow == direction)
    {
        PaintHoverPill (painter, area, wash);
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
//  GetNewTabWidthPx
//
//  Zero with no new-tab handler, so a strip without one has no + button.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetNewTabWidthPx() const
{
    return (HasBounds() && m_newTab) ? m_scaler.ToPx (kNewTabWidthDip) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNewTabRect
//
//  Just past the last tab while the tabs fit, at the strip's right end once
//  they overflow, as in File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetNewTabRect() const
{
    RECT  rc    = {};
    int   width = GetNewTabWidthPx();
    LONG  left  = m_boundsDip.left;



    if (width > 0)
    {
        if (IsOverflowing())
        {
            left = m_boundsDip.right - width;
        }
        else if (!m_tabs.empty())
        {
            left = m_tabs.back().rect.right;
        }

        rc = RECT { left, m_boundsDip.top, left + width, m_boundsDip.bottom };
    }

    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOverNewTab
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsOverNewTab (int x, int y) const
{
    RECT  rc = GetNewTabRect();



    return m_enabled && x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintNewTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintNewTab (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const
{
    constexpr float  s_kGlyphDip = 16.0f;



    HRESULT   hr   = S_OK;
    RECT      rc   = GetNewTabRect();
    uint32_t  wash = m_pressedNewTab ? DxuiColor::Darken (hoverArgb, kPressedScale) : hoverArgb;



    if (m_hoverNewTab && m_style == Style::Explorer)
    {
        painter.FillRect ((float) rc.left, (float) rc.top, (float) (rc.right - rc.left), (float) (rc.bottom - rc.top), wash);
    }
    else if (m_hoverNewTab)
    {
        PaintHoverPill (painter, rc, wash);
    }

    //  In the compact styles the + is set in the tabs' own font and size, so
    //  it sits on their centerline rather than a larger glyph's.
    hr = text.DrawString (L"+",
                          (float) rc.left, (float) rc.top, (float) (rc.right - rc.left), (float) (rc.bottom - rc.top),
                          textArgb,
                          m_scaler.ToPxf ((m_style == Style::Explorer) ? s_kGlyphDip : (float) s_kCompactFontDip),
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTabScreenRect
//
//  Where tab `index` is drawn: its strip rect moved by the scroll and past the
//  left arrow.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetTabScreenRect (int index) const
{
    const RECT &  r     = m_tabs[(size_t) index].rect;
    int           shift = GetArrowWidthPx() - m_scrollPx;



    if (m_gapIndex >= 0 && index >= m_gapIndex)
    {
        shift += m_gapPx;
    }

    return RECT { r.left + shift, r.top, r.right + shift, r.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInsertGapRect
//
//  The room opened ahead of the gap's tab, or past the last tab; empty while
//  no gap is open.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetInsertGapRect() const
{
    long  left  = 0;
    int   shift = GetArrowWidthPx() - m_scrollPx;



    if (m_gapIndex < 0)
    {
        return RECT {};
    }

    if (m_gapIndex < (int) m_tabs.size())
    {
        left = m_tabs[(size_t) m_gapIndex].rect.left + shift;
    }
    else
    {
        left = (m_tabs.empty() ? m_boundsDip.left : m_tabs.back().rect.right + shift);
    }

    return RECT { left, m_boundsDip.top, left + m_gapPx, m_boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInsertIndexAt
//
//  Ahead of the tab whose left half holds `x`, or after it for its right
//  half; past the last tab, the tab count. The tabs are taken where they are
//  drawn, so a pointer inside an open gap keeps that gap.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetInsertIndexAt (int x) const
{
    int   index = 0;
    RECT  tab   = {};



    while (index < (int) m_tabs.size())
    {
        tab = GetTabScreenRect (index);

        if (x < (tab.left + tab.right) / 2)
        {
            break;
        }

        index++;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelectedSpan
//
//  The selected tab where it is drawn, cut to the part of the strip between
//  the scroll arrows. A strip with no bounds has no arrows, and the tab
//  shows whole.
//
//  The strip's ends are the pane's sides. A tab that is not cut off and
//  ends nearer an end than DxuiPaneFrame::GetFlushReachPx is drawn reaching
//  it, as the pane's frame draws it flush with that side.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::GetSelectedSpan (long & left, long & right, bool & openLeft, bool & openRight) const
{
    RECT  r         = {};
    long  viewLeft  = 0;
    long  viewRight = 0;
    long  reach     = DxuiPaneFrame::GetFlushReachPx (GetCornerPx(), DxuiPaneMetrics::GetLinePx (m_scaler));



    if (m_style == Style::Explorer || m_selected < 0 || m_selected >= (int) m_tabs.size())
    {
        return false;
    }

    r         = GetTabScreenRect (m_selected);
    viewLeft  = HasBounds() ? (long) GetViewLeft()  : r.left;
    viewRight = HasBounds() ? (long) GetViewRight() : r.right;

    if (r.right <= viewLeft || r.left >= viewRight)
    {
        return false;
    }

    openLeft  = r.left  < viewLeft;
    openRight = r.right > viewRight;
    left      = (std::max) (r.left,  viewLeft);
    right     = (std::min) (r.right, viewRight);
    left      = (HasBounds() && !openLeft  && left - m_boundsDip.left   < reach) ? m_boundsDip.left  : left;
    right     = (HasBounds() && !openRight && m_boundsDip.right - right < reach) ? m_boundsDip.right : right;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHoverInsetPx
//
//  The wash under the pointer on an arrow or the + stands in from the
//  strip's edges by the compact inset, and never less than the outer corner
//  radius less a line, so it clears the rows where the selected tab's joins
//  curve into the line along the band: 4, 5 and 5 px at 100%, 125% and
//  150%.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetHoverInsetPx (const DxuiDpiScaler & scaler)
{
    return (std::max) (scaler.ToPx (s_kCompactInsetDip), DxuiPaneMetrics::GetCornerPx (scaler) - DxuiPaneMetrics::GetLinePx (scaler));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCloseRect
//
//  The close button's square: centered where Explorer centers its glyph,
//  or in the compact styles the tab's own close button.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetCloseRect (int index) const
{
    RECT  tab    = GetTabScreenRect (index);
    int   box    = m_scaler.ToPx (s_kCloseBoxDip);
    int   center = tab.right - m_scaler.ToPx (s_kCloseCenterDip);
    int   top    = tab.top + ((tab.bottom - tab.top) - box) / 2;



    if (m_style != Style::Explorer)
    {
        return GetTabButtonRect (index, TabButton::Close);
    }

    return RECT { center - box / 2, top, center - box / 2 + box, top + box };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTabButtonRect
//
//  Visual Studio's squares on a document tab, ToPx (24) across: the close
//  button ending one line in from the tab's right end and the pin just left
//  of it, each spanning the tab between its outline and the line along the
//  band. At 150% the close glyph's ink then ends 12 px in from the tab's
//  end, and the two glyphs are 36 px apart. A tool window's tabs have none.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetTabButtonRect (
    int        index,
    TabButton  button) const
{
    RECT  tab   = {};
    long  line  = DxuiPaneMetrics::GetLinePx (m_scaler);
    long  size  = m_scaler.ToPx (kTabButtonDip);
    long  right = 0;



    if (m_style != Style::Document || index < 0 || index >= (int) m_tabs.size() || button == TabButton::None)
    {
        return RECT {};
    }

    tab   = GetTabScreenRect (index);
    right = tab.right - line - ((button == TabButton::Pin) ? size : 0);

    return RECT { right - size, tab.top + line, right, tab.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTabButtonAt
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabStrip::TabButton DxuiTabStrip::GetTabButtonAt (
    int     x,
    int     y,
    int   & index,
    RECT  & button) const
{
    int        pin   = GetPinAt (x, y);
    int        close = GetCloseAt (x, y);
    TabButton  which = (pin >= 0) ? TabButton::Pin : (close >= 0) ? TabButton::Close : TabButton::None;



    index  = (pin >= 0) ? pin : close;
    button = (which == TabButton::Pin) ? GetTabButtonRect (index, TabButton::Pin) : (which == TabButton::Close) ? GetCloseRect (index) : RECT {};

    return which;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCloseShown
//
//  Explorer shows every tab's close button. Visual Studio's document tabs
//  show it on the selected tab and the one under the pointer, and its tool
//  windows' tabs show none.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsCloseShown (int index) const
{
    bool  isLit = index == m_selected || index == m_hover;
    bool  shown = (m_style == Style::Explorer) || (m_style == Style::Document && isLit);



    return m_close && shown && m_tabs[(size_t) index].closable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPinShown
//
//  A document tab's pin, on the selected tab and the one under the pointer,
//  as its close button is.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsPinShown (int index) const
{
    return m_pin && m_style == Style::Document && (index == m_selected || index == m_hover);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPinAt
//
//  The tab whose pin is under the point, or -1, from the part of the strip
//  between the arrows, as GetCloseAt finds a close button.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetPinAt (
    int  x,
    int  y) const
{
    int   hit    = -1;
    bool  inView = !HasBounds() || (x >= GetViewLeft() && x < GetViewRight());



    if (m_pin && m_enabled && inView)
    {
        for (int i = 0; i < (int) m_tabs.size() && hit < 0; i++)
        {
            RECT  rc = GetTabButtonRect (i, TabButton::Pin);

            if (IsPinShown (i) && x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom)
            {
                hit = i;
            }
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCornerPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetCornerPx() const
{
    return (m_cornerPx >= 0) ? m_cornerPx : DxuiPaneMetrics::GetCornerPx (m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLabelInk
//
//  Visual Studio's dark theme draws the selected tab of the pane in use in
//  #FFFFFF, its foreground, and every other label in #D7D7D7: 0.68 of the
//  way from that foreground to #C5C5C5, the system dark theme's muted
//  foreground. A theme whose two inks lie closer to the band is held to
//  WCAG's 4.5:1 on it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetLabelInk (
    const IDxuiTheme  & theme,
    bool                focusedSelected)
{
    constexpr float  kMutedMix    = 0.68f;
    constexpr float  kMinContrast = 4.5f;
    uint32_t         ink          = DxuiColor::Mix (theme.Foreground(), theme.ForegroundMuted(), kMutedMix);



    return focusedSelected ? theme.Foreground() : DxuiColor::ComputeInkForContrast (ink, theme.PaneBand(), kMinContrast);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGlyphInk
//
//  The pin and close glyphs: the full foreground on the selected tab of the
//  pane in use, and otherwise a shade under the label, Visual Studio's
//  #D1D1D1 beside #D7D7D7.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetGlyphInk (
    const IDxuiTheme  & theme,
    bool                focusedSelected)
{
    return focusedSelected ? theme.Foreground() : DxuiColor::Scale (GetLabelInk (theme, false), kGlyphScale);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHoveredLabelInk
//
//  The label of the tab under the pointer, unless it is the selected tab of
//  the pane in use: a step from another label's ink toward the full
//  foreground, Visual Studio's #D9D9D9 beside #D7D7D7.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetHoveredLabelInk (const IDxuiTheme & theme)
{
    constexpr float  kHoverMix = 0.05f;



    return DxuiColor::Mix (GetLabelInk (theme, false), theme.Foreground(), kHoverMix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHoveredGlyphInk
//
//  The pin and close glyphs on the tab under the pointer: a shade under its
//  label, as every other tab's are, Visual Studio's #D3D3D3 beside #D9D9D9.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetHoveredGlyphInk (const IDxuiTheme & theme)
{
    return DxuiColor::Scale (GetHoveredLabelInk (theme), kGlyphScale);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTabHoverFill
//
//  The foreground at alpha 0x0F: laid over the band, white turns Visual
//  Studio's #262626 into its #333333.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetTabHoverFill (const IDxuiTheme & theme)
{
    constexpr uint32_t  kHoverAlpha = 0x0F000000u;



    return (theme.Foreground() & 0x00FFFFFFu) | kHoverAlpha;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCloseAt
//
//  The tab whose close button is under the point, or -1. Only the part of the
//  strip between the arrows shows tabs, so only that part can hold one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetCloseAt (int x, int y) const
{
    int   hit    = -1;
    int   i      = 0;
    bool  inView = !HasBounds() || (x >= GetViewLeft() && x < GetViewRight());



    if (m_close && m_enabled && inView)
    {
        for (i = 0; i < (int) m_tabs.size() && hit < 0; i++)
        {
            RECT  rc = GetCloseRect (i);

            if (IsCloseShown (i) && x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom)
            {
                hit = i;
            }
        }
    }

    return hit;
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
    constexpr uint32_t  s_kStrip       = 0xFF202020;
    constexpr uint32_t  s_kTabHover    = 0x14FFFFFF;
    constexpr uint32_t  kCompactWash   = 0x0FFFFFFF;
    constexpr uint32_t  s_kTabSelected = 0xFF2C2C2C;
    constexpr uint32_t  s_kDivider     = 0xFF323232;
    constexpr uint32_t  s_kTextArgb    = 0xFFFFFFFF;
    constexpr uint32_t  s_kFocusRing   = 0xFFAACCFF;
    Palette             pal;



    pal.strip        = s_kStrip;
    pal.hover        = s_kTabHover;
    pal.fill         = s_kTabSelected;
    pal.divider      = s_kDivider;
    pal.text         = s_kTextArgb;
    pal.focus        = s_kFocusRing;
    pal.tabHover     = kCompactWash;
    pal.label        = s_kTextArgb;
    pal.labelFocused = s_kTextArgb;
    pal.labelHovered = s_kTextArgb;
    pal.glyph        = s_kTextArgb;
    pal.glyphFocused = s_kTextArgb;
    pal.glyphHovered = s_kTextArgb;

    PaintInternal (painter, text, pal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintInternal
//
//  Renders the tab row with caller-supplied colors so the themed and
//  fallback Paint entry points share one body.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintInternal (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const Palette      & pal) const
{
    constexpr float  s_kFocusThickDip  = 1.0f;
    constexpr float  s_kFocusInsetDip  = 1.0f;
    constexpr float  s_kDividerDip     = 16.0f;
    constexpr float  s_kMutedTextScale = 0.8f;    // Explorer's #CCCCCC on an unselected tab



    HRESULT   hr         = S_OK;
    int       i          = 0;
    size_t    n          = m_tabs.size();
    bool      compact    = m_style != Style::Explorer;
    long      line       = DxuiPaneMetrics::GetLinePx (m_scaler);
    long      lineAbove  = (m_style == Style::ToolWindow) ? line : 0;
    float     focusThick = m_scaler.ToPxf (s_kFocusThickDip);
    float     focusInset = m_scaler.ToPxf (s_kFocusInsetDip);
    float     padX       = m_scaler.ToPxf (kLabelPadXDip);
    float     fontDip    = m_scaler.ToPxf (kLabelFontDip);
    float     corner     = m_scaler.ToPxf ((float) s_kCornerDip);
    float     iconPx     = m_scaler.ToPxf ((float) s_kIconDip);
    float     dividerH   = m_scaler.ToPxf (s_kDividerDip);
    uint32_t  mutedText  = DxuiColor::Scale (pal.text, s_kMutedTextScale);
    uint32_t  closeHover = (pal.text & 0x00FFFFFFu) | 0x1F000000u;
    uint32_t  closePress = (pal.text & 0x00FFFFFFu) | 0x33000000u;
    uint32_t  wash       = compact ? pal.tabHover : pal.hover;



    //  File Explorer's tabs (research R13): the selected one is filled with the
    //  row below and joins it, rounded at the top and flared at the bottom;
    //  the rest are unfilled, split by short dividers, with dimmer labels.
    //  In the compact styles the tabs' fills are clipped to the same span,
    //  extended one line past the strip's edge along the pane, below a
    //  document's tabs and above a tool window's, so the selected tab can
    //  fill the line it opens into.
    if (HasBounds())
    {
        hr = text.PushClipRect ((float) GetViewLeft(), (float) m_boundsDip.top,
                                (float) (GetViewRight() - GetViewLeft()), (float) (m_boundsDip.bottom - m_boundsDip.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (HasBounds() && compact)
    {
        painter.PushClip ((float) GetViewLeft(), (float) (m_boundsDip.top - lineAbove),
                          (float) (GetViewRight() - GetViewLeft()), (float) (m_boundsDip.bottom - m_boundsDip.top + line));
    }

    for (i = 0; i < (int) n; ++i)
    {
        const Tab  & t       = m_tabs[(size_t) i];
        RECT         r       = GetTabScreenRect (i);
        float        left    = (float) r.left;
        float        top     = (float) r.top;
        float        width   = (float) (r.right - r.left);
        float        height  = (float) (r.bottom - r.top);
        float        bottom  = top + height;
        bool         isSel   = (i == m_selected);
        bool         isHover = (i == m_hover);
        bool         isArmed = (i == m_pressed && i == m_hover);
        bool         nextLit = (i + 1 == m_selected) || (i + 1 == m_hover);
        float        labelX  = left + m_scaler.ToPxf ((float) s_kLabelInsetDip);
        float        labelR  = (float) r.right - padX;
        std::wstring shown;

        //  The selected compact tab is painted after the loop, last of all.
        if (compact)
        {
            if (!isSel)
            {
                PaintCompactTab (painter, text, i, pal);
            }

            continue;
        }

        if (isSel)
        {
            painter.FillRoundedRect (left, top, width, height, corner, pal.fill);
            painter.FillRect        (left, bottom - corner, width, corner, pal.fill);

            //  The flare: a square of fill beside each bottom corner with a
            //  quarter circle of the strip cut out of it.
            painter.FillRect         (left - corner,          bottom - corner, corner, corner, pal.fill);
            painter.FillCircle (left - corner,          bottom - corner, corner, pal.strip);
            painter.FillRect         (left + width,           bottom - corner, corner, corner, pal.fill);
            painter.FillCircle (left + width + corner,  bottom - corner, corner, pal.strip);
        }
        else if (isHover || isArmed)
        {
            painter.FillRoundedRect (left, top, width, height, corner,
                                     isArmed ? DxuiColor::Darken (pal.hover, kPressedScale) : pal.hover);
        }
        else if (!nextLit && i + 1 < (int) n)
        {
            painter.FillRect (left + width - 1.0f, top + (height - dividerH) * 0.5f, 1.0f, dividerH, pal.divider);
        }

        if (m_focused && m_focusCueVisible && isSel)
        {
            painter.OutlineRoundedRect (left + focusInset, top + focusInset,
                                        width - focusInset * 2.0f, height - focusInset * 2.0f,
                                        corner, focusThick, pal.focus);
        }

        if (t.icon && !t.icon->bgraPremul.empty())
        {
            hr = text.DrawIconBitmap (t.icon->bgraPremul.data(), t.icon->width, t.icon->height,
                                      left + m_scaler.ToPxf ((float) s_kIconInsetDip), top + (height - iconPx) * 0.5f, iconPx, iconPx);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (m_close)
        {
            RECT  close = GetCloseRect (i);

            labelR = (float) close.left;

            if (m_hoverClose == i)
            {
                painter.FillRoundedRect ((float) close.left, (float) close.top,
                                         (float) (close.right - close.left), (float) (close.bottom - close.top), corner,
                                         (m_pressedClose == i) ? closePress : closeHover);
            }

            hr = text.DrawString (s_kpszMdl2Cancel,
                                  (float) close.left, (float) close.top,
                                  (float) (close.right - close.left), (float) (close.bottom - close.top),
                                  isSel ? pal.text : mutedText,
                                  m_scaler.ToPxf ((float) s_kCloseGlyphDip),
                                  m_iconFace,
                                  DxuiTextHAlign::Center,
                                  DxuiTextVAlign::Center,
                                  DxuiFontWeight::Normal,
                                  false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        //  A label wider than its room is cut off with an ellipsis, never wrapped.
        shown = DxuiTextElide::ToWidth (text, t.label, fontDip, DxuiTheme::kBodyFace, (std::max) (labelR - labelX, 0.0f), DxuiElide::Tail);

        hr = text.DrawString (shown.c_str(),
                              labelX, top, (std::max) (labelR - labelX, 0.0f), height,
                              isSel ? pal.text : mutedText,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              isSel ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The selected tab's body and label lie over its neighbors' washes.
    if (compact && m_selected >= 0 && m_selected < (int) n)
    {
        PaintCompactTab (painter, text, m_selected, pal);
    }

    if (HasBounds() && compact)
    {
        painter.PopClip();
    }

    if (HasBounds())
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (GetArrowWidthPx() > 0)
    {
        PaintArrow (painter, text, -1, wash, pal.text);
        PaintArrow (painter, text,  1, wash, pal.text);
    }

    if (GetNewTabWidthPx() > 0)
    {
        PaintNewTab (painter, text, wash, pal.text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintCompactTab
//
//  Visual Studio's tabs. The selected tab is filled with its pane's color,
//  rounded at its far corners and open into the pane; the pane's frame draws
//  its outline. The one under the pointer is a faint box the band's depth,
//  its label a step brighter. The rest are plain text on the band.
//
//  Each label starts where a pane's title does, at the pane's text inset
//  from the tab's left edge in whole pixels, so the first tab's label lines
//  up with a title to the pixel. A document tab's label runs to the pin's
//  square, which every document tab keeps whether or not its pin shows, and
//  a tool window tab's to the line at its end.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintCompactTab (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    int                  index,
    const Palette      & pal) const
{
    const Tab     & tab     = m_tabs[(size_t) index];
    RECT            r       = GetTabScreenRect (index);
    RECT            pin     = GetTabButtonRect (index, TabButton::Pin);
    long            labelR  = (m_style == Style::Document) ? pin.left : r.right - DxuiPaneMetrics::GetLinePx (m_scaler);
    float           top     = (float) r.top;
    float           height  = (float) (r.bottom - r.top);
    float           fontPx  = m_scaler.ToPxf ((float) s_kCompactFontDip);
    float           markPx  = (float) m_scaler.ToPx (s_kCompactMarkDip);
    float           labelX  = (float) (r.left + DxuiPaneMetrics::GetTextInsetPx (m_scaler));
    float           room    = 0.0f;
    bool            isSel   = index == m_selected;
    bool            isHover = index == m_hover;
    bool            isArmed = index == m_pressed && index == m_hover;
    uint32_t        ink     = (isSel && m_focusedLook) ? pal.labelFocused : isHover ? pal.labelHovered : pal.label;
    std::wstring    shown;
    HRESULT         hr      = S_OK;



    if (isSel)
    {
        PaintSelectedBody (painter, r, pal.fill);
    }
    else if (isHover)
    {
        PaintHoverBox (painter, r, isArmed ? DxuiColor::Darken (pal.tabHover, kPressedScale) : pal.tabHover);
    }

    if (!tab.mark.empty() && tab.markArgb != 0)
    {
        hr = text.DrawString (tab.mark.c_str(), labelX, top, markPx, height,
                              tab.markArgb, fontPx, tab.markFace.empty() ? DxuiTheme::kBodyFace : tab.markFace.c_str(),
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (!tab.mark.empty())
    {
        labelX += markPx;
    }

    if (IsPinShown (index))
    {
        PaintTabButton (painter, text, index, TabButton::Pin, pal);
    }

    if (IsCloseShown (index))
    {
        PaintTabButton (painter, text, index, TabButton::Close, pal);
    }

    room  = (std::max) ((float) labelR - labelX, 0.0f);
    shown = DxuiTextElide::ToWidth (text, tab.label, fontPx, DxuiTheme::kBodyFace, room, DxuiElide::Tail);

    hr = text.DrawString (shown.c_str(), labelX, top, room, height,
                          ink, fontPx, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintTabButton
//
//  A tab's pin or close button: its glyph centered in its square, in the
//  full foreground on the selected tab of the pane in use and a shade under
//  the tab's label otherwise, washed in a rounded square 3 DIP in from the
//  square's edges while the pointer is on it, as a title bar's button is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintTabButton (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    int                  index,
    TabButton            button,
    const Palette      & pal) const
{
    static constexpr int  kWashInsetDip = 3;
    RECT                  r             = GetTabButtonRect (index, button);
    bool                  isPin         = button == TabButton::Pin;
    bool                  hovered       = isPin ? m_hoverPin   == index : m_hoverClose   == index;
    bool                  pressed       = isPin ? m_pressedPin == index : m_pressedClose == index;
    bool                  isFocused     = index == m_selected && m_focusedLook;
    float                 inset         = (float) m_scaler.ToPx (kWashInsetDip);
    float                 corner        = (float) m_scaler.ToPx (s_kCompactCornerDip);
    float                 width         = (float) (r.right - r.left);
    float                 height        = (float) (r.bottom - r.top);
    uint32_t              ink           = isFocused ? pal.glyphFocused : (index == m_hover) ? pal.glyphHovered : pal.glyph;
    HRESULT               hr            = S_OK;



    if (hovered)
    {
        painter.FillRoundedRect ((float) r.left + inset, (float) r.top + inset, width - 2.0f * inset, height - 2.0f * inset, corner,
                                 pressed ? DxuiColor::Darken (pal.hover, kPressedScale) : pal.hover);
    }

    hr = text.DrawString (isPin ? s_kpszMdl2Pinned : s_kpszMdl2Cancel, (float) r.left, (float) r.top, width, height,
                          ink, m_scaler.ToPxf (isPin ? kPinGlyphDip : kCloseGlyphDip), m_iconFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintSelectedBody
//
//  The selected tab's fill, in the color of the pane it opens into, across
//  the span GetSelectedSpan gives: rounded at the pane's outer radius at its
//  two far corners, square along the edge it shares with the pane, and
//  extended one line past that edge so it fills the line it opens into.
//  Nothing is drawn outside that span and that line; the pane's frame draws
//  the outline and the joins. In a pane too small to round, it is square.
//
//  A side a scroll arrow cuts off is square to the arrow, as the frame's
//  outline runs straight on to it there: the fill reaches a radius past the
//  cut, so its rounded corner falls outside what shows.
//
//  A document's tabs sit above their pane, a tool window's below it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintSelectedBody (IDxuiPainter & painter, const RECT & tab, uint32_t fillArgb) const
{
    long   t         = DxuiPaneMetrics::GetLinePx (m_scaler);
    long   ro        = GetCornerPx();
    bool   below     = m_style == Style::ToolWindow;
    long   left      = tab.left;
    long   right     = tab.right;
    bool   openLeft  = false;
    bool   openRight = false;
    bool   shown     = GetSelectedSpan (left, right, openLeft, openRight);
    long   fillLeft  = 0;
    long   fillRight = 0;
    RECT   clip      = {};
    float  depth     = (float) (tab.bottom - tab.top + t + ro);
    float  top       = below ? (float) (tab.top - t - ro) : (float) tab.top;



    if (!shown)
    {
        return;
    }

    fillLeft  = openLeft  ? left  - ro : left;
    fillRight = openRight ? right + ro : right;
    clip      = below ? RECT { left, tab.top - t, right, tab.bottom } : RECT { left, tab.top, right, tab.bottom + t };

    painter.PushClip ((float) clip.left, (float) clip.top, (float) (clip.right - clip.left), (float) (clip.bottom - clip.top));

    if (ro <= 0 || fillRight - fillLeft < 2 * ro)
    {
        painter.FillRect ((float) clip.left, (float) clip.top, (float) (clip.right - clip.left), (float) (clip.bottom - clip.top), fillArgb);
    }
    else
    {
        painter.FillRoundedRect ((float) fillLeft, top, (float) (fillRight - fillLeft), depth, (float) ro, fillArgb);
    }

    painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintHoverBox
//
//  The tab under the pointer, as Visual Studio draws it: a translucent box
//  as wide as the tab and the band's full depth, rounded at the pane's outer
//  radius at its two far corners and square along the line, with no
//  outline. The pane's frame draws the selected tab's joins after it, so a
//  join flares over a hovered neighbor's box. In a pane too small to round,
//  it is square.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintHoverBox (
    IDxuiPainter  & painter,
    const RECT    & tab,
    uint32_t        argb) const
{
    long   ro     = GetCornerPx();
    float  left   = (float) tab.left;
    float  width  = (float) (tab.right - tab.left);
    float  height = (float) (tab.bottom - tab.top);
    float  top    = (m_style == Style::ToolWindow) ? (float) (tab.top - ro) : (float) tab.top;



    painter.PushClip (left, (float) tab.top, width, height);

    if (ro <= 0 || tab.right - tab.left < 2 * ro)
    {
        painter.FillRect (left, (float) tab.top, width, height, argb);
    }
    else
    {
        painter.FillRoundedRect (left, top, width, height + (float) ro, (float) ro, argb);
    }

    painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintHoverPill
//
//  The wash under the pointer on a scroll arrow or the + in the compact
//  styles: as wide as what it washes, standing in from the strip's edges by
//  the hover inset, with rounded corners.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintHoverPill (IDxuiPainter & painter, const RECT & rect, uint32_t argb) const
{
    float  inset  = (float) GetHoverInsetPx (m_scaler);
    float  corner = (float) m_scaler.ToPx (s_kCompactCornerDip);
    float  height = (float) (rect.bottom - rect.top) - 2.0f * inset;



    painter.FillRoundedRect ((float) rect.left, (float) rect.top + inset, (float) (rect.right - rect.left), (std::max) (0.0f, height), corner, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::GetTipAt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTabStrip::GetTipAt (int x, int y, RECT & tabRect) const
{
    int  hit = HitTest (x, y);



    if (hit < 0)
    {
        return std::wstring();
    }

    tabRect = GetTabScreenRect (hit);
    return m_tabs[(size_t) hit].tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::MeasureTabPx
//
//  The label's width, measured when a renderer can, plus the style's padding,
//  the icon, the mark and the close button, so the label is never cut short
//  in a tab the host sized with this.
//
//  A compact tab is Visual Studio's: the pane's text inset ahead of the
//  label, the label's width rounded, 6.5 DIP more, and a line at its end. A
//  document tab adds the pin's and the close button's squares ahead of that
//  line, whether or not either button shows, so neither hover nor selection
//  changes its width; a tool window's tabs have neither button. Every part
//  is in the whole pixels PaintCompactTab places them by.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::MeasureTabPx (IDxuiTextRenderer * text, const Tab & tab, Style style, bool hasClose, const DxuiDpiScaler & scaler)
{
    static constexpr float  kExplorerFontDip = 13.0f;
    static constexpr float  kExplorerPadDip  = 8.0f;
    static constexpr float  kLabelGapDip     = 6.5f;   // from a label's end to its tab's pin, or its end
    bool                    compact          = style != Style::Explorer;
    float                   fontPx           = scaler.ToPxf (compact ? (float) s_kCompactFontDip : kExplorerFontDip);
    float                   labelW           = scaler.ToPxf ((float) s_kCharEstimateDip) * (float) tab.label.size();
    float                   height           = 0.0f;
    float                   width            = 0.0f;
    long                    inset            = DxuiPaneMetrics::GetTextInsetPx (scaler);
    long                    line             = DxuiPaneMetrics::GetLinePx (scaler);
    long                    buttons          = (style == Style::Document) ? 2L * scaler.ToPx (kTabButtonDip) : 0L;
    HRESULT                 hr               = E_FAIL;



    if (text != nullptr)
    {
        hr = text->MeasureString (tab.label.c_str(), fontPx, DxuiTheme::kBodyFace, labelW, height);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (compact)
    {
        width  = (float) (inset + std::lround (labelW) + std::lround (scaler.ToPxf (kLabelGapDip)) + buttons + line);
        width += tab.mark.empty() ? 0.0f : (float) scaler.ToPx (s_kCompactMarkDip);
    }
    else
    {
        width = scaler.ToPxf ((float) s_kLabelInsetDip) + labelW + scaler.ToPxf (kExplorerPadDip);
        width += (hasClose && tab.closable) ? scaler.ToPxf ((float) s_kCloseCenterDip + s_kCloseBoxDip / 2) : 0.0f;
    }

    return (int) std::ceil (width);
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
    uint32_t  plain = (m_style == Style::Explorer) ? theme.BackgroundElevated() : theme.ContentBackground();
    Palette   pal;



    //  The strip's own fill, when the host gives one, goes down first.
    if (m_stripFill != 0 && HasBounds())
    {
        painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                          (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top), m_stripFill);
    }

    //  The selected tab takes the color of the row it joins, which the host
    //  sets; without one it takes the elevated surface in the Explorer style
    //  and the content color in the compact styles. A hovered tab is a
    //  faint wash of the text color, which reads in either theme, as Explorer's
    //  gray does, where the theme's hover is a saturated selection color.
    pal.strip        = (m_stripFill    != 0) ? m_stripFill    : theme.Background();
    pal.hover        = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;
    pal.fill         = (m_selectedFill != 0) ? m_selectedFill : plain;
    pal.divider      = theme.Divider();
    pal.text         = theme.Foreground();
    pal.focus        = theme.FocusRing();
    pal.tabHover     = GetTabHoverFill (theme);
    pal.label        = GetLabelInk (theme, false);
    pal.labelFocused = GetLabelInk (theme, true);
    pal.labelHovered = GetHoveredLabelInk (theme);
    pal.glyph        = GetGlyphInk (theme, false);
    pal.glyphFocused = GetGlyphInk (theme, true);
    pal.glyphHovered = GetHoveredGlyphInk (theme);

    PaintInternal (painter, text, pal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move is reported handled only when it changes which tab is hovered or
//  armed, as DxuiButton does, or while it drags a tab: a move that changes
//  nothing passes through to the widgets that handle it.
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
