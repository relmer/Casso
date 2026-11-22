#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiScrollbar.h"
#include "DxuiTextLayout.h"
#include "DxuiTextPicture.h"
#include "DxuiTextViewPictures.h"
#include "IDxuiTextPictureHost.h"





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
//  under the last cell's first character, not under the row's, or a row's
//  wrap indent past it, as source code wraps under its operand. A BASIC line is
//  a number and a statement, so a long statement wraps under itself; a detail
//  is a label and a value, so a long value wraps under the value.
//
//  A SELECTION RUNS FROM ANY CHARACTER TO ANY CHARACTER, across cells and rows.
//  Positions count characters in a row's text with its cells joined by tabs, so
//  a copy has a tab between cells and a line break between rows.
//
//  A TABLE DOESN'T WRAP. With SetNoWrap, each row is one line, and a row wider
//  than the view scrolls sideways by whole characters under a horizontal
//  scrollbar. Proportional text always wraps.
//
//  A row can have a warning, which draws a warning mark before its first cell,
//  and spans, runs of a cell's characters drawn in colors of their own, as
//  source code's syntax is colored.
//
//  The host sets the character cell size, or the view measures it from the
//  theme's fixed-width face, so layout and selection can be tested without a
//  device.
//
//  PROPORTIONAL TEXT, such as an image's details, is drawn in the theme's
//  body face, with every cell but a row's last in the muted color. Each
//  character's place is measured once a paint has the renderer, and the
//  columns, wrapping and selection are laid out by those places instead of
//  by cells.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextView : public IDxuiControl, private IDxuiTextPictureHost
{
public:
    using Span     = DxuiTextLayout::Span;
    using Picture  = DxuiTextLayout::Picture;
    using Row      = DxuiTextLayout::Row;
    using Position = DxuiTextLayout::Position;

    //  Asked for a context menu at a point in the same coordinates as the bounds.
    using ContextMenuFn = std::function<void (POINT atPoint)>;

    DxuiTextView() { m_focusable = true; }
    ~DxuiTextView() override = default;

    //  Replaces the text, clearing the selection and scrolling to the top.
    void                    SetRows   (std::vector<Row> rows);
    const DxuiTextLayout &  GetLayout () const { return m_layout; }

    //  One character cell, in the same coordinates as the bounds.
    void  SetCellSize      (int widthPx, int heightPx);
    void  SetProportional  (bool proportional);
    void  SetNoWrap        (bool noWrap);
    void  SetIconFace      (const wchar_t * face) { m_iconFace = face; }
    void  SetOwnerWindow   (HWND hwnd)            { m_hwnd = hwnd; }
    void  SetOnContextMenu (ContextMenuFn fn)     { m_onContextMenu = std::move (fn); }

    //  The face's size as a multiple of the theme's, and how far the text
    //  color goes from the background toward the theme's foreground (1 is
    //  the full foreground).
    void   SetZoom         (float zoom);

    //  The vertical bar neither drawn nor hit-tested, for a host that scrolls
    //  the view with another bar; its strip stays reserved. Off by
    //  default.
    void   SetVerticalScrollbarHidden (bool hidden)  { m_vertBarHidden = hidden; }
    float  GetZoom                    () const               { return m_zoom; }
    void   SetTextStrength            (float strength)       { m_textStrength = strength; }

    int   GetLineCap   () const;

    //  The text measured and laid out at its bounds' width before it is drawn,
    //  and the height its lines take there, for a view sized to its text.
    void  Measure            (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    int   GetContentHeightPx () const;
    int   GetTopLine         () const { return m_topLine; }
    void  SetTopLine         (int line);
    void  ScrollLines        (int delta) { SetTopLine (m_topLine + delta); }

    //  The character column at the view's left edge, when rows don't wrap.
    int   GetLeftColumn   () const { return m_leftColumn; }
    void  SetLeftColumn   (int column);
    void  ScrollColumns   (int delta) { SetLeftColumn (m_leftColumn + delta); }

    Position  HitTest (POINT point) const;

    //  The rows' pictures: painting, hit testing, the outline, and the zoom,
    //  pan and ping of a picture shown as a map.
    DxuiTextViewPictures        & GetPictures ()       { return m_pictures; }
    const DxuiTextViewPictures  & GetPictures () const { return m_pictures; }

    //  Whether every row of more than one cell fits the text's width with each
    //  cell at its natural width, so no cell wraps: false for a table that
    //  doesn't fit. True before the text is measured.
    bool  DoColumnsFit () const;

    void          Select           (Position anchor, Position caret);
    void          SelectAll        ();
    void          ClearSelection   ();
    bool          HasSelection     () const { return !(m_anchor == m_caret); }
    std::wstring  GetSelectionText () const;
    void          CopySelection    () const;

    bool  IsInteracting          () const { return m_dragging || m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }
    bool  IsScrollbarVisible     () const { return m_layout.GetLineCount() > GetLineCap(); }
    bool  IsHorzScrollbarVisible () const { return m_horzVisible; }

    //  Whether a point is on the scrollbar, and the hover that widens it.
    bool  IsOverScrollbar   (POINT pt) const override { return IsOverVertScrollbar (pt) || IsOverHorzScrollbar (pt); }
    bool  SetScrollbarHover (POINT pt);
    bool  TickScrollbars    (int64_t nowMs);

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;

    //  Windows' scrollbar menu when the point is on one of the bars, in the
    //  coordinates OnMouse takes; empty elsewhere. Its rows scroll the view.
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu  (POINT pt);
    bool                            ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, GetScrollbarMenu (pt)); }
    bool                            OnKey             (const DxuiKeyEvent   & ev) override;
    bool                            QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                            InvokeCommand     (DxuiStandardCommand command) override;
    DxuiAccessibleRole              GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Text"; }

    static constexpr int  kWheelLines     = 3;
    static constexpr int  kWheelColumns   = 6;

private:
    using Line = DxuiTextLayout::Line;

    static constexpr int  s_kPadDip             = 6;
    static constexpr int  s_kScrollbarWidthDip  = 10;


    //  IDxuiTextPictureHost: the lines, rows and geometry the pictures read.
    int                      GetPictureLineCount    () const override         { return m_layout.GetLineCount(); }
    int                      GetPictureLineRow      (int line) const override { return m_layout.GetLines()[(size_t) line].row; }
    int                      GetPictureRowCount     () const override         { return (int) m_layout.GetRows().size(); }
    const DxuiTextPicture  * GetRowPicture          (int row) const override  { return (row >= 0 && row < (int) m_layout.GetRows().size()) ? m_layout.GetRows()[(size_t) row].picture.get() : nullptr; }
    int                      GetPictureTopLine      () const override         { return m_topLine; }
    int                      GetPictureLineHeight   () const override         { return m_layout.GetCellHeightPx(); }
    int                      GetPictureTextTop      () const override         { return (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip); }
    float                    GetPictureTextLeft     () const override         { return GetTextLeft(); }
    float                    GetPictureTextWidth    () const override         { return (float) GetTextColumns (IsScrollbarVisible()) * m_layout.GetCellAdvance(); }
    const DxuiDpiScaler    & GetPictureScaler       () const override         { return m_scaler; }
    void                     Rebuild                ();
    int                      GetTextColumns         (bool scrollbar) const;
    float                    GetTextLeft            () const;
    bool                     IsOverVertScrollbar    (POINT pt) const { return IsScrollbarVisible() && !m_vertBarHidden && m_vertScroll.HitTest (pt.x, pt.y); }
    bool                     IsOverHorzScrollbar    (POINT pt) const { return m_horzVisible && m_horzScroll.HitTest (pt.x, pt.y); }
    void                     SyncScrollbar          ();
    void                     EnsureCellSize         (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void                     SelectWordAt           (Position pos);
    void                     GetWordBounds          (Position pos, Position & outFirst, Position & outLast) const;

    //  Word characters, spaces and tabs are three kinds; a run of one kind is
    //  what a double-click selects.
    static int    GetCharClass      (wchar_t ch) { return (ch == L'\t') ? 2 : (ch == L' ') ? 1 : 0; }
    void          PaintLine         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);
    void          FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const;
    void          DrawRun           (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int row, int cell, int cellFrom,
                                     int flatStart, const std::wstring & chars, bool selected, int selFrom, int selTo,
                                     const std::vector<uint32_t> & colors) const;
    std::vector<uint32_t>  GetRowColors (const Row & row) const;

    //  The line being painted: its shift for its row's alignment, each
    //  character's style bits, and the painter an underline and an inverse
    //  fill are drawn with.
    float                  m_paintShift    = 0.0f;
    std::vector<uint8_t>   m_paintStyles;
    IDxuiPainter         * m_paintPainter  = nullptr;
    DxuiTextViewPictures   m_pictures      { *this };

    DxuiTextLayout     m_layout;
    float              m_zoom          = 1.0f;
    float              m_textStrength  = 1.0f;
    int                m_topLine       = 0;
    int                m_leftColumn    = 0;
    bool               m_horzVisible   = false;
    float              m_wheelLines    = 0.0f;   // a touchpad's part notches, until they make a line
    float              m_wheelColumns  = 0.0f;   // and until they make a column
    Position           m_anchor;
    Position           m_caret;
    Position           m_lastClick;
    int64_t            m_lastClickMs   = 0;
    int                m_clicks        = 0;   // the clicks in a row on one spot, each within the double-click time
    bool               m_dragging      = false;
    bool               m_wordDrag      = false;
    Position           m_wordFirst;
    Position           m_wordLast;
    HWND               m_hwnd          = nullptr;
    const wchar_t    * m_iconFace      = L"Segoe MDL2 Assets";
    ContextMenuFn      m_onContextMenu;
    DxuiDpiScaler      m_scaler;
    DxuiScrollbar      m_vertScroll;
    bool               m_vertBarHidden = false;
    DxuiScrollbar      m_horzScroll;
};
