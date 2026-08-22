#include "Pch.h"

#include "Core/DxuiGridLayout.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGridLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiGridLayout::DxuiGridLayout (int rows, int cols, float gapDip)
    : m_rows   (rows < 1 ? 1 : rows),
      m_cols   (cols < 1 ? 1 : cols),
      m_gapDip (gapDip < 0.0f ? 0.0f : gapDip)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCell
//
////////////////////////////////////////////////////////////////////////////////

void DxuiGridLayout::SetCell (IDxuiControl * child, int row, int col, int rowSpan, int colSpan)
{
    Cell  cell;



    if (child == nullptr)
    {
        return;
    }

    cell.row     = (row     < 0) ? 0 : row;
    cell.col     = (col     < 0) ? 0 : col;
    cell.rowSpan = (rowSpan < 1) ? 1 : rowSpan;
    cell.colSpan = (colSpan < 1) ? 1 : colSpan;

    m_cells[child] = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Arrange
//
//  Places children into a uniform grid, honoring row and column spans.
//
//  Cell size is derived by removing the GAPS first and dividing what remains,
//  so the gap count is fixed by the grid shape rather than growing with the
//  cell size -- the alternative leaves a wider gutter on the right than on the
//  left.
//
//  A spanned cell is computed as the span's far EDGE minus one gap, not as
//  cells times width plus gaps. That way a span absorbs the gaps it crosses
//  and its right edge lands exactly on the boundary an unspanned neighbor
//  would use, so a two-column item aligns with the item below it.
//
//  Row and column indices are CLAMPED, and spans clamped to the grid, so a
//  cell assignment left over from a smaller grid places the child at the edge
//  rather than off it.
//
//  A child with no assignment defaults to cell (0,0) rather than being skipped,
//  which keeps it visible and obviously misplaced instead of invisible.
//
//  A degenerate bounds rect yields zero-size cells rather than negative ones,
//  so a grid laid out before its container has a size collapses harmlessly.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiGridLayout::Arrange (
    const RECT                          & boundsDip,
    const DxuiDpiScaler                 & /*scaler*/,
    std::span<IDxuiControl * const>       children)
{
    LONG   bandX    = boundsDip.left;
    LONG   bandY    = boundsDip.top;
    LONG   width    = boundsDip.right  - boundsDip.left;
    LONG   height   = boundsDip.bottom - boundsDip.top;
    LONG   gap      = (LONG) m_gapDip;
    LONG   colTotal = width  - gap * (m_cols - 1);
    LONG   rowTotal = height - gap * (m_rows - 1);
    LONG   cellW    = (colTotal > 0) ? colTotal / m_cols : 0;
    LONG   cellH    = (rowTotal > 0) ? rowTotal / m_rows : 0;



    if (children.empty())
    {
        return;
    }

    for (IDxuiControl * child : children)
    {
        auto  it     = m_cells.find (child);
        Cell  cell   = (it == m_cells.end()) ? Cell{} : it->second;
        int   row    = (cell.row >= m_rows) ? (m_rows - 1) : cell.row;
        int   col    = (cell.col >= m_cols) ? (m_cols - 1) : cell.col;
        int   rowEnd = row + cell.rowSpan;
        int   colEnd = col + cell.colSpan;
        RECT  bounds = {};

        if (rowEnd > m_rows) { rowEnd = m_rows; }
        if (colEnd > m_cols) { colEnd = m_cols; }

        bounds.left   = bandX + col    * (cellW + gap);
        bounds.top    = bandY + row    * (cellH + gap);
        bounds.right  = bandX + colEnd * (cellW + gap) - gap;
        bounds.bottom = bandY + rowEnd * (cellH + gap) - gap;

        child->SetBounds (bounds);
    }
}
