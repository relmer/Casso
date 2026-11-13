#pragma once

#include "Pch.h"
#include "Widgets/HexView/DxuiHexLayout.h"
#include "Widgets/HexView/DxuiHexPainter.h"
#include "Widgets/HexView/DxuiHexSelection.h"
#include "Widgets/HexView/DxuiHexScroll.h"
#include "Widgets/HexView/DxuiHexRegions.h"
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

    //  Writes an edited value at `offset`, returning whether it was taken. A
    //  source is read-only unless it overrides this. Const because the source
    //  object passes the write on rather than storing the bytes it changes.
    virtual bool  WriteBytes (uint64_t offset, std::span<const uint8_t> bytes) const
    {
        (void) offset;
        (void) bytes;
        return false;
    }

    //  A tip the view shows at once over the byte at `offset`, such as its
    //  address, following the pointer from byte to byte. A source without
    //  one returns false.
    virtual bool  TryGetByteTip (uint64_t offset, std::wstring & tip) const
    {
        (void) offset;
        (void) tip;
        return false;
    }

    //  One region per byte at `offset`, zero for a byte in none. A run of
    //  bytes with the same nonzero region is one region, which the view
    //  outlines while its outlines are on. A source with no regions leaves
    //  the default. kRegionUnknown is a byte whose region isn't known yet,
    //  such as one not read; the view draws it in the nearest known region,
    //  so a region's outline doesn't start where the known bytes end.
    static constexpr uint16_t  kRegionUnknown = 0xFFFF;

    virtual void  ReadRegions (uint64_t offset, std::span<uint16_t> out) const
    {
        (void) offset;
        std::fill (out.begin(), out.end(), uint16_t (0));
    }

    //  A region's outline color and the label drawn on its top edge. False
    //  for a region the source does not outline.
    virtual bool  TryGetRegionStyle (uint16_t region, uint32_t & outArgb, std::wstring & outLabel) const
    {
        (void) region;
        (void) outArgb;
        (void) outLabel;
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

class DxuiHexView : public IDxuiControl, public DxuiHexViewTypes
{
public:

    DxuiHexView() { m_focusable = true; }
    ~DxuiHexView() override = default;

    void                    SetSource (const IDxuiHexSource * source);
    const IDxuiHexSource  * GetSource () const { return m_source; }

    void      SetOriginAddress (uint64_t address);
    uint64_t  GetOriginAddress () const { return m_originAddress; }

    //  Addresses that wrap at `bytes`, as a machine's do: each row's address
    //  is taken modulo it and given in the digits its last address needs, so
    //  rows started partway along a 64K space keep four digits. Zero, the
    //  default, for addresses that run on past the end.
    void      SetAddressSpace  (uint64_t bytes);
    uint64_t  GetAddressSpace  () const { return m_addressSpace; }

    //  The row addresses, or line numbers, in the proportional face, left
    //  aligned in their column; the values and characters stay in the grid
    //  of the fixed-width face. Off by default.
    void      SetProportionalOffsets (bool on)  { m_isProportionalOffsets = on; }
    bool      IsProportionalOffsets  () const   { return m_isProportionalOffsets; }

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

    //  A find's matches marked down the vertical scrollbar, as fractions of
    //  the bytes; see DxuiScrollbar::SetMarks.
    void      SetScrollMarks    (std::vector<DxuiScrollbar::Mark> marks, uint32_t matchArgb, uint32_t currentArgb) { m_scroll.m_vertScroll.SetMarks (std::move (marks), matchArgb, currentArgb); }

    //  Where a byte is drawn, in the same DIPs as the bounds. An off-screen
    //  byte or an absent column comes back empty.
    RECT       GetByteRect      (uint64_t offset, Column column) const { return m_layout.GetByteRect (offset, column); }
    RECT       GetRowOffsetRect (uint64_t row) const { return m_layout.GetRowOffsetRect (row); }
    HitResult  HitTestPoint     (POINT clientDip) const { return m_layout.HitTestPoint (clientDip); }

    //  The source's tip for the byte under a point, and the byte's rect to
    //  anchor it on. None while a selection is dragged or a value is half
    //  typed, so the tip never sits over an edit in progress.
    bool       TryGetByteTipAt  (POINT clientDip, RECT & outCellDip, std::wstring & outTip) const;

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

    void  SetMarkColor     (MarkColorFn fn)   { m_painter.m_markColor = std::move (fn); }

    //  A byte's fill in the value and text columns, under the selection and
    //  the caret, which are drawn over it.
    void  SetByteFill      (ByteFillFn fn)    { m_painter.m_byteFill = std::move (fn); }

    //  A fill for a byte a search matched, zero for none. It is drawn in
    //  place of the caret's and the selection's fills, and the byte's text
    //  over it is in the color that reads on it.
    void  SetMatchFill     (ByteFillFn fn)    { m_painter.m_matchFill = std::move (fn); }
    void  SetOnContextMenu (ContextMenuFn fn) { m_onContextMenu = std::move (fn); }

    //  Outlines around the source's regions, off unless the host turns them
    //  on. Each region gets a rounded box that follows its values, stepping
    //  where it starts or ends partway along a row, with its label centered on
    //  the box's top edge. Rows move apart only where an edge runs between
    //  them, so no line or label crosses a value; in line mode, and with the
    //  values hidden, there are none.
    void  SetShowRegions    (bool show)       { m_regions.SetShow (show); }
    bool  IsShowingRegions  () const          { return m_regions.IsShowing(); }
    bool  AreRegionsDrawn   () const          { return m_regions.IsDrawn(); }

    //  The space above a row that the outlines need, where a row's values
    //  begin down from the bounds' top, and the row a point's height falls in.
    int       GetLaneAbove  (uint64_t row) const { return m_regions.GetLaneAbove (row); }
    int       GetRowTop     (uint64_t row) const { return m_regions.GetRowTop (row); }
    uint64_t  GetRowAtY     (int clientY) const  { return m_regions.GetRowAtY (clientY); }

    using LabelSegment = DxuiHexRegions::LabelSegment;

    static constexpr int    kEdgeLaneDip = DxuiHexRegions::kEdgeLaneDip;
    static constexpr int    kLabelPadDip = DxuiHexRegions::kLabelPadDip;
    static constexpr int    kCornerDip   = DxuiHexRegions::kCornerDip;
    static constexpr float  kLabelScale  = DxuiHexRegions::kLabelScale;
    static constexpr float  kOutlineMix  = DxuiHexRegions::kOutlineMix;

    //  The segment the label of the region that starts at a row's value goes
    //  on. False when no region starts there.
    bool  TryGetLabelSegment (uint64_t row, int unit, LabelSegment & out) const { return m_regions.TryGetLabelSegment (row, unit, out); }

    void  SetOnBuildContextMenu (BuildMenuFn fn) { m_onBuildMenu = std::move (fn); }
    void  SetOnSettingsChanged  (SettingsFn fn)  { m_onSettings  = std::move (fn); }
    void  ShowContextMenu       (POINT atDip);

    //  The menu's items, the host's included; ShowContextMenu shows them.
    void  BuildContextMenu (std::vector<DxuiPopupMenuItem> & items);
    //  The window a copy names as the clipboard's owner.
    void  SetOwnerWindow (HWND hwnd) { m_hwnd = hwnd; }

    //  Space before the address column.
    void  SetPaddingDip (int padDip) { m_layout.m_padDip = padDip; }

    //  A view that fills a pane: the address column starts where the pane's
    //  title does, in place of the padding. Off by default.
    void  SetPaneTextInset (bool on) { m_layout.m_paneTextInset = on; }

    //  The active column. A click sets it, and it determines the format Copy
    //  produces.
    Column  GetActiveColumn () const { return m_selection.GetActiveColumn(); }
    void    SetActiveColumn (Column column) { m_selection.SetActiveColumn (column); }

    //  Puts the caret on a byte and scrolls to it.
    void  GoToOffset (uint64_t offset);

    //  Typing edits the bytes when the view is editable. In the hex column,
    //  digits collect for the value under the caret (the hex format only) and
    //  the value is written, low byte first, as soon as it has all its digits;
    //  in the text column each character is written as it is typed, in the
    //  view's text encoding. The caret moves on after a write the source
    //  takes. Escape or any caret move drops digits not yet written.
    //
    //  An edit of a value is under way from its first digit, from Enter and
    //  from a double-click on it, until its last digit is typed, Enter writes
    //  the digits typed or Escape drops them; a box is drawn over the value
    //  while it is.
    void                  SetEditable       (bool editable)     { m_editable = editable; m_pending.clear(); m_isEditing = false; }
    bool                  IsEditing         () const            { return m_isEditing; }
    uint64_t              GetEditStart      () const            { return m_editStart; }
    bool                  IsEditable        () const            { return m_editable; }
    const std::wstring &  GetPendingDigits  () const            { return m_pending; }
    void                  SetOnWriteRefused (WriteRefusedFn fn) { m_onWriteRefused = std::move (fn); }

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

    //  Whether a point is on a scrollbar, and the hover that widens one.
    bool  IsOverScrollbar    (POINT pt) const override { return m_scroll.m_vertScroll.HitTest (pt.x, pt.y) || m_scroll.m_horzScroll.HitTest (pt.x, pt.y); }
    bool  SetScrollbarHover  (POINT pt)                { return ((int) m_scroll.m_vertScroll.SetHover (m_scroll.m_vertScroll.HitTest (pt.x, pt.y), pt) | (int) m_scroll.m_horzScroll.SetHover (m_scroll.m_horzScroll.HitTest (pt.x, pt.y), pt)) != 0; }
    bool  TickScrollbars     (int64_t nowMs)           { return ((int) m_scroll.m_vertScroll.Tick (nowMs) | (int) m_scroll.m_horzScroll.Tick (nowMs)) != 0; }
    void  SetLeftPx          (int leftPx) { m_scroll.SetLeftPx (leftPx); }
    bool  IsScrollbarVisible () const { return m_layout.GetRowCount() > (uint64_t) (std::max) (m_layout.GetRowCap(), 0); }

    void  Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  OnFocusEntered (bool forward) override;
    void  OnFocusChanged (bool focused) override { m_hasFocus = focused; }
    bool  QueryCommand   (DxuiStandardCommand command, bool & outEnabled) const override;
    bool  InvokeCommand  (DxuiStandardCommand command) override;
    bool  OnMouse        (const DxuiMouseEvent & ev) override;

    //  Windows' scrollbar menu when the point is on one of the bars, in the
    //  coordinates OnMouse takes; empty elsewhere. Its rows scroll the view.
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu  (POINT pt) { return m_scroll.GetScrollbarMenu (pt); }
    bool                            ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, GetScrollbarMenu (pt)); }
    bool                            OnKey             (const DxuiKeyEvent   & ev) override;
    void                            Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Hex view"; }

    static constexpr int  kDefaultBytesPerRow = 16;
    static constexpr int  kDefaultGrouping    = 1;

    //  Character cells between the three columns.
    static constexpr int  kGutterCells = 2;

    static constexpr int  kWheelRows = 3;

    //  The fill behind selected bytes, focused or not, and the ink on it:
    //  the theme's selection, moved off the background until it reaches WCAG
    //  AA contrast for a non-text mark, and whichever of the foreground and
    //  the background reads better over it.
    static constexpr float  kSelectionFillContrast = 3.0f;

    static uint32_t  GetSelectionFill (const IDxuiTheme & theme);
    static uint32_t  GetSelectionInk  (const IDxuiTheme & theme);

    //  The byte the keys act on: white or black, whichever reads better on
    //  the accent behind it.
    static uint32_t  GetCaretInk      (const IDxuiTheme & theme);

    //  The most bytes Copy takes.
    static constexpr uint64_t  kMaxCopyBytes = 16 * 1024 * 1024;

private:

    //  Moves the caret to `offset`, extending the run when `extend` is set and
    //  starting a new one otherwise, then brings it into view.
    uint64_t  GetLastOffset () const;

    //  Typing. Each returns whether it took the character.
    bool        TypeEditChar    (wchar_t ch);
    bool        TypeHexDigit    (wchar_t ch);
    bool        BeginEdit       ();
    bool        CommitEdit      ();
    bool        TypeCharacter   (wchar_t ch);
    void        WriteAndAdvance (uint64_t offset, std::span<const uint8_t> bytes);
    static int  GetDigitValue   (wchar_t ch);

    static constexpr int  s_kSelectionMarginDip = 2;

    static constexpr int  s_kScrollbarWidthDip  = 10;



    const IDxuiHexSource  * m_source                = nullptr;
    uint64_t                m_originAddress         = 0;
    uint64_t                m_addressSpace          = 0;
    bool                    m_isProportionalOffsets = false;
    int                     m_bytesPerRow           = kDefaultBytesPerRow;
    int                     m_grouping              = kDefaultGrouping;
    ValueFormat             m_format                = ValueFormat::Hex;
    bool                    m_showValues            = true;
    bool                    m_breakLines            = false;

    int                            m_cellWidthDip   = 0;
    HWND                           m_hwnd           = nullptr;
    TextEncoding                   m_encoding       = TextEncoding::Ascii;
    DxuiDpiScaler                  m_scaler;
    ContextMenuFn                  m_onContextMenu;
    BuildMenuFn                    m_onBuildMenu;
    SettingsFn                     m_onSettings;
    std::function<void ()>  m_onSelectionChanged;
    bool            m_editable       = false;
    bool            m_hasFocus       = false;
    std::wstring    m_pending;
    uint64_t        m_editStart      = 0;
    bool            m_isEditing      = false;
    uint64_t        m_pressMs        = 0;
    uint64_t        m_pressOffset    = UINT64_MAX;
    WriteRefusedFn  m_onWriteRefused;

    //  The view's parts, each with the state it uses.
    friend class      DxuiHexLayout;
    friend class      DxuiHexPainter;
    friend class      DxuiHexSelection;
    friend class      DxuiHexScroll;
    friend class      DxuiHexRegions;
    DxuiHexLayout     m_layout         { *this };
    DxuiHexPainter    m_painter        { *this };
    DxuiHexSelection  m_selection      { *this };
    DxuiHexScroll     m_scroll         { *this };
    DxuiHexRegions    m_regions        { *this };
};

