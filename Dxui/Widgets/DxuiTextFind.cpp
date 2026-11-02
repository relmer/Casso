#include "Pch.h"

#include "DxuiTextFind.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextFind::FindAll
//
//  An empty match from an expression moves the search on a character, so an
//  expression such as "x*" cannot match at one place forever.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<int, int>> DxuiTextFind::FindAll (const std::wstring & text, const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex)
{
    std::vector<std::pair<int, int>>  found;
    std::wstring                      hay   = text;
    std::wstring                      pin   = needle;
    size_t                            at    = 0;



    if (needle.empty())
    {
        return found;
    }

    if (isRegex)
    {
        try
        {
            std::wregex  pattern (needle, matchCase ? std::regex::ECMAScript : (std::regex::ECMAScript | std::regex::icase));

            for (std::wsregex_iterator it (text.begin(), text.end(), pattern), end; it != end; ++it)
            {
                size_t  start  = (size_t) it->position();
                size_t  length = (size_t) it->length();

                if (length > 0 && (!wholeWord || IsWholeWord (text, start, length)))
                {
                    found.push_back ({ (int) start, (int) (start + length) });
                }
            }
        }
        catch (const std::regex_error &)
        {
            found.clear();
        }

        return found;
    }

    if (!matchCase)
    {
        std::transform (hay.begin(), hay.end(), hay.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });
        std::transform (pin.begin(), pin.end(), pin.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });
    }

    while ((at = hay.find (pin, at)) != std::wstring::npos)
    {
        if (!wholeWord || IsWholeWord (text, at, pin.size()))
        {
            found.push_back ({ (int) at, (int) (at + pin.size()) });
            at += pin.size();
        }
        else
        {
            at++;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextFind::IsWholeWord
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextFind::IsWholeWord (const std::wstring & text, size_t at, size_t length)
{
    bool  isStart = at == 0 || !IsWordChar (text[at - 1]);
    bool  isEnd   = at + length >= text.size() || !IsWordChar (text[at + length]);



    return isStart && isEnd;
}