#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/SearchExpression.h"
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

    static constexpr int     s_kMaxZipDepth = 8;

    //  How often a walk publishes the results it has found so far.
    static constexpr ULONGLONG  s_kProgressMs = 250;

    //  Where a walk reports: a callback for its paths so far, called from
    //  time to time, and a check for whether the search was stopped. Paths
    //  already listed, lowercased, are passed over. The disk images the walk
    //  passes are collected, so they aren't found by a second walk.
    struct Reporter
    {
        std::function<void (const std::vector<std::wstring> & paths)>    progress;
        std::function<bool ()>                                            stopped;
        const std::unordered_set<std::wstring>                          * known  = nullptr;
        std::vector<std::wstring>                                       * images = nullptr;
    };

    static HRESULT  RunSearch       (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static bool     IsIndexed       (const std::wstring & scope);

    //  The options a query is parsed with: the search's, today, and the
    //  user's short date format.
    static SearchExpression::MatchOptions  GetMatchOptions (const SearchQuery::Options & options);

    //  Whether the index can answer the query as typed: a folder is walked
    //  instead for a pattern, a regular expression or a search of zips.
    static bool  CanUseIndex (const SearchExpression & expression, const SearchQuery::Options & options);

    //  The search as a .search-ms file, in the Saved Searches folder unless the
    //  user picks another; MakeSavedSearch is its XML.
    static HRESULT      SaveSearch      (HWND owner, const std::wstring & id);
    static std::string  MakeSavedSearch (const std::wstring & query, const std::vector<std::wstring> & scopes);
    static std::string  EscapeXml       (const std::string & text);

    static HRESULT  QueryIndex      (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths, bool currentFolder = false, std::map<std::wstring, int> * outRanks = nullptr);
    static void     WalkForNames    (const std::wstring & scope, const SearchExpression & expression, const SearchQuery::Options & options, std::vector<std::wstring> & ioPaths, const Reporter & reporter = {});
    static void     WalkFixedDrives (const SearchExpression & expression, const SearchQuery::Options & options, std::vector<std::wstring> & ioPaths, const Reporter & reporter = {});

    static std::vector<std::wstring>      GetFixedDrives       ();
    static std::wstring                   GetRecentFolder      ();
    static void                           AddNewPaths          (const std::vector<std::wstring> & found, std::vector<std::wstring> & ioPaths);
    static void                           AddRecentLinkMatches (const std::wstring & link, const SearchExpression & expression, const SearchQuery::Options & options, std::vector<std::wstring> & ioPaths);
    static void                           AddRecentMatches     (const SearchExpression & expression, std::vector<std::wstring> & ioPaths);


    //  The disk images a search reads, at most, so a folder of thousands
    //  cannot keep it running for minutes.
    static constexpr size_t  s_kMaxImages           = 2000;
    static constexpr size_t  s_kMaxImageDirectories = 256;   // in one image, so a looping catalog cannot run on

    static void     SearchImages    (const std::wstring & scope, const SearchExpression & expression, std::vector<IShellItemVerbs::ShellFolderItem> & outItems, bool currentFolder = false, const std::vector<std::wstring> * walked = nullptr);
    static void     SearchImage     (const std::wstring & image, const SearchExpression & expression, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static void     WalkForImages   (const std::wstring & scope, std::vector<std::wstring> & outImages, bool currentFolder = false);

    static void                         WalkZip           (IShellItem * zip, const std::wstring & path, const SearchExpression & expression, std::vector<std::wstring> & ioPaths, int depth);
    static bool                         IsSystemItem      (const std::wstring & path);
    static SearchExpression::FileFacts  GetFindFacts      (const std::wstring & path, const WIN32_FIND_DATAW & found, bool withDates);
    static SearchExpression::Date       GetLocalDate      (const FILETIME & when);
    static bool                         DescribeFound     (const std::wstring & path, bool systemFiles, IShellItemVerbs::ShellFolderItem & outEntry);
    static void                         DescribeNew       (const std::vector<std::wstring> & paths, size_t & ioDescribed, const std::map<std::wstring, int> & ranks, bool systemFiles, std::vector<IShellItemVerbs::ShellFolderItem> & ioItems);
    static void                         RunInParallel     (size_t count, const std::function<void (size_t)> & work);
    static bool                         IsStopped         (const Reporter & reporter);
    static bool                         IsKnown           (const Reporter & reporter, const std::wstring & path);
    static void                         ReportIfDue       (const Reporter & reporter, const std::vector<std::wstring> & paths, ULONGLONG & ioReportedAt, size_t & ioReported);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;
};
