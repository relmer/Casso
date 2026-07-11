#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"
#include "Ui/ColorUtil.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefsTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GlobalUserPrefsTests)
{
public:

    TEST_METHOD (DefaultConstruction_HasExpectedValues)
    {
        GlobalUserPrefs  prefs;

        Assert::AreEqual (1, prefs.version);
        Assert::AreEqual (string ("Skeuomorphic"), prefs.activeTheme);
        Assert::AreEqual (true,  prefs.activeTheme.size() > 0);
        Assert::AreEqual (false, prefs.crtByMode[0].scanlinesEnabled);
        Assert::AreEqual (size_t (0), prefs.window.placements.size());
    }


    TEST_METHOD (Load_MissingFile_Returns_S_FALSE_And_KeepsDefaults)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr;

        hr = prefs.Load (L"C:\\Casso", fs);
        Assert::IsTrue (hr == S_FALSE);
        Assert::AreEqual (string ("Skeuomorphic"), prefs.activeTheme);
    }


    TEST_METHOD (RoundTrip_FullPrefs)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     orig;
        GlobalUserPrefs     loaded;
        HRESULT             hr;

        orig.activeTheme            = "Retro Terminal";
        orig.lastSelectedMachine    = "Apple2e";
        orig.arrowsToJoystick       = true;
        orig.pointerMapping         = InputMappingMode::Mouse;
        orig.crtByMode[0].brightness         = 1.25f;
        orig.crtByMode[0].contrast           = 1.35f;
        orig.crtByMode[0].scanlinesEnabled   = true;
        orig.crtByMode[0].scanlinesIntensity = 0.75f;
        orig.crtByMode[0].bloomEnabled       = true;
        orig.crtByMode[0].bloomRadius        = 2.0f;
        orig.crtByMode[0].bloomStrength      = 0.6f;
        orig.crtByMode[0].colorBleedEnabled  = true;
        orig.crtByMode[0].colorBleedWidth    = 1.5f;
        orig.window.placements["topology-A"] = { 100, 50, 1280, 720 };
        orig.window.placements["topology-B"] = { 200, 75, 1920, 1080 };
        orig.window.fullscreen      = true;

        hr = orig.Save (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = loaded.Load (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (orig.activeTheme,         loaded.activeTheme);
        Assert::AreEqual (orig.lastSelectedMachine, loaded.lastSelectedMachine);
        Assert::AreEqual (orig.arrowsToJoystick, loaded.arrowsToJoystick);
        Assert::IsTrue   (orig.pointerMapping == loaded.pointerMapping,
                          L"split pointer mapping round-trips");
        Assert::AreEqual (orig.crtByMode[0].brightness,      loaded.crtByMode[0].brightness);
        Assert::AreEqual (orig.crtByMode[0].contrast,        loaded.crtByMode[0].contrast);
        Assert::AreEqual (orig.crtByMode[0].scanlinesEnabled,    loaded.crtByMode[0].scanlinesEnabled);
        Assert::AreEqual (orig.crtByMode[0].scanlinesIntensity,  loaded.crtByMode[0].scanlinesIntensity);
        Assert::AreEqual (orig.crtByMode[0].bloomEnabled,        loaded.crtByMode[0].bloomEnabled);
        Assert::AreEqual (orig.crtByMode[0].bloomRadius,         loaded.crtByMode[0].bloomRadius);
        Assert::AreEqual (orig.crtByMode[0].bloomStrength,       loaded.crtByMode[0].bloomStrength);
        Assert::AreEqual (orig.crtByMode[0].colorBleedEnabled,   loaded.crtByMode[0].colorBleedEnabled);
        Assert::AreEqual (orig.crtByMode[0].colorBleedWidth,     loaded.crtByMode[0].colorBleedWidth);
        Assert::AreEqual (size_t (2),                            loaded.window.placements.size());
        Assert::AreEqual (100, loaded.window.placements["topology-A"].x);
        Assert::AreEqual (720, loaded.window.placements["topology-A"].h);
        Assert::AreEqual (1920, loaded.window.placements["topology-B"].w);
        Assert::AreEqual (orig.window.fullscreen,       loaded.window.fullscreen);
    }


    TEST_METHOD (ResetColorMonitorText_RevertsModeToWhite_KeepsCustomArgb)
    {
        // Regression (013 #8): "Restore defaults" left a previously-picked
        // custom text colour active -- the control read White but the
        // emulator kept the old colour. The reset must revert the mode to
        // White (so the resolved colour is white) while remembering the
        // custom ARGB for the next time the user re-selects "Custom".
        GlobalUserPrefs  prefs;
        prefs.colorMonitorTextMode       = ColorMonitorTextMode::Custom;
        prefs.colorMonitorTextCustomArgb = 0xFFFF00FFu;   // magenta

        prefs.ResetColorMonitorTextToDefault();

        Assert::IsTrue   (ColorMonitorTextMode::White == prefs.colorMonitorTextMode);
        Assert::AreEqual (0xFFFF00FFu, prefs.colorMonitorTextCustomArgb);   // remembered
        Assert::AreEqual (ColorUtil::kWhiteArgb,
                          ColorUtil::ResolveColorMonitorTextArgb (prefs.colorMonitorTextMode,
                                                                  prefs.colorMonitorTextCustomArgb));
    }


    TEST_METHOD (ColorMonitorText_PersistsAcrossSaveLoad_AndRestoreClearsIt)
    {
        // The reported bug booted magenta because a custom text colour
        // round-tripped through UserPrefs.json and survived a Restore. Pin
        // both halves: (1) a custom colour persists, and (2) after a reset +
        // save the next load is White (not the stale custom colour).
        InMemoryFileSystem  fs;
        GlobalUserPrefs     orig;

        orig.colorMonitorTextMode       = ColorMonitorTextMode::Custom;
        orig.colorMonitorTextCustomArgb = 0xFF3399CCu;   // alpha forced FF on save

        Assert::IsTrue (SUCCEEDED (orig.Save (L"C:\\Casso", fs)));

        GlobalUserPrefs  loaded;
        Assert::IsTrue (SUCCEEDED (loaded.Load (L"C:\\Casso", fs)));
        Assert::IsTrue   (ColorMonitorTextMode::Custom == loaded.colorMonitorTextMode);
        Assert::AreEqual (0xFF3399CCu, loaded.colorMonitorTextCustomArgb);

        loaded.ResetColorMonitorTextToDefault();
        Assert::IsTrue (SUCCEEDED (loaded.Save (L"C:\\Casso", fs)));

        GlobalUserPrefs  reloaded;
        Assert::IsTrue (SUCCEEDED (reloaded.Load (L"C:\\Casso", fs)));
        Assert::IsTrue (ColorMonitorTextMode::White == reloaded.colorMonitorTextMode);
    }


    TEST_METHOD (Load_MissingFields_Tolerated)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr;

        hr = fs.WriteAllText (GlobalUserPrefs::FilePath (L"C:\\Casso"),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\"}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.Load (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("DarkModern"), prefs.activeTheme);
        // crt sub-object missing → struct defaults preserved.
        Assert::AreEqual (1.0f, prefs.crtByMode[0].brightness);
        Assert::AreEqual (1.0f, prefs.crtByMode[0].contrast);
    }


    // FR-013a migration: configs with only the legacy single mode split
    // correctly -- joystick -> Keys on; paddle/mouse -> Pointer; new keys win.
    TEST_METHOD (SplitInputMappings_MigrateFromLegacySingleMode)
    {
        auto load = [] (const std::string & body)
        {
            InMemoryFileSystem  fs;
            GlobalUserPrefs     p;
            std::string  json = std::string ("{\"$cassoGlobalPrefsVersion\":1,") + body + "}";
            Assert::IsTrue (SUCCEEDED (fs.WriteAllText (
                GlobalUserPrefs::FilePath (L"C:\Casso"), json)));
            Assert::IsTrue (SUCCEEDED (p.Load (L"C:\Casso", fs)));
            return p;
        };

        GlobalUserPrefs  joy = load ("\"inputMappingMode\":\"joystick\"");
        Assert::IsTrue  (joy.arrowsToJoystick,                          L"legacy joystick -> Keys on");
        Assert::IsTrue  (joy.pointerMapping == InputMappingMode::Off,   L"legacy joystick -> Pointer off");

        GlobalUserPrefs  pad = load ("\"inputMappingMode\":\"paddle\"");
        Assert::IsFalse (pad.arrowsToJoystick);
        Assert::IsTrue  (pad.pointerMapping == InputMappingMode::Paddle, L"legacy paddle -> Pointer paddle");

        GlobalUserPrefs  ms = load ("\"inputMappingMode\":\"mouse\"");
        Assert::IsTrue  (ms.pointerMapping == InputMappingMode::Mouse,   L"legacy mouse -> Pointer mouse");

        GlobalUserPrefs  split = load (
            "\"inputMappingMode\":\"paddle\",\"arrowsToJoystick\":true,\"pointerMapping\":\"mouse\"");
        Assert::IsTrue  (split.arrowsToJoystick,                         L"new keys win over legacy");
        Assert::IsTrue  (split.pointerMapping == InputMappingMode::Mouse);
    }


    TEST_METHOD (RoundTrip_PreservesUnknownTopLevelKey)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr;
        std::string         text;

        hr = fs.WriteAllText (GlobalUserPrefs::FilePath (L"C:\\Casso"),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"X\",\"futureKey\":\"keep me\"}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.Load (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.Save (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        text = fs.PeekContent (GlobalUserPrefs::FilePath (L"C:\\Casso"));
        Assert::IsTrue (text.find ("futureKey")  != std::string::npos);
        Assert::IsTrue (text.find ("\"keep me\"") != std::string::npos);
    }


    TEST_METHOD (Save_PreservesExistingMachinesSection)
    {
        // Regression: GlobalUserPrefs::Save previously wrote a hardcoded
        // empty "machines" object, wiping any per-machine prefs that
        // UserConfigStore had persisted. Main.cpp's pre-flight
        // disk-audio save then clobbered the saved disk path every
        // single launch. Verify Save reads the existing file and
        // preserves its "machines" section verbatim.
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        std::string         seed;
        std::string         text;
        HRESULT             hr;


        seed =
            "{"
              "\"global\": { \"$cassoGlobalPrefsVersion\": 1, \"activeTheme\": \"Skeuomorphic\" },"
              "\"machines\": {"
                "\"Apple2e\": {"
                  "\"$cassoMachineVersion\": 6,"
                  "\"$cassoUiPrefs\": { \"disk1Path\": \"D:\\\\boot.dsk\" }"
                "}"
              "}"
            "}";

        hr = fs.WriteAllText (GlobalUserPrefs::FilePath (L"C:\\Casso"), seed);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.Load (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        // The global-prefs Load path doesn't surface the machines
        // section into the in-memory struct, so a subsequent Save must
        // not lose it.
        hr = prefs.Save (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        text = fs.PeekContent (GlobalUserPrefs::FilePath (L"C:\\Casso"));
        Assert::IsTrue (text.find ("Apple2e")      != std::string::npos, L"Apple2e machine entry was wiped by Save");
        Assert::IsTrue (text.find ("disk1Path")    != std::string::npos, L"disk1Path was wiped by Save");
        Assert::IsTrue (text.find ("D:\\\\boot.dsk") != std::string::npos, L"disk1Path value was wiped by Save");
    }


    TEST_METHOD (FromJson_OnNonObject_Fails)
    {
        GlobalUserPrefs  prefs;
        JsonValue        v (42.0);
        HRESULT          hr;

        hr = prefs.FromJson (v);
        Assert::IsTrue (FAILED (hr));
    }


    TEST_METHOD (RecentDisks_RoundTrip)
    {
        GlobalUserPrefs  orig;
        GlobalUserPrefs  loaded;
        JsonValue        v;
        HRESULT          hr;

        orig.recentDisks.push_back ("C:\\Disks\\A.dsk");
        orig.recentDisks.push_back ("C:\\Disks\\B.dsk");

        v  = orig.ToJson();
        hr = loaded.FromJson (v);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 2, loaded.recentDisks.size());
        Assert::AreEqual (std::string ("C:\\Disks\\A.dsk"), loaded.recentDisks[0]);
        Assert::AreEqual (std::string ("C:\\Disks\\B.dsk"), loaded.recentDisks[1]);
    }


    TEST_METHOD (RecentDisks_DropsMalformedEntries)
    {
        GlobalUserPrefs                                  prefs;
        std::vector<std::pair<std::string, JsonValue>>   root;
        std::vector<JsonValue>                           arr;
        JsonValue                                        v;
        HRESULT                                          hr;

        arr.emplace_back (JsonValue (std::string ("C:\\good.dsk")));
        arr.emplace_back (JsonValue (42.0));                          // wrong type
        arr.emplace_back (JsonValue (std::string ("")));              // empty
        arr.emplace_back (JsonValue (std::string ("C:\\good2.dsk")));

        root.emplace_back ("recentDisks", JsonValue (std::move (arr)));
        v = JsonValue (std::move (root));

        hr = prefs.FromJson (v);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 2, prefs.recentDisks.size());
        Assert::AreEqual (std::string ("C:\\good.dsk"),  prefs.recentDisks[0]);
        Assert::AreEqual (std::string ("C:\\good2.dsk"), prefs.recentDisks[1]);
    }


    TEST_METHOD (RecentDiskLoadedAt_RoundTrip)
    {
        GlobalUserPrefs  orig;
        GlobalUserPrefs  loaded;
        JsonValue        v;
        HRESULT          hr;

        orig.recentDisks.push_back ("C:\\Disks\\A.dsk");
        orig.recentDisks.push_back ("C:\\Disks\\B.dsk");
        orig.recentDiskLoadedAt.push_back (1700000001);
        orig.recentDiskLoadedAt.push_back (1700000002);

        v  = orig.ToJson();
        hr = loaded.FromJson (v);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 2, loaded.recentDiskLoadedAt.size());
        Assert::AreEqual ((std::int64_t) 1700000001, loaded.recentDiskLoadedAt[0]);
        Assert::AreEqual ((std::int64_t) 1700000002, loaded.recentDiskLoadedAt[1]);
    }


    TEST_METHOD (RecentDiskLoadedAt_LegacyMissingKey_LoadsEmpty)
    {
        // A prefs file written before load-time tracking has recentDisks
        // but no recentDiskLoadedAt; the times array must load empty so
        // DiskMru treats every entry's load time as unknown (0).
        GlobalUserPrefs                                 prefs;
        std::vector<std::pair<std::string, JsonValue>>  root;
        std::vector<JsonValue>                          arr;
        JsonValue                                       v;
        HRESULT                                         hr;

        arr.emplace_back (JsonValue (std::string ("C:\\good.dsk")));
        root.emplace_back ("recentDisks", JsonValue (std::move (arr)));
        v = JsonValue (std::move (root));

        hr = prefs.FromJson (v);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 1, prefs.recentDisks.size());
        Assert::AreEqual ((size_t) 0, prefs.recentDiskLoadedAt.size());
    }
};
