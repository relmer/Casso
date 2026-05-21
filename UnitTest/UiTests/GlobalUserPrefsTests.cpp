#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"

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
        Assert::AreEqual (false, prefs.crt.scanlinesEnabled);
        Assert::AreEqual (false, prefs.window.fHaveLastBounds);
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
        orig.crt.brightness         = 1.25f;
        orig.crt.scanlinesEnabled   = true;
        orig.crt.scanlinesIntensity = 0.75f;
        orig.crt.bloomEnabled       = true;
        orig.crt.bloomRadius        = 2.0f;
        orig.crt.bloomStrength      = 0.6f;
        orig.crt.colorBleedEnabled  = true;
        orig.crt.colorBleedWidth    = 1.5f;
        orig.window.fHaveLastBounds = true;
        orig.window.x = 100; orig.window.y = 50;
        orig.window.w = 1280; orig.window.h = 720;
        orig.window.fullscreen      = true;

        hr = orig.Save (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = loaded.Load (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (orig.activeTheme,         loaded.activeTheme);
        Assert::AreEqual (orig.lastSelectedMachine, loaded.lastSelectedMachine);
        Assert::AreEqual (orig.crt.brightness,      loaded.crt.brightness);
        Assert::AreEqual (orig.crt.scanlinesEnabled,    loaded.crt.scanlinesEnabled);
        Assert::AreEqual (orig.crt.scanlinesIntensity,  loaded.crt.scanlinesIntensity);
        Assert::AreEqual (orig.crt.bloomEnabled,        loaded.crt.bloomEnabled);
        Assert::AreEqual (orig.crt.bloomRadius,         loaded.crt.bloomRadius);
        Assert::AreEqual (orig.crt.bloomStrength,       loaded.crt.bloomStrength);
        Assert::AreEqual (orig.crt.colorBleedEnabled,   loaded.crt.colorBleedEnabled);
        Assert::AreEqual (orig.crt.colorBleedWidth,     loaded.crt.colorBleedWidth);
        Assert::AreEqual (orig.window.fHaveLastBounds,  loaded.window.fHaveLastBounds);
        Assert::AreEqual (orig.window.x,                loaded.window.x);
        Assert::AreEqual (orig.window.fullscreen,       loaded.window.fullscreen);
    }


    TEST_METHOD (Load_MissingFields_Tolerated)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr;

        hr = fs.WriteAllText (
            GlobalUserPrefs::FilePath (L"C:\\Casso"),
            "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\"}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.Load (L"C:\\Casso", fs);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("DarkModern"), prefs.activeTheme);
        // crt sub-object missing → struct defaults preserved.
        Assert::AreEqual (1.0f, prefs.crt.brightness);
    }


    TEST_METHOD (RoundTrip_PreservesUnknownTopLevelKey)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr;
        std::string         text;

        hr = fs.WriteAllText (
            GlobalUserPrefs::FilePath (L"C:\\Casso"),
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


    TEST_METHOD (FromJson_OnNonObject_Fails)
    {
        GlobalUserPrefs  prefs;
        JsonValue        v (42.0);
        HRESULT          hr;

        hr = prefs.FromJson (v);
        Assert::IsTrue (FAILED (hr));
    }
};
