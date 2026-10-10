#pragma once

#include "Pch.h"





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

    static std::wstring  MakeId     (const std::wstring & scope, const std::wstring & query);
    static bool          TryParseId (const std::wstring & id, std::wstring & outScope, std::wstring & outQuery);
    static bool          IsId       (const std::wstring & id) { return id.starts_with (kPrefix); }

    //  What the address bar and the tab show, as Explorer's do.
    static std::wstring  GetLabel   (const std::wstring & scope);

    static std::vector<std::wstring>  GetNameWords (const std::wstring & query);
    static bool                       MatchesName  (const std::wstring & name, const std::vector<std::wstring> & words);
};
