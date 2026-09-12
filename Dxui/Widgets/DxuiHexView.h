#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





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
//  ONE SELECTION COVERS A RUN OF BYTES, NOT A RUN OF CHARACTERS. Selecting in
//  either column selects bytes, so the same run lights in both columns at once
//  and a selection made in the hex column can be copied as text and the other
//  way around. The column a selection was made in is remembered only so a copy
//  can default to that form.
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
    Column    GetSelectionColumn () const { return m_column; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Hex view"; }

    static constexpr int  kDefaultBytesPerRow = 16;
    static constexpr int  kDefaultGrouping    = 1;

    //  Character cells between the three columns.
    static constexpr int  kGutterCells = 2;

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
    Column                  m_column        = Column::None;
    DxuiDpiScaler           m_scaler;
};
