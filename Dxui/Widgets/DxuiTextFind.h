#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextFind
//
//  Every match of a needle in one string, as a find box searches: in any case
//  or the needle's, as a whole word or anywhere, and as plain text or a
//  regular expression. A needle that is not a valid expression matches
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextFind
{
public:
    //  Half-open character ranges, in order and not overlapping.
    static std::vector<std::pair<int, int>>  FindAll (const std::wstring & text, const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex);

private:
    static bool  IsWordChar (wchar_t ch) { return iswalnum (ch) || ch == L'_'; }
    static bool  IsWholeWord (const std::wstring & text, size_t at, size_t length);
};