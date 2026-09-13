#include "Pch.h"

#include "Config/MonitorCatalog.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorCatalogTests
//
//  The catalog's config names are DURABLE USER DATA, not labels. They key a
//  monitor's own settings in the user's prefs file, first monitorTilt and
//  next the CRT overrides. ByName recovers to the default without a
//  diagnostic, so a rename does not fail anywhere -- it silently files a
//  user's settings under a tube that never lights a pixel, and the settings
//  they had simply stop applying.
//
//  These tests are the enforcement for that. A rename fails the build here
//  rather than orphaning data on someone's disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MonitorCatalogTests)
{
public:

    // Both shipped names, written literally rather than derived, because deriving
    // them from the catalog would assert the catalog equals itself.
    TEST_METHOD (ShippedConfigNamesAreFrozen)
    {
        bool  foundMonitorII  = false;
        bool  foundMonitorIIc = false;

        for (const MonitorSpec & spec : s_kMonitors)
        {
            if (spec.configName == "AppleMonitorII")
            {
                foundMonitorII = true;
            }

            if (spec.configName == "AppleMonitorIIc")
            {
                foundMonitorIIc = true;
            }
        }

        Assert::IsTrue (foundMonitorII,  L"AppleMonitorII is a shipped key and cannot be renamed");
        Assert::IsTrue (foundMonitorIIc, L"AppleMonitorIIc is a shipped key and cannot be renamed");
    }


    // Presence, never an exact count. Adding a third tube is a decision
    // someone makes deliberately, and it should not turn this file red with
    // a message about a number.
    TEST_METHOD (CatalogHoldsAtLeastTheShippedMonitors)
    {
        Assert::IsTrue (_countof (s_kMonitors) >= 2);
    }


    TEST_METHOD (DefaultIsTheFirstEntry)
    {
        Assert::IsTrue (&MonitorCatalog::Default() == &s_kMonitors[0]);
        Assert::IsTrue (MonitorCatalog::Default().configName == "AppleMonitorII");
    }


    TEST_METHOD (ByNameFindsEachShippedMonitor)
    {
        Assert::IsTrue (MonitorCatalog::ByName ("AppleMonitorII").configName  == "AppleMonitorII");
        Assert::IsTrue (MonitorCatalog::ByName ("AppleMonitorIIc").configName == "AppleMonitorIIc");
    }


    // The silent recovery this file exists because of. Acceptable when the
    // answer picks a mesh, because the worst case is the wrong housing. Not
    // acceptable once the same string decides where a user's settings live,
    // which is why the names above are frozen rather than merely documented.
    TEST_METHOD (ByNameRecoversToDefaultForAnUnknownName)
    {
        Assert::IsTrue (MonitorCatalog::ByName ("MonitorII").configName == "AppleMonitorII");
        Assert::IsTrue (MonitorCatalog::ByName ("").configName          == "AppleMonitorII");
    }


    TEST_METHOD (ForMachineJsonReadsTheMonitorKey)
    {
        JsonValue       doc;
        JsonParseError  err;
        HRESULT         hr = S_OK;

        hr = JsonParser::Parse ("{\"monitor\":\"AppleMonitorIIc\"}", doc, err);
        AssertSucceeded (hr);

        Assert::IsTrue (MonitorCatalog::ForMachineJson (doc).configName == "AppleMonitorIIc");
    }


    TEST_METHOD (ForMachineJsonFallsBackWhenTheKeyIsAbsent)
    {
        JsonValue       doc;
        JsonParseError  err;
        HRESULT         hr = S_OK;

        hr = JsonParser::Parse ("{\"name\":\"Apple //e\"}", doc, err);
        AssertSucceeded (hr);

        Assert::IsTrue (MonitorCatalog::ForMachineJson (doc).configName == "AppleMonitorII");
    }


    // Both cataloged tubes are green today. This pins the mapping rather
    // than the fact, so it keeps holding when a color tube is added.
    TEST_METHOD (PhosphorSettingsIndexMatchesTheColorModeOrder)
    {
        for (const MonitorSpec & spec : s_kMonitors)
        {
            switch (spec.phosphor)
            {
                case ColorMode::GreenMono:
                    Assert::AreEqual (1, MonitorCatalog::PhosphorSettingsIndex (spec));
                    break;

                case ColorMode::AmberMono:
                    Assert::AreEqual (2, MonitorCatalog::PhosphorSettingsIndex (spec));
                    break;

                case ColorMode::WhiteMono:
                    Assert::AreEqual (3, MonitorCatalog::PhosphorSettingsIndex (spec));
                    break;

                default:
                    Assert::AreEqual (0, MonitorCatalog::PhosphorSettingsIndex (spec));
                    break;
            }
        }
    }


    static ColorMode  ColorModeOf (const char * json)
    {
        JsonValue       doc;
        JsonParseError  err;
        HRESULT         hr = S_OK;

        hr = JsonParser::Parse (json, doc, err);
        AssertSucceeded (hr);

        return MonitorCatalog::GetColorModeForMachineJson (doc);
    }


    // The case that shipped broken on a machine switch: a monochrome monitor
    // with the user's color saved over it.
    TEST_METHOD (SavedColorWinsOverAGreenMonitor)
    {
        Assert::AreEqual ((int) ColorMode::Color,
                          (int) ColorModeOf ("{\"$cassoUiPrefs\":{\"colorMode\":\"color\"}}"),
                          L"a saved color must not fall back to the monitor's green");
    }


    TEST_METHOD (EachSavedModeIsRead)
    {
        Assert::AreEqual ((int) ColorMode::GreenMono,
                          (int) ColorModeOf ("{\"monitor\":\"AppleMonitorII\",\"$cassoUiPrefs\":{\"colorMode\":\"green\"}}"));
        Assert::AreEqual ((int) ColorMode::AmberMono,
                          (int) ColorModeOf ("{\"$cassoUiPrefs\":{\"colorMode\":\"amber\"}}"));
        Assert::AreEqual ((int) ColorMode::WhiteMono,
                          (int) ColorModeOf ("{\"$cassoUiPrefs\":{\"colorMode\":\"white\"}}"));
    }


    // Nothing saved -- no key, no block, or a value this build does not know --
    // is the monitor's own phosphor, so the screen matches the tube on the desk.
    TEST_METHOD (NothingUsableSavedIsTheMonitorsPhosphor)
    {
        ColorMode  phosphor = MonitorCatalog::ByName ("AppleMonitorIIc").phosphor;

        Assert::AreEqual ((int) phosphor, (int) ColorModeOf ("{\"monitor\":\"AppleMonitorIIc\"}"),
                          L"no $cassoUiPrefs block");
        Assert::AreEqual ((int) phosphor, (int) ColorModeOf ("{\"monitor\":\"AppleMonitorIIc\",\"$cassoUiPrefs\":{}}"),
                          L"a block without colorMode");
        Assert::AreEqual ((int) phosphor, (int) ColorModeOf ("{\"monitor\":\"AppleMonitorIIc\",\"$cassoUiPrefs\":{\"colorMode\":\"sepia\"}}"),
                          L"a value this build does not know");
    }


    // Both paths turn the one answer into what they apply, so the two
    // conversions have to agree with each other for every mode.
    TEST_METHOD (ViewCommandAndSettingsIndexAgree)
    {
        Assert::AreEqual ((int) IDM_VIEW_COLOR, (int) MonitorCatalog::GetViewCommand (ColorMode::Color));
        Assert::AreEqual ((int) IDM_VIEW_GREEN, (int) MonitorCatalog::GetViewCommand (ColorMode::GreenMono));
        Assert::AreEqual ((int) IDM_VIEW_AMBER, (int) MonitorCatalog::GetViewCommand (ColorMode::AmberMono));
        Assert::AreEqual ((int) IDM_VIEW_WHITE, (int) MonitorCatalog::GetViewCommand (ColorMode::WhiteMono));

        Assert::AreEqual (0, MonitorCatalog::GetSettingsIndex (ColorMode::Color));
        Assert::AreEqual (1, MonitorCatalog::GetSettingsIndex (ColorMode::GreenMono));
        Assert::AreEqual (2, MonitorCatalog::GetSettingsIndex (ColorMode::AmberMono));
        Assert::AreEqual (3, MonitorCatalog::GetSettingsIndex (ColorMode::WhiteMono));
    }

};
