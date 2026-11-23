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
//  SearchQuery::MakeId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchQuery::MakeId (const std::wstring & scope, const std::wstring & query, const Options & options)
{
    std::wstring  flags;



    flags += options.currentFolder ? L"c" : L"";
    flags += options.systemFiles   ? L""  : L"x";
    flags += options.zipped        ? L"z" : L"";
    flags += options.contents      ? L""  : L"n";

    return MakeId (scope, query) + (flags.empty() ? std::wstring() : s_kOptionsSeparator + flags);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::GetOptions
//
////////////////////////////////////////////////////////////////////////////////

SearchQuery::Options SearchQuery::GetOptions (const std::wstring & id)
{
    Options       options;
    size_t        at    = id.find (s_kOptionsSeparator);
    std::wstring  flags = (at == std::wstring::npos) ? std::wstring() : id.substr (at + 1);



    options.currentFolder = flags.find (L'c') != std::wstring::npos;
    options.systemFiles   = flags.find (L'x') == std::wstring::npos;
    options.zipped        = flags.find (L'z') != std::wstring::npos;
    options.contents      = flags.find (L'n') == std::wstring::npos;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::SplitWords
//
//  The query's words as typed, a quoted phrase within one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SearchQuery::SplitWords (const std::wstring & query)
{
    std::vector<std::wstring>  words;
    std::wstring               word;
    bool                       quoted = false;



    for (wchar_t c : query)
    {
        if (c == L'"')
        {
            quoted = !quoted;
        }

        if (c == L' ' && !quoted)
        {
            if (!word.empty())
            {
                words.push_back (word);
                word.clear();
            }

            continue;
        }

        word += c;
    }

    if (!word.empty())
    {
        words.push_back (word);
    }

    return words;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::GetUnquoted
//
//  Lowercased, without its quotes.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchQuery::GetUnquoted (const std::wstring & word)
{
    std::wstring  plain;



    for (wchar_t c : word)
    {
        if (c != L'"')
        {
            plain += (wchar_t) towlower (c);
        }
    }

    return plain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::HasToken
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::HasToken (const std::wstring & query, const wchar_t * prefix, const std::wstring & value)
{
    std::wstring  wanted = prefix + value;



    for (const std::wstring & word : SplitWords (query))
    {
        if (GetUnquoted (word) == wanted)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::ToggleToken
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchQuery::ToggleToken (const std::wstring & query, const wchar_t * prefix, const std::wstring & value)
{
    std::wstring  result;
    bool          chosen = HasToken (query, prefix, value);
    auto          append = [&result] (const std::wstring & word)
    {
        result += (result.empty() ? L"" : L" ") + word;
    };



    for (const std::wstring & word : SplitWords (query))
    {
        if (!GetUnquoted (word).starts_with (prefix))
        {
            append (word);
        }
    }

    if (!chosen)
    {
        append (prefix + ((value.find (L' ') != std::wstring::npos) ? L"\"" + value + L"\"" : value));
    }

    return result;
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
    outQuery = id.substr (bar + 1, id.find (s_kOptionsSeparator, bar) - bar - 1);

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



    if (scope == kThisPc)
    {
        return L"Search results in This PC";
    }

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
        return (word.find_first_of (L"*?") != std::wstring::npos) ? MatchesWildcard (lower, word)
                                                                    : lower.find (word) != std::wstring::npos;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::FindNameMatches
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<int, int>> SearchQuery::FindNameMatches (const std::wstring & name, const std::vector<std::wstring> & words)
{
    std::vector<std::pair<int, int>>  ranges;
    std::vector<std::pair<int, int>>  merged;
    std::wstring                      lower;



    for (wchar_t c : name)
    {
        lower += (wchar_t) towlower (c);
    }

    //  A pattern with a period is not marked, as in File Explorer.
    for (const std::wstring & word : words)
    {
        bool    pattern = word.find_first_of (L"*?") != std::wstring::npos;
        size_t  at      = (word.empty() || pattern) ? std::wstring::npos : lower.find (word);



        if (pattern && word.find (L'.') == std::wstring::npos)
        {
            FindWildcardRanges (lower, word, ranges);
        }

        if (at != std::wstring::npos)
        {
            ranges.emplace_back ((int) at, (int) (at + word.size()));
        }
    }

    std::sort (ranges.begin(), ranges.end());

    for (const std::pair<int, int> & range : ranges)
    {
        if (!merged.empty() && range.first <= merged.back().second)
        {
            merged.back().second = (std::max) (merged.back().second, range.second);
            continue;
        }

        merged.push_back (range);
    }

    return merged;
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
//  SearchQuery::MatchesWildcard
//
//  A pattern with a period matches the whole name, so *.dsk finds only the
//  .dsk files. Any other pattern matches from the start of any word in the
//  name to anywhere after it, so wiz*scenario finds "Wizardry I scenario.woz".
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::MatchesWildcard (const std::wstring & name, const std::wstring & pattern)
{
    std::wstring  open = pattern + L'*';



    if (pattern.find (L'.') != std::wstring::npos)
    {
        return MatchesPattern (name.c_str(), pattern.c_str());
    }

    for (size_t i = 0; i < name.size(); i++)
    {
        bool  wordStart = i == 0 || !iswalnum (name[i - 1]) || (iswalpha (name[i]) && !iswalpha (name[i - 1]));



        if (wordStart && MatchesPattern (name.c_str() + i, open.c_str()))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::FindWildcardRanges
//
//  From the first word where the pattern matches, each run between * marks
//  is at the first place it fits after the one before it.
//
////////////////////////////////////////////////////////////////////////////////

void SearchQuery::FindWildcardRanges (const std::wstring & lower, const std::wstring & pattern, std::vector<std::pair<int, int>> & ioRanges)
{
    std::vector<std::wstring>  runs;
    std::wstring               open = pattern + L'*';
    size_t                     at   = std::wstring::npos;
    size_t                     from = 0;



    for (size_t i = 0; i < lower.size() && at == std::wstring::npos; i++)
    {
        bool  wordStart = i == 0 || !iswalnum (lower[i - 1]) || (iswalpha (lower[i]) && !iswalpha (lower[i - 1]));



        at = (wordStart && MatchesPattern (lower.c_str() + i, open.c_str())) ? i : std::wstring::npos;
    }

    if (at == std::wstring::npos)
    {
        return;
    }

    for (size_t start = 0; start <= pattern.size(); )
    {
        size_t  star = pattern.find (L'*', start);



        star = (star == std::wstring::npos) ? pattern.size() : star;
        runs.push_back (pattern.substr (start, star - start));
        start = star + 1;
    }

    for (size_t r = 0; r < runs.size(); r++)
    {
        const std::wstring  & run   = runs[r];
        size_t                place = (r == 0) ? at : from;
        bool                  fits  = false;



        if (run.empty())
        {
            continue;
        }

        for (; place + run.size() <= lower.size() && !fits; place += fits ? 0 : 1)
        {
            fits = std::equal (run.begin(), run.end(), lower.begin() + (std::ptrdiff_t) place, [] (wchar_t p, wchar_t c) { return p == L'?' || p == c; });

            if (r == 0 && !fits)
            {
                return;
            }
        }

        if (!fits)
        {
            return;
        }

        ioRanges.emplace_back ((int) place, (int) (place + run.size()));
        from = place + run.size();
    }
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
