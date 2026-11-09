#include "Pch.h"
#include "EhmTestHelper.h"
#include "CassoExplorer/Model/RowGrouping.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Seams/ShellItemId.h"
#include "Seams/ShellVerbs/Win32ShellNamespace.h"
#include "Seams/ShellVerbs/Win32ShellSearch.h"
#include "Seams/Win32PropertyColumns.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShellSearchTests
//
//  Search options and shell properties against real files and a real zip, in a
//  scratch folder the index doesn't cover, so the search walks it. Home's items
//  are checked as what Copy and Properties act on, without the clipboard or a
//  dialog.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerShellSearchTests)
{
public:

    using Items = std::vector<IShellItemVerbs::ShellFolderItem>;

    TEST_METHOD_INITIALIZE (Initialize)
    {
        HRESULT  hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);

        Assert::IsTrue (SUCCEEDED (hr));

        std::error_code  ec;

        m_dir = std::filesystem::temp_directory_path (ec) / std::format (L"casso-search-{}-{}", GetCurrentProcessId(), GetTickCount64());
        std::filesystem::create_directories (m_dir / L"sub", ec);

        WriteFile (m_dir / L"needle_plain.txt",      "plain");
        WriteFile (m_dir / L"content_only.txt",      "has NeedleWord inside");
        WriteFile (m_dir / L"other.txt",             "nothing");
        WriteFile (m_dir / L"needle_sys.txt",        "x");
        WriteFile (m_dir / L"sub" / L"deep_needle.txt", "x");
        WriteStoredZip (m_dir / L"pack.zip", "inner_needle.txt", "x");

        SetFileAttributesW ((m_dir / L"needle_sys.txt").c_str(), FILE_ATTRIBUTE_SYSTEM);
    }

    TEST_METHOD_CLEANUP (Cleanup)
    {
        std::error_code  ec;

        SetFileAttributesW ((m_dir / L"needle_sys.txt").c_str(), FILE_ATTRIBUTE_NORMAL);
        std::filesystem::remove_all (m_dir, ec);
        CoUninitialize();
    }

    TEST_METHOD (Search_AllSubfolders_FindsNamesBelowTheFolder)
    {
        SearchQuery::Options  options;

        options.contents = false;

        Items  items = Search (L"needle", options);

        Assert::IsTrue  (Has (items, L"needle_plain.txt"));
        Assert::IsTrue  (Has (items, L"needle_sys.txt"));
        Assert::IsTrue  (Has (items, L"deep_needle.txt"));
        Assert::IsFalse (Has (items, L"content_only.txt"), L"With File contents off, a word matches the name alone");
        Assert::IsFalse (Has (items, L"inner_needle.txt"), L"Zips aren't searched by default");
    }

    TEST_METHOD (Search_ContentsAreOnByDefault)
    {
        Items  items = Search (L"needleword", SearchQuery::Options());

        Assert::IsTrue  (Has (items, L"content_only.txt"));
        Assert::IsFalse (Has (items, L"other.txt"));
    }

    //  A word in the name and another in the text match the file together.
    TEST_METHOD (Search_Contents_MatchesWordByWord)
    {
        Items  items = Search (L"content needleword", SearchQuery::Options());

        Assert::IsTrue  (Has (items, L"content_only.txt"));
        Assert::IsFalse (Has (Search (L"name:needleword", SearchQuery::Options()), L"content_only.txt"), L"name: tests the name alone");
        Assert::IsTrue  (Has (Search (L"content:needleword", SearchQuery::Options()), L"content_only.txt"));
    }

    //  A property the walk doesn't test itself is looked up in the property
    //  system by its display name or its canonical name.
    TEST_METHOD (Search_OtherProperty_IsLookedUpByItsDisplayName)
    {
        Assert::IsTrue  (Has (Search (L"fileextension:.txt needle_plain", SearchQuery::Options()), L"needle_plain.txt"));
        Assert::IsFalse (Has (Search (L"fileextension:.zip needle_plain", SearchQuery::Options()), L"needle_plain.txt"));
        Assert::IsTrue  (Has (Search (L"System.FileExtension:.txt needle_plain", SearchQuery::Options()), L"needle_plain.txt"));
    }

    TEST_METHOD (Search_Syntax_CombinesTermsWithOrNotAndRegularExpressions)
    {
        SearchQuery::Options  options;

        options.contents = false;

        Assert::IsTrue  (Has (Search (L"plain OR deep", options), L"deep_needle.txt"));
        Assert::IsFalse (Has (Search (L"needle -deep", options), L"deep_needle.txt"));
        Assert::IsTrue  (Has (Search (L"/^needle_(plain|sys)\\.txt$/", options), L"needle_sys.txt"));
        Assert::IsFalse (Has (Search (L"/^needle_(plain|sys)\\.txt$/", options), L"deep_needle.txt"));
        Assert::IsTrue  (Has (Search (L"ext:txt attributes:system", options), L"needle_sys.txt"));
        Assert::IsFalse (Has (Search (L"ext:txt attributes:system", options), L"needle_plain.txt"));
    }

    TEST_METHOD (Search_CurrentFolder_LeavesSubfoldersOut)
    {
        SearchQuery::Options  options;

        options.currentFolder = true;

        Items  items = Search (L"needle", options);

        Assert::IsTrue  (Has (items, L"needle_plain.txt"));
        Assert::IsFalse (Has (items, L"deep_needle.txt"));
    }

    TEST_METHOD (Search_SystemFilesOff_LeavesThemOut)
    {
        SearchQuery::Options  options;

        options.systemFiles = false;

        Items  items = Search (L"needle", options);

        Assert::IsTrue  (Has (items, L"needle_plain.txt"));
        Assert::IsFalse (Has (items, L"needle_sys.txt"));
    }

    TEST_METHOD (Search_Zipped_FindsAFileInAZip_WithItsPath)
    {
        SearchQuery::Options  options;

        options.zipped = true;

        Items  items = Search (L"needle", options);

        Assert::IsTrue (Has (items, L"inner_needle.txt"));

        std::vector<PROPERTYKEY>  keys (1);
        HRESULT                   hr = Win32PropertyColumns::GetKey (L"System.ItemPathDisplay", keys[0]);

        Assert::IsTrue (SUCCEEDED (hr));

        Win32PropertyColumns::Values  values = Win32PropertyColumns::Read ((m_dir / L"pack.zip" / L"inner_needle.txt").wstring(), keys);

        Assert::IsTrue (values[0].has, L"An item in a zip has a path");
        Assert::IsTrue (values[0].text.ends_with (L"pack.zip\\inner_needle.txt"), values[0].text.c_str());
    }

    TEST_METHOD (Search_Contents_FindsAnsiAndUtf16)
    {
        SearchQuery::Options  options;

        options.contents = true;

        WriteFile (m_dir / L"wide.txt", std::string ("n\0e\0e\0d\0l\0e\0", 12));

        Items  items = Search (L"needle", options);

        Assert::IsTrue  (Has (items, L"content_only.txt"), L"In any case");
        Assert::IsTrue  (Has (items, L"wide.txt"));
        Assert::IsFalse (Has (items, L"other.txt"));
    }

    //  A word near the end of a large text file is found.
    TEST_METHOD (Search_Contents_FindsAWordNearTheEndOfALargeFile)
    {
        constexpr size_t  kBytes = 8 * 1024 * 1024;

        SearchQuery::Options  options;
        std::string           bytes (kBytes, 'x');

        options.contents = true;

        bytes.replace (kBytes - 10, 6, "needle");
        WriteFile (m_dir / L"large.log", bytes);

        Assert::IsTrue (Has (Search (L"needle", options), L"large.log"));
    }

    TEST_METHOD (Search_Filters_TestSizeKindAndDate)
    {
        WriteFile (m_dir / L"sized_needle.txt", std::string (20 * 1024, 'x'));

        Items  sized = Search (L"needle size:small", SearchQuery::Options());

        Assert::IsTrue  (Has (sized, L"sized_needle.txt"));
        Assert::IsFalse (Has (sized, L"needle_plain.txt"));

        Assert::IsTrue  (Has (Search (L"needle kind:document", SearchQuery::Options()), L"needle_plain.txt"));
        Assert::IsFalse (Has (Search (L"needle kind:picture",  SearchQuery::Options()), L"needle_plain.txt"));

        Assert::IsTrue  (Has (Search (L"needle datemodified:today",       SearchQuery::Options()), L"needle_plain.txt"));
        Assert::IsFalse (Has (Search (L"needle datemodified:\"last year\"", SearchQuery::Options()), L"needle_plain.txt"));
    }

    //  A filter alone matches every file it passes.
    TEST_METHOD (Search_FilterAlone_ListsWhatPasses)
    {
        Items  items = Search (L"kind:document", SearchQuery::Options());

        Assert::IsTrue (Has (items, L"other.txt"));
    }

    TEST_METHOD (Search_ShowsTheFolderAsTheShellDisplaysIt)
    {
        Items  items = Search (L"needle_plain", SearchQuery::Options());

        Assert::AreEqual ((size_t) 1, items.size());
        Assert::IsFalse  (items[0].folderShown.empty());
    }

    //  A saved search is the query, every kind of item, and each folder on the
    //  disk it covered, escaped for XML.
    TEST_METHOD (SavedSearch_HasTheQueryAndTheFolders)
    {
        std::string  xml = Win32ShellSearch::MakeSavedSearch (L"*.h & <x>", { L"C:\\Repo", L"pc:" });

        Assert::IsTrue (xml.find ("value=\"*.h &amp; &lt;x&gt;\"") != std::string::npos);
        Assert::IsTrue (xml.find ("<include path=\"C:\\Repo\"/>") != std::string::npos);
        Assert::IsTrue (xml.find ("pc:") == std::string::npos, L"This PC has no folder to include");
        Assert::IsTrue (xml.find ("<kind name=\"item\"/>") != std::string::npos);
    }

    //  The labels and the group of a real drive.
    TEST_METHOD (DriveProperties_GroupByWindowsRanges)
    {
        std::vector<PROPERTYKEY>  keys (2);
        HRESULT                   hrCapacity = Win32PropertyColumns::GetKey (L"System.Capacity", keys[0]);
        HRESULT                   hrFull     = Win32PropertyColumns::GetKey (L"System.PercentFull", keys[1]);

        Assert::IsTrue (SUCCEEDED (hrCapacity) && SUCCEEDED (hrFull));

        Win32PropertyColumns::Values  values = Win32PropertyColumns::Read (L"C:\\", keys);

        Assert::IsTrue (values[0].has);
        Assert::IsTrue (values[0].isDate == false);
        Assert::AreEqual (RowGrouping::GetCapacityGroup ((uint64_t) values[0].number).label, values[0].groupLabel);
        Assert::IsTrue (values[1].groupLabel.find (L'%') != std::wstring::npos, values[1].groupLabel.c_str());
    }

    //  What Copy and Properties act on: an item Home lists has an id the shell
    //  takes back, or its path.
    TEST_METHOD (HomeItems_HaveIdsForCopyAndProperties)
    {
        Items    items;
        HRESULT  hr = Win32ShellNamespace::ReadHomeFolder (items);

        Assert::IsTrue (SUCCEEDED (hr));

        for (const IShellItemVerbs::ShellFolderItem & item : items)
        {
            std::error_code   ec;
            PIDLIST_ABSOLUTE  pidl   = nullptr;
            HRESULT           hrPidl = S_OK;

            //  A recent file since deleted is listed with nothing to act on.
            if (!item.path.empty() && !std::filesystem::exists (item.path, ec))
            {
                continue;
            }

            hrPidl = ShellItemId::ToPidl (item.path.empty() ? item.id : item.path, &pidl);

            CoTaskMemFree (pidl);

            Assert::IsTrue (SUCCEEDED (hrPidl), item.name.c_str());
        }
    }

    //  A search of C:\Windows by name, timed twice, so the second run reads
    //  the folders from the cache, and a contents search's first results,
    //  stopped when they arrive. File Explorer's time for the same search is
    //  measured in File Explorer. Set CASSO_MEASURE_SEARCH to run it.
    TEST_METHOD (Measure_WindowsFolder_NameSearchAndFirstContentsResults)
    {
        constexpr double  kMaxFirstMs = 2000;
        constexpr DWORD   kFlagChars  = 8;
        constexpr int     kRuns       = 2;

        wchar_t               flag[kFlagChars] = {};
        SearchQuery::Options  options;
        Items                 found;
        double                firstMs          = -1;
        auto                  start            = std::chrono::steady_clock::now();
        HRESULT               hr               = S_OK;

        if (GetEnvironmentVariableW (L"CASSO_MEASURE_SEARCH", flag, kFlagChars) == 0)
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_MEASURE_SEARCH to measure");
            return;
        }

        options.contents = false;

        for (int run = 0; run < kRuns; run++)
        {
            found.clear();
            start = std::chrono::steady_clock::now();
            hr    = Win32ShellSearch::RunSearch (SearchQuery::MakeId (L"C:\\Windows", L"setup", options), found);

            Assert::IsTrue (SUCCEEDED (hr));
            Logger::WriteMessage (std::format ("Name search, run {}: {} results in {:.0f} ms", run + 1, found.size(),
                                               std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count()).c_str());
        }

        options.contents                   = true;
        start                              = std::chrono::steady_clock::now();
        IShellItemVerbs::s_listingStopped  = [&firstMs]() { return firstMs >= 0; };
        IShellItemVerbs::s_listingProgress = [&] (const Items & so)
        {
            if (firstMs < 0 && !so.empty())
            {
                firstMs = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();
            }
        };

        found.clear();
        hr = Win32ShellSearch::RunSearch (SearchQuery::MakeId (L"C:\\Windows", L"setup", options), found);

        IShellItemVerbs::s_listingProgress = nullptr;
        IShellItemVerbs::s_listingStopped  = nullptr;

        Logger::WriteMessage (std::format ("Contents search: first results at {:.0f} ms", firstMs).c_str());
        Assert::IsTrue (firstMs >= 0 && firstMs <= kMaxFirstMs, L"the first results within 2 seconds");
    }

private:

    Items Search (const wchar_t * query, const SearchQuery::Options & options)
    {
        Items    items;
        HRESULT  hr = Win32ShellSearch::RunSearch (SearchQuery::MakeId (m_dir.wstring(), query, options), items);

        Assert::IsTrue (SUCCEEDED (hr));

        return items;
    }

    static bool Has (const Items & items, const wchar_t * name)
    {
        return std::any_of (items.begin(), items.end(), [name] (const IShellItemVerbs::ShellFolderItem & item) { return _wcsicmp (item.name.c_str(), name) == 0; });
    }

    static void WriteFile (const std::filesystem::path & path, const std::string & bytes)
    {
        std::ofstream  file (path, std::ios::binary);

        file.write (bytes.data(), (std::streamsize) bytes.size());
    }

    static uint32_t Crc32 (const std::string & bytes)
    {
        uint32_t  crc = 0xFFFFFFFFu;

        for (char c : bytes)
        {
            crc ^= (uint8_t) c;

            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
            }
        }

        return ~crc;
    }

    //  A zip of one stored file, by its format, so the test needs no tool.
    static void WriteStoredZip (const std::filesystem::path & path, const std::string & name, const std::string & content)
    {
        std::string  out;
        uint32_t     crc   = Crc32 (content);
        uint32_t     size  = (uint32_t) content.size();
        auto         put16 = [&out] (uint16_t v) { out.push_back ((char) v); out.push_back ((char) (v >> 8)); };
        auto         put32 = [&put16] (uint32_t v) { put16 ((uint16_t) v); put16 ((uint16_t) (v >> 16)); };
        auto         shared = [&]()
        {
            put16 (20); put16 (0); put16 (0); put16 (0); put16 (0x5021);
            put32 (crc); put32 (size); put32 (size);
            put16 ((uint16_t) name.size()); put16 (0);
        };
        uint32_t     central = 0;

        put32 (0x04034B50u);
        shared();
        out += name;
        out += content;

        central = (uint32_t) out.size();

        put32 (0x02014B50u);
        put16 (20);
        shared();
        put16 (0); put16 (0); put16 (0); put32 (0); put32 (0);
        out += name;

        uint32_t  centralSize = (uint32_t) out.size() - central;

        put32 (0x06054B50u);
        put16 (0); put16 (0); put16 (1); put16 (1);
        put32 (centralSize); put32 (central);
        put16 (0);

        WriteFile (path, out);
    }

    std::filesystem::path  m_dir;
};
