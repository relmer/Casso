#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListViewTypes.h"
#include "Widgets/IDxuiFindTarget.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFind
//
//  Find over the text of the visible columns of every row: each match is
//  drawn on the match fill, the selected one on the current fill, and the
//  selected match's row is selected.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListFind
{
public:
    explicit DxuiListFind (DxuiListView & list);

    DxuiFindResult  SelectMatch       (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount);
    DxuiFindResult  SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount);
    void            ClearHighlights   () { m_needle.clear(); m_current.reset(); }
    void            SetColors         (uint32_t matchArgb, uint32_t currentArgb) { m_matchArgb = matchArgb; m_currentArgb = currentArgb; }

    //  Every match of the last search, in row, column and character order.
    std::vector<DxuiListViewTypes::FindSpot>          GetSpots   () const;
    const std::optional<DxuiListViewTypes::FindSpot> & GetCurrent () const { return m_current; }

    //  A cell's highlight bands, each a range and its fill: the matches set
    //  on the cell, in the list's color, then the find's.
    std::vector<std::tuple<int, int, uint32_t>>  GetMatchBands (int row, size_t column, const DxuiListViewTypes::Cell & cell, uint32_t listArgb, uint32_t under) const;

    //  The fill under each of a cell's characters: `under`, or a background
    //  range's color drawn over it.
    static std::vector<uint32_t>  GetCharacterFills (const DxuiListViewTypes::Cell & cell, uint32_t under);

    //  The range tip at a caret position in a cell's text: the range the
    //  position is in, or at the text's end the range that ends there.
    static const std::wstring *   FindRangeTip      (const DxuiListViewTypes::Cell & cell, int position);

private:
    //  Sets the search to the find bar's; returns whether it differs from the
    //  last one.
    bool  SetSearch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex);

    DxuiListView  & m_list;

    std::wstring                                m_needle;
    bool                                        m_matchCase   = false;
    bool                                        m_wholeWord   = false;
    bool                                        m_isRegex     = false;
    uint32_t                                    m_matchArgb   = 0;
    uint32_t                                    m_currentArgb = 0;
    std::optional<DxuiListViewTypes::FindSpot>  m_current;
};
