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

};
