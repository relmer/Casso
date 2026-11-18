#include "Pch.h"

#include "DxuiTextLayout.h"
#include "DxuiTextRowLayout.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetRowTexts
//
//  Each row's text, its cells joined by tabs, as find searches it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DxuiTextLayout::GetRowTexts() const
{
    std::vector<std::wstring>  texts;



    texts.reserve (m_rows.size());

    for (const Row & row : m_rows)
    {
        texts.push_back (GetRowText (row));
    }

    return texts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetCellSize
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::SetCellSize (int widthPx, float advancePx, int heightPx)
{
    m_cellWidthPx  = widthPx;
    m_cellAdvance  = advancePx;
    m_cellHeightPx = heightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetColumnCap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::SetColumnCap (int column, int cells)
{
    if (cells > 0)
    {
        m_columnCaps[column] = cells;
    }
    else
    {
        m_columnCaps.erase (column);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetColumnGap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::SetColumnGap (int column, int cells)
{
    if (cells > 0)
    {
        m_columnGaps[column] = cells;
    }
    else
    {
        m_columnGaps.erase (column);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetColumnGap
//
//  The characters after a column, kColumnGapCells unless set.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetColumnGap (int column) const
{
    auto  gap = m_columnGaps.find (column);



    return (gap != m_columnGaps.end()) ? gap->second : kColumnGapCells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::Measure
//
//  The width of each cell's characters up to each place, measured with a
//  mark after them so a trailing space counts, less the mark's width. Each
//  column is as wide as its widest cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::Measure (IDxuiTextRenderer & text, const DxuiFontHandle & font)
{
    HRESULT  hr     = S_OK;
    float    markW  = 0.0f;
    float    width  = 0.0f;
    float    height = 0.0f;



    hr = text.MeasureString (L"|", font.sizeDip, font.face, markW, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_charX.assign (m_rows.size(), {});
    m_columnPx.clear();

    for (size_t r = 0; r < m_rows.size(); r++)
    {
        const Row &           row    = m_rows[r];
        std::vector<uint8_t>  styles = GetRowStyles (row);

        auto  styleAt = [&] (size_t flat) { return (flat < styles.size()) ? styles[flat] : (uint8_t) 0; };

        m_charX[r].resize (row.cells.size());

        for (size_t c = 0; c < row.cells.size(); c++)
        {
            const std::wstring &  cell     = row.cells[c];
            std::vector<float> &  places   = m_charX[r][c];
            size_t                base     = (size_t) GetCellBase (row, (int) c);
            size_t                runStart = 0;
            float                 runLeft  = 0.0f;
            float                 runMarkW = markW;

            places.assign (cell.size() + 1, 0.0f);

            //  A bold or italic run is wider or narrower than the same
            //  characters in the regular face, so each run of one style is
            //  measured in its weight, from where the run before it ends.
            for (size_t i = 1; i <= cell.size(); i++)
            {
                uint8_t         style  = styleAt (base + i - 1);
                DxuiFontWeight  weight = DxuiTextRowLayout::GetWeight ((style & DxuiTextSpan::kStyleBold) != 0, (style & DxuiTextSpan::kStyleItalic) != 0);

                if (i == 1 || style != styleAt (base + runStart))
                {
                    runStart = i - 1;
                    runLeft  = places[i - 1];

                    hr = text.MeasureStringWeighted (L"|", font.sizeDip, font.face, weight, runMarkW, height);
                    runMarkW = SUCCEEDED (hr) ? runMarkW : markW;
                }

                hr = text.MeasureStringWeighted ((cell.substr (runStart, i - runStart) + L"|").c_str(), font.sizeDip, font.face, weight, width, height);

                places[i] = SUCCEEDED (hr) ? (std::max) (places[i - 1], runLeft + width - runMarkW) : places[i - 1] + m_cellAdvance;
            }

            if (c + 1 < row.cells.size())
            {
                if (m_columnPx.size() <= c)
                {
                    m_columnPx.resize (c + 1, 0.0f);
                }

                m_columnPx[c] = (std::max) (m_columnPx[c], places.back());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetRowText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextLayout::GetRowText (const Row & row)
{
    std::wstring  text;



    for (size_t c = 0; c < row.cells.size(); c++)
    {
        text += (c > 0 ? L"\t" : L"") + row.cells[c];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetRowLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetRowLength (const Row & row)
{
    return row.cells.empty() ? 0 : GetCellBase (row, (int) row.cells.size() - 1) + (int) row.cells.back().size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetCellBase
//
//  Where a cell's first character is in the row's text.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetCellBase (const Row & row, int cell)
{
    int  base = 0;



    for (int c = 0; c < cell && c < (int) row.cells.size(); c++)
    {
        base += (int) row.cells[(size_t) c].size() + 1;
    }

    return base;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetRowStyles
//
//  Each character's style bits by the row's spans, empty for a row with no
//  styled span.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> DxuiTextLayout::GetRowStyles (const Row & row)
{
    std::vector<uint8_t>  styles;



    for (const DxuiTextSpan & span : row.spans)
    {
        uint8_t  bits   = (uint8_t) ((span.bold ? DxuiTextSpan::kStyleBold : 0) | (span.italic ? DxuiTextSpan::kStyleItalic : 0) | (span.underline ? DxuiTextSpan::kStyleUnderline : 0) | (span.inverse ? DxuiTextSpan::kStyleInverse : 0));
        int      base   = GetCellBase (row, span.cell);
        int      length = (span.cell < (int) row.cells.size()) ? (int) row.cells[(size_t) span.cell].size() : 0;

        if (bits == 0)
        {
            continue;
        }

        if (styles.empty())
        {
            styles.assign ((size_t) GetRowLength (row), 0);
        }

        for (int i = (std::max) (0, span.start); i < span.start + span.length && i < length; i++)
        {
            styles[(size_t) (base + i)] |= bits;
        }
    }

    return styles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLinePart
//
//  The part of a cell drawn on a line: where it starts in the row's text,
//  its length and where it starts in the cell. False when none of the cell
//  is on the line, as a wrapped line has only the last cell's text.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextLayout::GetLinePart (const Row & row, const Line & line, int cell, int & outFlat, int & outLength, int & outCellFrom)
{
    int  last = (int) row.cells.size() - 1;



    if (cell < 0 || cell > last)
    {
        return false;
    }

    if (cell == last)
    {
        outFlat     = GetCellBase (row, last) + line.start;
        outLength   = line.length;
        outCellFrom = line.start;
        return true;
    }

    if (!line.first)
    {
        return false;
    }

    outFlat     = GetCellBase (row, cell);
    outLength   = (int) row.cells[(size_t) cell].size();
    outCellFrom = 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetRowExtent
//
//  The column just past a row's last character that counts toward the
//  sideways scroll. An empty cell adds nothing, not even the gap ahead of
//  it, and neither does a cell in a clipped column.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetRowExtent (const Row & row) const
{
    int  extent = row.warning ? kWarningCells : 0;



    for (int c = 0; c < (int) row.cells.size(); c++)
    {
        if (row.cells[(size_t) c].empty() || (m_firstClipped >= 0 && c >= m_firstClipped))
        {
            continue;
        }

        extent = (std::max) (extent, GetCellStart (row, c) + (int) row.cells[(size_t) c].size());
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetCellStart
//
//  The character column a cell begins in: past the warning mark, and past
//  every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetCellStart (const Row & row, int cell) const
{
    int  column = row.warning ? kWarningCells : 0;



    //  A cell longer than its capped column pushes the cells after it along.
    for (int c = 0; c < cell && c < (int) m_columnCells.size(); c++)
    {
        int  length = (c < (int) row.cells.size()) ? (int) row.cells[(size_t) c].size() : 0;

        column += (std::max) (m_columnCells[(size_t) c], length) + GetColumnGap (c);
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineColumn
//
//  The column a line of a row's last cell starts at: the cell's, then its
//  wrap indent on each wrapped line.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetLineColumn (const Row & row, const Line & line) const
{
    return GetCellStart (row, (int) row.cells.size() - 1) + (line.first ? 0 : (std::max) (0, row.wrapIndent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetColumnOfPosition
//
//  The character column a position's character is drawn in, before any
//  sideways scroll; zero for a row not laid out.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetColumnOfPosition (Position pos) const
{
    int  column = 0;



    if (pos.row < 0 || pos.row >= (int) m_rows.size())
    {
        return 0;
    }

    const Row  & row = m_rows[(size_t) pos.row];

    for (int c = 0; c < (int) row.cells.size(); c++)
    {
        if (c + 1 == (int) row.cells.size() || pos.offset < GetCellBase (row, c + 1))
        {
            column = GetCellStart (row, c) + pos.offset - GetCellBase (row, c);
            break;
        }
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetCellX
//
//  Where a cell begins on a line, from the line's left: past the warning
//  mark, and past every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextLayout::GetCellX (const Row & row, int cell) const
{
    float  x = row.warning ? (float) kWarningCells * m_cellAdvance : 0.0f;



    if (!m_proportional)
    {
        return (float) GetCellStart (row, cell) * m_cellAdvance;
    }

    for (int c = 0; c < cell && c < (int) m_columnPx.size(); c++)
    {
        x += m_columnPx[(size_t) c] + (float) GetColumnGap (c) * m_cellAdvance;
    }

    return x;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetCharsPx
//
//  How wide a cell's characters from `from` up to `to` are drawn.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextLayout::GetCharsPx (int row, int cell, int from, int to) const
{
    const std::vector<float>  * places = nullptr;



    if (m_proportional && row >= 0 && (size_t) row < m_charX.size() && cell >= 0 && (size_t) cell < m_charX[(size_t) row].size())
    {
        places = &m_charX[(size_t) row][(size_t) cell];
    }

    if (places == nullptr || places->empty())
    {
        return (float) (to - from) * m_cellAdvance;
    }

    from = std::clamp (from, 0, (int) places->size() - 1);
    to   = std::clamp (to,   0, (int) places->size() - 1);

    return (*places)[(size_t) to] - (*places)[(size_t) from];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetNearestChar
//
//  The boundary between a cell's characters, from `from` to `to`, nearest a
//  point `x` from where `from` is drawn, as an offset in the cell.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetNearestChar (int row, int cell, int from, int to, float x) const
{
    int  best = from;



    for (int i = from + 1; i <= to; i++)
    {
        if (GetCharsPx (row, cell, from, i) - GetCharsPx (row, cell, i - 1, i) / 2.0f > x)
        {
            break;
        }

        best = i;
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetPartPx
//
//  How far from the text's left the character `index` places into a part
//  of a cell drawn on a line, starting at `cellFrom` in the cell, is drawn:
//  a wrapped line of the last cell starts at its wrap indent. A wrapped line
//  is one of the last cell's that starts past its first character.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextLayout::GetPartPx (int row, int cell, int cellFrom, int index) const
{
    const Row  & rowData = m_rows[(size_t) row];
    bool         wrapped = cell == (int) rowData.cells.size() - 1 && cellFrom > 0;
    float        indent  = wrapped ? (float) (std::max) (0, rowData.wrapIndent) * m_cellAdvance : 0.0f;



    return GetCellX (rowData, cell) + indent + GetCharsPx (row, cell, cellFrom, cellFrom + index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::ClearLines
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::ClearLines()
{
    m_lines.clear();
    m_columnCells.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::Wrap
//
//  Proportional text is wrapped at `textWidthPx` by its measured places,
//  and has no lines until it is measured. Fixed-width text is wrapped at
//  `textColumns` characters, its column widths taken from every row.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::Wrap (float textWidthPx, int textColumns, float pxPerDip)
{
    if (m_proportional)
    {
        m_widestColumns = 0;

        if (IsMeasured())
        {
            BuildProportionalLines (textWidthPx, pxPerDip);
        }

        return;
    }

    BuildColumns();
    BuildLines (textColumns, pxPerDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::BuildColumns
//
//  Each column but the last as wide as its widest cell, or its cap.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::BuildColumns()
{
    m_columnCells.clear();

    for (const Row & row : m_rows)
    {
        for (size_t c = 0; c + 1 < row.cells.size(); c++)
        {
            if (m_columnCells.size() <= c)
            {
                m_columnCells.resize (c + 1, 0);
            }

            m_columnCells[c] = (std::max) (m_columnCells[c], (int) row.cells[c].size());
        }
    }

    for (const auto & [column, cells] : m_columnCaps)
    {
        if (column < (int) m_columnCells.size() && cells > 0)
        {
            m_columnCells[(size_t) column] = (std::min) (m_columnCells[(size_t) column], cells);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::BuildLines
//
//  Wraps each row's last cell at the last space that fits, or mid-word when
//  none does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::BuildLines (int columns, float pxPerDip)
{
    m_lines.clear();
    m_widestColumns = 0;

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        int             start  = GetCellStart (row, (int) row.cells.size() - 1);
        int             indent = (std::max) (0, row.wrapIndent);
        int             avail  = (m_noWrap && !row.warning) ? INT_MAX : (std::max) (1, columns - start);
        int             length = (int) last.size();
        int             pos    = 0;
        bool            first  = true;

        if (row.picture != nullptr)
        {
            AddPictureLines (r, (float) columns * m_cellAdvance, pxPerDip);
            continue;
        }

        m_widestColumns = row.warning ? m_widestColumns : (std::max) (m_widestColumns, GetRowExtent (row));

        do
        {
            int     take  = (std::min) (avail, length - pos);
            size_t  space = std::wstring::npos;

            if (pos + take < length)
            {
                space = last.rfind (L' ', (size_t) (pos + take - 1));
                take  = (space != std::wstring::npos && (int) space >= pos) ? (int) space + 1 - pos : take;
            }

            m_lines.push_back (Line { r, pos, take, first });

            pos  += take;
            first = false;
            avail = (m_noWrap && !row.warning) ? INT_MAX : (std::max) (1, columns - start - indent);
        }
        while (pos < length);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::BuildProportionalLines
//
//  As BuildLines, by the measured width of the characters rather than their
//  count.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::BuildProportionalLines (float widthPx, float pxPerDip)
{
    m_lines.clear();

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        int             cell   = (int) row.cells.size() - 1;
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        float           avail  = (std::max) (m_cellAdvance, widthPx - GetCellX (row, cell));
        int             length = (int) last.size();
        int             pos    = 0;
        bool            first  = true;

        if (row.picture != nullptr)
        {
            AddPictureLines (r, widthPx, pxPerDip);
            continue;
        }

        do
        {
            int     take  = 1;
            size_t  space = std::wstring::npos;

            while (pos + take < length && GetCharsPx (r, cell, pos, pos + take + 1) <= avail)
            {
                take++;
            }

            take = (std::min) (take, length - pos);

            if (pos + take < length)
            {
                space = last.rfind (L' ', (size_t) (pos + take - 1));
                take  = (space != std::wstring::npos && (int) space >= pos) ? (int) space + 1 - pos : take;
            }

            m_lines.push_back (Line { r, pos, (std::max) (take, 0), first });

            pos  += (std::max) (take, 1);
            first = false;
        }
        while (pos < length);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::AddPictureLines
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::AddPictureLines (int row, float widthPx, float pxPerDip)
{
    int  count = DxuiTextRowLayout::GetPictureLines (*m_rows[(size_t) row].picture, widthPx, pxPerDip, m_cellHeightPx);



    for (int i = 0; i < count; i++)
    {
        m_lines.push_back (Line { row, 0, 0, i == 0 });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetFirstLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetFirstLineOfRow (int row) const
{
    for (size_t i = 0; i < m_lines.size(); i++)
    {
        if (m_lines[i].row == row)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineOfPosition
//
//  The drawn line a position falls on: its row's first line, or a later one
//  when the position is in the part of the last cell that wrapped. -1 when
//  the row is not laid out.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetLineOfPosition (Position pos) const
{
    int  line = GetFirstLineOfRow (pos.row);
    int  base = 0;



    if (line < 0)
    {
        return line;
    }

    base = GetCellBase (m_rows[(size_t) pos.row], (int) m_rows[(size_t) pos.row].cells.size() - 1);

    while (line + 1 < (int) m_lines.size() && m_lines[(size_t) line + 1].row == pos.row &&
           base + m_lines[(size_t) line + 1].start <= pos.offset)
    {
        line++;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineHeightPx
//
//  A full line, or the fraction of one a short row asks for, never less
//  than a pixel.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetLineHeightPx (int lineIndex) const
{
    float  fraction = m_rows[(size_t) m_lines[(size_t) lineIndex].row].height;



    if (fraction >= 1.0f)
    {
        return m_cellHeightPx;
    }

    return (std::max) (1, (int) ((float) m_cellHeightPx * fraction + 0.5f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineOffsetPx
//
//  How far below the first line shown a line's top is; negative for a line
//  above it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetLineOffsetPx (int topLine, int lineIndex) const
{
    int  offset = 0;



    for (int line = topLine; line < lineIndex; line++)
    {
        offset += (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;
    }

    for (int line = lineIndex; line < topLine; line++)
    {
        offset -= GetLineHeightPx (line);
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineCap
//
//  The lines from `topLine` that fit in `heightPx`, at least one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetLineCap (int topLine, int heightPx) const
{
    int  used  = 0;
    int  count = 0;
    int  line  = topLine;



    if (m_cellHeightPx <= 0)
    {
        return 1;
    }

    //  Past the end, a line is taken as a full one, so the count does not
    //  depend on how much text there is.
    for (;;)
    {
        int  lineHeight = (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;

        if (used + lineHeight > heightPx)
        {
            break;
        }

        used += lineHeight;
        count++;
        line++;
    }

    return (std::max) (1, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetTopForLast
//
//  The top line that puts `lineIndex` last among the lines that fit in
//  `heightPx`.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetTopForLast (int lineIndex, int heightPx) const
{
    int  top  = lineIndex;
    int  used = 0;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size() || m_cellHeightPx <= 0)
    {
        return 0;
    }

    used = GetLineHeightPx (lineIndex);

    while (top > 0 && used + GetLineHeightPx (top - 1) <= heightPx)
    {
        top--;
        used += GetLineHeightPx (top);
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLinesHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetLinesHeightPx() const
{
    int  height = 0;



    for (int line = 0; line < (int) m_lines.size(); line++)
    {
        height += GetLineHeightPx (line);
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineShift
//
//  How far a line of a centered or right-aligned row of one cell starts
//  from the left of the text, in pixels.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextLayout::GetLineShift (int lineIndex, float textWidthPx) const
{
    float  used = 0.0f;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size())
    {
        return 0.0f;
    }

    const Line  & line = m_lines[(size_t) lineIndex];
    const Row   & row  = m_rows[(size_t) line.row];

    if (row.alignment == DxuiTextHAlign::Left || row.cells.size() != 1 || m_noWrap)
    {
        return 0.0f;
    }

    used = GetCharsPx (line.row, 0, line.start, line.start + line.length);

    return (row.alignment == DxuiTextHAlign::Right) ? (std::max) (0.0f, textWidthPx - used) : (std::max) (0.0f, textWidthPx - used) / 2.0f;
}
