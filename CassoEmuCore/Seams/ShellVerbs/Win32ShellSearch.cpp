#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellSearch.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/ISearchSources.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/ShellVerbs/Win32FileContents.h"
#include "Seams/ShellVerbs/Win32SearchSources.h"
#include "Seams/ShellVerbs/Win32ShellNamespace.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::Win32ShellSearch
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellSearch::Win32ShellSearch (Win32ShellItemVerbs & verbs)
    : m_verbs (verbs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::RunSearch
//
//  From the Windows search index where the folder is indexed and the index
//  can answer the query as typed; otherwise by walking the folder and testing
//  each file against the parsed query. Each result's details are read from
//  the shell as it is found, the results so far are published as the walk
//  goes, and a stopped search ends with the results found before the stop.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellSearch::RunSearch (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    HRESULT                      hr        = S_OK;
    std::wstring                 scope;
    std::wstring                 query;
    bool                         parsed    = SearchQuery::TryParseId (id, scope, query);
    SearchQuery::Options         options   = SearchQuery::GetOptions (id);
    SearchExpression             expression;
    std::vector<std::wstring>    paths;
    std::map<std::wstring, int>  ranks;
    size_t                       described = 0;
    Reporter                     reporter;



    CBREx (parsed, E_INVALIDARG);

    expression        = SearchExpression::Parse (query, GetMatchOptions (options));
    reporter.stopped  = [] () { return IShellItemVerbs::s_listingStopped && IShellItemVerbs::s_listingStopped(); };
    reporter.progress = [&] (const std::vector<std::wstring> & found)
    {
        DescribeNew (found, described, ranks, options.systemFiles, outItems);

        if (IShellItemVerbs::s_listingProgress)
        {
            IShellItemVerbs::s_listingProgress (outItems);
        }
    };

    //  A search from Casso's root covers each of its folders.
    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        std::vector<std::wstring>  found;

        bool     thisPc     = folder == SearchQuery::kThisPc;
        bool     everywhere = thisPc || folder == SearchQuery::kEverywhere;
        bool     indexed    = everywhere || (CanUseIndex (expression, options) && IsIndexed (folder));
        HRESULT  hrIndex    = indexed ? QueryIndex (everywhere ? SearchQuery::kEverywhere : folder, query, found, options.currentFolder, &ranks) : S_OK;

        if (IsStopped (reporter))
        {
            break;
        }

        //  A folder outside the index, or one the index fails on, is searched
        //  by walking it. Everywhere has no folder to walk.
        if (!everywhere && (!indexed || FAILED (hrIndex)))
        {
            WalkForNames (folder, expression, options, paths, reporter);
            continue;
        }

        AddNewPaths (found, paths);

        if (thisPc)
        {
            WalkFixedDrives (expression, options, paths, reporter);
        }
        else if (everywhere)
        {
            AddRecentMatches (expression, paths);
        }

        reporter.progress (paths);
    }

    DescribeNew (paths, described, ranks, options.systemFiles, outItems);

    //  Files inside the disk images there are tested too.
    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        if (IsStopped (reporter))
        {
            break;
        }

        if (folder == SearchQuery::kThisPc)
        {
            for (const std::wstring & root : GetFixedDrives())
            {
                SearchImages (root, expression, outItems);
            }
        }
        else if (folder != SearchQuery::kEverywhere)
        {
            SearchImages (folder, expression, outItems, options.currentFolder);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::GetMatchOptions
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::MatchOptions Win32ShellSearch::GetMatchOptions (const SearchQuery::Options & options)
{
    constexpr int  kMaxFormat = 80;



    SearchExpression::MatchOptions  match;
    wchar_t                         format[kMaxFormat] = {};
    int                             length             = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE, format, kMaxFormat);



    match.contents = options.contents;
    match.today    = RowGrouping::GetToday();

    if (length > 0)
    {
        match.shortDate = format;
    }

    return match;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::CanUseIndex
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::CanUseIndex (const SearchExpression & expression, const SearchQuery::Options & options)
{
    return !expression.HasWildcards() && !expression.HasRegex() && !options.zipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::DescribeNew
//
//  Each path found since the last call, as a result. Each takes the shell a
//  few milliseconds, so they are read on several threads, and they are
//  listed in the order found.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::DescribeNew (const std::vector<std::wstring> & paths, size_t & ioDescribed, const std::map<std::wstring, int> & ranks, bool systemFiles, std::vector<IShellItemVerbs::ShellFolderItem> & ioItems)
{
    size_t                                         first = ioDescribed;
    size_t                                         count = (paths.size() > first) ? paths.size() - first : 0;
    std::vector<IShellItemVerbs::ShellFolderItem>  described (count);
    std::vector<char>                              kept (count, 0);



    ioDescribed = paths.size();

    RunInParallel (count, [&] (size_t at)
    {
        kept[at] = DescribeFound (paths[first + at], systemFiles, described[at]) ? 1 : 0;
    });

    for (size_t at = 0; at < count; at++)
    {
        auto  rank = ranks.find (paths[first + at]);

        if (kept[at] == 0)
        {
            continue;
        }

        described[at].searchRank = (rank != ranks.end()) ? rank->second : 0;
        ioItems.push_back (std::move (described[at]));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::DescribeFound
//
//  A found path as a result: false for one the shell cannot read, or a
//  system item when those are left out.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::DescribeFound (const std::wstring & path, bool systemFiles, IShellItemVerbs::ShellFolderItem & outEntry)
{
    ComPtr<IShellItem>   item;
    ComPtr<IShellItem2>  item2;
    PWSTR                shown    = nullptr;
    PROPERTYKEY          shownKey = {};
    HRESULT              hr       = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));



    hr = SUCCEEDED (hr) ? Win32ShellNamespace::DescribeShellItem (item.Get(), outEntry) : hr;

    if (FAILED (hr) || (!systemFiles && IsSystemItem (path)))
    {
        return false;
    }

    //  Explorer displays a known folder by its display name, as Recent Items
    //  for Recent.
    hr = item.As (&item2);
    hr = SUCCEEDED (hr) ? PSGetPropertyKeyFromName (L"System.ItemFolderPathDisplay", &shownKey) : hr;
    hr = SUCCEEDED (hr) ? item2->GetString (shownKey, &shown) : hr;

    if (SUCCEEDED (hr) && shown != nullptr)
    {
        outEntry.folderShown = shown;
    }

    CoTaskMemFree (shown);

    outEntry.folder = std::filesystem::path (path).parent_path().wstring();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::RunInParallel
//
//  work for each index from 0 to count, on up to s_kMaxThreads threads, each
//  in the multithreaded apartment. Returns when every index is done.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::RunInParallel (size_t count, const std::function<void (size_t)> & work)
{
    std::atomic<size_t>       next    { 0 };
    size_t                    threads = (std::min) ({ count, (size_t) (std::max) (1u, std::thread::hardware_concurrency()), s_kMaxThreads });
    std::vector<std::thread>  workers;



    for (size_t t = 0; t < threads; t++)
    {
        workers.emplace_back ([&next, &work, count] ()
        {
            HRESULT  hrCom = CoInitializeEx (nullptr, COINIT_MULTITHREADED);

            for (size_t at = next++; at < count; at = next++)
            {
                work (at);
            }

            if (SUCCEEDED (hrCom))
            {
                CoUninitialize();
            }
        });
    }

    for (std::thread & worker : workers)
    {
        worker.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::IsStopped
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::IsStopped (const Reporter & reporter)
{
    return reporter.stopped && reporter.stopped();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::IsKnown
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::IsKnown (const Reporter & reporter, const std::wstring & path)
{
    std::wstring  lower;



    if (reporter.known == nullptr)
    {
        return false;
    }

    lower.reserve (path.size());

    for (wchar_t c : path)
    {
        lower += (wchar_t) towlower (c);
    }

    return reporter.known->contains (lower);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::ReportIfDue
//
//  The paths so far, when there are new ones and the last report was long
//  enough ago.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::ReportIfDue (const Reporter & reporter, const std::vector<std::wstring> & paths, ULONGLONG & ioReportedAt, size_t & ioReported)
{
    ULONGLONG  now = GetTickCount64();



    if (!reporter.progress || paths.size() == ioReported || now - ioReportedAt < s_kProgressMs)
    {
        return;
    }

    reporter.progress (paths);

    ioReportedAt = now;
    ioReported   = paths.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::GetFindFacts
//
//  What a folder's listing has about an entry. Converting its dates to local
//  time is left out for a query with no date terms.
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::FileFacts Win32ShellSearch::GetFindFacts (const std::wstring & path, const WIN32_FIND_DATAW & found, bool withDates)
{
    SearchExpression::FileFacts  facts;



    facts.name          = found.cFileName;
    facts.path          = path;
    facts.isFolder      = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    facts.hasSize       = !facts.isFolder;
    facts.sizeBytes     = ((uint64_t) found.nFileSizeHigh << 32) | found.nFileSizeLow;
    facts.hasAttributes = true;
    facts.attributes    = found.dwFileAttributes;

    if (withDates)
    {
        facts.modified = GetLocalDate (found.ftLastWriteTime);
        facts.created  = GetLocalDate (found.ftCreationTime);
        facts.accessed = GetLocalDate (found.ftLastAccessTime);
    }

    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::GetLocalDate
//
//  The date in the time zone's rules for that date, not today's, to match
//  the dates in File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::Date Win32ShellSearch::GetLocalDate (const FILETIME & when)
{
    SearchExpression::Date  date;
    SYSTEMTIME              utc   = {};
    SYSTEMTIME              local = {};



    if ((when.dwLowDateTime == 0 && when.dwHighDateTime == 0) || !FileTimeToSystemTime (&when, &utc) || !SystemTimeToTzSpecificLocalTime (nullptr, &utc, &local))
    {
        return date;
    }

    date.has   = true;
    date.year  = local.wYear;
    date.month = local.wMonth;
    date.day   = local.wDay;

    return date;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::IsSystemItem
//
//  A file with the system attribute, or anything in the Windows folder.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::IsSystemItem (const std::wstring & path)
{
    wchar_t  windows[MAX_PATH] = {};
    DWORD    attributes        = GetFileAttributesW (path.c_str());
    UINT     length            = GetWindowsDirectoryW (windows, MAX_PATH);



    if (length == 0 || length >= MAX_PATH)
    {
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_SYSTEM) != 0;
    }

    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_SYSTEM) != 0)
    {
        return true;
    }

    return _wcsnicmp (path.c_str(), windows, length) == 0 && path.size() > length && path[length] == L'\\';
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::WalkZip
//
//  A zip's items through the shell, which opens one as a folder. An item in
//  a zip has only its name to test.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkZip (IShellItem * zip, const std::wstring & path, const SearchExpression & expression, std::vector<std::wstring> & ioPaths, int depth)
{
    HRESULT                  hr = S_OK;
    ComPtr<IEnumShellItems>  items;
    ComPtr<IShellItem>       item;
    NullSearchSources        none;



    BAIL_OUT_IF (depth >= s_kMaxZipDepth, S_OK);

    hr = zip->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    while (ioPaths.size() < s_kMaxResults && items->Next (1, item.ReleaseAndGetAddressOf(), nullptr) == S_OK)
    {
        PWSTR                        text       = nullptr;
        SFGAOF                       attributes = 0;
        HRESULT                      hrName     = item->GetDisplayName (SIGDN_PARENTRELATIVEPARSING, &text);
        HRESULT                      hrAttr     = item->GetAttributes (SFGAO_FOLDER, &attributes);
        SearchExpression::FileFacts  facts;

        facts.name     = (SUCCEEDED (hrName) && text != nullptr) ? text : L"";
        facts.path     = path + L"\\" + facts.name;
        facts.isFolder = SUCCEEDED (hrAttr) && (attributes & SFGAO_FOLDER) != 0;
        facts.hasSize  = false;

        CoTaskMemFree (text);

        if (facts.name.empty())
        {
            continue;
        }

        if (expression.HasNameTerms() && expression.Matches (facts, none))
        {
            ioPaths.push_back (facts.path);
        }

        if (facts.isFolder)
        {
            WalkZip (item.Get(), facts.path, expression, ioPaths, depth + 1);
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::SearchImages
//
//  Each disk image below the scope, read as Casso Explorer lists it; a file in
//  its catalog that matches is a result, its folder the image. An image that
//  cannot be read is passed over. No image is read for a query without a
//  term that tests a file's name.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::SearchImages (const std::wstring & scope, const SearchExpression & expression, std::vector<IShellItemVerbs::ShellFolderItem> & outItems, bool currentFolder)
{
    std::vector<std::wstring>                                   images;
    std::vector<std::vector<IShellItemVerbs::ShellFolderItem>>  found;



    if (!expression.HasNameTerms())
    {
        return;
    }

    WalkForImages (scope, images, currentFolder);

    //  Each image is read on one of several threads, and its matches are
    //  listed in the order of the images.
    found.resize (images.size());

    RunInParallel (images.size(), [&] (size_t at)
    {
        SearchImage (images[at], expression, found[at]);
    });

    for (std::vector<IShellItemVerbs::ShellFolderItem> & matches : found)
    {
        for (IShellItemVerbs::ShellFolderItem & entry : matches)
        {
            if (outItems.size() < s_kMaxResults)
            {
                outItems.push_back (std::move (entry));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::SearchImage
//
//  Every directory in the image, the volume's first, as far as the limits
//  allow: a subdirectory's files match as the volume's do. A file in an
//  image has its name, its size and its file system's modified date.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::SearchImage (const std::wstring & image, const SearchExpression & expression, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    Win32DiskFileIo           fileIo;
    DiskOperations            operations (fileIo);
    std::vector<std::string>  pending = { std::string() };
    size_t                    read    = 0;
    NullSearchSources         none;



    while (!pending.empty() && read < s_kMaxImageDirectories && outItems.size() < s_kMaxResults)
    {
        std::string             directory = pending.back();
        VolumeListing           listing;
        VolumeKind              kind      = VolumeKind::Dos33;
        DiskOperations::Result  result    = operations.List (TextEncoding::WideToNarrow (image), directory, listing, kind);
        std::wstring            shownIn   = image;

        pending.pop_back();
        read++;

        if (!result.Succeeded())
        {
            continue;
        }

        if (!directory.empty())
        {
            shownIn += L"\\" + AppleFileName::ToDisplay (directory);
            std::replace (shownIn.begin(), shownIn.end(), L'/', L'\\');
        }

        for (const FileEntry & file : listing.entries)
        {
            IShellItemVerbs::ShellFolderItem  entry;
            SearchExpression::FileFacts       facts;
            __time64_t                        when  = (__time64_t) file.modifiedUnix;
            tm                                parts = {};

            if (file.isDirectory)
            {
                pending.push_back (directory.empty() ? file.name : directory + "/" + file.name);
            }

            entry.name      = AppleFileName::ToDisplay (file.name);
            entry.sizeBytes = file.hasEofBytes ? file.eofBytes : (uint64_t) file.sizeUnits * CatalogModel::GetUnitBytes (kind);

            //  A file system's dates are wall-clock times, stored as if UTC.
            facts.name      = entry.name;
            facts.path      = shownIn + L"\\" + entry.name;
            facts.isFolder  = file.isDirectory;
            facts.sizeBytes = entry.sizeBytes;
            facts.modified  = { file.hasModified && _gmtime64_s (&parts, &when) == 0, parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday };

            if (!expression.Matches (facts, none) || outItems.size() >= s_kMaxResults)
            {
                continue;
            }

            entry.id             = facts.path;
            entry.typeText       = CatalogModel::GetEntryTypeText (file, kind);
            entry.modifiedUnix   = file.modifiedUnix;
            entry.hasModified    = file.hasModified;
            entry.modifiedLocal  = true;
            entry.isFolder       = file.isDirectory;
            entry.folder         = shownIn;
            entry.imagePath      = image;
            entry.catalogIndex   = file.catalogIndex;
            entry.imageDirectory = directory;
            outItems.push_back (std::move (entry));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::WalkForImages
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkForImages (const std::wstring & scope, std::vector<std::wstring> & outImages, bool currentFolder)
{
    std::vector<std::wstring>  pending = { scope };



    while (!pending.empty() && outImages.size() < s_kMaxImages)
    {
        std::wstring      folder = pending.back();
        WIN32_FIND_DATAW  found  = {};
        HANDLE            find   = INVALID_HANDLE_VALUE;

        pending.pop_back();

        while (!folder.empty() && folder.back() == L'\\')
        {
            folder.pop_back();
        }

        find = FindFirstFileExW ((folder + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

        if (find == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        do
        {
            std::wstring  name     = found.cFileName;
            bool          isFolder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool          isLink   = (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

            if (name == L"." || name == L"..")
            {
                continue;
            }

            if (isFolder && !isLink)
            {
                if (!currentFolder)
                {
                    pending.push_back (folder + L"\\" + name);
                }
            }
            else if (!isFolder && TreeModel::IsSupportedImage (name))
            {
                outImages.push_back (folder + L"\\" + name);
            }
        }
        while (FindNextFileW (find, &found) && outImages.size() < s_kMaxImages);

        FindClose (find);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::IsIndexed
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::IsIndexed (const std::wstring & scope)
{
    HRESULT                            hr       = S_OK;
    ComPtr<ISearchManager>             manager;
    ComPtr<ISearchCatalogManager>      catalog;
    ComPtr<ISearchCrawlScopeManager>   scopes;
    BOOL                               included = FALSE;



    hr = CoCreateInstance (__uuidof (CSearchManager), nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (&manager));
    CHR (hr);

    hr = manager->GetCatalog (L"SystemIndex", &catalog);
    CHR (hr);

    hr = catalog->GetCrawlScopeManager (&scopes);
    CHR (hr);

    hr = scopes->IncludedInCrawlScope ((L"file:///" + scope).c_str(), &included);
    CHR (hr);

Error:
    return SUCCEEDED (hr) && included;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::SaveSearch
//
//  The save dialog opens in Saved Searches, with the query as the name, as
//  File Explorer's Save search does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellSearch::SaveSearch (HWND owner, const std::wstring & id)
{
    static constexpr COMDLG_FILTERSPEC  s_kTypes[] = { { L"Saved search", L"*.search-ms" } };
    HRESULT                             hr       = S_OK;
    std::wstring                        scope;
    std::wstring                        query;
    bool                                parsed   = SearchQuery::TryParseId (id, scope, query);
    ComPtr<IFileSaveDialog>             dialog;
    ComPtr<IShellItem>                  folder;
    ComPtr<IShellItem>                  chosen;
    PWSTR                               path     = nullptr;
    std::string                         xml;
    std::wstring                        name;
    HRESULT                             hrFolder = S_OK;
    bool                                written  = false;



    CBRAEx (parsed, E_INVALIDARG);

    xml  = MakeSavedSearch (query, SearchQuery::SplitScopes (scope));
    name = query;
    std::replace_if (name.begin(), name.end(), [] (wchar_t ch) { return wcschr (L"\\/:*?\"<>|", ch) != nullptr; }, L'_');

    hr = CoCreateInstance (CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes ((UINT) std::size (s_kTypes), s_kTypes);
    CHR (hr);

    hr = dialog->SetDefaultExtension (L"search-ms");
    CHR (hr);

    hr = dialog->SetFileName (name.c_str());
    CHR (hr);

    hrFolder = SHGetKnownFolderItem (FOLDERID_SavedSearches, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS (&folder));

    if (SUCCEEDED (hrFolder))
    {
        hr = dialog->SetFolder (folder.Get());
        CHR (hr);
    }

    hr = dialog->Show (owner);
    CHR (hr);

    hr = dialog->GetResult (&chosen);
    CHR (hr);

    hr = chosen->GetDisplayName (SIGDN_FILESYSPATH, &path);
    CHR (hr);

    {
        std::ofstream  out (path, std::ios::binary | std::ios::trunc);

        out.write (xml.data(), (std::streamsize) xml.size());
        written = out.good();
        CBREx (written, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

Error:
    CoTaskMemFree (path);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::MakeSavedSearch
//
//  The persisted query Windows reads a .search-ms file as: the query as the
//  box's words, every item kind, and each folder searched with its subfolders.
//
////////////////////////////////////////////////////////////////////////////////

std::string Win32ShellSearch::MakeSavedSearch (const std::wstring & query, const std::vector<std::wstring> & scopes)
{
    std::string  xml = "<?xml version=\"1.0\"?>\r\n<persistedQuery version=\"1.0\">\r\n"
                       "\t<viewInfo viewMode=\"details\" iconSize=\"16\" stackIconSize=\"0\" autoListFlags=\"0\"/>\r\n"
                       "\t<query>\r\n\t\t<conditions>\r\n";



    xml += "\t\t\t<condition type=\"leafCondition\" property=\"System.Generic.String\" operator=\"wordmatch\" propertyType=\"string\" value=\""
         + EscapeXml (TextEncoding::WideToUtf8 (query)) + "\"/>\r\n";
    xml += "\t\t</conditions>\r\n\t\t<kindList>\r\n\t\t\t<kind name=\"item\"/>\r\n\t\t</kindList>\r\n\t\t<scope>\r\n";

    //  Home's and This PC's searches have no folder; the whole index is theirs.
    for (const std::wstring & folder : scopes)
    {
        if (folder.size() < 3 || folder[1] != L':')
        {
            continue;
        }

        xml += "\t\t\t<include path=\"" + EscapeXml (TextEncoding::WideToUtf8 (folder)) + "\"/>\r\n";
    }

    xml += "\t\t</scope>\r\n\t</query>\r\n</persistedQuery>\r\n";

    return xml;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::EscapeXml
//
////////////////////////////////////////////////////////////////////////////////

std::string Win32ShellSearch::EscapeXml (const std::string & text)
{
    std::string  escaped;



    for (char ch : text)
    {
        switch (ch)
        {
            case '&':  escaped += "&amp;";  break;
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '"':  escaped += "&quot;"; break;
            default:   escaped += ch;       break;
        }
    }

    return escaped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::QueryIndex
//
//  The index's own helper turns the query into its SQL, so the query means
//  what it means in File Explorer's box; the results are read through the
//  index's data source.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellSearch::QueryIndex (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths, bool currentFolder, std::map<std::wstring, int> * outRanks)
{
    static constexpr CLSID  s_kCollatorDataSource = { 0x9E175B8B, 0xF52A, 0x11D8, { 0xB9, 0xA5, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };
    static constexpr GUID   s_kDbGuidDefault      = { 0xC8B521FB, 0x5CF3, 0x11CE, { 0xAD, 0xE5, 0x00, 0xAA, 0x00, 0x44, 0x77, 0x3D } };



    struct Row
    {
        DBSTATUS   status = 0;
        DBLENGTH   length = 0;
        wchar_t    value[MAX_PATH * 2] = {};
        DBSTATUS   rankStatus = 0;
        LONG       rank       = 0;
    };

    HRESULT                        hr          = S_OK;
    ComPtr<ISearchManager>         manager;
    ComPtr<ISearchCatalogManager>  catalog;
    ComPtr<ISearchQueryHelper>     helper;
    ComPtr<IDBInitialize>          source;
    ComPtr<IDBCreateSession>       sessions;
    ComPtr<IDBCreateCommand>       session;
    ComPtr<ICommandText>           command;
    ComPtr<IRowset>                rows;
    ComPtr<IAccessor>              accessor;
    PWSTR                          sql         = nullptr;
    HACCESSOR                      binder      = DB_NULL_HACCESSOR;
    DBBINDING                      bindings[2] = {};
    Row                            row;



    outPaths.clear();

    hr = CoCreateInstance (__uuidof (CSearchManager), nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (&manager));
    CHR (hr);

    hr = manager->GetCatalog (L"SystemIndex", &catalog);
    CHR (hr);

    hr = catalog->GetQueryHelper (&helper);
    CHR (hr);

    hr = helper->put_QuerySelectColumns (L"System.ItemPathDisplay,System.Search.Rank");
    CHR (hr);

    //  The whole index has no scope to restrict it.
    hr = helper->put_QueryWhereRestrictions (((currentFolder ? L"AND DIRECTORY='file:" : L"AND SCOPE='file:") + ((scope == SearchQuery::kEverywhere) ? std::wstring() : scope) + L"'").c_str());
    CHR (hr);

    hr = helper->put_QueryMaxResults ((LONG) s_kMaxResults);
    CHR (hr);

    hr = helper->GenerateSQLFromUserQuery (query.c_str(), &sql);
    CHR (hr);

    hr = CoCreateInstance (s_kCollatorDataSource, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&source));
    CHR (hr);

    hr = source->Initialize();
    CHR (hr);

    hr = source.As (&sessions);
    CHR (hr);

    hr = sessions->CreateSession (nullptr, __uuidof (IDBCreateCommand), reinterpret_cast<IUnknown **> (session.GetAddressOf()));
    CHR (hr);

    hr = session->CreateCommand (nullptr, __uuidof (ICommandText), reinterpret_cast<IUnknown **> (command.GetAddressOf()));
    CHR (hr);

    hr = command->SetCommandText (s_kDbGuidDefault, sql);
    CHR (hr);

    hr = command->Execute (nullptr, __uuidof (IRowset), nullptr, nullptr, reinterpret_cast<IUnknown **> (rows.GetAddressOf()));
    CHR (hr);

    hr = rows.As (&accessor);
    CHR (hr);

    bindings[0].iOrdinal   = 1;
    bindings[0].obValue    = offsetof (Row, value);
    bindings[0].obLength   = offsetof (Row, length);
    bindings[0].obStatus   = offsetof (Row, status);
    bindings[0].dwPart     = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
    bindings[0].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    bindings[0].eParamIO   = DBPARAMIO_NOTPARAM;
    bindings[0].cbMaxLen   = sizeof (row.value);
    bindings[0].wType      = DBTYPE_WSTR;

    bindings[1].iOrdinal   = 2;
    bindings[1].obValue    = offsetof (Row, rank);
    bindings[1].obStatus   = offsetof (Row, rankStatus);
    bindings[1].dwPart     = DBPART_VALUE | DBPART_STATUS;
    bindings[1].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    bindings[1].eParamIO   = DBPARAMIO_NOTPARAM;
    bindings[1].cbMaxLen   = sizeof (row.rank);
    bindings[1].wType      = DBTYPE_I4;

    hr = accessor->CreateAccessor (DBACCESSOR_ROWDATA, 2, bindings, sizeof (Row), &binder, nullptr);
    CHR (hr);

    for (;;)
    {
        HROW          handles[64] = {};
        HROW        * fetched     = handles;
        DBCOUNTITEM   count       = 0;
        HRESULT       hrRows      = rows->GetNextRows (DB_NULL_HCHAPTER, 0, 64, &count, &fetched);

        for (DBCOUNTITEM i = 0; i < count; i++)
        {
            HRESULT  hrData = S_OK;

            row    = Row();
            hrData = rows->GetData (handles[i], binder, &row);

            if (SUCCEEDED (hrData) && row.status == DBSTATUS_S_OK)
            {
                outPaths.emplace_back (row.value);

                if (outRanks != nullptr && row.rankStatus == DBSTATUS_S_OK)
                {
                    (*outRanks)[row.value] = (int) row.rank;
                }
            }
        }

        if (count > 0)
        {
            HRESULT  hrRelease = rows->ReleaseRows (count, handles, nullptr, nullptr, nullptr);

            IGNORE_RETURN_VALUE (hrRelease, S_OK);
        }

        if (FAILED (hrRows) || hrRows == DB_S_ENDOFROWSET || count == 0)
        {
            break;
        }
    }

Error:
    if (binder != DB_NULL_HACCESSOR)
    {
        accessor->ReleaseAccessor (binder, nullptr);
    }

    CoTaskMemFree (sql);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::WalkFixedDrives
//
//  Each fixed drive's matches that the list does not have yet.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkFixedDrives (const SearchExpression & expression, const SearchQuery::Options & options, std::vector<std::wstring> & ioPaths, const Reporter & reporter)
{
    std::unordered_set<std::wstring>  known;
    Reporter                          drives = reporter;



    if (expression.IsEmpty())
    {
        return;
    }

    for (const std::wstring & path : ioPaths)
    {
        std::wstring  lower;

        for (wchar_t c : path)
        {
            lower += (wchar_t) towlower (c);
        }

        known.insert (std::move (lower));
    }

    drives.known = &known;

    for (const std::wstring & root : GetFixedDrives())
    {
        WalkForNames (root, expression, options, ioPaths, drives);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::AddRecentLinkMatches
//
//  The matches in the Recent folder, listed under a junction to it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::AddRecentLinkMatches (const std::wstring & link, const SearchExpression & expression, const SearchQuery::Options & options, std::vector<std::wstring> & ioPaths)
{
    std::wstring               recent = GetRecentFolder();
    std::vector<std::wstring>  found;
    std::error_code            error;
    std::filesystem::path      target = std::filesystem::read_symlink (link, error);



    if (recent.empty() || error || _wcsicmp (target.c_str(), recent.c_str()) != 0)
    {
        return;
    }

    WalkForNames (recent, expression, options, found);

    for (const std::wstring & path : found)
    {
        ioPaths.push_back (link + path.substr (recent.size()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::GetFixedDrives
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> Win32ShellSearch::GetFixedDrives()
{
    std::vector<std::wstring>  roots;
    DWORD                      drives = GetLogicalDrives();



    for (wchar_t letter = L'A'; letter <= L'Z'; letter++)
    {
        std::wstring  root = std::wstring (1, letter) + L":\\";

        if ((drives & (1u << (letter - L'A'))) != 0 && GetDriveTypeW (root.c_str()) == DRIVE_FIXED)
        {
            roots.push_back (root);
        }
    }

    return roots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::AddNewPaths
//
//  Each found path the list doesn't have yet, in any case.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::AddNewPaths (const std::vector<std::wstring> & found, std::vector<std::wstring> & ioPaths)
{
    std::unordered_set<std::wstring>  listed;
    auto                              lower = [] (const std::wstring & path)
    {
        std::wstring  folded;

        for (wchar_t c : path)
        {
            folded += (wchar_t) towlower (c);
        }

        return folded;
    };



    for (const std::wstring & path : ioPaths)
    {
        listed.insert (lower (path));
    }

    for (const std::wstring & path : found)
    {
        if (listed.insert (lower (path)).second)
        {
            ioPaths.push_back (path);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::GetRecentFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellSearch::GetRecentFolder()
{
    PWSTR         known  = nullptr;
    std::wstring  folder;
    HRESULT       hr     = SHGetKnownFolderPath (FOLDERID_Recent, 0, nullptr, &known);



    if (SUCCEEDED (hr))
    {
        folder = known;
    }

    CoTaskMemFree (known);

    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::AddRecentMatches
//
//  Home's search also covers recent files, whose shortcuts' targets can be
//  outside the index, as File Explorer's does. Each is tested on its name,
//  size and properties; its contents aren't read.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::AddRecentMatches (const SearchExpression & expression, std::vector<std::wstring> & ioPaths)
{
    std::vector<IShellItemVerbs::ShellFolderItem>  items;
    std::vector<std::wstring>                      found;
    Win32SearchSources::ContentsLoader             noContents;
    HRESULT                                        hr = Win32ShellNamespace::ReadHomeFolder (items);



    if (FAILED (hr) || expression.IsEmpty())
    {
        return;
    }

    for (const IShellItemVerbs::ShellFolderItem & item : items)
    {
        SearchExpression::FileFacts  facts;
        Win32SearchSources           sources (item.path, noContents);

        facts.name      = item.name;
        facts.path      = item.path;
        facts.isFolder  = item.isFolder;
        facts.hasSize   = !item.isFolder;
        facts.sizeBytes = item.sizeBytes;

        if (!item.path.empty() && expression.Matches (facts, sources))
        {
            found.push_back (item.path);
        }
    }

    AddNewPaths (found, ioPaths);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::WalkForNames
//
//  Every file and folder below the scope that matches, folder by folder; a
//  link to another folder is not followed, so a loop cannot be. Each is
//  tested on what its folder's listing has first, and its properties and
//  contents are read only when the rest of the query doesn't settle it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkForNames (const std::wstring & scope, const SearchExpression & expression, const SearchQuery::Options & options, std::vector<std::wstring> & ioPaths, const Reporter & reporter)
{
    std::vector<std::wstring>           pending                  = { scope };
    wchar_t                             tempFolder[MAX_PATH + 1] = {};
    Win32FileContents                   contents;
    Win32SearchSources::ContentsLoader  loader                   = [&contents] (const std::wstring & path, std::wstring & outText) { return contents.Load (path, outText); };
    bool                                withDates                = expression.HasField (SearchExpression::Field::DateModified)
                                                                || expression.HasField (SearchExpression::Field::DateCreated)
                                                                || expression.HasField (SearchExpression::Field::DateAccessed);
    ULONGLONG                           reportedAt               = GetTickCount64();
    size_t                              reported                 = ioPaths.size();



    //  An empty name matches no folder if this fails.
    if (GetTempPathW (MAX_PATH + 1, tempFolder) == 0)
    {
        tempFolder[0] = L'\0';
    }

    while (!pending.empty() && ioPaths.size() < s_kMaxResults && !IsStopped (reporter))
    {
        std::wstring      folder = pending.back();
        WIN32_FIND_DATAW  found  = {};
        HANDLE            find   = INVALID_HANDLE_VALUE;

        pending.pop_back();

        while (!folder.empty() && folder.back() == L'\\')
        {
            folder.pop_back();
        }

        find = FindFirstFileExW ((folder + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

        if (find == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        do
        {
            std::wstring        name     = found.cFileName;
            std::wstring        path     = folder + L"\\" + name;
            bool                isFolder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool                isLink   = (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            Win32SearchSources  sources (path, loader);

            if (name == L"." || name == L"..")
            {
                continue;
            }

            if (!IsKnown (reporter, path) && expression.Matches (GetFindFacts (path, found, withDates), sources))
            {
                ioPaths.push_back (path);
            }

            if (options.zipped && !isFolder && name.size() > 4 && _wcsicmp (name.c_str() + name.size() - 4, L".zip") == 0)
            {
                ComPtr<IShellItem>  zip;
                HRESULT             hrZip = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&zip));

                if (SUCCEEDED (hrZip))
                {
                    WalkZip (zip.Get(), path, expression, ioPaths, 0);
                }
            }

            if (isFolder && !isLink && !options.currentFolder && _wcsicmp ((path + L"\\").c_str(), tempFolder) != 0)
            {
                pending.push_back (path);
            }

            //  The junction to Recent cannot be listed, but File Explorer lists
            //  its files under it; it skips the temporary files' folder.
            if (isFolder && isLink && _wcsicmp (name.c_str(), L"Recent") == 0)
            {
                AddRecentLinkMatches (path, expression, options, ioPaths);
            }

            ReportIfDue (reporter, ioPaths, reportedAt, reported);
        }
        while (FindNextFileW (find, &found) && ioPaths.size() < s_kMaxResults && !IsStopped (reporter));

        FindClose (find);
    }
}
