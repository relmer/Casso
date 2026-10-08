#include "Pch.h"

#include "CassoExplorer/Model/SearchQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::MakeId
//
//  The folder, then the query after a character no path holds.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchQuery::MakeId (const std::wstring & scope, const std::wstring & query)
{
    return kPrefix + scope + L"|" + query;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::TryParseId
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::TryParseId (const std::wstring & id, std::wstring & outScope, std::wstring & outQuery)
{
    size_t  bar = id.find (L'|');



    outScope.clear();
    outQuery.clear();

    if (!IsId (id) || bar == std::wstring::npos)
    {
        return false;
    }

    outScope = id.substr (wcslen (kPrefix), bar - wcslen (kPrefix));
    outQuery = id.substr (bar + 1);

    return !outScope.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchQuery::GetLabel (const std::wstring & scope)
{
    std::wstring  trimmed = scope;
    size_t        slash   = 0;



    while (trimmed.size() > 3 && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
    {
        trimmed.pop_back();
    }

    slash = trimmed.find_last_of (L"\\/");

    return L"Search results in " + ((slash == std::wstring::npos || slash + 1 == trimmed.size()) ? trimmed : trimmed.substr (slash + 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::GetNameWords
//
//  The query's plain words, lowercased; a quoted phrase is one word, and a
//  word naming a property is left to the index.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SearchQuery::GetNameWords (const std::wstring & query)
{
    std::vector<std::wstring>  words;
    std::wstring               word;
    bool                       quoted = false;



    for (size_t i = 0; i <= query.size(); i++)
    {
        wchar_t  c   = (i < query.size()) ? query[i] : L' ';
        bool     end = (c == L' ' && !quoted) || i == query.size();

        if (c == L'"')
        {
            quoted = !quoted;
        }
        else if (!end)
        {
            word += (wchar_t) towlower (c);
        }

        if (end && !word.empty())
        {
            if (word.find (L':') == std::wstring::npos)
            {
                words.push_back (word);
            }

            word.clear();
        }
    }

    return words;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::MatchesName
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::MatchesName (const std::wstring & name, const std::vector<std::wstring> & words)
{
    std::wstring  lower;



    for (wchar_t c : name)
    {
        lower += (wchar_t) towlower (c);
    }

    return std::all_of (words.begin(), words.end(), [&lower] (const std::wstring & word) { return lower.find (word) != std::wstring::npos; });
}
