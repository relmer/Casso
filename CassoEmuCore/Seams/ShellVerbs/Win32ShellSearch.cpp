#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellSearch.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/VolumeImage.h"
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
//  From the Windows search index where the folder is indexed, in File
//  Explorer's query syntax; otherwise by walking the folder and matching
//  names. Each result is described as any shell item is, with its folder.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellSearch::RunSearch (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    HRESULT                                        hr      = S_OK;
    std::wstring                                   scope;
    std::wstring                                   query;
    std::vector<std::wstring>                      paths;
    bool                                           indexed = false;
    bool                                           parsed  = SearchQuery::TryParseId (id, scope, query);
    bool                                           filters = SearchQuery::HasFilters (query);
    RowGrouping::Today                             today   = RowGrouping::GetToday();
    WalkOptions                                    walk;
    std::map<std::wstring, int>                    ranks;
    std::vector<IShellItemVerbs::ShellFolderItem>  described;
    std::vector<char>                              kept;



    CBREx (parsed, E_INVALIDARG);

    walk.search   = SearchQuery::GetOptions (id);
    walk.matchAll = filters && SearchQuery::GetNameWords (query).empty();

    //  A search from Casso's root covers each of its folders.
    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        std::vector<std::wstring>  found;

        bool  thisPc     = folder == SearchQuery::kThisPc;
        bool  everywhere = thisPc || folder == SearchQuery::kEverywhere;

        //  The index holds no zip's files and no unindexed file's text, so those
        //  options walk the folder.
        indexed = everywhere || (IsIndexed (folder) && !SearchQuery::HasWildcards (query) && !walk.search.zipped);
        hr      = indexed ? QueryIndex (everywhere ? SearchQuery::kEverywhere : folder, query, found, walk.search.currentFolder, &ranks) : S_OK;

        if (thisPc)
        {
            WalkFixedDrives (SearchQuery::GetNameWords (query), found, walk);
            hr = S_OK;
        }
        else if (everywhere)
        {
            AddRecentMatches (SearchQuery::GetNameWords (query), found);
        }

        //  The index has only the text of the files it was told to read.
        if (!everywhere && indexed && SUCCEEDED (hr) && walk.search.contents)
        {
            std::vector<std::wstring>  walked;

            WalkForNames (folder, SearchQuery::GetNameWords (query), walked, walk);
            AddNewPaths (walked, found);
        }

        //  A folder the index leaves out, or an index that cannot answer, is
        //  searched by walking it. Everywhere has no folder to walk.
        if (!everywhere && (!indexed || FAILED (hr)))
        {
            found.clear();
            WalkForNames (folder, SearchQuery::GetNameWords (query), found, walk);
            hr = S_OK;
        }

        paths.insert (paths.end(), found.begin(), found.end());
    }

    //  Each result costs the shell a few milliseconds, so the shell is asked for
    //  them on several threads, and they are listed in the order found.
    described.resize (paths.size());
    kept.assign (paths.size(), 0);

    RunInParallel (paths.size(), [&] (size_t at)
    {
        kept[at] = DescribeFound (paths[at], query, filters, walk.search.systemFiles, today, described[at]) ? 1 : 0;
    });

    for (size_t at = 0; at < paths.size(); at++)
    {
        if (kept[at] != 0)
        {
            auto  rank = ranks.find (paths[at]);

            described[at].searchRank = (rank != ranks.end()) ? rank->second : 0;
            outItems.push_back (std::move (described[at]));
        }
    }

    //  A filter on anything but size has nothing to test in a disk image.
    if (filters && (query.find (SearchQuery::kDateToken) != std::wstring::npos || query.find (SearchQuery::kKindToken) != std::wstring::npos))
    {
        BAIL_OUT_IF (true, hr);
    }

    //  Files inside the disk images there match by name too.
    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        if (folder == SearchQuery::kThisPc)
        {
            for (const std::wstring & root : GetFixedDrives())
            {
                SearchImages (root, SearchQuery::GetNameWords (query), outItems);
            }
        }
        else if (folder != SearchQuery::kEverywhere)
        {
            SearchImages (folder, SearchQuery::GetNameWords (query), outItems, walk.search.currentFolder);
        }
    }

    //  An image's file has only a size to test.
    if (filters)
    {
        std::erase_if (outItems, [&query, &today] (const IShellItemVerbs::ShellFolderItem & entry)
        {
            SearchQuery::ItemFacts  facts;

            facts.isFolder  = entry.isFolder;
            facts.sizeBytes = entry.sizeBytes;

            return !entry.imagePath.empty() && !SearchQuery::MatchesFilters (query, facts, today);
        });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::DescribeFound
//
//  A found path as a result: false for one the shell cannot read, a system
//  item when those are left out, or one that fails the query's filters.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::DescribeFound (const std::wstring & path, const std::wstring & query, bool filters, bool systemFiles, const RowGrouping::Today & today, IShellItemVerbs::ShellFolderItem & outEntry)
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

    hr = item.As (&item2);

    if (SUCCEEDED (hr) && filters && !SearchQuery::MatchesFilters (query, GetItemFacts (item2.Get(), outEntry), today))
    {
        return false;
    }

    //  Explorer displays a known folder by its display name, as Recent Items
    //  for Recent.
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
//  Win32ShellSearch::GetItemFacts
//
////////////////////////////////////////////////////////////////////////////////

SearchQuery::ItemFacts Win32ShellSearch::GetItemFacts (IShellItem2 * item, const IShellItemVerbs::ShellFolderItem & entry)
{
    SearchQuery::ItemFacts  facts;
    __time64_t              when    = (__time64_t) entry.modifiedUnix;
    tm                      parts   = {};
    PROPVARIANT             kinds   = {};
    PROPERTYKEY             kindKey = {};
    HRESULT                 hrKinds = PSGetPropertyKeyFromName (L"System.Kind", &kindKey);



    facts.isFolder  = entry.isFolder && !entry.isFile;
    facts.sizeBytes = entry.sizeBytes;
    facts.hasDate   = entry.hasModified && _localtime64_s (&parts, &when) == 0;
    facts.year      = parts.tm_year + 1900;
    facts.month     = parts.tm_mon + 1;
    facts.day       = parts.tm_mday;

    hrKinds = SUCCEEDED (hrKinds) ? item->GetProperty (kindKey, &kinds) : hrKinds;

    if (SUCCEEDED (hrKinds))
    {
        for (ULONG at = 0; at < PropVariantGetElementCount (kinds); at++)
        {
            PWSTR    kind   = nullptr;
            HRESULT  hrKind = PropVariantGetStringElem (kinds, at, &kind);

            if (SUCCEEDED (hrKind) && kind != nullptr)
            {
                std::wstring  lower = kind;

                std::transform (lower.begin(), lower.end(), lower.begin(), towlower);
                facts.kinds.push_back (std::move (lower));
            }

            CoTaskMemFree (kind);
        }
    }

    PropVariantClear (&kinds);

    return facts;
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
//  Win32ShellSearch::FileContainsWords
//
//  Every word somewhere in the file's bytes, as ANSI or as UTF-16. A file that
//  can't be read doesn't match.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellSearch::FileContainsWords (const std::wstring & path, const std::vector<std::wstring> & words)
{
    std::ifstream              file (std::filesystem::path (path), std::ios::binary);
    std::vector<std::string>   ansi;
    std::vector<std::string>   wide;
    std::vector<bool>          found (words.size(), false);
    std::string                chunk;
    size_t                     missing = words.size();



    if (words.empty() || !file)
    {
        return false;
    }

    for (const std::wstring & word : words)
    {
        ansi.emplace_back();
        wide.emplace_back();

        for (wchar_t c : word)
        {
            ansi.back() += (char) c;
            wide.back() += (char) c;
            wide.back() += (char) (c >> 8);
        }
    }

    while (missing > 0 && file)
    {
        size_t  kept = chunk.size();

        chunk.resize (kept + s_kContentChunkBytes);
        file.read (chunk.data() + kept, (std::streamsize) s_kContentChunkBytes);
        chunk.resize (kept + (size_t) file.gcount());

        std::transform (chunk.begin() + (std::ptrdiff_t) kept, chunk.end(), chunk.begin() + (std::ptrdiff_t) kept, [] (char c) { return (char) tolower ((unsigned char) c); });

        for (size_t at = 0; at < words.size(); at++)
        {
            if (!found[at] && (chunk.find (ansi[at]) != std::string::npos || chunk.find (wide[at]) != std::string::npos))
            {
                found[at] = true;
                missing--;
            }
        }

        if (chunk.size() > s_kContentOverlapBytes)
        {
            chunk.erase (0, chunk.size() - s_kContentOverlapBytes);
        }
    }

    return missing == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::WalkZip
//
//  A zip's items through the shell, which opens one as a folder.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkZip (IShellItem * zip, const std::wstring & path, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths, int depth)
{
    HRESULT                 hr = S_OK;
    ComPtr<IEnumShellItems> items;
    ComPtr<IShellItem>      item;



    BAIL_OUT_IF (depth >= s_kMaxZipDepth, S_OK);

    hr = zip->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    while (outPaths.size() < s_kMaxResults && items->Next (1, item.ReleaseAndGetAddressOf(), nullptr) == S_OK)
    {
        PWSTR         text       = nullptr;
        SFGAOF        attributes = 0;
        HRESULT       hrName     = item->GetDisplayName (SIGDN_PARENTRELATIVEPARSING, &text);
        std::wstring  name       = (SUCCEEDED (hrName) && text != nullptr) ? text : L"";
        HRESULT       hrAttr     = item->GetAttributes (SFGAO_FOLDER, &attributes);

        CoTaskMemFree (text);

        if (name.empty())
        {
            continue;
        }

        if (SearchQuery::MatchesName (name, words) && !words.empty())
        {
            outPaths.push_back (path + L"\\" + name);
        }

        if (SUCCEEDED (hrAttr) && (attributes & SFGAO_FOLDER) != 0)
        {
            WalkZip (item.Get(), path + L"\\" + name, words, outPaths, depth + 1);
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
//  its catalog whose name holds every word is a result, its folder the image.
//  An image that cannot be read is passed over. A query of properties alone
//  matches no name, so no image is read for it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::SearchImages (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<IShellItemVerbs::ShellFolderItem> & outItems, bool currentFolder)
{
    std::vector<std::wstring>                                   images;
    std::vector<std::vector<IShellItemVerbs::ShellFolderItem>>  found;



    if (words.empty())
    {
        return;
    }

    WalkForImages (scope, images, currentFolder);

    //  Each image is read on one of several threads, and its matches are
    //  listed in the order of the images.
    found.resize (images.size());

    RunInParallel (images.size(), [&] (size_t at)
    {
        SearchImage (images[at], words, found[at]);
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
//  allow: a subdirectory's files match as the volume's do.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::SearchImage (const std::wstring & image, const std::vector<std::wstring> & words, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    Win32DiskFileIo           fileIo;
    DiskOperations            operations (fileIo);
    std::vector<std::string>  pending = { std::string() };
    size_t                    read    = 0;



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

            if (file.isDirectory)
            {
                pending.push_back (directory.empty() ? file.name : directory + "/" + file.name);
            }

            entry.name = AppleFileName::ToDisplay (file.name);

            if (!SearchQuery::MatchesName (entry.name, words) || outItems.size() >= s_kMaxResults)
            {
                continue;
            }

            entry.id             = shownIn + L"\\" + entry.name;
            entry.typeText       = CatalogModel::GetEntryTypeText (file, kind);
            entry.sizeBytes      = file.hasEofBytes ? file.eofBytes : (uint64_t) file.sizeUnits * CatalogModel::GetUnitBytes (kind);
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
        CBREx (out.good(), HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
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

void Win32ShellSearch::WalkFixedDrives (const std::vector<std::wstring> & words, std::vector<std::wstring> & ioPaths, const WalkOptions & options)
{
    for (const std::wstring & root : GetFixedDrives())
    {
        std::vector<std::wstring>  found;

        if (words.empty() && !options.matchAll)
        {
            continue;
        }

        WalkForNames (root, words, found, options);
        AddNewPaths (found, ioPaths);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::AddRecentLinkMatches
//
//  The matches in the Recent folder, named under a junction to it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::AddRecentLinkMatches (const std::wstring & link, const std::vector<std::wstring> & words, std::vector<std::wstring> & ioPaths)
{
    std::wstring               recent = GetRecentFolder();
    std::vector<std::wstring>  found;
    std::error_code            error;
    std::filesystem::path      target = std::filesystem::read_symlink (link, error);



    if (recent.empty() || error || _wcsicmp (target.c_str(), recent.c_str()) != 0)
    {
        return;
    }

    WalkForNames (recent, words, found);

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
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::AddNewPaths (const std::vector<std::wstring> & found, std::vector<std::wstring> & ioPaths)
{
    for (const std::wstring & path : found)
    {
        bool  listed = std::any_of (ioPaths.begin(), ioPaths.end(), [&] (const std::wstring & have) { return _wcsicmp (have.c_str(), path.c_str()) == 0; });

        if (!listed)
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
//  Home's search also covers recent files, whose shortcuts' targets the index
//  may leave out, as File Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::AddRecentMatches (const std::vector<std::wstring> & words, std::vector<std::wstring> & ioPaths)
{
    std::vector<IShellItemVerbs::ShellFolderItem>  items;
    std::vector<std::wstring>                      found;
    HRESULT                                        hr = Win32ShellNamespace::ReadHomeFolder (items);



    if (FAILED (hr) || words.empty())
    {
        return;
    }

    for (const IShellItemVerbs::ShellFolderItem & item : items)
    {
        if (!item.path.empty() && SearchQuery::MatchesName (item.name, words))
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
//  Every file and folder below the scope whose name holds each word, folder
//  by folder; a link to another folder is not followed, so a loop cannot.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkForNames (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths, const WalkOptions & options)
{
    std::vector<std::wstring>  pending                  = { scope };
    wchar_t                    tempFolder[MAX_PATH + 1] = {};



    //  An empty name matches no folder if this fails.
    if (GetTempPathW (MAX_PATH + 1, tempFolder) == 0)
    {
        tempFolder[0] = L'\0';
    }

    while (!pending.empty() && outPaths.size() < s_kMaxResults)
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

            if ((SearchQuery::MatchesName (name, words) && !words.empty()) || options.matchAll
                || (options.search.contents && !isFolder && FileContainsWords (folder + L"\\" + name, words)))
            {
                outPaths.push_back (folder + L"\\" + name);
            }

            if (options.search.zipped && !isFolder && name.size() > 4 && _wcsicmp (name.c_str() + name.size() - 4, L".zip") == 0)
            {
                ComPtr<IShellItem>  zip;
                HRESULT             hrZip = SHCreateItemFromParsingName ((folder + L"\\" + name).c_str(), nullptr, IID_PPV_ARGS (&zip));

                if (SUCCEEDED (hrZip))
                {
                    WalkZip (zip.Get(), folder + L"\\" + name, words, outPaths, 0);
                }
            }

            if (isFolder && !isLink && !options.search.currentFolder && _wcsicmp ((folder + L"\\" + name + L"\\").c_str(), tempFolder) != 0)
            {
                pending.push_back (folder + L"\\" + name);
            }

            //  The junction to Recent cannot be listed, but File Explorer lists
            //  its files under it; it skips the temporary files' folder.
            if (isFolder && isLink && _wcsicmp (name.c_str(), L"Recent") == 0)
            {
                AddRecentLinkMatches (folder + L"\\" + name, words, outPaths);
            }
        }
        while (FindNextFileW (find, &found) && outPaths.size() < s_kMaxResults);

        FindClose (find);
    }
}
