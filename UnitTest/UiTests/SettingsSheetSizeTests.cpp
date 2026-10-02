#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"
#include "Ui/Settings/SettingsSheetSize.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsSheetSizeTests
//
//  The Settings sheet's remembered size: what it opens at, what is recorded,
//  and that the record survives a save and a load. Driven against an
//  in-memory file system, so nothing touches disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SettingsSheetSizeTests)
{
public:

    static constexpr SIZE  kDesignDip = { 720, 880 };
    static constexpr SIZE  kMinDip    = { 720, 480 };
    static constexpr UINT  kScaledDpi = 144;


    TEST_METHOD (InitialSize_IsTheDesignUntilOneIsRemembered)
    {
        GlobalUserPrefs  prefs;
        SIZE             sizeDip = SettingsSheetSize::GetInitialSizeDip (prefs, kDesignDip, kMinDip);

        Assert::AreEqual (kDesignDip.cx, sizeDip.cx);
        Assert::AreEqual (kDesignDip.cy, sizeDip.cy);
    }


    TEST_METHOD (InitialSize_IsTheRememberedOne)
    {
        GlobalUserPrefs  prefs;
        SIZE             sizeDip;
        constexpr int    kWidthDip  = 760;
        constexpr int    kHeightDip = 600;



        prefs.settingsWidthDip  = kWidthDip;
        prefs.settingsHeightDip = kHeightDip;
        sizeDip                 = SettingsSheetSize::GetInitialSizeDip (prefs, kDesignDip, kMinDip);

        Assert::AreEqual (kWidthDip,  (int) sizeDip.cx);
        Assert::AreEqual (kHeightDip, (int) sizeDip.cy);
    }


    TEST_METHOD (InitialSize_IsNeverBelowTheMinimum)
    {
        GlobalUserPrefs  prefs;
        SIZE             sizeDip;
        constexpr int    kTinyDip = 100;



        prefs.settingsWidthDip  = kTinyDip;
        prefs.settingsHeightDip = kTinyDip;
        sizeDip                 = SettingsSheetSize::GetInitialSizeDip (prefs, kDesignDip, kMinDip);

        Assert::AreEqual (kMinDip.cx, sizeDip.cx);
        Assert::AreEqual (kMinDip.cy, sizeDip.cy);
    }


    TEST_METHOD (StoredSize_SurvivesASaveAndALoad)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     saved;
        GlobalUserPrefs     loaded;
        HRESULT             hr         = S_OK;
        constexpr SIZE      kResizeDip = { 730, 640 };



        Assert::IsTrue  (SettingsSheetSize::TryStoreSizeDip (saved, kResizeDip), L"a new size is a change");
        Assert::IsFalse (SettingsSheetSize::TryStoreSizeDip (saved, kResizeDip), L"the same size again is not");

        hr = saved.Save (L"C:\\Casso", fs);
        Assert::AreEqual (S_OK, hr);

        hr = loaded.Load (L"C:\\Casso", fs);
        Assert::AreEqual (S_OK, hr);

        Assert::AreEqual ((int) kResizeDip.cx, loaded.settingsWidthDip);
        Assert::AreEqual ((int) kResizeDip.cy, loaded.settingsHeightDip);
    }


    TEST_METHOD (NoSizeIsWrittenUntilOneIsStored)
    {
        GlobalUserPrefs  prefs;
        JsonValue        json     = prefs.ToJson();
        int              widthDip = 0;

        Assert::IsFalse (json.HasInt ("settingsWidthDip",  widthDip));
        Assert::IsFalse (json.HasInt ("settingsHeightDip", widthDip));
    }


    TEST_METHOD (Dips_ConvertToPixelsAndBackAtTheDpi)
    {
        SIZE  sizePx = SettingsSheetSize::DipToPx (kDesignDip, kScaledDpi);
        SIZE  back   = SettingsSheetSize::PxToDip (sizePx, kScaledDpi);

        Assert::AreEqual (kDesignDip.cx * (LONG) kScaledDpi / USER_DEFAULT_SCREEN_DPI, sizePx.cx);
        Assert::AreEqual (kDesignDip.cx, back.cx);
        Assert::AreEqual (kDesignDip.cy, back.cy);
    }
};