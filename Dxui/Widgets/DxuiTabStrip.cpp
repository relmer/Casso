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
#include "Widgets/DxuiPaneFrame.h"





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
        RECT  tab = GetTabScreenRect (hit);

        m_pressed  = hit;
        m_pressX   = x;
        m_pressY   = y;
        m_dragging = false;
        m_tornOff  = false;
        m_grab     = POINT { x - tab.left, y - tab.top };
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
//  threshold, either way, becomes a drag: the pressed tab is lifted out of
//  the row, the others close up and open a gap where it would drop, and with
//  the pointer past either end of the strip the tabs scroll toward it. Far
//  enough off the strip the tab is torn off, and the gap closes.
//
//  Only a host that reorders its own tabs gets the drag. Without a move
//  handler the strip's order would no longer match the host's, so a held
//  press stays hover, which cancels it once the pointer leaves its tab.
//
//  With a drag-out handler, a tab dragged off the tabs, past the strip's top
//  or bottom or past either end of the row of tabs, goes to the host
//  instead; with no move handler as well, any drag does.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::OnMouseMove (int x, int y)
{
    int   threshold = m_scaler.ToPx (s_kDragThresholdDip);
    int   step      = m_scaler.ToPx (s_kDragScrollDip);
    bool  handled   = false;
    bool  started   = false;
    bool  moved     = std::abs (x - m_pressX) > threshold || std::abs (y - m_pressY) > threshold;
    int   carried   = m_pressed;



    if (m_pressed >= 0 && m_dragOut && moved && (IsOffTheTabs (x, y, threshold) || !m_move))
    {
        m_pressed  = -1;
        m_dragging = false;
        m_tornOff  = false;
        m_dragSlides.UpdateDragGap();
        m_dragOut (carried, POINT { x, y });
        return true;
    }

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



    if (m_gapIndex >= 0 && index >= m_gapIndex)
    {
        shift += m_gapPx;
    }

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



    if (IsCompact())
    {
        return GetTabButtonRect (index, TabButton::Close);
    }

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
    DxuiTabStripPalette pal;



    pal.hover        = kExplorerHover;
    pal.fill         = kExplorerSelected;
    pal.text         = kExplorerText;
    pal.tabHover     = 0x0FFFFFFF;
    pal.label        = kExplorerText;
    pal.labelFocused = kExplorerText;
    pal.labelHovered = kExplorerText;
    pal.glyph        = kExplorerText;
    pal.glyphFocused = kExplorerText;
    pal.glyphHovered = kExplorerText;

    if (IsCompact())
    {
        m_painter.PaintCompact (painter, text, pal);
    }
    else if (m_style == DxuiTabStripStyle::Explorer)
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
    if (IsCompact())
    {
        m_painter.PaintCompactThemed (painter, text, theme);
    }
    else if (m_style == DxuiTabStripStyle::Explorer)
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
    int   prevPin     = m_hoverPin;
    bool  prevNewTab  = m_hoverNewTab;
    bool  handled     = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        handled = OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        handled = handled || m_hover != prevHover || m_pressed != prevPressed ||
                  m_hoverArrow != prevArrow || m_hoverClose != prevClose || m_hoverPin != prevPin || m_hoverNewTab != prevNewTab;
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
            m_hoverPin    = -1;
            m_hoverNewTab = false;
        }

        handled = m_hover != prevHover || m_hoverArrow != prevArrow || m_hoverClose != prevClose || m_hoverPin != prevPin || m_hoverNewTab != prevNewTab;
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





////////////////////////////////////////////////////////////////////////////////
//
//  IsOffTheTabs
//
//  Past the strip's top or bottom, or past the row of tabs at either end:
//  the first tab's left edge or the last tab's right edge, as far as they
//  lie within the strip, by more than the threshold. While the tabs
//  overflow, the row's ends are the strip's, so a drag over the arrows scrolls.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsOffTheTabs (int x, int y, int threshold) const
{
    long  left  = m_boundsDip.left;
    long  right = m_boundsDip.right;



    if (!m_tabs.empty())
    {
        left  = (std::max) (left,  GetTabScreenRect (0).left);
        right = (std::min) (right, GetTabScreenRect ((int) m_tabs.size() - 1).right);
    }

    return HasBounds() && (y < m_boundsDip.top - threshold || y >= m_boundsDip.bottom + threshold ||
                           x < left - threshold || x >= right + threshold);
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
//  Ahead of the tab whose left half has `x`, or after it for its right half;
//  past the last tab, the tab count. The tabs are taken where they are drawn,
//  so a pointer inside an open gap keeps that gap.
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



    if (!IsCompact() || m_selected < 0 || m_selected >= (int) m_tabs.size())
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
//  GetTabButtonRect
//
//  Visual Studio's squares on a document tab, ToPx (24) across: the close
//  button ending one line in from the tab's right end and the pin just left
//  of it, each spanning the tab between its outline and the line along the
//  band. At 150% the close glyph's ink then ends 12 px in from the tab's
//  end, and the two glyphs are 36 px apart. A tool window's tabs have none.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabStrip::GetTabButtonRect (int index, TabButton button) const
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

DxuiTabStrip::TabButton DxuiTabStrip::GetTabButtonAt (int x, int y, int & index, RECT & button) const
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
//  The standard and Explorer styles show every tab's close button. Visual
//  Studio's document tabs show it on the selected tab and the one under the
//  pointer, and its tool windows' tabs show none.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsCloseShown (int index) const
{
    bool  isLit = index == m_selected || index == m_hover;
    bool  shown = !IsCompact() || (m_style == Style::Document && isLit);



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

int DxuiTabStrip::GetPinAt (int x, int y) const
{
    int   hit    = -1;
    bool  inView = !IsOverflowing() || (x >= GetViewLeft() && x < GetViewRight());



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

uint32_t DxuiTabStrip::GetLabelInk (const IDxuiTheme & theme, bool focusedSelected)
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

uint32_t DxuiTabStrip::GetGlyphInk (const IDxuiTheme & theme, bool focusedSelected)
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
//  is in the whole pixels the painter places them by.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabStrip::MeasureTabPx (IDxuiTextRenderer * text, const Tab & tab, Style style, bool hasClose, const DxuiDpiScaler & scaler)
{
    static constexpr float  kExplorerPadDip  = 8.0f;
    static constexpr float  kLabelGapDip     = 6.5f;   // from a label's end to its tab's pin, or its end
    bool                    compact          = style == Style::Document || style == Style::ToolWindow;
    float                   fontPx           = scaler.ToPxf (compact ? (float) s_kCompactFontDip
                                                                     : (style == Style::Explorer) ? kExplorerLabelFontDip : kLabelFontDip);
    float                   labelW           = scaler.ToPxf ((float) s_kCharEstimateDip) * (float) tab.label.size();
    float                   height           = 0.0f;
    float                   width            = 0.0f;
    long                    inset            = DxuiPaneMetrics::GetTextInsetPx (scaler);
    long                    line             = DxuiPaneMetrics::GetLinePx (scaler);
    long                    buttons          = (style == Style::Document) ? 2L * scaler.ToPx (kTabButtonDip) : 0L;
    HRESULT                 hr               = E_FAIL;



    if (text != nullptr)
    {
        hr = text->MeasureString (tab.label.c_str(), fontPx, GetLabelFace(), labelW, height);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (compact)
    {
        width  = (float) (inset + std::lround (labelW) + std::lround (scaler.ToPxf (kLabelGapDip)) + buttons + line);
        width += tab.mark.empty() ? 0.0f : (float) scaler.ToPx (s_kCompactMarkDip);
    }
    else
    {
        width = (tab.icon ? scaler.ToPxf ((float) s_kLabelInsetDip) : scaler.ToPxf (kLabelPadXDip)) + labelW + scaler.ToPxf (kExplorerPadDip);
        width += (hasClose && tab.closable) ? scaler.ToPxf ((float) s_kCloseCenterDip + s_kCloseBoxDip / 2) : 0.0f;
    }

    return (int) std::ceil (width);
}
