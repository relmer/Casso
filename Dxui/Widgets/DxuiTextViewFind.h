#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"
#include "IDxuiFindTarget.h"


class DxuiScrollbar;
class DxuiTextLayout;
class DxuiTextViewSelection;
class IDxuiTextScroller;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFind
//
//  Find in a text view: the match selected and scrolled into view, every
//  match of the last search highlighted, find in selection, and the colors
//  of the marks under the matches.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewFind
{
public:
    using Position   = DxuiTextPosition;
    using FindMatch  = DxuiTextMatch;
    using FindResult = DxuiFindResult;
    using ClampFn    = std::function<void (Position & at)>;

    DxuiTextViewFind (const DxuiTextLayout & layout, DxuiTextViewSelection & selection, IDxuiTextScroller & scroller) : m_layout (layout), m_selection (selection), m_scroller (scroller) {}

    FindResult  SelectMatch       (const std::wstring & needle, bool matchCase, bool wholeWord, bool forward);
    FindResult  SelectMatch       (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount);
    FindResult  SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount);

    void        SetScopeToSelection ();
    void        ClearScope          ()       { m_scoped = false; }
    bool        IsScoped            () const { return m_scoped; }

    const std::vector<FindMatch> &  GetHighlights   () const { return m_highlights; }
    void                            ClearHighlights ()       { m_highlights.clear(); }

    void      SetColors      (uint32_t matchArgb, uint32_t currentArgb) { m_matchArgb = matchArgb; m_currentArgb = currentArgb; }
    uint32_t  GetMatchArgb   () const { return m_matchArgb; }
    uint32_t  GetCurrentArgb () const { return m_currentArgb; }
    bool      IsCurrentMatch (const FindMatch & match) const;
    uint32_t  GetFillOf      (const FindMatch & match, const IDxuiTheme & theme) const;
    void      SetScrollMarks (DxuiScrollbar & bar, int lineCount) const;

    //  The rows replaced, the same rows changed, and rows dropped from the
    //  front of the text.
    void  Reset     ();
    void  ClampTo   (int rowCount, const ClampFn & clamp);
    void  DropFront (int dropped);

private:
    bool  GetScopedMatches  (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, std::vector<FindMatch> & outMatches) const;
    void  KeepUserSelection ();
    void  GetSelection      (Position & anchor, Position & caret) const;
    void  SelectFound       (Position anchor, Position caret);

    const DxuiTextLayout     & m_layout;
    DxuiTextViewSelection    & m_selection;
    IDxuiTextScroller        & m_scroller;
    bool                       m_scoped      = false;
    Position                   m_scopeStart;
    Position                   m_scopeEnd;
    std::vector<FindMatch>     m_highlights;
    Position                   m_userAnchor;
    Position                   m_userCaret;
    uint32_t                   m_matchArgb   = 0;
    uint32_t                   m_currentArgb = 0;
};
