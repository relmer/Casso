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


    TEST_METHOD (VariantOverrides_Parsed_AndStoredKeyedByDisplayName)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\Variants";
        std::string         kJson;

        kJson = R"({
            "$cassoThemeVersion": 1,
            "name": "T", "familyId": "apple2", "variantId": "ii",
            "uiTokens": {},
            "driveVisualProfile": {
                "style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"
            },
            "variantOverrides": {
                "Apple //e": { "crtDefaults": { "brightness": 1.25 } },
                "Apple //c": {
                    "driveVisualProfile": { "style": "integrated", "colorway": "platinum" }
                }
            }
        })";

        fs.WriteAllText (dir + L"\\theme.json", kJson);
        hr = ThemeLoader::Load (fs, dir, theme, err);

        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 2, theme.variantOverrides.size(),
            L"Two variant overrides expected (Apple //e, Apple //c)");

        // Order-preserving vector of pairs; spec doesn't promise ordering but
        // current parser keeps insertion order from the JSON.
        Assert::IsTrue (theme.variantOverrides[0].first == "Apple //e" ||
                        theme.variantOverrides[1].first == "Apple //e");
        Assert::IsTrue (theme.variantOverrides[0].first == "Apple //c" ||
                        theme.variantOverrides[1].first == "Apple //c");
    }


    TEST_METHOD (ResolveForMachine_NoOverrides_ReturnsBaseUnchanged)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        LoadedTheme         resolved;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\Plain";

        WriteHappyTheme (fs, dir);
        hr = ThemeLoader::Load (fs, dir, theme, err);
        Assert::IsTrue (SUCCEEDED (hr));

        // Brightness from kHappyJson defaults; no overrides exist.
        resolved = theme.ResolveForMachine ("Apple //e");

        Assert::AreEqual (theme.crtDefaults.brightness,  resolved.crtDefaults.brightness);
        Assert::AreEqual (theme.driveVisualProfile.style, resolved.driveVisualProfile.style);
    }


    TEST_METHOD (ResolveForMachine_UnknownMachine_FallsThroughToBase)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\Variants2";
        std::string         kJson;

        kJson = R"({
            "$cassoThemeVersion": 1,
            "name": "T", "familyId": "apple2", "variantId": "ii",
            "uiTokens": {},
            "driveVisualProfile": {
                "style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"
            },
            "variantOverrides": {
                "Apple //e": { "crtDefaults": { "brightness": 1.5 } }
            }
        })";

        fs.WriteAllText (dir + L"\\theme.json", kJson);
        hr = ThemeLoader::Load (fs, dir, theme, err);
        Assert::IsTrue (SUCCEEDED (hr));

        {
            LoadedTheme  resolved = theme.ResolveForMachine ("Commodore 64");
            Assert::AreEqual (theme.crtDefaults.brightness, resolved.crtDefaults.brightness,
                L"Unknown machine must inherit base verbatim");
        }
    }


    TEST_METHOD (ResolveForMachine_MatchingMachine_AppliesOverrideValues)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\Variants3";
        std::string         kJson;

        kJson = R"({
            "$cassoThemeVersion": 1,
            "name": "T", "familyId": "apple2", "variantId": "ii",
            "uiTokens": {},
            "driveVisualProfile": {
                "style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"
            },
            "crtDefaults": {
                "brightness": 1.0,
                "scanlines": { "enabled": false, "intensity": 0.0 }
            },
            "variantOverrides": {
                "Apple //c": {
                    "crtDefaults": {
                        "brightness": 1.3,
                        "scanlines": { "enabled": true, "intensity": 0.6 }
                    },
                    "driveVisualProfile": {
                        "style": "integrated",
                        "colorway": "platinum"
                    },
                    "useMicaBackdrop": true
                }
            }
        })";

        fs.WriteAllText (dir + L"\\theme.json", kJson);
        hr = ThemeLoader::Load (fs, dir, theme, err);
        Assert::IsTrue (SUCCEEDED (hr));

        {
            LoadedTheme  resolved = theme.ResolveForMachine ("Apple //c");

            Assert::AreEqual (1.3f,  resolved.crtDefaults.brightness,         L"brightness overridden");
            Assert::IsTrue   (resolved.crtDefaults.scanlinesEnabled,          L"scanlines toggled on");
            Assert::AreEqual (0.6f,  resolved.crtDefaults.scanlinesIntensity, L"scanline intensity overridden");
            Assert::AreEqual (string ("integrated"), resolved.driveVisualProfile.style);
            Assert::AreEqual (string ("platinum"),   resolved.driveVisualProfile.colorway);
            Assert::IsTrue   (resolved.useMicaBackdrop);

            // Base value still wins for fields the override didn't touch.
            Assert::AreEqual (string ("x"), resolved.driveVisualProfile.doorAnimation);
        }
    }


    TEST_METHOD (ResolveForMachine_DoesNotMutateBaseTheme)
    {
        InMemoryFileSystem  fs;
        LoadedTheme         theme;
        ThemeLoadError      err;
        HRESULT             hr;
        std::wstring        dir = std::wstring (kThemesBase) + L"\\Variants4";
        std::string         kJson;

        kJson = R"({
            "$cassoThemeVersion": 1,
            "name": "T", "familyId": "apple2", "variantId": "ii",
            "uiTokens": {},
            "driveVisualProfile": {
                "style":"disk2","colorway":"beige","doorAnimation":"x","syncChannel":"drive-door"
            },
            "variantOverrides": {
                "Apple //c": { "crtDefaults": { "brightness": 2.0 } }
            }
        })";

        fs.WriteAllText (dir + L"\\theme.json", kJson);
        hr = ThemeLoader::Load (fs, dir, theme, err);
        Assert::IsTrue (SUCCEEDED (hr));

        float  baseBefore = theme.crtDefaults.brightness;
        {
            LoadedTheme  resolved = theme.ResolveForMachine ("Apple //c");
            Assert::AreEqual (2.0f, resolved.crtDefaults.brightness);
        }

        Assert::AreEqual (baseBefore, theme.crtDefaults.brightness,
            L"ResolveForMachine must return a copy; base theme must remain untouched");
    }
};
