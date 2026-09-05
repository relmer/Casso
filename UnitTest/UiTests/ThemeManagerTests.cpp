#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Ui/ThemeManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeManagerTests
//
//  Exercises the ThemeManager + ThemeBootstrapPlanner without a
//  painter context. Per the deferred timing note,
//  hot-swap latency is validated against a live context in a later
//  phase.
//
//  All file I/O routes through `InMemoryFileSystem`. Document
//  rendering is not part of this surface; Activate() succeeds on the
//  data side and a later phase attaches docs once a real painter
//  exists in the running shell.
//
////////////////////////////////////////////////////////////////////////////////






TEST_CLASS (ThemeManagerTests)
{
public:

    static constexpr const wchar_t * kThemesBase = L"C:\\Casso\\Themes";

    static constexpr const char * kSkeuoJson = R"({
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

    static constexpr const char * kDarkJson = R"({
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

    static constexpr const char * kRetroJson = R"({
        "$cassoThemeVersion": 1,
        "$cassoBuiltIn":      true,
        "name":               "RetroTerminal",
        "familyId":           "apple2",
        "variantId":          "iic",
        "uiTokens":           {},
        "driveVisualProfile": { "style":"disk2", "colorway":"platinum", "doorAnimation":"x", "syncChannel":"drive-door" },
        "author":             "Casso",
        "useMicaBackdrop":    false
    })";

    static constexpr const char * kBadJson  = R"({ "name": "no version", "familyId":"apple2", "variantId":"ii", "uiTokens":{}, "driveVisualProfile":{"style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"} })";


    // Write a complete minimal theme that ThemeLoader will accept
    // (theme.json + every entryDocument it references).
    void WriteValidTheme (
        InMemoryFileSystem & fs,
        const std::wstring & dir,
        const char         * themeJson)
    {
        fs.WriteAllText (dir + L"\\theme.json", themeJson);
    }

    TEST_METHOD (Discover_IncludesValidExcludesInvalid)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr;
        ThemeManager        mgr (fs, kThemesBase);

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",   kDarkJson);

        // Three flavors of malformed:
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\NoVersion\\theme.json", kBadJson);
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\Garbage\\theme.json",   "{ not json");
        fs.WriteAllText (std::wstring (kThemesBase) + L"\\Future\\theme.json",
                         R"({"$cassoThemeVersion": 99, "name": "x"})");

        hr = mgr.Discover();

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (2), mgr.GetAvailableThemes().size());
    }


    TEST_METHOD (Discover_OrdersBuiltInsForPresentation)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr;
        ThemeManager        mgr (fs, kThemesBase);

        // Written in an order the dropdown must NOT show, and stored under
        // directory names that sort the wrong way alphabetically too.
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",    kDarkJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\RetroTerminal", kRetroJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic",  kSkeuoJson);

        hr = mgr.Discover();

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (3), mgr.GetAvailableThemes().size());
        Assert::AreEqual (string ("Skeuomorphic"),  mgr.GetAvailableThemes()[0].name);
        Assert::AreEqual (string ("RetroTerminal"), mgr.GetAvailableThemes()[1].name);
        Assert::AreEqual (string ("DarkModern"),    mgr.GetAvailableThemes()[2].name);
    }


    TEST_METHOD (Discover_UserThemeSortsAfterTheBuiltIns)
    {
        InMemoryFileSystem  fs;
        std::string         userJson = R"({
            "$cassoThemeVersion": 1,
            "name":               "AardvarkCustom",
            "familyId":           "apple2",
            "variantId":          "ii",
            "uiTokens":           {},
            "driveVisualProfile": { "style":"disk2", "colorway":"beige", "doorAnimation":"x", "syncChannel":"drive-door" },
            "author":             "Someone"
        })";
        ThemeManager        mgr (fs, kThemesBase);

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\AardvarkCustom", userJson.c_str());
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",     kDarkJson);

        AssertSucceeded (mgr.Discover());

        Assert::AreEqual (size_t (2), mgr.GetAvailableThemes().size());
        Assert::AreEqual (string ("DarkModern"),     mgr.GetAvailableThemes()[0].name,
            L"A named built-in outranks a user theme however the directory sorts");
        Assert::AreEqual (string ("AardvarkCustom"), mgr.GetAvailableThemes()[1].name);
    }


    TEST_METHOD (Discover_MissingBaseDir_EmptyList)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr;
        ThemeManager        mgr (fs, L"C:\\Does\\Not\\Exist");

        hr = mgr.Discover();

        // EnumerateDirectories returns S_FALSE through Discover().
        // Either S_OK or S_FALSE is acceptable; what matters is that
        // the list is empty and we did not crash.
        AssertSucceeded (hr);
        Assert::AreEqual (size_t (0), mgr.GetAvailableThemes().size());
    }


    TEST_METHOD (Activate_UnknownTheme_Returns_S_FALSE)
    {
        InMemoryFileSystem  fs;
        ThemeManager        mgr (fs, kThemesBase);

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        mgr.Discover();

        HRESULT hr = mgr.Activate ("NoSuchTheme");

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr,
            L"An unknown theme name must fail, and the code must give the reason");
        Assert::IsTrue (mgr.GetActiveThemeName().empty());
    }


    TEST_METHOD (Activate_KnownTheme_FiresObserverAndUpdatesActive)
    {
        InMemoryFileSystem  fs;
        std::string         observed;
        int                 hits = 0;
        ThemeManager        mgr (fs, kThemesBase);

        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\Skeuomorphic", kSkeuoJson);
        WriteValidTheme (fs, std::wstring (kThemesBase) + L"\\DarkModern",   kDarkJson);
        mgr.Discover();

        mgr.AddChangeListener ([&] (const LoadedTheme & t)
        {
            observed = t.name;
            ++hits;
        });

        HRESULT hr = mgr.Activate ("DarkModern");

        AssertSucceeded (hr);
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

        AssertSucceeded (hr);
        Assert::AreEqual (string ("Skeuomorphic"), mgr.GetActiveThemeName());
    }


    TEST_METHOD (BootstrapPlanner_MissingDisk_InstallsBuiltIn)
    {
        std::string  embedded =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (nullptr, embedded, 1)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }


    TEST_METHOD (BootstrapPlanner_UserThemeNeverOverwritten)
    {
        // No $cassoBuiltIn marker -> user theme -> skip even if
        // version is older than the embedded built-in's.
        std::string userTheme = R"({"$cassoThemeVersion": 1, "name": "MyCustom"})";
        std::string embedded  = R"({"$cassoThemeVersion": 99, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&userTheme, embedded, 99)
                        == ThemeBootstrapAction::Skip);
    }


    TEST_METHOD (BootstrapPlanner_BuiltInUpgrade_TriggersInstall)
    {
        std::string oldBuiltIn =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";
        std::string embeddedV3 =
            R"({"$cassoThemeVersion": 3, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        // Embedded built-in is now v3 -> upgrade required (bytes differ too).
        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&oldBuiltIn, embeddedV3, 3)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }


    TEST_METHOD (BootstrapPlanner_BuiltInCurrent_Skips)
    {
        std::string currentBuiltIn =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        // Bytes match the embedded canonical -> skip.
        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&currentBuiltIn, currentBuiltIn, 1)
                        == ThemeBootstrapAction::Skip);
    }


    TEST_METHOD (BootstrapPlanner_BuiltInBytesDiffer_TriggersInstall)
    {
        // Same version, but the embedded canonical bytes were edited
        // (developer tweaked a theme value without bumping the version).
        // Content drift must re-extract.
        std::string onDisk =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic", "crtDefaults": {"brightness": 1.0}})";
        std::string embedded =
            R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic", "crtDefaults": {"brightness": 1.1}})";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&onDisk, embedded, 1)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }


    TEST_METHOD (BootstrapPlanner_GarbageDisk_InstallsBuiltIn)
    {
        // A half-written extract from a previous crash. Replace it.
        std::string garbage  = "{ not json at all";
        std::string embedded = R"({"$cassoThemeVersion": 1, "$cassoBuiltIn": true, "name": "Skeuomorphic"})";

        Assert::IsTrue (ThemeBootstrapPlanner::Plan (&garbage, embedded, 1)
                        == ThemeBootstrapAction::InstallBuiltIn);
    }
};

