#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTabStrip.h"
#include "DxuiTabDragModel.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiAnimation.h"
#include "Core/DxuiSystemSettings.h"
#include "Core/DxuiTextElide.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::DxuiTabStrip
//
//  Tabs slide to their places only while the system's animations are on.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabStrip::DxuiTabStrip()
{
    m_focusable = true;
    m_dragSlides.m_animate   = DxuiSystemSettings::Instance().AreAnimationsEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLabelFace
//
//  The face both styles draw tab labels in, which is the theme's UI face. A
//  host sizing tabs to their labels measures in this face, so the measure
//  and the drawing cannot disagree.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiTabStrip::GetLabelFace()
{
    return DxuiTheme::GetUiFace();
}





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



    //  Tab rects are in strip coordinates before scrolling, and while the
    //  arrows show, only the part of the strip between them shows tabs.
    inView = !IsOverflowing() || (x >= GetViewLeft() && x < GetViewRight());

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
        m_pressed  = hit;
        m_pressX   = x;
        m_pressY   = y;
        m_dragging = false;
        m_tornOff  = false;
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



    //  A drag ends where the gap is, wherever the pointer is, or torn off.
    if (m_dragging)
    {
        EndDrag (x, y);
        return true;
    }

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
//  threshold, either way, becomes a drag: the pressed tab is lifted out of
//  the row, the others close up and open a gap where it would drop, and with
//  the pointer past either end of the strip the tabs scroll toward it. Far
//  enough off the strip the tab is torn off, and the gap closes.
//
//  Only a host that reorders its own tabs gets the drag. Without a move
//  handler the strip's order would no longer match the host's, so a held
//  press stays hover, which cancels it once the pointer leaves its tab.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouseMove (int x, int y)
{
    int   threshold = m_scaler.ToPx (s_kDragThresholdDip);
    int   step      = m_scaler.ToPx (s_kDragScrollDip);
    bool  handled   = false;
    bool  started   = false;



    if (m_pressed < 0 || !m_move)
    {
        SetMouseHover (x, y);
        return false;
    }

    started = !m_dragging && (std::abs (x - m_pressX) > threshold || std::abs (y - m_pressY) > threshold);

    if (started)
    {
        RECT  tab = GetTabScreenRect (m_pressed);

        m_dragging = true;
        m_grab     = POINT { m_pressX - tab.left, m_pressY - tab.top };
    }

    if (m_dragging)
    {
        m_tornOff = HasBounds() && DxuiTabDragModel::IsTornOff (m_boundsDip, POINT { x, y }, m_scaler.ToPx (s_kTearOffDip));

        if (!m_tornOff && HasBounds() && x >= GetViewRight())
        {
            m_scrollPx += step;
        }
        else if (!m_tornOff && HasBounds() && x < GetViewLeft())
        {
            m_scrollPx -= step;
        }

        ClampScroll();

        m_dragX = x;
        m_hover = -1;
        handled = true;

        m_dragSlides.UpdateDragGap();

        if (m_dragMove)
        {
            m_dragMove (m_pressed, POINT { x, y }, m_tornOff);
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDrag
//
//  A drop on the strip moves the tab into the gap and selects it; it slides
//  the rest of the way in from where it was let go. A torn-off drop moves
//  nothing, and the host handles it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::EndDrag (int x, int y)
{
    int    index   = m_pressed;
    int    gap     = m_dragSlides.m_gap;
    bool   tornOff = m_tornOff;
    float  dropped = (float) (GetContentX (x) - m_grab.x);



    if (!tornOff && gap >= 0)
    {
        MoveDraggedTab (gap);
        index = gap;
    }

    m_pressed       = -1;
    m_dragging      = false;
    m_tornOff       = false;
    m_pressedArrow  = 0;
    m_pressedNewTab = false;
    m_pressedClose  = -1;

    m_dragSlides.UpdateDragGap();

    if (!tornOff && index >= 0 && index < (int) m_tabs.size() && m_dragSlides.m_animate)
    {
        DxuiTabSlides::Slide &  slide = m_dragSlides.m_slides[(size_t) index];

        slide.from    = dropped;
        slide.startMs = m_dragSlides.GetNowMs();
    }

    if (!tornOff)
    {
        Commit (index);
    }

    if (m_dragEnd)
    {
        m_dragEnd (index, POINT { x, y }, tornOff);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CancelDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::CancelDrag()
{
    m_pressed  = -1;
    m_dragging = false;
    m_tornOff  = false;

    m_dragSlides.UpdateDragGap();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPaintedSelection
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetPaintedSelection() const
{
    return (m_dragging && m_selected == m_pressed) ? -1 : m_selected;
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
//  New tabs are drawn where they are laid out at once, so a window being
//  sized does not trail its tabs; only a slide already under way, such as a
//  dropped tab's, continues to the new layout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::SetTabs (std::vector<Tab> tabs)
{
    bool  keepSlides = tabs.size() == m_tabs.size() && IsSliding();



    m_tabs = std::move (tabs);

    //  With fewer tabs than before, the last one is selected.
    m_selected = m_tabs.empty() ? 0 : std::clamp (m_selected, 0, (int) m_tabs.size() - 1);

    if (m_pressed >= (int) m_tabs.size())
    {
        m_pressed  = -1;
        m_dragging = false;
    }

    if (!keepSlides)
    {
        m_dragSlides.SnapSlides();
    }

    m_dragSlides.UpdateDragGap();
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
//  tab it was on, and each tab's slide moves with it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::MoveDraggedTab (int to)
{
    int   from   = m_pressed;
    LONG  left   = 0;
    bool  slides = m_dragSlides.m_slides.size() == m_tabs.size() + 1;



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

    if (slides && from < to)
    {
        std::rotate (m_dragSlides.m_slides.begin() + from, m_dragSlides.m_slides.begin() + from + 1, m_dragSlides.m_slides.begin() + to + 1);
    }
    else if (slides)
    {
        std::rotate (m_dragSlides.m_slides.begin() + to, m_dragSlides.m_slides.begin() + from, m_dragSlides.m_slides.begin() + from + 1);
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
//  A strip in either style scrolls when its tabs do not fit.
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
//  GetNewTabWidthPx
//
//  Zero with no new-tab handler, so a strip without one has no + button.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetNewTabWidthPx() const
{
    return (HasBounds() && m_newTab) ? m_scaler.ToPx (kNewTabWidthDip) + (int) std::lround (m_scaler.ToPxf (m_newTabGapDip)) : 0;
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
            //  It slides along after the last tab as a drag opens and closes
            //  the gap.
            left = (LONG) std::lround (m_dragSlides.GetSlideLeft (m_tabs.size())) + GetArrowWidthPx() - m_scrollPx;
        }

        //  The gap is before the button, not part of it.
        left  += width - m_scaler.ToPx (kNewTabWidthDip);
        width  = m_scaler.ToPx (kNewTabWidthDip);

        //  Over the tabs' own span rather than the whole strip, which reaches
        //  above them into the caption.
        rc = RECT { left, m_tabs.empty() ? m_boundsDip.top : m_tabs.front().rect.top, left + width, m_boundsDip.bottom };
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
//  GetTabScreenRect
//
//  Where tab `index` is drawn: where it has slid to, moved by the scroll and
//  past the left arrow.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetTabScreenRect (int index) const
{
    const RECT &  r     = m_tabs[(size_t) index].rect;
    int           shift = GetArrowWidthPx() - m_scrollPx;
    LONG          left  = (LONG) std::lround (m_dragSlides.GetSlideLeft ((size_t) index));



    return RECT { left + shift, r.top, left + (r.right - r.left) + shift, r.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCloseRect
//
//  The close button's square, centered where Explorer centers its glyph.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetCloseRect (int index) const
{
    RECT  tab    = GetTabScreenRect (index);
    int   box    = m_scaler.ToPx (s_kCloseBoxDip);
    int   center = tab.right - ((m_style == DxuiTabStripStyle::Explorer) ? (int) std::lround (m_scaler.ToPxf (s_kExplorerCloseCenterDip))
                                                                     : m_scaler.ToPx (s_kCloseCenterDip));
    int   top    = tab.top + ((tab.bottom - tab.top) - box) / 2;



    return RECT { center - box / 2, top, center - box / 2 + box, top + box };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCloseAt
//
//  The tab whose close button is under the point, or -1. While the arrows
//  show, only the part of the strip between them shows tabs, so only that
//  part can hold one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::GetCloseAt (int x, int y) const
{
    int   hit    = -1;
    int   i      = 0;
    bool  inView = !IsOverflowing() || (x >= GetViewLeft() && x < GetViewRight());



    if (m_close && m_enabled && inView)
    {
        for (i = 0; i < (int) m_tabs.size() && hit < 0; i++)
        {
            RECT  rc = GetCloseRect (i);

            if (x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom)
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
//  Draws with fixed dark colors, for a host with no theme.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr uint32_t  kFocusRing        = 0xFFAACCFF;
    constexpr uint32_t  kStandardHover    = 0xFF38465E;
    constexpr uint32_t  kStandardSelected = 0xFF4C6480;
    constexpr uint32_t  kStandardText     = 0xFFE8EEF4;
    constexpr uint32_t  kExplorerStrip    = 0xFF202020;
    constexpr uint32_t  kExplorerHover    = 0x14FFFFFF;
    constexpr uint32_t  kExplorerSelected = 0xFF2C2C2C;
    constexpr uint32_t  kExplorerDivider  = 0xFF323232;
    constexpr uint32_t  kExplorerText     = 0xFFFFFFFF;



    if (m_style == DxuiTabStripStyle::Explorer)
    {
        m_painter.PaintExplorer (painter, text, kExplorerStrip, kExplorerHover, kExplorerSelected, kExplorerDivider, kExplorerText, kFocusRing);
    }
    else
    {
        m_painter.PaintStandard (painter, text, kStandardHover, kStandardSelected, kStandardText, kFocusRing);
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
    LONG  oldWidth = m_boundsDip.right - m_boundsDip.left;
    UINT  oldDpi   = m_scaler.GetDpi();



    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    //  A tab selected before the first layout, or one a narrower strip cuts
    //  off, comes into view; after any other layout the scroll is where the
    //  wheel put it.
    if (m_boundsDip.right - m_boundsDip.left != oldWidth || m_scaler.GetDpi() != oldDpi)
    {
        ScrollIntoView (m_selected);
    }
    else
    {
        ClampScroll();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_style == DxuiTabStripStyle::Explorer)
    {
        m_painter.PaintExplorerThemed (painter, text, theme);
    }
    else
    {
        //  The selection color marks the selected tab's underline and hover is
        //  a subtle fill hint; unselected tabs blend with the page.
        m_painter.PaintStandard (painter, text,
                       theme.HoverBackground(),
                       theme.SelectionBackground(),
                       theme.Foreground(),
                       theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move is reported handled only when it drags a tab or changes which tab,
//  close button, scroll arrow or + button is hovered or armed, as DxuiButton
//  does: that is what makes the window repaint at once rather than on its
//  half-second tick, and a move that changes nothing still passes through to
//  the widgets that handle it.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouse (const DxuiMouseEvent & ev)
{
    int   prevHover   = m_hover;
    int   prevPressed = m_pressed;
    int   prevArrow   = m_hoverArrow;
    int   prevClose   = m_hoverClose;
    bool  prevNewTab  = m_hoverNewTab;
    bool  handled     = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        handled = OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        handled = handled || m_hover != prevHover || m_pressed != prevPressed ||
                  m_hoverArrow != prevArrow || m_hoverClose != prevClose || m_hoverNewTab != prevNewTab;
        break;
    case DxuiMouseEventKind::Wheel:
        //  A horizontal wheel's positive notch is rightward, a vertical one's
        //  is up, which moves the tabs the other way.
        //  Only over the strip: a host can offer the strip every wheel turn,
        //  as a property sheet does before it scrolls its page.
        handled = PtInRect (&m_boundsDip, ev.positionDip) && OnWheel (ev.wheelHorizontal ? -ev.wheelDelta : ev.wheelDelta);
        break;
    case DxuiMouseEventKind::Leave:
        if (m_pressed < 0)
        {
            m_hover       = -1;
            m_hoverArrow  = 0;
            m_hoverClose  = -1;
            m_hoverNewTab = false;
        }

        handled = m_hover != prevHover || m_hoverArrow != prevArrow || m_hoverClose != prevClose || m_hoverNewTab != prevNewTab;
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
