#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Ui/ThemeLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoaderTests
//
//  Pure-logic tests for parsing and validating `theme.json`. No RmlUi,
//  no Win32 file I/O — every fixture lives in `InMemoryFileSystem`.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const wchar_t * kThemesBase = L"C:\\Casso\\Themes";

    constexpr const char * kHappyJson = R"({
        "$cassoThemeVersion": 1,
        "name": "Skeuomorphic",
        "author": "Casso",
        "description": "test",
        "entryDocuments": {
            "titleBar":     "title_bar.rml",
            "navLayer":     "nav_layer.rml",
            "settings":     "settings.rml",
            "driveWidgets": "drive_widgets.rml"
        },
        "useMicaBackdrop": false
    })";


    void WriteHappyTheme (InMemoryFileSystem & fs, const std::wstring & dir)
    {
        fs.WriteAllText (dir + L"\\theme.json",        kHappyJson);
        fs.WriteAllText (dir + L"\\title_bar.rml",     "<rml/>");
        fs.WriteAllText (dir + L"\\nav_layer.rml",     "<rml/>");
        fs.WriteAllText (dir + L"\\settings.rml",      "<rml/>");
        fs.WriteAllText (dir + L"\\drive_widgets.rml", "<rml/>");
    }
}





TEST_CLASS (ThemeLoaderTests)
{
public:

    TEST_METHOD (HappyPath_LoadsAndResolvesEntryDocs)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\Skeuomorphic";
        WriteHappyTheme (fs, dir);

        hr = ThemeLoader::Load (fs, dir, L"", theme, err);

        Assert::IsTrue (SUCCEEDED (hr), L"Load failed");
        Assert::AreEqual (string ("Skeuomorphic"), theme.name);
        Assert::AreEqual (1, theme.version);
        Assert::IsFalse (theme.isBuiltIn);
        Assert::IsFalse (theme.useMicaBackdrop);
        // Entry docs resolved to absolute paths
        Assert::IsTrue (theme.entryDocs.titleBar.find (L"title_bar.rml") != std::wstring::npos);
        Assert::IsTrue (theme.entryDocs.navLayer.find (L"nav_layer.rml") != std::wstring::npos);
    }


    TEST_METHOD (Missing_VersionKey_Rejected)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\NoVersion";
        fs.WriteAllText (dir + L"\\theme.json", R"({ "name": "x" })");

        hr = ThemeLoader::Load (fs, dir, L"", theme, err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::MetadataInvalid);
    }


    TEST_METHOD (FutureVersion_Rejected)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\Future";
        fs.WriteAllText (dir + L"\\theme.json",
                         R"({"$cassoThemeVersion": 999, "name": "x"})");

        hr = ThemeLoader::Load (fs, dir, L"", theme, err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::VersionTooNew);
    }


    TEST_METHOD (MalformedJson_ProducesStructuredError)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\Bad";
        fs.WriteAllText (dir + L"\\theme.json", "{ this is not json");

        hr = ThemeLoader::Load (fs, dir, L"", theme, err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::MetadataInvalid);
        Assert::IsFalse (err.message.empty());
    }


    TEST_METHOD (MissingEntryDocument_Rejected)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\BrokenLinks";
        fs.WriteAllText (dir + L"\\theme.json", kHappyJson);
        // ... but no .rml files exist on disk.

        hr = ThemeLoader::Load (fs, dir, L"", theme, err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::DocumentMissing);
        Assert::IsFalse (err.offendingPath.empty());
    }


    TEST_METHOD (EntryDocumentsOmitted_FallsBackToShared)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir       = std::wstring (kThemesBase) + L"\\Minimal";
        std::wstring        sharedDir = std::wstring (kThemesBase) + L"\\_shared";

        fs.WriteAllText (dir + L"\\theme.json",
                         R"({"$cassoThemeVersion": 1, "name": "Minimal"})");
        fs.WriteAllText (sharedDir + L"\\title_bar.rml",     "<rml/>");
        fs.WriteAllText (sharedDir + L"\\nav_layer.rml",     "<rml/>");
        fs.WriteAllText (sharedDir + L"\\settings.rml",      "<rml/>");
        fs.WriteAllText (sharedDir + L"\\drive_widgets.rml", "<rml/>");

        hr = ThemeLoader::Load (fs, dir, sharedDir, theme, err);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (theme.entryDocs.titleBar.find (L"_shared") != std::wstring::npos);
    }


    TEST_METHOD (MissingThemeJson_ReportsMetadataMissing)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        hr = ThemeLoader::Load (fs,
                                std::wstring (kThemesBase) + L"\\Empty",
                                L"",
                                theme,
                                err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::MetadataMissing);
    }


    TEST_METHOD (EnumerateCandidateDirs_SkipsDirsWithoutThemeJson)
    {
        InMemoryFileSystem         fs;
        std::vector<std::wstring>  found;
        HRESULT                    hr;

        fs.WriteAllText (std::wstring (kThemesBase) + L"\\A\\theme.json", kHappyJson);
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\B\\something.txt", "");
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\C\\theme.json", kHappyJson);
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\_shared\\title_bar.rml", "");

        hr = ThemeLoader::EnumerateCandidateDirs (fs, kThemesBase, found);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (2), found.size());
        // InMemoryFileSystem normalizes keys to lowercase + forward
        // slashes; the enumerator returns whatever normalized
        // segment it derived. Order is alphabetic per the set.
        Assert::AreEqual (std::wstring (L"a"), found[0]);
        Assert::AreEqual (std::wstring (L"c"), found[1]);
    }
};
