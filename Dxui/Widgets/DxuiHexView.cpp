#include "Pch.h"

#include "DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetSource
//
//  A new set of bytes is a new view: the old selection pointed at offsets
//  that mean nothing now, and the old scroll position may be past the end.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetSource (const IDxuiHexSource * source)
{
    m_source = source;

    ClearSelection();
    m_topRow = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetOriginAddress
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetOriginAddress (uint64_t address)
{
    m_originAddress = address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetBytesPerRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetBytesPerRow (int count)
{
    assert (count > 0);

    if (count <= 0)
    {
        return;
    }

    m_bytesPerRow = count;

    //  A grouping the new row width cannot divide falls back to single bytes,
    //  which every row width divides.
    if ((m_bytesPerRow % m_grouping) != 0)
    {
        m_grouping = kDefaultGrouping;
    }

    ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetGrouping
//
//  One, two, four or eight bytes between spaces, and only a grouping the row
//  divides evenly -- a half group at the end of a row would put the same byte
//  at two different distances from the left on different rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::SetGrouping (int bytesPerGroup)
{
    bool  known = (bytesPerGroup == 1) || (bytesPerGroup == 2)
               || (bytesPerGroup == 4) || (bytesPerGroup == 8);



    if (!known || ((m_bytesPerRow % bytesPerGroup) != 0))
    {
        return false;
    }

    m_grouping = bytesPerGroup;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetCellSizeDip
//
//  One character cell of the fixed-width face the host paints with. Every
//  column position here is a whole number of cells, which is what keeps the
//  hex digits and their characters in line down the view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetCellSizeDip (int widthDip, int heightDip)
{
    assert (widthDip > 0 && heightDip > 0);

    m_cellWidthDip  = (std::max) (widthDip,  0);
    m_cellHeightDip = (std::max) (heightDip, 0);

    ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetRowCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetRowCount() const
{
    uint64_t  bytes   = (m_source != nullptr) ? m_source->GetByteCount() : 0;
    uint64_t  perRow  = (uint64_t) m_bytesPerRow;



    return (bytes + perRow - 1) / perRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetRowCap
//
//  Rows the bounds can show. A partly visible last row does not count: the
//  page keys and the wheel move by whole rows the user can read.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetRowCap() const
{
    if (m_cellHeightDip <= 0)
    {
        return 0;
    }

    return (std::max) (GetHeightDip() / m_cellHeightDip, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetMaxTopRow
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetMaxTopRow() const
{
    uint64_t  rows = GetRowCount();
    uint64_t  cap  = (uint64_t) GetRowCap();



    return (rows > cap) ? (rows - cap) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetTopRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetTopRow (uint64_t row)
{
    m_topRow = (std::min) (row, GetMaxTopRow());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ScrollRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ScrollRows (int64_t delta)
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
//  DxuiHexView::EnsureByteVisible
//
//  Scrolls the least that brings a byte's row into view, so a caret walked
//  off either end moves the view by one row rather than recentering it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::EnsureByteVisible (uint64_t offset)
{
    uint64_t  row = offset / (uint64_t) m_bytesPerRow;
    int       cap = GetRowCap();



    if (cap <= 0)
    {
        return;
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
//  DxuiHexView::GetOffsetDigits
//
//  Four hex digits while the last address fits in sixteen bits, eight after
//  that. A 64 KB file and a machine's memory both label in the four digits
//  their own listings use.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetOffsetDigits() const
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;
    uint64_t  last  = (bytes > 0) ? (m_originAddress + bytes - 1) : m_originAddress;



    return (last > 0xFFFF) ? 8 : 4;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetGroupCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetGroupCount() const
{
    return m_bytesPerRow / m_grouping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetHexCells
//
//  Two cells a byte, and one cell between groups.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetHexCells() const
{
    return (m_bytesPerRow * 2) + (GetGroupCount() - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetByteCellInRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetByteCellInRow (int indexInRow) const
{
    return (indexInRow * 2) + (indexInRow / m_grouping);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetColumnStartCell
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetColumnStartCell (Column column) const
{
    int  hexStart = GetOffsetDigits() + kGutterCells;



    if (column == Column::Hex)
    {
        return hexStart;
    }

    if (column == Column::Text)
    {
        return hexStart + GetHexCells() + kGutterCells;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexView::GetCellRect (int cellX, uint64_t row, int cellCount) const
{
    RECT      rect  = {};
    int64_t   rowUp = (int64_t) (row - m_topRow);
    LONG      top   = m_boundsDip.top + (LONG) (rowUp * m_cellHeightDip);
    LONG      left  = m_boundsDip.left + (LONG) (cellX * m_cellWidthDip);



    rect.left   = left;
    rect.top    = top;
    rect.right  = left + (LONG) (cellCount * m_cellWidthDip);
    rect.bottom = top + (LONG) m_cellHeightDip;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::IsRowVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::IsRowVisible (uint64_t row) const
{
    int  cap = GetRowCap();



    return (row >= m_topRow) && (row < (m_topRow + (uint64_t) cap));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ClampTopRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ClampTopRow()
{
    m_topRow = (std::min) (m_topRow, GetMaxTopRow());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetRowOffsetRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexView::GetRowOffsetRect (uint64_t row) const
{
    if (!IsRowVisible (row))
    {
        return RECT{};
    }

    return GetCellRect (0, row, GetOffsetDigits());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetByteRect
//
//  A byte is two cells wide under the hex column and one under the text
//  column, which is what lets one selection light in both at once.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexView::GetByteRect (uint64_t offset, Column column) const
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;
    uint64_t  row   = offset / (uint64_t) m_bytesPerRow;
    int       index = (int) (offset % (uint64_t) m_bytesPerRow);



    if ((offset >= bytes) || !IsRowVisible (row) || (column == Column::None))
    {
        return RECT{};
    }

    if (column == Column::Hex)
    {
        return GetCellRect (GetColumnStartCell (Column::Hex) + GetByteCellInRow (index), row, 2);
    }

    return GetCellRect (GetColumnStartCell (Column::Text) + index, row, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::HitTestPoint
//
//  The space between two groups belongs to the byte on its left, so a drag
//  across a row never stalls between bytes. A point past the last byte of a
//  short final row lands on that byte rather than on nothing, which is what
//  makes dragging to the end of the file work.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::HitResult DxuiHexView::HitTestPoint (POINT clientDip) const
{
    HitResult  result;
    uint64_t   bytes    = (m_source != nullptr) ? m_source->GetByteCount() : 0;
    int        cellW    = m_cellWidthDip;
    int        cellH    = m_cellHeightDip;
    int        rowUp    = 0;
    int        cellX    = 0;
    int        hexStart = GetColumnStartCell (Column::Hex);
    int        txtStart = GetColumnStartCell (Column::Text);
    uint64_t   row      = 0;
    int        index    = -1;



    if ((bytes == 0) || (cellW <= 0) || (cellH <= 0))
    {
        return result;
    }

    if ((clientDip.x < m_boundsDip.left) || (clientDip.y < m_boundsDip.top))
    {
        return result;
    }

    rowUp = (int) ((clientDip.y - m_boundsDip.top) / cellH);
    cellX = (int) ((clientDip.x - m_boundsDip.left) / cellW);

    if (rowUp >= GetRowCap())
    {
        return result;
    }

    row = m_topRow + (uint64_t) rowUp;

    if (row >= GetRowCount())
    {
        return result;
    }

    if ((cellX >= hexStart) && (cellX < (hexStart + GetHexCells())))
    {
        int  inHex = cellX - hexStart;

        //  Walk the row's bytes rather than inverting the group spacing:
        //  sixteen comparisons are cheaper to read than the arithmetic.
        for (int idx = 0; idx < m_bytesPerRow; idx++)
        {
            int  start = GetByteCellInRow (idx);

            if (inHex >= start)
            {
                index = idx;
            }
        }

        result.column = Column::Hex;
    }
    else if ((cellX >= txtStart) && (cellX < (txtStart + m_bytesPerRow)))
    {
        index         = cellX - txtStart;
        result.column = Column::Text;
    }

    if (index < 0)
    {
        return result;
    }

    result.offset = (row * (uint64_t) m_bytesPerRow) + (uint64_t) index;

    //  The last row can be short.
    if (result.offset >= bytes)
    {
        result.offset = bytes - 1;
    }

    result.hit = true;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SelectByte
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SelectByte (uint64_t offset, Column column)
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    if (offset >= bytes)
    {
        return;
    }

    m_anchor       = offset;
    m_caret        = offset;
    m_column       = (column == Column::None) ? Column::Hex : column;
    m_hasSelection = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ExtendSelectionTo
//
//  Only the moving end moves. Extending before anything was selected starts
//  the run where it lands, so Shift with a click into an empty view behaves
//  like an ordinary click.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ExtendSelectionTo (uint64_t offset)
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    if (offset >= bytes)
    {
        return;
    }

    if (!m_hasSelection)
    {
        SelectByte (offset, m_column);
        return;
    }

    m_caret = offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SelectAll()
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    if (bytes == 0)
    {
        ClearSelection();
        return;
    }

    m_anchor       = 0;
    m_caret        = bytes - 1;
    m_column       = (m_column == Column::None) ? Column::Hex : m_column;
    m_hasSelection = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ClearSelection()
{
    m_hasSelection = false;
    m_anchor       = 0;
    m_caret        = 0;
    m_column       = Column::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionFirst
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetSelectionFirst() const
{
    return (std::min) (m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionLast
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetSelectionLast() const
{
    return (std::max) (m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetSelectionCount() const
{
    if (!m_hasSelection)
    {
        return 0;
    }

    return GetSelectionLast() - GetSelectionFirst() + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::IsByteSelected
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::IsByteSelected (uint64_t offset) const
{
    return m_hasSelection
        && (offset >= GetSelectionFirst())
        && (offset <= GetSelectionLast());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    (void) painter;
    (void) text;
    (void) theme;
}
