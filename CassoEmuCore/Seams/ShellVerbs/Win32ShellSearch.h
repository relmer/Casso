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
    static constexpr size_t  s_kMaxResults = 5000;

    //  A file's contents are searched up to this size.
    static constexpr uint64_t  s_kMaxContentBytes = 16 * 1024 * 1024;
    static constexpr int       s_kMaxZipDepth     = 8;

    //  What a walk does beyond matching names: everything matches for a query
    //  of filters alone, which are tested afterward.
    struct WalkOptions
    {
        SearchQuery::Options  search;
        bool                  matchAll = false;
    };

    static HRESULT  RunSearch       (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static bool     IsIndexed       (const std::wstring & scope);
    static HRESULT  QueryIndex      (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths, bool currentFolder = false);
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
    static void     WalkForImages   (const std::wstring & scope, std::vector<std::wstring> & outImages, bool currentFolder = false);

    static bool                    FileContainsWords (const std::wstring & path, uint64_t sizeBytes, const std::vector<std::wstring> & words);
    static void                    WalkZip           (IShellItem * zip, const std::wstring & path, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths, int depth);
    static bool                    IsSystemItem      (const std::wstring & path);
    static SearchQuery::ItemFacts  GetItemFacts      (IShellItem2 * item, const IShellItemVerbs::ShellFolderItem & entry);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;
};
