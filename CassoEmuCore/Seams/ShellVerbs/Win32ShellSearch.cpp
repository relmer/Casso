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
    HRESULT                    hr      = S_OK;
    std::wstring               scope;
    std::wstring               query;
    std::vector<std::wstring>  paths;
    bool                       indexed = false;
    bool                       parsed  = SearchQuery::TryParseId (id, scope, query);



    CBREx (parsed, E_INVALIDARG);

    //  A search from Casso's root covers each of its folders.
    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        std::vector<std::wstring>  found;

        bool  everywhere = folder == SearchQuery::kEverywhere;

        indexed = everywhere || (IsIndexed (folder) && !SearchQuery::HasWildcards (query));
        hr      = indexed ? QueryIndex (folder, query, found) : S_OK;

        //  A folder the index leaves out, or an index that cannot answer, is
        //  searched by walking it. Everywhere has no folder to walk.
        if (!everywhere && (!indexed || FAILED (hr)))
        {
            found.clear();
            WalkForNames (folder, SearchQuery::GetNameWords (query), found);
            hr = S_OK;
        }

        paths.insert (paths.end(), found.begin(), found.end());
    }

    for (const std::wstring & path : paths)
    {
        ComPtr<IShellItem>                item;
        IShellItemVerbs::ShellFolderItem  entry;
        HRESULT                           hrItem = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));

        if (SUCCEEDED (hrItem))
        {
            hrItem = Win32ShellNamespace::DescribeShellItem (item.Get(), entry);
        }

        if (SUCCEEDED (hrItem))
        {
            entry.folder = std::filesystem::path (path).parent_path().wstring();
            outItems.push_back (std::move (entry));
        }
    }

    //  Files inside the disk images there match by name too.
    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        if (folder != SearchQuery::kEverywhere)
        {
            SearchImages (folder, SearchQuery::GetNameWords (query), outItems);
        }
    }

Error:
    return hr;
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

void Win32ShellSearch::SearchImages (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    std::vector<std::wstring>  images;
    Win32DiskFileIo            fileIo;
    DiskOperations             operations (fileIo);



    if (words.empty())
    {
        return;
    }

    WalkForImages (scope, images);

    for (const std::wstring & image : images)
    {
        std::vector<std::string>  pending = { std::string() };
        size_t                    read    = 0;

        //  Every directory in the image, the volume's first, as far as the
        //  limits allow: a subdirectory's files match as the volume's do.
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
                entry.typeText       = CatalogModel::GetTypeText (file.type, kind);
                entry.sizeBytes      = file.hasEofBytes ? file.eofBytes : (uint64_t) file.sizeUnits * ((kind == VolumeKind::Dos33) ? 256u : 512u);
                entry.isFolder       = file.isDirectory;
                entry.folder         = shownIn;
                entry.imagePath      = image;
                entry.catalogIndex   = file.catalogIndex;
                entry.imageDirectory = directory;
                outItems.push_back (std::move (entry));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch::WalkForImages
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkForImages (const std::wstring & scope, std::vector<std::wstring> & outImages)
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
                pending.push_back (folder + L"\\" + name);
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
//  Win32ShellSearch::QueryIndex
//
//  The index's own helper turns the query into its SQL, so the query means
//  what it means in File Explorer's box; the results are read through the
//  index's data source.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellSearch::QueryIndex (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths)
{
    static constexpr CLSID  s_kCollatorDataSource = { 0x9E175B8B, 0xF52A, 0x11D8, { 0xB9, 0xA5, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };
    static constexpr GUID   s_kDbGuidDefault      = { 0xC8B521FB, 0x5CF3, 0x11CE, { 0xAD, 0xE5, 0x00, 0xAA, 0x00, 0x44, 0x77, 0x3D } };



    struct Row
    {
        DBSTATUS   status = 0;
        DBLENGTH   length = 0;
        wchar_t    value[MAX_PATH * 2] = {};
    };

    HRESULT                          hr       = S_OK;
    ComPtr<ISearchManager>           manager;
    ComPtr<ISearchCatalogManager>    catalog;
    ComPtr<ISearchQueryHelper>       helper;
    ComPtr<IDBInitialize>            source;
    ComPtr<IDBCreateSession>         sessions;
    ComPtr<IDBCreateCommand>         session;
    ComPtr<ICommandText>             command;
    ComPtr<IRowset>                  rows;
    ComPtr<IAccessor>                accessor;
    PWSTR                            sql      = nullptr;
    HACCESSOR                        binder   = DB_NULL_HACCESSOR;
    DBBINDING                        binding  = {};
    Row                              row;



    outPaths.clear();

    hr = CoCreateInstance (__uuidof (CSearchManager), nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (&manager));
    CHR (hr);

    hr = manager->GetCatalog (L"SystemIndex", &catalog);
    CHR (hr);

    hr = catalog->GetQueryHelper (&helper);
    CHR (hr);

    hr = helper->put_QuerySelectColumns (L"System.ItemPathDisplay");
    CHR (hr);

    //  The whole index has no scope to restrict it.
    hr = helper->put_QueryWhereRestrictions ((L"AND SCOPE='file:" + ((scope == SearchQuery::kEverywhere) ? std::wstring() : scope) + L"'").c_str());
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

    binding.iOrdinal   = 1;
    binding.obValue    = offsetof (Row, value);
    binding.obLength   = offsetof (Row, length);
    binding.obStatus   = offsetof (Row, status);
    binding.dwPart     = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
    binding.dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    binding.eParamIO   = DBPARAMIO_NOTPARAM;
    binding.cbMaxLen   = sizeof (row.value);
    binding.wType      = DBTYPE_WSTR;

    hr = accessor->CreateAccessor (DBACCESSOR_ROWDATA, 1, &binding, sizeof (Row), &binder, nullptr);
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
//  Win32ShellSearch::WalkForNames
//
//  Every file and folder below the scope whose name holds each word, folder
//  by folder; a link to another folder is not followed, so a loop cannot.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellSearch::WalkForNames (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths)
{
    std::vector<std::wstring>  pending = { scope };



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

            if (SearchQuery::MatchesName (name, words) && !words.empty())
            {
                outPaths.push_back (folder + L"\\" + name);
            }

            if (isFolder && !isLink)
            {
                pending.push_back (folder + L"\\" + name);
            }
        }
        while (FindNextFileW (find, &found) && outPaths.size() < s_kMaxResults);

        FindClose (find);
    }
}
