#include "Pch.h"

#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemMetrics
//
//  Every view but Details lays its entries out as items of one size. The big
//  icon views put the name under the icon; the rest put it beside. List runs
//  its items down columns and scrolls sideways; the others run them along
//  rows and scroll down.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ItemMetrics DxuiListView::GetItemMetrics (View view)
{
    switch (view)
    {
        case View::ExtraLargeIcons: return ItemMetrics { 280, 312, 256, false, true,  2 };
        case View::LargeIcons:      return ItemMetrics { 128, 146,  96, false, true,  2 };
        case View::MediumIcons:     return ItemMetrics {  88,  98,  48, false, true,  2 };
        case View::SmallIcons:      return ItemMetrics { 240,  24,  16, false, false, 1 };
        case View::List:            return ItemMetrics { 240,  24,  16, true,  false, 1 };
        case View::Tiles:           return ItemMetrics { 254,  58,  48, false, false, 3, 17 };
        case View::Content:         return ItemMetrics {   0,  56,  32, false, false, 2 };
        default:                    return ItemMetrics {   0,  30,  16, false, false, 1 };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::SetView
//
//  The header belongs to Details alone, so it is put away while another view
//  shows and comes back with Details. The view opens at its start, with the
//  selected entry scrolled into sight.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetView (View view)
{
    if (view == m_view)
    {
        return;
    }

    if (m_view == View::Details)
    {
        m_detailsHeader = m_showHeader;
        m_showHeader    = false;
    }
    else if (view == View::Details)
    {
        m_showHeader = m_detailsHeader;
    }

    m_view   = view;
    m_topRow = 0;
    m_leftPx = 0;

    EnsureVisible (m_selectedRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemGrid
//
//  How the items of the current view fit the list: the cell in pixels, how
//  many share a line (a row, or a column for List), and how many lines show.
//  The vertical bar's width is always kept back, so the count per row does not
//  change as the bar comes and goes.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ItemGrid DxuiListView::GetItemGrid() const
{
    ItemGrid     grid;
    ItemMetrics  metrics = GetItemMetrics (m_view);
    int          fullW   = m_boundsDip.right  - m_boundsDip.left;
    int          fullH   = m_boundsDip.bottom - m_boundsDip.top;
    int          barW    = GetScrollbarWidthPx();
    int          rows    = GetRowCount();



    grid.cellH = m_scaler.ToPx (metrics.cellHDip);

    if (metrics.columns)
    {
        grid.cellW   = m_scaler.ToPx (metrics.cellWDip);
        grid.perLine = (grid.cellH > 0) ? (std::max) (1, (fullH - barW) / grid.cellH) : 1;
        grid.lines   = (rows + grid.perLine - 1) / grid.perLine;
        grid.visible = (grid.cellW > 0) ? (std::max) (1, fullW / grid.cellW) : 1;

        return grid;
    }

    grid.cellW   = (metrics.cellWDip > 0) ? m_scaler.ToPx (metrics.cellWDip) : (std::max) (1, fullW - barW);
    grid.perLine = (grid.cellW > 0) ? (std::max) (1, (fullW - barW) / grid.cellW) : 1;
    grid.lines   = (rows + grid.perLine - 1) / grid.perLine;
    grid.visible = (grid.cellH > 0) ? (std::max) (1, fullH / grid.cellH) : 1;

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::ComputeItemScrollLayout
//
//  List scrolls sideways through its columns and never down; every other
//  item view scrolls down through its rows and never sideways. The row
//  capacity is counted in items, so the scrollbar and paging arithmetic that
//  Details uses reads the same way here.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ScrollLayout DxuiListView::ComputeItemScrollLayout() const
{
    ScrollLayout  layout;
    ItemGrid      grid  = GetItemGrid();
    int           fullW = m_boundsDip.right - m_boundsDip.left;
    int           barW  = GetScrollbarWidthPx();



    if (GetItemMetrics (m_view).columns)
    {
        layout.contentW  = grid.lines * grid.cellW;
        layout.hBar      = layout.contentW > fullW;
        layout.vBar      = false;
        layout.rowCap    = GetRowCount();
        layout.viewportW = fullW;

        return layout;
    }

    layout.vBar      = grid.lines > grid.visible;
    layout.hBar      = false;
    layout.rowCap    = grid.visible * grid.perLine;
    layout.viewportW = fullW - (layout.vBar ? barW : 0);
    layout.contentW  = layout.viewportW;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemRectPx
//
//  An item's cell relative to the list's top-left, wherever it is scrolled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::GetItemRectPx (int item, RECT & outRect) const
{
    ItemGrid  grid = GetItemGrid();
    int       line = 0;
    int       slot = 0;



    if (m_view == View::Details || item < 0 || item >= GetRowCount() || grid.perLine <= 0)
    {
        return false;
    }

    line = item / grid.perLine;
    slot = item % grid.perLine;

    if (GetItemMetrics (m_view).columns)
    {
        outRect.left = line * grid.cellW - m_leftPx;
        outRect.top  = slot * grid.cellH;
    }
    else
    {
        outRect.left = slot * grid.cellW;
        outRect.top  = (line - m_topRow / grid.perLine) * grid.cellH;
    }

    outRect.right  = outRect.left + grid.cellW;
    outRect.bottom = outRect.top  + grid.cellH;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HitTestItem
//
//  The item whose cell holds a point relative to the list, or -1 between
//  items, past the last one, or over a scrollbar.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestItem (int xPx, int yPx) const
{
    ItemGrid      grid   = GetItemGrid();
    ScrollLayout  layout = ComputeItemScrollLayout();
    int           fullH  = m_boundsDip.bottom - m_boundsDip.top;
    int           barW   = GetScrollbarWidthPx();
    int           item   = -1;



    if (xPx < 0 || yPx < 0 || xPx >= layout.viewportW || grid.cellW <= 0 || grid.cellH <= 0)
    {
        return -1;
    }

    if (GetItemMetrics (m_view).columns)
    {
        if (layout.hBar && yPx >= fullH - barW)
        {
            return -1;
        }

        if (yPx / grid.cellH >= grid.perLine)
        {
            return -1;
        }

        item = ((xPx + m_leftPx) / grid.cellW) * grid.perLine + yPx / grid.cellH;
    }
    else
    {
        if (xPx / grid.cellW >= grid.perLine)
        {
            return -1;
        }

        item = (m_topRow / grid.perLine + yPx / grid.cellH) * grid.perLine + xPx / grid.cellW;
    }

    return (item < GetRowCount()) ? item : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::EnsureItemVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::EnsureItemVisible (int item)
{
    ItemGrid  grid = GetItemGrid();
    int       line = 0;
    int       top  = 0;



    if (item < 0 || item >= GetRowCount() || grid.perLine <= 0)
    {
        return;
    }

    line = item / grid.perLine;

    if (GetItemMetrics (m_view).columns)
    {
        int  viewW = m_boundsDip.right - m_boundsDip.left;
        int  left  = line * grid.cellW;

        if (left < m_leftPx)
        {
            SetLeftPx (left);
        }
        else if (left + grid.cellW > m_leftPx + viewW)
        {
            SetLeftPx (left + grid.cellW - viewW);
        }

        return;
    }

    top = m_topRow / grid.perLine;

    if (line < top)
    {
        SetTopRow (line * grid.perLine);
    }
    else if (line >= top + grid.visible)
    {
        SetTopRow ((line - grid.visible + 1) * grid.perLine);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HandleKeyboardItemNav
//
//  Arrows move through the grid as it lies on screen: along a row with Left
//  and Right and between rows with Up and Down, or down a column and across
//  columns in List. Page keys move a screenful.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardItemNav (WPARAM vk, bool shift)
{
    ItemGrid  grid    = GetItemGrid();
    bool      columns = GetItemMetrics (m_view).columns;
    int       rows    = GetRowCount();
    int       cur     = (std::max) (0, GetSelectedRow());
    int       across  = columns ? grid.perLine : 1;
    int       down    = columns ? 1 : grid.perLine;
    int       page    = (std::max) (1, grid.visible * (columns ? grid.perLine : grid.perLine));
    int       next    = cur;
    bool      moved   = rows > 0;



    switch (vk)
    {
        case VK_LEFT:  next = cur - across; break;
        case VK_RIGHT: next = cur + across; break;
        case VK_UP:    next = cur - down;   break;
        case VK_DOWN:  next = cur + down;   break;
        case VK_HOME:  next = 0;            break;
        case VK_END:   next = rows - 1;     break;
        case VK_PRIOR: next = cur - page;   break;
        case VK_NEXT:  next = cur + page;   break;
        default:       moved = false;       break;
    }

    if (!moved)
    {
        return false;
    }

    //  A move off the grid's edge stays where it is, as Explorer's does,
    //  rather than wrapping to the far side.
    if (next < 0 || next >= rows)
    {
        next = (vk == VK_PRIOR || vk == VK_HOME) ? 0 : (vk == VK_NEXT || vk == VK_END) ? rows - 1 : cur;
    }

    if (m_multiSelect && shift)
    {
        SelectRangeFromAnchor (next);
    }
    else
    {
        SetSelectedRow (next);
    }

    EnsureItemVisible (next);

    if (m_multiSelect && m_onSelectionChanged)
    {
        m_onSelectionChanged (next);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemLabelRectPx
//
//  Where an item's name is drawn in its cell: under the icon for the big icon
//  views, beside it otherwise.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListView::GetItemLabelRectPx (const RECT & cell) const
{
    ItemMetrics  metrics = GetItemMetrics (m_view);
    int          pad     = m_scaler.ToPx (s_kItemPadDip);
    int          iconPx  = m_scaler.ToPx (metrics.iconDip);
    int          lineH   = m_scaler.ToPx (metrics.lineDip);



    if (metrics.labelBelow)
    {
        return RECT { cell.left + pad, cell.top + pad + iconPx + pad, cell.right - pad, cell.top + pad + iconPx + pad + lineH * metrics.textLines };
    }

    return RECT { cell.left + pad + iconPx + pad, cell.top + ((cell.bottom - cell.top) - lineH * metrics.textLines) / 2,
                  cell.right - pad, cell.top + ((cell.bottom - cell.top) - lineH * metrics.textLines) / 2 + lineH };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::PaintItems
//
//  Each item in sight: the selection or hover card, the icon at the view's
//  size, the name, and for Tiles and Content the other columns in the muted
//  color on the lines below.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintItems (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal, float x, float y) const
{
    HRESULT            hr      = S_OK;
    ItemMetrics        metrics = GetItemMetrics (m_view);
    ItemGrid           grid    = GetItemGrid();
    int                rows    = GetRowCount();
    int                first   = 0;
    int                last    = rows;
    int                pad     = m_scaler.ToPx (s_kItemPadDip);
    int                iconPx  = m_scaler.ToPx (metrics.iconDip);
    int                lineH   = m_scaler.ToPx (metrics.lineDip);
    float              fontPx  = m_scaler.ToPxf (m_fontDip);
    float              radius  = m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip);



    if (metrics.columns)
    {
        first = (grid.cellW > 0) ? (m_leftPx / grid.cellW) * grid.perLine : 0;
        last  = (std::min) (rows, first + (grid.visible + 1) * grid.perLine);
    }
    else
    {
        first = m_topRow;
        last  = (std::min) (rows, first + (grid.visible + 1) * grid.perLine);
    }

    for (int item = first; item < last; item++)
    {
        RECT                        cell   = {};
        RECT                        label  = {};
        const std::vector<Cell> &   cells  = GetRowCells (item);
        bool                        isSel  = (m_listFocused || m_alwaysShowSelection) &&
                                             (m_multiSelect ? IsRowSelected (item) : item == m_selectedRow);
        float                       iconX  = 0.0f;
        float                       iconY  = 0.0f;

        if (!GetItemRectPx (item, cell) || cells.empty())
        {
            continue;
        }

        OffsetRect (&cell, (int) x, (int) y);

        if (isSel || item == m_hovered)
        {
            painter.FillRoundedRect ((float) cell.left, (float) cell.top, (float) (cell.right - cell.left), (float) (cell.bottom - cell.top),
                                     radius, isSel ? pal.bgSel : pal.bgHover);
        }

        if (metrics.labelBelow)
        {
            iconX = (float) cell.left + ((float) (cell.right - cell.left) - (float) iconPx) * 0.5f;
            iconY = (float) (cell.top + pad);
        }
        else
        {
            iconX = (float) (cell.left + pad);
            iconY = (float) cell.top + ((float) (cell.bottom - cell.top) - (float) iconPx) * 0.5f;
        }

        if (cells[0].icon && !cells[0].icon->bgraPremul.empty())
        {
            float  alpha = text.GetGlobalAlpha();

            if (cells[0].iconGhosted)
            {
                text.SetGlobalAlpha (alpha * s_kGhostedIconAlpha);
            }

            hr = text.DrawIconBitmap (cells[0].icon->bgraPremul.data(), cells[0].icon->width, cells[0].icon->height,
                                      iconX, iconY, (float) iconPx, (float) iconPx);
            IGNORE_RETURN_VALUE (hr, S_OK);

            text.SetGlobalAlpha (alpha);
        }

        label = GetItemLabelRectPx (cell);

        hr = text.DrawString (cells[0].text.c_str(), (float) label.left, (float) label.top,
                              (float) (label.right - label.left), (float) (label.bottom - label.top),
                              cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx, DxuiTheme::kBodyFace,
                              metrics.labelBelow ? DxuiTextHAlign::Center : DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                              DxuiFontWeight::Normal, metrics.labelBelow);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  Tiles and Content show the other columns underneath, one a line,
        //  or the row's own tile lines when it has them.
        for (int line = 1; !metrics.labelBelow && line < metrics.textLines; line++)
        {
            bool                        own    = !cells[0].tileLines.empty();
            const std::vector<Cell> &   source = own ? cells[0].tileLines : cells;
            size_t                      index  = own ? (size_t) (line - 1) : (size_t) line;
            float                       top    = (float) (label.top + line * lineH);

            if (index >= source.size())
            {
                break;
            }

            if (source[index].meter >= 0.0f)
            {
                PaintMeter (painter, (float) label.left, top, (float) (label.right - label.left), (float) lineH, source[index].meter);
                continue;
            }

            hr = text.DrawString (source[index].text.c_str(), (float) label.left, top,
                                  (float) (label.right - label.left), (float) lineH,
                                  pal.fgDim, fontPx, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    //  The rubber band, over everything it selects.
    if (m_bandActive)
    {
        RECT  band = GetSelectionBandPx();

        OffsetRect (&band, (int) x, (int) y);

        painter.FillRect    ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                             (pal.bgSel & 0x00FFFFFFu) | 0x60000000u);
        painter.OutlineRect ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                             1.0f, pal.edgeSel != 0 ? pal.edgeSel : pal.fg);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::PaintMeter
//
//  Explorer's usage bar: an edged track, filled from the left to the
//  fraction, centered in its line and no wider than the line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintMeter (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const
{
    float     edge   = (std::max) (1.0f, m_scaler.ToPxf (1.0f));
    float     width  = (std::min) (w, m_scaler.ToPxf ((float) s_kMeterWidthDip));
    float     height = m_scaler.ToPxf ((float) s_kMeterHeightDip);
    float     top    = y + (lineH - height) * 0.5f;
    float     fill   = std::clamp (fraction, 0.0f, 1.0f);
    uint32_t  argb   = (fill >= s_kMeterFullAt) ? s_kMeterFullArgb : s_kMeterFillArgb;



    if (width <= edge * 2.0f)
    {
        return;
    }

    painter.FillRect (x, top, width, height, s_kMeterEdgeArgb);
    painter.FillRect (x + edge, top + edge, width - edge * 2.0f, height - edge * 2.0f, s_kMeterTrackArgb);
    painter.FillRect (x + edge, top + edge, (width - edge * 2.0f) * fill, height - edge * 2.0f, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemScrollOffsetPx
//
//  How far the item view is scrolled, so a point in the list's pixels can be
//  turned into one in the items' own space.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiListView::GetItemScrollOffsetPx() const
{
    ItemGrid  grid = GetItemGrid();



    if (GetItemMetrics (m_view).columns)
    {
        return POINT { m_leftPx, 0 };
    }

    return POINT { 0, (grid.perLine > 0) ? (m_topRow / grid.perLine) * grid.cellH : 0 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::BeginSelectionBand
//
//  Without Ctrl the band starts from nothing; with it, the band adds to the
//  selection the press found.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::BeginSelectionBand (int lx, int ly, bool ctrl)
{
    POINT  offset = GetItemScrollOffsetPx();



    m_bandActive = true;
    m_bandStart  = POINT { lx + offset.x, ly + offset.y };
    m_bandEnd    = m_bandStart;
    m_bandBase   = ctrl ? m_selectedRows : std::vector<int>();

    if (!ctrl)
    {
        ClearSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::UpdateSelectionBand
//
//  Every item the band touches is selected, with what Ctrl kept.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::UpdateSelectionBand (int lx, int ly)
{
    POINT             offset = GetItemScrollOffsetPx();
    RECT              band   = {};
    std::vector<int>  rows   = m_bandBase;
    int               last   = -1;



    m_bandEnd = POINT { lx + offset.x, ly + offset.y };

    band = RECT { (std::min) (m_bandStart.x, m_bandEnd.x), (std::min) (m_bandStart.y, m_bandEnd.y),
                  (std::max) (m_bandStart.x, m_bandEnd.x), (std::max) (m_bandStart.y, m_bandEnd.y) };

    for (int item = 0; item < GetRowCount(); item++)
    {
        RECT  cell = {};
        RECT  hit  = {};

        if (!GetItemRectPx (item, cell))
        {
            continue;
        }

        OffsetRect (&cell, offset.x, offset.y);

        if (IntersectRect (&hit, &cell, &band))
        {
            rows.push_back (item);
            last = item;
        }
    }

    SetSelectedRows (std::move (rows), last);

    if (m_onSelectionChanged && last >= 0)
    {
        m_onSelectionChanged (last);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetSelectionBandPx
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListView::GetSelectionBandPx() const
{
    POINT  offset = GetItemScrollOffsetPx();



    if (!m_bandActive)
    {
        return RECT {};
    }

    return RECT { (std::min) (m_bandStart.x, m_bandEnd.x) - offset.x, (std::min) (m_bandStart.y, m_bandEnd.y) - offset.y,
                  (std::max) (m_bandStart.x, m_bandEnd.x) - offset.x, (std::max) (m_bandStart.y, m_bandEnd.y) - offset.y };
}
