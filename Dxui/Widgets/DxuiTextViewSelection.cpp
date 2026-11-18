#include "Pch.h"

#include "DxuiTextViewSelection.h"
#include "DxuiTextLayout.h"
#include "Core/DxuiClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewSelection::SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewSelection::SelectAll()
{
    const std::vector<DxuiTextRow>  & rows = m_layout.GetRows();



    m_anchor = Position();
    m_caret  = rows.empty() ? Position() : Position { (int) rows.size() - 1, DxuiTextLayout::GetRowLength (rows.back()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewSelection::GetWordBounds
//
//  The start and end of the run of characters around a position, bounded by
//  spaces and the tabs between cells.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewSelection::GetWordBounds (Position pos, Position & outFirst, Position & outLast) const
{
    const std::vector<DxuiTextRow>  & rows  = m_layout.GetRows();
    std::wstring                      text  = (pos.row < (int) rows.size()) ? DxuiTextLayout::GetRowText (rows[(size_t) pos.row]) : std::wstring();
    int                               size  = (int) text.size();
    int                               first = (std::min) (pos.offset, size);
    int                               last  = first;
    int                               kind  = 0;



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
//  DxuiTextViewSelection::GetText
//
//  The selected characters, a tab between cells and a line break between rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextViewSelection::GetText() const
{
    const std::vector<DxuiTextRow>  & rows = m_layout.GetRows();
    Position                          from = (std::min) (m_anchor, m_caret);
    Position                          to   = (std::max) (m_anchor, m_caret);
    std::wstring                      out;



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
//  DxuiTextViewSelection::Copy
//
//  With nothing selected the clipboard is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewSelection::Copy (HWND hwnd) const
{
    if (HasSelection())
    {
        DxuiClipboard::SetText (hwnd, GetText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewSelection::DropFront
//
//  An end in a row that went clears the selection; otherwise both ends move
//  up with their rows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewSelection::DropFront (int dropped)
{
    auto  moveUp = [dropped] (Position & at) { at.row -= dropped; return at.row >= 0; };



    if (!moveUp (m_anchor) || !moveUp (m_caret))
    {
        Reset();
    }
}
