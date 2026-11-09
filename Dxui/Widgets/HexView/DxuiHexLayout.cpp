#include "Pch.h"

#include "Widgets/HexView/DxuiHexLayout.h"
#include "Widgets/DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::DxuiHexLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexLayout::DxuiHexLayout (DxuiHexView & view)
    : m_view (view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::ResetLineIndex
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayout::ResetLineIndex() const
{
    m_bookmarks.clear();
    m_scanBuffer.clear();

    m_scanBufferBase = 0;
    m_scanOffset     = 0;
    m_scanRows       = 0;
    m_scanLine       = 1;
    m_scanStarts     = true;
    m_scanComplete   = false;
    m_spanRow        = UINT64_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetByteAt
//
//  One byte for the line scan, from a buffer refilled a piece at a time.
//
////////////////////////////////////////////////////////////////////////////////

uint8_t DxuiHexLayout::GetByteAt (uint64_t offset) const
{
    uint64_t  count = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;



    if (offset >= count)
    {
        return 0;
    }

    if (offset < m_scanBufferBase || offset >= m_scanBufferBase + m_scanBuffer.size())
    {
        m_scanBufferBase = offset;
        m_scanBuffer.resize ((size_t) (std::min) ((uint64_t) s_kScanBytes, count - offset));
        m_view.m_source->ReadBytes (offset, std::span<uint8_t> (m_scanBuffer.data(), m_scanBuffer.size()));
    }

    return m_scanBuffer[(size_t) (offset - m_scanBufferBase)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::IsLineBreak
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexLayout::IsLineBreak (uint8_t byte) const
{
    return (byte == '\r') || (byte == '\n') || ((m_view.m_encoding == DxuiHexViewTypes::TextEncoding::AppleHighBit) && (byte == 0x8D));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetNextRowStart
//
//  Where the row after the one starting at `start` begins: past the line
//  break that ends it, or where it reaches the row width.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::GetNextRowStart (uint64_t start, bool & outNewLine) const
{
    uint64_t  count  = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  pos    = start;
    int       column = 0;



    outNewLine = false;

    while (pos < count)
    {
        uint8_t  byte = GetByteAt (pos);

        if (IsLineBreak (byte))
        {
            outNewLine = true;

            //  A carriage return and a line feed together are one break.
            if (byte != '\n' && pos + 1 < count && GetByteAt (pos + 1) == '\n')
            {
                return pos + 2;
            }

            return pos + 1;
        }

        if (column == m_view.m_bytesPerRow)
        {
            return pos;
        }

        column++;
        pos++;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::StepLineIndex
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayout::StepLineIndex() const
{
    uint64_t  count   = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    bool      newLine = false;



    if (m_scanOffset >= count)
    {
        m_scanComplete = true;
        return;
    }

    if ((m_scanRows % s_kBookmarkRows) == 0)
    {
        m_bookmarks.push_back (Bookmark { m_scanOffset, m_scanLine, m_scanStarts });
    }

    m_scanOffset = GetNextRowStart (m_scanOffset, newLine);
    m_scanRows++;
    m_scanLine    += newLine ? 1 : 0;
    m_scanStarts   = newLine;
    m_scanComplete = m_scanOffset >= count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::IndexThroughRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayout::IndexThroughRow (uint64_t row) const
{
    while (!m_scanComplete && m_scanRows <= row)
    {
        StepLineIndex();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::IndexThroughOffset
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayout::IndexThroughOffset (uint64_t offset) const
{
    while (!m_scanComplete && m_scanOffset <= offset)
    {
        StepLineIndex();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetShownLength
//
//  The characters a row shows: its bytes less the line break that ends it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::GetShownLength (uint64_t start, uint64_t end) const
{
    uint64_t  length = end - start;



    while (length > 0 && IsLineBreak (GetByteAt (start + length - 1)))
    {
        length--;
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetRowSpan
//
//  A row's first byte, the byte after it, its line number, and whether it
//  begins that line. The last row asked for is remembered, so painting rows
//  in order scans one row each.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexLayout::GetRowSpan (uint64_t row, uint64_t & outStart, uint64_t & outEnd, uint64_t & outLine, bool & outStarts) const
{
    uint64_t  at      = 0;
    bool      newLine = false;



    IndexThroughRow (row);

    if (row >= m_scanRows)
    {
        return false;
    }

    if (m_spanRow == UINT64_MAX || m_spanRow > row || (row / s_kBookmarkRows) != (m_spanRow / s_kBookmarkRows))
    {
        const Bookmark &  mark = m_bookmarks[(size_t) (row / s_kBookmarkRows)];

        m_spanRow    = (row / s_kBookmarkRows) * s_kBookmarkRows;
        m_spanStart  = mark.offset;
        m_spanLine   = mark.line;
        m_spanStarts = mark.starts;
    }

    for (at = m_spanRow; at < row; at++)
    {
        m_spanStart   = GetNextRowStart (m_spanStart, newLine);
        m_spanLine   += newLine ? 1 : 0;
        m_spanStarts  = newLine;
    }

    m_spanRow = row;

    outStart  = m_spanStart;
    outEnd    = GetNextRowStart (m_spanStart, newLine);
    outLine   = m_spanLine;
    outStarts = m_spanStarts;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetRowOfOffset
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::GetRowOfOffset (uint64_t offset) const
{
    uint64_t  count   = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  row     = 0;
    uint64_t  start   = 0;
    uint64_t  next    = 0;
    size_t    mark    = 0;
    bool      newLine = false;



    if (!m_view.IsLineMode())
    {
        return offset / (uint64_t) m_view.m_bytesPerRow;
    }

    IndexThroughOffset (offset);

    if (m_bookmarks.empty())
    {
        return 0;
    }

    while (mark + 1 < m_bookmarks.size() && m_bookmarks[mark + 1].offset <= offset)
    {
        mark++;
    }

    row   = (uint64_t) mark * s_kBookmarkRows;
    start = m_bookmarks[mark].offset;

    for (;;)
    {
        next = GetNextRowStart (start, newLine);

        if (next > offset || next >= count)
        {
            return row;
        }

        start = next;
        row++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetColumnOfOffset
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetColumnOfOffset (uint64_t offset) const
{
    uint64_t  start   = 0;
    uint64_t  end     = 0;
    uint64_t  line    = 0;
    bool      starts  = false;



    if (!m_view.IsLineMode() || !GetRowSpan (GetRowOfOffset (offset), start, end, line, starts))
    {
        return (int) (offset % (uint64_t) m_view.m_bytesPerRow);
    }

    return (int) (std::min) (offset - start, GetShownLength (start, end));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetLineDigits
//
//  Room for the largest line number the file could have, one line a byte, so
//  the column does not widen as the scan finds more lines.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetLineDigits() const
{
    uint64_t  count  = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    int       digits = 0;



    for (count = (std::max) (count, (uint64_t) 1); count > 0; count /= 10)
    {
        digits++;
    }

    return (std::max) (digits, 4);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetLineModeTarget
//
//  Where a movement key takes the caret in line mode: the same column of the
//  row above or below, clamped to that row's length, or the ends of the row.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::GetLineModeTarget (WPARAM vk, bool ctrl) const
{
    uint64_t  row    = GetRowOfOffset (m_view.m_selection.GetCaret());
    uint64_t  start  = 0;
    uint64_t  end    = 0;
    uint64_t  line   = 0;
    uint64_t  length = 0;
    uint64_t  column = 0;
    uint64_t  cap    = (uint64_t) (std::max) (GetRowCap(), 1);
    uint64_t  last   = m_view.GetLastOffset();
    uint64_t  target = row;
    bool      starts = false;



    if (!GetRowSpan (row, start, end, line, starts))
    {
        return last;
    }

    column = m_view.m_selection.GetCaret() - start;
    length = GetShownLength (start, end);

    switch (vk)
    {
    case VK_HOME:  return ctrl ? 0 : start;
    case VK_END:   return ctrl ? last : (start + ((length > 0) ? length - 1 : 0));
    case VK_UP:    target = (row > 0) ? row - 1 : 0;             break;
    case VK_PRIOR: target = (row > cap) ? row - cap : 0;         break;
    case VK_DOWN:  target = row + 1;                             break;
    default:       target = row + cap;                           break;
    }

    if (!GetRowSpan (target, start, end, line, starts))
    {
        return last;
    }

    length = GetShownLength (start, end);

    return (std::min) (start + (std::min) (column, (length > 0) ? length - 1 : 0), last);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::RecomputeRowWidth
//
//  Automatic columns take as many values as fit beside the offsets and the
//  text column, with room kept for the scrollbar so a row does not change
//  width when the scrollbar comes and goes. A caret the new width moves off
//  the screen is brought back into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayout::RecomputeRowWidth()
{
    int   values  = m_columns;
    int   width   = 0;
    int   cells   = 0;
    int   fixed   = 0;
    int   each    = 0;
    bool  changed = false;



    if (m_columns == DxuiHexViewTypes::s_kFixedRowWidth)
    {
        return;
    }

    if (m_columns == 0)
    {
        width  = (m_view.m_boundsDip.right - m_view.m_boundsDip.left) - m_view.m_scaler.ToPx (m_padDip) - m_view.m_scaler.ToPx (DxuiHexView::s_kScrollbarWidthDip);
        cells  = (m_view.m_cellWidthDip > 0) ? (width / m_view.m_cellWidthDip) : 0;
        fixed  = GetOffsetDigits() + DxuiHexView::kGutterCells + (m_view.m_showValues ? (DxuiHexView::kGutterCells - 1) : 0);
        each   = m_view.m_grouping + (m_view.m_showValues ? (GetValueCells() + 1) : 0);
        values = (std::max) ((cells - fixed) / each, 1);
    }

    changed = m_view.m_bytesPerRow != values * m_view.m_grouping;

    if (changed)
    {
        ResetLineIndex();
    }

    m_view.m_bytesPerRow = values * m_view.m_grouping;

    ClampTopRow();
    m_view.SetLeftPx (m_view.m_scroll.GetLeftPx());

    //  Only a new row width moves the caret's row; after any other layout the
    //  scroll is where the user put it.
    if (m_view.m_selection.HasSelection() && changed)
    {
        m_view.EnsureByteVisible (m_view.m_selection.GetCaret());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetHeightDip() const
{
    int  height = m_view.m_boundsDip.bottom - m_view.m_boundsDip.top;



    if (m_view.IsHorzScrollbarVisible())
    {
        height -= m_view.m_scaler.ToPx (DxuiHexView::s_kScrollbarWidthDip);
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetContentWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetContentWidthPx() const
{
    return m_view.m_scaler.ToPx (m_padDip) + ((GetColumnStartCell (DxuiHexViewTypes::Column::Text) + m_view.m_bytesPerRow) * m_view.m_cellWidthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetViewWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetViewWidthPx() const
{
    return (std::max) (0, (int) (m_view.m_boundsDip.right - m_view.m_boundsDip.left) - m_view.m_scaler.ToPx (DxuiHexView::s_kScrollbarWidthDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetValueCells
//
//  Two digits a byte in hex; in decimal, the digits of the largest value the
//  bytes hold, and a sign's cell when signed.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetValueCells() const
{
    int  digits = 0;



    if (m_view.m_format == DxuiHexViewTypes::ValueFormat::Hex)
    {
        return m_view.m_grouping * 2;
    }

    switch (m_view.m_grouping)
    {
    case 1:  digits = 3;  break;
    case 2:  digits = 5;  break;
    case 4:  digits = 10; break;
    default: digits = 20; break;
    }

    return (m_view.m_format == DxuiHexViewTypes::ValueFormat::Signed) ? (digits + 1) : digits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::SnapToValue
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::SnapToValue (uint64_t offset, bool toEnd) const
{
    uint64_t  start = offset - (offset % (uint64_t) m_view.m_grouping);



    return toEnd ? (std::min) (start + (uint64_t) m_view.m_grouping - 1, m_view.GetLastOffset()) : start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetValueSelectionRect
//
//  A selected value's fill reaches halfway into the space on each side, so a
//  run of values reads as one band.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexLayout::GetValueSelectionRect (uint64_t first, const RECT & cell) const
{
    RECT      rect   = cell;
    int       margin = m_view.m_scaler.ToPx (DxuiHexView::s_kSelectionMarginDip);
    uint64_t  perRow = (uint64_t) m_view.m_bytesPerRow;



    rect.left  -= m_view.m_cellWidthDip / 2;
    rect.right += m_view.m_cellWidthDip - (m_view.m_cellWidthDip / 2);

    if ((first < perRow) || !m_view.IsByteSelected (first - perRow))
    {
        rect.top -= margin;
    }

    if (!m_view.IsByteSelected (first + perRow))
    {
        rect.bottom += margin;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetRowCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::GetRowCount() const
{
    uint64_t  bytes  = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  perRow = (uint64_t) m_view.m_bytesPerRow;



    //  In line mode the rows scanned so far, and an estimate for the rest from
    //  their average length until the scan reaches the end.
    if (m_view.IsLineMode() && bytes > 0)
    {
        //  The first megabyte is scanned whole, so a small file's count is
        //  exact and a large file's estimate rests on a fair sample.
        IndexThroughOffset ((std::min) (bytes - 1, (uint64_t) 1024 * 1024));

        return m_scanComplete ? m_scanRows
                              : m_scanRows + ((bytes - m_scanOffset) * m_scanRows) / (std::max) (m_scanOffset, (uint64_t) 1) + 1;
    }

    return (bytes + perRow - 1) / perRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetRowCap
//
//  Rows the bounds can show. A partly visible last row does not count: the
//  page keys and the wheel move by whole rows the user can read.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetRowCap() const
{
    if (m_cellHeightDip <= 0)
    {
        return 0;
    }

    return (std::max) (GetHeightDip() / m_cellHeightDip, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetMaxTopRow
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexLayout::GetMaxTopRow() const
{
    uint64_t  rows = GetRowCount();
    uint64_t  cap  = (uint64_t) GetRowCap();



    return (rows > cap) ? (rows - cap) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetOffsetDigits
//
//  Four hex digits while the last address fits in sixteen bits, eight after
//  that. A 64 KB file and a machine's memory both label in the four digits
//  their own listings use.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetOffsetDigits() const
{
    uint64_t  bytes = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  last  = (bytes > 0) ? (m_view.m_originAddress + bytes - 1) : m_view.m_originAddress;



    if (m_view.IsLineMode())
    {
        return GetLineDigits();
    }

    return (last > 0xFFFF) ? 8 : 4;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetGroupCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetGroupCount() const
{
    return m_view.m_bytesPerRow / m_view.m_grouping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetHexCells
//
//  Each value's cells, and one cell between values; nothing without values.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetHexCells() const
{
    if (!m_view.m_showValues)
    {
        return 0;
    }

    return (GetGroupCount() * (GetValueCells() + 1)) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetByteCellInRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetByteCellInRow (int indexInRow) const
{
    int  value = indexInRow / m_view.m_grouping;
    int  start = value * (GetValueCells() + 1);



    //  A hex value is little-endian, so its first byte's digits come last.
    if (m_view.m_format == DxuiHexViewTypes::ValueFormat::Hex)
    {
        start += (m_view.m_grouping - 1 - (indexInRow % m_view.m_grouping)) * 2;
    }

    return start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetColumnStartCell
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayout::GetColumnStartCell (DxuiHexViewTypes::Column column) const
{
    int  hexStart = GetOffsetDigits() + DxuiHexView::kGutterCells;



    if (column == DxuiHexViewTypes::Column::Hex)
    {
        return hexStart;
    }

    if (column == DxuiHexViewTypes::Column::Text)
    {
        return m_view.m_showValues ? (hexStart + GetHexCells() + DxuiHexView::kGutterCells) : hexStart;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexLayout::GetCellRect (int cellX, uint64_t row, int cellCount) const
{
    RECT      rect  = {};
    int64_t   rowUp = (int64_t) (row - m_view.m_scroll.GetTopRow());
    LONG      top   = m_view.m_boundsDip.top + (LONG) (rowUp * m_cellHeightDip);
    LONG      left  = m_view.m_boundsDip.left + m_view.m_scaler.ToPx (m_padDip) + (LONG) (cellX * m_view.m_cellWidthDip) - m_view.m_scroll.GetLeftPx();



    rect.left   = left;
    rect.top    = top;
    rect.right  = left + (LONG) (cellCount * m_view.m_cellWidthDip);
    rect.bottom = top + (LONG) m_cellHeightDip;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::IsRowVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexLayout::IsRowVisible (uint64_t row) const
{
    int  cap = GetRowCap();



    return (row >= m_view.m_scroll.GetTopRow()) && (row < (m_view.m_scroll.GetTopRow() + (uint64_t) cap));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::ClampTopRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayout::ClampTopRow()
{
    m_view.m_scroll.SetTopRow (m_view.m_scroll.GetTopRow());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetRowOffsetRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexLayout::GetRowOffsetRect (uint64_t row) const
{
    if (!IsRowVisible (row))
    {
        return RECT{};
    }

    return GetCellRect (0, row, GetOffsetDigits());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::GetByteRect
//
//  A byte is two cells wide under the hex column and one under the text
//  column, which is what lets one selection light in both at once.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexLayout::GetByteRect (uint64_t offset, DxuiHexViewTypes::Column column) const
{
    uint64_t  bytes = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  row   = GetRowOfOffset (offset);
    int       index = GetColumnOfOffset (offset);



    if ((offset >= bytes) || !IsRowVisible (row) || (column == DxuiHexViewTypes::Column::None))
    {
        return RECT{};
    }

    if (column == DxuiHexViewTypes::Column::Hex)
    {
        if (!m_view.m_showValues)
        {
            return RECT{};
        }

        //  A decimal value has no digits of its own for each byte, so every
        //  byte in it is the whole value.
        return GetCellRect (GetColumnStartCell (DxuiHexViewTypes::Column::Hex) + GetByteCellInRow (index), row,
                            (m_view.m_format == DxuiHexViewTypes::ValueFormat::Hex) ? 2 : GetValueCells());
    }

    return GetCellRect (GetColumnStartCell (DxuiHexViewTypes::Column::Text) + index, row, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout::HitTestPoint
//
//  The space between two groups belongs to the byte on its left, so a drag
//  across a row never stalls between bytes. A point past the last byte of a
//  short final row lands on that byte rather than on nothing, which is what
//  makes dragging to the end of the file work.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexViewTypes::HitResult DxuiHexLayout::HitTestPoint (POINT clientDip) const
{
    DxuiHexViewTypes::HitResult  result;
    uint64_t                     bytes    = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    int                          cellW    = m_view.m_cellWidthDip;
    int                          cellH    = m_cellHeightDip;
    int                          rowUp    = 0;
    int                          cellX    = 0;
    int                          hexStart = GetColumnStartCell (DxuiHexViewTypes::Column::Hex);
    int                          txtStart = GetColumnStartCell (DxuiHexViewTypes::Column::Text);
    uint64_t                     row      = 0;
    int                          index    = -1;



    if ((bytes == 0) || (cellW <= 0) || (cellH <= 0))
    {
        return result;
    }

    if ((clientDip.x < m_view.m_boundsDip.left) || (clientDip.y < m_view.m_boundsDip.top))
    {
        return result;
    }

    rowUp = (int) ((clientDip.y - m_view.m_boundsDip.top) / cellH);
    cellX = (int) ((clientDip.x - m_view.m_boundsDip.left - m_view.m_scaler.ToPx (m_padDip) + m_view.m_scroll.GetLeftPx()) / cellW);

    if (rowUp >= GetRowCap())
    {
        return result;
    }

    row = m_view.m_scroll.GetTopRow() + (uint64_t) rowUp;

    if (row >= GetRowCount())
    {
        return result;
    }

    //  In line mode a point lands on the character under it, or on the row's
    //  last character past its end.
    if (m_view.IsLineMode())
    {
        uint64_t  start  = 0;
        uint64_t  end    = 0;
        uint64_t  line   = 0;
        uint64_t  length = 0;
        bool      starts = false;

        if (cellX < txtStart)
        {
            return result;
        }

        result.hit    = true;
        result.column = DxuiHexViewTypes::Column::Text;
        result.offset = bytes - 1;

        if (GetRowSpan (row, start, end, line, starts))
        {
            length        = GetShownLength (start, end);
            result.offset = (std::min) (start + (std::min) ((uint64_t) (cellX - txtStart), (length > 0) ? length - 1 : 0), bytes - 1);
        }

        return result;
    }

    if (m_view.m_showValues && (cellX >= hexStart) && (cellX < (hexStart + GetHexCells())))
    {
        int  inHex  = cellX - hexStart;
        int  each   = GetValueCells() + 1;
        int  value  = (std::min) (inHex / each, GetGroupCount() - 1);
        int  within = inHex - (value * each);

        //  In hex a point lands on the byte whose digits it is over, counting
        //  from the right since the value is little-endian; in decimal, on the
        //  value's first byte.
        index = value * m_view.m_grouping;

        if (m_view.m_format == DxuiHexViewTypes::ValueFormat::Hex)
        {
            index += m_view.m_grouping - 1 - ((std::min) (within, (m_view.m_grouping * 2) - 1) / 2);
        }

        result.column = DxuiHexViewTypes::Column::Hex;
    }
    else if ((cellX >= txtStart) && (cellX < (txtStart + m_view.m_bytesPerRow)))
    {
        index         = cellX - txtStart;
        result.column = DxuiHexViewTypes::Column::Text;
    }

    if (index < 0)
    {
        return result;
    }

    result.offset = (row * (uint64_t) m_view.m_bytesPerRow) + (uint64_t) index;

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
//  DxuiHexLayout::GetSelectionCellRect
//
//  A selected byte's fill reaches halfway into the gap on each side, so a run
//  reads as one band with the spacing inside it and a margin past its ends.
//  A byte at the row's edge borrows the gap from its other side, and a text
//  column cell, which has no gap, keeps its width.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexLayout::GetSelectionCellRect (uint64_t offset, int index, const RECT & cell, bool hexColumn) const
{
    RECT      rect     = cell;
    int       gapLeft  = 0;
    int       gapRight = 0;
    int       margin   = m_view.m_scaler.ToPx (DxuiHexView::s_kSelectionMarginDip);
    uint64_t  perRow   = (uint64_t) m_view.m_bytesPerRow;



    if (hexColumn && index + 1 < m_view.m_bytesPerRow)
    {
        gapRight = GetByteRect (offset + 1, DxuiHexViewTypes::Column::Hex).left - cell.right;
    }

    if (hexColumn && index > 0)
    {
        gapLeft = cell.left - GetByteRect (offset - 1, DxuiHexViewTypes::Column::Hex).right;
    }

    gapRight = (index + 1 < m_view.m_bytesPerRow) ? gapRight : gapLeft;
    gapLeft  = (index > 0) ? gapLeft : gapRight;

    rect.left  -= gapLeft / 2;
    rect.right += gapRight - gapRight / 2;

    //  A run's top and bottom edges reach a little past the characters, but
    //  not where the neighboring row is selected too, whose text would be
    //  covered.
    if (offset < perRow || !m_view.IsByteSelected (offset - perRow))
    {
        rect.top -= margin;
    }

    if (!m_view.IsByteSelected (offset + perRow))
    {
        rect.bottom += margin;
    }

    return rect;
}
