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



    //  Several folders are Casso's own, searched from its root.
    if (scope.find (kScopeSeparator) != std::wstring::npos)
    {
        return L"Search results in Casso";
    }

    while (trimmed.size() > 3 && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
    {
        trimmed.pop_back();
    }

    slash = trimmed.find_last_of (L"\\/");

    return L"Search results in " + ((slash == std::wstring::npos || slash + 1 == trimmed.size()) ? trimmed : trimmed.substr (slash + 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::JoinScopes
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchQuery::JoinScopes (const std::vector<std::wstring> & folders)
{
    std::wstring  scope;



    for (const std::wstring & folder : folders)
    {
        if (!scope.empty())
        {
            scope += kScopeSeparator;
        }

        scope += folder;
    }

    return scope;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::SplitScopes
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SearchQuery::SplitScopes (const std::wstring & scope)
{
    std::vector<std::wstring>  folders;
    size_t                     start = 0;
    size_t                     at    = 0;



    while (start <= scope.size())
    {
        at = scope.find (kScopeSeparator, start);
        at = (at == std::wstring::npos) ? scope.size() : at;

        if (at > start)
        {
            folders.push_back (scope.substr (start, at - start));
        }

        start = at + 1;
    }

    return folders;
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

    return std::all_of (words.begin(), words.end(), [&lower] (const std::wstring & word)
    {
        return (word.find_first_of (L"*?") != std::wstring::npos) ? MatchesPattern (lower.c_str(), word.c_str())
                                                                    : lower.find (word) != std::wstring::npos;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::HasWildcards
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::HasWildcards (const std::wstring & query)
{
    return query.find_first_of (L"*?") != std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::MatchesPattern
//
//  * stands for any run of characters and ? for any one, over the whole name.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::MatchesPattern (const wchar_t * name, const wchar_t * pattern)
{
    const wchar_t *  star     = nullptr;
    const wchar_t *  resumeAt = nullptr;



    while (*name != L'\0')
    {
        if (*pattern == L'*')
        {
            star     = pattern++;
            resumeAt = name;
        }
        else if (*pattern == L'?' || *pattern == *name)
        {
            pattern++;
            name++;
        }
        else if (star != nullptr)
        {
            pattern = star + 1;
            name    = ++resumeAt;
        }
        else
        {
            return false;
        }
    }

    while (*pattern == L'*')
    {
        pattern++;
    }

    return *pattern == L'\0';
}