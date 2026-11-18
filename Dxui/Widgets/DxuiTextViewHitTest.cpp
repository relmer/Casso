#include "Pch.h"

#include "DxuiTextViewHitTest.h"
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
    Position  pos;
    int       pad       = m_look.padPx;
    int       y         = point.y - (int) m_view.GetLayoutBounds().top - pad;
    int       x         = point.x - m_view.GetTextLeft();
    int       lineIndex = 0;
    int       column    = 0;
    int       last      = 0;



    if (m_view.GetLayoutLines().empty() || m_look.cellWidthPx <= 0 || m_look.cellHeightPx <= 0)
    {
        return pos;
    }

    lineIndex = (y < 0) ? m_look.topLine - 1 : m_look.topLine;

    while (y >= 0 && lineIndex + 1 < (int) m_view.GetLayoutLines().size() && m_view.GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    lineIndex = (std::max) (0, (std::min) (lineIndex, (int) m_view.GetLayoutLines().size() - 1));
    column    = (x < 0) ? 0 : (int) (((float) x + m_look.cellAdvance / 2.0f) / m_look.cellAdvance);

    const Line &  line = m_view.GetLayoutLines()[(size_t) lineIndex];
    const Row &   row  = m_view.GetLayoutRows()[(size_t) line.row];

    pos.row = line.row;
    last    = (int) row.cells.size() - 1;

    if (last < 0)
    {
        return pos;
    }

    //  Proportional text: the cell whose column the point is in, then the
    //  character boundary nearest it.
    if (m_look.proportional)
    {
        for (int c = 0; line.first && c < last; c++)
        {
            if ((float) x < m_view.GetCellX (row, c + 1))
            {
                pos.offset = m_view.GetCellBase (row, c) + m_view.GetNearestChar (line.row, c, 0, (int) row.cells[(size_t) c].size(), (float) x - m_view.GetCellX (row, c));
                return pos;
            }
        }

        pos.offset = m_view.GetCellBase (row, last) + m_view.GetNearestChar (line.row, last, line.start, line.start + line.length, (float) x - m_view.GetPartPx (line.row, last, line.start, 0));
        return pos;
    }

    for (int c = 0; line.first && c < last; c++)
    {
        int  start  = m_view.GetCellStart (row, c);
        int  length = (int) row.cells[(size_t) c].size();

        if (column < m_view.GetCellStart (row, c + 1))
        {
            pos.offset = m_view.GetCellBase (row, c) + (std::max) (0, (std::min) (column - start, length));
            return pos;
        }
    }

    pos.offset = m_view.GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - m_view.GetLineColumn (row, line), line.length));

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest::TryGetCharAt
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewHitTest::TryGetCharAt (POINT point, int & outLine, int & outFlat, int & outColumn) const
{
    const RECT  & bounds    = m_view.GetLayoutBounds();
    int           y         = point.y - (int) m_view.GetLayoutBounds().top - m_look.padPx;
    int           x         = point.x - m_view.GetTextLeft();
    int           lineIndex = m_look.topLine;
    int           column    = 0;
    bool          inside    = point.x >= bounds.left && point.x < bounds.right && point.y >= bounds.top && point.y < bounds.bottom;
    const Line  * line      = nullptr;
    const Row   * row       = nullptr;



    if (!inside || m_view.GetLayoutLines().empty() || m_look.cellWidthPx <= 0 || m_look.cellHeightPx <= 0 || y < 0 || x < 0 ||
        point.x < m_view.GetTextBaseLeft() || m_look.topLine >= (int) m_view.GetLayoutLines().size() || m_view.IsOverScrollbar (point))
    {
        return false;
    }

    while (lineIndex + 1 < (int) m_view.GetLayoutLines().size() && m_view.GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    if (y >= m_view.GetLineOffsetPx (lineIndex) + m_view.GetLineHeightPx (lineIndex))
    {
        return false;
    }

    column = (int) ((float) x / m_look.cellAdvance);
    line   = &m_view.GetLayoutLines()[(size_t) lineIndex];
    row    = &m_view.GetLayoutRows()[(size_t) line->row];

    for (int c = 0; c < (int) row->cells.size(); c++)
    {
        int  flat     = 0;
        int  length   = 0;
        int  cellFrom = 0;



        if (!m_view.GetLinePart (*row, *line, c, flat, length, cellFrom))
        {
            continue;
        }

        for (int i = 0; i < length; i++)
        {
            if ((float) x >= m_view.GetPartPx (line->row, c, cellFrom, i) && (float) x < m_view.GetPartPx (line->row, c, cellFrom, i + 1))
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

    line = &m_view.GetLayoutLines()[(size_t) lineIndex];
    row  = &m_view.GetLayoutRows()[(size_t) line->row];
    top  = (int) m_view.GetLayoutBounds().top + m_look.padPx + m_view.GetLineOffsetPx (lineIndex);

    for (int s = 0; s < (int) row->spans.size(); s++)
    {
        const Span  & span     = row->spans[(size_t) s];
        int           spanFlat = m_view.GetCellBase (*row, span.cell) + span.start;
        int           partFlat = 0;
        int           length   = 0;
        int           cellFrom = 0;
        int           first    = 0;
        int           end      = 0;



        if (!span.isLink || flat < spanFlat || flat >= spanFlat + span.length || !m_view.GetLinePart (*row, *line, span.cell, partFlat, length, cellFrom))
        {
            continue;
        }

        first   = (std::max) (spanFlat, partFlat);
        end     = (std::min) (spanFlat + span.length, partFlat + length);
        outRow  = line->row;
        outSpan = s;
        outRect = RECT { (LONG) ((float) m_view.GetTextLeft() + m_view.GetPartPx (line->row, span.cell, cellFrom, first - partFlat)),
                         (LONG) top,
                         (LONG) ((float) m_view.GetTextLeft() + m_view.GetPartPx (line->row, span.cell, cellFrom, end - partFlat)),
                         (LONG) (top + m_view.GetLineHeightPx (lineIndex)) };
        return true;
    }

    return false;
}
