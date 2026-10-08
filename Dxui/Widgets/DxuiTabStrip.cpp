#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTabStrip.h"
#include "Widgets/DxuiPaneFrame.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiTextElide.h"
#include "Core/DxuiUnicodeSymbols.h"





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
    bool  newTab   = m_pressedNewTab;
    bool  consumed = (pressed >= 0) && (m_dragging || hit == pressed);



    //  A drag ends on the tab it carried, wherever the pointer is.
    m_pressed       = -1;
    m_dragging      = false;
    m_pressedArrow  = 0;
    m_pressedNewTab = false;
    m_pressedClose  = -1;

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
    constexpr float  s_kPressedScale   = 0.82f;



    HRESULT   hr      = S_OK;
    int       arrow   = GetArrowWidthPx();
    float     left    = (float) ((direction < 0) ? (int) m_boundsDip.left : GetViewRight());
    float     top     = (float) m_boundsDip.top;
    float     height  = (float) (m_boundsDip.bottom - m_boundsDip.top);
    bool      enabled = CanScroll (direction);
    uint32_t  color   = enabled ? textArgb : DxuiColor::Scale (textArgb, s_kArrowIdleScale);
    uint32_t  wash    = (m_pressedArrow == direction) ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb;
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
    constexpr float  s_kGlyphDip     = 16.0f;
    constexpr float  s_kPressedScale = 0.82f;



    HRESULT   hr   = S_OK;
    RECT      rc   = GetNewTabRect();
    uint32_t  wash = m_pressedNewTab ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb;



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
    long  reach     = DxuiPaneFrame::GetFlushReachPx (DxuiPaneMetrics::GetCornerPx (m_scaler), DxuiPaneMetrics::GetLinePx (m_scaler));



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
//  A pill stands in from the strip's edges by the compact inset, and never
//  less than the outer corner radius less a line: that keeps it out of the
//  rows where the selected tab's joins curve into the line along the band.
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
//  The close button's square, centered where Explorer centers its glyph, or
//  in the compact styles a small square just inside the tab's right end.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetCloseRect (int index) const
{
    RECT  tab     = GetTabScreenRect (index);
    bool  compact = m_style != Style::Explorer;
    int   box     = m_scaler.ToPx (compact ? s_kCompactCloseDip : s_kCloseBoxDip);
    int   center  = compact ? tab.right - m_scaler.ToPx (s_kCompactPadDip / 2) - box / 2 : tab.right - m_scaler.ToPx (s_kCloseCenterDip);
    int   top     = tab.top + ((tab.bottom - tab.top) - box) / 2;



    return RECT { center - box / 2, top, center - box / 2 + box, top + box };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCloseShown
//
//  Explorer shows every tab's close button. Visual Studio's document tabs
//  show it on the selected tab and the one under the pointer; its tool
//  windows close from their title bar, not their tabs.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsCloseShown (int index) const
{
    bool  shown = false;



    switch (m_style)
    {
    case Style::Explorer:
        shown = true;
        break;

    case Style::Document:
        shown = index == m_selected || index == m_hover;
        break;

    case Style::ToolWindow:
        shown = false;
        break;
    }

    return m_close && shown && m_tabs[(size_t) index].closable;
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
    constexpr uint32_t  s_kTabSelected = 0xFF2C2C2C;
    constexpr uint32_t  s_kDivider     = 0xFF323232;
    constexpr uint32_t  s_kTextArgb    = 0xFFFFFFFF;
    constexpr uint32_t  s_kFocusRing   = 0xFFAACCFF;



    PaintInternal (painter, text, s_kStrip, s_kTabHover, s_kTabSelected, s_kDivider, s_kTextArgb, s_kFocusRing);
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
                                  uint32_t stripArgb, uint32_t hoverArgb, uint32_t fillArgb, uint32_t dividerArgb,
                                  uint32_t textArgb, uint32_t focusArgb) const
{
    constexpr float  s_kFontDip        = 13.0f;
    constexpr float  s_kFocusThickDip  = 1.0f;
    constexpr float  s_kFocusInsetDip  = 1.0f;
    constexpr float  s_kPadXDp         = 8.0f;
    constexpr float  s_kDividerDip     = 16.0f;
    constexpr float  s_kPressedScale   = 0.82f;   // armed-tab tint, a touch darker than hover
    constexpr float  s_kMutedTextScale = 0.8f;    // Explorer's #CCCCCC on an unselected tab



    HRESULT   hr         = S_OK;
    int       i          = 0;
    size_t    n          = m_tabs.size();
    bool      compact    = m_style != Style::Explorer;
    long      line       = DxuiPaneMetrics::GetLinePx (m_scaler);
    long      lineAbove  = (m_style == Style::ToolWindow) ? line : 0;
    float     focusThick = m_scaler.ToPxf (s_kFocusThickDip);
    float     focusInset = m_scaler.ToPxf (s_kFocusInsetDip);
    float     padX       = m_scaler.ToPxf (s_kPadXDp);
    float     fontDip    = m_scaler.ToPxf (s_kFontDip);
    float     corner     = m_scaler.ToPxf ((float) s_kCornerDip);
    float     iconPx     = m_scaler.ToPxf ((float) s_kIconDip);
    float     dividerH   = m_scaler.ToPxf (s_kDividerDip);
    uint32_t  mutedText  = DxuiColor::Scale (textArgb, s_kMutedTextScale);
    uint32_t  closeHover = (textArgb & 0x00FFFFFFu) | 0x1F000000u;
    uint32_t  closePress = (textArgb & 0x00FFFFFFu) | 0x33000000u;



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
                PaintCompactTab (painter, text, i, hoverArgb, fillArgb, textArgb);
            }

            continue;
        }

        if (isSel)
        {
            painter.FillRoundedRect (left, top, width, height, corner, fillArgb);
            painter.FillRect        (left, bottom - corner, width, corner, fillArgb);

            //  The flare: a square of fill beside each bottom corner with a
            //  quarter circle of the strip cut out of it.
            painter.FillRect         (left - corner,          bottom - corner, corner, corner, fillArgb);
            painter.FillCircle (left - corner,          bottom - corner, corner, stripArgb);
            painter.FillRect         (left + width,           bottom - corner, corner, corner, fillArgb);
            painter.FillCircle (left + width + corner,  bottom - corner, corner, stripArgb);
        }
        else if (isHover || isArmed)
        {
            painter.FillRoundedRect (left, top, width, height, corner,
                                     isArmed ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
        }
        else if (!nextLit && i + 1 < (int) n)
        {
            painter.FillRect (left + width - 1.0f, top + (height - dividerH) * 0.5f, 1.0f, dividerH, dividerArgb);
        }

        if (m_focused && m_focusCueVisible && isSel)
        {
            painter.OutlineRoundedRect (left + focusInset, top + focusInset,
                                        width - focusInset * 2.0f, height - focusInset * 2.0f,
                                        corner, focusThick, focusArgb);
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
                                  isSel ? textArgb : mutedText,
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
                              isSel ? textArgb : mutedText,
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
        PaintCompactTab (painter, text, m_selected, hoverArgb, fillArgb, textArgb);
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
        PaintArrow (painter, text, -1, hoverArgb, textArgb);
        PaintArrow (painter, text,  1, hoverArgb, textArgb);
    }

    if (GetNewTabWidthPx() > 0)
    {
        PaintNewTab (painter, text, hoverArgb, textArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintCompactTab
//
//  Visual Studio's tabs. The selected tab is filled with its pane's color,
//  rounded at its far corners and open into the pane; the pane's frame draws
//  its outline. The rest are plain text, washed in a rounded pill while
//  hovered. Each label starts where a pane's title does, at the text inset
//  from the tab's left edge in whole pixels, so the first tab's label lines
//  up with a title to the pixel.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintCompactTab (IDxuiPainter & painter, IDxuiTextRenderer & text, int index,
                                    uint32_t hoverArgb, uint32_t fillArgb, uint32_t textArgb) const
{
    static constexpr float    kMutedTextScale = 0.75f;
    static constexpr float    kPressedScale   = 0.82f;
    static constexpr float    kCloseScale     = 0.8f;
    const Tab               & tab             = m_tabs[(size_t) index];
    RECT                      r               = GetTabScreenRect (index);
    float                     left            = (float) r.left;
    float                     top             = (float) r.top;
    float                     height          = (float) (r.bottom - r.top);
    float                     fontPx          = m_scaler.ToPxf ((float) s_kCompactFontDip);
    float                     labelX          = left + (float) m_scaler.ToPx (s_kCompactPadDip);
    float                     labelR          = (float) r.right - (float) m_scaler.ToPx (s_kCompactPadDip);
    float                     corner          = (float) m_scaler.ToPx (s_kCompactCornerDip);
    bool                      isSel           = index == m_selected;
    bool                      isHover         = index == m_hover || (index == m_pressed && index == m_hover);
    uint32_t                  color           = isSel ? textArgb : DxuiColor::Scale (textArgb, kMutedTextScale);
    DxuiTextHAlign            align           = DxuiTextHAlign::Left;
    std::wstring              shown;
    HRESULT                   hr              = S_OK;



    if (isSel)
    {
        PaintSelectedBody (painter, r, fillArgb);
    }
    else if (isHover)
    {
        PaintHoverPill (painter, r, hoverArgb);
    }

    //  A tool window's tabs center their titles, as Visual Studio's do.
    if (m_style == Style::ToolWindow && tab.mark.empty())
    {
        align = DxuiTextHAlign::Center;
    }

    if (!tab.mark.empty() && tab.markArgb != 0)
    {
        hr = text.DrawString (tab.mark.c_str(), labelX, top, m_scaler.ToPxf ((float) s_kCompactMarkDip), height,
                              tab.markArgb, fontPx, tab.markFace.empty() ? DxuiTheme::kBodyFace : tab.markFace.c_str(),
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        labelX += m_scaler.ToPxf ((float) s_kCompactMarkDip);
    }

    if (IsCloseShown (index))
    {
        RECT  close = GetCloseRect (index);

        labelR = (float) close.left;

        if (m_hoverClose == index)
        {
            painter.FillRoundedRect ((float) close.left, (float) close.top,
                                     (float) (close.right - close.left), (float) (close.bottom - close.top), corner,
                                     (m_pressedClose == index) ? DxuiColor::Darken (hoverArgb, kPressedScale) : hoverArgb);
        }

        hr = text.DrawString (s_kpszMdl2Cancel,
                              (float) close.left, (float) close.top,
                              (float) (close.right - close.left), (float) (close.bottom - close.top),
                              color, m_scaler.ToPxf ((float) s_kCloseGlyphDip) * kCloseScale, m_iconFace,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    shown = DxuiTextElide::ToWidth (text, tab.label, fontPx, DxuiTheme::kBodyFace, (std::max) (labelR - labelX, 0.0f), DxuiElide::Tail);

    hr = text.DrawString (shown.c_str(), labelX, top, (std::max) (labelR - labelX, 0.0f), height,
                          color, fontPx, DxuiTheme::kBodyFace,
                          align, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
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
//  the outline and the joins.
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
    long   ro        = DxuiPaneMetrics::GetCornerPx (m_scaler);
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

    if (fillRight - fillLeft < 2 * ro)
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
//  DxuiTabStrip::PaintHoverPill
//
//  The wash under the pointer in the compact styles, on a tab, a scroll
//  arrow or the +: as wide as what it washes, standing in from the strip's
//  edges by the hover inset, with rounded corners.
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
//  in a tab the host sized with this. A compact tab's padding is counted in
//  the whole pixels PaintCompactTab places its label by.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::MeasureTabPx (IDxuiTextRenderer * text, const Tab & tab, Style style, bool hasClose, const DxuiDpiScaler & scaler)
{
    static constexpr float  kExplorerFontDip = 13.0f;
    static constexpr float  kExplorerPadDip  = 8.0f;
    bool                    compact          = style != Style::Explorer;
    float                   fontPx           = scaler.ToPxf (compact ? (float) s_kCompactFontDip : kExplorerFontDip);
    float                   labelW           = scaler.ToPxf ((float) s_kCharEstimateDip) * (float) tab.label.size();
    float                   height           = 0.0f;
    float                   width            = 0.0f;
    HRESULT                 hr               = E_FAIL;



    if (text != nullptr)
    {
        hr = text->MeasureString (tab.label.c_str(), fontPx, DxuiTheme::kBodyFace, labelW, height);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (compact)
    {
        width = (float) scaler.ToPx (s_kCompactPadDip) * 2.0f + labelW;
        width += tab.mark.empty() ? 0.0f : scaler.ToPxf ((float) s_kCompactMarkDip);
        width += (hasClose && tab.closable && style == Style::Document) ? scaler.ToPxf ((float) s_kCompactCloseDip) : 0.0f;
        width  = (style == Style::ToolWindow) ? (std::max) (width, scaler.ToPxf ((float) s_kToolTabMinDip)) : width;
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
    uint32_t  fill  = (m_selectedFill != 0) ? m_selectedFill : plain;
    uint32_t  strip = (m_stripFill    != 0) ? m_stripFill    : theme.Background();
    uint32_t  hover = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;



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
    PaintInternal (painter, text,
                   strip,
                   hover,
                   fill,
                   theme.Divider(),
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
