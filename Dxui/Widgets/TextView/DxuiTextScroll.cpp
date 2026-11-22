#include "Pch.h"

#include "Widgets/TextView/DxuiTextScroll.h"
#include "Widgets/TextView/DxuiTextLayout.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiEvents.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::GetLineCap
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextScroll::GetLineCap() const
{
    const RECT &  bounds = m_geometry.GetViewBounds();
    int           height = (int) (bounds.bottom - bounds.top) - m_geometry.GetPadPx() * 2 - (m_horzVisible ? m_geometry.GetBarWidthPx() : 0);
    int           lineH  = m_layout.GetCellHeightPx();



    return (lineH > 0) ? (std::max) (1, height / lineH) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::IsScrollbarVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::IsScrollbarVisible() const
{
    return m_layout.GetLineCount() > GetLineCap();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::SetTopLine
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextScroll::SetTopLine (int line)
{
    int  maxTop = (std::max) (0, m_layout.GetLineCount() - GetLineCap());



    m_topLine = (std::max) (0, (std::min) (line, maxTop));

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::SetLeftColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextScroll::SetLeftColumn (int column)
{
    int  maxLeft = m_horzVisible ? (std::max) (0, m_layout.GetContentColumns() - m_geometry.GetTextColumns (IsScrollbarVisible())) : 0;



    m_leftColumn = (std::max) (0, (std::min) (column, maxLeft));

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextScroll::SyncScrollbar()
{
    const RECT &  bounds = m_geometry.GetViewBounds();
    int           barW   = m_geometry.GetBarWidthPx();
    bool          vert   = IsScrollbarVisible();
    SCROLLINFO    info   = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { bounds.right - barW, bounds.top, bounds.right, bounds.bottom - (m_horzVisible ? barW : 0) });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (std::max) (0, m_layout.GetLineCount() - 1);
    info.nPage = (UINT) GetLineCap();
    info.nPos  = m_topLine;
    m_vertScroll.SetScrollInfo (info);

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, 1);
    m_horzScroll.SetTrack (RECT { bounds.left, bounds.bottom - barW, bounds.right - (vert ? barW : 0), bounds.bottom });

    info.nMax  = (std::max) (0, m_layout.GetContentColumns() - 1);
    info.nPage = (UINT) m_geometry.GetTextColumns (vert);
    info.nPos  = m_leftColumn;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::SetScrollbarHover
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::SetScrollbarHover (POINT pt)
{
    bool  vert = m_vertScroll.SetHover (IsOverVertScrollbar (pt), pt);
    bool  horz = m_horzScroll.SetHover (IsOverHorzScrollbar (pt), pt);



    return vert || horz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::TickScrollbars (int64_t nowMs)
{
    bool  vert = m_vertScroll.Tick (nowMs);
    bool  horz = m_horzScroll.Tick (nowMs);



    return vert || horz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::GetScrollbarMenu
//
//  Windows' scrollbar menu when the point is on one of the bars, in the
//  coordinates the view's OnMouse takes; empty elsewhere. Its rows scroll
//  the view.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiTextScroll::GetScrollbarMenu (POINT pt)
{
    m_vertScroll.SetOnScroll ([this] (int, int pos) { SetTopLine (pos); });
    m_horzScroll.SetOnScroll ([this] (int, int pos) { SetLeftColumn (pos); });

    if (!IsOverScrollbar (pt))
    {
        return {};
    }

    SyncScrollbar();

    return IsOverVertScrollbar (pt) ? m_vertScroll.BuildContextMenu (pt.x, pt.y) : m_horzScroll.BuildContextMenu (pt.x, pt.y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextScroll::Paint (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    if (IsScrollbarVisible() && !m_vertBarHidden)
    {
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }

    if (m_horzVisible)
    {
        m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::OnWheel
//
//  A horizontal wheel, or Shift with the wheel, scrolls sideways. A
//  precision touchpad sends part notches, which add up until they move a
//  whole line or column.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::OnWheel (const DxuiMouseEvent & ev)
{
    if (m_horzVisible && (ev.wheelHorizontal || ev.shift))
    {
        m_wheelColumns += (ev.wheelHorizontal ? ev.wheelDelta : -ev.wheelDelta) * (float) kWheelColumns;
        ScrollColumns ((int) m_wheelColumns);
        m_wheelColumns -= (float) (int) m_wheelColumns;

        return true;
    }

    m_wheelLines += -ev.wheelDelta * (float) kWheelLines;
    ScrollLines ((int) m_wheelLines);
    m_wheelLines -= (float) (int) m_wheelLines;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::TryStartDrag
//
//  A left press on either bar. Returns whether the press is on one, and
//  sets outHandled to whether the bar took it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::TryStartDrag (POINT pt, bool & outHandled)
{
    if (IsOverVertScrollbar (pt))
    {
        outHandled = m_vertScroll.OnMouseDown (pt.x, pt.y);
        SetTopLine (m_vertScroll.GetScrollPos());

        return true;
    }

    if (IsOverHorzScrollbar (pt))
    {
        outHandled = m_horzScroll.OnMouseDown (pt.x, pt.y);
        SetLeftColumn (m_horzScroll.GetScrollPos());

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::TryDrag
//
//  A move while a bar's thumb is dragged. Returns whether one is.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::TryDrag (POINT pt, bool & outHandled)
{
    if (m_vertScroll.IsDragging())
    {
        outHandled = m_vertScroll.OnMouseMove (pt.x, pt.y);
        SetTopLine (m_vertScroll.GetScrollPos());

        return true;
    }

    if (m_horzScroll.IsDragging())
    {
        outHandled = m_horzScroll.OnMouseMove (pt.x, pt.y);
        SetLeftColumn (m_horzScroll.GetScrollPos());

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::TryEndDrag
//
//  The release of a bar's thumb. Returns whether one was being dragged.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::TryEndDrag (bool & outHandled)
{
    if (m_vertScroll.IsDragging())
    {
        outHandled = m_vertScroll.OnMouseUp();

        return true;
    }

    if (m_horzScroll.IsDragging())
    {
        outHandled = m_horzScroll.OnMouseUp();

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll::OnKey
//
//  The arrow and page keys scroll; there is no caret to move in read-only text.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextScroll::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = ev.kind == DxuiKeyEventKind::Down;



    if (!handled)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_UP:    ScrollLines (-1);                        break;
    case VK_DOWN:  ScrollLines (1);                         break;
    case VK_PRIOR: ScrollLines (-GetLineCap());             break;
    case VK_NEXT:  ScrollLines (GetLineCap());              break;
    case VK_HOME:  SetTopLine (0);                          break;
    case VK_END:   SetTopLine (m_layout.GetLineCount());    break;
    case VK_LEFT:  ScrollColumns (-1);                      handled = m_horzVisible; break;
    case VK_RIGHT: ScrollColumns (1);                       handled = m_horzVisible; break;
    default:       handled = false;                         break;
    }

    return handled;
}
