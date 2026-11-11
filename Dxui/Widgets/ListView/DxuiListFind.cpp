#include "Pch.h"

#include "Widgets/ListView/DxuiListFind.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextFind.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::DxuiListFind
//
////////////////////////////////////////////////////////////////////////////////

DxuiListFind::DxuiListFind (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::SetSearch
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListFind::SetSearch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex)
{
    bool  isNew = needle != m_needle || matchCase != m_matchCase || wholeWord != m_wholeWord || isRegex != m_isRegex;



    m_needle    = needle;
    m_matchCase = matchCase;
    m_wholeWord = wholeWord;
    m_isRegex   = isRegex;

    return isNew;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::GetSpots
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListViewTypes::FindSpot> DxuiListFind::GetSpots() const
{
    std::vector<DxuiListViewTypes::FindSpot>  spots;



    if (m_needle.empty())
    {
        return spots;
    }

    for (int row = 0; row < m_list.GetRowCount(); row++)
    {
        const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (row);

        for (size_t c = 0; c < cells.size() && c < m_list.m_columns.size(); c++)
        {
            if (!m_list.m_columns[c].visible || !m_list.m_columns[c].isSearched)
            {
                continue;
            }

            for (const auto & [start, end] : DxuiTextFind::FindAll (cells[c].text, m_needle, m_matchCase, m_wholeWord, m_isRegex))
            {
                spots.push_back ({ row, c, start, end });
            }
        }
    }

    return spots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::GetCharacterFills
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DxuiListFind::GetCharacterFills (const DxuiListViewTypes::Cell & cell, uint32_t under)
{
    std::vector<uint32_t>  fills (cell.text.size(), under);



    for (const auto & [first, last, rangeArgb] : cell.backgroundRanges)
    {
        uint32_t  fill = DxuiColor::Composite (rangeArgb, under);

        for (int k = (std::max) (0, first); k < last && k < (int) fills.size(); k++)
        {
            fills[(size_t) k] = fill;
        }
    }

    return fills;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::FindRangeTip
//
////////////////////////////////////////////////////////////////////////////////

const std::wstring * DxuiListFind::FindRangeTip (const DxuiListViewTypes::Cell & cell, int position)
{
    for (const auto & [first, last, text] : cell.rangeTips)
    {
        bool  isIn    = position >= first && position < last;
        bool  isAtEnd = position == last && last == (int) cell.text.size();

        if (isIn || isAtEnd)
        {
            return &text;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::GetMatchBands
//
//  The find's fills are moved off the color under them until they contrast
//  with it, so a match reads on a filled row as on a plain one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::tuple<int, int, uint32_t>> DxuiListFind::GetMatchBands (int row, size_t column, const DxuiListViewTypes::Cell & cell, uint32_t listArgb, uint32_t under) const
{
    constexpr float                              kMinRatio = 3.0f;
    std::vector<std::tuple<int, int, uint32_t>>  bands;
    uint32_t                                     match     = DxuiColor::ComputeFillForContrast (((m_matchArgb   != 0) ? m_matchArgb   : listArgb) | 0xFF000000u, under | 0xFF000000u, kMinRatio);
    uint32_t                                     current   = DxuiColor::ComputeFillForContrast (((m_currentArgb != 0) ? m_currentArgb : listArgb) | 0xFF000000u, under | 0xFF000000u, kMinRatio);



    if (m_list.m_columns[column].align == DxuiTextHAlign::Left)
    {
        for (const std::pair<int, int> & range : cell.matches)
        {
            bands.push_back ({ range.first, range.second, listArgb });
        }
    }

    if (m_needle.empty() || !m_list.m_columns[column].visible || !m_list.m_columns[column].isSearched)
    {
        return bands;
    }

    for (const auto & [start, end] : DxuiTextFind::FindAll (cell.text, m_needle, m_matchCase, m_wholeWord, m_isRegex))
    {
        bool  isCurrent = (m_current.has_value() && m_current->row == row && m_current->column == column && m_current->start == start) ||
                          (m_isSelectedCurrent && row == m_list.m_selectedRow);

        bands.push_back ({ start, end, isCurrent ? current : match });
    }

    return bands;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::SelectMatch
//
//  The next match after the selected one, or before it going back; a new
//  search starts from the selected row. Past the last match it wraps to the
//  first, and before the first to the last.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFindResult DxuiListFind::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount)
{
    std::vector<DxuiListViewTypes::FindSpot>  spots;
    DxuiListViewTypes::FindSpot               from;
    int                                       at    = -1;
    bool                                      isNew = SetSearch (needle, matchCase, wholeWord, isRegex);



    m_isSelectedCurrent = false;

    spots    = GetSpots();
    outIndex = 0;
    outCount = (int) spots.size();

    if (spots.empty())
    {
        m_current.reset();
        return DxuiFindResult::NotFound;
    }

    from = (m_current.has_value() && !isNew) ? *m_current : DxuiListViewTypes::FindSpot { (std::max) (0, m_list.m_selectedRow), 0, forward ? -1 : INT_MAX, 0 };

    for (int i = 0; i < (int) spots.size(); i++)
    {
        if (forward && spots[(size_t) i] > from && at < 0)
        {
            at = i;
        }

        if (!forward && spots[(size_t) i] < from)
        {
            at = i;
        }
    }

    outIndex  = (at < 0) ? (forward ? 1 : (int) spots.size()) : at + 1;
    m_current = spots[(size_t) outIndex - 1];

    m_list.SetSelectedRow (m_current->row);

    return (at < 0) ? DxuiFindResult::Wrapped : DxuiFindResult::Found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind::SelectMatchInView
//
//  The first match at or below the top row, or the first of all when none
//  is.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFindResult DxuiListFind::SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount)
{
    std::vector<DxuiListViewTypes::FindSpot>  spots;



    SetSearch (needle, matchCase, wholeWord, isRegex);
    m_isSelectedCurrent = false;

    spots    = GetSpots();
    outIndex = 0;
    outCount = (int) spots.size();

    m_current.reset();

    if (spots.empty())
    {
        return DxuiFindResult::NotFound;
    }

    for (size_t i = 0; i < spots.size(); i++)
    {
        if (spots[i].row >= m_list.m_topRow)
        {
            outIndex  = (int) i + 1;
            m_current = spots[i];

            m_list.SetSelectedRow (spots[i].row);
            return DxuiFindResult::Found;
        }
    }

    outIndex  = 1;
    m_current = spots.front();

    m_list.SetSelectedRow (spots.front().row);

    return DxuiFindResult::Wrapped;
}
