#include "Pch.h"

#include "DxuiHexView.h"

#include "Core/DxuiClipboard.h"
#include "Widgets/DxuiContextMenu.h"
#include "Window/DxuiHwndSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::BuildContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::BuildContextMenu (std::vector<DxuiPopupMenuItem> & items)
{
    std::vector<DxuiPopupMenuItem>  columns;
    std::shared_ptr<DxuiCommand>    parent;



    //  A choice, checked when the view is set to it, applying and reporting.
    auto  choice = [this] (const wchar_t * label, std::function<bool()> checked, std::function<void()> apply)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = label;
        command->radio     = true;
        command->isChecked = std::move (checked);
        command->dispatch  = [this, apply = std::move (apply)]()
        {
            apply();

            if (m_onSettings)
            {
                m_onSettings();
            }
        };

        return DxuiPopupMenuItem::ForCommand (command);
    };

    items.push_back (choice (L"Show &text only", [this]() { return !m_showValues; },
                                                 [this]() { SetShowValues (!m_showValues); }));

    for (int bytes : { 1, 2, 4 })
    {
        static constexpr const wchar_t *  s_kLabels[] = { L"&1-byte integer", L"&2-byte integer", nullptr, L"&4-byte integer" };

        items.push_back (choice (s_kLabels[bytes - 1], [this, bytes]() { return m_showValues && m_grouping == bytes; },
                                                       [this, bytes]() { if (SetGrouping (bytes)) { SetShowValues (true); } }));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (ValueFormat format : { ValueFormat::Hex, ValueFormat::Signed, ValueFormat::Unsigned })
    {
        const wchar_t *  label = (format == ValueFormat::Hex) ? L"&Hexadecimal" : (format == ValueFormat::Signed) ? L"&Signed" : L"&Unsigned";

        items.push_back (choice (label, [this, format]() { return m_format == format; },
                                        [this, format]() { SetShowValues (true); SetValueFormat (format); }));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int count : { 0, 1, 2, 4, 8, 16 })
    {
        static constexpr const wchar_t *  s_kCounts[] = { L"&Auto", L"&1", L"&2", L"&4", L"&8", L"1&6" };
        int                               index       = (count == 0) ? 0 : (count == 1) ? 1 : (count == 2) ? 2 : (count == 4) ? 3 : (count == 8) ? 4 : 5;

        columns.push_back (choice (s_kCounts[index], [this, count]() { return m_layout.m_columns == count; },
                                                     [this, count]() { SetColumns (count); }));
    }

    parent        = std::make_shared<DxuiCommand>();
    parent->label = L"Columns";
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (columns)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    {
        std::shared_ptr<DxuiCommand>  copy = std::make_shared<DxuiCommand>();

        copy->label       = L"&Copy";
        copy->accelerator = L"Ctrl+C";
        copy->isEnabled   = [this]() { return m_layout.m_hasSelection && GetSelectionCount() <= kMaxCopyBytes; };
        copy->dispatch    = [this]() { CopySelection(); };
        items.push_back (DxuiPopupMenuItem::ForCommand (copy));
    }

    if (m_onBuildMenu)
    {
        m_onBuildMenu (items);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ShowContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ShowContextMenu (POINT atDip)
{
    DxuiHwndSource *                host = DxuiHwndSource::FromHwnd (m_hwnd);
    std::vector<DxuiPopupMenuItem>  items;



    if (host == nullptr)
    {
        return;
    }

    BuildContextMenu (items);
    DxuiContextMenu::Show (*host, atDip.x, atDip.y, std::move (items));
}





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
    m_layout.ResetLineIndex();
    m_layout.m_topRow = 0;

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetOriginAddress
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetOriginAddress (uint64_t address)
{
    m_originAddress = address;

    m_layout.RecomputeRowWidth();
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

    m_bytesPerRow      = count;
    m_layout.m_columns = s_kFixedRowWidth;

    //  A grouping the new row width cannot divide falls back to single bytes,
    //  which every row width divides.
    if ((m_bytesPerRow % m_grouping) != 0)
    {
        m_grouping = kDefaultGrouping;
    }

    m_layout.ClampTopRow();
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
    if ((m_layout.m_columns == s_kFixedRowWidth) && ((m_bytesPerRow % bytesPerGroup) != 0))
    {
        return false;
    }

    m_grouping = bytesPerGroup;

    m_layout.RecomputeRowWidth();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetZoom
//
//  The cells are measured again at the new size on the next paint.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetZoom (float zoom)
{
    if (zoom == m_painter.m_zoom || zoom <= 0.0f)
    {
        return;
    }

    m_painter.m_zoom         = zoom;
    m_cellWidthDip           = 0;
    m_layout.m_cellHeightDip = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetColumns (int valuesPerRow)
{
    assert (valuesPerRow >= 0);

    m_layout.m_columns = (std::max) (valuesPerRow, 0);

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetValueFormat
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetValueFormat (ValueFormat format)
{
    m_format = format;

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetShowValues
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetShowValues (bool show)
{
    m_showValues = show;

    m_layout.ResetLineIndex();
    m_layout.RecomputeRowWidth();
    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetBreakLines
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetBreakLines (bool breakLines)
{
    m_breakLines = breakLines;

    m_layout.ResetLineIndex();
    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetLeftPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetLeftPx (int leftPx)
{
    m_layout.m_leftPx = (std::max) (0, (std::min) (leftPx, m_layout.GetContentWidthPx() - m_layout.GetViewWidthPx()));
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
    int   index = m_layout.GetColumnOfOffset (m_layout.m_caret);
    bool  text  = (m_activeColumn == Column::Text) || !m_showValues;
    int   cell  = text ? (m_layout.GetColumnStartCell (Column::Text) + index) : (m_layout.GetColumnStartCell (Column::Hex) + m_layout.GetByteCellInRow (index));
    int   wide  = text ? 1 : ((m_format == ValueFormat::Hex) ? 2 : m_layout.GetValueCells());
    int   left  = m_scaler.ToPx (m_layout.m_padDip) + (cell * m_cellWidthDip);
    int   right = left + (wide * m_cellWidthDip);
    int   view  = m_layout.GetViewWidthPx();



    if (left < m_layout.m_leftPx)
    {
        SetLeftPx (left - m_scaler.ToPx (m_layout.m_padDip));
    }
    else if (right > (m_layout.m_leftPx + view))
    {
        SetLeftPx (right - view);
    }
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

    m_cellWidthDip           = (std::max) (widthDip,  0);
    m_layout.m_cellHeightDip = (std::max) (heightDip, 0);

    m_layout.RecomputeRowWidth();
    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetTopRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetTopRow (uint64_t row)
{
    m_layout.m_topRow = (std::min) (row, m_layout.GetMaxTopRow());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ScrollRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ScrollRows (int64_t delta)
{
    uint64_t  top = m_layout.m_topRow;



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
    uint64_t  row = m_layout.GetRowOfOffset (offset);
    int       cap = m_layout.GetRowCap();



    if (cap <= 0)
    {
        return;
    }

    if (offset == m_layout.m_caret)
    {
        KeepCaretInView();
    }

    if (row < m_layout.m_topRow)
    {
        SetTopRow (row);
    }
    else if (row >= (m_layout.m_topRow + (uint64_t) cap))
    {
        SetTopRow (row - (uint64_t) cap + 1);
    }
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

    m_anchor                = offset;
    m_layout.m_caret        = offset;
    m_layout.m_hasSelection = true;

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

    if (!m_layout.m_hasSelection)
    {
        SelectByte (offset, m_activeColumn);
        return;
    }

    m_layout.m_caret = offset;

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

    m_anchor                = 0;
    m_layout.m_caret        = bytes - 1;
    m_layout.m_hasSelection = true;

    NotifySelectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ClearSelection()
{
    bool  had = m_layout.m_hasSelection;



    m_layout.m_hasSelection = false;
    m_anchor                = 0;
    m_layout.m_caret        = 0;

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
    return (std::min) (m_anchor, m_layout.m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionLast
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetSelectionLast() const
{
    return (std::max) (m_anchor, m_layout.m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetSelectionCount() const
{
    if (!m_layout.m_hasSelection)
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
    return m_layout.m_hasSelection
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

    //  With the values hidden, the text column is the only one there is.
    return (m_activeColumn == Column::Text || !m_showValues) ? m_painter.GetTextFor (first, count)
                                                             : m_painter.GetHexFor  (first, count);
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

        hit = m_layout.HitTestPoint (ev.positionDip);

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
            else
            {
                ShowContextMenu (ev.positionDip);
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
            m_pressFirst = m_layout.SnapToValue (hit.offset, false);
            m_pressLast  = m_layout.SnapToValue (hit.offset, true);

            SelectByte (m_pressFirst, hit.column);
            ExtendSelectionTo (m_pressLast);
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

        hit = m_layout.HitTestPoint (ev.positionDip);

        //  Back from the value the press took, the anchor is that value's last
        //  byte, so the selection covers all of it.
        if (hit.hit && (hit.column == Column::Hex) && (m_grouping > 1) && !ev.shift)
        {
            m_anchor = (hit.offset >= m_pressFirst) ? m_pressFirst : m_pressLast;
            ExtendSelectionTo (m_layout.SnapToValue (hit.offset, hit.offset >= m_pressFirst));
            EnsureByteVisible (hit.offset);
        }
        else if (hit.hit)
        {
            ExtendSelectionTo (hit.offset);
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
        //  A precision touchpad sends part notches, which add up until they
        //  move a whole row or pixel.
        if (ev.wheelHorizontal)
        {
            m_wheelPx += ev.wheelDelta * (float) (kWheelRows * m_cellWidthDip);
            SetLeftPx (m_layout.m_leftPx + (int) m_wheelPx);
            m_wheelPx -= (float) (int) m_wheelPx;
            return IsHorzScrollbarVisible();
        }

        m_wheelRows += ev.wheelDelta * (float) kWheelRows;
        ScrollRows (-(int64_t) m_wheelRows);
        m_wheelRows -= (float) (int64_t) m_wheelRows;

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
    uint64_t  caret  = m_layout.m_caret;
    uint64_t  perRow = (uint64_t) m_bytesPerRow;
    uint64_t  page   = (uint64_t) (std::max) (m_layout.GetRowCap(), 1) * perRow;
    uint64_t  last   = GetLastOffset();
    bool      extend = ev.shift;



    if ((ev.kind != DxuiKeyEventKind::Down) || (m_layout.GetRowCount() == 0))
    {
        return false;
    }

    //  Line mode's rows are not a fixed number of bytes, so the keys that move
    //  by rows find the row above or below instead of subtracting a width.
    if (IsLineMode() && (ev.vk == VK_UP || ev.vk == VK_DOWN || ev.vk == VK_PRIOR || ev.vk == VK_NEXT || ev.vk == VK_HOME || ev.vk == VK_END))
    {
        MoveCaretTo (m_layout.GetLineModeTarget (ev.vk, ev.ctrl), extend);
        return true;
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
        if (ev.ctrl || !m_showValues)
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
        //  A copy is built in memory, so a selection past the cap is not
        //  copied at all rather than exhausting memory on a huge file.
        outEnabled = m_layout.m_hasSelection && GetSelectionCount() <= kMaxCopyBytes;
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
    m_activeColumn = (forward && m_showValues) ? Column::Hex : Column::Text;
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

    m_layout.RecomputeRowWidth();
    m_layout.ClampTopRow();
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
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.top + m_layout.GetHeightDip() });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (int) (std::min) (m_layout.GetRowCount(), (uint64_t) INT_MAX) - 1;
    info.nPage = (UINT) (std::max) (m_layout.GetRowCap(), 0);
    info.nPos  = (int) (std::min) (m_layout.m_topRow, (uint64_t) INT_MAX);
    m_vertScroll.SetScrollInfo (info);

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, (std::max) (m_cellWidthDip, 1));
    m_horzScroll.SetTrack (RECT { m_boundsDip.left, m_boundsDip.bottom - barW, m_boundsDip.left + m_layout.GetViewWidthPx(), m_boundsDip.bottom });

    info.nMax  = (std::max) (m_layout.GetContentWidthPx() - 1, 0);
    info.nPage = (UINT) m_layout.GetViewWidthPx();
    info.nPos  = m_layout.m_leftPx;
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
    float    height = (float) m_layout.GetHeightDip();
    int      cap    = 0;
    HRESULT  hr     = S_OK;



    //  The background goes through the painter, whose layer is beneath the
    //  text renderer's, so the scrollbar the painter draws is not covered.
    painter.FillRect (x, y, width, height, theme.ContentBackground());

    m_painter.EnsureCellSize (text, theme);

    cap = m_layout.GetRowCap();

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
        uint64_t  index = m_layout.m_topRow + (uint64_t) row;
        int       count = 0;

        if (index >= m_layout.GetRowCount())
        {
            break;
        }

        count = m_painter.ReadRow (index);

        m_painter.PaintRow (text, theme, index, count);
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
//  DxuiHexView::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiHexView::GetScrollbarMenu (POINT pt)
{
    SyncScrollbar();

    m_vertScroll.SetOnScroll ([this] (int, int) { ScrollToBarPos(); });
    m_horzScroll.SetOnScroll ([this] (int, int) { ScrollToBarPos(); });

    if (IsScrollbarVisible() && m_vertScroll.HitTest (pt.x, pt.y))
    {
        return m_vertScroll.BuildContextMenu (pt.x, pt.y);
    }

    if (IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y))
    {
        return m_horzScroll.BuildContextMenu (pt.x, pt.y);
    }

    return {};
}
