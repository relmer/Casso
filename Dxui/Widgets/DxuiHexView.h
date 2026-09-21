#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiScrollbar.h"





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

    //  Writes an edited value at `offset`, returning whether it was taken. A
    //  source is read-only unless it overrides this. Const because the source
    //  object passes the write on rather than holding the bytes it changes.
    virtual bool  WriteBytes (uint64_t offset, std::span<const uint8_t> bytes) const
    {
        (void) offset;
        (void) bytes;
        return false;
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

class DxuiHexView : public IDxuiControl
{
public:
    enum class Column { None, Hex, Text };

    //  Which byte a point landed on, and in which column. `hit` is false for
    //  a point in the offset gutter or past the last row.
    struct HitResult
    {
        bool      hit    = false;
        uint64_t  offset = 0;
        Column    column = Column::None;
    };

    //  How the text column decodes a byte. Apple II text has the high bit set
    //  for ordinary characters and clear for inverse and flashing ones, so
    //  both values decode to the same character.
    enum class TextEncoding { Ascii, AppleHighBit };

    //  A color for a marked byte, or false to leave it the ordinary one.
    using MarkColorFn = std::function<bool (uint8_t mark, uint32_t & outArgb)>;

    //  Told the offset of a value the source would not take.
    using WriteRefusedFn = std::function<void (uint64_t offset)>;

    //  Asked for a context menu at a point in the same DIPs as the bounds.
    using ContextMenuFn = std::function<void (POINT atDip)>;

    DxuiHexView() { m_focusable = true; }
    ~DxuiHexView() override = default;

    void  SetSource (const IDxuiHexSource * source);
    const IDxuiHexSource *  GetSource () const { return m_source; }

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
    int   GetColumns     () const { return m_columns; }

    //  How the value column reads each value: its bytes as one little-endian
    //  integer, in hex or in decimal. Without values only the text column
    //  shows.
    enum class ValueFormat { Hex, Signed, Unsigned };

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
    float  GetZoom         () const         { return m_zoom; }
    void   SetTextStrength (float strength) { m_textStrength = strength; }

    //  One character cell of the face the host paints with, in DIPs.
    void  SetCellSizeDip (int widthDip, int heightDip);

    //  Rows the source fills, and rows the bounds can show.
    uint64_t  GetRowCount     () const;
    int       GetRowCap       () const;
    uint64_t  GetTopRow       () const { return m_topRow; }
    uint64_t  GetMaxTopRow    () const;
    void      SetTopRow       (uint64_t row);
    void      ScrollRows      (int64_t delta);
    void      EnsureByteVisible (uint64_t offset);

    //  Where a byte is drawn, in the same DIPs as the bounds. An off-screen
    //  byte or an absent column comes back empty.
    RECT       GetByteRect      (uint64_t offset, Column column) const;
    RECT       GetRowOffsetRect (uint64_t row) const;
    HitResult  HitTestPoint     (POINT clientDip) const;

    //  Selection. `SelectByte` starts a new run, `ExtendSelectionTo` moves the
    //  moving end of it, and the anchor stays where the run began.
    void  SelectByte       (uint64_t offset, Column column);
    void  ExtendSelectionTo (uint64_t offset);
    void  SelectAll        ();
    void  ClearSelection   ();

    bool      HasSelection       () const { return m_hasSelection; }
    bool      IsByteSelected     (uint64_t offset) const;
    uint64_t  GetSelectionFirst  () const;
    uint64_t  GetSelectionLast   () const;
    uint64_t  GetSelectionCount  () const;
    uint64_t  GetSelectionAnchor () const { return m_anchor; }
    uint64_t  GetCaret           () const { return m_caret; }

    void          SetTextEncoding (TextEncoding encoding) { m_encoding = encoding; ResetLineIndex(); }
    TextEncoding  GetTextEncoding () const                { return m_encoding; }

    void  SetMarkColor     (MarkColorFn fn)   { m_markColor = std::move (fn); }
    void  SetOnContextMenu (ContextMenuFn fn) { m_onContextMenu = std::move (fn); }

    //  The window a copy names as the clipboard's owner.
    void  SetOwnerWindow (HWND hwnd) { m_hwnd = hwnd; }

    //  Space before the address column.
    void  SetPaddingDip (int padDip) { m_padDip = padDip; }

    //  The active column. A click sets it, and it determines the format Copy
    //  produces.
    Column  GetActiveColumn () const { return m_activeColumn; }
    void    SetActiveColumn (Column column);

    //  Puts the caret on a byte and scrolls to it.
    void  GoToOffset (uint64_t offset);

    //  Typing edits the bytes when the view is editable. In the hex column,
    //  digits collect for the value under the caret (the hex format only) and
    //  the value is written, low byte first, as soon as it has all its digits;
    //  in the text column each character is written as it is typed, in the
    //  view's text encoding. The caret moves on after a write the source
    //  takes. Escape or any caret move drops digits not yet written.
    void                  SetEditable       (bool editable)     { m_editable = editable; m_pending.clear(); }
    bool                  IsEditable        () const            { return m_editable; }
    const std::wstring &  GetPendingDigits  () const            { return m_pending; }
    void                  SetOnWriteRefused (WriteRefusedFn fn) { m_onWriteRefused = std::move (fn); }

    //  Copies the selection to the clipboard in the active column's format.
    //  With nothing selected, the clipboard is not changed.
    void          CopySelection () const;
    std::wstring  GetSelectionText () const;

    //  The characters the text column displays for a run of bytes, and the
    //  hex digits the hex column displays for it. Painting and Copy use the
    //  same decoding.
    std::wstring  GetTextFor (uint64_t offset, uint64_t count) const;
    std::wstring  GetHexFor  (uint64_t offset, uint64_t count) const;

    //  Told after any change to the selection, including its loss.
    void  SetOnSelectionChanged (std::function<void ()> fn) { m_onSelectionChanged = std::move (fn); }

    bool  IsDragging () const { return m_dragging || m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }

    //  Rows wider than the view scroll sideways, so the text column can always
    //  be reached whatever the columns and grouping.
    bool  IsHorzScrollbarVisible () const { return GetContentWidthPx() > GetViewWidthPx(); }
    int   GetLeftPx              () const { return m_leftPx; }

    //  Whether a point is on a scrollbar, and the hover that widens one.
    bool  IsOverScrollbar   (POINT pt) const override { return m_vertScroll.HitTest (pt.x, pt.y) || m_horzScroll.HitTest (pt.x, pt.y); }
    bool  SetScrollbarHover (POINT pt)                { return ((int) m_vertScroll.SetHover (m_vertScroll.HitTest (pt.x, pt.y), pt) | (int) m_horzScroll.SetHover (m_horzScroll.HitTest (pt.x, pt.y), pt)) != 0; }
    bool  TickScrollbars    (int64_t nowMs)           { return ((int) m_vertScroll.Tick (nowMs) | (int) m_horzScroll.Tick (nowMs)) != 0; }
    void  SetLeftPx              (int leftPx);
    bool  IsScrollbarVisible () const { return GetRowCount() > (uint64_t) (std::max) (GetRowCap(), 0); }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  OnFocusEntered (bool forward) override;
    void  OnFocusChanged (bool focused) override { m_hasFocus = focused; }
    bool  QueryCommand   (DxuiStandardCommand command, bool & outEnabled) const override;
    bool  InvokeCommand  (DxuiStandardCommand command) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    bool  OnKey   (const DxuiKeyEvent   & ev) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

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
    //  Hex digits the offset column spends, which is eight once the last
    //  address does not fit in four.
    int   GetOffsetDigits () const;
    int   GetHexCells     () const;
    int   GetGroupCount   () const;

    //  First character cell of a byte's two hex digits, within the hex column.
    int   GetByteCellInRow (int indexInRow) const;
    int   GetColumnStartCell (Column column) const;
    RECT  GetCellRect (int cellX, uint64_t row, int cellCount) const;
    bool  IsRowVisible (uint64_t row) const;
    void  ClampTopRow ();

    //  Moves the caret to `offset`, extending the run when `extend` is set and
    //  starting a new one otherwise, then brings it into view.
    void  MoveCaretTo (uint64_t offset, bool extend);
    void  NotifySelectionChanged ();
    uint64_t  GetLastOffset () const;

    //  Typing. Each returns whether it took the character.
    bool  TypeEditChar  (wchar_t ch);
    bool  TypeHexDigit  (wchar_t ch);
    bool  TypeCharacter (wchar_t ch);
    void  WriteAndAdvance (uint64_t offset, std::span<const uint8_t> bytes);
    static int  GetDigitValue (wchar_t ch);

    //  One row's bytes and marks, read straight from the source into the
    //  buffers the view reuses frame after frame.
    int   ReadRow (uint64_t row);
    void  PaintRow (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row, int count);
    void  EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    //  One cell run of the fixed-width face, and the fill behind it.
    static void  DrawCell (IDxuiTextRenderer & text, const RECT & rect, const wchar_t * chars, uint32_t argb, const DxuiFontHandle & font);
    RECT  GetSelectionCellRect (uint64_t offset, int index, const RECT & cell, bool hexColumn) const;

    static constexpr int  s_kSelectionMarginDip = 2;
    static constexpr int  s_kFixedRowWidth      = -1;

    //  The addresses' strength as a fraction of the bytes'.
    static constexpr float  s_kAddressStrength  = 0.55f;

    //  The ink on the byte the keys act on, over the accent.
    static constexpr uint32_t  s_kCaretInkArgb  = 0xFFFFFFFF;

    //  Cells one value spends, its text, and the fill behind a selected one.
    int           GetValueCells         () const;
    std::wstring  FormatValue           (uint64_t value, int present) const;
    uint64_t      SnapToValue           (uint64_t offset, bool toEnd) const;
    RECT          GetValueSelectionRect (uint64_t first, const RECT & cell) const;
    uint32_t      GetByteColor          (const IDxuiTheme & theme, uint8_t mark) const;

    //  Recomputes the bytes in a row from the columns, the grouping and, for
    //  automatic columns, the width.
    void  RecomputeRowWidth ();

    //  THE LINE INDEX IS BUILT ONLY AS FAR AS IT HAS BEEN NEEDED, so a huge
    //  file is not read end to end to show its first screen. A bookmark every
    //  s_kBookmarkRows rows holds where that row starts, its line number and
    //  whether it begins a line; a row between bookmarks is found again by
    //  scanning from the one before. Until the scan reaches the end, the row
    //  count is estimated from the rows scanned so far.
    struct Bookmark
    {
        uint64_t  offset = 0;
        uint64_t  line   = 1;
        bool      starts = true;
    };

    static constexpr uint64_t  s_kBookmarkRows = 256;
    static constexpr size_t    s_kScanBytes    = 64 * 1024;

    void      ResetLineIndex     () const;
    void      StepLineIndex      () const;
    void      IndexThroughRow    (uint64_t row) const;
    void      IndexThroughOffset (uint64_t offset) const;
    uint64_t  GetNextRowStart    (uint64_t start, bool & outNewLine) const;
    uint8_t   GetByteAt          (uint64_t offset) const;
    bool      IsLineBreak        (uint8_t byte) const;
    uint64_t  GetShownLength     (uint64_t start, uint64_t end) const;
    bool      GetRowSpan         (uint64_t row, uint64_t & outStart, uint64_t & outEnd, uint64_t & outLine, bool & outStarts) const;
    uint64_t  GetRowOfOffset     (uint64_t offset) const;
    int       GetColumnOfOffset  (uint64_t offset) const;
    int       GetLineDigits      () const;
    uint64_t  GetLineModeTarget  (WPARAM vk, bool ctrl) const;
    void      PaintLineRow       (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row);
    static constexpr int  s_kScrollbarWidthDip  = 10;

    void  SyncScrollbar ();
    void  ScrollToBarPos ();
    static void  FillCell (IDxuiTextRenderer & text, const RECT & rect, uint32_t argb);

    static wchar_t  GetHexDigit (int value);
    wchar_t         GetCharFor  (uint8_t byte) const;

    //  The rows' height, less the horizontal scrollbar when it shows; the
    //  full width a row needs; and the width left for rows beside the
    //  vertical scrollbar, whose room is always kept.
    int   GetHeightDip      () const;
    int   GetContentWidthPx () const;
    int   GetViewWidthPx    () const;
    void  KeepCaretInView   ();

    const IDxuiHexSource  * m_source        = nullptr;
    uint64_t                m_originAddress = 0;
    int                     m_bytesPerRow   = kDefaultBytesPerRow;
    int                     m_grouping      = kDefaultGrouping;
    int                     m_columns       = s_kFixedRowWidth;
    ValueFormat             m_format        = ValueFormat::Hex;
    bool                    m_showValues    = true;
    float                   m_zoom          = 1.0f;
    bool                    m_breakLines    = false;

    mutable std::vector<Bookmark>  m_bookmarks;
    mutable std::vector<uint8_t>   m_scanBuffer;
    mutable uint64_t               m_scanBufferBase = 0;
    mutable uint64_t               m_scanOffset     = 0;
    mutable uint64_t               m_scanRows       = 0;
    mutable uint64_t               m_scanLine       = 1;
    mutable bool                   m_scanStarts     = true;
    mutable bool                   m_scanComplete   = false;
    mutable uint64_t               m_spanRow        = UINT64_MAX;
    mutable uint64_t               m_spanStart      = 0;
    mutable uint64_t               m_spanLine       = 1;
    mutable bool                   m_spanStarts     = true;
    float                          m_textStrength   = 1.0f;
    int                            m_cellWidthDip   = 0;
    int                            m_cellHeightDip  = 0;
    uint64_t                       m_topRow         = 0;
    bool                           m_hasSelection   = false;
    uint64_t                       m_anchor         = 0;
    uint64_t                       m_caret          = 0;
    Column                         m_activeColumn   = Column::Hex;
    HWND                           m_hwnd           = nullptr;
    int                            m_padDip         = 0;
    bool                           m_dragging       = false;
    TextEncoding                   m_encoding       = TextEncoding::Ascii;
    DxuiDpiScaler                  m_scaler;
    DxuiScrollbar                  m_vertScroll;
    DxuiScrollbar                  m_horzScroll;
    int                            m_leftPx         = 0;
    MarkColorFn                    m_markColor;
    ContextMenuFn                  m_onContextMenu;
    std::function<void ()>  m_onSelectionChanged;
    std::vector<uint8_t>    m_rowBytes;
    std::vector<uint8_t>    m_rowMarks;
    bool                    m_editable  = false;
    bool                    m_hasFocus  = false;
    std::wstring            m_pending;
    uint64_t                m_editStart = 0;
    WriteRefusedFn          m_onWriteRefused;
};
