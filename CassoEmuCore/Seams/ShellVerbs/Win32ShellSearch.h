#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/SearchQuery.h"
#include "Seams/IShellItemVerbs.h"

class Win32ShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch
//
//  Search: the Windows Search index queried where a folder is indexed,
//  and a walk of the folders where it is not, for matching files and disk
//  images.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellSearch
{
public:
    explicit Win32ShellSearch (Win32ShellItemVerbs & verbs);

    //  A search stops at this many results, as a folder too large to read
    //  whole would otherwise never finish.
    static constexpr size_t  s_kMaxResults = 50000;

    //  The most threads a search uses for its results and its disk images.
    static constexpr size_t  s_kMaxThreads = 16;

    //  A file's contents are read in pieces of this size, each starting far
    //  enough back to find a word across the join.
    static constexpr size_t  s_kContentChunkBytes   = 1024 * 1024;
    static constexpr size_t  s_kContentOverlapBytes = 1024;
    static constexpr int     s_kMaxZipDepth         = 8;

    //  What a walk does beyond matching names: everything matches for a query
    //  of filters alone, which are tested afterward.
    struct WalkOptions
    {
        SearchQuery::Options  search;
        bool                  matchAll = false;
    };

    static HRESULT  RunSearch       (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static bool     IsIndexed       (const std::wstring & scope);

    //  The search as a .search-ms file, in the Saved Searches folder unless the
    //  user picks another; MakeSavedSearch is its XML.
    static HRESULT      SaveSearch      (HWND owner, const std::wstring & id);
    static std::string  MakeSavedSearch (const std::wstring & query, const std::vector<std::wstring> & scopes);
    static std::string  EscapeXml       (const std::string & text);
    static HRESULT  QueryIndex      (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths, bool currentFolder = false, std::map<std::wstring, int> * outRanks = nullptr);
    static void     WalkForNames    (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths, const WalkOptions & options = {});
    static void     WalkFixedDrives (const std::vector<std::wstring> & words, std::vector<std::wstring> & ioPaths, const WalkOptions & options = {});

    static std::vector<std::wstring>      GetFixedDrives       ();
    static std::wstring                   GetRecentFolder      ();
    static void                           AddNewPaths          (const std::vector<std::wstring> & found, std::vector<std::wstring> & ioPaths);
    static void                           AddRecentLinkMatches (const std::wstring & link, const std::vector<std::wstring> & words, std::vector<std::wstring> & ioPaths);
    static void                           AddRecentMatches     (const std::vector<std::wstring> & words, std::vector<std::wstring> & ioPaths);


    //  The disk images a search reads, at most, so a folder of thousands
    //  cannot keep it running for minutes.
    static constexpr size_t  s_kMaxImages           = 2000;
    static constexpr size_t  s_kMaxImageDirectories = 256;   // in one image, so a looping catalog cannot run on

    static void     SearchImages    (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<IShellItemVerbs::ShellFolderItem> & outItems, bool currentFolder = false);
    static void     SearchImage     (const std::wstring & image, const std::vector<std::wstring> & words, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static void     WalkForImages   (const std::wstring & scope, std::vector<std::wstring> & outImages, bool currentFolder = false);

    static bool                    FileContainsWords (const std::wstring & path, const std::vector<std::wstring> & words);
    static void                    WalkZip           (IShellItem * zip, const std::wstring & path, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths, int depth);
    static bool                    IsSystemItem      (const std::wstring & path);
    static SearchQuery::ItemFacts  GetItemFacts      (IShellItem2 * item, const IShellItemVerbs::ShellFolderItem & entry);
    static bool                    DescribeFound     (const std::wstring & path, const std::wstring & query, bool filters, bool systemFiles, const RowGrouping::Today & today, IShellItemVerbs::ShellFolderItem & outEntry);
    static void                    RunInParallel     (size_t count, const std::function<void (size_t)> & work);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;
};
