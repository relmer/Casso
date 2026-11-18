#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Shell/Components/ShellUpdater.h"
#include "Update/IUpdaterHost.h"
#include "Update/UpdateResult.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeUpdaterHost
//
//  A host with no windows, so ShellUpdater never builds its service. The stored
//  prefs replace the file another app writes.
//
////////////////////////////////////////////////////////////////////////////////

class FakeUpdaterHost : public IUpdaterHost
{
public:
    HWND                       GetResultWindow() override { return nullptr; }
    HWND                       GetOwnerWindow() override { return nullptr; }
    HINSTANCE                  GetInstance() override { return nullptr; }
    const IDxuiTheme         * GetDialogTheme() override { return nullptr; }
    GlobalUserPrefs          & GetPrefs() override { return prefs; }
    void                       SavePrefs() override { saves++; stored.CopyUpdateFieldsFrom (prefs); }
    HRESULT                    ReadStoredPrefs            (GlobalUserPrefs & outPrefs) override { outPrefs = stored; return readResult; }
    bool                       HasCaption() override { return true; }
    bool                       TryGetClientWidthPx        (int & outWidthPx) override { outWidthPx = 0; return false; }
    UINT                       GetDpi() override { return USER_DEFAULT_SCREEN_DPI; }
    void                       SetCaptionAccessory        (IDxuiControl * accessory) override { this->accessory = accessory; }
    void                       SetCaptionAccessoryWidth   (int) override {}
    int                        GetCaptionReservedWidthDip() override { return 0; }
    void                       RequestRepaint() override {}
    void                       ShowIndicatorTip           (const RECT &, const std::wstring &, int64_t) override {}
    void                       HideIndicatorTip() override {}
    void                       ShowNotice                 (const std::wstring & text) override { notices.push_back (text); }
    void                       ShowNotification           (const std::wstring & text) override { notifications.push_back (text); }
    int                        ShowModalDialog            (const DialogDefinition &) override { dialogs++; return 0; }
    void                       RefreshUpdateStatus() override {}
    HRESULT                    StartSharedCheckTimer      (UINT) override { isTimerRunning = true; return S_OK; }
    void                       StopSharedCheckTimer() override { isTimerRunning = false; }
    void                       PrepareForDeploy() override {}
    void                       RequestClose() override { closes++; }
    std::vector<std::wstring>  GetRelaunchArguments() override { return {}; }

    GlobalUserPrefs            prefs;
    GlobalUserPrefs            stored;
    HRESULT                    readResult     = S_OK;
    IDxuiControl             * accessory      = nullptr;
    std::vector<std::wstring>  notices;
    std::vector<std::wstring>  notifications;
    int                        saves          = 0;
    int                        dialogs        = 0;
    int                        closes         = 0;
    bool                       isTimerRunning = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdaterTests
//
//  The update logic both apps share, through a fake host.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellUpdaterTests)
{
public:
    static UpdateResult MakeCheckResult (const char * version, bool isOffered)
    {
        UpdateResult  result;
        bool          isParsed = ReleaseVersion::TryParse (version, result.release.version);



        Assert::IsTrue (isParsed);

        result.kind         = UpdateResultKind::Check;
        result.trigger      = UpdateCheckTrigger::Automatic;
        result.checkedAtUtc = 1790000000;
        result.isNewer      = isOffered;
        result.isOffered    = isOffered;

        return result;
    }


    TEST_METHOD (CheckResult_RecordsAndShowsIndicator)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);
        UpdateResult     result  = MakeCheckResult ("99.1.0", true);



        updater.HandleUpdateResult (result);

        Assert::AreEqual (1, host.saves);
        Assert::AreEqual (std::string ("99.1.0"), host.stored.latestKnownVersion);
        Assert::AreEqual ((long long) 1790000000, (long long) host.stored.lastUpdateCheckUtc);
        Assert::IsTrue   (updater.IsIndicatorVisible());
    }


    TEST_METHOD (CheckResult_KeepsSkipTheOtherAppWrote)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);
        UpdateResult     result  = MakeCheckResult ("99.1.0", true);



        host.stored.skippedVersion = "98.0.0";

        updater.HandleUpdateResult (result);

        Assert::AreEqual (std::string ("98.0.0"), host.stored.skippedVersion);
    }


    TEST_METHOD (CheckResult_FailedAutomaticCheckRecordsNothing)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);
        UpdateResult     result  = MakeCheckResult ("99.1.0", true);



        result.failure = UpdateFailure::Network;

        updater.HandleUpdateResult (result);

        Assert::AreEqual (0, host.saves);
        Assert::AreEqual (0, host.dialogs);
        Assert::IsFalse  (updater.IsIndicatorVisible());
    }


    TEST_METHOD (StopSkipping_ShowsLatestKnownFromOtherApp)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);



        host.stored.latestKnownVersion = "99.1.0";
        host.stored.skippedVersion     = "99.1.0";

        updater.StartAutomaticUpdateCheck();
        updater.StopSkippingVersion();

        Assert::IsTrue   (host.stored.skippedVersion.empty());
        Assert::AreEqual (std::string ("99.1.0"), host.stored.latestKnownVersion);
        Assert::IsTrue   (updater.IsIndicatorVisible());
    }


    TEST_METHOD (SetAutoUpdateCheck_FailedReadKeepsValueInMemory)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);



        host.readResult           = E_FAIL;
        host.prefs.skippedVersion = "1.0.0";

        updater.SetAutoUpdateCheck (false);

        Assert::IsFalse  (host.prefs.autoUpdateCheck);
        Assert::AreEqual (std::string ("1.0.0"), host.prefs.skippedVersion);
    }


    TEST_METHOD (CheckSkipped_PollAdoptsOtherAppsRecordAndStops)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);
        UpdateResult     skipped;



        skipped.kind = UpdateResultKind::CheckSkipped;

        updater.HandleUpdateResult (skipped);
        Assert::IsTrue (host.isTimerRunning);

        updater.PollSharedCheckRecord();
        Assert::IsTrue  (host.isTimerRunning, L"The record is no newer yet");
        Assert::IsFalse (updater.IsIndicatorVisible());

        host.stored.lastUpdateCheckUtc = 1790000000;
        host.stored.latestKnownVersion = "99.1.0";

        updater.PollSharedCheckRecord();

        Assert::IsFalse  (host.isTimerRunning);
        Assert::AreEqual (std::string ("99.1.0"), host.prefs.latestKnownVersion);
        Assert::IsTrue   (updater.IsIndicatorVisible());
    }


    TEST_METHOD (ApplyFailure_NoDialogNotifiesAndDoesNotClose)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);
        UpdateResult     result;



        result.kind    = UpdateResultKind::Applied;
        result.failure = UpdateFailure::OtherInstanceRunning;

        updater.HandleUpdateResult (result);

        Assert::AreEqual ((size_t) 1, host.notifications.size());
        Assert::IsTrue   (host.notifications[0].find (L"Casso Explorer") != std::wstring::npos);
        Assert::AreEqual (0, host.closes, L"The other app is the user's to close, and so is this one");
    }


    TEST_METHOD (Applied_ZipClosesThroughHost)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);
        UpdateResult     result;



        result.kind        = UpdateResultKind::Applied;
        result.installType = InstallType::Zip;

        updater.HandleUpdateResult (result);

        Assert::AreEqual (1, host.closes);
    }


    TEST_METHOD (RebindIndicator_BeforeStartLeavesCaptionAlone)
    {
        FakeUpdaterHost  host;
        ShellUpdater     updater (host);



        updater.RebindIndicator();
        Assert::IsNull (host.accessory);
    }
};
