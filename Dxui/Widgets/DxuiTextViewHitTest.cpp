#include "Pch.h"

#include "DxuiTextViewHitTest.h"
#include "DxuiTextLayout.h"
#include "Core/DxuiDpiScaler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest::HitTest
//
//  The character nearest a point, rounding to the closer side of a cell. A
//  point in the gap after a column lands at the end of that column's cell.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextViewHitTest::Position DxuiTextViewHitTest::HitTest (POINT point) const
{
    const std::vector<Line>  & lines     = m_layout.GetLines();
    float                      advance   = m_layout.GetCellAdvance();
    int                        topLine   = m_viewport.topLine;
    Position                   pos;
    int                        y         = point.y - (int) m_viewport.bounds.top - m_viewport.padPx;
    int                        x         = point.x - m_viewport.textLeft;
    int                        lineIndex = 0;
    int                        column    = 0;
    int                        last      = 0;



    if (lines.empty() || m_layout.GetCellWidthPx() <= 0 || m_layout.GetCellHeightPx() <= 0)
    {
        return pos;
    }

    lineIndex = (y < 0) ? topLine - 1 : topLine;

    while (y >= 0 && lineIndex + 1 < (int) lines.size() && GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    lineIndex = (std::max) (0, (std::min) (lineIndex, (int) lines.size() - 1));
    column    = (x < 0) ? 0 : (int) (((float) x + advance / 2.0f) / advance);

    const Line &  line = lines[(size_t) lineIndex];
    const Row &   row  = m_layout.GetRows()[(size_t) line.row];

    pos.row = line.row;
    last    = (int) row.cells.size() - 1;

    if (last < 0)
    {
        return pos;
    }

    //  Proportional text: the cell whose column the point is in, then the
    //  character boundary nearest it.
    if (m_layout.IsProportional())
    {
        for (int c = 0; line.first && c < last; c++)
        {
            if ((float) x < m_layout.GetCellX (row, c + 1))
            {
                pos.offset = DxuiTextLayout::GetCellBase (row, c) + m_layout.GetNearestChar (line.row, c, 0, (int) row.cells[(size_t) c].size(), (float) x - m_layout.GetCellX (row, c));
                return pos;
            }
        }

        pos.offset = DxuiTextLayout::GetCellBase (row, last) + m_layout.GetNearestChar (line.row, last, line.start, line.start + line.length, (float) x - m_layout.GetPartPx (line.row, last, line.start, 0));
        return pos;
    }

    for (int c = 0; line.first && c < last; c++)
    {
        int  start  = m_layout.GetCellStart (row, c);
        int  length = (int) row.cells[(size_t) c].size();

        if (column < m_layout.GetCellStart (row, c + 1))
        {
            pos.offset = DxuiTextLayout::GetCellBase (row, c) + (std::max) (0, (std::min) (column - start, length));
            return pos;
        }
    }

    pos.offset = DxuiTextLayout::GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - m_layout.GetLineColumn (row, line), line.length));

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest::TryGetCharAt
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewHitTest::TryGetCharAt (POINT point, int & outLine, int & outFlat, int & outColumn) const
{
    const std::vector<Line>  & lines     = m_layout.GetLines();
    const RECT               & bounds    = m_viewport.bounds;
    int                        y         = point.y - (int) bounds.top - m_viewport.padPx;
    int                        x         = point.x - m_viewport.textLeft;
    int                        lineIndex = m_viewport.topLine;
    int                        column    = 0;
    bool                       inside    = point.x >= bounds.left && point.x < bounds.right && point.y >= bounds.top && point.y < bounds.bottom;
    const Line               * line      = nullptr;
    const Row                * row       = nullptr;



    if (!inside || lines.empty() || m_layout.GetCellWidthPx() <= 0 || m_layout.GetCellHeightPx() <= 0 || y < 0 || x < 0 ||
        point.x < m_viewport.textBaseLeft || m_viewport.topLine >= (int) lines.size())
    {
        return false;
    }

    while (lineIndex + 1 < (int) lines.size() && GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    if (y >= GetLineOffsetPx (lineIndex) + m_layout.GetLineHeightPx (lineIndex))
    {
        return false;
    }

    column = (int) ((float) x / m_layout.GetCellAdvance());
    line   = &lines[(size_t) lineIndex];
    row    = &m_layout.GetRows()[(size_t) line->row];

    for (int c = 0; c < (int) row->cells.size(); c++)
    {
        int  flat     = 0;
        int  length   = 0;
        int  cellFrom = 0;



        if (!DxuiTextLayout::GetLinePart (*row, *line, c, flat, length, cellFrom))
        {
            continue;
        }

        for (int i = 0; i < length; i++)
        {
            if ((float) x >= m_layout.GetPartPx (line->row, c, cellFrom, i) && (float) x < m_layout.GetPartPx (line->row, c, cellFrom, i + 1))
            {
                outLine   = lineIndex;
                outFlat   = flat + i;
                outColumn = column;
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest::TryGetLinkAt
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewHitTest::TryGetLinkAt (POINT point, int & outRow, int & outSpan, RECT & outRect) const
{
    int            lineIndex = 0;
    int            flat      = 0;
    int            column    = 0;
    int            top       = 0;
    const Line   * line      = nullptr;
    const Row    * row       = nullptr;



    if (!TryGetCharAt (point, lineIndex, flat, column))
    {
        return false;
    }

    line = &m_layout.GetLines()[(size_t) lineIndex];
    row  = &m_layout.GetRows()[(size_t) line->row];
    top  = (int) m_viewport.bounds.top + m_viewport.padPx + GetLineOffsetPx (lineIndex);

    for (int s = 0; s < (int) row->spans.size(); s++)
    {
        const Span  & span     = row->spans[(size_t) s];
        int           spanFlat = DxuiTextLayout::GetCellBase (*row, span.cell) + span.start;
        int           partFlat = 0;
        int           length   = 0;
        int           cellFrom = 0;
        int           first    = 0;
        int           end      = 0;



        if (!span.isLink || flat < spanFlat || flat >= spanFlat + span.length || !DxuiTextLayout::GetLinePart (*row, *line, span.cell, partFlat, length, cellFrom))
        {
            continue;
        }

        first   = (std::max) (spanFlat, partFlat);
        end     = (std::min) (spanFlat + span.length, partFlat + length);
        outRow  = line->row;
        outSpan = s;
        outRect = RECT { (LONG) ((float) m_viewport.textLeft + m_layout.GetPartPx (line->row, span.cell, cellFrom, first - partFlat)),
                         (LONG) top,
                         (LONG) ((float) m_viewport.textLeft + m_layout.GetPartPx (line->row, span.cell, cellFrom, end - partFlat)),
                         (LONG) (top + m_layout.GetLineHeightPx (lineIndex)) };
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest::GetLineOffsetPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextViewHitTest::GetLineOffsetPx (int lineIndex) const
{
    return m_layout.GetLineOffsetPx (m_viewport.topLine, lineIndex);
}
