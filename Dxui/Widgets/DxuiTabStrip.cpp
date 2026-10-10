#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTabStrip.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiTextElide.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/DxuiTextRenderer.h"





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
    int   closing  = m_pressedClose;
    bool  newTab   = m_pressedNewTab;
    bool  consumed = (pressed >= 0) && (m_dragging || hit == pressed);



    //  A drag ends on the tab it moved, wherever the pointer is.
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
//  it scrolls the tabs toward the pointer.
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



    if (m_pressed < 0 || !m_move)
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
//  Only the Explorer style scrolls; a Standard strip draws tabs that do not
//  fit past its end, and never shows the arrows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::IsOverflowing() const
{
    bool  scrolls = m_style == DxuiTabStripStyle::Explorer;



    return scrolls && HasBounds() && !m_tabs.empty() && m_tabs.back().rect.right > m_boundsDip.right - GetNewTabWidthPx();
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



    if (enabled && m_hoverArrow == direction)
    {
        painter.FillRect (left, top, (float) arrow, height,
                          (m_pressedArrow == direction) ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
    }

    hr = text.DrawString ((direction < 0) ? s_kpszTriangleLeft : s_kpszTriangleRight,
                          left, top, (float) arrow, height,
                          color,
                          m_scaler.ToPxf (s_kArrowFontDip),
                          DxuiTheme::GetUiFace(),
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
            left = m_tabs.back().rect.right;
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
//  PaintNewTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintNewTab (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const
{
    constexpr float  s_kGlyphDip     = 12.0f;
    constexpr float  s_kHoverInset   = 4.0f;
    constexpr float  s_kPressedScale = 0.82f;



    HRESULT  hr    = S_OK;
    RECT     rc    = GetNewTabRect();
    float    inset = m_scaler.ToPxf (s_kHoverInset);



    //  Explorer's: a rounded box inside the button's span, and the icon font's
    //  thin Add glyph rather than a heavy text plus.
    if (m_hoverNewTab)
    {
        painter.FillRoundedRect ((float) rc.left + inset, (float) rc.top + inset,
                                 (float) (rc.right - rc.left) - inset * 2.0f, (float) (rc.bottom - rc.top) - inset * 2.0f,
                                 m_scaler.ToPxf ((float) s_kCornerDip),
                                 m_pressedNewTab ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
    }

    hr = text.DrawString (s_kpszMdl2Add,
                          (float) rc.left, (float) rc.top, (float) (rc.right - rc.left), (float) (rc.bottom - rc.top),
                          textArgb,
                          m_scaler.ToPxf (s_kGlyphDip),
                          DxuiTextRenderer::IsFontFamilyInstalled (L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets",
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



    return RECT { r.left + shift, r.top, r.right + shift, r.bottom };
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
        PaintExplorer (painter, text, kExplorerStrip, kExplorerHover, kExplorerSelected, kExplorerDivider, kExplorerText, kFocusRing);
    }
    else
    {
        PaintStandard (painter, text, kStandardHover, kStandardSelected, kStandardText, kFocusRing);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::GetHoverFill
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetHoverFill (int index, uint32_t hoverArgb) const
{
    constexpr float  s_kPressedScale = 0.82f;   // armed-tab tint, a touch darker than hover



    return (index == m_pressed && index == m_hover) ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::GetBesideFill
//
//  The hover is translucent over the strip, so it is mixed onto it here.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStrip::GetBesideFill (int index, uint32_t stripArgb, uint32_t hoverArgb) const
{
    uint32_t  hover = 0;



    if (index < 0 || index >= (int) m_tabs.size() || index != m_hover)
    {
        return stripArgb;
    }

    hover = GetHoverFill (index, hoverArgb);

    return DxuiColor::Mix (stripArgb, hover | 0xFF000000u, (float) (hover >> 24) / 255.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::HasTabIcon
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStrip::HasTabIcon (int index) const
{
    const Tab  & t = m_tabs[(size_t) index];



    return t.icon && !t.icon->bgraPremul.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintTabIcon
//
//  Draws tab `index`'s icon, if it has one, near its left edge and centered
//  on its height.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintTabIcon (IDxuiTextRenderer & text, int index) const
{
    HRESULT  hr     = S_OK;
    RECT     r      = GetTabScreenRect (index);
    float    iconPx = m_scaler.ToPxf ((float) s_kIconDip);
    float    height = (float) (r.bottom - r.top);



    if (HasTabIcon (index))
    {
        const DxuiIconImage  & icon = *m_tabs[(size_t) index].icon;

        hr = text.DrawIconBitmap (icon.bgraPremul.data(), icon.width, icon.height,
                                  (float) r.left + m_scaler.ToPxf ((float) s_kIconInsetDip), (float) r.top + (height - iconPx) * 0.5f, iconPx, iconPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintTabClose
//
//  Draws tab `index`'s close button: the glyph, over a faint wash of
//  `washArgb` while hovered, a stronger one while pressed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintTabClose (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    int                  index,
    uint32_t             glyphArgb,
    uint32_t             washArgb,
    float                cornerPx) const
{
    constexpr uint32_t  kHoverAlpha   = 0x1F000000u;
    constexpr uint32_t  kPressedAlpha = 0x33000000u;



    HRESULT   hr    = S_OK;
    RECT      close = GetCloseRect (index);
    uint32_t  alpha = (m_pressedClose == index) ? kPressedAlpha : kHoverAlpha;



    if (m_hoverClose == index)
    {
        painter.FillRoundedRect ((float) close.left, (float) close.top,
                                 (float) (close.right - close.left), (float) (close.bottom - close.top), cornerPx,
                                 (washArgb & 0x00FFFFFFu) | alpha);
    }

    hr = text.DrawString (s_kpszMdl2ChromeClose,
                          (float) close.left, (float) close.top,
                          (float) (close.right - close.left), (float) (close.bottom - close.top),
                          glyphArgb,
                          m_scaler.ToPxf ((m_style == DxuiTabStripStyle::Explorer) ? s_kExplorerCloseGlyphDip : (float) s_kCloseGlyphDip),
                          m_iconFace,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintStandard
//
//  The Standard style, with caller-supplied colors so the themed and fallback
//  Paint entry points share one body.
//
//  A connected-tab look: the selected tab shares the page background (no
//  chip fill) and is marked by a thick accent underline flush with the page
//  edge; the rest are unfilled with dimmed labels, and a hovered or armed one
//  gets a subtle fill hint. Each label is centered in its tab, inside the
//  label pad; an icon or a close button takes its room from that box.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintStandard (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    uint32_t             hoverArgb,
    uint32_t             underlineArgb,
    uint32_t             textArgb,
    uint32_t             focusArgb) const
{
    constexpr float  kFocusThickDip  = 1.0f;
    constexpr float  kFocusInsetDip  = 1.0f;
    constexpr float  kPadYDip        = 4.0f;
    constexpr float  kPressedScale   = 0.82f;   // armed-tab tint, a touch darker than hover
    constexpr float  kUnderlineDip   = 3.0f;    // thick selected-tab underline
    constexpr float  kMutedTextScale = 0.62f;   // dim unselected labels



    HRESULT   hr         = S_OK;
    int       i          = 0;
    size_t    n          = m_tabs.size();
    float     focusThick = m_scaler.ToPxf (kFocusThickDip);
    float     focusInset = m_scaler.ToPxf (kFocusInsetDip);
    float     padX       = m_scaler.ToPxf (kLabelPadXDip);
    float     padY       = m_scaler.ToPxf (kPadYDip);
    float     fontDip    = m_scaler.ToPxf (kLabelFontDip);
    float     underline  = m_scaler.ToPxf (kUnderlineDip);
    float     corner     = m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip);
    uint32_t  mutedText  = DxuiColor::Scale (textArgb, kMutedTextScale);



    for (i = 0; i < (int) n; ++i)
    {
        const Tab  & t       = m_tabs[(size_t) i];
        RECT         r       = GetTabScreenRect (i);
        float        width   = (float) (r.right  - r.left);
        float        height  = (float) (r.bottom - r.top);
        bool         isSel   = (i == m_selected);
        bool         isHover = (i == m_hover);
        bool         isArmed = (i == m_pressed && i == m_hover);
        float        labelX  = (float) r.left + padX;
        float        labelW  = width - padX * 2.0f;

        if (!isSel && (isHover || isArmed))
        {
            painter.FillRoundedRect ((float) r.left, (float) r.top, width, height, corner,
                                     isArmed ? DxuiColor::Darken (hoverArgb, kPressedScale) : hoverArgb);
        }

        if (isSel)
        {
            painter.FillRect ((float) r.left, (float) r.bottom - underline, width, underline, underlineArgb);
        }

        if (m_focused && m_focusCueVisible && isSel)
        {
            painter.OutlineRoundedRect ((float) r.left + focusInset, (float) r.top + focusInset,
                                        width - focusInset * 2.0f, height - focusInset * 2.0f,
                                        corner, focusThick, focusArgb);
        }

        if (HasTabIcon (i))
        {
            PaintTabIcon (text, i);

            labelX = (float) r.left + m_scaler.ToPxf ((float) s_kLabelInsetDip);
            labelW = (std::max) ((float) r.right - padX - labelX, 0.0f);
        }

        if (m_close)
        {
            PaintTabClose (painter, text, i, isSel ? textArgb : mutedText, textArgb, corner);

            labelW = (std::max) ((float) GetCloseRect (i).left - labelX, 0.0f);
        }

        hr = text.DrawString (t.label.c_str(),
                              labelX,
                              (float) r.top + padY,
                              labelW,
                              height - padY * 2.0f,
                              isSel ? textArgb : mutedText,
                              fontDip,
                              GetLabelFace(),
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (GetNewTabWidthPx() > 0)
    {
        PaintNewTab (painter, text, hoverArgb, textArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintExplorer
//
//  The Explorer style, with caller-supplied colors so the themed and
//  fallback Paint entry points share one body.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintExplorer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    uint32_t             stripArgb,
    uint32_t             hoverArgb,
    uint32_t             fillArgb,
    uint32_t             dividerArgb,
    uint32_t             textArgb,
    uint32_t             focusArgb) const
{
    constexpr float  s_kFocusThickDip  = 1.0f;
    constexpr float  s_kFocusInsetDip  = 1.0f;
    constexpr float  s_kDividerDip     = 16.0f;
    constexpr float  s_kMutedTextScale = 0.8f;    // Explorer's #CCCCCC on an unselected tab
    constexpr float  s_kBaseLineDip    = 1.0f;    // Explorer's 2 px at 150%
    constexpr float  s_kBaseLineScale  = 0.9f;    // Explorer's #1D1D1D under #202020



    HRESULT   hr         = S_OK;
    int       i          = 0;
    size_t    n          = m_tabs.size();
    float     focusThick = m_scaler.ToPxf (s_kFocusThickDip);
    float     focusInset = m_scaler.ToPxf (s_kFocusInsetDip);
    float     padX       = m_scaler.ToPxf (kLabelPadXDip);
    float     fontDip    = m_scaler.ToPxf (kLabelFontDip);
    float     corner     = m_scaler.ToPxf ((float) s_kCornerDip);
    float     dividerH   = m_scaler.ToPxf (s_kDividerDip);
    float     baseLine   = (std::max) (1.0f, std::round (m_scaler.ToPxf (s_kBaseLineDip)));
    uint32_t  mutedText  = DxuiColor::Scale (textArgb, s_kMutedTextScale);



    //  File Explorer's tabs (research R13): the selected one is filled with the
    //  row below and joins it, rounded at the top and square at the bottom;
    //  the rest are unfilled, split by short dividers, with dimmer labels.
    if (HasBounds())
    {
        hr = text.PushClipRect ((float) GetViewLeft(), (float) m_boundsDip.top,
                                (float) (GetViewRight() - GetViewLeft()), (float) (m_boundsDip.bottom - m_boundsDip.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  Explorer's strip ends in a darker line the selected tab breaks, and
    //  which a hovered tab stops short of.
    //  It is black laid over the strip, which darkens an opaque strip by the
    //  same amount and darkens a backdrop showing through an empty one too,
    //  and it stops at the selected tab, whose translucent fill would show it.
    if (HasBounds())
    {
        float     lineTop   = (float) m_boundsDip.bottom - baseLine;
        float     viewLeft  = (float) GetViewLeft();
        float     viewRight = (float) GetViewRight();
        uint32_t  lineArgb  = (uint32_t) std::lround ((1.0f - s_kBaseLineScale) * 255.0f) << 24;

        if (m_selected >= 0 && m_selected < (int) n)
        {
            RECT  sel = GetTabScreenRect (m_selected);

            painter.FillRect (viewLeft,          lineTop, (std::max) (0.0f, (float) sel.left - viewLeft),   baseLine, lineArgb);
            painter.FillRect ((float) sel.right, lineTop, (std::max) (0.0f, viewRight - (float) sel.right), baseLine, lineArgb);
        }
        else
        {
            painter.FillRect (viewLeft, lineTop, viewRight - viewLeft, baseLine, lineArgb);
        }
    }

    //  Fills first, so no fill covers another tab's label or close button;
    //  the selected tab last.
    for (i = 0; i < (int) n; ++i)
    {
        RECT  r = GetTabScreenRect (i);

        //  Explorer's hovered tab: rounded at the top, square where it meets
        //  the row below, with a 1 px edge halfway between it and the strip
        //  along its top and sides.
        if (i != m_selected && i == m_hover)
        {
            float     left   = (float) r.left;
            float     top    = (float) r.top;
            float     width  = (float) (r.right - r.left);
            float     height = (float) (r.bottom - r.top) - baseLine;
            uint32_t  fill   = GetBesideFill (i, stripArgb, fillArgb);
            uint32_t  edge   = DxuiColor::Mix (stripArgb, fill, 0.5f);

            painter.FillRoundedRect (left, top, width, height, corner, edge);
            painter.FillRect        (left, top + height - corner, width, corner, edge);
            painter.FillRoundedRect (left + 1.0f, top + 1.0f, width - 2.0f, height - 1.0f, (std::max) (corner - 1.0f, 0.0f), fill);
            painter.FillRect        (left + 1.0f, top + height - corner, width - 2.0f, corner, fill);
        }
    }

    if (m_selected >= 0 && m_selected < (int) n)
    {
        RECT   r      = GetTabScreenRect (m_selected);
        float  left   = (float) r.left;
        float  top    = (float) r.top;
        float  width  = (float) (r.right - r.left);
        float  height = (float) (r.bottom - r.top);
        float  bottom = top + height;

        //  The rounded top is clipped short of the square bottom, so a
        //  translucent fill is laid down once everywhere.
        painter.PushClipRect    (left, top, width, height - corner);
        painter.FillRoundedRect (left, top, width, height, corner, fillArgb);
        painter.PopClipRect();
        painter.FillRect        (left, bottom - corner, width, corner, fillArgb);
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

        if (!isSel && !isHover && !isArmed && !nextLit && i + 1 < (int) n)
        {
            painter.FillRect (left + width - 1.0f, top + (height - dividerH) * 0.5f, 1.0f, dividerH, dividerArgb);
        }

        if (m_focused && m_focusCueVisible && isSel)
        {
            painter.OutlineRoundedRect (left + focusInset, top + focusInset,
                                        width - focusInset * 2.0f, height - focusInset * 2.0f,
                                        corner, focusThick, focusArgb);
        }

        PaintTabIcon (text, i);

        if (m_close)
        {
            PaintTabClose (painter, text, i, isSel ? textArgb : mutedText, textArgb, corner);

            labelR = (float) GetCloseRect (i).left;
        }

        //  A label wider than its room is cut off with an ellipsis, never wrapped.
        shown = DxuiTextElide::ToWidth (text, t.label, fontDip, GetLabelFace(), (std::max) (labelR - labelX, 0.0f), DxuiElide::Tail);

        hr = text.DrawString (shown.c_str(),
                              labelX, top, (std::max) (labelR - labelX, 0.0f), height,
                              isSel ? textArgb : mutedText,
                              fontDip,
                              GetLabelFace(),
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              isSel ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
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
    if (m_style == DxuiTabStripStyle::Explorer)
    {
        PaintExplorerThemed (painter, text, theme);
    }
    else
    {
        //  The selection color marks the selected tab's underline and hover is
        //  a subtle fill hint; unselected tabs blend with the page.
        PaintStandard (painter, text,
                       theme.HoverBackground(),
                       theme.SelectionBackground(),
                       theme.Foreground(),
                       theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip::PaintExplorerThemed
//
//  The Explorer style in the theme's colors, with the host's strip and
//  selected fills where it gives them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStrip::PaintExplorerThemed (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    uint32_t  fill  = (m_selectedFill != 0) ? m_selectedFill : theme.BackgroundElevated();
    uint32_t  strip = (m_stripFill    != 0) ? m_stripFill    : theme.Background();
    uint32_t  hover = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;



    //  The strip's own fill, when the host gives one, goes down first.
    if (m_stripFill != 0 && HasBounds())
    {
        painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                          (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top), m_stripFill);
    }

    //  The selected tab takes the color of the row it joins, which the host
    //  gives; without one it takes the elevated surface. A hovered tab is a
    //  faint wash of the text color, which reads in either theme, as Explorer's
    //  gray does, where the theme's hover is a saturated selection color.
    PaintExplorer (painter, text,
                   strip,
                   hover,
                   fill,
                   theme.Divider(),
                   theme.Foreground(),
                   theme.FocusRing());

    //  Explorer's light theme lines the strip's bottom, except under the
    //  selected tab, which joins the row below.
    if (theme.TabStripEdge() != 0 && HasBounds())
    {
        float  line   = (std::max) (1.0f, m_scaler.ToPxf (1.0f));
        float  bottom = (float) m_boundsDip.bottom - line;
        float  left   = (float) m_boundsDip.left;
        float  right  = (float) m_boundsDip.right;
        RECT   tab    = (m_selected >= 0 && m_selected < (int) m_tabs.size()) ? GetTabScreenRect (m_selected) : RECT {};

        if (tab.right > tab.left)
        {
            painter.FillRect (left, bottom, (std::max) (0.0f, (float) tab.left - left), line, theme.TabStripEdge());
            painter.FillRect ((float) tab.right, bottom, (std::max) (0.0f, right - (float) tab.right), line, theme.TabStripEdge());
        }
        else
        {
            painter.FillRect (left, bottom, right - left, line, theme.TabStripEdge());
        }
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
