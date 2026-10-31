#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/RowGrouping.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SearchQuery
//
//  A search typed into the search box, as a location of its own: the folder
//  it covers and the query, carried in the id of a shell folder so the list,
//  the tabs and the history treat its results as they treat any folder.
//
//  The index answers a query in File Explorer's full syntax. Where a folder
//  is not indexed, its files are matched by name: every plain word of the
//  query must appear in the name, in any case. A word naming a property, such
//  as kind:document, needs the index, and is not matched by name.
//
////////////////////////////////////////////////////////////////////////////////

class SearchQuery
{
public:
    static constexpr const wchar_t *  kPrefix = L"casso-search:";

    //  Between the folders of a search that covers several, as one from
    //  Casso's root covers Casso's own: a character no path holds.
    static constexpr wchar_t          kScopeSeparator = L'*';

    static std::wstring               JoinScopes  (const std::vector<std::wstring> & folders);
    static std::vector<std::wstring>  SplitScopes (const std::wstring & scope);

    //  A search from Home covers everything the Windows index has, as File
    //  Explorer's does, rather than a folder.
    static constexpr const wchar_t *  kEverywhere = L"everywhere:";

    //  This PC: the whole index, and each fixed drive walked for what the index leaves out.
    static constexpr const wchar_t *  kThisPc     = L"thispc:";

    //  File Explorer's Search options, after the query in the id. The defaults
    //  add nothing to it.
    struct Options
    {
        bool  currentFolder = false;
        bool  systemFiles   = true;
        bool  zipped        = false;
        bool  contents      = false;
    };

    static constexpr const wchar_t *  kDateToken = L"datemodified:";
    static constexpr const wchar_t *  kKindToken = L"kind:";
    static constexpr const wchar_t *  kSizeToken = L"size:";

    //  What a filter word is tested against, where no index answers it.
    struct ItemFacts
    {
        bool                       hasDate   = false;
        int                        year      = 0;
        int                        month     = 0;
        int                        day       = 0;
        bool                       isFolder  = false;
        uint64_t                   sizeBytes = 0;
        std::vector<std::wstring>  kinds;
    };

    static std::wstring  MakeId     (const std::wstring & scope, const std::wstring & query, const Options & options);
    static Options       GetOptions (const std::wstring & id);

    //  A menu's word in the query: choosing another value of the same prefix
    //  replaces it, and choosing the value again removes it.
    static std::wstring  ToggleToken    (const std::wstring & query, const wchar_t * prefix, const std::wstring & value);
    static bool          HasToken       (const std::wstring & query, const wchar_t * prefix, const std::wstring & value);
    static bool          HasFilters     (const std::wstring & query);
    static bool          MatchesFilters (const std::wstring & query, const ItemFacts & facts, const RowGrouping::Today & today);

    static std::wstring  MakeId     (const std::wstring & scope, const std::wstring & query);
    static bool          TryParseId (const std::wstring & id, std::wstring & outScope, std::wstring & outQuery);
    static bool          IsId       (const std::wstring & id) { return id.starts_with (kPrefix); }

    //  What the address bar and the tab show, as Explorer's do.
    static std::wstring  GetLabel   (const std::wstring & scope);

    static std::vector<std::wstring>  GetNameWords (const std::wstring & query);
    static bool                       MatchesName  (const std::wstring & name, const std::vector<std::wstring> & words);

    //  A word with * or ? matches the whole name as a pattern, as in File
    //  Explorer's box; the index is left out of such a search.
    static bool  HasWildcards   (const std::wstring & query);
    static bool  MatchesPattern (const wchar_t * name, const wchar_t * pattern);

private:
    static constexpr wchar_t  s_kOptionsSeparator = L'\x1F';

    static std::vector<std::wstring>  SplitWords     (const std::wstring & query);
    static std::wstring               GetUnquoted    (const std::wstring & word);
    static bool                       MatchesDate    (const std::wstring & value, const ItemFacts & facts, const RowGrouping::Today & today);
};
