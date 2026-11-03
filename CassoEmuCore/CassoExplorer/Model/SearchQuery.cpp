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
    flags += options.contents      ? L"t" : L"";

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
    options.contents      = flags.find (L't') != std::wstring::npos;

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
//  SearchQuery::HasFilters
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::HasFilters (const std::wstring & query)
{
    for (const std::wstring & word : SplitWords (query))
    {
        std::wstring  plain = GetUnquoted (word);

        if (plain.starts_with (kDateToken) || plain.starts_with (kKindToken) || plain.starts_with (kSizeToken))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::MatchesDate
//
//  Weeks start on the locale's first day, as the date groups' do.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::MatchesDate (const std::wstring & value, const ItemFacts & facts, const RowGrouping::Today & today)
{
    int64_t  d         = 0;
    int64_t  t         = RowGrouping::ToDayNumber (today.year, today.month, today.day);
    int64_t  dow       = ((t % 7) + 7 + 4) % 7;       // 1970-01-01 was a Thursday
    int64_t  weekStart = t - ((dow - today.firstDayOfWeek + 7) % 7);
    int      lastYear  = (today.month == 1) ? today.year - 1 : today.year;
    int      lastMonth = (today.month == 1) ? 12 : today.month - 1;



    if (!facts.hasDate)
    {
        return false;
    }

    d = RowGrouping::ToDayNumber (facts.year, facts.month, facts.day);

    if (value == L"today")      { return d == t; }
    if (value == L"yesterday")  { return d == t - 1; }
    if (value == L"this week")  { return d >= weekStart && d < weekStart + 7; }
    if (value == L"last week")  { return d >= weekStart - 7 && d < weekStart; }
    if (value == L"this month") { return facts.year == today.year && facts.month == today.month; }
    if (value == L"last month") { return facts.year == lastYear && facts.month == lastMonth; }
    if (value == L"this year")  { return facts.year == today.year; }
    if (value == L"last year")  { return facts.year == today.year - 1; }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery::MatchesFilters
//
//  Every filter word must hold. Another property's word is left to the index.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchQuery::MatchesFilters (const std::wstring & query, const ItemFacts & facts, const RowGrouping::Today & today)
{
    static constexpr const wchar_t *  s_kSizes[] = { L"empty", L"tiny", L"small", L"medium", L"large", L"huge", L"gigantic" };



    for (const std::wstring & word : SplitWords (query))
    {
        std::wstring  plain = GetUnquoted (word);

        if (plain.starts_with (kDateToken) && !MatchesDate (plain.substr (wcslen (kDateToken)), facts, today))
        {
            return false;
        }

        if (plain.starts_with (kSizeToken))
        {
            int  rank = RowGrouping::GetSizeGroup (facts.isFolder, facts.sizeBytes).rank;

            for (size_t at = 0; at < std::size (s_kSizes); at++)
            {
                if (plain.substr (wcslen (kSizeToken)) == s_kSizes[at] && rank != (int) at)
                {
                    return false;
                }
            }
        }

        if (plain.starts_with (kKindToken))
        {
            std::wstring  kind = plain.substr (wcslen (kKindToken));
            bool          any  = std::find (facts.kinds.begin(), facts.kinds.end(), kind) != facts.kinds.end();

            if (kind == L"unknown" ? !facts.kinds.empty() : !any)
            {
                return false;
            }
        }
    }

    return true;
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
        return (word.find_first_of (L"*?") != std::wstring::npos) ? MatchesPattern (lower.c_str(), word.c_str())
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

    for (const std::wstring & word : words)
    {
        size_t  from  = 0;
        size_t  start = 0;

        //  Each run of text between wildcards, in order, after the last found.
        while (start <= word.size())
        {
            size_t        end   = word.find_first_of (L"*?", start);
            std::wstring  piece = word.substr (start, (end == std::wstring::npos) ? std::wstring::npos : end - start);
            size_t        at    = piece.empty() ? std::wstring::npos : lower.find (piece, from);

            if (at != std::wstring::npos)
            {
                ranges.emplace_back ((int) at, (int) (at + piece.size()));
                from = at + piece.size();
            }

            if (end == std::wstring::npos)
            {
                break;
            }

            start = end + 1;
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