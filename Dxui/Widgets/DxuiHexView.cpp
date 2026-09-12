#include "Pch.h"

#include "DxuiHexView.h"

#include "Theme/IDxuiTheme.h"
#include "Render/IDxuiTextRenderer.h"





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
        SelectByte (offset, m_column);
        return;
    }

    m_caret = offset;

    NotifySelectionChanged();
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
    m_column       = Column::None;

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
//  Keyboard movement keeps the column the selection was made in, so walking
//  the caret out of a run made in the text column does not silently turn it
//  into a hex one.
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
        SelectByte (offset, m_column);
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
        if (ev.button != DxuiMouseButton::Left)
        {
            return false;
        }

        hit = HitTestPoint (ev.positionDip);

        if (!hit.hit)
        {
            return false;
        }

        if (ev.shift)
        {
            ExtendSelectionTo (hit.offset);
        }
        else
        {
            SelectByte (hit.offset, hit.column);
        }

        m_dragging = true;

        return true;

    case DxuiMouseEventKind::Move:
        if (!m_dragging)
        {
            return false;
        }

        hit = HitTestPoint (ev.positionDip);

        if (hit.hit)
        {
            ExtendSelectionTo (hit.offset);
            EnsureByteVisible (hit.offset);
        }

        return true;

    case DxuiMouseEventKind::Up:
        if (!m_dragging)
        {
            return false;
        }

        m_dragging = false;

        return true;

    case DxuiMouseEventKind::Wheel:
        if (ev.wheelHorizontal)
        {
            return false;
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
//  The arrows walk a byte or a row, Home and End take the row's ends and take
//  the whole view's ends with Ctrl, the page keys move by what the view shows,
//  and Shift with any of them extends the run instead of starting a new one.
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
        if (!ev.ctrl)
        {
            return false;
        }

        SelectAll();
        return true;

    default:
        break;
    }

    return false;
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
    float    x      = (float) m_boundsDip.left;
    float    y      = (float) m_boundsDip.top;
    float    width  = (float) (m_boundsDip.right - m_boundsDip.left);
    float    height = (float) GetHeightDip();
    int      cap    = 0;
    HRESULT  hr     = S_OK;



    (void) painter;

    hr = text.FillRect (x, y, width, height, theme.Background());
    IGNORE_RETURN_VALUE (hr, S_OK);

    EnsureCellSize (text, theme);

    cap = GetRowCap();

    if ((m_source == nullptr) || (cap <= 0))
    {
        return;
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::EnsureCellSize
//
//  The cell the columns are built on, measured once from the face that will
//  draw them. A host that has already set a size keeps it, which is what lets
//  the geometry be exercised with no device at all.
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
    hr = text.MeasureString (L"00000000", font.sizeDip, font.face, width, height);

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
//  The one place bytes are read. A row at a time, into buffers the view keeps,
//  so what a frame costs follows the rows on screen and not the size of what
//  is being shown.
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



    for (int digit = digits - 1; digit >= 0; digit--)
    {
        label.push_back (GetHexDigit ((int) ((address >> (digit * 4)) & 0xF)));
    }

    DrawCell (text, gutter, label.c_str(), theme.ForegroundMuted(), font);

    for (int index = 0; index < count; index++)
    {
        uint64_t      offset   = (row * (uint64_t) m_bytesPerRow) + (uint64_t) index;
        uint8_t       byte     = m_rowBytes[(size_t) index];
        RECT          hexRect  = GetByteRect (offset, Column::Hex);
        RECT          txtRect  = GetByteRect (offset, Column::Text);
        uint32_t      argb     = theme.Foreground();
        std::wstring  digitsOf = { GetHexDigit ((byte >> 4) & 0xF), GetHexDigit (byte & 0xF) };
        std::wstring  charOf   = { GetCharFor (byte) };

        if (m_markColor)
        {
            uint32_t  marked = argb;

            if (m_markColor (m_rowMarks[(size_t) index], marked))
            {
                argb = marked;
            }
        }

        if (IsByteSelected (offset))
        {
            FillCell (text, hexRect, theme.SelectionBackground());
            FillCell (text, txtRect, theme.SelectionBackground());
        }

        DrawCell (text, hexRect, digitsOf.c_str(), argb, font);

        DrawCell (text, txtRect, charOf.c_str(), argb, font);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::DrawCell
//
//  Everything here is drawn at a cell boundary in a fixed-width face, so a
//  cell run is always left-aligned, unwrapped, and exactly as wide as the
//  characters it holds.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::DrawCell (IDxuiTextRenderer & text, const RECT & rect, const wchar_t * chars, uint32_t argb, const DxuiFontHandle & font)
{
    HRESULT  hr = S_OK;



    hr = text.DrawString (chars,
                          (float) rect.left,
                          (float) rect.top,
                          (float) (rect.right - rect.left),
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

    for (size_t index = 0; index < bytes.size(); index++)
    {
        if ((index > 0) && ((index % (size_t) m_grouping) == 0))
        {
            out.push_back (L' ');
        }

        out.push_back (GetHexDigit ((bytes[index] >> 4) & 0xF));
        out.push_back (GetHexDigit (bytes[index] & 0xF));
    }

    return out;
}
