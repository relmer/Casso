#include "Pch.h"

#include "DxuiTextLayout.h"
#include "DxuiTextRowLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetRows
//
//  New rows are measured again before proportional text is laid out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::SetRows (std::vector<Row> rows)
{
    m_rows = std::move (rows);

    m_charX.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetProportional
//
//  Returns whether the setting changed. A change measures the characters
//  again, and the cells too unless the host set them.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextLayout::SetProportional (bool proportional)
{
    if (proportional == m_proportional)
    {
        return false;
    }

    m_proportional = proportional;
    m_charX.clear();

    ResetCellSize();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetNoWrap
//
//  Returns whether the setting changed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextLayout::SetNoWrap (bool noWrap)
{
    if (noWrap == m_noWrap)
    {
        return false;
    }

    m_noWrap = noWrap;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::SetCellSize
//
//  A size set here stays the same across DPI changes, as a test's does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::SetCellSize (int widthPx, int heightPx)
{
    m_cellWidthPx  = (std::max) (widthPx,  0);
    m_cellHeightPx = (std::max) (heightPx, 0);
    m_cellAdvance  = (float) m_cellWidthPx;
    m_cellPinned   = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::ResetCellSize
//
//  The cells are measured again on the next paint, unless the host set them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::ResetCellSize()
{
    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::MeasureCellSize
//
//  Measured at the size the face is drawn at, so cells and glyphs are the same width.
//  Returns whether the cells were measured; false when they already were.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextLayout::MeasureCellSize (IDxuiTextRenderer & text, float sizePx, const wchar_t * face, UINT dpi)
{
    float    width  = 0.0f;
    float    height = 0.0f;
    HRESULT  hr     = S_OK;



    if (m_cellWidthPx > 0)
    {
        return false;
    }

    hr = text.MeasureString (L"00000000", sizePx, face, width, height);

    if (FAILED (hr) || width <= 0.0f || height <= 0.0f)
    {
        return false;
    }

    m_cellWidthPx  = (std::max) (1, (int) (width / 8.0f + 0.5f));
    m_cellAdvance  = (std::max) (1.0f, width / 8.0f);
    m_cellHeightPx = (std::max) (1, (int) (height + 0.5f));
    m_measuredDpi  = dpi;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::MeasureCharacters
//
//  The width of each cell's characters up to each place, measured with a
//  mark after them so a trailing space counts, less the mark's width.
//  Each column is as wide as its widest cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::MeasureCharacters (IDxuiTextRenderer & text, const DxuiFontHandle & font)
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
                uint8_t  style = styleAt (base + i - 1);

                if (i == 1 || style != styleAt (base + runStart))
                {
                    runStart = i - 1;
                    runLeft  = places[i - 1];

                    hr = text.MeasureStringWeighted (L"|", font.sizeDip, font.face, GetWeight ((style & kStyleBold) != 0, (style & kStyleItalic) != 0), runMarkW, height);
                    runMarkW = SUCCEEDED (hr) ? runMarkW : markW;
                }

                hr = text.MeasureStringWeighted ((cell.substr (runStart, i - runStart) + L"|").c_str(), font.sizeDip, font.face, GetWeight ((style & kStyleBold) != 0, (style & kStyleItalic) != 0), width, height);

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
//  DxuiTextLayout::BuildColumnCells
//
//  Each column but the last is as many characters wide as its widest cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::BuildColumnCells()
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::BuildLines
//
//  Wraps each row's last cell at the last space that fits, or mid-word when
//  none does. With no wrap, a row is one line, except a warning, which wraps
//  in the view's width so its message can be read without scrolling.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextLayout::BuildLines (int columns, float pxPerDip)
{
    m_lines.clear();

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        int             start  = GetCellStart (row, (int) row.cells.size() - 1);
        int             length = (int) last.size();
        int             avail  = (m_noWrap && !row.warning) ? (std::max) (1, length) : (std::max) (1, columns - start);
        int             indent = GetWrapIndent (row, avail);
        int             pos    = 0;
        bool            first  = true;

        if (row.picture != nullptr)
        {
            for (int i = 0; i < DxuiTextRowLayout::GetPictureLines (*row.picture, (float) columns * m_cellAdvance, pxPerDip, m_cellHeightPx); i++)
            {
                m_lines.push_back (Line { r, 0, 0, i == 0 });
            }

            continue;
        }

        do
        {
            int     take  = (std::min) (first ? avail : avail - indent, length - pos);
            size_t  space = std::wstring::npos;

            if (pos + take < length)
            {
                space = last.rfind (L' ', (size_t) (pos + take - 1));
                take  = (space != std::wstring::npos && (int) space >= pos) ? (int) space + 1 - pos : take;
            }

            m_lines.push_back (Line { r, pos, take, first });

            pos  += take;
            first = false;
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
        const Row &           row    = m_rows[(size_t) r];
        int                   cell   = (int) row.cells.size() - 1;
        std::wstring          last   = row.cells.empty() ? std::wstring() : row.cells.back();
        float                 avail  = (std::max) (m_cellAdvance, widthPx - GetCellX (row, cell));
        int                   length = (int) last.size();
        int                   pos    = 0;
        bool                  first  = true;

        if (row.picture != nullptr)
        {
            for (int i = 0; i < DxuiTextRowLayout::GetPictureLines (*row.picture, widthPx, pxPerDip, m_cellHeightPx); i++)
            {
                m_lines.push_back (Line { r, 0, 0, i == 0 });
            }

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
//  DxuiTextLayout::FindLine
//
//  After a new wrap, the line that has the text a line of the old wrap
//  started with.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::FindLine (const Line & top) const
{
    int  line = 0;



    while (line + 1 < (int) m_lines.size()
           && (m_lines[(size_t) line + 1].row < top.row || (m_lines[(size_t) line + 1].row == top.row && m_lines[(size_t) line + 1].start <= top.start)))
    {
        line++;
    }

    return line;
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
        x += m_columnPx[(size_t) c] + (float) kColumnGapCells * m_cellAdvance;
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
    const std::vector<float> *  places = nullptr;



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
//  DxuiTextLayout::GetCellStart
//
//  The character column a cell begins in: past the warning mark, and past
//  every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetCellStart (const Row & row, int cell) const
{
    int  column = row.warning ? kWarningCells : 0;



    for (int c = 0; c < cell && c < (int) m_columnCells.size(); c++)
    {
        column += m_columnCells[(size_t) c] + kColumnGapCells;
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetContentColumns
//
//  The widest row in character columns.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetContentColumns() const
{
    int  widest = 0;



    //  A warning wraps, so it doesn't widen the content.
    for (const Row & row : m_rows)
    {
        if (!row.cells.empty() && !row.warning)
        {
            widest = (std::max) (widest, GetCellStart (row, (int) row.cells.size() - 1) + (int) row.cells.back().size());
        }
    }

    return widest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::DoColumnsFit
//
//  Whether every row of more than one cell fits a width with each cell at
//  its natural width. A row's last cell starts past the columns before it,
//  and its natural width is its last character's place. True before the
//  text is measured.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextLayout::DoColumnsFit (float widthPx) const
{
    bool  fits = true;



    for (size_t r = 0; fits && r < m_rows.size() && r < m_charX.size(); r++)
    {
        const Row &  row  = m_rows[r];
        int          last = (int) row.cells.size() - 1;

        if (last > 0 && (size_t) last < m_charX[r].size() && !m_charX[r][(size_t) last].empty())
        {
            fits = GetCellX (row, last) + m_charX[r][(size_t) last].back() <= widthPx;
        }
    }

    return fits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetLineShift
//
//  How far a line of a centered or right-aligned row of one cell starts
//  from the left of the text, in pixels, in a text `textColumns` wide.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextLayout::GetLineShift (int lineIndex, int textColumns) const
{
    float  width = 0.0f;
    float  used  = 0.0f;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size())
    {
        return 0.0f;
    }

    const Line &  line = m_lines[(size_t) lineIndex];
    const Row &   row  = m_rows[(size_t) line.row];

    if (!line.first && !m_proportional && row.wrapIndent > 0)
    {
        int  avail = textColumns - GetCellStart (row, (int) row.cells.size() - 1);

        return (float) GetWrapIndent (row, (std::max) (1, avail)) * m_cellAdvance;
    }

    if (row.alignment == DxuiTextHAlign::Left || row.cells.size() != 1 || m_noWrap)
    {
        return 0.0f;
    }

    width = (float) textColumns * m_cellAdvance;
    used  = GetCharsPx (line.row, 0, line.start, line.start + line.length);

    return (row.alignment == DxuiTextHAlign::Right) ? (std::max) (0.0f, width - used) : (std::max) (0.0f, width - used) / 2.0f;
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
//  DxuiTextLayout::GetRowLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetRowLength (const Row & row)
{
    return row.cells.empty() ? 0 : GetCellBase (row, (int) row.cells.size() - 1) + (int) row.cells.back().size();
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
//  DxuiTextLayout::GetRowStyles
//
//  Each character's style bits by the row's spans; empty for a row with no
//  styled span.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> DxuiTextLayout::GetRowStyles (const Row & row)
{
    std::vector<uint8_t>  styles;



    for (const Span & span : row.spans)
    {
        uint8_t  bits   = (uint8_t) ((span.bold ? kStyleBold : 0) | (span.italic ? kStyleItalic : 0) | (span.underline ? kStyleUnderline : 0) | (span.inverse ? kStyleInverse : 0));
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
//  DxuiTextLayout::GetWrapIndent
//
//  A row's wrap indent in a last cell `avail` characters wide, or zero
//  when less than half of that width would remain for the wrapped text.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextLayout::GetWrapIndent (const Row & row, int avail)
{
    return (row.wrapIndent > 0 && row.wrapIndent * 2 <= avail) ? row.wrapIndent : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayout::GetWeight
//
//  Italic is a bit above the weights, so one value has a weight and the
//  italic style.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFontWeight DxuiTextLayout::GetWeight (bool bold, bool italic)
{
    int  weight = bold ? (int) DxuiFontWeight::Bold : (int) DxuiFontWeight::Normal;



    weight |= italic ? ((int) DxuiFontWeight::Italic & ~(int) DxuiFontWeight::Normal) : 0;

    return (DxuiFontWeight) weight;
}
