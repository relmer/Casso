#include "Pch.h"
#include "Core/DxuiSystemSettings.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiClipboard.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiPaneMetrics.h"
#include "Render/IDxuiTextRenderer.h"
#include "Render/IDxuiPainter.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiEvents.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_rows       = std::move (rows);
    m_anchor     = Position();
    m_caret      = Position();
    m_topLine    = m_followEnd ? m_topLine : 0;
    m_dragging   = false;
    m_scoped     = false;
    m_userAnchor = Position();
    m_userCaret  = Position();
    m_hoverRow   = -1;
    m_pressRow   = -1;
    m_focusRow   = -1;
    m_highlights.clear();
    m_charX.clear();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::UpdateRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::UpdateRows (std::vector<Row> rows)
{
    auto  clamp = [this] (Position & at)
    {
        if (m_rows.empty())
        {
            at = Position();
            return;
        }

        at.row    = std::clamp (at.row, 0, (int) m_rows.size() - 1);
        at.offset = std::clamp (at.offset, 0, GetRowLength (m_rows[(size_t) at.row]));
    };



    m_rows = std::move (rows);

    clamp (m_anchor);
    clamp (m_caret);
    clamp (m_userAnchor);
    clamp (m_userCaret);

    std::erase_if (m_highlights, [this] (const FindMatch & match) { return match.start.row >= (int) m_rows.size(); });

    m_focusRow = (std::min) (m_focusRow, (int) m_rows.size() - 1);
    m_charX.clear();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRowsDroppingFront
//
//  A position in a row that went is dropped: a highlight there goes, and a
//  selection or a scope with an end there is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRowsDroppingFront (std::vector<Row> rows, int dropped)
{
    auto  moveUp = [dropped] (Position & at) { at.row -= dropped; return at.row >= 0; };



    m_rows = std::move (rows);

    if (!moveUp (m_anchor) || !moveUp (m_caret))
    {
        m_anchor = Position();
        m_caret  = Position();
    }

    if (!moveUp (m_userAnchor) || !moveUp (m_userCaret))
    {
        m_userAnchor = Position();
        m_userCaret  = Position();
    }

    m_scoped = m_scoped && moveUp (m_scopeStart) && moveUp (m_scopeEnd);

    //  A link under the pointer or pressed moves up with its row.
    m_hoverRow = (m_hoverRow >= dropped) ? m_hoverRow - dropped : -1;
    m_pressRow = (m_pressRow >= dropped) ? m_pressRow - dropped : -1;

    std::erase_if (m_highlights, [&moveUp] (FindMatch & match) { return !moveUp (match.start); });
    m_charX.clear();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetCellSize
//
//  A size set here is kept across DPI changes, as a test's is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetCellSize (int widthPx, int heightPx)
{
    m_cellWidthPx  = (std::max) (widthPx,  0);
    m_cellHeightPx = (std::max) (heightPx, 0);
    m_cellAdvance  = (float) m_cellWidthPx;
    m_cellPinned   = true;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetProportional
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetProportional (bool proportional)
{
    if (proportional == m_proportional)
    {
        return;
    }

    m_proportional = proportional;
    m_leftColumn   = 0;
    m_charX.clear();

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineCap
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineCap() const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2 - GetHorzBarPx();
    int  used   = 0;
    int  count  = 0;
    int  line   = m_topLine;



    if (m_cellHeightPx <= 0)
    {
        return 1;
    }

    //  Past the end, a line is taken as a full one, so the count does not
    //  depend on how much text there is.
    for (;;)
    {
        int  lineHeight = (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;

        if (used + lineHeight > height)
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
//  DxuiTextView::GetLineHeightPx
//
//  A full line, or the fraction of one a short row asks for, never less
//  than a pixel.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineHeightPx (int lineIndex) const
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
//  DxuiTextView::GetLineOffsetPx
//
//  How far below the first line shown a line's top is; negative for a line
//  above it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineOffsetPx (int lineIndex) const
{
    int  offset = 0;



    for (int line = m_topLine; line < lineIndex; line++)
    {
        offset += (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;
    }

    for (int line = lineIndex; line < m_topLine; line++)
    {
        offset -= GetLineHeightPx (line);
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTopForLast
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTopForLast (int lineIndex) const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2;
    int  top    = lineIndex;
    int  used   = 0;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size() || m_cellHeightPx <= 0)
    {
        return 0;
    }

    used = GetLineHeightPx (lineIndex);

    while (top > 0 && used + GetLineHeightPx (top - 1) <= height)
    {
        top--;
        used += GetLineHeightPx (top);
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFirstLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetFirstLineOfRow (int row) const
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
//  DxuiTextView::GetCellAnchorPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellAnchorPx (int row, int cell, float & outX, float & outY) const
{
    int  line = GetFirstLineOfRow (row);
    int  side = 0;



    if (!GetRowSide (row, side) || side != 0 || m_cellHeightPx <= 0 || !GetCellLeftPx (row, cell, outX))
    {
        return false;
    }

    outY = (float) m_boundsDip.top + (float) m_scaler.ToPx (s_kPadDip) + (float) GetLineOffsetPx (line) + (float) GetLineHeightPx (line) * 0.5f;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowSide
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetRowSide (int row, int & outSide) const
{
    int  line = GetFirstLineOfRow (row);



    if (line < 0)
    {
        return false;
    }

    outSide = (line < m_topLine) ? -1 : (line >= m_topLine + GetLineCap()) ? 1 : 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellLeftPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellLeftPx (int row, int cell, float & outX) const
{
    if (row < 0 || row >= (int) m_rows.size())
    {
        return false;
    }

    outX = (float) GetTextLeft() + GetCellX (m_rows[(size_t) row], cell);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLinesSpanPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetLinesSpanPx (float & outTop, float & outBottom) const
{
    int  shown = (std::max) (0, (std::min) (GetLineCap(), (int) m_lines.size() - m_topLine));



    outTop    = (float) m_boundsDip.top + (float) m_scaler.ToPx (s_kPadDip);
    outBottom = outTop + (float) GetLineOffsetPx (m_topLine + shown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetTopLine
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetTopLine (int line)
{
    int  maxTop = GetTopForLast ((int) m_lines.size() - 1);



    m_topLine = (std::max) (0, (std::min) (line, maxTop));
    m_atEnd   = m_topLine >= maxTop;

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextColumns
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextColumns (bool scrollbar) const
{
    return (m_cellWidthPx > 0) ? (std::max) (1, (int) (GetTextWidthPx (scrollbar) / m_cellAdvance)) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextWidthPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetTextWidthPx (bool scrollbar) const
{
    int  width = (int) (m_boundsDip.right - m_boundsDip.left) - GetGutterLeftPx() - m_scaler.ToPx (s_kPadDip) - GetGutterPx();



    width -= scrollbar ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;

    return (float) width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellStart
//
//  The character column a cell begins in: past the warning mark, and past
//  every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetCellStart (const Row & row, int cell) const
{
    int  column = row.warning ? kWarningCells : 0;



    //  A cell longer than its capped column pushes the cells after it along.
    for (int c = 0; c < cell && c < (int) m_columnCells.size(); c++)
    {
        int   length = (c < (int) row.cells.size()) ? (int) row.cells[(size_t) c].size() : 0;
        auto  gap    = m_columnGaps.find (c);

        column += (std::max) (m_columnCells[(size_t) c], length) + ((gap != m_columnGaps.end()) ? gap->second : kColumnGapCells);
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellBase
//
//  Where a cell's first character is in the row's text.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetCellBase (const Row & row, int cell) const
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
//  DxuiTextView::GetRowLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetRowLength (const Row & row) const
{
    return row.cells.empty() ? 0 : GetCellBase (row, (int) row.cells.size() - 1) + (int) row.cells.back().size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowExtent
//
//  The column just past a row's last character that counts toward the
//  sideways scroll. An empty cell adds nothing, not even the gap ahead of
//  it, and neither does a cell in a clipped column.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetRowExtent (const Row & row) const
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
//  DxuiTextView::GetRowText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetRowText (const Row & row) const
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
//  DxuiTextView::Rebuild
//
//  Column widths from every row, then the drawn lines, laid out again
//  narrower if they turn out to need the scrollbar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Rebuild()
{
    Line  top  = (m_topLine >= 0 && m_topLine < (int) m_lines.size()) ? m_lines[(size_t) m_topLine] : Line();
    int   line = 0;



    m_lines.clear();
    m_columnCells.clear();

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    //  Proportional text is laid out once its characters are measured, by
    //  their places rather than by cells, and always wraps.
    if (m_proportional)
    {
        m_widestColumns = 0;
        m_leftColumn    = 0;

        if (m_charX.size() != m_rows.size())
        {
            return;
        }

        BuildProportionalLines (GetTextWidthPx (false));

        if (IsScrollbarVisible())
        {
            BuildProportionalLines (GetTextWidthPx (true));
        }

        //  A new wrap moves every line, so the top is the line that now has
        //  the text the old top started with.
        while (line + 1 < (int) m_lines.size()
               && (m_lines[(size_t) line + 1].row < top.row || (m_lines[(size_t) line + 1].row == top.row && m_lines[(size_t) line + 1].start <= top.start)))
        {
            line++;
        }

        SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : line);
        return;
    }

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

    BuildLines (GetTextColumns (false));

    if (IsScrollbarVisible())
    {
        BuildLines (GetTextColumns (true));
    }

    m_leftColumn = std::clamp (m_leftColumn, 0, (std::max) (0, m_widestColumns - GetTextColumns (IsScrollbarVisible())));

    SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : m_topLine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineColumn
//
//  The column a line of a row's last cell starts at: the cell's, then its
//  wrap indent on each wrapped line.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineColumn (const Row & row, const Line & line) const
{
    return GetCellStart (row, (int) row.cells.size() - 1) + (line.first ? 0 : (std::max) (0, row.wrapIndent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::BuildLines
//
//  Wraps each row's last cell at the last space that fits, or mid-word when
//  none does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::BuildLines (int columns)
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
//  DxuiTextView::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SyncScrollbar()
{
    int         barW = m_scaler.ToPx (s_kScrollbarWidthDip);
    SCROLLINFO  info = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.bottom });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (std::max) (0, (int) m_lines.size() - 1);
    info.nPage = (UINT) GetLineCap();
    info.nPos  = m_topLine;
    m_vertScroll.SetScrollInfo (info);

    if (!IsHorzScrollbarVisible())
    {
        return;
    }

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, 1);
    m_horzScroll.SetTrack (RECT { GetTextBaseLeft(), m_boundsDip.bottom - barW, m_boundsDip.right - (IsScrollbarVisible() ? barW : 0), m_boundsDip.bottom });

    info.nMax  = (std::max) (0, m_widestColumns - 1);
    info.nPage = (UINT) GetTextColumns (IsScrollbarVisible());
    info.nPos  = m_leftColumn;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::HitTest
//
//  The character nearest a point, rounding to the closer side of a cell. A
//  point in the gap after a column lands at the end of that column's cell.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::Position DxuiTextView::HitTest (POINT point) const
{
    Position  pos;
    int       pad       = m_scaler.ToPx (s_kPadDip);
    int       y         = point.y - (int) m_boundsDip.top - pad;
    int       x         = point.x - GetTextLeft();
    int       lineIndex = 0;
    int       column    = 0;
    int       last      = 0;



    if (m_lines.empty() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return pos;
    }

    lineIndex = (y < 0) ? m_topLine - 1 : m_topLine;

    while (y >= 0 && lineIndex + 1 < (int) m_lines.size() && GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    lineIndex = (std::max) (0, (std::min) (lineIndex, (int) m_lines.size() - 1));
    column    = (x < 0) ? 0 : (int) (((float) x + m_cellAdvance / 2.0f) / m_cellAdvance);

    const Line &  line = m_lines[(size_t) lineIndex];
    const Row &   row  = m_rows[(size_t) line.row];

    pos.row = line.row;
    last    = (int) row.cells.size() - 1;

    if (last < 0)
    {
        return pos;
    }

    //  Proportional text: the cell whose column the point is in, then the
    //  character boundary nearest it.
    if (m_proportional)
    {
        for (int c = 0; line.first && c < last; c++)
        {
            if ((float) x < GetCellX (row, c + 1))
            {
                pos.offset = GetCellBase (row, c) + GetNearestChar (line.row, c, 0, (int) row.cells[(size_t) c].size(), (float) x - GetCellX (row, c));
                return pos;
            }
        }

        pos.offset = GetCellBase (row, last) + GetNearestChar (line.row, last, line.start, line.start + line.length, (float) x - GetPartPx (line.row, last, line.start, 0));
        return pos;
    }

    for (int c = 0; line.first && c < last; c++)
    {
        int  start  = GetCellStart (row, c);
        int  length = (int) row.cells[(size_t) c].size();

        if (column < GetCellStart (row, c + 1))
        {
            pos.offset = GetCellBase (row, c) + (std::max) (0, (std::min) (column - start, length));
            return pos;
        }
    }

    pos.offset = GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - GetLineColumn (row, line), line.length));

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Select
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Select (Position anchor, Position caret)
{
    m_anchor = anchor;
    m_caret  = caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SelectAll()
{
    m_anchor = Position();
    m_caret  = m_rows.empty() ? Position() : Position { (int) m_rows.size() - 1, GetRowLength (m_rows.back()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::ClearSelection()
{
    m_anchor = m_caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectWordAt
//
//  The run of characters around a position that holds no space or tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SelectWordAt (Position pos)
{
    GetWordBounds (pos, m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetWordBounds
//
//  The start and end of the run of characters around a position, bounded by
//  spaces and the tabs between cells.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetWordBounds (Position pos, Position & outFirst, Position & outLast) const
{
    std::wstring  text  = (pos.row < (int) m_rows.size()) ? GetRowText (m_rows[(size_t) pos.row]) : std::wstring();
    int           size  = (int) text.size();
    int           first = (std::min) (pos.offset, size);
    int           last  = first;
    int           kind  = 0;



    //  The run is of whatever kind is under the position, or just before it at
    //  the end of the row, so a run of spaces is taken as one too.
    if (first < size)
    {
        kind = GetCharClass (text[(size_t) first]);
    }
    else if (first > 0)
    {
        kind = GetCharClass (text[(size_t) first - 1]);
    }

    while (first > 0 && GetCharClass (text[(size_t) first - 1]) == kind)
    {
        first--;
    }

    while (last < size && GetCharClass (text[(size_t) last]) == kind)
    {
        last++;
    }

    outFirst = Position { pos.row, first };
    outLast  = Position { pos.row, last };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetSelectionText
//
//  The selected characters, a tab between cells and a line break between rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetSelectionText() const
{
    Position      from = (std::min) (m_anchor, m_caret);
    Position      to   = (std::max) (m_anchor, m_caret);
    std::wstring  out;



    for (int r = from.row; HasSelection() && r <= to.row && r < (int) m_rows.size(); r++)
    {
        std::wstring  text  = GetRowText (m_rows[(size_t) r]);
        int           begin = (r == from.row) ? (std::min) (from.offset, (int) text.size()) : 0;
        int           end   = (r == to.row)   ? (std::min) (to.offset,   (int) text.size()) : (int) text.size();

        out += text.substr ((size_t) begin, (size_t) (std::max) (0, end - begin));

        if (r < to.row)
        {
            out += L"\r\n";
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::CopySelection
//
//  With nothing selected the clipboard is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::CopySelection() const
{
    if (HasSelection())
    {
        DxuiClipboard::SetText (m_hwnd, GetSelectionText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectMatch
//
//  A forward search starts at the end of the selection and a backward one at
//  its start, so repeating either steps from match to match. With nothing
//  selected both start at the caret, where the last click left it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool forward)
{
    std::vector<std::wstring>  texts;
    Position                   from   = forward ? (std::max) (m_anchor, m_caret) : (std::min) (m_anchor, m_caret);
    Position                   start;
    FindResult                 result = FindResult::NotFound;



    texts.reserve (m_rows.size());

    for (const Row & row : m_rows)
    {
        texts.push_back (GetRowText (row));
    }

    result = FindInRows (texts, needle, matchCase, wholeWord, forward, from, start);

    if (result != FindResult::NotFound)
    {
        m_anchor = start;
        m_caret  = Position { start.row, start.offset + (int) needle.size() };

        ScrollToPosition (start);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectMatch  (with a regular expression option)
//
//  From the selection as the plain search starts: forward, the first match
//  starting at or after its end; backward, the last starting before its
//  start; each going round when there is none.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount)
{
    std::vector<FindMatch>     matches;
    Position                   from   = forward ? (std::max) (m_anchor, m_caret) : (std::min) (m_anchor, m_caret);
    FindResult                 result = FindResult::Wrapped;
    size_t                     pick   = 0;



    outIndex = 0;
    outCount = 0;

    KeepUserSelection();

    (void) GetScopedMatches (needle, matchCase, wholeWord, isRegex, matches);
    m_highlights = matches;

    if (matches.empty())
    {
        return FindResult::NotFound;
    }

    if (forward)
    {
        auto  it = std::ranges::find_if (matches, [from] (const FindMatch & match) { return !(match.start < from); });

        pick   = (it == matches.end()) ? 0 : (size_t) (it - matches.begin());
        result = (it == matches.end()) ? FindResult::Wrapped : FindResult::Found;
    }
    else
    {
        pick = matches.size() - 1;

        for (size_t i = matches.size(); i > 0; i--)
        {
            if (matches[i - 1].start < from)
            {
                pick   = i - 1;
                result = FindResult::Found;
                break;
            }
        }
    }

    m_anchor = matches[pick].start;
    m_caret  = Position { matches[pick].start.row, matches[pick].start.offset + matches[pick].length };
    ScrollToPosition (m_anchor);

    outIndex = (int) pick + 1;
    outCount = (int) matches.size();
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectMatchInView
//
//  A match is on screen when the line it starts on is one the view shows.
//  Without one there, the first starting below the top of the view is taken,
//  going round to the first in the text when there is none below.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount)
{
    std::vector<FindMatch>  matches;
    int                     cap    = GetLineCap();
    size_t                  pick   = 0;
    FindResult              result = FindResult::Found;



    outIndex = 0;
    outCount = 0;

    KeepUserSelection();

    (void) GetScopedMatches (needle, matchCase, wholeWord, isRegex, matches);
    m_highlights = matches;

    if (matches.empty())
    {
        return FindResult::NotFound;
    }

    auto  onScreen = std::ranges::find_if (matches, [this, cap] (const FindMatch & match)
    {
        int  line = GetLineOfPosition (match.start);

        return line >= m_topLine && line < m_topLine + cap;
    });

    auto  below = std::ranges::find_if (matches, [this] (const FindMatch & match) { return GetLineOfPosition (match.start) >= m_topLine; });

    if (onScreen != matches.end())
    {
        pick = (size_t) (onScreen - matches.begin());
    }
    else if (below != matches.end())
    {
        pick = (size_t) (below - matches.begin());
    }
    else
    {
        result = FindResult::Wrapped;
    }

    m_anchor = matches[pick].start;
    m_caret  = Position { matches[pick].start.row, matches[pick].start.offset + matches[pick].length };
    ScrollToPosition (m_anchor);

    outIndex = (int) pick + 1;
    outCount = (int) matches.size();
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetScopedMatches
//
//  Every match in the text, less those outside the scope while find in
//  selection is on. False when the needle is not a valid expression.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetScopedMatches (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, std::vector<FindMatch> & outMatches) const
{
    std::vector<std::wstring>  texts;
    bool                       valid = false;



    texts.reserve (m_rows.size());

    for (const Row & row : m_rows)
    {
        texts.push_back (GetRowText (row));
    }

    valid = FindAllInRows (texts, needle, matchCase, wholeWord, isRegex, outMatches);

    if (valid && m_scoped)
    {
        std::erase_if (outMatches, [this] (const FindMatch & match)
        {
            Position  end { match.start.row, match.start.offset + match.length };

            return match.start < m_scopeStart || m_scopeEnd < end;
        });
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindHighlightFill
//
//  3:1 is what WCAG asks of a mark that is not text. A dark background
//  takes the accent lighter, a light one darker.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextView::GetFindHighlightFill (const IDxuiTheme & theme)
{
    constexpr float  kMinRatio = 3.0f;
    constexpr int    kMaxSteps = 40;



    uint32_t  background = theme.TextViewBackground() | 0xFF000000u;
    uint32_t  fill       = theme.WarningAccent()      | 0xFF000000u;
    bool      darkBack   = DxuiColor::ComputeRelativeLuminance (background) < 0.18f;



    for (int step = 0; step < kMaxSteps && DxuiColor::ComputeContrastRatio (fill, background) < kMinRatio; step++)
    {
        fill = darkBack ? DxuiColor::Lighten (fill, 0.1f) : DxuiColor::Darken (fill, 0.9f);
    }

    return fill;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindHighlightText
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextView::GetFindHighlightText (const IDxuiTheme & theme)
{
    uint32_t  fill  = GetFindHighlightFill (theme);
    uint32_t  fore  = theme.Foreground()         | 0xFF000000u;
    uint32_t  back  = theme.TextViewBackground() | 0xFF000000u;



    return (DxuiColor::ComputeContrastRatio (fore, fill) >= DxuiColor::ComputeContrastRatio (back, fill)) ? fore : back;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsCurrentMatch
//
//  The match the search is at is the one the selection covers exactly.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsCurrentMatch (const FindMatch & match) const
{
    Position  from = (std::min) (m_anchor, m_caret);
    Position  to   = (std::max) (m_anchor, m_caret);



    return from == match.start && to == Position { match.start.row, match.start.offset + match.length };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindFillOf
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextView::GetFindFillOf (const FindMatch & match, const IDxuiTheme & theme) const
{
    if (m_findCurrentArgb != 0 && IsCurrentMatch (match))
    {
        return m_findCurrentArgb;
    }

    return (m_findMatchArgb != 0) ? m_findMatchArgb : GetFindHighlightFill (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetFindScopeToSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetFindScopeToSelection()
{
    bool  byFind = std::ranges::any_of (m_highlights, [this] (const FindMatch & match) { return IsCurrentMatch (match); });



    //  A search selects its match, so the selection to keep to is the one
    //  there before the search moved it.
    m_scoped     = byFind ? !(m_userAnchor == m_userCaret) : HasSelection();
    m_scopeStart = byFind ? (std::min) (m_userAnchor, m_userCaret) : (std::min) (m_anchor, m_caret);
    m_scopeEnd   = byFind ? (std::max) (m_userAnchor, m_userCaret) : (std::max) (m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::KeepUserSelection
//
//  Called before a search moves the selection: a selection the search did
//  not make is kept, for find in selection to take.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::KeepUserSelection()
{
    bool  byFind = std::ranges::any_of (m_highlights, [this] (const FindMatch & match) { return IsCurrentMatch (match); });



    if (!byFind)
    {
        m_userAnchor = m_anchor;
        m_userCaret  = m_caret;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FindAllInRows
//
//  A plain needle is found as a substring, lowered on both sides without
//  matching case. A regular expression uses ECMAScript syntax; a match of
//  no characters is passed over, since it selects nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::FindAllInRows (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              matchCase,
    bool                              wholeWord,
    bool                              isRegex,
    std::vector<FindMatch>          & outMatches)
{
    std::wregex  pattern;
    std::wstring key = matchCase ? needle : GetLowered (needle);



    outMatches.clear();

    if (needle.empty())
    {
        return true;
    }

    if (isRegex)
    {
        try
        {
            pattern = std::wregex (needle, matchCase ? std::regex_constants::ECMAScript : (std::regex_constants::ECMAScript | std::regex_constants::icase));
        }
        catch (const std::regex_error &)
        {
            return false;
        }
    }

    for (size_t r = 0; r < rows.size(); r++)
    {
        const std::wstring &  text = rows[r];

        if (isRegex)
        {
            //  The matcher throws once a pattern backtracks past its budget,
            //  which a pattern typed, or half typed, into find can do.
            try
            {
                for (std::wsregex_iterator it (text.begin(), text.end(), pattern), end; it != end; ++it)
                {
                    size_t  at     = (size_t) it->position (0);
                    size_t  length = (size_t) it->length (0);

                    if (length > 0 && (!wholeWord || IsWholeWordAt (text, at, length)))
                    {
                        outMatches.push_back (FindMatch { Position { (int) r, (int) at }, (int) length });
                    }
                }
            }
            catch (const std::regex_error &)
            {
                outMatches.clear();
                return false;
            }

            continue;
        }

        std::wstring  hay = matchCase ? text : GetLowered (text);

        for (size_t at = hay.find (key); at != std::wstring::npos; at = hay.find (key, at + 1))
        {
            if (!wholeWord || IsWholeWordAt (text, at, key.size()))
            {
                outMatches.push_back (FindMatch { Position { (int) r, (int) at }, (int) key.size() });
            }
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FindInRows
//
//  Without a match past the starting point the search goes round: forward
//  from the top, backward from the bottom. Without matching case, both sides
//  are lowered first, which keeps every offset where it was, and lowering
//  leaves a word character a word character.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::FindInRows (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              matchCase,
    bool                              wholeWord,
    bool                              forward,
    Position                          from,
    Position                        & outStart)
{
    std::vector<std::wstring>          folded;
    const std::vector<std::wstring>  * haystack = &rows;
    std::wstring                       key      = needle;
    Position                           around   = forward ? Position() : Position { (int) rows.size(), 0 };



    if (needle.empty())
    {
        return FindResult::NotFound;
    }

    if (!matchCase)
    {
        for (const std::wstring & text : rows)
        {
            folded.push_back (GetLowered (text));
        }

        key      = GetLowered (needle);
        haystack = &folded;
    }

    if (TryFindOnce (*haystack, key, wholeWord, forward, from, outStart))
    {
        return FindResult::Found;
    }

    if (TryFindOnce (*haystack, key, wholeWord, forward, around, outStart))
    {
        return FindResult::Wrapped;
    }

    return FindResult::NotFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLowered
//
//  One character for one, so an offset in the result is the same offset in
//  the text it came from.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetLowered (const std::wstring & text)
{
    std::wstring  lowered = text;



    for (wchar_t & ch : lowered)
    {
        ch = (wchar_t) towlower (ch);
    }

    return lowered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TryFindOnce
//
//  One pass, with no going round: forward, the first match starting at or
//  after `from`; backward, the last match starting before it. A match that is
//  part of a longer word is passed over when only whole words count.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryFindOnce (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              wholeWord,
    bool                              forward,
    Position                          from,
    Position                        & outStart)
{
    int     count = (int) rows.size();
    size_t  at    = std::wstring::npos;



    if (forward)
    {
        for (int r = (std::max) (from.row, 0); r < count; r++)
        {
            const std::wstring &  text  = rows[(size_t) r];
            size_t                start = (r == from.row) ? (size_t) (std::max) (from.offset, 0) : 0;

            at = (start <= text.size()) ? text.find (needle, start) : std::wstring::npos;

            while (wholeWord && at != std::wstring::npos && !IsWholeWordAt (text, at, needle.size()))
            {
                at = text.find (needle, at + 1);
            }

            if (at != std::wstring::npos)
            {
                outStart = Position { r, (int) at };
                return true;
            }
        }

        return false;
    }

    for (int r = (std::min) (from.row, count - 1); r >= 0; r--)
    {
        const std::wstring &  text = rows[(size_t) r];

        if (r == from.row && from.offset <= 0)
        {
            continue;
        }

        at = text.rfind (needle, (r == from.row) ? (size_t) (from.offset - 1) : std::wstring::npos);

        while (wholeWord && at != std::wstring::npos && !IsWholeWordAt (text, at, needle.size()))
        {
            at = (at == 0) ? std::wstring::npos : text.rfind (needle, at - 1);
        }

        if (at != std::wstring::npos)
        {
            outStart = Position { r, (int) at };
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsWholeWordAt
//
//  Whether the run of text at `at` has a line end or a character that is not
//  part of a word on each side of it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsWholeWordAt (const std::wstring & text, size_t at, size_t length)
{
    size_t  end = at + length;



    if (at > 0 && IsWordChar (text[at - 1]))
    {
        return false;
    }

    return end >= text.size() || !IsWordChar (text[end]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineOfPosition
//
//  The drawn line a position falls on: its row's first line, or a later one
//  when the position is in the part of the last cell that wrapped. -1 when
//  the row is not laid out.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineOfPosition (Position pos) const
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
//  DxuiTextView::ScrollToPosition
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::ScrollToPosition (Position pos)
{
    int  line = GetLineOfPosition (pos);
    int  cap  = GetLineCap();



    if (line < 0)
    {
        return;
    }

    if (line < m_topLine)
    {
        SetTopLine (line);
    }
    else if (line >= m_topLine + cap)
    {
        SetTopLine (GetTopForLast (line));
    }

    //  Without wrapping, the position's column is brought into view too.
    if (m_noWrap && pos.row >= 0 && pos.row < (int) m_rows.size())
    {
        const Row &  row     = m_rows[(size_t) pos.row];
        int          column  = 0;
        int          visible = GetTextColumns (IsScrollbarVisible());

        for (int c = 0; c < (int) row.cells.size(); c++)
        {
            if (c + 1 == (int) row.cells.size() || pos.offset < GetCellBase (row, c + 1))
            {
                column = GetCellStart (row, c) + pos.offset - GetCellBase (row, c);
                break;
            }
        }

        if (column < m_leftColumn)
        {
            SetLeftColumn (column);
        }
        else if (column >= m_leftColumn + visible)
        {
            SetLeftColumn (column - visible + 1);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetZoom
//
//  A new size is measured again on the next paint, unless the host set the
//  cells itself.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetZoom (float zoom)
{
    if (zoom == m_zoom || zoom <= 0.0f)
    {
        return;
    }

    m_zoom = zoom;
    m_charX.clear();

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLineSpacing
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLineSpacing (float spacing)
{
    if (spacing == m_lineSpacing || spacing < 1.0f)
    {
        return;
    }

    m_lineSpacing = spacing;

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    if (!m_cellPinned && m_measuredDpi != scaler.GetDpi())
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
        m_charX.clear();
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::EnsureCellSize
//
//  Measured at the size the face is drawn at, so cells and glyphs agree.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font   = m_proportional ? theme.BodyFont() : theme.MonospaceFont();
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    if (m_cellWidthPx > 0)
    {
        return;
    }

    hr = text.MeasureString (L"00000000", m_scaler.ToPxf (font.sizeDip * m_zoom), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    m_cellWidthPx  = (std::max) (1, (int) (width / 8.0f + 0.5f));
    m_cellAdvance  = (std::max) (1.0f, width / 8.0f);
    m_cellHeightPx = (std::max) (1, (int) (height * m_lineSpacing + 0.5f));
    m_textTopPx    = (std::max) (0, (m_cellHeightPx - (int) (height + 0.5f)) / 2);
    m_measuredDpi  = m_scaler.GetDpi();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT         hr       = S_OK;
    DxuiFontHandle  font     = m_proportional ? theme.BodyFont() : theme.MonospaceFont();
    int             last     = 0;
    float           clipLeft = 0.0f;



    font.sizeDip *= m_zoom;

    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.TextViewBackground());

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);

    if (m_proportional && m_charX.size() != m_rows.size())
    {
        MeasureCharacters (text, font);
    }

    last = (std::min) ((int) m_lines.size(), m_topLine + GetLineCap() + 1);

    //  The rows' fills and the gutter's icons, which do not scroll sideways,
    //  then the text, clipped to the text area once it has scrolled.
    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        PaintLineGutter (painter, text, theme, lineIndex);
    }

    clipLeft = (m_leftColumn > 0) ? (float) GetTextBaseLeft() : (float) m_boundsDip.left;

    hr = text.PushClipRect (clipLeft, (float) m_boundsDip.top, (float) m_boundsDip.right - clipLeft, (float) (m_boundsDip.bottom - m_boundsDip.top - GetHorzBarPx()));
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.PushClip (clipLeft, (float) m_boundsDip.top, (float) m_boundsDip.right - clipLeft, (float) (m_boundsDip.bottom - m_boundsDip.top - GetHorzBarPx()));

    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        PaintLine (painter, text, theme, lineIndex, font);
    }

    painter.PopClip();

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (IsScrollbarVisible())
    {
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }

    if (IsHorzScrollbarVisible())
    {
        m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::PaintLineGutter
//
//  A row's fill, which runs across the view on each of its lines, gutter and
//  all, over the focused row's selection or the highlighted row's hover,
//  the gutter band's fill on each of them too, and on its first line the
//  gutter's icon.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::PaintLineGutter (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex)
{
    HRESULT       hr       = S_OK;
    const Line  & line     = m_lines[(size_t) lineIndex];
    const Row   & row      = m_rows[(size_t) line.row];
    int           left     = GetTextBaseLeft();
    int           y        = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + GetLineOffsetPx (lineIndex);
    int           gutter   = GetGutterPx();
    float         iconPx   = m_scaler.ToPxf ((float) m_gutterIconDip);
    float         iconX    = (float) (left - gutter) + ((float) gutter - iconPx) * 0.5f;
    float         width    = (float) (GetTextAreaRightPx() - m_boundsDip.left);
    float         radius   = m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip);
    float         cellLeft = 0.0f;



    //  The focused row, under the row's fill, as a list draws its selected
    //  row.
    if (IsRowSelectionShown (line.row))
    {
        painter.FillRoundedRect ((float) m_boundsDip.left, (float) y, width, (float) GetLineHeightPx (lineIndex), radius, theme.ContentSelection());

        if (theme.ContentSelectionEdge() != 0)
        {
            painter.OutlineRoundedRect ((float) m_boundsDip.left, (float) y, width, (float) GetLineHeightPx (lineIndex), radius,
                                        (std::max) (1.0f, m_scaler.ToPxf (1.0f)), theme.ContentSelectionEdge());
        }
    }

    if (row.background != 0)
    {
        painter.FillRect ((float) m_boundsDip.left, (float) y, (float) (m_boundsDip.right - m_boundsDip.left), (float) GetLineHeightPx (lineIndex),
                          row.background);
    }

    //  The row an in-place tip is up for, in the tip's color from its
    //  highlight cell to the text area's right, over the row's fill.
    if (line.first && line.row == m_highlightRow && GetCellLeftPx (line.row, m_highlightCell, cellLeft))
    {
        painter.FillRect (cellLeft, (float) y, (float) GetTextAreaRightPx() - cellLeft, (float) GetLineHeightPx (lineIndex), theme.ContentSelection());
    }

    if (row.gutterFill != 0 && m_bandWidthDip > 0)
    {
        painter.FillRect ((float) (m_boundsDip.left + m_scaler.ToPx (m_bandLeftDip)), (float) y, (float) m_scaler.ToPx (m_bandWidthDip),
                          (float) GetLineHeightPx (lineIndex), row.gutterFill);
    }

    if (!line.first || row.icon == nullptr || row.icon->bgraPremul.empty() || gutter <= 0)
    {
        return;
    }

    //  A glyph margin centers its icon on a set line.
    if (m_glyphCenterDip > 0.0f)
    {
        iconX = (float) m_boundsDip.left + m_scaler.ToPxf (m_glyphCenterDip) - iconPx * 0.5f;
    }

    //  An icon wider than it is tall runs on to the right of its first square.
    hr = text.DrawIconBitmap (row.icon->bgraPremul.data(), row.icon->width, row.icon->height,
                              iconX,
                              (float) y + ((float) m_cellHeightPx - iconPx) * 0.5f,
                              iconPx * (float) row.icon->width / (float) (std::max) (1, row.icon->height), iconPx);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::PaintLine
//
//  The selection fill under a line's characters, then the characters. On a
//  row's first line every cell is drawn; on the lines after, only the run of
//  the last cell that wraps there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::PaintLine (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font)
{
    HRESULT                  hr       = S_OK;
    const Line             & line     = m_lines[(size_t) lineIndex];
    const Row              & row      = m_rows[(size_t) line.row];
    int                      left     = GetTextLeft();
    int                      y        = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + GetLineOffsetPx (lineIndex);
    int                      last     = (int) row.cells.size() - 1;
    Position                 from     = (std::min) (m_anchor, m_caret);
    Position                 to       = (std::max) (m_anchor, m_caret);
    bool                     selected = HasSelection() && from.row <= line.row && line.row <= to.row;
    int                      selFrom  = (line.row == from.row) ? from.offset : 0;
    int                      selTo    = (line.row == to.row)   ? to.offset   : INT_MAX;
    uint32_t                 selArgb  = theme.SelectionBackground();
    std::vector<uint32_t>    colors   = GetRowColors (row, theme);
    int                      gutter   = GetGutterPx();
    uint32_t                 litText  = GetFindHighlightText (theme);
    uint32_t                 litEdge  = (litText == (theme.Foreground() | 0xFF000000u)) ? theme.TextViewBackground() : theme.Foreground();
    uint32_t                 fore     = theme.Foreground()         | 0xFF000000u;
    uint32_t                 back     = theme.TextViewBackground() | 0xFF000000u;
    std::vector<bool>        lit;



    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              (float) left, (float) y, (float) (kWarningCells * m_cellAdvance), (float) m_cellHeightPx,
                              theme.WarningAccent(),
                              (float) m_cellHeightPx * 0.8f,
                              m_iconFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The current match's own fill is opaque, so its characters take the
    //  color with more contrast on it; the other matches' is a tint, which
    //  leaves their text its own colors.
    if (m_findCurrentArgb != 0)
    {
        litText = (DxuiColor::ComputeContrastRatio (fore, m_findCurrentArgb | 0xFF000000u) >= DxuiColor::ComputeContrastRatio (back, m_findCurrentArgb | 0xFF000000u)) ? fore : back;
    }

    //  A highlighted match's characters are drawn in the color that reads on
    //  its fill.
    for (const FindMatch & match : m_highlights)
    {
        bool  ownTint = (m_findCurrentArgb != 0 && !IsCurrentMatch (match)) || (m_findCurrentArgb == 0 && m_findMatchArgb != 0);

        if (match.start.row == line.row && !ownTint)
        {
            lit.resize ((size_t) GetRowLength (row) + 1, false);

            for (int i = match.start.offset; i < match.start.offset + match.length && i < (int) lit.size(); i++)
            {
                lit[(size_t) i] = true;
            }
        }
    }

    //  A span's fill, under the selection's.
    for (const Span & span : row.spans)
    {
        if (line.first && span.background != 0 && span.cell < (int) row.cells.size())
        {
            painter.FillRect ((float) left + GetPartPx (line.row, span.cell, 0, span.start), (float) y,
                              GetCharsPx (line.row, span.cell, span.start, span.start + span.length), (float) m_cellHeightPx, span.background);
        }
    }

    for (int c = 0; line.first && c < last; c++)
    {
        const std::wstring &  cell  = row.cells[(size_t) c];
        int                   count = (int) cell.size();

        if (selected)
        {
            FillSelectedRange (painter, y, line.row, c, 0, GetCellBase (row, c), count,
                               GetCellX (row, c + 1) - GetCellX (row, c) - GetCharsPx (line.row, c, 0, count), selFrom, selTo, selArgb);
        }

        FillHighlights (painter, theme, y, line.row, c, 0, GetCellBase (row, c), count, litEdge);
        DrawRun (text, theme, font, y, line.row, c, 0, GetCellBase (row, c), cell, selected, selFrom, selTo, colors, lit, litText);
    }

    if (last >= 0)
    {
        std::wstring  run = row.cells[(size_t) last].substr ((size_t) line.start, (size_t) line.length);

        if (selected)
        {
            FillSelectedRange (painter, y, line.row, last, line.start, GetCellBase (row, last) + line.start, line.length,
                               0.0f, selFrom, selTo, selArgb);
        }

        FillHighlights (painter, theme, y, line.row, last, line.start, GetCellBase (row, last) + line.start, line.length, litEdge);
        DrawRun (text, theme, font, y, line.row, last, line.start, GetCellBase (row, last) + line.start, run, selected, selFrom, selTo, colors, lit, litText);
    }

    if (line.row == m_hoverRow)
    {
        PaintLinkUnderline (painter, theme, lineIndex, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::PaintLinkUnderline
//
//  The hovered link's underline on one drawn line, under the part of the
//  link drawn on that line, at the bottom of the text.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::PaintLinkUnderline (IDxuiPainter & painter, const IDxuiTheme & theme, int lineIndex, int y) const
{
    const Line  & line      = m_lines[(size_t) lineIndex];
    const Row   & row       = m_rows[(size_t) line.row];
    const Span  * span      = nullptr;
    int           flat      = 0;
    int           length    = 0;
    int           cellFrom  = 0;
    int           first     = 0;
    int           end       = 0;
    int           spanFlat  = 0;
    float         thickness = m_scaler.ToPxf (1.0f);



    if (m_hoverSpan < 0 || m_hoverSpan >= (int) row.spans.size())
    {
        return;
    }

    span = &row.spans[(size_t) m_hoverSpan];

    if (!span->isLink || !GetLinePart (row, line, span->cell, flat, length, cellFrom))
    {
        return;
    }

    spanFlat = GetCellBase (row, span->cell) + span->start;
    first    = (std::max) (spanFlat, flat);
    end      = (std::min) (spanFlat + span->length, flat + length);

    if (end <= first)
    {
        return;
    }

    painter.FillRect ((float) GetTextLeft() + GetPartPx (line.row, span->cell, cellFrom, first - flat),
                      (float) (y + m_cellHeightPx - m_textTopPx) - thickness,
                      GetCharsPx (line.row, span->cell, cellFrom + first - flat, cellFrom + end - flat),
                      thickness,
                      (span->argb != 0) ? span->argb : theme.TextColor (DxuiTextRole::Link).argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLinePart
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetLinePart (const Row & row, const Line & line, int cell, int & outFlat, int & outLength, int & outCellFrom) const
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
//  DxuiTextView::TryGetCharAt
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryGetCharAt (POINT point, int & outLine, int & outFlat, int & outColumn) const
{
    const RECT  & bounds    = GetBounds();
    int           y         = point.y - (int) m_boundsDip.top - m_scaler.ToPx (s_kPadDip);
    int           x         = point.x - GetTextLeft();
    int           lineIndex = m_topLine;
    int           column    = 0;
    bool          inside    = point.x >= bounds.left && point.x < bounds.right && point.y >= bounds.top && point.y < bounds.bottom;
    const Line  * line      = nullptr;
    const Row   * row       = nullptr;



    if (!inside || m_lines.empty() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0 || y < 0 || x < 0 ||
        point.x < GetTextBaseLeft() || m_topLine >= (int) m_lines.size() || IsOverScrollbar (point))
    {
        return false;
    }

    while (lineIndex + 1 < (int) m_lines.size() && GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    if (y >= GetLineOffsetPx (lineIndex) + GetLineHeightPx (lineIndex))
    {
        return false;
    }

    column = (int) ((float) x / m_cellAdvance);
    line   = &m_lines[(size_t) lineIndex];
    row    = &m_rows[(size_t) line->row];

    for (int c = 0; c < (int) row->cells.size(); c++)
    {
        int  flat     = 0;
        int  length   = 0;
        int  cellFrom = 0;



        if (!GetLinePart (*row, *line, c, flat, length, cellFrom))
        {
            continue;
        }

        for (int i = 0; i < length; i++)
        {
            if ((float) x >= GetPartPx (line->row, c, cellFrom, i) && (float) x < GetPartPx (line->row, c, cellFrom, i + 1))
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
//  DxuiTextView::TryGetLinkAt
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryGetLinkAt (POINT point, int & outRow, int & outSpan, RECT & outRect) const
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

    line = &m_lines[(size_t) lineIndex];
    row  = &m_rows[(size_t) line->row];
    top  = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + GetLineOffsetPx (lineIndex);

    for (int s = 0; s < (int) row->spans.size(); s++)
    {
        const Span  & span     = row->spans[(size_t) s];
        int           spanFlat = GetCellBase (*row, span.cell) + span.start;
        int           partFlat = 0;
        int           length   = 0;
        int           cellFrom = 0;
        int           first    = 0;
        int           end      = 0;



        if (!span.isLink || flat < spanFlat || flat >= spanFlat + span.length || !GetLinePart (*row, *line, span.cell, partFlat, length, cellFrom))
        {
            continue;
        }

        first   = (std::max) (spanFlat, partFlat);
        end     = (std::min) (spanFlat + span.length, partFlat + length);
        outRow  = line->row;
        outSpan = s;
        outRect = RECT { (LONG) ((float) GetTextLeft() + GetPartPx (line->row, span.cell, cellFrom, first - partFlat)),
                         (LONG) top,
                         (LONG) ((float) GetTextLeft() + GetPartPx (line->row, span.cell, cellFrom, end - partFlat)),
                         (LONG) (top + GetLineHeightPx (lineIndex)) };
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetHoverPoint
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::SetHoverPoint (POINT point)
{
    int   row     = -1;
    int   span    = -1;
    RECT  rect    = {};
    bool  changed = false;



    if (!TryGetLinkAt (point, row, span, rect))
    {
        row  = -1;
        span = -1;
    }

    changed     = row != m_hoverRow || span != m_hoverSpan;
    m_hoverRow  = row;
    m_hoverSpan = span;

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::DrawRun
//
//  A run of characters on a line, from `cellFrom` in its cell, whose first
//  is at `flatStart` in the row's text. The selected part is drawn in the
//  full foreground and the rest in its span's color or the view's text color:
//  a heading's in the accent and, in proportional text, every cell but the
//  last in the muted color. Each stretch of one color starts where its first
//  character is placed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::DrawRun (IDxuiTextRenderer    & text,
                            const IDxuiTheme     & theme,
                            const DxuiFontHandle & font,
                            int                    y,
                            int                    row,
                            int                    cell,
                            int                    cellFrom,
                            int                    flatStart,
                            const std::wstring   & chars,
                            bool                   selected,
                            int                    selFrom,
                            int                    selTo,
                            const std::vector<uint32_t> & colors,
                            const std::vector<bool>     & lit,
                            uint32_t                      litArgb) const
{
    HRESULT   hr     = S_OK;
    float     left   = (float) GetTextLeft();
    int       count  = (int) chars.size();
    int       first  = selected ? std::clamp (selFrom - flatStart, 0, count) : count;
    int       end    = selected ? std::clamp (selTo - flatStart, first, count) : count;
    bool      label  = m_proportional && cell + 1 < (int) m_rows[(size_t) row].cells.size();
    uint32_t  normal = m_rows[(size_t) row].heading ? theme.Accent()
                     : label                         ? theme.ForegroundMuted()
                                                     : DxuiColor::Mix (theme.TextViewBackground(), theme.Foreground(), m_textStrength);
    int       start  = 0;
    auto      colorAt = [&] (int i)
                        {
                            size_t  flat = (size_t) (flatStart + i);

                            if (flat < lit.size() && lit[flat])
                            {
                                return litArgb;
                            }

                            if (i >= first && i < end)
                            {
                                return theme.Foreground();
                            }

                            return (flat < colors.size() && colors[flat] != 0) ? colors[flat] : normal;
                        };



    //  Each run of one color is drawn on its own, from its first cell.
    while (start < count)
    {
        uint32_t  argb = colorAt (start);
        int       stop = start + 1;

        while (stop < count && colorAt (stop) == argb)
        {
            stop++;
        }

        hr = text.DrawString (chars.substr ((size_t) start, (size_t) (stop - start)).c_str(),
                              left + GetPartPx (row, cell, cellFrom, start), (float) (y + m_textTopPx),
                              GetCharsPx (row, cell, cellFrom + start, cellFrom + stop) + m_cellAdvance, (float) (m_cellHeightPx - m_textTopPx),
                              argb, font.sizeDip, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        start = stop;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextLeft
//
//  Where the text's first column is drawn: past the pad and the gutter, and
//  left by the columns scrolled past.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextLeft() const
{
    return GetTextBaseLeft() - (int) std::lround ((float) m_leftColumn * m_cellAdvance);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCursorForPoint
//
//  The I-beam over the text, where a press selects; IDC_HAND over a link;
//  the arrow over the gutter and the scroll bars.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiTextView::GetCursorForPoint (POINT clientPx) const
{
    const RECT  & bounds = GetBounds();
    bool          inside = clientPx.x >= bounds.left && clientPx.x < bounds.right && clientPx.y >= bounds.top && clientPx.y < bounds.bottom;
    int           row    = -1;
    int           span   = -1;
    RECT          rect   = {};



    if (!inside || clientPx.x < GetTextBaseLeft() || IsOverScrollbar (clientPx))
    {
        return nullptr;
    }

    if (TryGetLinkAt (clientPx, row, span, rect))
    {
        return IDC_HAND;
    }

    return IDC_IBEAM;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextBaseLeft
//
//  The left of the text area, which does not scroll.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextBaseLeft() const
{
    return (int) m_boundsDip.left + GetGutterLeftPx() + GetGutterPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetGutterLeftPx
//
//  How far in from the view's left edge the gutter starts: at the edge for a
//  glyph margin, and past the pad otherwise.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetGutterLeftPx() const
{
    return (m_glyphCenterDip > 0.0f) ? 0 : GetPadLeftPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsInGutterBand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsInGutterBand (POINT point) const
{
    int  left = (int) m_boundsDip.left + m_scaler.ToPx (m_bandLeftDip);



    return m_bandWidthDip > 0 && point.y >= m_boundsDip.top && point.y < m_boundsDip.bottom &&
           point.x >= left && point.x < left + m_scaler.ToPx (m_bandWidthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsInGutter
//
//  Left of the text and off the gutter's band.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsInGutter (POINT point) const
{
    return point.x >= m_boundsDip.left && point.x < GetTextBaseLeft() && point.y >= m_boundsDip.top && point.y < m_boundsDip.bottom &&
           !IsInGutterBand (point);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextAreaLeftPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextAreaLeftPx() const
{
    return GetTextBaseLeft();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextAreaRightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextAreaRightPx() const
{
    return (int) m_boundsDip.right - (IsScrollbarVisible() ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellRectPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellRectPx (int row, int cell, RECT & outRect) const
{
    int    first = GetFirstLineOfRow (row);
    float  x     = 0.0f;



    if (first < m_topLine || first >= m_topLine + GetLineCap() || !GetCellLeftPx (row, cell, x))
    {
        return false;
    }

    outRect.left   = (LONG) std::lround (x);
    outRect.top    = (LONG) (m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + GetLineOffsetPx (first));
    outRect.right  = GetTextAreaRightPx();
    outRect.bottom = outRect.top + GetLineHeightPx (first);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TryGetCellTextClipPx
//
//  DrawRun starts a cell's characters at its column, unrounded, and Paint
//  clips the text at the view's right edge. The scroll bar has no track,
//  so text under it is visible.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryGetCellTextClipPx (int row, int cell, float & left, float & right) const
{
    RECT  rect    = {};
    bool  isShown = GetCellRectPx (row, cell, rect) && GetCellLeftPx (row, cell, left);



    right = (float) m_boundsDip.right;

    return isShown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetFocusRow
//
//  The focused row replaces any selection, so it is drawn, and is scrolled
//  into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetFocusRow (int row)
{
    if (row < 0 || row >= (int) m_rows.size())
    {
        m_focusRow = -1;
        return;
    }

    m_focusRow = row;
    m_anchor   = Position { row, 0 };
    m_caret    = m_anchor;

    ScrollToPosition (m_anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TryMoveFocusRow
//
//  With no row focused, the moves count from the top row in view. False
//  for a key that moves nothing, such as Ctrl+Up, which is the host's.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryMoveFocusRow (WPARAM vk, bool ctrl)
{
    int   rows   = (int) m_rows.size();
    int   page   = (std::max) (1, GetLineCap());
    int   last   = (int) m_lines.size() - 1;
    int   top    = 0;
    int   bottom = 0;
    int   from   = 0;
    int   next   = 0;
    bool  isMove = true;



    if (rows == 0 || last < 0)
    {
        return false;
    }

    top    = m_lines[(size_t) std::clamp (m_topLine, 0, last)].row;
    bottom = m_lines[(size_t) std::clamp (m_topLine + page - 1, 0, last)].row;
    from   = (m_focusRow >= 0 && m_focusRow < rows) ? m_focusRow : top;

    switch (vk)
    {
    case VK_UP:    next = from - 1;   isMove = !ctrl; break;
    case VK_DOWN:  next = from + 1;   isMove = !ctrl; break;
    case VK_PRIOR: next = ctrl ? top    : from - page; break;
    case VK_NEXT:  next = ctrl ? bottom : from + page; break;
    case VK_HOME:  next = ctrl ? 0        : top;       break;
    case VK_END:   next = ctrl ? rows - 1 : bottom;    break;
    default:       isMove = false;                     break;
    }

    if (isMove)
    {
        SetFocusRow (std::clamp (next, 0, rows - 1));
    }

    return isMove;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetHorzBarPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetHorzBarPx() const
{
    return IsHorzScrollbarVisible() ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsHorzScrollbarVisible
//
//  Measured against the width left beside a vertical bar, so the answer does
//  not depend on whether that bar is visible, which depends on this one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsHorzScrollbarVisible() const
{
    return m_noWrap && !m_proportional && m_cellWidthPx > 0 && m_widestColumns > GetTextColumns (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLeftColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLeftColumn (int column)
{
    int  most = (std::max) (0, m_widestColumns - GetTextColumns (IsScrollbarVisible()));



    m_leftColumn = m_noWrap ? std::clamp (column, 0, most) : 0;
    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetColumnCap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetColumnCap (int column, int cells)
{
    if (cells > 0)
    {
        m_columnCaps[column] = cells;
    }
    else
    {
        m_columnCaps.erase (column);
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetColumnGap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetColumnGap (int column, int cells)
{
    if (cells > 0)
    {
        m_columnGaps[column] = cells;
    }
    else
    {
        m_columnGaps.erase (column);
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetNoWrap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetNoWrap (bool noWrap)
{
    m_noWrap     = noWrap;
    m_leftColumn = noWrap ? m_leftColumn : 0;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetFirstClippedColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetFirstClippedColumn (int column)
{
    m_firstClipped = (column >= 0) ? column : -1;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetPadLeftPx
//
//  The space left of the gutter and the text: the pad every side has, or in
//  a view that fills a pane, the pane's text inset, so the text starts where
//  the pane's title does.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetPadLeftPx() const
{
    return m_paneTextInset ? DxuiPaneMetrics::GetContentTextInsetPx (m_scaler) : m_scaler.ToPx (s_kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowColors
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DxuiTextView::GetRowColors (const Row & row, const IDxuiTheme & theme) const
{
    std::vector<uint32_t>  colors;
    uint32_t               link   = theme.TextColor (DxuiTextRole::Link);



    if (row.spans.empty())
    {
        return colors;
    }

    colors.assign ((size_t) GetRowLength (row), 0u);

    for (const Span & span : row.spans)
    {
        int       base   = GetCellBase (row, span.cell);
        int       length = (span.cell < (int) row.cells.size()) ? (int) row.cells[(size_t) span.cell].size() : 0;
        uint32_t  argb   = (span.argb == 0 && span.isLink) ? link : span.argb;



        for (int i = (std::max) (0, span.start); i < span.start + span.length && i < length && argb != 0; i++)
        {
            colors[(size_t) (base + i)] = argb;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FillHighlights
//
//  Each highlighted match on the row that falls in the `count` characters
//  starting at `flatStart`, the way the selection is filled. The selected
//  match, the one the search is at, is also outlined in `edge`, unless it
//  has a fill of its own to set it apart.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::FillHighlights (IDxuiPainter & painter, const IDxuiTheme & theme, int y, int row, int cell, int cellFrom, int flatStart, int count, uint32_t edge) const
{
    float  left = (float) GetTextLeft();



    for (const FindMatch & match : m_highlights)
    {
        int  first = (std::max) (match.start.offset, flatStart);
        int  end   = (std::min) (match.start.offset + match.length, flatStart + count);

        if (match.start.row != row || end <= first)
        {
            continue;
        }

        FillSelectedRange (painter, y, row, cell, cellFrom, flatStart, count, 0.0f, match.start.offset, match.start.offset + match.length, GetFindFillOf (match, theme));

        if (m_findCurrentArgb == 0 && IsCurrentMatch (match))
        {
            painter.OutlineRect (left + GetPartPx (row, cell, cellFrom, first - flatStart), (float) y,
                                 GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_cellHeightPx,
                                 m_scaler.ToPxf (2.0f), edge);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FillSelectedRange
//
//  The selected part of `count` characters starting at `flatStart` in the
//  row's text and at `cellFrom` in the cell, and the `trailPx` after them
//  when the separator that follows is selected too: the gap after a column,
//  or the end of a row whose selection goes on to the next.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const
{
    float  left      = (float) GetTextLeft();
    int    first     = (std::max) (selFrom, flatStart);
    int    end       = (std::min) (selTo, flatStart + count);
    int    separator = flatStart + count;



    if (end > first)
    {
        painter.FillRect (left + GetPartPx (row, cell, cellFrom, first - flatStart), (float) y,
                          GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_cellHeightPx, argb);
    }

    if (trailPx > 0.0f && selFrom <= separator && separator < selTo)
    {
        painter.FillRect (left + GetPartPx (row, cell, cellFrom, count), (float) y,
                          trailPx, (float) m_cellHeightPx, argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnMouse
//
//  A press places both ends of the selection, or moves only the caret with
//  Shift, and a drag moves the caret, scrolling past either edge. A second
//  press at the same place within the double-click time selects the word.
//  A release on the link the press began on, with nothing selected, is a
//  click on the link.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT     point    = ev.positionDip;
    int64_t   nowMs    = (int64_t) GetTickCount64();
    bool      handled  = false;
    bool      twice    = false;
    bool      thrice   = false;
    int       linkRow  = -1;
    int       linkSpan = -1;
    RECT      linkRect = {};
    bool      isOnLink = false;
    Position  pos;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        //  The horizontal wheel's positive turn is to the right, the
        //  opposite of the Shift+wheel's, and wrapped text doesn't scroll
        //  sideways at all.
        //  A precision touchpad sends part notches, which add up until they
        //  move a whole line or column.
        if (ev.wheelHorizontal || (ev.shift && m_noWrap))
        {
            if (m_noWrap)
            {
                m_wheelColumns += (ev.wheelHorizontal ? ev.wheelDelta : -ev.wheelDelta) * (float) kWheelColumns;
                ScrollColumns ((int) m_wheelColumns);
                m_wheelColumns -= (float) (int) m_wheelColumns;
            }
        }
        else
        {
            m_wheelLines += -ev.wheelDelta * (float) kWheelLines;
            ScrollLines ((int) m_wheelLines);
            m_wheelLines -= (float) (int) m_wheelLines;
        }

        handled = true;
        break;

    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Right)
        {
            if (m_onContextMenu)
            {
                m_onContextMenu (point);
            }

            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && IsScrollbarVisible() && m_vertScroll.HitTest (point.x, point.y))
        {
            handled = m_vertScroll.OnMouseDown (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (ev.button == DxuiMouseButton::Left && IsHorzScrollbarVisible() && m_horzScroll.HitTest (point.x, point.y))
        {
            handled = m_horzScroll.OnMouseDown (point.x, point.y);
            SetLeftColumn (m_horzScroll.GetScrollPos());
        }
        else if (ev.button == DxuiMouseButton::Left)
        {
            pos    = HitTest (point);
            twice  = (nowMs - m_lastClickMs) <= (int64_t) DxuiSystemSettings::Instance().GetDoubleClickMs() && pos == m_lastClick;
            thrice = twice && m_clicks == 2;

            m_clicks      = twice ? m_clicks + 1 : 1;
            m_lastClickMs = nowMs;
            m_lastClick   = pos;

            m_wordDrag = twice && !thrice;
            m_focusRow = m_rowFocus ? pos.row : m_focusRow;
            isOnLink   = !twice && !ev.shift && TryGetLinkAt (point, linkRow, linkSpan, linkRect);
            m_pressRow  = isOnLink ? linkRow  : -1;
            m_pressSpan = isOnLink ? linkSpan : -1;

            //  A triple-click selects everything, as in a browser's text; a
            //  double-click selects the word, and dragging on from it takes
            //  whole words until the button comes up.
            if (thrice)
            {
                SelectAll();
                m_clicks   = 0;
                m_dragging = false;
            }
            else if (twice)
            {
                GetWordBounds (pos, m_wordFirst, m_wordLast);

                m_anchor   = m_wordFirst;
                m_caret    = m_wordLast;
                m_dragging = true;
            }
            else if (ev.shift)
            {
                m_caret    = pos;
                m_dragging = true;
            }
            else
            {
                m_anchor   = pos;
                m_caret    = pos;
                m_dragging = true;
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Move:
        if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseMove (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (m_horzScroll.IsDragging())
        {
            handled = m_horzScroll.OnMouseMove (point.x, point.y);
            SetLeftColumn (m_horzScroll.GetScrollPos());
        }
        else if (m_dragging)
        {
            if (point.y < m_boundsDip.top)
            {
                ScrollLines (-1);
            }
            else if (point.y >= m_boundsDip.bottom)
            {
                ScrollLines (1);
            }

            if (m_noWrap && point.x < GetTextBaseLeft())
            {
                SetLeftColumn (m_leftColumn - 1);
            }
            else if (m_noWrap && point.x >= m_boundsDip.right)
            {
                SetLeftColumn (m_leftColumn + 1);
            }

            pos = HitTest (point);

            if (m_wordDrag)
            {
                Position  first;
                Position  last;

                GetWordBounds (pos, first, last);

                m_anchor = (pos < m_wordFirst) ? m_wordLast : m_wordFirst;
                m_caret  = (pos < m_wordFirst) ? first      : (std::max) (last, m_wordLast);
            }
            else
            {
                m_caret = pos;
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseUp();
        }
        else if (m_horzScroll.IsDragging())
        {
            handled = m_horzScroll.OnMouseUp();
        }
        else if (m_dragging)
        {
            m_dragging = false;
            handled    = true;
            isOnLink   = m_pressRow >= 0 && !HasSelection() && TryGetLinkAt (point, linkRow, linkSpan, linkRect) &&
                         linkRow == m_pressRow && linkSpan == m_pressSpan;
            m_pressRow = -1;

            if (isOnLink && m_onLink)
            {
                m_onLink (linkRow, linkSpan);
            }
        }

        break;

    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnKey
//
//  The arrow and page keys scroll; there is no caret to move in read-only
//  text. With row focus on, they move the focused row instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnKey (const DxuiKeyEvent & ev)
{
    //  With Ctrl or Alt down the key is a shortcut, left to the host, but for
    //  the keys that move the focused row.
    bool  handled = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (m_rowFocus && ev.kind == DxuiKeyEventKind::Down && !ev.alt && TryMoveFocusRow (ev.vk, ev.ctrl))
    {
        return true;
    }

    if (!handled)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_UP:    ScrollLines (-1);             break;
    case VK_DOWN:  ScrollLines (1);              break;
    case VK_PRIOR: ScrollLines (-GetLineCap());  break;
    case VK_NEXT:  ScrollLines (GetLineCap());   break;
    case VK_HOME:  SetTopLine (0);               break;
    case VK_END:   SetTopLine (GetLineCount());  break;
    default:       handled = false;              break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::QueryCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    bool  handled = false;



    if (command == DxuiStandardCommand::Copy)
    {
        outEnabled = HasSelection();
        handled    = true;
    }
    else if (command == DxuiStandardCommand::SelectAll)
    {
        outEnabled = !m_rows.empty();
        handled    = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::InvokeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;
    bool  handled = QueryCommand (command, enabled);



    if (handled && enabled && command == DxuiStandardCommand::Copy)
    {
        CopySelection();
    }
    else if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        SelectAll();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::BuildProportionalLines
//
//  As BuildLines, by the measured width of the characters rather than their
//  count.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::BuildProportionalLines (float widthPx)
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
//  DxuiTextView::MeasureCharacters
//
//  The width of each cell's characters up to each place, measured with a
//  mark after them so a trailing space counts, less the mark's width. Each
//  column is as wide as its widest cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::MeasureCharacters (IDxuiTextRenderer & text, const DxuiFontHandle & font)
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
        const Row &  row = m_rows[r];

        m_charX[r].resize (row.cells.size());

        for (size_t c = 0; c < row.cells.size(); c++)
        {
            const std::wstring &  cell   = row.cells[c];
            std::vector<float> &  places = m_charX[r][c];

            places.assign (cell.size() + 1, 0.0f);

            for (size_t i = 1; i <= cell.size(); i++)
            {
                hr = text.MeasureString ((cell.substr (0, i) + L"|").c_str(), font.sizeDip, font.face, width, height);

                places[i] = SUCCEEDED (hr) ? (std::max) (places[i - 1], width - markW) : places[i - 1] + m_cellAdvance;
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

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellX
//
//  Where a cell begins on a line, from the line's left: past the warning
//  mark, and past every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetCellX (const Row & row, int cell) const
{
    float  x = row.warning ? (float) kWarningCells * m_cellAdvance : 0.0f;



    if (!m_proportional)
    {
        return (float) GetCellStart (row, cell) * m_cellAdvance;
    }

    for (int c = 0; c < cell && c < (int) m_columnPx.size(); c++)
    {
        auto  gap = m_columnGaps.find (c);

        x += m_columnPx[(size_t) c] + (float) ((gap != m_columnGaps.end()) ? gap->second : kColumnGapCells) * m_cellAdvance;
    }

    return x;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCharsPx
//
//  How wide a cell's characters from `from` up to `to` are drawn.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetCharsPx (int row, int cell, int from, int to) const
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
//  DxuiTextView::GetNearestChar
//
//  The boundary between a cell's characters, from `from` to `to`, nearest a
//  point `x` from where `from` is drawn, as an offset in the cell.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetNearestChar (int row, int cell, int from, int to, float x) const
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
//  DxuiTextView::GetPartPx
//
//  A wrapped line is one of the last cell's that starts past its first
//  character.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetPartPx (int row, int cell, int cellFrom, int index) const
{
    const Row  & rowData = m_rows[(size_t) row];
    bool         wrapped = cell == (int) rowData.cells.size() - 1 && cellFrom > 0;
    float        indent  = wrapped ? (float) (std::max) (0, rowData.wrapIndent) * m_cellAdvance : 0.0f;



    return GetCellX (rowData, cell) + indent + GetCharsPx (row, cell, cellFrom, cellFrom + index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Measure
//
//  What Paint measures, done ahead of it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Measure (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font = m_proportional ? theme.BodyFont() : theme.MonospaceFont();



    EnsureCellSize (text, theme);

    if (m_proportional && m_charX.size() != m_rows.size() && m_cellWidthPx > 0)
    {
        font.sizeDip = m_scaler.ToPxf (font.sizeDip * m_zoom);

        MeasureCharacters (text, font);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetContentHeightPx
//
//  Zero before the text is measured.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetContentHeightPx() const
{
    int  height = 0;



    if (m_cellHeightPx <= 0)
    {
        return 0;
    }

    for (int line = 0; line < (int) m_lines.size(); line++)
    {
        height += GetLineHeightPx (line);
    }

    return height + m_scaler.ToPx (s_kPadDip) * 2 + GetHorzBarPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiTextView::GetScrollbarMenu (POINT pt)
{
    bool  onVert = IsScrollbarVisible()     && m_vertScroll.HitTest (pt.x, pt.y);
    bool  onHorz = IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y);



    m_vertScroll.SetOnScroll ([this] (int, int pos) { SetTopLine (pos); });
    m_horzScroll.SetOnScroll ([this] (int, int pos) { SetLeftColumn (pos); });

    if (!onVert && !onHorz)
    {
        return {};
    }

    SyncScrollbar();

    return onVert ? m_vertScroll.BuildContextMenu (pt.x, pt.y) : m_horzScroll.BuildContextMenu (pt.x, pt.y);
}
