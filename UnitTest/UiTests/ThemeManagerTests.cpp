#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Ui/ThemeManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeManagerTests
//
//  Exercises the ThemeManager + ThemeBootstrapPlanner without an
//  Active RmlUi context. Per the deferred timing note,
//  hot-swap latency is validated against a live context in P9.
//
//  All file I/O routes through `InMemoryFileSystem`. A null
//  Rml::Context is intentional: Activate() succeeds on the data
//  side and skips document loading (BindRml would attach docs once
//  a real context exists in the running shell — covered by P9
//  acceptance).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const wchar_t * kThemesBase = L"C:\\Casso\\Themes";

    constexpr const char * kSkeuoJson = R"({
        "$cassoThemeVersion": 1,
        "$cassoBuiltIn":      true,
        "name":               "Skeuomorphic",
        "familyId":           "apple2",
        "variantId":          "ii",
        "uiTokens":           {},
        "driveVisualProfile": { "style":"disk2", "colorway":"beige", "doorAnimation":"x", "syncChannel":"drive-door" },
        "author":             "Casso",
        "useMicaBackdrop":    false
    })";

    constexpr const char * kDarkJson = R"({
        "$cassoThemeVersion": 1,
        "$cassoBuiltIn":      true,
        "name":               "DarkModern",
        "familyId":           "apple2",
        "variantId":          "iie",
        "uiTokens":           {},
        "driveVisualProfile": { "style":"disk2", "colorway":"graphite", "doorAnimation":"x", "syncChannel":"drive-door" },
        "author":             "Casso",
        "useMicaBackdrop":    true
    })";

    constexpr const char * kBadJson  = R"({ "name": "no version", "familyId":"apple2", "variantId":"ii", "uiTokens":{}, "driveVisualProfile":{"style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"} })";


    // Write a complete minimal theme that ThemeLoader will accept
    // (theme.json + every entryDocument it references).
    void WriteValidTheme (
        InMemoryFileSystem & fs,
        const std::wstring & dir,
        const char         * themeJson)
    {
        fs.WriteAllText (dir + L"\\theme.json", themeJson);
        // theme.json doesn't declare entryDocuments → fallback to
        // shared. Make sure the shared fallbacks exist.
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\_shared\\title_bar.rml",     "<rml/>");
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\_shared\\nav_layer.rml",     "<rml/>");
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\_shared\\settings.rml",      "<rml/>");
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\_shared\\drive_widgets.rml", "<rml/>");
    }
}





TEST_CLASS (ThemeManagerTests)
{
public:

    TEST_METHOD (Discover_IncludesValidExcludesInvalid)
    {
        InMemoryFileSystem  fs;
        ThemeManager        mgr (fs, kThemesBase);
        HRESULT             hr;

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",   kDarkJson);

        // Three flavors of malformed:
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\NoVersion\\theme.json", kBadJson);
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\Garbage\\theme.json",   "{ not json");
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\Future\\theme.json",
                         R"({"$cassoThemeVersion": 99, "name": "x"})");

        hr = mgr.Discover();

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (2), mgr.GetAvailableThemes().size());
    }


    TEST_METHOD (Discover_MissingBaseDir_EmptyList)
    {
        InMemoryFileSystem  fs;
        ThemeManager        mgr (fs, L"C:\\Does\\Not\\Exist");
        HRESULT             hr;

        hr = mgr.Discover();

        // EnumerateDirectories returns S_FALSE through Discover().
        // Either S_OK or S_FALSE is acceptable; what matters is that
        // the list is empty and we did not crash.
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (0), mgr.GetAvailableThemes().size());
    }


    TEST_METHOD (Activate_UnknownTheme_Returns_S_FALSE)
    {
        InMemoryFileSystem  fs;
        ThemeManager        mgr (fs, kThemesBase);

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        mgr.Discover();

        HRESULT hr = mgr.Activate ("NoSuchTheme");

        Assert::IsTrue (hr == S_FALSE);
        Assert::IsTrue (mgr.GetActiveThemeName().empty());
    }


    TEST_METHOD (Activate_KnownTheme_FiresObserverAndUpdatesActive)
    {
        InMemoryFileSystem  fs;
        ThemeManager        mgr (fs, kThemesBase);
        std::string         observed;
        int                 hits = 0;

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",   kDarkJson);
        mgr.Discover();

        mgr.AddChangeListener ([&] (const LoadedTheme & t)
        {
            observed = t.name;
            ++hits;
        });

        HRESULT hr = mgr.Activate ("DarkModern");

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("DarkModern"), mgr.GetActiveThemeName());
        Assert::AreEqual (string ("DarkModern"), observed);
        Assert::AreEqual (1, hits);
        Assert::AreEqual (string ("apple2"), mgr.GetActiveFamilyId());
        Assert::AreEqual (string ("iie"), mgr.GetActiveVariantId());
    }


    TEST_METHOD (ActivateByFamilyVariant_FindsAndActivatesTheme)
    {
        InMemoryFileSystem  fs;
        ThemeManager        mgr (fs, kThemesBase);

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",   kDarkJson);
        mgr.Discover();

        HRESULT hr = mgr.ActivateByFamilyVariant ("apple2", "ii");

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("Skeuomorphic"), mgr.GetActiveThemeName());
    }


    TEST_METHOD (BootstrapPlanner_MissingDisk_InstallsBuiltIn)
    {
        Assert::IsTrue (ThemeBootstrapPlanner::Plan (nullptr, 1)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }


    TEST_METHOD (BootstrapPlanner_UserThemeNeverOverwritten)
    {
        // No $cassoBuiltIn marker -> user theme -> skip even if
        // version is older than the embedded built-in's.
        std::string userTheme = R"({"$cassoThemeVersion": 1, "name": "MyCustom"})";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&userTheme, 99)
                        == ThemeBootstrapAction::Skip);
    }


    TEST_METHOD (BootstrapPlanner_BuiltInUpgrade_TriggersInstall)
    {
        std::string oldBuiltIn =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        // Embedded built-in is now v3 -> upgrade required.
        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&oldBuiltIn, 3)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }


    TEST_METHOD (BootstrapPlanner_BuiltInCurrent_Skips)
    {
        std::string currentBuiltIn =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&currentBuiltIn, 1)
                        == ThemeBootstrapAction::Skip);
    }


    TEST_METHOD (BootstrapPlanner_GarbageDisk_InstallsBuiltIn)
    {
        // A half-written extract from a previous crash. Replace it.
        std::string garbage = "{ not json at all";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&garbage, 1)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }
};
