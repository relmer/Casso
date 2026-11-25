#include "Pch.h"

#include "Widgets/TextView/DxuiTextSelection.h"
#include "Widgets/TextView/DxuiTextViewInterfaces.h"
#include "Core/DxuiClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::HitTest
//
//  The character nearest a point, rounding to the closer side of a cell. A
//  point in the gap after a column lands at the end of that column's cell.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextSelection::Position DxuiTextSelection::HitTest (POINT point) const
{
    using Row  = DxuiTextLayout::Row;
    using Line = DxuiTextLayout::Line;

    Position  pos;
    POINT     origin    = m_geometry.GetTextOrigin();
    float     advance   = m_layout.GetCellAdvance();
    int       lineH     = m_layout.GetCellHeightPx();
    int       y         = point.y - (int) origin.y;
    int       x         = point.x - (int) origin.x + (int) ((float) m_viewport.GetLeftColumn() * advance);
    int       lineIndex = 0;
    int       column    = 0;
    int       last      = 0;



    if (m_layout.GetLines().empty() || m_layout.GetCellWidthPx() <= 0 || lineH <= 0)
    {
        return pos;
    }

    lineIndex = m_viewport.GetTopLine() + ((y < 0) ? -1 : y / lineH);
    lineIndex = (std::max) (0, (std::min) (lineIndex, m_layout.GetLineCount() - 1));

    const Line &  line = m_layout.GetLines()[(size_t) lineIndex];
    const Row &   row  = m_layout.GetRows()[(size_t) line.row];

    x      -= (int) m_layout.GetLineShift (lineIndex, m_geometry.GetTextColumns (m_viewport.IsScrollbarVisible()));
    column  = (x < 0) ? 0 : (int) (((float) x + advance / 2.0f) / advance);
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

        pos.offset = DxuiTextLayout::GetCellBase (row, last) + m_layout.GetNearestChar (line.row, last, line.start, line.start + line.length, (float) x - m_layout.GetCellX (row, last));
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

    pos.offset = DxuiTextLayout::GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - m_layout.GetCellStart (row, last), line.length));

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::Select
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::Select (Position anchor, Position caret)
{
    m_anchor = anchor;
    m_caret  = caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::SelectAll()
{
    const std::vector<DxuiTextLayout::Row> &  rows = m_layout.GetRows();



    m_anchor = Position();
    m_caret  = rows.empty() ? Position() : Position { (int) rows.size() - 1, DxuiTextLayout::GetRowLength (rows.back()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::ClearSelection()
{
    m_anchor = m_caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::Reset
//
//  New text starts with nothing selected and no drag.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::Reset()
{
    m_anchor   = Position();
    m_caret    = Position();
    m_dragging = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::SelectWordAt
//
//  The run of characters around a position that has no space or tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::SelectWordAt (Position pos)
{
    GetWordBounds (pos, m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::GetWordBounds
//
//  The start and end of the run of characters around a position, bounded by
//  spaces and the tabs between cells.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::GetWordBounds (Position pos, Position & outFirst, Position & outLast) const
{
    const std::vector<DxuiTextLayout::Row> &  rows  = m_layout.GetRows();
    std::wstring                              text  = (pos.row < (int) rows.size()) ? DxuiTextLayout::GetRowText (rows[(size_t) pos.row]) : std::wstring();
    int                                       size  = (int) text.size();
    int                                       first = (std::min) (pos.offset, size);
    int                                       last  = first;
    int                                       kind  = 0;



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
//  DxuiTextSelection::GetSelectionText
//
//  The selected characters, a tab between cells and a line break between rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextSelection::GetSelectionText() const
{
    const std::vector<DxuiTextLayout::Row> &  rows = m_layout.GetRows();
    Position                                  from = GetFrom();
    Position                                  to   = GetTo();
    std::wstring                              out;



    for (int r = from.row; HasSelection() && r <= to.row && r < (int) rows.size(); r++)
    {
        std::wstring  text  = DxuiTextLayout::GetRowText (rows[(size_t) r]);
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
//  DxuiTextSelection::CopySelection
//
//  With nothing selected the clipboard is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::CopySelection (HWND hwnd) const
{
    if (HasSelection())
    {
        DxuiClipboard::SetText (hwnd, GetSelectionText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::OnPress
//
//  A press places both ends of the selection, or moves only the caret with
//  Shift. A second press at the same place within the double-click time
//  selects the word, and a third selects everything.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextSelection::OnPress (POINT point, bool shift, int64_t nowMs)
{
    Position  pos    = HitTest (point);
    bool      twice  = (nowMs - m_lastClickMs) <= (int64_t) GetDoubleClickTime() && pos == m_lastClick;
    bool      thrice = twice && m_clicks == 2;



    m_clicks      = twice ? m_clicks + 1 : 1;
    m_lastClickMs = nowMs;
    m_lastClick   = pos;

    m_wordDrag = twice && !thrice;

    if (thrice)
    {
        //  A triple-click selects everything, as in a browser's text.
        SelectAll();
        m_clicks   = 0;
        m_dragging = false;
    }
    else if (twice)
    {
        //  A double-click selects the word, and dragging on from it
        //  takes whole words until the button comes up.
        GetWordBounds (pos, m_wordFirst, m_wordLast);

        m_anchor   = m_wordFirst;
        m_caret    = m_wordLast;
        m_dragging = true;
    }
    else if (shift)
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::OnDrag
//
//  A drag moves the caret, scrolling past either edge of the view.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextSelection::OnDrag (POINT point)
{
    const RECT &  bounds = m_geometry.GetViewBounds();
    Position      pos;
    Position      first;
    Position      last;



    if (!m_dragging)
    {
        return false;
    }

    if (point.x < bounds.left)
    {
        m_viewport.ScrollColumns (-1);
    }
    else if (point.x >= bounds.right)
    {
        m_viewport.ScrollColumns (1);
    }

    if (point.y < bounds.top)
    {
        m_viewport.ScrollLines (-1);
    }
    else if (point.y >= bounds.bottom)
    {
        m_viewport.ScrollLines (1);
    }

    pos = HitTest (point);

    if (m_wordDrag)
    {
        GetWordBounds (pos, first, last);

        m_anchor = (pos < m_wordFirst) ? m_wordLast : m_wordFirst;
        m_caret  = (pos < m_wordFirst) ? first      : (std::max) (last, m_wordLast);
    }
    else
    {
        m_caret = pos;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection::OnRelease
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextSelection::OnRelease()
{
    bool  wasDragging = m_dragging;



    m_dragging = false;

    return wasDragging;
}
