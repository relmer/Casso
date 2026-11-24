#pragma once

#include "Pch.h"
#include "Widgets/HexView/DxuiHexLayout.h"
#include "Widgets/HexView/DxuiHexPainter.h"
#include "Widgets/HexView/DxuiHexSelection.h"
#include "Widgets/HexView/DxuiHexScroll.h"
#include "Widgets/DxuiScrollbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiHexSource
//
//  The bytes a hex view displays, supplied by the host rather than copied
//  into the widget. A file preview reads from its payload; a debugger reads
//  from the machine's bus. The view requests only the rows it draws, so a
//  whole address space costs no more than one screen.
//
//  Mark values are defined by the host: zero is an ordinary byte, and any
//  other value is a byte the host colors differently, such as a byte changed
//  since the last stop or a byte in a breakpoint's instruction. The view
//  passes the value to painting and never interprets it.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiHexSource
{
public:
    virtual ~IDxuiHexSource() = default;

    virtual uint64_t  GetByteCount () const = 0;

    //  Fills `out` with the bytes at `offset`. The view never requests bytes
    //  past the end, so a source may treat a short read as a programming error.
    virtual void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const = 0;

    //  One mark per byte at `offset`, zero for an ordinary byte. A source
    //  with nothing to mark leaves the default.
    virtual void  ReadMarks (uint64_t offset, std::span<uint8_t> out) const
    {
        (void) offset;
        std::fill (out.begin(), out.end(), uint8_t (0));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView
//
//  A selectable hex dump: offsets on the left, bytes in the middle grouped
//  one, two, four or eight at a time, and their characters on the right.
//
//  THE TWO COLUMNS ARE TAB STOPS WITHIN THE WIDGET. Tab into the view moves
//  focus to the hex column, Tab again to the text column, and a third Tab to
//  the next control; Shift+Tab reverses the order. The active column can be
//  changed without the mouse.
//
//  ONE SELECTION COVERS A RUN OF BYTES, NOT OF CHARACTERS. Selecting in either
//  column selects bytes, and the selection is highlighted in both columns. The
//  last-clicked column is the active one: it determines whether Copy produces
//  hex digits or characters. There is a single Copy command, not separate
//  copy-as-hex and copy-as-text commands.
//
//  Offsets count from the first byte of the source; the address displayed for
//  a row adds the host's origin, so the same widget labels a file from zero
//  and a machine's memory from $C000.
//
//  The host sets the character cell size, or the view measures it once from
//  the theme's fixed-width face, so the geometry and selection logic can be
//  tested without a device.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexView : public IDxuiControl, public DxuiHexViewTypes
{
public:

    DxuiHexView() { m_focusable = true; }
    ~DxuiHexView() override = default;

    void                    SetSource (const IDxuiHexSource * source);
    const IDxuiHexSource  * GetSource () const { return m_source; }

    void      SetOriginAddress (uint64_t address);
    uint64_t  GetOriginAddress () const { return m_originAddress; }

    //  Bytes across a row, and how many of them sit together between spaces.
    //  A grouping that does not divide the row evenly is refused.
    void  SetBytesPerRow (int count);
    int   GetBytesPerRow () const { return m_bytesPerRow; }
    bool  SetGrouping    (int bytesPerGroup);
    int   GetGrouping    () const { return m_grouping; }

    //  Values across a row, each one grouping's bytes wide, or 0 for as many
    //  as the width holds. Setting a count replaces a row set in bytes.
    void  SetColumns     (int valuesPerRow);
    int   GetColumns     () const { return m_layout.m_columns; }

    void         SetValueFormat  (ValueFormat format);
    ValueFormat  GetValueFormat  () const { return m_format; }
    void         SetShowValues   (bool show);
    bool         IsShowingValues () const { return m_showValues; }

    //  With no values shown, rows break where the text's lines do, wrapping at
    //  the row width, and the offset column numbers the lines instead. A line
    //  ends at a carriage return, a line feed, or the two together, and in
    //  Apple text at a carriage return with the high bit set.
    void  SetBreakLines (bool breakLines);
    bool  IsLineMode    () const { return !m_showValues && m_breakLines; }

    //  The face's size as a multiple of the theme's, and how far the bytes'
    //  color goes from the background toward the theme's foreground.
    void   SetZoom         (float zoom);
    float  GetZoom         () const         { return m_painter.m_zoom; }
    void   SetTextStrength (float strength) { m_painter.m_textStrength = strength; }

    //  One character cell of the face the host paints with, in DIPs.
    void  SetCellSizeDip (int widthDip, int heightDip);

    //  Rows the source fills, and rows the bounds can show.
    uint64_t  GetRowCount       () const { return m_layout.GetRowCount(); }
    int       GetRowCap         () const { return m_layout.GetRowCap(); }
    uint64_t  GetTopRow         () const { return m_scroll.GetTopRow(); }
    uint64_t  GetMaxTopRow      () const { return m_layout.GetMaxTopRow(); }
    void      SetTopRow         (uint64_t row) { m_scroll.SetTopRow (row); }
    void      ScrollRows        (int64_t delta) { m_scroll.ScrollRows (delta); }
    void      EnsureByteVisible (uint64_t offset) { m_scroll.EnsureByteVisible (offset); }

    //  Where a byte is drawn, in the same DIPs as the bounds. An off-screen
    //  byte or an absent column comes back empty.
    RECT       GetByteRect      (uint64_t offset, Column column) const { return m_layout.GetByteRect (offset, column); }
    RECT       GetRowOffsetRect (uint64_t row) const { return m_layout.GetRowOffsetRect (row); }
    HitResult  HitTestPoint     (POINT clientDip) const { return m_layout.HitTestPoint (clientDip); }

    //  Selection. `SelectByte` starts a new run, `ExtendSelectionTo` moves the
    //  moving end of it, and the anchor stays where the run began.
    void  SelectByte        (uint64_t offset, Column column) { m_selection.SelectByte (offset, column); }
    void  ExtendSelectionTo (uint64_t offset) { m_selection.ExtendSelectionTo (offset); }
    void  SelectAll         () { m_selection.SelectAll(); }
    void  ClearSelection    () { m_selection.ClearSelection(); }

    bool      HasSelection       () const { return m_selection.HasSelection(); }
    bool      IsByteSelected     (uint64_t offset) const { return m_selection.IsByteSelected (offset); }
    uint64_t  GetSelectionFirst  () const { return m_selection.GetSelectionFirst(); }
    uint64_t  GetSelectionLast   () const { return m_selection.GetSelectionLast(); }
    uint64_t  GetSelectionCount  () const { return m_selection.GetSelectionCount(); }
    uint64_t  GetSelectionAnchor () const { return m_selection.GetAnchor(); }
    uint64_t  GetCaret           () const { return m_selection.GetCaret(); }

    void          SetTextEncoding (TextEncoding encoding) { m_encoding = encoding; m_layout.ResetLineIndex(); }
    TextEncoding  GetTextEncoding () const                { return m_encoding; }

    //  An editable view reserves room for the edit frame inside its bounds,
    //  so it doesn't move when editing starts or stops, and draws the edit
    //  look: the accent behind the caret byte while the view has the keys,
    //  changed bytes in red, and
    //  while editing, a light accent tint behind the bytes and an accent
    //  frame around them. In edit mode, a hex digit typed in the hex column
    //  sets the caret byte's high nibble, the next its low nibble, and the
    //  caret moves on; a character typed in the text column replaces the
    //  caret byte and the caret moves on.
    void  SetEditable  (bool isEditable)  { m_isEditable = isEditable; }
    bool  IsEditable   () const           { return m_isEditable; }
    void  SetEditing   (bool isEditing)   { m_isLowNibble = m_isLowNibble && isEditing == m_isEditing; m_isEditing = isEditing; }
    bool  IsEditing    () const           { return m_isEditing; }
    void  SetOnType    (TypeFn fn)        { m_onType    = std::move (fn); }
    void  SetIsChanged (ChangedFn fn)     { m_isChanged = std::move (fn); }
    bool  HasFocus     () const           { return m_hasFocus; }

    //  The next hex digit sets a high nibble again.
    void  ResetNibble () { m_isLowNibble = false; }

    //  True when a byte's high nibble is typed and its low nibble isn't yet.
    bool  IsTypingAt (uint64_t offset) const { return m_isLowNibble && m_nibbleAt == offset; }

    //  How the edit look draws a byte in one column: false for the view's
    //  look.
    static bool  PickEditStyle (const EditColors & colors, const EditByteState & state, ByteStyle & outStyle);

    //  ink, moved toward black or white until it reads on fill.
    static uint32_t  GetReadableInk (uint32_t ink, uint32_t fill);

    //  A changed byte's ink before it is fitted to its fill.
    static uint32_t  GetChangedInk  (uint32_t background);

    static constexpr int    kEditFrameDip = 2;       // the edit frame, reserved whether or not it's drawn
    static constexpr float  kTintShare    = 0.12f;   // the accent's share of the edit tint behind the bytes
    static constexpr float  kMinContrast  = 4.5f;    // the least contrast of a changed byte's ink with its fill

    void  SetMarkColor     (MarkColorFn fn)   { m_painter.m_markColor = std::move (fn); }
    void  SetByteStyle     (ByteStyleFn fn)   { m_painter.m_byteStyle = std::move (fn); }
    void  SetOnContextMenu (ContextMenuFn fn) { m_onContextMenu = std::move (fn); }

    void  SetOnBuildContextMenu (BuildMenuFn fn) { m_onBuildMenu = std::move (fn); }
    void  SetOnSettingsChanged  (SettingsFn fn)  { m_onSettings  = std::move (fn); }
    void  ShowContextMenu       (POINT atDip);

    //  The menu's items, the host's included; ShowContextMenu shows them.
    void  BuildContextMenu (std::vector<DxuiPopupMenuItem> & items);
    //  The window a copy names as the clipboard's owner.
    void  SetOwnerWindow (HWND hwnd) { m_hwnd = hwnd; }

    //  Space before the address column.
    void  SetPaddingDip (int padDip) { m_layout.m_padDip = padDip; }

    //  The active column. A click sets it, and it determines the format Copy
    //  produces.
    Column  GetActiveColumn () const { return m_selection.GetActiveColumn(); }
    void    SetActiveColumn (Column column) { m_selection.SetActiveColumn (column); }

    //  Puts the caret on a byte and scrolls to it.
    void  GoToOffset (uint64_t offset);

    //  Copies the selection to the clipboard in the active column's format.
    //  With nothing selected, the clipboard is not changed.
    void          CopySelection    () const { m_selection.CopySelection(); }
    std::wstring  GetSelectionText () const { return m_selection.GetSelectionText(); }

    //  The characters the text column displays for a run of bytes, and the
    //  hex digits the hex column displays for it. Painting and Copy use the
    //  same decoding.
    std::wstring  GetTextFor (uint64_t offset, uint64_t count) const { return m_painter.GetTextFor (offset, count); }
    std::wstring  GetHexFor  (uint64_t offset, uint64_t count) const { return m_painter.GetHexFor (offset, count); }

    //  Told after any change to the selection, including its loss.
    void  SetOnSelectionChanged (std::function<void ()> fn) { m_onSelectionChanged = std::move (fn); }

    bool  IsDragging () const { return m_selection.IsDragging() || m_scroll.IsDragging(); }

    //  Rows wider than the view scroll sideways, so the text column can always
    //  be reached whatever the columns and grouping.
    bool  IsHorzScrollbarVisible () const { return m_layout.GetContentWidthPx() > m_layout.GetViewWidthPx(); }
    int   GetLeftPx              () const { return m_scroll.GetLeftPx(); }

    //  The width every column takes, once the view has measured its text at
    //  its first paint; zero before then.
    int   GetContentWidthPx      () const { return m_cellWidthDip > 0 ? m_layout.GetContentWidthPx() : 0; }

    //  Whether a point is on a scrollbar, and the hover that widens one.
    bool  IsOverScrollbar    (POINT pt) const override { return m_scroll.m_vertScroll.HitTest (pt.x, pt.y) || m_scroll.m_horzScroll.HitTest (pt.x, pt.y); }
    bool  SetScrollbarHover  (POINT pt)                { return ((int) m_scroll.m_vertScroll.SetHover (m_scroll.m_vertScroll.HitTest (pt.x, pt.y), pt) | (int) m_scroll.m_horzScroll.SetHover (m_scroll.m_horzScroll.HitTest (pt.x, pt.y), pt)) != 0; }
    bool  TickScrollbars     (int64_t nowMs)           { return ((int) m_scroll.m_vertScroll.Tick (nowMs) | (int) m_scroll.m_horzScroll.Tick (nowMs)) != 0; }
    void  SetLeftPx          (int leftPx) { m_scroll.SetLeftPx (leftPx); }
    bool  IsScrollbarVisible () const { return m_layout.GetRowCount() > (uint64_t) (std::max) (m_layout.GetRowCap(), 0); }

    void  Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  OnFocusEntered (bool forward) override;
    bool  QueryCommand   (DxuiStandardCommand command, bool & outEnabled) const override;
    bool  InvokeCommand  (DxuiStandardCommand command) override;
    bool  OnMouse        (const DxuiMouseEvent & ev) override;

    //  Windows' scrollbar menu when the point is on one of the bars, in the
    //  coordinates OnMouse takes; empty elsewhere. Its rows scroll the view.
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu  (POINT pt) { return m_scroll.GetScrollbarMenu (pt); }
    bool                            ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, GetScrollbarMenu (pt)); }
    bool                            OnKey             (const DxuiKeyEvent   & ev) override;
    bool                            OnChar            (wchar_t ch) override;
    void                            OnFocusChanged    (bool isFocused) override { m_hasFocus = isFocused; }
    void                            Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Hex view"; }

    static constexpr int  kDefaultBytesPerRow = 16;
    static constexpr int  kDefaultGrouping    = 1;

    //  Character cells between the three columns.
    static constexpr int  kGutterCells = 2;

    static constexpr int  kWheelRows = 3;

    //  The most bytes Copy takes.
    static constexpr uint64_t  kMaxCopyBytes = 16 * 1024 * 1024;

private:

    //  Moves the caret to `offset`, extending the run when `extend` is set and
    //  starting a new one otherwise, then brings it into view.
    uint64_t  GetLastOffset () const;

    //  The edit frame, and a byte's look with the edit look and the host's
    //  style combined.
    void  PaintEditFrame (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    bool  GetByteStyle   (const IDxuiTheme & theme, uint64_t offset, uint8_t mark, bool isSelected, Column column, ByteStyle & outStyle) const;

    static constexpr int  s_kSelectionMarginDip = 2;

    static constexpr int  s_kScrollbarWidthDip  = 10;



    const IDxuiHexSource  * m_source        = nullptr;
    uint64_t                m_originAddress = 0;
    int                     m_bytesPerRow   = kDefaultBytesPerRow;
    int                     m_grouping      = kDefaultGrouping;
    ValueFormat             m_format        = ValueFormat::Hex;
    bool                    m_showValues    = true;
    bool                    m_breakLines    = false;

    int                            m_cellWidthDip   = 0;
    HWND                           m_hwnd           = nullptr;
    TextEncoding                   m_encoding       = TextEncoding::Ascii;
    DxuiDpiScaler                  m_scaler;
    ContextMenuFn                  m_onContextMenu;
    BuildMenuFn                    m_onBuildMenu;
    SettingsFn                     m_onSettings;
    std::function<void ()>         m_onSelectionChanged;
    TypeFn                         m_onType;
    ChangedFn                      m_isChanged;
    RECT                           m_frameDip       = {};
    uint64_t                       m_nibbleAt       = 0;
    bool                           m_isEditable     = false;
    bool                           m_isEditing      = false;
    bool                           m_isLowNibble    = false;
    bool                           m_hasFocus       = false;

    //  The view's parts, each with the state it uses.
    friend class      DxuiHexLayout;
    friend class      DxuiHexPainter;
    friend class      DxuiHexSelection;
    friend class      DxuiHexScroll;
    DxuiHexLayout     m_layout         { *this };
    DxuiHexPainter    m_painter        { *this };
    DxuiHexSelection  m_selection      { *this };
    DxuiHexScroll     m_scroll         { *this };
};

