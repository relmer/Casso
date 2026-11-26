#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/TextView/DxuiTextPicture.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout
//
//  A text view's rows laid out as lines at a width: the columns of cells,
//  the wrapping of each row's last cell, and the place of each character,
//  by cells for fixed-width text or by measured widths for proportional
//  text.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextLayout
{
public:
    //  A stretch of a cell's text in a color, zero for the text color, and
    //  bold, italic or underlined. An inverse stretch is filled with that
    //  color and its text drawn in the background color.
    struct Span
    {
        int       cell      = 0;
        int       start     = 0;
        int       length    = 0;
        uint32_t  argb      = 0;
        bool      bold      = false;
        bool      italic    = false;
        bool      underline = false;
        bool      inverse   = false;
    };

    using Picture = DxuiTextPicture;

    //  A row of cells. A row of one cell is aligned in the text's width on
    //  each line it wraps onto; Justify draws as Left. A row with a picture
    //  is the picture alone, on as many lines as it is tall.
    struct Row
    {
        std::vector<std::wstring>       cells;
        bool                            warning     = false;
        bool                            info        = false;     // an info mark in the warning mark's place
        bool                            link        = false;     // a click on the row calls the view's row click function
        bool                            heading     = false;     // drawn in the accent color
        std::vector<Span>               spans;
        DxuiTextHAlign                  alignment   = DxuiTextHAlign::Left;
        int                             wrapIndent  = 0;         // characters past the last cell's start that a wrapped line starts at
        std::shared_ptr<const Picture>  picture;
        bool                            rule        = false;     // a line across the text under the row, in the divider color
        int                             columnGroup = 0;         // rows of one group line their cells up in columns; groups are apart

        bool  HasMark () const { return warning || info; }
    };

    //  A character in a row's text, its cells joined by tabs.
    struct Position
    {
        int  row    = 0;
        int  offset = 0;

        bool operator== (const Position & other) const { return row == other.row && offset == other.offset; }
        bool operator<  (const Position & other) const { return row < other.row || (row == other.row && offset < other.offset); }
    };

    //  One drawn line: its row, and the run of the row's last cell on it.
    //  The row's other cells are drawn on its first line only.
    struct Line
    {
        int   row    = 0;
        int   start  = 0;
        int   length = 0;
        bool  first  = true;
    };

    static constexpr int      kColumnGapCells = 2;
    static constexpr int      kWarningCells   = 3;
    static constexpr uint8_t  kStyleBold      = 0x01;
    static constexpr uint8_t  kStyleItalic    = 0x02;
    static constexpr uint8_t  kStyleUnderline = 0x04;
    static constexpr uint8_t  kStyleInverse   = 0x08;

    void                       SetRows          (std::vector<Row> rows);
    const std::vector<Row>   & GetRows          () const { return m_rows; }
    const std::vector<Line>  & GetLines         () const { return m_lines; }
    int                        GetLineCount     () const { return (int) m_lines.size(); }
    int                        GetLineRow       (int line) const { return (line >= 0 && line < (int) m_lines.size()) ? m_lines[(size_t) line].row : -1; }

    bool   SetProportional   (bool proportional);
    bool   IsProportional    () const { return m_proportional; }
    bool   SetNoWrap         (bool noWrap);
    bool   IsNoWrap          () const { return m_noWrap; }
    void   SetCellSize       (int widthPx, int heightPx);
    void   ResetCellSize     ();
    void   ClearMeasurement  () { m_charX.clear(); }
    bool   IsCellPinned      () const { return m_cellPinned; }
    bool   HasCells          () const { return m_cellWidthPx > 0 && m_cellHeightPx > 0; }
    bool   IsMeasured        () const { return m_charX.size() == m_rows.size(); }
    int    GetCellWidthPx    () const { return m_cellWidthPx; }
    int    GetCellHeightPx   () const { return m_cellHeightPx; }
    float  GetCellAdvance    () const { return m_cellAdvance; }
    UINT   GetMeasuredDpi    () const { return m_measuredDpi; }
    bool   MeasureCellSize   (IDxuiTextRenderer & text, float sizePx, const wchar_t * face, UINT dpi);
    void   MeasureCharacters (IDxuiTextRenderer & text, const DxuiFontHandle & font);

    void   ClearLines             ();
    void   BuildColumnCells       ();
    void   BuildLines             (int columns, float pxPerDip);
    void   BuildProportionalLines (float widthPx, float pxPerDip);
    int    FindLine               (const Line & top) const;

    float  GetCellX           (const Row & row, int cell) const;
    float  GetCharsPx         (int row, int cell, int from, int to) const;
    int    GetNearestChar     (int row, int cell, int from, int to, float x) const;
    int    GetCellStart       (const Row & row, int cell) const;
    int    GetContentColumns  () const;
    bool   DoColumnsFit       (float widthPx) const;
    float  GetLineShift       (int lineIndex, int textColumns) const;

    static int                   GetCellBase   (const Row & row, int cell);
    static int                   GetRowLength  (const Row & row);
    static std::wstring          GetRowText    (const Row & row);
    static std::vector<uint8_t>  GetRowStyles  (const Row & row);
    static int                   GetWrapIndent (const Row & row, int avail);
    static DxuiFontWeight        GetWeight     (bool bold, bool italic);

private:
    //  A row's column group's widths, made when absent, or nullptr when absent.
    template <typename T>
    static std::vector<T> & GetGroupColumns (std::vector<std::vector<T>> & groups, const Row & row)
    {
        size_t  group = (size_t) (std::max) (0, row.columnGroup);

        if (groups.size() <= group)
        {
            groups.resize (group + 1);
        }

        return groups[group];
    }

    template <typename T>
    static const std::vector<T> * FindGroupColumns (const std::vector<std::vector<T>> & groups, const Row & row)
    {
        size_t  group = (size_t) (std::max) (0, row.columnGroup);

        return (group < groups.size()) ? &groups[group] : nullptr;
    }

    std::vector<Row>               m_rows;
    std::vector<Line>              m_lines;
    std::vector<std::vector<int>>  m_columnCells;

    //  Proportional text: each row's cells' character places, the width of
    //  the characters before each, and each column's widest cell, in pixels.
    //  Empty until a paint measures them.
    bool                                          m_proportional = false;
    std::vector<std::vector<std::vector<float>>>  m_charX;
    std::vector<std::vector<float>>               m_columnPx;

    //  The face's advance, which is rarely a whole number of pixels. Columns
    //  are placed by it rather than by the rounded cell width, so a line split
    //  into selected and unselected runs lands where the whole line would.
    int    m_cellWidthPx  = 0;
    float  m_cellAdvance  = 0.0f;
    int    m_cellHeightPx = 0;
    bool   m_cellPinned   = false;
    UINT   m_measuredDpi  = 0;
    bool   m_noWrap       = false;
};
