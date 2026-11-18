#include "Pch.h"

#include "DxuiTextViewMetrics.h"
#include "DxuiTextRowLayout.h"
#include "DxuiTextView.h"
#include "IDxuiTextMetricsHost.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewMetrics::Measure
//
//  The width of each cell's characters up to each place, measured with a
//  mark after them so a trailing space counts, less the mark's width. Each
//  column is as wide as its widest cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewMetrics::Measure (IDxuiTextRenderer & text, const DxuiFontHandle & font)
{
    const std::vector<Row>  & rows   = m_host.GetMetricsRows();
    HRESULT                   hr     = S_OK;
    float                     markW  = 0.0f;
    float                     width  = 0.0f;
    float                     height = 0.0f;



    hr = text.MeasureString (L"|", font.sizeDip, font.face, markW, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_charX.assign (rows.size(), {});
    m_columnPx.clear();

    for (size_t r = 0; r < rows.size(); r++)
    {
        const Row &           row    = rows[r];
        std::vector<uint8_t>  styles = m_host.GetRowStyles (row);

        auto  styleAt = [&] (size_t flat) { return (flat < styles.size()) ? styles[flat] : (uint8_t) 0; };

        m_charX[r].resize (row.cells.size());

        for (size_t c = 0; c < row.cells.size(); c++)
        {
            const std::wstring &  cell     = row.cells[c];
            std::vector<float> &  places   = m_charX[r][c];
            size_t                base     = (size_t) m_host.GetCellBase (row, (int) c);
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

                places[i] = SUCCEEDED (hr) ? (std::max) (places[i - 1], runLeft + width - runMarkW) : places[i - 1] + m_host.GetMetricsAdvance();
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
//  DxuiTextViewMetrics::GetCellX
//
//  Where a cell begins on a line, from the line's left: past the warning
//  mark, and past every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextViewMetrics::GetCellX (const Row & row, int cell) const
{
    float  x = row.warning ? (float) DxuiTextView::kWarningCells * m_host.GetMetricsAdvance() : 0.0f;



    if (!m_host.IsMetricsProportional())
    {
        return (float) m_host.GetCellStart (row, cell) * m_host.GetMetricsAdvance();
    }

    for (int c = 0; c < cell && c < (int) m_columnPx.size(); c++)
    {
        x += m_columnPx[(size_t) c] + (float) m_host.GetMetricsColumnGap (c) * m_host.GetMetricsAdvance();
    }

    return x;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewMetrics::GetCharsPx
//
//  How wide a cell's characters from `from` up to `to` are drawn.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextViewMetrics::GetCharsPx (int row, int cell, int from, int to) const
{
    const std::vector<float>  * places = nullptr;



    if (m_host.IsMetricsProportional() && row >= 0 && (size_t) row < m_charX.size() && cell >= 0 && (size_t) cell < m_charX[(size_t) row].size())
    {
        places = &m_charX[(size_t) row][(size_t) cell];
    }

    if (places == nullptr || places->empty())
    {
        return (float) (to - from) * m_host.GetMetricsAdvance();
    }

    from = std::clamp (from, 0, (int) places->size() - 1);
    to   = std::clamp (to,   0, (int) places->size() - 1);

    return (*places)[(size_t) to] - (*places)[(size_t) from];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewMetrics::GetNearestChar
//
//  The boundary between a cell's characters, from `from` to `to`, nearest a
//  point `x` from where `from` is drawn, as an offset in the cell.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextViewMetrics::GetNearestChar (int row, int cell, int from, int to, float x) const
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
//  DxuiTextViewMetrics::GetPartPx
//
//  A wrapped line is one of the last cell's that starts past its first
//  character.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextViewMetrics::GetPartPx (int row, int cell, int cellFrom, int index) const
{
    const Row  & rowData = m_host.GetMetricsRows()[(size_t) row];
    bool         wrapped = cell == (int) rowData.cells.size() - 1 && cellFrom > 0;
    float        indent  = wrapped ? (float) (std::max) (0, rowData.wrapIndent) * m_host.GetMetricsAdvance() : 0.0f;



    return GetCellX (rowData, cell) + indent + GetCharsPx (row, cell, cellFrom, cellFrom + index);
}
