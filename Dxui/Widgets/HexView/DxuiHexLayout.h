#pragma once

#include "Pch.h"

#include "Widgets/HexView/DxuiHexViewTypes.h"

class DxuiHexView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayout
//
//  Where each byte is: the rows over the bytes, by width or by line
//  breaks, a row's span and an offset's row and column, the cells of the
//  offset, hex and text columns, their rects, and the byte under a point.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexLayout
{
public:
    explicit DxuiHexLayout (DxuiHexView & view);

    uint64_t  GetRowCount     () const;
    int       GetRowCap       () const;
    uint64_t  GetMaxTopRow    () const;

    RECT       GetByteRect      (uint64_t offset, DxuiHexViewTypes::Column column) const;
    RECT       GetRowOffsetRect (uint64_t row) const;
    DxuiHexViewTypes::HitResult  HitTestPoint     (POINT clientDip) const;

    //  Hex digits the offset column spends, which is eight once the last
    //  address does not fit in four.
    int   GetOffsetDigits () const;
    int   GetHexCells     () const;
    int   GetGroupCount   () const;

    //  First character cell of a byte's two hex digits, within the hex column.
    int   GetByteCellInRow (int indexInRow) const;
    int   GetColumnStartCell (DxuiHexViewTypes::Column column) const;
    RECT  GetCellRect (int cellX, uint64_t row, int cellCount) const;
    bool  IsRowVisible (uint64_t row) const;
    void  ClampTopRow ();
    RECT  GetSelectionCellRect (uint64_t offset, int index, const RECT & cell, bool hexColumn) const;

    //  Cells one value spends, its text, and the fill behind a selected one.
    int           GetValueCells         () const;
    uint64_t      SnapToValue           (uint64_t offset, bool toEnd) const;
    RECT          GetValueSelectionRect (uint64_t first, const RECT & cell) const;

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

    //  The rows' height, less the horizontal scrollbar when it shows; the
    //  full width a row needs; and the width left for rows beside the
    //  vertical scrollbar, whose room is always kept.
    int   GetHeightDip      () const;
    int   GetContentWidthPx () const;
    int   GetViewWidthPx    () const;

private:
    friend class DxuiHexView;

    DxuiHexView  & m_view;

    int                     m_columns       = DxuiHexViewTypes::s_kFixedRowWidth;

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
    int                            m_cellHeightDip  = 0;
    uint64_t                       m_topRow         = 0;
    bool                           m_hasSelection   = false;
    uint64_t                       m_caret          = 0;
    int                            m_padDip         = 0;
    int                            m_leftPx         = 0;
};
