#include "Pch.h"

#include "Widgets/ListView/DxuiListFlowLayout.h"
#include "Core/DxuiClipboard.h"
#include "Theme/DxuiTheme.h"
#include "Core/DxuiTextElide.h"
#include "Theme/DxuiRowLook.h"
#include "Theme/DxuiColor.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiSystemSettings.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::DxuiListFlowLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiListFlowLayout::DxuiListFlowLayout (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::GetListColumnLefts
//
//  Where each of List's columns starts, each as wide as the widest name in
//  it, with one more entry for where the last ends. Uniform columns before
//  the names are measured.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<int> & DxuiListFlowLayout::GetListColumnLefts() const
{
    DxuiListViewTypes::ItemGrid  grid  = m_list.m_items.GetItemGrid();
    int                          rows  = m_list.GetRowCount();
    bool                         sized = m_list.m_itemNameWPx.size() == (size_t) rows && rows > 0 && m_list.m_itemNameKey.rowsVersion == m_list.m_rowsVersion;
    std::array<int, 5>           key   = { grid.perLine, sized ? m_list.m_itemTextVersion : -1, m_list.m_rowsVersion, grid.cellW, (int) m_list.m_scaler.GetDpi() };
    int                          x     = 0;



    if (key == m_listColumnsKey && !m_listColumnLefts.empty())
    {
        return m_listColumnLefts;
    }

    m_listColumnsKey = key;
    x                = m_list.GetItemsLeftPx();
    m_listColumnLefts.assign (1, x);

    for (int first = 0; first < rows; first += grid.perLine)
    {
        int  widest = 0;

        for (int item = first; sized && item < (std::min) (rows, first + grid.perLine); item++)
        {
            widest = (std::max) (widest, m_list.m_itemNameWPx[(size_t) item]);
        }

        x += sized ? m_list.m_items.GetNameCellWPx (widest) : grid.cellW;
        m_listColumnLefts.push_back (x);
    }

    return m_listColumnLefts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::FindListColumn
//
//  List's column at a point in its content's own pixels, or -1 past them.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListFlowLayout::FindListColumn (int x) const
{
    const std::vector<int> & lefts = GetListColumnLefts();
    auto                     after = std::upper_bound (lefts.begin(), lefts.end(), x);



    if (x < 0 || after == lefts.begin() || after == lefts.end())
    {
        return -1;
    }

    return (int) (after - lefts.begin()) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::HasListGroups
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListFlowLayout::HasListGroups() const
{
    return !m_list.m_groups.empty() && m_list.IsItemsView() && DxuiListItemLayout::GetItemMetrics (m_list.m_view).columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::GetListLayout
//
//  Built again only when what it is built from changes.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiListViewTypes::ListLayout & DxuiListFlowLayout::GetListLayout() const
{
    DxuiListViewTypes::LayoutKey  key = m_list.m_items.GetLayoutKey();



    if (!m_listLayoutBuilt || !(key == m_listLayoutKey))
    {
        m_listLayout      = BuildListLayout();
        m_listLayoutKey   = std::move (key);
        m_listLayoutBuilt = true;
    }

    return m_listLayout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::BuildListLayout
//
//  Explorer's grouped List: each group a block of columns side by side, its
//  label across the top and its items under it, indented from the label and
//  running down each column before the next. A collapsed group keeps a
//  column's width for its label.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ListLayout DxuiListFlowLayout::BuildListLayout() const
{
    DxuiListViewTypes::ListLayout  layout;
    DxuiListViewTypes::ItemGrid    grid     = m_list.m_items.GetItemGrid();
    int                            rows     = m_list.GetRowCount();
    int                            fullH    = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int                            x        = 0;
    bool                           measured = m_list.m_itemNameWPx.size() == (size_t) (std::max) (rows, 0) && rows > 0;



    layout.headerH = m_list.GetRowHeightPx();
    layout.indent  = m_list.m_scaler.ToPx (DxuiListView::s_kGroupLabelDip) - (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kListGroupItemsPullDip));
    layout.perCol  = (grid.cellH > 0) ? (std::max) (1, (fullH - m_list.GetScrollbarWidthPx() - layout.headerH) / grid.cellH) : 1;
    layout.blockOf.assign ((size_t) (std::max) (rows, 0), -1);

    for (size_t g = 0; g < m_list.m_groups.size(); g++)
    {
        int                           start = std::clamp (m_list.m_groups[g].firstRow, 0, rows);
        int                           end   = (g + 1 < m_list.m_groups.size()) ? std::clamp (m_list.m_groups[g + 1].firstRow, start, rows) : rows;
        DxuiListViewTypes::ListBlock  block;

        block.group = (int) g;
        block.first = start;
        block.count = m_list.m_groups[g].collapsed ? 0 : end - start;
        block.left  = x;
        block.colLefts.push_back (0);

        for (int k = 0; k < (std::max) (1, block.count); k += layout.perCol)
        {
            int  widest = 0;

            for (int item = start + k; item < start + (std::min) (k + layout.perCol, block.count); item++)
            {
                widest = (std::max) (widest, measured ? m_list.m_itemNameWPx[(size_t) item] : 0);
            }

            block.colLefts.push_back (block.colLefts.back() + (measured ? m_list.m_items.GetNameCellWPx (widest) - (int) std::lround (m_list.m_scaler.ToPxf (s_kListGroupColumnTrimDip)) : grid.cellW));
        }

        block.width = layout.indent + block.colLefts.back();

        for (int item = start; item < start + block.count; item++)
        {
            layout.blockOf[(size_t) item] = (int) layout.blocks.size();
        }

        layout.blocks.push_back (block);
        x += block.width;
    }

    layout.totalW = x;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::GetListItemRectPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListFlowLayout::GetListItemRectPx (const DxuiListViewTypes::ListLayout & layout, int item, RECT & outRect) const
{
    DxuiListViewTypes::ItemGrid  grid = m_list.m_items.GetItemGrid();
    int                          at   = (item >= 0 && item < (int) layout.blockOf.size()) ? layout.blockOf[(size_t) item] : -1;
    int                          k    = 0;



    if (at < 0)
    {
        return false;
    }

    k              = item - layout.blocks[(size_t) at].first;
    outRect.left   = layout.blocks[(size_t) at].left + layout.indent + layout.blocks[(size_t) at].colLefts[(size_t) (k / layout.perCol)] - m_list.m_leftPx;
    outRect.top    = layout.headerH + (k % layout.perCol) * grid.cellH;
    outRect.right  = layout.blocks[(size_t) at].left + layout.indent + layout.blocks[(size_t) at].colLefts[(size_t) (k / layout.perCol) + 1] - m_list.m_leftPx - m_list.GetListGroupGapPx (grid);
    outRect.bottom = outRect.top  + grid.cellH;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::FindListBlock
//
//  The block at a distance from the content's left, or -1 past the last.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListFlowLayout::FindListBlock (const DxuiListViewTypes::ListLayout & layout, int x)
{
    for (size_t b = 0; b < layout.blocks.size(); b++)
    {
        if (x >= layout.blocks[b].left && x < layout.blocks[b].left + layout.blocks[b].width)
        {
            return (int) b;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::HitTestListItem
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListFlowLayout::HitTestListItem (const DxuiListViewTypes::ListLayout & layout, int xPx, int yPx) const
{
    DxuiListViewTypes::ItemGrid  grid = m_list.m_items.GetItemGrid();
    int                          at   = FindListBlock (layout, xPx + m_list.m_leftPx);
    int                          col  = 0;
    int                          row  = 0;
    int                          k    = 0;



    if (at < 0 || yPx < layout.headerH || grid.cellW <= 0 || grid.cellH <= 0)
    {
        return -1;
    }

    col = (xPx + m_list.m_leftPx - layout.blocks[(size_t) at].left - layout.indent);
    row = (yPx - layout.headerH) / grid.cellH;

    if (col < 0 || row >= layout.perCol)
    {
        return -1;
    }

    k = 0;

    while ((size_t) (k / layout.perCol) + 1 < layout.blocks[(size_t) at].colLefts.size() && col >= layout.blocks[(size_t) at].colLefts[(size_t) (k / layout.perCol) + 1])
    {
        k += layout.perCol;
    }

    k += row;

    return (k < layout.blocks[(size_t) at].count) ? layout.blocks[(size_t) at].first + k : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::GetListColumns
//
//  List's columns left to right, grouped or not. A collapsed group has none,
//  since its header holds no items.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListViewTypes::ListColumn> DxuiListFlowLayout::GetListColumns() const
{
    std::vector<DxuiListViewTypes::ListColumn>    cols;
    DxuiListViewTypes::ItemGrid                   grid  = m_list.m_items.GetItemGrid();
    int                                           rows  = m_list.GetRowCount();
    int                                           gapPx = (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kNameColumnGapDip));
    const std::vector<int>                      & lefts = GetListColumnLefts();



    if (HasListGroups())
    {
        const DxuiListViewTypes::ListLayout & layout = GetListLayout();

        for (const DxuiListViewTypes::ListBlock & block : layout.blocks)
        {
            for (int k = 0; k < block.count; k += layout.perCol)
            {
                int  left  = block.left + layout.indent + block.colLefts[(size_t) (k / layout.perCol)];
                int  right = block.left + layout.indent + block.colLefts[(size_t) (k / layout.perCol) + 1] - m_list.GetListGroupGapPx (grid);

                cols.push_back (DxuiListViewTypes::ListColumn { left, right, block.first + k, (std::min) (layout.perCol, block.count - k) });
            }
        }

        return cols;
    }

    //  Each column's box ends short of the next by the gap after its names.
    for (size_t c = 0; c + 1 < lefts.size(); c++)
    {
        int  first = (int) c * grid.perLine;

        cols.push_back (DxuiListViewTypes::ListColumn { lefts[c], lefts[c + 1] - gapPx, first, (std::min) (grid.perLine, rows - first) });
    }

    return cols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout::FindColumnOfItem
//
//  The column holding an item, or -1 for one under a collapsed header.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListFlowLayout::FindColumnOfItem (const std::vector<DxuiListViewTypes::ListColumn> & cols, int item)
{
    for (size_t c = 0; c < cols.size(); c++)
    {
        if (item >= cols[c].first && item < cols[c].first + cols[c].count)
        {
            return (int) c;
        }
    }

    return -1;
}
