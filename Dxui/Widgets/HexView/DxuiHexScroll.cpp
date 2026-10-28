#include "Pch.h"

#include "Widgets/HexView/DxuiHexScroll.h"
#include "Widgets/DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::DxuiHexScroll
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexScroll::DxuiHexScroll (DxuiHexView & view)
    : m_view (view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::SetLeftPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::SetLeftPx (int leftPx)
{
    m_leftPx = (std::max) (0, (std::min) (leftPx, m_view.m_layout.GetContentWidthPx() - m_view.m_layout.GetViewWidthPx()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::KeepCaretInView
//
//  Scrolls sideways the least that shows the caret's cell in the active
//  column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::KeepCaretInView()
{
    int   index = m_view.m_layout.GetColumnOfOffset (m_view.m_selection.GetCaret());
    bool  text  = (m_view.m_selection.GetActiveColumn() == DxuiHexViewTypes::Column::Text) || !m_view.m_showValues;
    int   cell  = text ? (m_view.m_layout.GetColumnStartCell (DxuiHexViewTypes::Column::Text) + index) : (m_view.m_layout.GetColumnStartCell (DxuiHexViewTypes::Column::Hex) + m_view.m_layout.GetByteCellInRow (index));
    int   wide  = text ? 1 : ((m_view.m_format == DxuiHexViewTypes::ValueFormat::Hex) ? 2 : m_view.m_layout.GetValueCells());
    int   left  = m_view.m_scaler.ToPx (m_view.m_layout.GetPadDip()) + (cell * m_view.m_cellWidthDip);
    int   right = left + (wide * m_view.m_cellWidthDip);
    int   view  = m_view.m_layout.GetViewWidthPx();



    if (left < m_leftPx)
    {
        SetLeftPx (left - m_view.m_scaler.ToPx (m_view.m_layout.GetPadDip()));
    }
    else if (right > (m_leftPx + view))
    {
        SetLeftPx (right - view);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::SetTopRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::SetTopRow (uint64_t row)
{
    m_topRow = (std::min) (row, m_view.m_layout.GetMaxTopRow());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::ScrollRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::ScrollRows (int64_t delta)
{
    uint64_t  top = m_topRow;



    if (delta < 0)
    {
        uint64_t  back = (uint64_t) (-delta);

        top = (back > top) ? 0 : (top - back);
    }
    else
    {
        top += (uint64_t) delta;
    }

    SetTopRow (top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::EnsureByteVisible
//
//  Scrolls the least that brings a byte's row into view, so a caret walked
//  off either end moves the view by one row rather than recentering it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::EnsureByteVisible (uint64_t offset)
{
    uint64_t  row = m_view.m_layout.GetRowOfOffset (offset);
    int       cap = m_view.m_layout.GetRowCap();



    if (cap <= 0)
    {
        return;
    }

    if (offset == m_view.m_selection.GetCaret())
    {
        KeepCaretInView();
    }

    if (row < m_topRow)
    {
        SetTopRow (row);
    }
    else if (row >= (m_topRow + (uint64_t) cap))
    {
        SetTopRow (row - (uint64_t) cap + 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::SyncScrollbar()
{
    int         barW = m_view.m_scaler.ToPx (DxuiHexView::s_kScrollbarWidthDip);
    SCROLLINFO  info = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { m_view.m_boundsDip.right - barW, m_view.m_boundsDip.top, m_view.m_boundsDip.right, m_view.m_boundsDip.top + m_view.m_layout.GetHeightDip() });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (int) (std::min) (m_view.m_layout.GetRowCount(), (uint64_t) INT_MAX) - 1;
    info.nPage = (UINT) (std::max) (m_view.m_layout.GetRowCap(), 0);
    info.nPos  = (int) (std::min) (m_topRow, (uint64_t) INT_MAX);
    m_vertScroll.SetScrollInfo (info);

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, (std::max) (m_view.m_cellWidthDip, 1));
    m_horzScroll.SetTrack (RECT { m_view.m_boundsDip.left, m_view.m_boundsDip.bottom - barW, m_view.m_boundsDip.left + m_view.m_layout.GetViewWidthPx(), m_view.m_boundsDip.bottom });

    info.nMax  = (std::max) (m_view.m_layout.GetContentWidthPx() - 1, 0);
    info.nPage = (UINT) m_view.m_layout.GetViewWidthPx();
    info.nPos  = m_leftPx;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::ScrollToBarPos
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexScroll::ScrollToBarPos()
{
    SetTopRow ((uint64_t) (std::max) (m_vertScroll.GetScrollPos(), 0));
    SetLeftPx (m_horzScroll.GetScrollPos());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiHexScroll::GetScrollbarMenu (POINT pt)
{
    SyncScrollbar();

    m_vertScroll.SetOnScroll ([this] (int, int) { ScrollToBarPos(); });
    m_horzScroll.SetOnScroll ([this] (int, int) { ScrollToBarPos(); });

    if (m_view.IsScrollbarVisible() && m_vertScroll.HitTest (pt.x, pt.y))
    {
        return m_vertScroll.BuildContextMenu (pt.x, pt.y);
    }

    if (m_view.IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y))
    {
        return m_horzScroll.BuildContextMenu (pt.x, pt.y);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::OnPress
//
//  A left press on either scrollbar steps, pages or grabs the thumb.
//  Returns whether the press was on one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexScroll::OnPress (const DxuiMouseEvent & ev)
{
    DxuiScrollbar  * bar = nullptr;



    if (ev.button != DxuiMouseButton::Left)
    {
        return false;
    }

    if (m_view.IsScrollbarVisible() && m_vertScroll.HitTest (ev.positionDip.x, ev.positionDip.y))
    {
        bar = &m_vertScroll;
    }
    else if (m_view.IsHorzScrollbarVisible() && m_horzScroll.HitTest (ev.positionDip.x, ev.positionDip.y))
    {
        bar = &m_horzScroll;
    }

    if (bar == nullptr)
    {
        return false;
    }

    SyncScrollbar();
    bar->OnMouseDown (ev.positionDip.x, ev.positionDip.y);
    ScrollToBarPos();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::OnDrag
//
//  A thumb being dragged follows the pointer. Returns whether one was.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexScroll::OnDrag (const DxuiMouseEvent & ev)
{
    if (!IsDragging())
    {
        return false;
    }

    m_vertScroll.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
    m_horzScroll.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
    ScrollToBarPos();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::TryRelease
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexScroll::TryRelease (bool & outHandled)
{
    if (m_horzScroll.IsDragging())
    {
        outHandled = m_horzScroll.OnMouseUp();
        return true;
    }

    if (m_vertScroll.IsDragging())
    {
        outHandled = m_vertScroll.OnMouseUp();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll::OnWheel
//
//  A precision touchpad sends part notches, which add up until they move a
//  whole row or pixel.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexScroll::OnWheel (const DxuiMouseEvent & ev)
{
    if (ev.wheelHorizontal)
    {
        m_wheelPx += ev.wheelDelta * (float) (DxuiHexView::kWheelRows * m_view.m_cellWidthDip);
        SetLeftPx (m_leftPx + (int) m_wheelPx);
        m_wheelPx -= (float) (int) m_wheelPx;
        return m_view.IsHorzScrollbarVisible();
    }

    m_wheelRows += ev.wheelDelta * (float) DxuiHexView::kWheelRows;
    ScrollRows (-(int64_t) m_wheelRows);
    m_wheelRows -= (float) (int64_t) m_wheelRows;

    return true;
}
