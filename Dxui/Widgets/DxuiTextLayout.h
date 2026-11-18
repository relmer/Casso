#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"
#include "DxuiTextRow.h"


class IDxuiTextRenderer;
struct DxuiFontHandle;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout
//
//  Text as rows of cells laid out on lines: each column's width, the lines
//  the rows wrap onto at a width, and where each character is drawn from
//  the text's left. Fixed-width text is placed by whole cells of the face's
//  advance, proportional text by each character's measured place. It has no
//  window, no scroll position and no painting, so a test can lay out text
//  with a cell size alone.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextLayout
{
public:
    using Row      = DxuiTextRow;
    using Line     = DxuiTextLine;
    using Position = DxuiTextPosition;

    static constexpr int  kColumnGapCells = 2;
    static constexpr int  kWarningCells   = 3;

    //  Setting the rows clears their measured places.
    void                        SetRows     (std::vector<Row> rows) { m_rows = std::move (rows); m_charX.clear(); }
    const std::vector<Row>    & GetRows     () const                { return m_rows; }
    std::vector<std::wstring>   GetRowTexts () const;

    //  One character cell: its width in whole pixels, the face's advance and
    //  a line's height.
    void   SetCellSize     (int widthPx, float advancePx, int heightPx);
    void   ClearCellSize   ()       { m_cellWidthPx = 0; m_cellHeightPx = 0; }
    bool   HasCellSize     () const { return m_cellWidthPx > 0 && m_cellHeightPx > 0; }
    int    GetCellWidthPx  () const { return m_cellWidthPx; }
    float  GetCellAdvance  () const { return m_cellAdvance; }
    int    GetCellHeightPx () const { return m_cellHeightPx; }

    void   SetProportional       (bool proportional) { m_proportional = proportional; m_charX.clear(); }
    bool   IsProportional        () const            { return m_proportional; }
    void   SetNoWrap             (bool noWrap)       { m_noWrap = noWrap; }
    bool   IsNoWrap              () const            { return m_noWrap; }
    void   SetColumnCap          (int column, int cells);
    void   SetColumnGap          (int column, int cells);
    int    GetColumnGap          (int column) const;
    void   SetFirstClippedColumn (int column)        { m_firstClipped = (column >= 0) ? column : -1; }
    int    GetWidestColumns      () const            { return m_widestColumns; }

    //  Proportional text's character places, measured with the renderer.
    void   Measure      (IDxuiTextRenderer & text, const DxuiFontHandle & font);
    bool   IsMeasured   () const { return m_charX.size() == m_rows.size(); }
    void   ClearMeasure ()       { m_charX.clear(); }

    //  A row's text with its cells joined by tabs, and places in it.
    static std::wstring          GetRowText   (const Row & row);
    static int                   GetRowLength (const Row & row);
    static int                   GetCellBase  (const Row & row, int cell);
    static std::vector<uint8_t>  GetRowStyles (const Row & row);
    static bool                  GetLinePart  (const Row & row, const Line & line, int cell, int & outFlat, int & outLength, int & outCellFrom);

    //  Where a row's cells and characters fall, in character columns and
    //  in pixels from the text's left.
    int    GetRowExtent        (const Row & row) const;
    int    GetCellStart        (const Row & row, int cell) const;
    int    GetLineColumn       (const Row & row, const Line & line) const;
    int    GetColumnOfPosition (Position pos) const;
    float  GetCellX            (const Row & row, int cell) const;
    float  GetCharsPx          (int row, int cell, int from, int to) const;
    int    GetNearestChar      (int row, int cell, int from, int to, float x) const;
    float  GetPartPx           (int row, int cell, int cellFrom, int index) const;

    //  The lines: laid out at a width, found by row and position, and
    //  measured from a top line.
    void                       ClearLines        ();
    void                       Wrap              (float textWidthPx, int textColumns, float pxPerDip);
    const std::vector<Line>  & GetLines          () const { return m_lines; }
    int                        GetLineCount      () const { return (int) m_lines.size(); }
    int                        GetFirstLineOfRow (int row) const;
    int                        GetLineOfPosition (Position pos) const;
    int                        GetLineHeightPx   (int lineIndex) const;
    int                        GetLineOffsetPx   (int topLine, int lineIndex) const;
    int                        GetLineCap        (int topLine, int heightPx) const;
    int                        GetTopForLast     (int lineIndex, int heightPx) const;
    int                        GetLinesHeightPx  () const;
    float                      GetLineShift      (int lineIndex, float textWidthPx) const;

private:
    void  BuildColumns           ();
    void  BuildLines             (int columns, float pxPerDip);
    void  BuildProportionalLines (float widthPx, float pxPerDip);
    void  AddPictureLines        (int row, float widthPx, float pxPerDip);

    std::vector<Row>    m_rows;
    std::vector<Line>   m_lines;
    std::vector<int>    m_columnCells;
    std::map<int, int>  m_columnCaps;
    std::map<int, int>  m_columnGaps;
    int                 m_cellWidthPx   = 0;
    int                 m_cellHeightPx  = 0;
    bool                m_proportional  = false;
    bool                m_noWrap        = false;
    int                 m_widestColumns = 0;
    int                 m_firstClipped  = -1;

    //  The face's advance, which is rarely a whole number of pixels. Columns
    //  are placed by it rather than by the rounded cell width, so a line split
    //  into selected and unselected runs lands where the whole line would.
    float               m_cellAdvance   = 0.0f;

    //  Each row's cells' character places, the width of the characters
    //  before each, and each column's widest cell, in pixels. Empty until a
    //  paint measures them.
    std::vector<std::vector<std::vector<float>>>  m_charX;
    std::vector<float>                            m_columnPx;
};
