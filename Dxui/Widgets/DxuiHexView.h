#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiHexSource
//
//  The bytes a hex view shows, supplied by the host rather than copied into
//  the widget. A file preview reads from its payload; a debugger reads from
//  the machine's bus. The view asks only for the rows it draws, so a whole
//  address space costs what a screenful does.
//
//  Marks are the host's own: zero means an ordinary byte, and any other value
//  is something the host paints differently -- a byte that changed since the
//  last stop, a byte inside a breakpoint's instruction. The view carries the
//  value through to painting and never interprets it.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiHexSource
{
public:
    virtual ~IDxuiHexSource() = default;

    virtual uint64_t  GetByteCount () const = 0;

    //  Fills `out` with the bytes at `offset`. The view never asks past the
    //  end, so a source may treat a short read as a programming error.
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
//  A hex dump a user can select in: offsets down the left, the bytes in the
//  middle grouped one, two, four or eight at a time, and their characters on
//  the right.
//
//  THE TWO COLUMNS ARE TAB STOPS INSIDE THE ONE WIDGET. Tab into the view
//  lands in the hex column, Tab again moves to the characters, and a third Tab
//  leaves for the next control; Shift+Tab runs the same three steps backwards.
//  So a hex view costs two stops on a walk of the window, not one, and the
//  user never has to reach for the mouse to change which form they are in.
//
//  ONE SELECTION COVERS A RUN OF BYTES, NOT A RUN OF CHARACTERS. Selecting in
//  either column selects bytes, so the same run lights in both columns at once.
//  What differs between the columns is the form the user is working in: the
//  column last clicked is the active one, and it decides what Ctrl+A takes and
//  whether a copy yields hex digits or characters. There is no copy-as-hex and
//  copy-as-text pair of commands -- there is one Copy, and the column the user
//  is in says what it means.
//
//  Offsets are the view's own, counted from the first byte the source holds;
//  the address shown beside a row adds the origin the host set, so the same
//  widget labels a file from zero and a machine's memory from $C000.
//
//  Everything here is geometry and selection -- no device, no text format, no
//  measurement of real glyphs. The host measures one cell of the fixed-width
//  face it paints with and hands the size over, which is what lets the whole
//  of this be exercised headlessly.
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

    //  What the text column makes of a byte. Apple II text carries the high
    //  bit set for ordinary characters and clear for inverse and flashing
    //  ones, so both halves decode to the same letter.
    enum class TextEncoding { Ascii, AppleHighBit };

    //  A color for a marked byte, or false to leave it the ordinary one.
    using MarkColorFn = std::function<bool (uint8_t mark, uint32_t & outArgb)>;

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

    void          SetTextEncoding (TextEncoding encoding) { m_encoding = encoding; }
    TextEncoding  GetTextEncoding () const                { return m_encoding; }

    void  SetMarkColor     (MarkColorFn fn)   { m_markColor = std::move (fn); }
    void  SetOnContextMenu (ContextMenuFn fn) { m_onContextMenu = std::move (fn); }

    //  The window a copy names as the clipboard's owner.
    void  SetOwnerWindow (HWND hwnd) { m_hwnd = hwnd; }

    //  Which column the user is working in. A click sets it, and it decides
    //  what Ctrl+A takes and what a copy yields.
    Column  GetActiveColumn () const { return m_activeColumn; }
    void    SetActiveColumn (Column column);

    //  Puts the caret on a byte and scrolls to it.
    void  GoToOffset (uint64_t offset);

    //  The selection in the form the active column shows it, on the
    //  clipboard. Nothing selected copies nothing.
    void          CopySelection () const;
    std::wstring  GetSelectionText () const;

    //  The characters the text column shows for a run of bytes, and the hex
    //  digits the hex column shows for it. Painting and a copy of the
    //  selection are the same two answers.
    std::wstring  GetTextFor (uint64_t offset, uint64_t count) const;
    std::wstring  GetHexFor  (uint64_t offset, uint64_t count) const;

    //  Told after any change to the selection, including its loss.
    void  SetOnSelectionChanged (std::function<void ()> fn) { m_onSelectionChanged = std::move (fn); }

    bool  IsDragging () const { return m_dragging; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  OnFocusEntered (bool forward) override;
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

    //  One row's bytes and marks, read straight from the source into the
    //  buffers the view reuses frame after frame.
    int   ReadRow (uint64_t row);
    void  PaintRow (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row, int count);
    void  EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    //  One cell run of the fixed-width face, and the fill behind it.
    static void  DrawCell (IDxuiTextRenderer & text, const RECT & rect, const wchar_t * chars, uint32_t argb, const DxuiFontHandle & font);
    static void  FillCell (IDxuiTextRenderer & text, const RECT & rect, uint32_t argb);

    static wchar_t  GetHexDigit (int value);
    wchar_t         GetCharFor  (uint8_t byte) const;

    int   GetHeightDip () const { return m_boundsDip.bottom - m_boundsDip.top; }

    const IDxuiHexSource *  m_source        = nullptr;
    uint64_t                m_originAddress = 0;
    int                     m_bytesPerRow   = kDefaultBytesPerRow;
    int                     m_grouping      = kDefaultGrouping;
    int                     m_cellWidthDip  = 0;
    int                     m_cellHeightDip = 0;
    uint64_t                m_topRow        = 0;
    bool                    m_hasSelection  = false;
    uint64_t                m_anchor        = 0;
    uint64_t                m_caret         = 0;
    Column                  m_activeColumn  = Column::Hex;
    HWND                    m_hwnd          = nullptr;
    bool                    m_dragging      = false;
    TextEncoding            m_encoding      = TextEncoding::Ascii;
    DxuiDpiScaler           m_scaler;
    MarkColorFn             m_markColor;
    ContextMenuFn           m_onContextMenu;
    std::function<void ()>  m_onSelectionChanged;
    std::vector<uint8_t>    m_rowBytes;
    std::vector<uint8_t>    m_rowMarks;
};
