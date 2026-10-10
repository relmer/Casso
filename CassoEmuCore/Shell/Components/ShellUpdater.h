#pragma once

#include "Pch.h"

#include "Ui/Chrome/UpdateIndicatorButton.h"
#include "Update/InstallTypeDetector.h"
#include "Update/ReleaseInfo.h"
#include "Update/UpdateFailure.h"



class EmulatorShell;
class UpdateDialog;
class UpdateRuntime;
class UpdateService;
struct UpdateResult;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater
//
//  Update notification and self-update. The service does the slow work on its
//  own threads and posts each result back as WM_APP_UPDATE_RESULT; everything
//  here runs on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

class ShellUpdater
{
public:
    explicit ShellUpdater (EmulatorShell & shell);
    ~ShellUpdater();

    // A launch by a finished zip update (--updated, --cleanup-old <pid>).
    // Set before Initialize; the old files are removed once the first frame
    // is up and the old process has exited.
    void  SetUpdateLaunch (bool wasUpdated, DWORD cleanupOldPid) { m_wasLaunchedByUpdate = wasUpdated; m_cleanupOldPid = cleanupOldPid; }

    // Settings > General: whether the once-a-day update check runs, and the
    // skipped release. Each is saved immediately, like the other live
    // toggles in Settings.
    void  SetAutoUpdateCheck  (bool enabled);
    void  StopSkippingVersion ();

    bool                    HasCheckStarted             () const { return m_updateCheckStarted; }
    void                    StartAutomaticUpdateCheck   ();
    void                    CheckForUpdatesNow          ();
    void                    HandleUpdateResult          (UpdateResult & result);
    void                    PollSharedCheckRecord       ();
    bool                    TickUpdateIndicator         (int64_t nowMs);
    std::optional<int64_t>  GetMsUntilShimmer           (int64_t nowMs) const;
    bool                    OfferMouseToUpdateIndicator (DxuiMouseEventKind kind, int xPx, int yPx);
    void                    OnMouseLeave                ();
    void                    StopUpdateService           ();
    void                    CommitPendingUpdateAtExit   ();

    static void             OpenUrl                     (const std::wstring & url);

    // An instance whose startup check was skipped for the check lock polls
    // the prefs file for the record the lock holder writes.
    static constexpr UINT_PTR  kSharedCheckTimerId = 0xCA56;

private:
    UpdateService *        GetUpdateService            ();
    void                   HandleUpdateCheckResult     (UpdateResult & result);
    void                   StartSharedCheckWait        ();
    void                   StopSharedCheckWait         ();
    void                   HandleUpdateApplyResult     (UpdateResult & result);
    void                   ShowUpdateIndicator         (bool isShown);
    void                   RefitUpdateIndicator        (bool force);
    void                   OpenUpdateDialog            ();
    void                   ReportUpdateCheckFailure    (UpdateFailure failure);
    void                   ReportUpToDate              ();
    void                   SkipOfferedRelease          ();
    void                   RefreshSettingsUpdateStatus ();
    static ReleaseVersion  GetRunningVersion           ();
    static size_t          GetRandomIndex              (size_t count);
    std::wstring           MakeUpdateHeader            (const std::string & runningReleaseDate);
    void                   HandlePendingUpdateAtLaunch ();
    void                   SetUpdatePending            (const UpdateResult & result);
    void                   ClearPendingUpdatePrefs     ();
    void                   ApplyPendingUpdateNow       ();
    static std::vector<std::wstring>  GetRelaunchArguments ();
    static std::wstring    GetInstallDirectory         ();

    EmulatorShell                 & m_shell;

    std::unique_ptr<UpdateRuntime>  m_updateRuntime;
    UpdateIndicatorButton           m_updateIndicator;
    UpdateDialog                  * m_updateDialog          = nullptr;
    ReleaseInfo                     m_updateRelease;
    InstallType                     m_updateInstallType     = InstallType::Unknown;
    bool                            m_hasUpdateRelease      = false;
    bool                            m_updateCheckStarted    = false;
    bool                            m_isManualCheckPending  = false;
    bool                            m_wasLaunchedByUpdate   = false;
    DWORD                           m_cleanupOldPid         = 0;

    std::int64_t                    m_launchCheckUtc        = 0;
    int                             m_sharedCheckPolls      = 0;
    bool                            m_isSharedCheckWaiting  = false;
    std::wstring                    m_updateIndicatorLine;
    int                             m_indicatorClientPx     = -1;
    int                             m_indicatorWidthDip     = 0;

    // An update applied when Casso closes: the zip copy's staged files, or
    // the bundle Windows registers once Casso exits.
    bool                            m_isUpdatePending       = false;
    InstallType                     m_pendingInstallType    = InstallType::Unknown;
    std::wstring                    m_pendingInstallDir;
    std::vector<std::string>        m_pendingPaths;
    std::wstring                    m_pendingBundlePath;
};
