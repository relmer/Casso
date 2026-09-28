#include "Pch.h"

#include "Core/DxuiSystemSettings.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettingsTests
//
//  The cached interaction settings, and that a WM_SETTINGCHANGE reaching a
//  DxuiHwndSource re-reads them. Before that routing existed the cache was
//  filled once at startup, so turning animations off in Windows had no effect
//  on a running Casso.
//
//  Values come from a fake parameter reader, never from the real system.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiSystemSettingsTests)
{
public:

    static constexpr UINT  s_kFakeMenuDelayMs   = 1234;
    static constexpr UINT  s_kFakeMessageSec    = 9;
    static constexpr int   s_kMsPerSecond       = 1000;
    static constexpr UINT  s_kFakeWheelLines    = 7;
    static constexpr int   s_kDefaultWheelChars = 3;

    static inline bool  s_fakeAnimations = false;
    static inline UINT  s_fakeWheel      = s_kFakeWheelLines;


    static BOOL WINAPI FakeReader (UINT action, UINT param, PVOID pvParam, UINT winIni)
    {
        UNREFERENCED_PARAMETER (param);
        UNREFERENCED_PARAMETER (winIni);

        switch (action)
        {
            case SPI_GETCLIENTAREAANIMATION:
                *(BOOL *) pvParam = s_fakeAnimations ? TRUE : FALSE;
                return TRUE;

            case SPI_GETMENUSHOWDELAY:
                *(UINT *) pvParam = s_kFakeMenuDelayMs;
                return TRUE;

            case SPI_GETMESSAGEDURATION:
                *(UINT *) pvParam = s_kFakeMessageSec;
                return TRUE;

            case SPI_GETWHEELSCROLLLINES:
                *(UINT *) pvParam = s_fakeWheel;
                return TRUE;

            default:
                return FALSE;
        }
    }


    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
        s_fakeAnimations = false;
        s_fakeWheel      = s_kFakeWheelLines;
    }


    TEST_METHOD_CLEANUP (Cleanup)
    {
        DxuiSystemSettings::Instance().SetParameterReader (nullptr);
        DxuiSystemSettings::Instance().Refresh();
    }


    TEST_METHOD (Refresh_ReadsThroughTheInstalledReader)
    {
        DxuiSystemSettings &  settings = DxuiSystemSettings::Instance();



        settings.SetParameterReader (FakeReader);
        settings.Refresh();

        Assert::IsFalse  (settings.AreAnimationsEnabled());
        Assert::AreEqual ((int) s_kFakeMenuDelayMs, settings.GetMenuShowDelayMs());
        Assert::AreEqual ((int) s_kFakeMessageSec * s_kMsPerSecond, settings.GetMessageDurationMs());
        Assert::AreEqual ((int) s_kFakeWheelLines, settings.GetWheelLinesPerNotch());
    }


    TEST_METHOD (Refresh_WheelPageScroll_MapsToSentinel)
    {
        DxuiSystemSettings &  settings = DxuiSystemSettings::Instance();



        s_fakeWheel = WHEEL_PAGESCROLL;
        settings.SetParameterReader (FakeReader);
        settings.Refresh();

        Assert::AreEqual (DxuiSystemSettings::kWheelPageScroll, settings.GetWheelLinesPerNotch());
    }


    TEST_METHOD (Refresh_FailedQuery_KeepsWindowsDefault)
    {
        DxuiSystemSettings &  settings = DxuiSystemSettings::Instance();



        // FakeReader fails SPI_GETWHEELSCROLLCHARS; the Windows default stands.
        settings.SetParameterReader (FakeReader);
        settings.Refresh();

        Assert::AreEqual (s_kDefaultWheelChars, settings.GetWheelCharsPerNotch());
    }


    TEST_METHOD (HandleMessage_SettingChange_RefreshesSettings)
    {
        DxuiSystemSettings &              settings  = DxuiSystemSettings::Instance();
        std::unique_ptr<DxuiHwndSource>   host;
        DxuiHwndSource::CreateParams      cp;
        HRESULT                           hr        = S_OK;
        LRESULT                           outResult = 0;
        bool                              handled   = false;



        cp.title = L"SettingChangeTest";

        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, cp, host);
        Assert::AreEqual (S_OK, hr);

        s_fakeAnimations = true;
        settings.SetParameterReader (FakeReader);
        settings.Refresh();
        Assert::IsTrue (settings.AreAnimationsEnabled());

        // The user turns animations off; Windows broadcasts the change.
        s_fakeAnimations = false;
        handled = host->HandleMessage (WM_SETTINGCHANGE, SPI_SETCLIENTAREAANIMATION, 0, outResult);

        Assert::IsFalse (handled);
        Assert::IsFalse (settings.AreAnimationsEnabled());
    }
};





