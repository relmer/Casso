#include "Pch.h"

#include "Widgets/ListView/DxuiListTextSelect.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::DxuiListTextSelect
//
////////////////////////////////////////////////////////////////////////////////

DxuiListTextSelect::DxuiListTextSelect (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListTextSelect::SetEnabled (bool enabled)
{
    m_enabled = enabled;

    if (!enabled)
    {
        Clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::Clear
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListTextSelect::Clear()
{
    m_hasSel  = false;
    m_pressed = false;
    m_anchor  = DxuiListViewTypes::TextPos {};
    m_focus   = DxuiListViewTypes::TextPos {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::GetRows
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::GetRows (int & first, int & last) const
{
    if (!m_hasSel)
    {
        return false;
    }

    first = (std::min) (m_anchor.row, m_focus.row);
    last  = (std::max) (m_anchor.row, m_focus.row);

    return first >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::TryHitTestCellChar
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::TryHitTestCellChar (POINT localPx, int & row, size_t & column, int & ch) const
{
    RECT  cell = {};



    row = m_list.HitTestRow (localPx.x, localPx.y);

    if (row < 0)
    {
        return false;
    }

    for (size_t c = 0; c < m_list.m_columns.size() && c < m_list.GetRowCells (row).size(); c++)
    {
        if (m_list.GetCellTextRectPx (row, c, cell) && localPx.x >= cell.left && localPx.x < cell.right)
        {
            column = c;
            ch     = HitTestCharInCell (row, c, localPx.x);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::HitTestCharInCell
//
//  Measures the cell's text a character longer at a time until the x falls
//  short of the middle of the next character. With no renderer yet, until the
//  first paint, each character is taken as six tenths of the font.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListTextSelect::HitTestCharInCell (int row, size_t col, int lx) const
{
    const DxuiListViewTypes::Cell &  cellData = m_list.GetRowCells (row)[col];
    const std::wstring             & text     = cellData.text;
    const wchar_t                  * face     = (cellData.face != nullptr) ? cellData.face : m_list.GetBodyFace();
    float                            fontPx   = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    RECT                             cell     = {};
    float                            into     = 0.0f;
    float                            prev     = 0.0f;



    if (!m_list.GetCellTextRectPx (row, col, cell) || text.empty())
    {
        return 0;
    }

    into = (float) (lx - cell.left);

    for (int k = 1; k <= (int) text.size(); k++)
    {
        float    width = 0.0f;
        float    h     = 0.0f;
        HRESULT  hr    = S_OK;

        if (m_list.m_measureText != nullptr)
        {
            hr = m_list.m_measureText->MeasureString (text.substr (0, (size_t) k).c_str(), fontPx, face, width, h);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
        else
        {
            width = fontPx * 0.6f * (float) k;
        }

        if (into < (prev + width) * 0.5f)
        {
            return k - 1;
        }

        prev = width;
    }

    return (int) text.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::IsOverCellText
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::IsOverCellText (POINT localPx) const
{
    int   row  = m_list.HitTestRow (localPx.x, localPx.y);
    RECT  cell = {};



    if (row < 0)
    {
        return false;
    }

    for (size_t c = 0; c < m_list.m_columns.size() && c < m_list.GetRowCells (row).size(); c++)
    {
        if (m_list.m_columns[c].isText && !m_list.GetRowCells (row)[c].text.empty() &&
            m_list.GetCellTextRectPx (row, c, cell) && localPx.x >= cell.left && localPx.x < cell.right)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::HitTestText
//
//  The place in the text under a point: above the rows, the start of the top
//  row; past a row's last cell, the end of its text; below the last row, the
//  end of that row.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::TextPos DxuiListTextSelect::HitTestText (int lx, int ly) const
{
    int                         rowH    = m_list.GetRowHeightPx();
    int                         headerH = m_list.GetHeaderHeightPx();
    int                         hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx() : 0;
    int                         cap     = m_list.GetVisibleRowCapacity();
    int                         topRow  = m_list.m_topRow;
    int                         last    = (std::min) (topRow + cap, m_list.GetRowCount()) - 1;
    int                         body    = ly - headerH - hdrGap + m_list.m_scrollbars.GetRowShiftPx();
    DxuiListViewTypes::TextPos  pos;
    RECT                        cell    = {};
    bool                        found   = false;



    if (last < topRow || rowH <= 0)
    {
        return pos;
    }

    pos.row = std::clamp (topRow + (body < 0 ? -1 : body / rowH), topRow, last);

    if (body < 0)
    {
        return pos;
    }

    const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (pos.row);

    for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); c++)
    {
        if (!m_list.GetCellTextRectPx (pos.row, c, cell))
        {
            continue;
        }

        pos.col = (int) c;

        if (lx < cell.right)
        {
            pos.ch = (lx <= cell.left) ? 0 : HitTestCharInCell (pos.row, c, lx);
            found  = true;
            break;
        }
    }

    if (cells.empty())
    {
        return pos;
    }

    if (!found)
    {
        pos.ch = (int) cells[(size_t) pos.col].text.size();
    }

    if (body >= (last - topRow + 1) * rowH)
    {
        pos.col = (int) cells.size() - 1;
        pos.ch  = (int) cells[(size_t) pos.col].text.size();
    }

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::GetCellSelection
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::GetCellSelection (int row, size_t col, int length, int & start, int & end) const
{
    DxuiListViewTypes::TextPos  lo   = (std::min) (m_anchor, m_focus);
    DxuiListViewTypes::TextPos  hi   = (std::max) (m_anchor, m_focus);
    DxuiListViewTypes::TextPos  from = { row, (int) col, 0 };
    DxuiListViewTypes::TextPos  to   = { row, (int) col, length };



    if (!m_hasSel || to < lo || from > hi || (m_isColumnSel && (int) col != m_anchor.col))
    {
        return false;
    }

    start = std::clamp ((from < lo) ? lo.ch : 0,      0,     length);
    end   = std::clamp ((to > hi)   ? hi.ch : length, start, length);

    return end > start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::IsCellSelected
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::IsCellSelected (int row, size_t col) const
{
    int   start    = 0;
    int   end      = 0;
    int   length   = 0;
    bool  isInList = m_hasSel && row >= 0 && row < m_list.GetRowCount() && col < m_list.GetRowCells (row).size();



    if (!isInList)
    {
        return false;
    }

    length = (int) m_list.GetRowCells (row)[col].text.size();

    return GetCellSelection (row, col, length, start, end);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::IsWordChar
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::IsWordChar (wchar_t ch)
{
    return iswalnum (ch) || ch == L'_' || ch == L'$' || ch == L'#' || ch == L'%';
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::SelectWordAt
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListTextSelect::SelectWordAt (int lx, int ly)
{
    DxuiListViewTypes::TextPos  at    = HitTestText (lx, ly);
    int                         start = at.ch;
    int                         end   = at.ch;



    if (at.row < 0 || (size_t) at.col >= m_list.GetRowCells (at.row).size())
    {
        return;
    }

    const std::wstring &  text = m_list.GetRowCells (at.row)[(size_t) at.col].text;

    //  On the boundary after a word, the word is the one just passed.
    if (end >= (int) text.size() || !IsWordChar (text[(size_t) end]))
    {
        start = end = (std::max) (0, at.ch - 1);
    }

    while (start > 0 && IsWordChar (text[(size_t) start - 1]))         { start--; }
    while (end < (int) text.size() && IsWordChar (text[(size_t) end]))  { end++;   }

    m_anchor = DxuiListViewTypes::TextPos { at.row, at.col, start };
    m_focus  = DxuiListViewTypes::TextPos { at.row, at.col, end };
    m_hasSel = end > start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::GetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiListTextSelect::GetText() const
{
    DxuiListViewTypes::TextPos  lo   = (std::min) (m_anchor, m_focus);
    DxuiListViewTypes::TextPos  hi   = (std::max) (m_anchor, m_focus);
    std::wstring                text;



    for (int row = lo.row; row <= hi.row && row < m_list.GetRowCount(); row++)
    {
        const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (row);
        std::wstring                                  line;

        for (size_t col = 0; col < cells.size(); col++)
        {
            int  start = 0;
            int  end   = 0;

            if (GetCellSelection (row, col, (int) cells[col].text.size(), start, end))
            {
                line += (line.empty() ? L"" : L"\t") + cells[col].text.substr ((size_t) start, (size_t) (end - start));
            }
        }

        text += (row > lo.row ? L"\r\n" : L"") + line;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::OnPress
//
//  A press on text starts a selection there. Nothing is selected until the
//  pointer moves, so a plain click only picks its row and drops any text
//  selection there was.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListTextSelect::OnPress (int lx, int ly, bool ctrl)
{
    if (!m_enabled)
    {
        return;
    }

    m_hasSel      = false;
    m_pressed     = true;
    m_isColumnSel = ctrl;
    m_pressPx     = POINT { lx, ly };
    m_anchor      = HitTestText (lx, ly);
    m_focus       = m_anchor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::OnDrag
//
//  Returns whether a press is selecting text. A few pixels of jitter in a
//  click is not a drag.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::OnDrag (int lx, int ly)
{
    int  slop = m_list.m_scaler.ToPx (s_kDragSlopDip);



    if (!m_pressed)
    {
        return false;
    }

    if (m_hasSel || abs (lx - m_pressPx.x) > slop || abs (ly - m_pressPx.y) > slop)
    {
        m_focus = HitTestText (lx, ly);

        //  A Ctrl+drag stays in its column: the pointer's row, and the
        //  character under it as the column's cell has it.
        if (m_isColumnSel && m_focus.row >= 0)
        {
            m_focus.col = m_anchor.col;
            m_focus.ch  = HitTestCharInCell (m_focus.row, (size_t) m_anchor.col, lx);
        }

        m_hasSel = m_focus != m_anchor;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect::OnRelease
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListTextSelect::OnRelease()
{
    bool  dragged = m_pressed && m_hasSel;



    m_pressed = false;

    return dragged;
}
