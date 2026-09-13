#include "Pch.h"

#include "DxuiHexView.h"

#include "Theme/IDxuiTheme.h"
#include "Render/IDxuiTextRenderer.h"
#include "Core/DxuiClipboard.h"





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

    RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetOriginAddress
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetOriginAddress (uint64_t address)
{
    m_originAddress = address;

    RecomputeRowWidth();
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
    m_columns     = s_kFixedRowWidth;

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
//  One, two, four or eight bytes between spaces, and only a grouping that
//  divides the row evenly; a partial group at the end of a row would place the
//  same byte index at different positions on different rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::SetGrouping (int bytesPerGroup)
{
    bool  known = (bytesPerGroup == 1) || (bytesPerGroup == 2)
               || (bytesPerGroup == 4) || (bytesPerGroup == 8);



    if (!known)
    {
        return false;
    }

    //  A row counted in values is always a whole number of them; only a row
    //  set in bytes can be one the grouping does not divide.
    if ((m_columns == s_kFixedRowWidth) && ((m_bytesPerRow % bytesPerGroup) != 0))
    {
        return false;
    }

    m_grouping = bytesPerGroup;

    RecomputeRowWidth();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetColumns (int valuesPerRow)
{
    assert (valuesPerRow >= 0);

    m_columns = (std::max) (valuesPerRow, 0);

    RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetValueFormat
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetValueFormat (ValueFormat format)
{
    m_format = format;

    RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetShowValues
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetShowValues (bool show)
{
    m_showValues = show;

    RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::RecomputeRowWidth
//
//  Automatic columns take as many values as fit beside the offsets and the
//  text column, with room kept for the scrollbar so a row does not change
//  width when the scrollbar comes and goes. A caret the new width moves off
//  the screen is brought back into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::RecomputeRowWidth()
{
    int  values = m_columns;
    int  width  = 0;
    int  cells  = 0;
    int  fixed  = 0;
    int  each   = 0;



    if (m_columns == s_kFixedRowWidth)
    {
        return;
    }

    if (m_columns == 0)
    {
        width  = (m_boundsDip.right - m_boundsDip.left) - m_scaler.ToPx (m_padDip) - m_scaler.ToPx (s_kScrollbarWidthDip);
        cells  = (m_cellWidthDip > 0) ? (width / m_cellWidthDip) : 0;
        fixed  = GetOffsetDigits() + kGutterCells + (m_showValues ? (kGutterCells - 1) : 0);
        each   = m_grouping + (m_showValues ? (GetValueCells() + 1) : 0);
        values = (std::max) ((cells - fixed) / each, 1);
    }

    m_bytesPerRow = values * m_grouping;

    ClampTopRow();
    SetLeftPx (m_leftPx);

    if (m_hasSelection)
    {
        EnsureByteVisible (m_caret);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetLeftPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetLeftPx (int leftPx)
{
    m_leftPx = (std::max) (0, (std::min) (leftPx, GetContentWidthPx() - GetViewWidthPx()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetHeightDip() const
{
    int  height = m_boundsDip.bottom - m_boundsDip.top;



    if (IsHorzScrollbarVisible())
    {
        height -= m_scaler.ToPx (s_kScrollbarWidthDip);
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetContentWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetContentWidthPx() const
{
    return m_scaler.ToPx (m_padDip) + ((GetColumnStartCell (Column::Text) + m_bytesPerRow) * m_cellWidthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetViewWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetViewWidthPx() const
{
    return (std::max) (0, (int) (m_boundsDip.right - m_boundsDip.left) - m_scaler.ToPx (s_kScrollbarWidthDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::KeepCaretInView
//
//  Scrolls sideways the least that shows the caret's cell in the active
//  column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::KeepCaretInView()
{
    int   index = (int) (m_caret % (uint64_t) m_bytesPerRow);
    bool  text  = (m_activeColumn == Column::Text) || !m_showValues;
    int   cell  = text ? (GetColumnStartCell (Column::Text) + index) : (GetColumnStartCell (Column::Hex) + GetByteCellInRow (index));
    int   wide  = text ? 1 : ((m_format == ValueFormat::Hex) ? 2 : GetValueCells());
    int   left  = m_scaler.ToPx (m_padDip) + (cell * m_cellWidthDip);
    int   right = left + (wide * m_cellWidthDip);
    int   view  = GetViewWidthPx();



    if (left < m_leftPx)
    {
        SetLeftPx (left - m_scaler.ToPx (m_padDip));
    }
    else if (right > (m_leftPx + view))
    {
        SetLeftPx (right - view);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetValueCells
//
//  Two digits a byte in hex; in decimal, the digits of the largest value the
//  bytes hold, and a sign's cell when signed.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetValueCells() const
{
    int  digits = 0;



    if (m_format == ValueFormat::Hex)
    {
        return m_grouping * 2;
    }

    switch (m_grouping)
    {
    case 1:  digits = 3;  break;
    case 2:  digits = 5;  break;
    case 4:  digits = 10; break;
    default: digits = 20; break;
    }

    return (m_format == ValueFormat::Signed) ? (digits + 1) : digits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::FormatValue
//
//  A value's text, right-aligned to its cells. `present` is how many of its
//  bytes the source has, which is short only for the last value of the file:
//  in hex the missing bytes are blanks, and in decimal they count as zero.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexView::FormatValue (uint64_t value, int present) const
{
    std::wstring  out;
    int           cells    = GetValueCells();
    int64_t       asSigned = (int64_t) value;



    if (m_format == ValueFormat::Hex)
    {
        for (int index = m_grouping - 1; index >= 0; index--)
        {
            if (index < present)
            {
                out.push_back (GetHexDigit ((int) ((value >> (index * 8 + 4)) & 0xF)));
                out.push_back (GetHexDigit ((int) ((value >> (index * 8)) & 0xF)));
            }
            else
            {
                out.append (L"  ");
            }
        }

        return out;
    }

    if (m_format == ValueFormat::Signed)
    {
        if ((present < 8) && (((value >> (present * 8 - 1)) & 1) != 0))
        {
            asSigned = (int64_t) (value | (~0ull << (present * 8)));
        }

        out = std::to_wstring (asSigned);
    }
    else
    {
        out = std::to_wstring (value);
    }

    if ((int) out.size() < cells)
    {
        out.insert (0, (size_t) (cells - (int) out.size()), L' ');
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SnapToValue
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::SnapToValue (uint64_t offset, bool toEnd) const
{
    uint64_t  start = offset - (offset % (uint64_t) m_grouping);



    return toEnd ? (std::min) (start + (uint64_t) m_grouping - 1, GetLastOffset()) : start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetValueSelectionRect
//
//  A selected value's fill reaches halfway into the space on each side, so a
//  run of values reads as one band.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexView::GetValueSelectionRect (uint64_t first, const RECT & cell) const
{
    RECT      rect   = cell;
    int       margin = m_scaler.ToPx (s_kSelectionMarginDip);
    uint64_t  perRow = (uint64_t) m_bytesPerRow;



    rect.left  -= m_cellWidthDip / 2;
    rect.right += m_cellWidthDip - (m_cellWidthDip / 2);

    if ((first < perRow) || !IsByteSelected (first - perRow))
    {
        rect.top -= margin;
    }

    if (!IsByteSelected (first + perRow))
    {
        rect.bottom += margin;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetByteColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexView::GetByteColor (const IDxuiTheme & theme, uint8_t mark) const
{
    uint32_t  argb = theme.Foreground();



    if (m_markColor)
    {
        uint32_t  marked = argb;

        if (m_markColor (mark, marked))
        {
            argb = marked;
        }
    }

    return argb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetCellSizeDip
//
//  One character cell of the host's fixed-width face. Every column position
//  is a whole number of cells, so hex digits and characters stay aligned down
//  the view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetCellSizeDip (int widthDip, int heightDip)
{
    assert (widthDip > 0 && heightDip > 0);

    m_cellWidthDip  = (std::max) (widthDip,  0);
    m_cellHeightDip = (std::max) (heightDip, 0);

    RecomputeRowWidth();
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

    if (offset == m_caret)
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
//  Each value's cells, and one cell between values; nothing without values.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetHexCells() const
{
    if (!m_showValues)
    {
        return 0;
    }

    return (GetGroupCount() * (GetValueCells() + 1)) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetByteCellInRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetByteCellInRow (int indexInRow) const
{
    int  value = indexInRow / m_grouping;
    int  start = value * (GetValueCells() + 1);



    //  A hex value is little-endian, so its first byte's digits come last.
    if (m_format == ValueFormat::Hex)
    {
        start += (m_grouping - 1 - (indexInRow % m_grouping)) * 2;
    }

    return start;
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
        return m_showValues ? (hexStart + GetHexCells() + kGutterCells) : hexStart;
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
    LONG      left  = m_boundsDip.left + m_scaler.ToPx (m_padDip) + (LONG) (cellX * m_cellWidthDip) - m_leftPx;



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
        if (!m_showValues)
        {
            return RECT{};
        }

        //  A decimal value has no digits of its own for each byte, so every
        //  byte in it is the whole value.
        return GetCellRect (GetColumnStartCell (Column::Hex) + GetByteCellInRow (index), row,
                            (m_format == ValueFormat::Hex) ? 2 : GetValueCells());
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
    cellX = (int) ((clientDip.x - m_boundsDip.left - m_scaler.ToPx (m_padDip) + m_leftPx) / cellW);

    if (rowUp >= GetRowCap())
    {
        return result;
    }

    row = m_topRow + (uint64_t) rowUp;

    if (row >= GetRowCount())
    {
        return result;
    }

    if (m_showValues && (cellX >= hexStart) && (cellX < (hexStart + GetHexCells())))
    {
        int  inHex  = cellX - hexStart;
        int  each   = GetValueCells() + 1;
        int  value  = (std::min) (inHex / each, GetGroupCount() - 1);
        int  within = inHex - (value * each);

        //  In hex a point lands on the byte whose digits it is over, counting
        //  from the right since the value is little-endian; in decimal, on the
        //  value's first byte.
        index = value * m_grouping;

        if (m_format == ValueFormat::Hex)
        {
            index += m_grouping - 1 - ((std::min) (within, (m_grouping * 2) - 1) / 2);
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
    m_hasSelection = true;

    if (column != Column::None)
    {
        m_activeColumn = column;
    }

    NotifySelectionChanged();
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
        SelectByte (offset, m_activeColumn);
        return;
    }

    m_caret = offset;

    NotifySelectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SelectAll
//
//  Every byte, which is the same run whichever column the user is in. What
//  differs is what it means: in the hex column Ctrl+A then Ctrl+C yields the
//  digits, in the text column the characters.
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
    m_hasSelection = true;

    NotifySelectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ClearSelection()
{
    bool  had = m_hasSelection;



    m_hasSelection = false;
    m_anchor       = 0;
    m_caret        = 0;

    if (had)
    {
        NotifySelectionChanged();
    }
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
//  DxuiHexView::SetActiveColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetActiveColumn (Column column)
{
    if (column != Column::None)
    {
        m_activeColumn = column;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GoToOffset
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::GoToOffset (uint64_t offset)
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    if (offset >= bytes)
    {
        return;
    }

    SelectByte (offset, m_activeColumn);
    EnsureByteVisible (offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionText
//
//  The selection in the active column's format: hex digits or characters. A
//  single Copy command covers both, based on the active column rather than a
//  separate menu command for each format.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexView::GetSelectionText() const
{
    uint64_t  first = GetSelectionFirst();
    uint64_t  count = GetSelectionCount();



    if (count == 0)
    {
        return std::wstring();
    }

    return (m_activeColumn == Column::Text) ? GetTextFor (first, count)
                                            : GetHexFor  (first, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::CopySelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::CopySelection() const
{
    DxuiClipboard::SetText (m_hwnd, GetSelectionText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::NotifySelectionChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::NotifySelectionChanged()
{
    if (m_onSelectionChanged)
    {
        m_onSelectionChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetLastOffset
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetLastOffset() const
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    return (bytes > 0) ? (bytes - 1) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::MoveCaretTo
//
//  Keyboard movement does not change the active column, so moving the caret
//  out of a run selected in the text column does not switch to the hex column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::MoveCaretTo (uint64_t offset, bool extend)
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;
    uint64_t  last  = GetLastOffset();



    if (bytes == 0)
    {
        return;
    }

    offset = (std::min) (offset, last);

    if (extend)
    {
        ExtendSelectionTo (offset);
    }
    else
    {
        SelectByte (offset, m_activeColumn);
    }

    EnsureByteVisible (offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::OnMouse  (IDxuiControl override)
//
//  A press selects the byte under it, or extends the run when Shift is held,
//  and holding the button drags the moving end across either column.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::OnMouse (const DxuiMouseEvent & ev)
{
    HitResult  hit;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left && IsScrollbarVisible()
            && m_vertScroll.HitTest (ev.positionDip.x, ev.positionDip.y))
        {
            SyncScrollbar();
            m_vertScroll.OnMouseDown (ev.positionDip.x, ev.positionDip.y);
            ScrollToBarPos();
            return true;
        }

        if (ev.button == DxuiMouseButton::Left && IsHorzScrollbarVisible()
            && m_horzScroll.HitTest (ev.positionDip.x, ev.positionDip.y))
        {
            SyncScrollbar();
            m_horzScroll.OnMouseDown (ev.positionDip.x, ev.positionDip.y);
            ScrollToBarPos();
            return true;
        }

        hit = HitTestPoint (ev.positionDip);

        if (!hit.hit)
        {
            return false;
        }

        //  A right-click inside the selection leaves it unchanged; outside
        //  it, the clicked byte is selected. Then the host's menu callback
        //  runs.
        if (ev.button == DxuiMouseButton::Right)
        {
            SetActiveColumn (hit.column);

            if (!IsByteSelected (hit.offset))
            {
                SelectByte (hit.offset, hit.column);
            }

            if (m_onContextMenu)
            {
                m_onContextMenu (ev.positionDip);
            }

            return true;
        }

        if (ev.button != DxuiMouseButton::Left)
        {
            return false;
        }

        SetActiveColumn (hit.column);

        if (ev.shift)
        {
            ExtendSelectionTo (hit.offset);
        }
        else if ((hit.column == Column::Hex) && (m_grouping > 1))
        {
            //  In the value column a press takes the whole value under it.
            SelectByte (SnapToValue (hit.offset, false), hit.column);
            ExtendSelectionTo (SnapToValue (hit.offset, true));
        }
        else
        {
            SelectByte (hit.offset, hit.column);
        }

        m_dragging = true;

        return true;

    case DxuiMouseEventKind::Move:
        if (m_vertScroll.IsDragging() || m_horzScroll.IsDragging())
        {
            m_vertScroll.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
            m_horzScroll.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
            ScrollToBarPos();
            return true;
        }

        if (!m_dragging)
        {
            return false;
        }

        hit = HitTestPoint (ev.positionDip);

        if (hit.hit)
        {
            ExtendSelectionTo (((hit.column == Column::Hex) && (m_grouping > 1)) ? SnapToValue (hit.offset, hit.offset >= m_anchor)
                                                                                 : hit.offset);
            EnsureByteVisible (hit.offset);
        }

        return true;

    case DxuiMouseEventKind::Up:
        if (m_horzScroll.IsDragging())
        {
            return m_horzScroll.OnMouseUp();
        }

        if (m_vertScroll.IsDragging())
        {
            return m_vertScroll.OnMouseUp();
        }

        if (!m_dragging)
        {
            return false;
        }

        m_dragging = false;

        return true;

    case DxuiMouseEventKind::Wheel:
        if (ev.wheelHorizontal)
        {
            SetLeftPx (m_leftPx + (int) (ev.wheelDelta * (float) (kWheelRows * m_cellWidthDip)));
            return IsHorzScrollbarVisible();
        }

        ScrollRows (-(int64_t) (ev.wheelDelta * (float) kWheelRows));

        return true;

    default:
        break;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::OnKey  (IDxuiControl override)
//
//  The arrows move by a byte or a row, Home and End move to the ends of the
//  row (with Ctrl, of the whole source), Page Up and Page Down move by the
//  visible row count, and Shift with any of them extends the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::OnKey (const DxuiKeyEvent & ev)
{
    uint64_t  caret  = m_caret;
    uint64_t  perRow = (uint64_t) m_bytesPerRow;
    uint64_t  page   = (uint64_t) (std::max) (GetRowCap(), 1) * perRow;
    uint64_t  last   = GetLastOffset();
    bool      extend = ev.shift;



    if ((ev.kind != DxuiKeyEventKind::Down) || (GetRowCount() == 0))
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_LEFT:
        MoveCaretTo ((caret > 0) ? (caret - 1) : 0, extend);
        return true;

    case VK_RIGHT:
        MoveCaretTo (caret + 1, extend);
        return true;

    case VK_UP:
        MoveCaretTo ((caret >= perRow) ? (caret - perRow) : caret, extend);
        return true;

    case VK_DOWN:
        MoveCaretTo (((caret + perRow) <= last) ? (caret + perRow) : caret, extend);
        return true;

    case VK_PRIOR:
        MoveCaretTo ((caret >= page) ? (caret - page) : (caret % perRow), extend);
        return true;

    case VK_NEXT:
        MoveCaretTo (caret + page, extend);
        return true;

    case VK_HOME:
        MoveCaretTo (ev.ctrl ? 0 : (caret - (caret % perRow)), extend);
        return true;

    case VK_END:
        MoveCaretTo (ev.ctrl ? last : (caret - (caret % perRow) + perRow - 1), extend);
        return true;

    case 'A':
    case 'C':
    case 'X':
    case 'V':
        //  Copy and select all are standard commands, so the keystroke and
        //  the menu row run the same code.
        return InvokeCommand (DxuiCommandRouter::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift));

    case VK_TAB:
        //  Tab moves between the columns while another column remains, and
        //  returns false after the last one, so focus moves on to the next
        //  control instead of staying in the view.
        if (ev.ctrl)
        {
            return false;
        }

        if (!ev.shift && (m_activeColumn == Column::Hex))
        {
            SetActiveColumn (Column::Text);
            return true;
        }

        if (ev.shift && (m_activeColumn == Column::Text))
        {
            SetActiveColumn (Column::Hex);
            return true;
        }

        return false;

    default:
        break;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::QueryCommand  (IDxuiControl override)
//
//  The view handles Copy and Select all, in the active column's format. The
//  bytes are read-only here, so Cut and Paste are not handled and pass to the
//  containing control.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    switch (command)
    {
    case DxuiStandardCommand::Copy:
        outEnabled = m_hasSelection;
        return true;

    case DxuiStandardCommand::SelectAll:
        outEnabled = (bytes > 0);
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::InvokeCommand  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;



    if (!QueryCommand (command, enabled))
    {
        return false;
    }

    if (enabled && (command == DxuiStandardCommand::Copy))
    {
        CopySelection();
    }
    else if (enabled)
    {
        SelectAll();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::OnFocusEntered  (IDxuiControl override)
//
//  Focus arriving forward starts at the hex column and backward at the text
//  column, so Shift+Tab from the next control goes to the text column, and
//  another Shift+Tab to the hex column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::OnFocusEntered (bool forward)
{
    m_activeColumn = forward ? Column::Hex : Column::Text;
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

    RecomputeRowWidth();
    ClampTopRow();
    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SyncScrollbar()
{
    int         barW = m_scaler.ToPx (s_kScrollbarWidthDip);
    SCROLLINFO  info = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.top + GetHeightDip() });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (int) (std::min) (GetRowCount(), (uint64_t) INT_MAX) - 1;
    info.nPage = (UINT) (std::max) (GetRowCap(), 0);
    info.nPos  = (int) (std::min) (m_topRow, (uint64_t) INT_MAX);
    m_vertScroll.SetScrollInfo (info);

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, (std::max) (m_cellWidthDip, 1));
    m_horzScroll.SetTrack (RECT { m_boundsDip.left, m_boundsDip.bottom - barW, m_boundsDip.left + GetViewWidthPx(), m_boundsDip.bottom });

    info.nMax  = (std::max) (GetContentWidthPx() - 1, 0);
    info.nPage = (UINT) GetViewWidthPx();
    info.nPos  = m_leftPx;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ScrollToBarPos
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ScrollToBarPos()
{
    SetTopRow ((uint64_t) (std::max) (m_vertScroll.GetScrollPos(), 0));
    SetLeftPx (m_horzScroll.GetScrollPos());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float    x      = (float) m_boundsDip.left;
    float    y      = (float) m_boundsDip.top;
    float    width  = (float) (m_boundsDip.right - m_boundsDip.left);
    float    height = (float) GetHeightDip();
    int      cap    = 0;
    HRESULT  hr     = S_OK;



    //  The background goes through the painter, whose layer is beneath the
    //  text renderer's, so the scrollbar the painter draws is not covered.
    painter.FillRect (x, y, width, height, theme.ContentBackground());

    EnsureCellSize (text, theme);

    cap = GetRowCap();

    if ((m_source == nullptr) || (cap <= 0))
    {
        return;
    }

    //  The rows stop short of the scrollbar, which paints over nothing.
    if (IsScrollbarVisible())
    {
        width -= (float) m_scaler.ToPx (s_kScrollbarWidthDip);
    }

    hr = text.PushClipRect (x, y, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int row = 0; row < cap; row++)
    {
        uint64_t  index = m_topRow + (uint64_t) row;
        int       count = 0;

        if (index >= GetRowCount())
        {
            break;
        }

        count = ReadRow (index);

        PaintRow (text, theme, index, count);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    SyncScrollbar();

    if (IsScrollbarVisible())
    {
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }

    if (IsHorzScrollbarVisible())
    {
        m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::EnsureCellSize
//
//  Measures the cell size once from the face used to draw the columns. A size
//  the host has already set is left unchanged, so the geometry can be tested
//  without a device.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    if (m_cellWidthDip > 0)
    {
        return;
    }

    //  Eight digits at once, so a face whose advance is not a whole number
    //  of DIPs is rounded once rather than eight times.
    hr = text.MeasureString (L"00000000", m_scaler.ToPxf (font.sizeDip), font.face, width, height);

    if (FAILED (hr) || (width <= 0.0f) || (height <= 0.0f))
    {
        return;
    }

    SetCellSizeDip ((int) ((width / 8.0f) + 0.5f), (int) (height + 0.5f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ReadRow
//
//  The only place bytes are read: one row at a time, into reused buffers, so
//  the cost of a frame depends on the visible rows, not the source size.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::ReadRow (uint64_t row)
{
    uint64_t  start = row * (uint64_t) m_bytesPerRow;
    uint64_t  bytes = m_source->GetByteCount();
    uint64_t  left  = (start < bytes) ? (bytes - start) : 0;
    int       count = (int) (std::min) (left, (uint64_t) m_bytesPerRow);



    if (count <= 0)
    {
        return 0;
    }

    m_rowBytes.resize ((size_t) m_bytesPerRow);
    m_rowMarks.resize ((size_t) m_bytesPerRow);

    m_source->ReadBytes (start, std::span<uint8_t> (m_rowBytes.data(), (size_t) count));
    m_source->ReadMarks (start, std::span<uint8_t> (m_rowMarks.data(), (size_t) count));

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::PaintRow
//
//  The offset, then each byte's digits and its character. Selected bytes are
//  lit in both columns at once, which is the whole point of selecting bytes
//  rather than characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::PaintRow (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row, int count)
{
    DxuiFontHandle  font     = theme.MonospaceFont();
    RECT            gutter   = GetRowOffsetRect (row);
    int             digits   = GetOffsetDigits();
    uint64_t        address  = m_originAddress + (row * (uint64_t) m_bytesPerRow);
    std::wstring    label;



    //  The theme's size is in DIPs; the renderer draws in pixels, and the cell
    //  size was measured at this same pixel size.
    font.sizeDip = m_scaler.ToPxf (font.sizeDip);
    for (int digit = digits - 1; digit >= 0; digit--)
    {
        label.push_back (GetHexDigit ((int) ((address >> (digit * 4)) & 0xF)));
    }

    DrawCell (text, gutter, label.c_str(), theme.ForegroundMuted(), font);

    //  The value column, a value at a time.
    for (int first = 0; m_showValues && (first < count); first += m_grouping)
    {
        uint64_t  offset   = (row * (uint64_t) m_bytesPerRow) + (uint64_t) first;
        int       present  = (std::min) (m_grouping, count - first);
        uint64_t  value    = 0;
        bool      selected = false;
        uint32_t  argb     = GetByteColor (theme, m_rowMarks[(size_t) first]);
        RECT      cell     = GetCellRect (GetColumnStartCell (Column::Hex) + ((first / m_grouping) * (GetValueCells() + 1)),
                                          row, GetValueCells());

        for (int index = 0; index < present; index++)
        {
            value   |= (uint64_t) m_rowBytes[(size_t) (first + index)] << (index * 8);
            selected = selected || IsByteSelected (offset + (uint64_t) index);
        }

        if (selected)
        {
            FillCell (text, GetValueSelectionRect (offset, cell), theme.SelectionBackground());
        }

        DrawCell (text, cell, FormatValue (value, present).c_str(), argb, font);
    }

    //  The text column, a byte at a time.
    for (int index = 0; index < count; index++)
    {
        uint64_t      offset  = (row * (uint64_t) m_bytesPerRow) + (uint64_t) index;
        RECT          txtRect = GetByteRect (offset, Column::Text);
        uint32_t      argb    = GetByteColor (theme, m_rowMarks[(size_t) index]);
        std::wstring  charOf  = { GetCharFor (m_rowBytes[(size_t) index]) };

        if (IsByteSelected (offset))
        {
            FillCell (text, GetSelectionCellRect (offset, index, txtRect, false), theme.SelectionBackground());
        }

        DrawCell (text, txtRect, charOf.c_str(), argb, font);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionCellRect
//
//  A selected byte's fill reaches halfway into the gap on each side, so a run
//  reads as one band with the spacing inside it and a margin past its ends.
//  A byte at the row's edge borrows the gap from its other side, and a text
//  column cell, which has no gap, keeps its width.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiHexView::GetSelectionCellRect (uint64_t offset, int index, const RECT & cell, bool hexColumn) const
{
    RECT      rect     = cell;
    int       gapLeft  = 0;
    int       gapRight = 0;
    int       margin   = m_scaler.ToPx (s_kSelectionMarginDip);
    uint64_t  perRow   = (uint64_t) m_bytesPerRow;



    if (hexColumn && index + 1 < m_bytesPerRow)
    {
        gapRight = GetByteRect (offset + 1, Column::Hex).left - cell.right;
    }

    if (hexColumn && index > 0)
    {
        gapLeft = cell.left - GetByteRect (offset - 1, Column::Hex).right;
    }

    gapRight = (index + 1 < m_bytesPerRow) ? gapRight : gapLeft;
    gapLeft  = (index > 0) ? gapLeft : gapRight;

    rect.left  -= gapLeft / 2;
    rect.right += gapRight - gapRight / 2;

    //  A run's top and bottom edges reach a little past the characters, but
    //  not where the neighboring row is selected too, whose text would be
    //  covered.
    if (offset < perRow || !IsByteSelected (offset - perRow))
    {
        rect.top -= margin;
    }

    if (!IsByteSelected (offset + perRow))
    {
        rect.bottom += margin;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::DrawCell
//
//  All text here starts at a cell boundary in a fixed-width face, so a cell
//  run is always left-aligned, unwrapped, and exactly as wide as its
//  characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::DrawCell (IDxuiTextRenderer & text, const RECT & rect, const wchar_t * chars, uint32_t argb, const DxuiFontHandle & font)
{
    HRESULT  hr = S_OK;



    //  The box is a cell wider than the characters: a glyph whose ink runs past
    //  its advance, or a width rounded down to whole pixels, is otherwise
    //  cut off at its right edge.
    hr = text.DrawString (chars,
                          (float) rect.left,
                          (float) rect.top,
                          (float) (rect.right - rect.left) + (float) (rect.bottom - rect.top),
                          (float) (rect.bottom - rect.top),
                          argb,
                          font.sizeDip,
                          font.face,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Top,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::FillCell
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::FillCell (IDxuiTextRenderer & text, const RECT & rect, uint32_t argb)
{
    HRESULT  hr = S_OK;



    hr = text.FillRect ((float) rect.left,
                        (float) rect.top,
                        (float) (rect.right - rect.left),
                        (float) (rect.bottom - rect.top),
                        argb);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetHexDigit
//
////////////////////////////////////////////////////////////////////////////////

wchar_t DxuiHexView::GetHexDigit (int value)
{
    return (value < 10) ? (wchar_t) (L'0' + value) : (wchar_t) (L'A' + value - 10);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetCharFor
//
//  Anything the face cannot show as itself becomes a period, so the text
//  column stays one cell a byte and the two columns keep their rows.
//
////////////////////////////////////////////////////////////////////////////////

wchar_t DxuiHexView::GetCharFor (uint8_t byte) const
{
    uint8_t  value = (m_encoding == TextEncoding::AppleHighBit) ? (uint8_t) (byte & 0x7F) : byte;



    return ((value >= 0x20) && (value < 0x7F)) ? (wchar_t) value : L'.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetTextFor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexView::GetTextFor (uint64_t offset, uint64_t count) const
{
    std::wstring          out;
    std::vector<uint8_t>  bytes;



    if ((m_source == nullptr) || (count == 0))
    {
        return out;
    }

    bytes.resize ((size_t) count);
    m_source->ReadBytes (offset, std::span<uint8_t> (bytes.data(), bytes.size()));

    for (uint8_t byte : bytes)
    {
        out.push_back (GetCharFor (byte));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetHexFor
//
//  The digits spaced by the grouping in force, counted from the first byte
//  asked for, so a copy of a selection reads the way the column does.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexView::GetHexFor (uint64_t offset, uint64_t count) const
{
    std::wstring          out;
    std::vector<uint8_t>  bytes;



    if ((m_source == nullptr) || (count == 0))
    {
        return out;
    }

    bytes.resize ((size_t) count);
    m_source->ReadBytes (offset, std::span<uint8_t> (bytes.data(), bytes.size()));

    //  Decimal values copy as the numbers shown, a space between each.
    if (m_format != ValueFormat::Hex)
    {
        for (size_t first = 0; first < bytes.size(); first += (size_t) m_grouping)
        {
            uint64_t      value   = 0;
            int           present = (int) (std::min) ((size_t) m_grouping, bytes.size() - first);
            std::wstring  shown;

            for (int index = 0; index < present; index++)
            {
                value |= (uint64_t) bytes[first + (size_t) index] << (index * 8);
            }

            shown = FormatValue (value, present);
            shown.erase (0, shown.find_first_not_of (L' '));

            out += out.empty() ? shown : (L" " + shown);
        }

        return out;
    }

    for (size_t index = 0; index < bytes.size(); index++)
    {
        out.push_back (GetHexDigit ((bytes[index] >> 4) & 0xF));
        out.push_back (GetHexDigit (bytes[index] & 0xF));
    }

    return out;
}
