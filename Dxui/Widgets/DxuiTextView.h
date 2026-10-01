#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"
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
    //  A run of a cell's characters drawn in a color of their own.
    struct Span
    {
        int       cell   = 0;
        int       start  = 0;
        int       length = 0;
        uint32_t  argb   = 0;
    };

    //  A row's fill spans the view's width on every line it takes; its icon
    //  is drawn in the gutter, on its first line.
    struct Row
    {
        std::vector<std::wstring>              cells;
        bool                                   warning    = false;
        uint32_t                               background = 0;
        std::shared_ptr<const DxuiIconImage>   icon;
        std::vector<Span>                      spans;
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

    //  A column ahead of the text for each row's icon, none by default. The
    //  icon is drawn at the given size, centered in the column.
    void  SetGutter        (int widthDip, int iconDip) { m_gutterDip = widthDip; m_gutterIconDip = iconDip; }
    int   GetGutterPx      () const               { return m_scaler.ToPx (m_gutterDip); }

    //  The face's size as a multiple of the theme's, and how far the text
    //  color goes from the background toward the theme's foreground (1 is
    //  the full foreground).
    void   SetZoom         (float zoom);
    float  GetZoom         () const               { return m_zoom; }
    void   SetTextStrength (float strength)       { m_textStrength = strength; }

    int   GetLineCount () const { return (int) m_lines.size(); }
    int   GetLineCap   () const;
    int   GetTopLine   () const { return m_topLine; }
    void  SetTopLine   (int line);
    void  ScrollLines  (int delta) { SetTopLine (m_topLine + delta); }

    //  A view that follows its end, as a console does: at the bottom, it stays
    //  there through new rows and a layout; scrolled up, new rows keep its place.
    void  SetFollowEnd (bool follow) { m_followEnd = follow; }
    bool  IsAtEnd      () const { return m_atEnd; }

    //  The drawn line a row starts on, which differs from the row once a row
    //  above it has wrapped. -1 for a row not laid out.
    int   GetFirstLineOfRow (int row) const;

    //  The left of a cell and the middle of its row's first line, in the
    //  coordinates the view paints in; false when that line is not drawn.
    bool  GetCellAnchorPx (int row, int cell, float & outX, float & outY) const;

    //  The top and bottom of the drawn lines, and pixels to a DIP.
    void   GetLinesSpanPx (float & outTop, float & outBottom) const;
    float  GetPxPerDip    () const { return m_scaler.ToPxf (1.0f); }

    Position  HitTest (POINT point) const;

    void          Select           (Position anchor, Position caret);
    void          SelectAll        ();
    void          ClearSelection   ();
    bool          HasSelection     () const { return !(m_anchor == m_caret); }
    std::wstring  GetSelectionText () const;
    void          CopySelection    () const;

    //  How a search ended: no match, a match, or a match reached only by
    //  going round past the end of the text (the start, searching backward).
    enum class FindResult { NotFound, Found, Wrapped };

    //  Selects the next match after the selection, or the one before it, and
    //  scrolls it into view. A match is always searched for all the way round.
    FindResult  SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool forward);

    //  One match: where it starts and how many characters it takes, which a
    //  regular expression can make differ from the text searched for.
    struct FindMatch
    {
        Position  start;
        int       length = 0;
    };

    //  The same with an option to read the needle as a regular expression,
    //  and the match's place among all of them: outIndex counts from 1, and
    //  both are 0 with no match. A needle that is not a valid expression
    //  matches nothing.
    FindResult  SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount);

    //  Find in selection: while set, SelectMatch takes only a match that lies
    //  wholly inside the selection this was set from. Setting it with nothing
    //  selected leaves the search the whole text.
    void  SetFindScopeToSelection ();
    void  ClearFindScope          ()       { m_scoped = false; }
    bool  IsFindScoped            () const { return m_scoped; }

    //  Every match in the rows, in order. False when the needle is not a valid
    //  regular expression.
    static bool  FindAllInRows (const std::vector<std::wstring> & rows,
                                const std::wstring              & needle,
                                bool                              matchCase,
                                bool                              wholeWord,
                                bool                              isRegex,
                                std::vector<FindMatch>          & outMatches);

    //  The search itself, over each row's text with its cells joined by tabs.
    //  A forward search takes the first match starting at or after `from`, a
    //  backward one the last match starting before it. A whole-word search
    //  takes only a match with no word character -- a letter, a digit or an
    //  underscore -- on either side of it.
    static FindResult  FindInRows (const std::vector<std::wstring> & rows,
                                   const std::wstring              & needle,
                                   bool                              matchCase,
                                   bool                              wholeWord,
                                   bool                              forward,
                                   Position                          from,
                                   Position                        & outStart);

    //  Scrolls as little as it takes to show the line a position is drawn on.
    void  ScrollToPosition (Position pos);

    bool  IsInteracting      () const { return m_dragging || m_vertScroll.IsDragging(); }
    bool  IsScrollbarVisible () const { return (int) m_lines.size() > GetLineCap(); }

    //  Whether a point is on the scrollbar, and the hover that widens it.
    bool  IsOverScrollbar   (POINT pt) const override { return IsScrollbarVisible() && m_vertScroll.HitTest (pt.x, pt.y); }
    bool  SetScrollbarHover (POINT pt)                { return m_vertScroll.SetHover (IsOverScrollbar (pt), pt); }
    bool  TickScrollbars    (int64_t nowMs)           { return m_vertScroll.Tick (nowMs); }

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
    void          GetWordBounds     (Position pos, Position & outFirst, Position & outLast) const;
    int           GetLineOfPosition (Position pos) const;
    static bool   TryFindOnce       (const std::vector<std::wstring> & rows, const std::wstring & needle, bool wholeWord, bool forward, Position from, Position & outStart);
    static bool   IsWordChar        (wchar_t ch) { return iswalnum (ch) || ch == L'_'; }
    static bool   IsWholeWordAt     (const std::wstring & text, size_t at, size_t length);

    //  A copy in lower case, one character for one.
    static std::wstring  GetLowered (const std::wstring & text);

    //  Word characters, spaces and tabs are three kinds; a run of one kind is
    //  what a double-click selects.
    static int    GetCharClass      (wchar_t ch) { return (ch == L'\t') ? 2 : (ch == L' ') ? 1 : 0; }
    void          PaintLine         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);
    void          FillSelectedRange (IDxuiPainter & painter, int y, int column, int flatStart, int count, int trailCells, int selFrom, int selTo, uint32_t argb) const;
    void          DrawRun           (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int column,
                                     int flatStart, const std::wstring & chars, bool selected, int selFrom, int selTo,
                                     const std::vector<uint32_t> & colors) const;
    int           GetTextLeft       () const;

    //  Each character of a row's text, its cells joined by tabs: its span's
    //  color, or 0 for the view's own.
    std::vector<uint32_t>  GetRowColors (const Row & row) const;

    std::vector<Row>   m_rows;
    std::vector<Line>  m_lines;
    std::vector<int>   m_columnCells;
    int                m_cellWidthPx   = 0;

    //  The face's advance, which is rarely a whole number of pixels. Columns
    //  are placed by it rather than by the rounded cell width, so a line split
    //  into selected and unselected runs lands where the whole line would.
    float     m_cellAdvance   = 0.0f;
    int       m_cellHeightPx  = 0;
    bool      m_cellPinned    = false;
    float     m_zoom          = 1.0f;
    float     m_textStrength  = 1.0f;
    int       m_gutterDip     = 0;
    int       m_gutterIconDip = 0;
    UINT      m_measuredDpi   = 0;
    int       m_topLine       = 0;
    bool      m_followEnd     = false;
    bool      m_atEnd         = true;
    Position  m_anchor;
    Position  m_caret;
    bool      m_scoped        = false;
    Position  m_scopeStart;
    Position  m_scopeEnd;
    Position  m_lastClick;
    int64_t   m_lastClickMs   = 0;
    bool      m_dragging      = false;
    bool      m_wordDrag      = false;
    Position  m_wordFirst;
    Position  m_wordLast;
    HWND      m_hwnd          = nullptr;
    const wchar_t    * m_iconFace      = L"Segoe MDL2 Assets";
    ContextMenuFn      m_onContextMenu;
    DxuiDpiScaler      m_scaler;
    DxuiScrollbar      m_vertScroll;
};
