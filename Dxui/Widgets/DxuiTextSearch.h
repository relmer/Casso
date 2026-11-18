#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"
#include "IDxuiFindTarget.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch
//
//  The search over a text view's rows, each row's text with its cells joined
//  by tabs, and the colors a match is marked in.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextSearch
{
public:
    using Position   = DxuiTextPosition;
    using FindMatch  = DxuiTextMatch;
    using FindResult = DxuiFindResult;

    static bool        FindAllInRows (const std::vector<std::wstring> & rows,
                                      const std::wstring              & needle,
                                      bool                              matchCase,
                                      bool                              wholeWord,
                                      bool                              isRegex,
                                      std::vector<FindMatch>          & outMatches);
    static FindResult  FindInRows    (const std::vector<std::wstring> & rows,
                                      const std::wstring              & needle,
                                      bool                              matchCase,
                                      bool                              wholeWord,
                                      bool                              forward,
                                      Position                          from,
                                      Position                        & outStart);

    static uint32_t  GetFindHighlightFill (const IDxuiTheme & theme);
    static uint32_t  GetFindHighlightText (const IDxuiTheme & theme);

private:
    static bool          TryFindOnce   (const std::vector<std::wstring> & rows, const std::wstring & needle, bool wholeWord, bool forward, Position from, Position & outStart);
    static bool          IsWordChar    (wchar_t ch) { return iswalnum (ch) || ch == L'_'; }
    static bool          IsWholeWordAt (const std::wstring & text, size_t at, size_t length);

    //  A copy in lower case, one character for one.
    static std::wstring  GetLowered    (const std::wstring & text);
};
