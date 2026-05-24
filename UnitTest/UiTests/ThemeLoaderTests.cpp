#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Ui/ThemeLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoaderTests
//
//  Pure-logic tests for parsing and validating `theme.json`. No painter,
//  no Win32 file I/O — every fixture lives in `InMemoryFileSystem`.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const wchar_t * kThemesBase = L"C:\\Casso\\Themes";

    constexpr const char * kHappyJson = R"({
        "$cassoThemeVersion": 1,
        "name": "Skeuomorphic",
        "familyId": "apple2",
        "variantId": "ii",
        "author": "Casso",
        "description": "test",
        "uiTokens": { "chrome": { "titleTextColor": "#fff" } },
        "driveVisualProfile": {
            "style": "disk2",
            "colorway": "beige",
            "doorAnimation": "mechanicalSwing",
            "syncChannel": "drive-door"
        },
        "useMicaBackdrop": false
    })";


    void WriteHappyTheme (InMemoryFileSystem & fs, const std::wstring & dir)
    {
        fs.WriteAllText (dir + L"\\theme.json", kHappyJson);
    }
}





TEST_CLASS (ThemeLoaderTests)
{
public:

    TEST_METHOD (HappyPath_LoadsMetadata)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\Skeuomorphic";
        WriteHappyTheme (fs, dir);

        hr = ThemeLoader::Load (fs, dir, theme, err);

        Assert::IsTrue (SUCCEEDED (hr), L"Load failed");
        Assert::AreEqual (string ("Skeuomorphic"), theme.name);
        Assert::AreEqual (string ("apple2"), theme.familyId);
        Assert::AreEqual (string ("ii"), theme.variantId);
        Assert::AreEqual (1, theme.version);
        Assert::IsFalse (theme.isBuiltIn);
        Assert::IsFalse (theme.useMicaBackdrop);
    }


    TEST_METHOD (Missing_VersionKey_Rejected)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        std::wstring  dir = std::wstring (kThemesBase) + L"\\NoVersion";
        fs.WriteAllText (dir + L"\\theme.json", R"({ "name": "x", "familyId": "apple2", "variantId": "ii", "uiTokens": {}, "driveVisualProfile": {"style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"} })");

        hr = ThemeLoader::Load (fs, dir, theme, err);

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
                         R"({"$cassoThemeVersion": 999, "name": "x", "familyId": "apple2", "variantId": "ii", "uiTokens": {}, "driveVisualProfile": {"style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"}})");

        hr = ThemeLoader::Load (fs, dir, theme, err);

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

        hr = ThemeLoader::Load (fs, dir, theme, err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::MetadataInvalid);
        Assert::IsFalse (err.message.empty());
    }


    TEST_METHOD (MissingThemeJson_ReportsMetadataMissing)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;

        hr = ThemeLoader::Load (fs,
                                std::wstring (kThemesBase) + L"\\Empty",
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

        hr = ThemeLoader::EnumerateCandidateDirs (fs, kThemesBase, found);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (2), found.size());
        // InMemoryFileSystem normalizes keys to lowercase + forward
        // slashes; the enumerator returns whatever normalized
        // segment it derived. Order is alphabetic per the set.
        Assert::AreEqual (std::wstring (L"a"), found[0]);
        Assert::AreEqual (std::wstring (L"c"), found[1]);
    }


    TEST_METHOD (MissingFamilyOrVariant_Rejected)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\BrokenIds";

        fs.WriteAllText (dir + L"\\theme.json",
                         R"({"$cassoThemeVersion":1,"name":"Broken","familyId":"apple2","uiTokens":{},"driveVisualProfile":{"style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"}})");

        hr = ThemeLoader::Load (fs, dir, theme, err);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (err.code == ThemeLoadResult::MetadataInvalid);
    }


    TEST_METHOD (NonAppleFamily_IsAccepted)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\Commodore";

        fs.WriteAllText (dir + L"\\theme.json",
                         R"({"$cassoThemeVersion":1,"name":"Commodore","familyId":"commodore64","variantId":"c64c","uiTokens":{},"driveVisualProfile":{"style":"disk2","colorway":"ivory","doorAnimation":"x","syncChannel":"drive-door"}})");

        hr = ThemeLoader::Load (fs, dir, theme, err);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("commodore64"), theme.familyId);
        Assert::AreEqual (string ("c64c"), theme.variantId);
    }
};
