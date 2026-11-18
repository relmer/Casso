#include "Pch.h"

#include "Widgets/ListView/DxuiListTips.h"
#include "Widgets/ListView/DxuiListFind.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiTip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTips::DxuiListTips
//
////////////////////////////////////////////////////////////////////////////////

DxuiListTips::DxuiListTips (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTips::HitTestColumn
//
//  The column under an x in the list's pixels, in the order the columns are
//  drawn. A point in the pane lead is in the first column; -1 past the last.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListTips::HitTestColumn (int xPx) const
{
    int               fullW  = m_list.m_scrollbars.ComputeScrollLayout().viewportW;
    int               colOff = m_list.m_hScrollEnabled ? -m_list.m_leftPx : 0;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t n = 0; n < m_list.m_columns.size(); n++)
    {
        size_t  c = (n < m_list.GetColumnOrder().size()) ? m_list.GetColumnOrder()[n] : n;

        if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        if (xPx < colXPx[c] + colOff + colWPx[c])
        {
            return (int) c;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTips::TryGetCellTip
//
//  Over the header, the heading's tip at the pointer. Over a cell, the cell's
//  tip, under the tip of the range the pointer is over, anchored on the
//  cell's text.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTips::TryGetCellTip (POINT pointPx, DxuiTip & tip) const
{
    const std::vector<DxuiListViewTypes::Cell>  * cells    = nullptr;
    const DxuiListViewTypes::Cell               * cell     = nullptr;
    RECT                                          pointer  = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    int                                           column   = HitTestColumn (pointPx.x);
    int                                           row      = -1;
    const std::wstring                          * rangeTip = nullptr;



    if (column < 0)
    {
        return false;
    }

    tip             = DxuiTip();
    tip.isMonospace = m_list.m_isMonospaceTips;
    tip.isColorKey  = true;

    if (pointPx.y < m_list.GetHeaderHeightPx())
    {
        tip.text       = m_list.m_columns[(size_t) column].tip;
        tip.anchor     = pointer;
        tip.lineScales = m_list.m_columns[(size_t) column].tipLineScales;

        return !tip.text.empty();
    }

    row = m_list.HitTestRow (pointPx.x, pointPx.y);

    if (row < 0 || row >= m_list.GetRowCount())
    {
        return false;
    }

    cells = &m_list.GetRowCells (row);

    if ((size_t) column >= cells->size())
    {
        return false;
    }

    cell     = &(*cells)[(size_t) column];
    rangeTip = cell->rangeTips.empty() ? nullptr : DxuiListFind::FindRangeTip (*cell, m_list.m_textSelect.HitTestCharInCell (row, (size_t) column, pointPx.x));

    if (cell->tip.empty() && rangeTip == nullptr)
    {
        return false;
    }

    if (!m_list.GetCellTextRectPx (row, (size_t) column, tip.anchor))
    {
        tip.anchor = pointer;
    }

    tip.text         = (rangeTip == nullptr) ? cell->tip
                     : cell->tip.empty()     ? *rangeTip
                     :                         *rangeTip + L"\n\n" + cell->tip;
    tip.gradient     = cell->tipGradient;
    tip.gradientMark = cell->tipGradientMark;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTips::TryGetCellTextClipPx
//
//  Where the row painting starts a cell's text, after its lead, and where it
//  stops drawing it: short of the padding and the trail, and of the list's
//  right edge. A cell that spans the row runs to the row's end.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTips::TryGetCellTextClipPx (int row, size_t column, float & left, float & right) const
{
    HRESULT                          hr      = S_OK;
    float                            layoutW = (float) m_list.m_scrollbars.ComputeScrollLayout().viewportW;
    float                            colOff  = m_list.m_hScrollEnabled ? -(float) m_list.m_leftPx : 0.0f;
    float                            cellW   = 0.0f;
    const DxuiListViewTypes::Cell  * cell    = nullptr;
    RECT                             text    = {};
    bool                             isShown = false;
    std::vector<int>                 colXPx;
    std::vector<int>                 colWPx;



    isShown = !m_list.IsItemsView() && m_list.GetCellTextRectPx (row, column, text) && column < m_list.GetRowCells (row).size();
    CBR (isShown);

    cell = &m_list.GetRowCells (row)[column];

    m_list.m_columnSizing.ComputeColumnLayout (layoutW, colXPx, colWPx);

    cellW = cell->spansRow ? (std::max) ((float) colWPx[column], layoutW - colOff - (float) colXPx[column]) : (float) colWPx[column];
    left  = colOff + (float) colXPx[column] + (float) (m_list.m_columnSizing.GetCellPadLeftPx (column) + m_list.m_columnSizing.GetCellLeadPx (*cell, column));
    right = (std::min) (colOff + (float) colXPx[column] + cellW - (float) m_list.m_columnSizing.GetCellPadRightPx() - m_list.m_scaler.ToPxf ((float) cell->trailDip), layoutW);

Error:
    return isShown;
}