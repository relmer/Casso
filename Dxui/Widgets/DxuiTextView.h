#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiScrollbar.h"
#include "IDxuiFindTarget.h"

struct DxuiIconImage;





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
//  A SPAN CAN BE A LINK: drawn in the theme's link color and underlined while
//  the pointer is over it, and the pointer over it is IDC_HAND. The link function
//  is called for a click on it: a press and a release on the same link with
//  nothing selected. That function opens whatever the link is for.
//
//  The host sets the character cell size, or the view measures it from the
//  theme's fixed-width face, so layout and selection can be tested without a
//  device.
//
//  PROPORTIONAL TEXT, such as an image's details, is drawn in the theme's
//  body face, with every cell but a row's last in the muted color. Each
//  character's place is measured once a paint has the renderer, and the
//  columns, wrapping and selection are laid out by those places instead of
//  by cells. Proportional text always wraps.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextView : public IDxuiControl, public IDxuiFindTarget
{
public:
    //  A run of a cell's characters drawn in a color of their own.
    struct Span
    {
        int       cell       = 0;
        int       start      = 0;
        int       length     = 0;
        uint32_t  argb       = 0;
        uint32_t  background = 0;   // a fill behind the characters; with argb zero, their text color is the usual one
        bool      isLink     = false;   // with argb zero, drawn in the theme's link color
    };

    //  A row's fill spans the view's width on every line it takes; its icon
    //  is drawn in the gutter, on its first line.
    struct Row
    {
        std::vector<std::wstring>              cells;
        bool                                   warning    = false;
        bool                                   heading    = false;   // drawn in the accent color
        uint32_t                               background = 0;
        std::shared_ptr<const DxuiIconImage>   icon;
        std::vector<Span>                      spans;

        //  The row's height as a fraction of a line, for a blank row that
        //  spaces the rows around it. A row this applies to takes one line.
        float                                  height     = 1.0f;

        //  A wrapped line of the last cell starts this many characters into
        //  the cell, so a help row's description wraps under itself rather
        //  than under its syntax.
        int                                    wrapIndent = 0;

        //  The fill of the gutter's band (SetGutterBand) on each line the
        //  row takes; zero for none.
        uint32_t                               gutterFill = 0;
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

    //  Called for a click on a link: its row and its index in the row's spans.
    using LinkFn        = std::function<void (int row, int span)>;

    DxuiTextView() { m_focusable = true; }
    ~DxuiTextView() override = default;

    //  Replaces the text, clearing the selection and scrolling to the top.
    void                      SetRows  (std::vector<Row> rows);

    //  The same document's rows again, as after a change of their colors or
    //  marks: the selection, a drag in progress, the find highlights and the
    //  top line are left in place, each end cut back to the new rows.
    void                      UpdateRows (std::vector<Row> rows);

    //  The rows again after `dropped` rows went from the front, as a log's
    //  oldest do, with others perhaps at the end: the selection, the find
    //  highlights and the find scope move up with the text.
    void                      SetRowsDroppingFront (std::vector<Row> rows, int dropped);
    const std::vector<Row> &  GetRows  () const { return m_rows; }

    //  One character cell, in the same coordinates as the bounds.
    void  SetCellSize      (int widthPx, int heightPx);
    void  SetProportional  (bool proportional);
    bool  IsProportional   () const               { return m_proportional; }
    void  SetIconFace      (const wchar_t * face) { m_iconFace = face; }
    void  SetOwnerWindow   (HWND hwnd)            { m_hwnd = hwnd; }
    void  SetOnContextMenu (ContextMenuFn fn)     { m_onContextMenu = std::move (fn); }
    void  SetOnLink        (LinkFn fn)            { m_onLink = std::move (fn); }

    //  A column ahead of the text for each row's icon, none by default. The
    //  icon is drawn at the given size, centered in the column.
    void  SetGutter        (int widthDip, int iconDip) { m_gutterDip = widthDip; m_gutterIconDip = iconDip; }
    int   GetGutterPx      () const               { return m_scaler.ToPx (m_gutterDip); }

    //  The gutter as an editor's glyph margin: its icon centered this far in
    //  from the view's left edge, as Visual Studio centers a breakpoint, and
    //  the gutter itself starting at that edge, as a list's first column
    //  does, rather than after the pad. Zero, the default, centers the icon
    //  in a gutter after the pad.
    void  SetGlyphCenterDip (float dip)           { m_glyphCenterDip = dip; }

    //  A band of the gutter, this far in from the view's left edge and this
    //  wide, filled on each row's lines with the row's gutterFill, as a list
    //  fills a narrow column's cells. None by default.
    void  SetGutterBand    (int leftDip, int widthDip) { m_bandLeftDip = leftDip; m_bandWidthDip = widthDip; }

    //  Whether a point is on the gutter's band, or on the rest of the gutter.
    bool  IsInGutterBand   (POINT point) const;
    bool  IsInGutter       (POINT point) const;

    //  With row focus on, a press focuses the row under it, drawn as a list
    //  draws its selected row while the view has the focus and no text is
    //  selected, and the keys move that row: Up and Down a row, Page Up and
    //  Page Down a page, Home and End, or Ctrl+Page Up and Ctrl+Page Down, to
    //  the top and bottom row in view, and Ctrl+Home and Ctrl+End to the
    //  first and last row. The view scrolls as little as it can with the
    //  row in view. Off by default; -1 is no row.
    void  SetRowFocus      (bool on)              { m_rowFocus = on; }
    int   GetFocusRow      () const               { return m_focusRow; }
    void  SetFocusRow      (int row);

    //  Whether a row is drawn as the focused row: row focus on, the view
    //  focused, no text selected, and the row the focused one.
    bool  IsRowSelectionShown (int row) const     { return m_rowFocus && m_isFocused && row == m_focusRow && !HasSelection(); }

    //  A row drawn in the theme's hover across the view, under the row's
    //  fill, as a list draws the row under the pointer, for a tip laid over
    //  it. -1, the default, is no row.
    void  SetHighlightRow  (int row, int fromCell = 0) { m_highlightRow = row; m_highlightCell = fromCell; }
    int   GetHighlightRow  () const               { return m_highlightRow; }

    //  The left and right of the text area, past the gutter and short of the
    //  scroll bar; and a cell's rect from its left to the text area's right,
    //  on the first line of its row, false when that line is not drawn.
    int   GetTextAreaLeftPx  () const;
    int   GetTextAreaRightPx () const;
    bool  GetCellRectPx      (int row, int cell, RECT & outRect) const;

    //  Where Paint starts a cell's text on the first line of its row and where
    //  its drawing of a cell in a clipped column is clipped, false when that
    //  line is not drawn.
    bool  TryGetCellTextClipPx (int row, int cell, float & left, float & right) const;

    //  A view that fills a pane: its text starts where the pane's title
    //  does, rather than a pad in from its left. Off by default.
    void  SetPaneTextInset (bool on)              { m_paneTextInset = on; }

    //  The face's size as a multiple of the theme's, and how far the text
    //  color goes from the background toward the theme's foreground (1 is
    //  the full foreground).
    void   SetZoom         (float zoom);
    float  GetZoom         () const               { return m_zoom; }
    void   SetTextStrength (float strength)       { m_textStrength = strength; }

    //  A line's height as a multiple of the face's, the extra split above and
    //  below the text. 1 by default; ignored while the cell size is pinned.
    void   SetLineSpacing  (float spacing);

    //  The widest a column but the last lines up at, in characters; zero for
    //  no limit. A row's longer cell pushes only that row's next cell along.
    void   SetColumnCap    (int column, int cells);

    //  The characters between a column and the next, kColumnGapCells unless
    //  set; zero or less puts the default back.
    void   SetColumnGap    (int column, int cells);

    //  With it on, the last cell is one line, not wrapped, and a bar along the
    //  bottom, or Shift with the wheel, scrolls the text sideways. Off by
    //  default.
    void   SetNoWrap       (bool noWrap);
    bool   IsNoWrap        () const               { return m_noWrap; }
    int    GetLeftColumn   () const               { return m_leftColumn; }
    void   SetLeftColumn   (int column);
    void   ScrollColumns   (int delta)            { SetLeftColumn (m_leftColumn + delta); }
    bool   IsHorzScrollbarVisible () const;

    //  Cells in this column and after it are cut off at the view's right
    //  edge and don't count toward the sideways scroll, which ends with the
    //  cells ahead of them; meant for text a tip draws whole. With -1, the
    //  default, every column scrolls.
    void   SetFirstClippedColumn (int column);

    //  A line's height and the space above its text, in pixels, once the
    //  view has been painted; zero before.
    int    GetCellHeightPx () const               { return m_cellHeightPx; }
    int    GetTextTopPx    () const               { return m_textTopPx; }

    int   GetLineCount () const { return (int) m_lines.size(); }

    //  The text measured and laid out at its bounds' width before it is drawn,
    //  and the height its lines take there, for a view sized to its text.
    void  Measure            (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    int   GetContentHeightPx () const;

    //  How many characters fit across the text with a scroll bar beside it.
    int   GetVisibleColumns () const { return GetTextColumns (true); }
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

    //  Where a row lies against the lines drawn: negative above them, zero
    //  among them, positive below. False for a row not laid out.
    bool  GetRowSide (int row, int & outSide) const;

    //  The left of a cell, drawn or not; false for a row not laid out.
    bool  GetCellLeftPx (int row, int cell, float & outX) const;

    //  The top and bottom of the drawn lines, and pixels to a DIP.
    void   GetLinesSpanPx (float & outTop, float & outBottom) const;
    float  GetPxPerDip    () const { return m_scaler.ToPxf (1.0f); }

    Position  HitTest (POINT point) const;

    //  The link under a point: its row, its index in the row's spans, and the
    //  part of it drawn on the line the point is on. False off every link.
    bool      TryGetLinkAt (POINT point, int & outRow, int & outSpan, RECT & outRect) const;

    //  The pointer's place, which sets the link drawn underlined; true when
    //  that link changed, so the view is drawn again. A point outside the view
    //  is over no link.
    bool      SetHoverPoint (POINT point);
    bool      IsLinkHovered (int row, int span) const { return m_hoverRow == row && m_hoverSpan == span; }

    void          Select           (Position anchor, Position caret);
    void          SelectAll        ();
    void          ClearSelection   ();
    bool          HasSelection     () const { return !(m_anchor == m_caret); }
    std::wstring  GetSelectionText () const override;
    void          CopySelection    () const;

    //  How a search ended: no match, a match, or a match reached only by
    //  going round past the end of the text (the start, searching backward).
    using FindResult = DxuiFindResult;

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
    FindResult  SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount) override;

    //  Find in selection: while set, SelectMatch takes only a match that lies
    //  wholly inside the selection this was set from. Setting it with nothing
    //  selected leaves the search the whole text.
    void  SetFindScopeToSelection () override;
    void  ClearFindScope          () override { m_scoped = false; }
    bool  IsFindScoped            () const { return m_scoped; }

    //  The search as it is typed: every match is highlighted, and the first
    //  one on screen is selected; with none on screen, the first after the
    //  top of the view is, scrolled into view. outIndex and outCount are as
    //  SelectMatch gives them.
    FindResult  SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount) override;

    //  Every match of the last search, each drawn on a fill that stands out
    //  from the text around it.
    const std::vector<FindMatch> &  GetFindHighlights   () const { return m_highlights; }
    void                            ClearFindHighlights () override { m_highlights.clear(); }

    //  The fill under a match: the theme's warning accent, made darker or
    //  lighter until it has at least 3:1 contrast with the text's background,
    //  and the color of the characters on it, whichever of the foreground and
    //  the background has more contrast with the fill.
    static uint32_t  GetFindHighlightFill (const IDxuiTheme & theme);
    static uint32_t  GetFindHighlightText (const IDxuiTheme & theme);

    //  Colors of the owner's own for the marks: one under every match and
    //  one under the match the search is at, the selected one. 0 for either
    //  keeps the theme's fill above for it.
    void      SetFindColors  (uint32_t matchArgb, uint32_t currentArgb) override { m_findMatchArgb = matchArgb; m_findCurrentArgb = currentArgb; }

    IDxuiControl * GetFindControl () override { return this; }
    RECT      GetFindBounds  () const override { return GetBounds(); }
    bool      IsFindVisible  () const override { return IsVisible(); }
    bool      IsCurrentMatch (const FindMatch & match) const;
    uint32_t  GetFindFillOf  (const FindMatch & match, const IDxuiTheme & theme) const;

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

    bool  IsInteracting      () const { return m_dragging || m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }
    bool  IsScrollbarVisible () const { return (int) m_lines.size() > GetLineCap(); }

    //  Whether a point is on the scrollbar, and the hover that widens it.
    bool  IsOverScrollbar   (POINT pt) const override { return (IsScrollbarVisible() && m_vertScroll.HitTest (pt.x, pt.y)) || (IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y)); }
    bool  SetScrollbarHover (POINT pt)                { bool  vert = m_vertScroll.SetHover (IsScrollbarVisible() && m_vertScroll.HitTest (pt.x, pt.y), pt); return m_horzScroll.SetHover (IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y), pt) || vert; }
    bool  TickScrollbars    (int64_t nowMs)           { bool  vert = m_vertScroll.Tick (nowMs); return m_horzScroll.Tick (nowMs) || vert; }

    //  Windows' scrollbar menu when the point is on one of the bars, in the
    //  coordinates OnMouse takes; empty elsewhere. Its rows scroll the view.
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu  (POINT pt);
    bool                            ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, GetScrollbarMenu (pt)); }

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override { m_isFocused = focused; }
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                InvokeCommand     (DxuiStandardCommand command) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Text"; }

    static constexpr int  kColumnGapCells = 2;
    static constexpr int  kWarningCells   = 3;
    static constexpr int  kWheelLines     = 3;
    static constexpr int  kWheelColumns   = 8;

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
    float         GetTextWidthPx    (bool scrollbar) const;

    //  Proportional text: the lines wrapped by the measured characters, each
    //  cell's characters measured, where a cell begins on its line, how wide
    //  a cell's characters from one place to another are drawn, and the
    //  boundary nearest a point. In the fixed-width face each is the cell
    //  arithmetic.
    void          BuildProportionalLines (float widthPx);
    void          MeasureCharacters      (IDxuiTextRenderer & text, const DxuiFontHandle & font);
    float         GetCellX               (const Row & row, int cell) const;
    float         GetCharsPx             (int row, int cell, int from, int to) const;
    int           GetNearestChar         (int row, int cell, int from, int to, float x) const;

    //  How far from the text's left the character `index` places into a
    //  part of a cell drawn on a line, starting at `cellFrom` in the cell,
    //  is drawn: a wrapped line of the last cell starts at its wrap indent.
    float         GetPartPx              (int row, int cell, int cellFrom, int index) const;
    int           GetCellStart      (const Row & row, int cell) const;
    int           GetLineColumn     (const Row & row, const Line & line) const;
    int           GetCellBase       (const Row & row, int cell) const;
    int           GetRowLength      (const Row & row) const;
    int           GetRowExtent      (const Row & row) const;
    std::wstring  GetRowText        (const Row & row) const;
    void          SyncScrollbar     ();
    void          EnsureCellSize    (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void          SelectWordAt      (Position pos);
    void          GetWordBounds     (Position pos, Position & outFirst, Position & outLast) const;
    int           GetLineOfPosition (Position pos) const;
    static bool   TryFindOnce       (const std::vector<std::wstring> & rows, const std::wstring & needle, bool wholeWord, bool forward, Position from, Position & outStart);
    static bool   IsWordChar        (wchar_t ch) { return iswalnum (ch) || ch == L'_'; }
    static bool   IsWholeWordAt     (const std::wstring & text, size_t at, size_t length);
    bool          GetScopedMatches  (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, std::vector<FindMatch> & outMatches) const;
    void          KeepUserSelection ();
    void          FillHighlights    (IDxuiPainter & painter, const IDxuiTheme & theme, int y, int row, int cell, int cellFrom, int flatStart, int count, uint32_t edge) const;

    //  A copy in lower case, one character for one.
    static std::wstring  GetLowered (const std::wstring & text);

    //  Word characters, spaces and tabs are three kinds; a run of one kind is
    //  what a double-click selects.
    static int    GetCharClass      (wchar_t ch) { return (ch == L'\t') ? 2 : (ch == L' ') ? 1 : 0; }
    void          PaintLine         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);
    void          FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const;
    void          DrawRun           (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int row, int cell, int cellFrom,
                                     int flatStart, const std::wstring & chars, bool selected, int selFrom, int selTo,
                                     const std::vector<uint32_t> & colors, const std::vector<bool> & lit, uint32_t litArgb) const;
    int           GetTextLeft       () const;
    int           GetPadLeftPx      () const;
    int           GetGutterLeftPx   () const;
    int           GetTextBaseLeft   () const;
    int           GetHorzBarPx      () const;
    void          PaintLineGutter   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex);
    bool          TryMoveFocusRow   (WPARAM vk, bool ctrl);

    //  A drawn line's height, a line's top counted from the first line shown,
    //  and the first line to show for a given line to be the last that fits.
    int           GetLineHeightPx   (int lineIndex) const;
    int           GetLineOffsetPx   (int lineIndex) const;
    int           GetTopForLast     (int lineIndex) const;

    //  Each character of a row's text, its cells joined by tabs: its span's
    //  color, the theme's link color for a link's, or 0 for the view's text color.
    std::vector<uint32_t>  GetRowColors (const Row & row, const IDxuiTheme & theme) const;

    //  The part of a cell drawn on a line: where it starts in the row's text,
    //  its length and where it starts in the cell. False when none of the
    //  cell is on the line, as a wrapped line has only the last cell's text.
    bool          GetLinePart       (const Row & row, const Line & line, int cell, int & outFlat, int & outLength, int & outCellFrom) const;

    //  The character under a point, not the nearest edge as HitTest takes:
    //  its line, its place in the row's text and its column. False between
    //  characters, past a line's end and off the lines.
    bool          TryGetCharAt      (POINT point, int & outLine, int & outFlat, int & outColumn) const;
    void          PaintLinkUnderline (IDxuiPainter & painter, const IDxuiTheme & theme, int lineIndex, int y) const;

    std::vector<Row>   m_rows;
    std::vector<Line>  m_lines;
    std::vector<int>   m_columnCells;
    int                m_cellWidthPx   = 0;

    //  Proportional text: each row's cells' character places, the width of
    //  the characters before each, and each column's widest cell, in pixels.
    //  Empty until a paint measures them.
    bool                                          m_proportional = false;
    std::vector<std::vector<std::vector<float>>>  m_charX;
    std::vector<float>                            m_columnPx;

    //  The face's advance, which is rarely a whole number of pixels. Columns
    //  are placed by it rather than by the rounded cell width, so a line split
    //  into selected and unselected runs lands where the whole line would.
    float                   m_cellAdvance     = 0.0f;
    int                     m_cellHeightPx    = 0;
    bool                    m_cellPinned      = false;
    float                   m_zoom            = 1.0f;
    float                   m_textStrength    = 1.0f;
    float                   m_lineSpacing     = 1.0f;
    int                     m_textTopPx       = 0;
    int                     m_gutterDip       = 0;
    int                     m_gutterIconDip   = 0;
    float                   m_glyphCenterDip  = 0.0f;
    int                     m_bandLeftDip     = 0;
    int                     m_bandWidthDip    = 0;
    UINT                    m_measuredDpi     = 0;
    int                     m_topLine         = 0;
    bool                    m_followEnd       = false;
    bool                    m_paneTextInset   = false;
    bool                    m_atEnd           = true;
    Position                m_anchor;
    Position                m_caret;
    bool                    m_scoped          = false;
    Position                m_scopeStart;
    Position                m_scopeEnd;
    std::vector<FindMatch>  m_highlights;
    Position                m_userAnchor;
    Position                m_userCaret;
    uint32_t                m_findMatchArgb   = 0;
    uint32_t                m_findCurrentArgb = 0;
    Position                m_lastClick;
    int64_t                 m_lastClickMs     = 0;
    int                     m_clicks          = 0;      // the clicks in a row on one spot, each within the double-click time
    float                   m_wheelLines      = 0.0f;   // a touchpad's part notches, until they make a line
    float                   m_wheelColumns    = 0.0f;   // and until they make a column
    bool                    m_dragging        = false;
    bool                    m_wordDrag        = false;
    Position                m_wordFirst;
    Position                m_wordLast;
    HWND                    m_hwnd            = nullptr;
    const wchar_t    * m_iconFace      = L"Segoe MDL2 Assets";
    ContextMenuFn      m_onContextMenu;
    LinkFn             m_onLink;
    int                m_hoverRow      = -1;   // the link under the pointer, -1 for none
    int                m_hoverSpan     = -1;
    int                m_pressRow      = -1;   // the link a press began on, -1 for none
    int                m_pressSpan     = -1;
    DxuiDpiScaler      m_scaler;
    DxuiScrollbar      m_vertScroll;
    DxuiScrollbar      m_horzScroll;
    std::map<int, int> m_columnCaps;
    std::map<int, int> m_columnGaps;
    bool               m_noWrap        = false;
    int                m_leftColumn    = 0;
    int                m_widestColumns = 0;
    int                m_firstClipped  = -1;
    bool               m_rowFocus      = false;
    int                m_focusRow      = -1;
    bool               m_isFocused     = false;
    int                m_highlightRow  = -1;
    int                m_highlightCell = 0;
};
