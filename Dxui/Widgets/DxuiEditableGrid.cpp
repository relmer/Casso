#include "Pch.h"

#include "DxuiEditableGrid.h"
#include "Theme/DxuiTheme.h"
#include "Render/IDxuiPainter.h"
#include "Core/DxuiEvents.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::DxuiEditableGrid
//
////////////////////////////////////////////////////////////////////////////////

DxuiEditableGrid::DxuiEditableGrid()
{
    m_editor.SetOverText (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::SetGridColumns
//
//  The gripper's column first, hidden when rows cannot be moved, then the
//  grid's.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::SetGridColumns (std::vector<GridColumn> columns)
{
    std::vector<Column>  listColumns;
    Column               gripper;



    gripper.widthDip = kGripperDip;
    gripper.visible  = m_isMovable;
    listColumns.push_back (gripper);

    for (const GridColumn & each : columns)
    {
        listColumns.push_back (each.column);
    }

    m_columns = std::move (columns);
    m_pending.assign (m_columns.size(), std::wstring());

    SetColumns (std::move (listColumns));
    RebuildRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::SetGridRows
//
//  An edit of the hint row goes on in the hint row wherever it now is; an
//  edit of a row no longer there ends.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::SetGridRows (std::vector<GridRow> rows)
{
    bool  isAddEdit = IsEditing() && IsAddRow (m_editRow);
    RECT  box       = {};



    m_rows = std::move (rows);
    RebuildRows();

    if (isAddEdit)
    {
        m_editRow = GetDataRowCount();
    }
    else if (m_editRow > GetDataRowCount() || (m_editRow >= 0 && IsAddRow (m_editRow)))
    {
        m_editRow    = -1;
        m_editColumn = -1;
        m_editor.SetFocused (false);
    }

    if (IsEditing() && TryGetEditorRect (m_editRow, m_editColumn, box))
    {
        m_editor.Layout (box, m_gridScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::RebuildRows
//
//  The hint row has the text typed into it, or else its hint in the first
//  editable column, muted and italic.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::RebuildRows()
{
    std::vector<std::vector<Cell>>  rows;
    std::vector<Cell>               addRow (m_columns.size() + 1);
    bool                            isTyped = std::ranges::any_of (m_pending, [] (const std::wstring & text) { return !text.empty(); });
    int                             first   = GetNextEditColumn (-1, 1);



    for (const GridRow & row : m_rows)
    {
        std::vector<Cell>  cells (m_columns.size() + 1);

        for (size_t c = 0; c < row.cells.size() && c < m_columns.size(); c++)
        {
            cells[c + 1] = row.cells[c];
        }

        rows.push_back (std::move (cells));
    }

    for (size_t c = 0; isTyped && c < m_pending.size(); c++)
    {
        addRow[c + 1].text = m_pending[c];
    }

    if (!isTyped && first >= 0)
    {
        Cell  & hint = addRow[(size_t) first + 1];

        hint.text   = m_addRowText;
        hint.dim    = true;
        hint.face   = DxuiTheme::kBodyFace;
        hint.weight = DxuiFontWeight::Italic;
    }

    rows.push_back (std::move (addRow));
    SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::GetGridColumnAt
//
//  The grid column at x relative to the list, counted from the list's pane
//  lead; -1 for the gripper's column.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiEditableGrid::GetGridColumnAt (int xPx) const
{
    int  right = GetPaneLeadPx() - GetLeftPx();



    for (size_t c = 0; c < GetColumnCount(); c++)
    {
        right += GetColumnEffectiveWidthPx (c);

        if (xPx < right)
        {
            return (int) c - 1;
        }
    }

    return (int) GetColumnCount() - 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::IsRowEditable
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::IsRowEditable (int row) const
{
    return row >= 0 && row < GetDataRowCount() && !m_rows[(size_t) row].isReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::IsColumnEditable
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::IsColumnEditable (int column) const
{
    return column >= 0 && column < (int) m_columns.size() && m_columns[(size_t) column].isEditable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::IsCellEditable
//
//  Every editable column of the hint row; in a data row that is not
//  read-only, each editable column but those of its read-only cells.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::IsCellEditable (int row, int column) const
{
    const std::vector<bool>  * readOnly = nullptr;



    if (!IsColumnEditable (column))
    {
        return false;
    }

    if (IsAddRow (row))
    {
        return true;
    }

    if (!IsRowEditable (row))
    {
        return false;
    }

    readOnly = &m_rows[(size_t) row].readOnlyCells;

    return (size_t) column >= readOnly->size() || !(*readOnly)[(size_t) column];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::DeleteRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::DeleteRow (int row)
{
    if (!CanDeleteRow (row))
    {
        return;
    }

    if (m_editRow == row)
    {
        EndEdit (false);
    }

    if (m_delete)
    {
        m_delete (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::GetEditText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiEditableGrid::GetEditText (int row, int column) const
{
    const GridRow  * gridRow = nullptr;



    if (IsAddRow (row))
    {
        return m_pending[(size_t) column];
    }

    gridRow = &m_rows[(size_t) row];

    if ((size_t) column < gridRow->editTexts.size())
    {
        return gridRow->editTexts[(size_t) column];
    }

    return ((size_t) column < gridRow->cells.size()) ? gridRow->cells[(size_t) column].text : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::TryGetEditorRect
//
//  The box over a cell, in the window's coordinates, moved left by the
//  input's own text padding so its text starts where the cell's does.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::TryGetEditorRect (int row, int column, RECT & outRect) const
{
    RECT  bounds = GetBounds();



    if (!GetCellTextRectPx (row, (size_t) column + 1, outRect))
    {
        return false;
    }

    OffsetRect (&outRect, bounds.left, bounds.top);
    outRect.left -= m_gridScaler.ToPx (DxuiTextInput::kPadLeftDip);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::BeginEdit
//
//  In the list's own face and size, so the text does not move when the box
//  opens over it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::BeginEdit (int row, int column)
{
    RECT  box = {};



    if (!IsCellEditable (row, column))
    {
        return false;
    }

    SetSelectedRow (row);
    EnsureVisible  (row);

    if (!TryGetEditorRect (row, column, box))
    {
        return false;
    }

    m_editRow    = row;
    m_editColumn = column;

    m_editor.SetFont    (IsMonospace() ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace, GetFontSizeDip());
    m_editor.SetText    (GetEditText (row, column));
    m_editor.Layout     (box, m_gridScaler);
    m_editor.SetFocused (true);
    m_editor.SelectAll();

    if (m_begin)
    {
        m_begin (row, column);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::EndEdit
//
//  A canceled edit of the hint row drops everything typed into it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::EndEdit (bool commit)
{
    bool  isEnded = true;



    if (!IsEditing() || m_isEnding)
    {
        return true;
    }

    m_isEnding = true;

    if (commit)
    {
        isEnded = CommitEdit();
    }
    else
    {
        if (IsAddRow (m_editRow))
        {
            m_pending.assign (m_columns.size(), std::wstring());
        }

        m_editRow    = -1;
        m_editColumn = -1;
        m_editor.SetFocused (false);
        ReportError (L"");
        RebuildRows();
    }

    m_isEnding = false;
    return isEnded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::CommitEdit
//
//  The edit closes when the host takes it, or when it changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::CommitEdit()
{
    std::wstring  text   = m_editor.GetText();
    std::wstring  error;
    int           row    = m_editRow;
    int           column = m_editColumn;



    if (m_validate && !m_validate (column, text, error))
    {
        ReportError (error);
        return false;
    }

    if (IsAddRow (row))
    {
        return CommitAddRow (text, -1);
    }

    if (text != GetEditText (row, column) && m_commit && !m_commit (row, column, text, error))
    {
        ReportError (error);
        return false;
    }

    m_editRow    = -1;
    m_editColumn = -1;
    m_editor.SetFocused (false);
    ReportError (L"");
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::CommitAddRow
//
//  The text goes into the hint row. With every required column filled, the
//  row goes to the add callback, and when that returns true the hint row
//  starts over. Otherwise the edit moves to `next`, or closes for -1, with
//  the typed text in place. When the add callback returns false, the box
//  stays open where it is for -1.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::CommitAddRow (const std::wstring & text, int next)
{
    std::wstring  error;
    bool          isComplete = true;



    m_pending[(size_t) m_editColumn] = text;

    for (size_t c = 0; c < m_columns.size(); c++)
    {
        if (m_columns[c].isRequired && m_pending[c].find_first_not_of (L" \t") == std::wstring::npos)
        {
            isComplete = false;
        }
    }

    if (isComplete && m_add && m_add (m_pending, error))
    {
        m_pending.assign (m_columns.size(), std::wstring());
        m_editRow    = -1;
        m_editColumn = -1;
        m_editor.SetFocused (false);
        ReportError (L"");
        RebuildRows();
        return true;
    }

    RebuildRows();

    if (isComplete)
    {
        ReportError (error);

        if (next < 0)
        {
            return false;
        }
    }
    else
    {
        ReportError (L"");
    }

    m_editRow    = -1;
    m_editColumn = -1;
    m_editor.SetFocused (false);

    if (next >= 0)
    {
        BeginEdit (GetDataRowCount(), next);
    }

    return !isComplete;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::MoveEdit
//
//  Tab: the edit is committed and the next editable column of its row, or
//  the one before for Shift+Tab, opens. In the hint row, a row that is
//  complete is added.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::MoveEdit (int step)
{
    int           row    = m_editRow;
    int           next   = GetNextEditColumn (m_editColumn, step);
    std::wstring  text   = m_editor.GetText();
    std::wstring  error;



    if (m_validate && !m_validate (m_editColumn, text, error))
    {
        ReportError (error);
        return false;
    }

    if (IsAddRow (row))
    {
        return CommitAddRow (text, next);
    }

    if (!CommitEdit())
    {
        return false;
    }

    if (next >= 0)
    {
        BeginEdit (row, next);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::GetNextEditColumn
//
//  The next editable column after `column` in the direction of `step`, or -1
//  past the last.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiEditableGrid::GetNextEditColumn (int column, int step) const
{
    for (int c = column + step; c >= 0 && c < (int) m_columns.size(); c += step)
    {
        if (m_columns[(size_t) c].isEditable)
        {
            return c;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::GetNextEmptyColumn
//
//  Where Enter goes next in the hint row: the first empty required column,
//  else the first empty editable column after this one; -1 for none.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiEditableGrid::GetNextEmptyColumn (int column) const
{
    for (size_t c = 0; c < m_columns.size(); c++)
    {
        if ((int) c != column && m_columns[c].isRequired && m_columns[c].isEditable && m_pending[c].empty())
        {
            return (int) c;
        }
    }

    for (int c = GetNextEditColumn (column, 1); c >= 0; c = GetNextEditColumn (c, 1))
    {
        if (m_pending[(size_t) c].empty())
        {
            return c;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::ReportError
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::ReportError (const std::wstring & error)
{
    if (m_error)
    {
        m_error (error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::GetDropRowAt
//
//  The data row under y, the first in view above them and the last below
//  them.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiEditableGrid::GetDropRowAt (int yPx) const
{
    int   row   = HitTestRow (GetPaneLeadPx(), yPx);
    RECT  first = {};



    if (row < 0)
    {
        row = (GetCellTextRectPx (GetTopRow(), 0, first) && yPx < first.top) ? GetTopRow() : GetDataRowCount() - 1;
    }

    return std::clamp (row, 0, std::max (0, GetDataRowCount() - 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  box = {};



    m_gridScaler = scaler;
    m_editor.SetDpi (scaler.GetDpi());
    DxuiListView::Layout (boundsDip, scaler);

    if (IsEditing() && TryGetEditorRect (m_editRow, m_editColumn, box))
    {
        m_editor.Layout (box, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::Paint
//
//  The edit box over its cell, and the grippers of the rows under the
//  pointer, selected or dragged.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_editor.SetTextRenderer (&text);
    m_editor.SetTheme        (&theme);

    DxuiListView::Paint (painter, text, theme);

    if (m_isMovable)
    {
        PaintGrippers (painter, theme);
    }

    if (IsDraggingRow())
    {
        PaintDropLine (painter, theme);
    }

    if (IsEditing())
    {
        m_editor.Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::PaintGrippers
//
//  Two columns of three dots, centered in the gripper's column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::PaintGrippers (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    RECT   bounds = GetBounds();
    float  radius = m_gridScaler.ToPxf (1.0f);
    float  step   = m_gridScaler.ToPxf (4.0f);
    int    rows[] = { GetHoveredRow(), GetSelectedRow(), m_dragRow };



    for (int row : rows)
    {
        RECT   cell = {};
        float  cx   = 0.0f;
        float  cy   = 0.0f;

        if (row < 0 || row >= GetDataRowCount() || !GetCellTextRectPx (row, 0, cell))
        {
            continue;
        }

        cx = (float) (bounds.left + GetPaneLeadPx() - GetLeftPx()) + (float) GetColumnEffectiveWidthPx (0) / 2.0f;
        cy = (float) bounds.top + (float) (cell.top + cell.bottom) / 2.0f;

        for (int dy = -1; dy <= 1; dy++)
        {
            painter.FillCircle (cx - step / 2.0f, cy + step * (float) dy, radius, theme.ForegroundMuted());
            painter.FillCircle (cx + step / 2.0f, cy + step * (float) dy, radius, theme.ForegroundMuted());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::PaintDropLine
//
//  A line where the dragged row would go: over the row it would drop at when
//  it moves up, under it when it moves down.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::PaintDropLine (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    RECT   bounds = GetBounds();
    RECT   cell   = {};
    float  y      = 0.0f;



    if (m_dropRow < 0 || m_dropRow == m_dragRow || !GetCellTextRectPx (m_dropRow, 0, cell))
    {
        return;
    }

    y = (float) bounds.top + (float) ((m_dropRow < m_dragRow) ? cell.top : cell.bottom);

    painter.DrawLine ((float) bounds.left, y, (float) bounds.right, y, m_gridScaler.ToPxf (2.0f), theme.Accent());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::RouteEditorMouse
//
//  The box takes the pointer while it is over it, and a drag it started. A
//  press or a wheel anywhere else commits the edit; when the commit fails,
//  the box stays open and a press inside the grid goes no further.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::RouteEditorMouse (const DxuiMouseEvent & ev)
{
    RECT            bounds = GetBounds();
    RECT            box    = m_editor.GetBounds();
    DxuiMouseEvent  local  = ev;
    bool            inside = ev.positionDip.x >= 0 && ev.positionDip.y >= 0 &&
                             ev.positionDip.x < bounds.right - bounds.left && ev.positionDip.y < bounds.bottom - bounds.top;



    local.positionDip = { ev.positionDip.x + bounds.left, ev.positionDip.y + bounds.top };

    if (PtInRect (&box, local.positionDip) || ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up)
    {
        bool  isOver = PtInRect (&box, local.positionDip) != FALSE;

        m_editor.OnMouse (local);

        if (isOver)
        {
            return true;
        }
    }

    if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel)
    {
        return !EndEdit (true) && inside;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::OnMouse
//
//  A press on a gripper drags its row; a press and release on one cell,
//  not on its checkbox, edits the cell. A cell of the hint row that cannot
//  be edited edits the row's first column that can.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::OnMouse (const DxuiMouseEvent & ev)
{
    int   lx      = ev.positionDip.x;
    int   ly      = ev.positionDip.y;
    RECT  bounds  = GetBounds();
    bool  inside  = lx >= 0 && ly >= 0 && lx < bounds.right - bounds.left && ly < bounds.bottom - bounds.top;
    bool  isLeft  = ev.button == DxuiMouseButton::Left;
    bool  handled = false;
    int   row     = -1;
    int   column  = -1;
    RECT  check   = {};



    if (IsEditing() && RouteEditorMouse (ev))
    {
        return true;
    }

    if (IsDraggingRow())
    {
        if (ev.kind == DxuiMouseEventKind::Move && isLeft)
        {
            m_dropRow = GetDropRowAt (ly);
        }
        else if (ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Move)
        {
            int  from = m_dragRow;
            int  to   = m_dropRow;

            m_dragRow = -1;
            m_dropRow = -1;

            if (to >= 0 && to != from && m_move)
            {
                m_move (from, to);
                SetSelectedRow (to);
            }
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && isLeft && inside)
    {
        row         = HitTestRow (lx, ly);
        column      = GetGridColumnAt (lx);
        m_pressRow  = -1;

        if (m_isMovable && row >= 0 && row < GetDataRowCount() && column < 0)
        {
            m_dragRow = row;
            m_dropRow = row;
            SetSelectedRow (row);
            return true;
        }

        if (row >= 0 && !(GetCheckRectPx (row, (size_t) column + 1, check) && PtInRect (&check, POINT { lx, ly })))
        {
            m_pressRow    = row;
            m_pressColumn = column;
        }
    }

    handled = DxuiListView::OnMouse (ev);

    if (ev.kind == DxuiMouseEventKind::Up && m_pressRow >= 0)
    {
        row    = HitTestRow (lx, ly);
        column = GetGridColumnAt (lx);

        if (row == m_pressRow && column == m_pressColumn)
        {
            if (IsAddRow (row) && !IsColumnEditable (column))
            {
                column = GetNextEditColumn (-1, 1);
            }

            handled = BeginEdit (row, column) || handled;
        }

        m_pressRow = -1;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::GetCursorForPoint
//
//  The point is relative to the grid, as the list takes it. Over a gripper,
//  and while a row is dragged, the cursor is the up-and-down arrow.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiEditableGrid::GetCursorForPoint (POINT localPx) const
{
    int  row = HitTestRow (localPx.x, localPx.y);



    if (IsDraggingRow() || (m_isMovable && row >= 0 && row < GetDataRowCount() && GetGridColumnAt (localPx.x) < 0))
    {
        return IDC_SIZENS;
    }

    return DxuiListView::GetCursorForPoint (localPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::OnKey
//
//  While a cell is edited: Enter commits it (in the hint row, going on to
//  the next empty column), Escape cancels it, Tab and Shift+Tab move
//  along the row, and every other key edits the text. Otherwise F2 and Enter
//  edit the selected row, Delete deletes it, and Insert starts a new row.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::OnKey (const DxuiKeyEvent & ev)
{
    int  selected = GetSelectedRow();
    int  first    = GetNextEditColumn (-1, 1);



    if (IsEditing())
    {
        if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
        {
            if (IsAddRow (m_editRow))
            {
                std::wstring  text = m_editor.GetText();
                std::wstring  error;

                if (m_validate && !m_validate (m_editColumn, text, error))
                {
                    ReportError (error);
                    return true;
                }

                CommitAddRow (text, GetNextEmptyColumn (m_editColumn));
            }
            else
            {
                EndEdit (true);
            }

            return true;
        }

        if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE)
        {
            EndEdit (false);
            return true;
        }

        if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_TAB)
        {
            MoveEdit (ev.shift ? -1 : 1);
            return true;
        }

        m_editor.OnKey (ev);
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_F2 || ev.vk == VK_RETURN) && selected >= 0)
    {
        while (first >= 0 && !IsCellEditable (selected, first))
        {
            first = GetNextEditColumn (first, 1);
        }

        return BeginEdit (selected, first);
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_DELETE && CanDeleteRow (selected))
    {
        DeleteRow (selected);
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_INSERT)
    {
        return BeginEdit (GetDataRowCount(), first);
    }

    return DxuiListView::OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::OnChar
//
//  A character typed while a cell is edited, from a window that sends
//  characters apart from keys, as a dialog does.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiEditableGrid::OnChar (wchar_t ch)
{
    return IsEditing() ? m_editor.OnChar (ch) : DxuiListView::OnChar (ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid::OnFocusChanged
//
//  Leaving the grid commits an open edit.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiEditableGrid::OnFocusChanged (bool focused)
{
    DxuiListView::OnFocusChanged (focused);

    if (!focused && IsEditing())
    {
        EndEdit (true);
    }
}

