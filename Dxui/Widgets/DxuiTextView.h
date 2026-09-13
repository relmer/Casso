#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiScrollbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView
//
//  Read-only text in the fixed-width face, laid out as rows of cells, with
//  selection by character.
//
//  CELLS LINE UP IN COLUMNS AND THE LAST ONE WRAPS. Every cell but a row's last
//  starts in a column as wide as that column's widest cell. The last cell takes
//  the rest of the width and wraps at a space, and a wrapped line continues
//  under the last cell's first character, not under the row's. A BASIC line is
//  a number and a statement, so a long statement wraps under itself; a detail
//  is a label and a value, so a long value wraps under the value.
//
//  A SELECTION RUNS FROM ANY CHARACTER TO ANY CHARACTER, across cells and rows.
//  Positions count characters in a row's text with its cells joined by tabs, so
//  a copy has a tab between cells and a line break between rows.
//
//  A row can carry a warning, which draws a warning mark before its first cell.
//
//  The host sets the character cell size, or the view measures it from the
//  theme's fixed-width face, so layout and selection can be tested without a
//  device.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextView : public IDxuiControl
{
public:
    struct Row
    {
        std::vector<std::wstring>  cells;
        bool                       warning = false;
    };

    //  A character in a row's text, its cells joined by tabs.
    struct Position
    {
        int  row    = 0;
        int  offset = 0;

        bool operator== (const Position & other) const { return row == other.row && offset == other.offset; }
        bool operator<  (const Position & other) const { return row < other.row || (row == other.row && offset < other.offset); }
    };

    //  Asked for a context menu at a point in the same coordinates as the bounds.
    using ContextMenuFn = std::function<void (POINT atPoint)>;

    DxuiTextView() { m_focusable = true; }
    ~DxuiTextView() override = default;

    //  Replaces the text, clearing the selection and scrolling to the top.
    void                      SetRows  (std::vector<Row> rows);
    const std::vector<Row> &  GetRows  () const { return m_rows; }

    //  One character cell, in the same coordinates as the bounds.
    void  SetCellSize      (int widthPx, int heightPx);
    void  SetIconFace      (const wchar_t * face) { m_iconFace = face; }
    void  SetOwnerWindow   (HWND hwnd)            { m_hwnd = hwnd; }
    void  SetOnContextMenu (ContextMenuFn fn)     { m_onContextMenu = std::move (fn); }

    int   GetLineCount () const { return (int) m_lines.size(); }
    int   GetLineCap   () const;
    int   GetTopLine   () const { return m_topLine; }
    void  SetTopLine   (int line);
    void  ScrollLines  (int delta) { SetTopLine (m_topLine + delta); }

    Position  HitTest (POINT point) const;

    void          Select           (Position anchor, Position caret);
    void          SelectAll        ();
    void          ClearSelection   ();
    bool          HasSelection     () const { return !(m_anchor == m_caret); }
    std::wstring  GetSelectionText () const;
    void          CopySelection    () const;

    bool  IsInteracting      () const { return m_dragging || m_vertScroll.IsDragging(); }
    bool  IsScrollbarVisible () const { return (int) m_lines.size() > GetLineCap(); }

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                InvokeCommand     (DxuiStandardCommand command) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Text"; }

    static constexpr int  kColumnGapCells = 2;
    static constexpr int  kWarningCells   = 3;
    static constexpr int  kWheelLines     = 3;

private:
    //  One drawn line: its row, and the run of the row's last cell it shows.
    //  The row's other cells are drawn on its first line only.
    struct Line
    {
        int   row    = 0;
        int   start  = 0;
        int   length = 0;
        bool  first  = true;
    };

    static constexpr int  s_kPadDip             = 6;
    static constexpr int  s_kScrollbarWidthDip  = 10;

    void          Rebuild           ();
    void          BuildLines        (int columns);
    int           GetTextColumns    (bool scrollbar) const;
    int           GetCellStart      (const Row & row, int cell) const;
    int           GetCellBase       (const Row & row, int cell) const;
    int           GetRowLength      (const Row & row) const;
    std::wstring  GetRowText        (const Row & row) const;
    void          SyncScrollbar     ();
    void          EnsureCellSize    (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void          SelectWordAt      (Position pos);
    void          PaintLine         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);
    void          FillSelectedRange (IDxuiPainter & painter, int y, int column, int flatStart, int count, int trailCells, int selFrom, int selTo, uint32_t argb) const;

    std::vector<Row>   m_rows;
    std::vector<Line>  m_lines;
    std::vector<int>   m_columnCells;
    int                m_cellWidthPx   = 0;
    int                m_cellHeightPx  = 0;
    bool               m_cellPinned    = false;
    UINT               m_measuredDpi   = 0;
    int                m_topLine       = 0;
    Position           m_anchor;
    Position           m_caret;
    Position           m_lastClick;
    int64_t            m_lastClickMs   = 0;
    bool               m_dragging      = false;
    HWND               m_hwnd          = nullptr;
    const wchar_t    * m_iconFace      = L"Segoe MDL2 Assets";
    ContextMenuFn      m_onContextMenu;
    DxuiDpiScaler      m_scaler;
    DxuiScrollbar      m_vertScroll;
};
