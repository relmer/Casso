#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiScrollbar.h"
#include "DxuiTextLayout.h"
#include "DxuiTextPosition.h"
#include "DxuiTextRow.h"
#include "DxuiTextSearch.h"
#include "DxuiTextViewFind.h"
#include "DxuiTextViewHitTest.h"
#include "DxuiTextViewInput.h"
#include "DxuiTextViewSelection.h"
#include "DxuiTextViewport.h"
#include "IDxuiFindTarget.h"
#include "IDxuiTextInputHost.h"
#include "IDxuiTextScroller.h"

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

class DxuiTextView : public IDxuiControl,
                     public IDxuiFindTarget,
                     private IDxuiTextInputHost,
                     private IDxuiTextScroller
{
public:
    using Span     = DxuiTextSpan;
    using Picture  = DxuiTextPicture;
    using Row      = DxuiTextRow;
    using Position = DxuiTextPosition;

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
    const std::vector<Row>  & GetRows              () const { return m_layout.GetRows(); }

    //  One character cell, in the same coordinates as the bounds.
    void  SetCellSize      (int widthPx, int heightPx);
    void  SetProportional  (bool proportional);
    bool  IsProportional   () const               { return m_layout.IsProportional(); }
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
    bool  IsRowSelectionShown (int row) const { return m_rowFocus && m_isFocused && row == m_focusRow && !HasSelection(); }

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

    //  The vertical bar neither drawn nor hit-tested, for a host that scrolls
    //  the view with another bar; its strip stays reserved. Off by default.
    void   SetVerticalScrollbarHidden (bool hidden) { m_vertBarHidden = hidden; }
    int    GetLineHeightPx            () const      { return m_layout.GetCellHeightPx(); }

    //  A line's height as a multiple of the face's, the extra split above and
    //  below the text. 1 by default; ignored while the cell size is pinned.
    void   SetLineSpacing  (float spacing);

    //  The widest a column but the last lines up at, in characters; zero for
    //  no limit. A row's longer cell pushes only that row's next cell along.
    void   SetColumnCap    (int column, int cells) { m_layout.SetColumnCap (column, cells); Rebuild(); }

    //  The characters between a column and the next, kColumnGapCells unless
    //  set; zero or less puts the default back.
    void   SetColumnGap    (int column, int cells) { m_layout.SetColumnGap (column, cells); Rebuild(); }

    //  With it on, the last cell is one line, not wrapped, and a bar along the
    //  bottom, or Shift with the wheel, scrolls the text sideways. Off by
    //  default.
    void  SetNoWrap              (bool noWrap);
    bool  IsNoWrap               () const               { return m_layout.IsNoWrap(); }
    int   GetLeftColumn          () const override      { return m_leftColumn; }
    void  SetLeftColumn          (int column) override;
    void  ScrollColumns          (int delta) override   { SetLeftColumn (m_leftColumn + delta); }
    bool  IsHorzScrollbarVisible () const;

    //  Cells in this column and after it are cut off at the view's right
    //  edge and don't count toward the sideways scroll, which ends with the
    //  cells ahead of them; meant for text a tip draws whole. With -1, the
    //  default, every column scrolls.
    void   SetFirstClippedColumn (int column) { m_layout.SetFirstClippedColumn (column); Rebuild(); }

    //  A line's height and the space above its text, in pixels, once the
    //  view has been painted; zero before.
    int    GetCellHeightPx () const               { return m_layout.GetCellHeightPx(); }
    int    GetTextTopPx    () const               { return m_textTopPx; }

    int   GetLineCount () const { return m_layout.GetLineCount(); }

    //  The text measured and laid out at its bounds' width before it is drawn,
    //  and the height its lines take there, for a view sized to its text.
    void  Measure            (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    int   GetContentHeightPx () const;

    //  How many characters fit across the text with a scroll bar beside it.
    int   GetVisibleColumns () const { return GetTextColumns (true); }
    int   GetLineCap        () const override;
    int   GetTopLine        () const override { return m_topLine; }
    void  SetTopLine        (int line) override;
    void  ScrollLines       (int delta) override { SetTopLine (m_topLine + delta); }

    //  A view that follows its end, as a console does: at the bottom, it stays
    //  there through new rows and a layout; scrolled up, new rows keep its place.
    void  SetFollowEnd (bool follow) { m_followEnd = follow; }
    bool  IsAtEnd      () const { return m_atEnd; }

    //  The drawn line a row starts on, which differs from the row once a row
    //  above it has wrapped. -1 for a row not laid out.
    int   GetFirstLineOfRow (int row) const { return m_layout.GetFirstLineOfRow (row); }

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

    Position  HitTest (POINT point) const { return DxuiTextViewHitTest (m_layout, GetViewport()).HitTest (point); }

    //  The link under a point: its row, its index in the row's spans, and the
    //  part of it drawn on the line the point is on. False off every link.
    bool      TryGetLinkAt (POINT point, int & outRow, int & outSpan, RECT & outRect) const { return !IsOverScrollbar (point) && DxuiTextViewHitTest (m_layout, GetViewport()).TryGetLinkAt (point, outRow, outSpan, outRect); }

    //  The pointer's place, which sets the link drawn underlined; true when
    //  that link changed, so the view is drawn again. A point outside the view
    //  is over no link.
    bool      SetHoverPoint (POINT point);
    bool      IsLinkHovered (int row, int span) const { return m_hoverRow == row && m_hoverSpan == span; }

    void          Select           (Position anchor, Position caret) { m_selection.Select (anchor, caret); }
    void          SelectAll        ()                                { m_selection.SelectAll(); }
    void          ClearSelection   ()                                { m_selection.Clear(); }
    bool          HasSelection     () const                          { return m_selection.HasSelection(); }
    std::wstring  GetSelectionText () const override                 { return m_selection.GetText(); }
    void          CopySelection    () const                          { m_selection.Copy (m_hwnd); }

    //  How a search ended: no match, a match, or a match reached only by
    //  going round past the end of the text (the start, searching backward).
    using FindResult = DxuiFindResult;

    //  Selects the next match after the selection, or the one before it, and
    //  scrolls it into view. A match is always searched for all the way round.
    FindResult  SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool forward) { return m_find.SelectMatch (needle, matchCase, wholeWord, forward); }

    //  One match: where it starts and how many characters it takes, which a
    //  regular expression can make differ from the text searched for.
    using FindMatch = DxuiTextMatch;

    //  The same with an option to read the needle as a regular expression,
    //  and the match's place among all of them: outIndex counts from 1, and
    //  both are 0 with no match. A needle that is not a valid expression
    //  matches nothing.
    FindResult  SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount) override { return m_find.SelectMatch (needle, matchCase, wholeWord, isRegex, forward, outIndex, outCount); }

    //  Find in selection: while set, SelectMatch takes only a match that lies
    //  wholly inside the selection this was set from. Setting it with nothing
    //  selected leaves the search the whole text.
    void  SetFindScopeToSelection () override { m_find.SetScopeToSelection(); }
    void  ClearFindScope          () override { m_find.ClearScope(); }
    bool  IsFindScoped            () const    { return m_find.IsScoped(); }

    //  The search as it is typed: every match is highlighted, and the first
    //  one on screen is selected; with none on screen, the first after the
    //  top of the view is, scrolled into view. outIndex and outCount are as
    //  SelectMatch gives them.
    FindResult  SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount) override { return m_find.SelectMatchInView (needle, matchCase, wholeWord, isRegex, outIndex, outCount); }

    //  Every match of the last search, each drawn on a fill that stands out
    //  from the text around it.
    const std::vector<FindMatch> &  GetFindHighlights   () const    { return m_find.GetHighlights(); }
    void                            ClearFindHighlights () override { m_find.ClearHighlights(); }

    //  The fill under a match: the theme's warning accent, made darker or
    //  lighter until it has at least 3:1 contrast with the text's background,
    //  and the color of the characters on it, whichever of the foreground and
    //  the background has more contrast with the fill.
    static uint32_t  GetFindHighlightFill (const IDxuiTheme & theme) { return DxuiTextSearch::GetFindHighlightFill (theme); }
    static uint32_t  GetFindHighlightText (const IDxuiTheme & theme) { return DxuiTextSearch::GetFindHighlightText (theme); }

    //  Colors of the owner's own for the marks: one under every match and
    //  one under the match the search is at, the selected one. 0 for either
    //  keeps the theme's fill above for it.
    void      SetFindColors  (uint32_t matchArgb, uint32_t currentArgb) override { m_find.SetColors (matchArgb, currentArgb); }

    IDxuiControl  * GetFindControl () override { return this; }
    RECT            GetFindBounds  () const override { return GetBounds(); }
    bool            IsFindVisible  () const override { return IsVisible(); }
    bool            IsCurrentMatch (const FindMatch & match) const { return m_find.IsCurrentMatch (match); }
    uint32_t        GetFindFillOf  (const FindMatch & match, const IDxuiTheme & theme) const { return m_find.GetFillOf (match, theme); }

    //  Every match in the rows, in order. False when the needle is not a valid
    //  regular expression.
    static bool  FindAllInRows (const std::vector<std::wstring> & rows,
                                const std::wstring              & needle,
                                bool                              matchCase,
                                bool                              wholeWord,
                                bool                              isRegex,
                                std::vector<FindMatch>          & outMatches) { return DxuiTextSearch::FindAllInRows (rows, needle, matchCase, wholeWord, isRegex, outMatches); }

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
                                   Position                        & outStart) { return DxuiTextSearch::FindInRows (rows, needle, matchCase, wholeWord, forward, from, outStart); }

    //  Scrolls as little as it takes to show the line a position is drawn on.
    void  ScrollToPosition (Position pos) override;

    bool  IsInteracting      () const { return m_input.IsDragging() || m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }
    bool  IsScrollbarVisible () const { return m_layout.GetLineCount() > GetLineCap(); }

    //  Whether a point is on the scrollbar, and the hover that widens it.
    bool  IsOverScrollbar   (POINT pt) const override { return (IsScrollbarVisible() && !m_vertBarHidden && m_vertScroll.HitTest (pt.x, pt.y)) || (IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y)); }
    bool  SetScrollbarHover (POINT pt)                { bool  vert = m_vertScroll.SetHover (IsScrollbarVisible() && !m_vertBarHidden && m_vertScroll.HitTest (pt.x, pt.y), pt); return m_horzScroll.SetHover (IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y), pt) || vert; }
    bool  TickScrollbars    (int64_t nowMs)           { bool  vert = m_vertScroll.Tick (nowMs); return m_horzScroll.Tick (nowMs) || vert; }

    //  Windows' scrollbar menu when the point is on one of the bars, in the
    //  coordinates OnMouse takes; empty elsewhere. Its rows scroll the view.
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu  (POINT pt);
    bool                            ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, GetScrollbarMenu (pt)); }

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override { return m_input.OnMouse (ev); }
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override { return m_input.OnKey (ev); }
    void                OnFocusChanged    (bool focused) override { m_isFocused = focused; }
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override { return m_input.QueryCommand (command, outEnabled); }
    bool                InvokeCommand     (DxuiStandardCommand command) override { return m_input.InvokeCommand (command); }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Text"; }

    static constexpr int  kColumnGapCells = DxuiTextLayout::kColumnGapCells;
    static constexpr int  kWarningCells   = DxuiTextLayout::kWarningCells;
    static constexpr int  kWheelLines     = DxuiTextViewInput::kWheelLines;
    static constexpr int  kWheelColumns   = DxuiTextViewInput::kWheelColumns;

private:
    using Line = DxuiTextLine;

    static constexpr int  s_kPadDip             = 6;
    static constexpr int  s_kScrollbarWidthDip  = 10;

    void              Rebuild           ();
    int               GetTextColumns    (bool scrollbar) const;
    float             GetTextWidthPx    (bool scrollbar) const;
    void              MeasureCharacters (IDxuiTextRenderer & text, const DxuiFontHandle & font);
    void              SyncScrollbar     ();
    void              EnsureCellSize    (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void              SetFindMarks      () { m_find.SetScrollMarks (m_vertScroll, m_layout.GetLineCount()); }
    int               GetTextLeft       () const;
    int               GetPadLeftPx      () const;
    int               GetGutterLeftPx   () const;
    int               GetTextBaseLeft   () const;
    int               GetHorzBarPx      () const;
    bool              TryMoveFocusRow   (WPARAM vk, bool ctrl);
    DxuiTextViewport  GetViewport       () const;

    //  A drawn line's height, a line's top counted from the first line shown,
    //  and the first line to show for a given line to be the last that fits.
    int           GetLineHeightPx   (int lineIndex) const { return m_layout.GetLineHeightPx (lineIndex); }
    int           GetLineOffsetPx   (int lineIndex) const { return m_layout.GetLineOffsetPx (m_topLine, lineIndex); }
    int           GetTopForLast     (int lineIndex) const;

    //  IDxuiTextInputHost: the view, for its mouse and keys.
    Position                   HitTestInput          (POINT point) const override                                    { return HitTest (point); }
    bool                       TryGetInputLink       (POINT point, int & outRow, int & outSpan) const override { RECT  rect = {}; return TryGetLinkAt (point, outRow, outSpan, rect); }
    void                       GetInputWordBounds    (Position pos, Position & outFirst, Position & outLast) const override { m_selection.GetWordBounds (pos, outFirst, outLast); }
    RECT                       GetInputBounds        () const override                                               { return m_boundsDip; }
    int                        GetInputTextLeft      () const override                                               { return GetTextBaseLeft(); }
    bool                       IsInputNoWrap         () const override                                               { return m_layout.IsNoWrap(); }
    bool                       HasInputRows          () const override                                               { return !m_layout.GetRows().empty(); }
    bool                       HasInputSelection     () const override                                               { return HasSelection(); }
    DxuiScrollbar            * GetInputVertBar       (POINT point) override                                         { return (IsScrollbarVisible() && !m_vertBarHidden && m_vertScroll.HitTest (point.x, point.y)) ? &m_vertScroll : nullptr; }
    DxuiScrollbar            * GetInputHorzBar       (POINT point) override                                         { return (IsHorzScrollbarVisible() && m_horzScroll.HitTest (point.x, point.y)) ? &m_horzScroll : nullptr; }
    DxuiScrollbar            & GetVertBar            () override                                                     { return m_vertScroll; }
    DxuiScrollbar            & GetHorzBar            () override                                                     { return m_horzScroll; }
    void                       ScrollInputLines      (int delta) override                                            { ScrollLines (delta); }
    void                       ScrollInputColumns    (int delta) override                                            { ScrollColumns (delta); }
    void                       SetInputTopLine       (int line) override                                             { SetTopLine (line); }
    void                       SetInputLeftColumn    (int column) override                                           { SetLeftColumn (column); }
    int                        GetInputLeftColumn    () const override                                               { return m_leftColumn; }
    int                        GetInputLineCap       () const override                                               { return GetLineCap(); }
    int                        GetInputLineCount     () const override                                               { return GetLineCount(); }
    void                       SetInputSelection     (Position anchor, Position caret) override                      { m_selection.Select (anchor, caret); }
    void                       SetInputCaret         (Position caret) override                                       { m_selection.SetCaret (caret); }
    void                       SelectAllInput        () override                                                     { SelectAll(); }
    void                       CopyInputSelection    () const override                                               { CopySelection(); }
    void                       FocusRowOnPress       (int row) override                                              { m_focusRow = m_rowFocus ? row : m_focusRow; }
    bool                       TryMoveInputRow       (WPARAM vk, bool ctrl) override                                 { return m_rowFocus && TryMoveFocusRow (vk, ctrl); }
    void                       ShowInputMenu         (POINT point) override                                          { if (m_onContextMenu) { m_onContextMenu (point); } }
    void                       ClickInputLink        (int row, int span) override                                    { if (m_onLink) { m_onLink (row, span); } }

    DxuiTextLayout          m_layout;
    bool                    m_cellPinned      = false;
    float                   m_zoom            = 1.0f;
    float                   m_textStrength    = 1.0f;
    bool                    m_vertBarHidden   = false;

    float                           m_lineSpacing     = 1.0f;
    int                             m_textTopPx       = 0;
    int                             m_gutterDip       = 0;
    int                             m_gutterIconDip   = 0;
    float                           m_glyphCenterDip  = 0.0f;
    int                             m_bandLeftDip     = 0;
    int                             m_bandWidthDip    = 0;
    UINT                            m_measuredDpi     = 0;
    int                             m_topLine         = 0;
    bool                            m_followEnd       = false;
    bool                            m_paneTextInset   = false;
    bool                            m_atEnd           = true;
    DxuiTextViewSelection           m_selection       { m_layout };
    DxuiTextViewFind                m_find            { m_layout, m_selection, *this };
    DxuiTextViewInput               m_input           { *this };
    HWND                            m_hwnd            = nullptr;
    const wchar_t    * m_iconFace      = L"Segoe MDL2 Assets";
    ContextMenuFn      m_onContextMenu;
    LinkFn             m_onLink;
    int                m_hoverRow      = -1;   // the link under the pointer, -1 for none
    int                m_hoverSpan     = -1;

    DxuiDpiScaler      m_scaler;
    DxuiScrollbar      m_vertScroll;
    DxuiScrollbar      m_horzScroll;
    int                m_leftColumn    = 0;
    bool               m_rowFocus      = false;
    int                m_focusRow      = -1;
    bool               m_isFocused     = false;
    int                m_highlightRow  = -1;
    int                m_highlightCell = 0;
};
