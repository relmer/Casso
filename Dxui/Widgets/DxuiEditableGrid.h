#pragma once

#include "Pch.h"
#include "DxuiListView.h"
#include "DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGrid
//
//  A list whose rows are edited where they lie, as Visual Studio's grids are:
//  a click on a cell opens an edit box over it, Enter or a click elsewhere
//  commits it and Escape cancels it, and Tab moves to the next cell. The last
//  row is a hint, "Add new range" or the like, that starts a new row; once
//  every required column of the new row has text, the row is offered to the
//  host and, if taken, a new hint row follows it. Unless the host turns it
//  off, each row has a gripper at its left that drags it to another place.
//  Delete removes the selected row.
//
//  The grid has no data of its own: the host sets its rows and columns, and
//  each edit, addition, move and deletion is a callback that changes the
//  host's data and sets the grid's rows again. When a callback returns
//  false, the edit box stays open and the error callback is called with the
//  reason.
//
//  The list's first column is the gripper's, so a grid column c is the
//  list's column c + 1; the grid's own methods take grid columns.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiEditableGrid : public DxuiListView
{
public:
    struct GridColumn
    {
        Column  column;
        bool    isEditable = true;
        bool    isRequired = false;
    };

    struct GridRow
    {
        std::vector<Cell>          cells;       // one per grid column
        std::vector<std::wstring>  editTexts;   // the text an edit of each cell starts with; a cell's own text when absent
        bool                       isReadOnly = false;
        std::vector<bool>          readOnlyCells;   // per grid column; a column past its end can be edited
    };

    //  The row is the grid's row, GetDataRowCount() for the hint row.
    using ValidateFn = std::function<bool (int column, const std::wstring & text, std::wstring & outError)>;
    using CommitFn   = std::function<bool (int row, int column, const std::wstring & text, std::wstring & outError)>;
    using AddFn      = std::function<bool (const std::vector<std::wstring> & texts, std::wstring & outError)>;
    using MoveFn     = std::function<void (int from, int to)>;
    using DeleteFn   = std::function<void (int row)>;
    using ErrorFn    = std::function<void (const std::wstring & error)>;
    using BeginFn    = std::function<void (int row, int column)>;

    static constexpr int  kGripperDip = 16;

    DxuiEditableGrid();
    ~DxuiEditableGrid() override = default;

    void  SetGridColumns (std::vector<GridColumn> columns);
    void  SetGridRows    (std::vector<GridRow> rows);
    void  SetAddRowText  (const std::wstring & text)  { m_addRowText = text; RebuildRows(); }
    void  SetEditorHwnd  (HWND hwnd)                  { m_editor.SetHwnd (hwnd); }
    void  SetEditorMaxLength (size_t maxLength)       { m_editor.SetMaxLength (maxLength); }

    //  Whether rows have grippers and can be dragged to another place; on by
    //  default. Set before the columns.
    void  SetRowsMovable (bool isMovable)            { m_isMovable = isMovable; }

    void  SetOnValidate   (ValidateFn fn)  { m_validate = std::move (fn); }
    void  SetOnCommitCell (CommitFn fn)    { m_commit   = std::move (fn); }
    void  SetOnAddRow     (AddFn fn)       { m_add      = std::move (fn); }
    void  SetOnMoveRow    (MoveFn fn)      { m_move     = std::move (fn); }
    void  SetOnDeleteRow  (DeleteFn fn)    { m_delete   = std::move (fn); }
    void  SetOnError      (ErrorFn fn)     { m_error    = std::move (fn); }
    void  SetOnBeginEdit  (BeginFn fn)     { m_begin    = std::move (fn); }

    int   GetDataRowCount () const               { return (int) m_rows.size(); }
    bool  IsAddRow        (int row) const        { return row == GetDataRowCount(); }
    int   ToGridColumn    (int listColumn) const { return listColumn - 1; }
    int   GetGridColumnAt (int xPx) const;

    //  Whether a row can be edited or deleted: a data row not read-only; and
    //  whether one of its cells can be edited.
    bool  IsRowEditable   (int row) const;
    bool  IsCellEditable  (int row, int column) const;
    bool  CanDeleteRow    (int row) const        { return IsRowEditable (row); }
    void  DeleteRow       (int row);

    //  Opens the box over a cell of an editable column; false when the cell
    //  cannot be edited or is not in view.
    bool  BeginEdit       (int row, int column);

    //  Commits or cancels the open edit. When the commit callback returns
    //  false, the box stays open and this returns false.
    bool  EndEdit         (bool commit);

    bool             IsEditing      () const { return m_editRow >= 0; }
    int              GetEditRow     () const { return m_editRow; }
    int              GetEditColumn  () const { return m_editColumn; }
    DxuiTextInput  & GetEditor      ()       { return m_editor; }
    const std::vector<std::wstring> & GetPendingTexts() const { return m_pending; }

    //  The row a drag of row `from` would drop at for a point at y, relative
    //  to the list, and whether a drag is under way.
    int   GetDropRowAt   (int yPx) const;
    bool  IsDraggingRow  () const { return m_dragRow >= 0; }

    //  A row's drag counts, so a host that sends moves only to a list mid-drag
    //  sends them to the grid.
    bool  IsInteracting  () const override { return IsDraggingRow() || DxuiListView::IsInteracting(); }

    void     Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT localPx) const override;
    bool     OnKey             (const DxuiKeyEvent & ev) override;
    bool     OnChar            (wchar_t ch) override;
    void     OnFocusChanged    (bool focused) override;

    using DxuiListView::Paint;

private:
    void  RebuildRows      ();
    bool  CommitEdit       ();
    bool  CommitAddRow     (const std::wstring & text, int next);
    bool  MoveEdit         (int step);
    int   GetNextEditColumn (int column, int step) const;
    int   GetNextEmptyColumn (int column) const;
    bool  IsColumnEditable (int column) const;
    void  ReportError      (const std::wstring & error);
    bool  RouteEditorMouse (const DxuiMouseEvent & ev);
    void  PaintGrippers    (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    void  PaintDropLine    (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    std::wstring  GetEditText (int row, int column) const;
    bool          TryGetEditorRect (int row, int column, RECT & outRect) const;

    std::vector<GridColumn>    m_columns;
    std::vector<GridRow>       m_rows;
    std::vector<std::wstring>  m_pending;
    std::wstring               m_addRowText;
    DxuiTextInput              m_editor;
    DxuiDpiScaler              m_gridScaler;
    ValidateFn                 m_validate;
    CommitFn                   m_commit;
    AddFn                      m_add;
    MoveFn                     m_move;
    DeleteFn                   m_delete;
    ErrorFn                    m_error;
    BeginFn                    m_begin;
    int                        m_editRow     = -1;
    int                        m_editColumn  = -1;
    int                        m_pressRow    = -1;
    int                        m_pressColumn = -1;
    int                        m_dragRow     = -1;
    int                        m_dropRow     = -1;
    bool                       m_isEnding    = false;
    bool                       m_isMovable   = true;
};
