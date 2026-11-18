#include "Pch.h"

#include "DxuiTextViewFind.h"
#include "DxuiTextSearch.h"
#include "DxuiScrollbar.h"
#include "DxuiTextLayout.h"
#include "DxuiTextViewSelection.h"
#include "IDxuiTextScroller.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::SelectMatch
//
//  A forward search starts at the end of the selection and a backward one at
//  its start, so repeating either steps from match to match. With nothing
//  selected both start at the caret, where the last click left it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextViewFind::FindResult DxuiTextViewFind::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool forward)
{
    std::vector<std::wstring>  texts  = m_layout.GetRowTexts();
    Position                   anchor;
    Position                   caret;
    Position                   from;
    Position                   start;
    FindResult                 result = FindResult::NotFound;



    GetSelection (anchor, caret);
    from   = forward ? (std::max) (anchor, caret) : (std::min) (anchor, caret);
    result = DxuiTextSearch::FindInRows (texts, needle, matchCase, wholeWord, forward, from, start);

    if (result != FindResult::NotFound)
    {
        SelectFound (start, Position { start.row, start.offset + (int) needle.size() });
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::SelectMatch  (with a regular expression option)
//
//  From the selection as the plain search starts: forward, the first match
//  starting at or after its end; backward, the last starting before its
//  start; each going round when there is none.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextViewFind::FindResult DxuiTextViewFind::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount)
{
    std::vector<FindMatch>     matches;
    Position                   anchor;
    Position                   caret;
    Position                   from;
    FindResult                 result = FindResult::Wrapped;
    size_t                     pick   = 0;



    outIndex = 0;
    outCount = 0;

    GetSelection (anchor, caret);
    from = forward ? (std::max) (anchor, caret) : (std::min) (anchor, caret);

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

    SelectFound (matches[pick].start, Position { matches[pick].start.row, matches[pick].start.offset + matches[pick].length });

    outIndex = (int) pick + 1;
    outCount = (int) matches.size();
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::SelectMatchInView
//
//  A match is on screen when the line it starts on is one the view shows.
//  Without one there, the first starting below the top of the view is taken,
//  going round to the first in the text when there is none below.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextViewFind::FindResult DxuiTextViewFind::SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount)
{
    std::vector<FindMatch>  matches;
    int                     cap    = m_scroller.GetLineCap();
    int                     top    = m_scroller.GetTopLine();
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

    auto  onScreen = std::ranges::find_if (matches, [this, cap, top] (const FindMatch & match)
    {
        int  line = m_layout.GetLineOfPosition (match.start);

        return line >= top && line < top + cap;
    });

    auto  below = std::ranges::find_if (matches, [this, top] (const FindMatch & match) { return m_layout.GetLineOfPosition (match.start) >= top; });

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

    SelectFound (matches[pick].start, Position { matches[pick].start.row, matches[pick].start.offset + matches[pick].length });

    outIndex = (int) pick + 1;
    outCount = (int) matches.size();
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::GetScopedMatches
//
//  Every match in the text, less those outside the scope while find in
//  selection is on. False when the needle is not a valid expression.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewFind::GetScopedMatches (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, std::vector<FindMatch> & outMatches) const
{
    std::vector<std::wstring>  texts = m_layout.GetRowTexts();
    bool                       valid = false;



    valid = DxuiTextSearch::FindAllInRows (texts, needle, matchCase, wholeWord, isRegex, outMatches);

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
//  DxuiTextViewFind::IsCurrentMatch
//
//  The match the search is at is the one the selection covers exactly.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewFind::IsCurrentMatch (const FindMatch & match) const
{
    Position  anchor;
    Position  caret;



    GetSelection (anchor, caret);

    return (std::min) (anchor, caret) == match.start && (std::max) (anchor, caret) == Position { match.start.row, match.start.offset + match.length };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::GetFillOf
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextViewFind::GetFillOf (const FindMatch & match, const IDxuiTheme & theme) const
{
    if (m_currentArgb != 0 && IsCurrentMatch (match))
    {
        return m_currentArgb;
    }

    return (m_matchArgb != 0) ? m_matchArgb : DxuiTextSearch::GetFindHighlightFill (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::SetScopeToSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::SetScopeToSelection()
{
    bool      byFind = std::ranges::any_of (m_highlights, [this] (const FindMatch & match) { return IsCurrentMatch (match); });
    Position  anchor;
    Position  caret;



    GetSelection (anchor, caret);

    //  A search selects its match, so the selection to keep to is the one
    //  there before the search moved it.
    m_scoped     = byFind ? !(m_userAnchor == m_userCaret) : !(anchor == caret);
    m_scopeStart = byFind ? (std::min) (m_userAnchor, m_userCaret) : (std::min) (anchor, caret);
    m_scopeEnd   = byFind ? (std::max) (m_userAnchor, m_userCaret) : (std::max) (anchor, caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::KeepUserSelection
//
//  Called before a search moves the selection: a selection the search did
//  not make is kept, for find in selection to take.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::KeepUserSelection()
{
    bool  byFind = std::ranges::any_of (m_highlights, [this] (const FindMatch & match) { return IsCurrentMatch (match); });



    if (!byFind)
    {
        GetSelection (m_userAnchor, m_userCaret);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::Reset
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::Reset()
{
    m_scoped     = false;
    m_userAnchor = Position();
    m_userCaret  = Position();
    m_highlights.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::ClampTo
//
//  The selection a search moved from is cut back to the rows, and a
//  highlight past the last row goes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::ClampTo (int rowCount, const ClampFn & clamp)
{
    clamp (m_userAnchor);
    clamp (m_userCaret);

    std::erase_if (m_highlights, [rowCount] (const FindMatch & match) { return match.start.row >= rowCount; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::DropFront
//
//  A position in a row that went is dropped: a highlight there goes, and the
//  selection a search moved from or a scope with an end there is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::DropFront (int dropped)
{
    auto  moveUp = [dropped] (Position & at) { at.row -= dropped; return at.row >= 0; };



    if (!moveUp (m_userAnchor) || !moveUp (m_userCaret))
    {
        m_userAnchor = Position();
        m_userCaret  = Position();
    }

    m_scoped = m_scoped && moveUp (m_scopeStart) && moveUp (m_scopeEnd);

    std::erase_if (m_highlights, [&moveUp] (FindMatch & match) { return !moveUp (match.start); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::SetScrollMarks
//
//  A mark on the scrollbar for each line drawn with a find's match, at the
//  line's middle down the text, and the selected match's line marked as the
//  current one; none once the find's highlights are cleared.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::SetScrollMarks (DxuiScrollbar & bar, int lineCount) const
{
    std::vector<DxuiScrollbar::Mark>  marks;
    Position                          anchor;
    Position                          caret;
    Position                          selected;
    int                               last     = -1;



    if (m_highlights.empty() || lineCount <= 0)
    {
        bar.ClearMarks();
        return;
    }

    GetSelection (anchor, caret);
    selected = (std::min) (anchor, caret);

    for (const FindMatch & match : m_highlights)
    {
        bool  isCurrent = match.start == selected && !(anchor == caret);
        int   line      = m_layout.GetLineOfPosition (match.start);

        if (line >= 0 && line != last)
        {
            marks.push_back ({ ((float) line + 0.5f) / (float) lineCount, isCurrent });
            last = line;
        }
        else if (isCurrent && !marks.empty())
        {
            marks.back().isCurrent = true;
        }
    }

    bar.SetMarks (std::move (marks), m_matchArgb, m_currentArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::GetSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::GetSelection (Position & anchor, Position & caret) const
{
    anchor = m_selection.GetAnchor();
    caret  = m_selection.GetCaret();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind::SelectFound
//
//  A match is selected and scrolled into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewFind::SelectFound (Position anchor, Position caret)
{
    m_selection.Select (anchor, caret);
    m_scroller.ScrollToPosition (anchor);
}
