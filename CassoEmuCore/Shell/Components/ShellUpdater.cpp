#include "Pch.h"

#include "Shell/Components/ShellUpdater.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Dialogs/UpdateDialog.h"
#include "Config/GlobalUserPrefs.h"
#include "Core/AppMessages.h"
#include "Update/IUpdaterHost.h"
#include "Update/UpdateDialogModel.h"
#include "Update/UpdateResult.h"
#include "Update/UpdateRuntime.h"
#include "Update/UpdateSchedule.h"
#include "Ui/Chrome/UpdateIndicatorModel.h"
#include "Update/AuthenticodeVerifier.h"
#include "Update/PendingUpdateModel.h"
#include "Update/Win32UpdateFileSystem.h"
#include "Update/Win32UpdateHost.h"
#include "Update/ZipUpdateInstaller.h"
#include "Core/TextEncoding.h"
#include "Version.h"
#include "Core/AppTimerIds.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater
//
////////////////////////////////////////////////////////////////////////////////

ShellUpdater::ShellUpdater (IUpdaterHost & host)
    : m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellUpdater
//
//  Out of line so UpdateRuntime is complete where the unique_ptr destroys it.
//
////////////////////////////////////////////////////////////////////////////////

ShellUpdater::~ShellUpdater() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::GetMsUntilShimmer
//
//  How long the idle loop may sleep before the indicator's shimmer wants a
//  frame; nothing when the indicator is hidden.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int64_t> ShellUpdater::GetMsUntilShimmer (int64_t nowMs) const
{
    return m_updateIndicator.IsVisible() ? m_updateIndicator.GetMsUntilShimmer (nowMs) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::OnMouseLeave
//
//  Leaving the window -- into the caption counts -- from the update indicator
//  takes its tooltip down at once; nothing else would, since the indicator is
//  a client-area control under the caption's tooltip.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::OnMouseLeave()
{
    if (m_updateIndicator.OnPointer (false, (int64_t) GetTickCount64()).hideTip)
    {
        m_host.HideIndicatorTip();
        m_host.RequestRepaint();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::GetRunningVersion
//
////////////////////////////////////////////////////////////////////////////////

ReleaseVersion ShellUpdater::GetRunningVersion()
{
    return ReleaseVersion { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::GetRandomIndex
//
//  A uniform index below `count`, for picking the update dialog's remark.
//
////////////////////////////////////////////////////////////////////////////////

size_t ShellUpdater::GetRandomIndex (size_t count)
{
    std::random_device                     device;
    std::uniform_int_distribution<size_t>  pick (0, (count > 0) ? count - 1 : 0);



    return pick (device);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::MakeUpdateHeader
//
//  The dialog's header with its one closing remark. The running build's
//  date, when the notes gave one, adds the age remarks to the pick.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellUpdater::MakeUpdateHeader (const std::string & runningReleaseDate)
{
    std::optional<int>  ageDays;
    int                 days    = 0;
    bool                isKnown = UpdateDialogModel::TryGetDaysSince (runningReleaseDate, UpdateRuntime::GetUtcNow(), days);



    if (isKnown)
    {
        ageDays = days;
    }

    return UpdateDialogModel::MakeFinalHeader (m_updateRelease.version,
                                               m_updateRelease.publishedDate,
                                               GetRunningVersion(),
                                               ageDays,
                                               &ShellUpdater::GetRandomIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::GetUpdateService
//
//  Built on first use, once there is a window to post results to.
//
////////////////////////////////////////////////////////////////////////////////

UpdateService * ShellUpdater::GetUpdateService()
{
    if (m_updateRuntime == nullptr && m_host.GetResultWindow() != nullptr)
    {
        m_updateRuntime = std::make_unique<UpdateRuntime> (m_host.GetResultWindow(), (UINT) AppMessage::ShellUpdateResult);
    }

    return (m_updateRuntime != nullptr) ? &m_updateRuntime->GetService() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::SetAutoUpdateCheck
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::SetAutoUpdateCheck (bool enabled)
{
    AdoptStoredUpdateFields();

    m_host.GetPrefs().autoUpdateCheck = enabled;
    m_host.SavePrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::StopSkippingVersion
//
//  Settings > General > Cancel skip. Clearing the skip lets the release
//  last found show the indicator again, decided as at startup.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::StopSkippingVersion()
{
    bool  isShown = false;



    AdoptStoredUpdateFields();

    m_host.GetPrefs().skippedVersion.clear();
    m_host.SavePrefs();

    isShown = m_isUpdatePending ||
              (m_host.GetPrefs().autoUpdateCheck &&
               UpdateSchedule::ShouldShowIndicator (GetRunningVersion(), m_host.GetPrefs().latestKnownVersion, m_host.GetPrefs().skippedVersion));

    if (m_updateCheckStarted)
    {
        ShowUpdateIndicator (isShown);
    }

    RefreshSettingsUpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::AdoptStoredUpdateFields
//
//  Casso and Casso Explorer share these fields in one file, so a change starts
//  from what the other app last wrote. A failed read keeps the values in memory.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::AdoptStoredUpdateFields()
{
    GlobalUserPrefs  stored;
    HRESULT          hr     = m_host.ReadStoredPrefs (stored);



    if (SUCCEEDED (hr))
    {
        m_host.GetPrefs().CopyUpdateFieldsFrom (stored);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::RebindIndicator
//
//  The host moved to another caption. The indicator goes into that caption at
//  the width its text fits there.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::RebindIndicator()
{
    if (m_updateCheckStarted)
    {
        m_host.SetCaptionAccessory (&m_updateIndicator);

        m_indicatorWidthDip = 0;
        RefitUpdateIndicator (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::RefreshSettingsUpdateStatus
//
//  Brings an open Settings sheet's update lines up to date with the prefs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::RefreshSettingsUpdateStatus()
{
    m_host.RefreshUpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::StartAutomaticUpdateCheck
//
//  Runs once, after the first frame is on screen, so nothing about updates
//  delays startup. Puts the indicator in the caption (hidden), starts the
//  removal of a zip update's old files when this launch came from one, and
//  then either starts the daily check or, when it is not due, shows the
//  indicator from what the last check found.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::StartAutomaticUpdateCheck()
{
    UpdateService  * service  = nullptr;
    HRESULT          hr       = S_OK;
    ReleaseVersion   running  = GetRunningVersion();
    bool             isDue    = false;
    bool             isShown  = false;



    m_updateCheckStarted = true;

    AdoptStoredUpdateFields();

    service = GetUpdateService();
    BAIL_OUT_IF (service == nullptr, S_OK);

    m_host.SetCaptionAccessory (&m_updateIndicator);

    service->SetRelaunchArguments (m_host.GetRelaunchArguments());

    if (m_cleanupOldPid != 0)
    {
        hr = service->StartCleanup (m_cleanupOldPid);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_wasLaunchedByUpdate)
    {
        m_host.ShowNotice (UpdateDialogModel::MakeUpdatedNotice (VERSION_STRING));
    }
    else
    {
        HandlePendingUpdateAtLaunch();
    }

    isDue = UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic,
                                        m_host.GetPrefs().autoUpdateCheck,
                                        m_host.GetPrefs().lastUpdateCheckUtc,
                                        UpdateRuntime::GetUtcNow(),
                                        m_updateRuntime->IsUsingLocalFeed());

    if (isDue)
    {
        m_launchCheckUtc = m_host.GetPrefs().lastUpdateCheckUtc;

        hr = service->StartCheck (UpdateCheckTrigger::Automatic, running, m_host.GetPrefs().skippedVersion);
        IGNORE_RETURN_VALUE (hr, S_OK);

        OutputDebugStringW (std::format (L"Casso: update check started (automatic, hr=0x{:08X})\n", (unsigned) hr).c_str());
    }
    else
    {
        isShown = m_host.GetPrefs().autoUpdateCheck &&
                  UpdateSchedule::ShouldShowIndicator (running, m_host.GetPrefs().latestKnownVersion, m_host.GetPrefs().skippedVersion);
        ShowUpdateIndicator (isShown);

        OutputDebugStringW (std::format (L"Casso: update check not due (auto={}, last={}); latest known '{}', skipped '{}': indicator {}\n",
                                         m_host.GetPrefs().autoUpdateCheck, m_host.GetPrefs().lastUpdateCheckUtc,
                                         TextEncoding::Utf8ToWide (m_host.GetPrefs().latestKnownVersion),
                                         TextEncoding::Utf8ToWide (m_host.GetPrefs().skippedVersion),
                                         isShown ? L"shown" : L"hidden").c_str());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::CheckForUpdatesNow
//
//  Help > Check for updates, and a click on the indicator before this
//  session has a release record. A check already in flight answers for
//  this one too; its result is then reported as a manual check's would be.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::CheckForUpdatesNow()
{
    UpdateService  * service = GetUpdateService();
    HRESULT          hr      = S_OK;



    CBRA (service != nullptr);

    m_isManualCheckPending = true;

    AdoptStoredUpdateFields();

    hr = service->StartCheck (UpdateCheckTrigger::Manual, GetRunningVersion(), m_host.GetPrefs().skippedVersion);

    if (hr == E_PENDING)
    {
        hr = S_OK;
    }

    CHRF (hr, ReportUpdateCheckFailure (UpdateFailure::Network));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::HandleUpdateResult
//
//  The UI-thread end of every piece of update work.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::HandleUpdateResult (UpdateResult & result)
{
    UpdateService  * service  = GetUpdateService();
    HRESULT          hrImages = S_OK;



    switch (result.kind)
    {
        case UpdateResultKind::Check:
            HandleUpdateCheckResult (result);
            break;

        case UpdateResultKind::CheckSkipped:
            StartSharedCheckWait();
            break;

        case UpdateResultKind::Notes:
            if (m_updateDialog != nullptr && result.release.tag == m_updateRelease.tag)
            {
                if (result.failure == UpdateFailure::None)
                {
                    m_updateDialog->ShowNotes (result.notes);

                    // The images come after the text, each on its own result.
                    hrImages = (service != nullptr) ? service->StartFetchImages (m_updateRelease.tag, m_updateDialog->GetImageSources()) : E_POINTER;
                    IGNORE_RETURN_VALUE (hrImages, S_OK);
                }
                else
                {
                    m_updateDialog->ShowNotesMissing();
                }

                m_updateDialog->SetHeader (MakeUpdateHeader (result.notes.runningReleaseDate));
            }

            break;

        case UpdateResultKind::Image:
            if (m_updateDialog != nullptr)
            {
                m_updateDialog->ShowImage (result.imageSrc, result.failure == UpdateFailure::None ? result.image : nullptr);
            }

            break;

        case UpdateResultKind::ReadyToDeploy:
        case UpdateResultKind::Applied:
            HandleUpdateApplyResult (result);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::HandleUpdateCheckResult
//
//  A failed automatic check says nothing. A successful check is recorded
//  (time and version) whatever it found, so the daily limit and the
//  indicator at the next start both work from it. A manual check always
//  reports: the dialog for a newer release, even a skipped one, or a
//  message for up to date and for failure.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::HandleUpdateCheckResult (UpdateResult & result)
{
    bool  isManual  = result.trigger == UpdateCheckTrigger::Manual || m_isManualCheckPending;
    bool  isOffered = isManual ? result.isNewer : result.isOffered;



    m_isManualCheckPending = false;

    StopSharedCheckWait();

    OutputDebugStringW (std::format (L"Casso: update check result ({}): failure {}, release '{}', newer {}, offered {}\n",
                                     isManual ? L"manual" : L"automatic", (int) result.failure,
                                     TextEncoding::Utf8ToWide (result.release.version.ToString()),
                                     result.isNewer, isOffered).c_str());

    if (result.failure != UpdateFailure::None)
    {
        if (isManual)
        {
            ReportUpdateCheckFailure (result.failure);
        }

        return;
    }

    AdoptStoredUpdateFields();

    m_host.GetPrefs().lastUpdateCheckUtc = result.checkedAtUtc;
    m_host.GetPrefs().latestKnownVersion = result.release.version.ToString();
    m_host.SavePrefs();
    RefreshSettingsUpdateStatus();

    m_updateRelease     = result.release;
    m_updateInstallType = result.installType;
    m_hasUpdateRelease  = true;

    ShowUpdateIndicator (isOffered);

    if (isManual && !result.isNewer)
    {
        ReportUpToDate();
    }
    else if (isManual)
    {
        OpenUpdateDialog();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::StartSharedCheckWait
//
//  The startup check was skipped because another Casso holds the check
//  lock. That instance records what it finds in the prefs file, so poll
//  the file for it rather than leave the indicator hidden all session.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::StartSharedCheckWait()
{
    HRESULT  hr = S_OK;



    m_sharedCheckPolls = 0;

    hr = m_host.StartSharedCheckTimer (UpdateSchedule::kSharedCheckPollMs);
    CHRA (hr);

    m_isSharedCheckWaiting = true;

    OutputDebugStringW (L"Casso: waiting for another Casso's update check record\n");

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::PollSharedCheckRecord
//
//  One re-read of the update fields. A record newer than the one read at
//  launch is adopted and decides the indicator as the not-due path would,
//  and the poll stops. When the wait runs out, the record on disk decides
//  it whatever its age. A read that fails, say against a file mid-write,
//  waits for the next poll.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::PollSharedCheckRecord()
{
    HRESULT             hr       = S_OK;
    GlobalUserPrefs     stored;
    SharedCheckOutcome  outcome  = SharedCheckOutcome::Wait;



    m_sharedCheckPolls++;

    hr = m_host.ReadStoredPrefs (stored);
    CHR (hr);

    outcome = UpdateSchedule::DecideSharedCheck (m_launchCheckUtc,
                                                 stored.lastUpdateCheckUtc,
                                                 m_host.GetPrefs().autoUpdateCheck,
                                                 GetRunningVersion(),
                                                 stored.latestKnownVersion,
                                                 stored.skippedVersion,
                                                 m_sharedCheckPolls >= UpdateSchedule::kSharedCheckPollLimit);
    BAIL_OUT_IF (outcome == SharedCheckOutcome::Wait, S_OK);

    m_host.GetPrefs().lastUpdateCheckUtc = stored.lastUpdateCheckUtc;
    m_host.GetPrefs().latestKnownVersion = stored.latestKnownVersion;
    m_host.GetPrefs().skippedVersion     = stored.skippedVersion;

    ShowUpdateIndicator (outcome == SharedCheckOutcome::Show);
    RefreshSettingsUpdateStatus();

    OutputDebugStringW (std::format (L"Casso: adopted another Casso's update check record; latest known '{}': indicator {}\n",
                                     TextEncoding::Utf8ToWide (m_host.GetPrefs().latestKnownVersion),
                                     outcome == SharedCheckOutcome::Show ? L"shown" : L"hidden").c_str());

Error:
    if (outcome != SharedCheckOutcome::Wait || m_sharedCheckPolls >= UpdateSchedule::kSharedCheckPollLimit)
    {
        StopSharedCheckWait();
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::StopSharedCheckWait
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::StopSharedCheckWait()
{
    if (m_isSharedCheckWaiting)
    {
        m_host.StopSharedCheckTimer();
    }

    m_isSharedCheckWaiting = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::HandleUpdateApplyResult
//
//  A bundle ready to deploy gets the normal exit flush first, because a
//  successful deploy ends this process without running the destructor. A
//  finished zip update closes the window the ordinary way, which flushes or
//  asks about unsaved disks as any exit does; the new Casso is already
//  starting and waits for this one to go.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::HandleUpdateApplyResult (UpdateResult & result)
{
    HRESULT          hr      = S_OK;
    UpdateService  * service = GetUpdateService();
    std::wstring     message;



    if (result.wasCanceled)
    {
        if (m_updateDialog != nullptr)
        {
            m_updateDialog->ShowCanceled();
        }

        return;
    }

    if (result.failure != UpdateFailure::None)
    {
        message = UpdateDialogModel::MakeUpdateFailedText (result.failure);

        if (m_updateDialog != nullptr)
        {
            m_updateDialog->ShowFailure (result.failure);
        }
        else
        {
            m_host.ShowNotification (message);
        }

        return;
    }

    if (result.kind == UpdateResultKind::ReadyToDeploy)
    {
        if (m_updateDialog != nullptr)
        {
            m_updateDialog->ShowInstalling();
        }

        m_host.PrepareForDeploy();

        CBRA (service != nullptr);

        hr = service->StartDeploy (result.bundlePath);

        if (FAILED (hr) && m_updateDialog != nullptr)
        {
            m_updateDialog->ShowFailure (UpdateFailure::InstallFailed);
        }

        return;
    }

    if (result.isPending)
    {
        SetUpdatePending (result);
        return;
    }

    // Applied now: nothing is left waiting for the exit.
    m_isUpdatePending = false;
    ClearPendingUpdatePrefs();

    if (m_updateDialog != nullptr)
    {
        m_updateDialog->ShowRestarting();
        m_updateDialog->EndDialog (UpdateDialog::kIdPrimary);
    }

    if (result.installType == InstallType::Zip)
    {
        m_host.RequestClose();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::ShowUpdateIndicator
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::ShowUpdateIndicator (bool isShown)
{
    std::string  version = m_host.GetPrefs().latestKnownVersion;



    if (m_hasUpdateRelease)
    {
        version = m_updateRelease.version.ToString();
    }

    if (m_updateIndicatorLine.empty())
    {
        m_updateIndicatorLine = UpdateIndicatorModel::PickLine (version, &ShellUpdater::GetRandomIndex);
        m_updateIndicator.SetRandomSource (&ShellUpdater::GetRandomIndex);
    }

    if (isShown && !m_updateIndicator.IsVisible())
    {
        m_updateIndicator.StartShimmerClock ((int64_t) GetTickCount64());
    }

    // An update waiting for Casso to close says so, and does not shimmer:
    // there is nothing left to draw the eye to.
    if (m_isUpdatePending)
    {
        m_updateIndicator.SetToolTipText (L"Casso " + TextEncoding::NarrowToWide (version) + L" installs when you close Casso");
        m_updateIndicator.SetText        (UpdateDialogModel::kpszPendingLine);
    }
    else
    {
        m_updateIndicator.SetToolTipText (L"Update available: Casso " + TextEncoding::NarrowToWide (version));
        m_updateIndicator.SetText        (m_updateIndicatorLine);
    }

    m_updateIndicator.SetQuiet (m_isUpdatePending);
    m_updateIndicator.SetVisible     (isShown);
    RefitUpdateIndicator (true);

    if (!isShown)
    {
        m_updateIndicator.SetHovered (false);
        m_updateIndicator.SetPressed (false);
    }

    m_host.RequestRepaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::OpenUpdateDialog
//
//  Opens the dialog for the release this session last found, and starts
//  fetching its notes. A download still running when the dialog closes is
//  canceled; an install already under way is left to finish.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::OpenUpdateDialog()
{
    constexpr int  kWidthDip     = 600;
    constexpr int  kHeightDip    = 560;
    constexpr int  kMinWidthDip  = 480;
    constexpr int  kMinHeightDip = 420;



    UpdateDialog              dlg;
    UpdateDialog::Callbacks   callbacks;
    DxuiWindow::CreateParams  params;
    UpdateService           * service   = GetUpdateService();
    ReleaseVersion            running   = GetRunningVersion();
    ReleaseArch               arch      = UpdateRuntime::GetRunningArch();
    std::wstring              pageUrl   = m_updateRelease.pageUrl.empty() ? std::wstring (UpdateService::kpszReleasesPage)
                                                                         : m_updateRelease.pageUrl;
    UpdateButtonSet           buttons   = UpdateButtonSet::ReleasePage;
    HRESULT                   hr        = S_OK;
    HRESULT                   hrNotes   = S_OK;
    int                       result    = 0;



    BAIL_OUT_IF (m_updateDialog != nullptr || !m_hasUpdateRelease, S_OK);
    CBRA (service != nullptr);

    buttons = UpdateDialogModel::SelectButtons (m_updateInstallType,
                                                UpdateDialogModel::HasAsset (m_updateRelease, m_updateInstallType, arch));

    callbacks.onUpdateNow = [this, service, &dlg, arch] ()
    {
        HRESULT  hrApply = S_OK;

        if (m_isUpdatePending)
        {
            ApplyPendingUpdateNow();
            return;
        }

        hrApply = service->StartApply (m_updateRelease, m_updateInstallType, arch);

        if (FAILED (hrApply))
        {
            dlg.ShowFailure (UpdateFailure::InstallFailed);
        }
    };

    callbacks.onUpdateWhenClosed = [this, service, &dlg, arch] ()
    {
        HRESULT  hrApply = service->StartApply (m_updateRelease, m_updateInstallType, arch, DeployTiming::WhenClosed);

        if (FAILED (hrApply))
        {
            dlg.ShowFailure (UpdateFailure::InstallFailed);
        }
    };

    callbacks.onCancelUpdate = [service] ()
    {
        service->CancelApply();
    };

    callbacks.onOpenUrl = [this] (const std::wstring & url)
    {
        OpenUrl (url);
    };

    callbacks.onTick = [service, &dlg] ()
    {
        std::uint64_t  done  = 0;
        std::uint64_t  total = 0;

        if (service->IsApplying())
        {
            service->GetProgress (done, total);
            dlg.ShowProgress (done, total);
        }
    };

    dlg.Configure (buttons,
                   UpdateDialogModel::PickOpener (&ShellUpdater::GetRandomIndex),
                   UpdateDialogModel::MakeAgeHeader (m_updateRelease.version, m_updateRelease.publishedDate, running, L""),
                   UpdateDialogModel::PickDeveloperNudge (&ShellUpdater::GetRandomIndex),
                   pageUrl,
                   m_isUpdatePending,
                   std::move (callbacks));

    params.title                    = UpdateDialogModel::kpszTitle;
    params.hInstance                = m_host.GetInstance();
    params.ownerHwnd                = m_host.GetOwnerWindow();
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::MaxClose;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    // Smaller than the default but no further: the opener, a wrapped header,
    // a few lines of notes, the link and the button row with Skip and the
    // primary side by side all still fit.
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };

    hr = dlg.Create (params);
    CHRA (hr);

    dlg.SetTheme (m_host.GetDialogTheme());

    m_updateDialog = &dlg;

    hrNotes = service->StartFetchNotes (m_updateRelease, running);
    IGNORE_RETURN_VALUE (hrNotes, S_OK);

    result = dlg.ShowModalDialog (dlg.GetDefaultCommandId());

    m_updateDialog = nullptr;

    service->CancelImages();

    if (dlg.IsBusy())
    {
        service->CancelApply();
    }

    if (result == UpdateDialog::kIdSkip)
    {
        SkipOfferedRelease();
    }

Error:
    m_updateDialog = nullptr;
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::SkipOfferedRelease
//
//  The skip is for this version only: a newer release shows the indicator
//  again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::SkipOfferedRelease()
{
    AdoptStoredUpdateFields();

    m_host.GetPrefs().skippedVersion = m_updateRelease.version.ToString();
    m_host.SavePrefs();

    ShowUpdateIndicator (false);
    RefreshSettingsUpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::ReportUpdateCheckFailure
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::ReportUpdateCheckFailure (UpdateFailure failure)
{
    DialogDefinition  def;



    def.title = UpdateDialogModel::kpszTitle;
    def.icon  = DialogIcon::Warning;
    def.body.push_back ({ UpdateDialogModel::MakeCheckFailedText (failure), false, L"" });
    def.body.push_back ({ L"", false, L"" });
    def.body.push_back ({ L"Open the release page", true, UpdateService::kpszReleasesPage });
    def.buttons.push_back ({ L"OK", 0, true, true });

    (void) m_host.ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::ReportUpToDate
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::ReportUpToDate()
{
    DialogDefinition  def;



    def.title = UpdateDialogModel::kpszTitle;
    def.icon  = DialogIcon::Info;
    def.body.push_back ({ UpdateDialogModel::MakeUpToDateText (GetRunningVersion()), false, L"" });
    def.buttons.push_back ({ L"OK", 0, true, true });

    (void) m_host.ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::OpenUrl
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::OpenUrl (const std::wstring & url)
{
    constexpr INT_PTR  kShellExecOk = 0;



    INT_PTR  rc = (INT_PTR) ShellExecuteW (nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);



    IGNORE_RETURN_VALUE (rc, kShellExecOk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::OfferMouseToUpdateIndicator
//
//  The caption does not route client input to its children, so the shell
//  drives the indicator: hover and its tooltip on a move, press on a down,
//  and the click on an up that lands where the press did. True when the
//  event was the indicator's.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellUpdater::OfferMouseToUpdateIndicator (DxuiMouseEventKind kind, int xPx, int yPx)
{
    constexpr int  kBaseDpi = 96;



    int64_t  nowMs      = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                              std::chrono::steady_clock::now().time_since_epoch()).count();
    UINT                                  dpi        = m_host.GetDpi();
    POINT                                 pointDip   = { MulDiv (xPx, kBaseDpi, (int) dpi), MulDiv (yPx, kBaseDpi, (int) dpi) };
    bool                                  hasCaption = m_host.HasCaption();
    bool                                  isInside   = hasCaption && m_updateIndicator.ContainsDip (pointDip);
    bool                                  isTaken    = false;
    UpdateIndicatorButton::PointerResult  pointer;
    RECT                                  bounds     = m_updateIndicator.GetBounds();
    DxuiDpiScaler                         scaler;
    RECT                                  anchorPx   = {};



    scaler.SetDpi (dpi);
    anchorPx = { scaler.ToPx (bounds.left), scaler.ToPx (bounds.top), scaler.ToPx (bounds.right), scaler.ToPx (bounds.bottom) };

    switch (kind)
    {
        case DxuiMouseEventKind::Move:
            pointer = m_updateIndicator.OnPointer (isInside, (int64_t) GetTickCount64());

            if (pointer.repaint)
            {
                m_host.RequestRepaint();
            }

            if (pointer.showTip)
            {
                m_host.ShowIndicatorTip (UpdateIndicatorButton::GetTipAnchorPx (anchorPx, DxuiTooltip::MeasurePointerExtent(), scaler.ToPx (UpdateIndicatorButton::kTipGapDip)),
                                         m_updateIndicator.GetToolTipText(),
                                         nowMs);
            }

            if (pointer.hideTip)
            {
                m_host.HideIndicatorTip();
            }

            isTaken = isInside;
            break;

        case DxuiMouseEventKind::Down:
            isTaken = isInside;

            if (isInside)
            {
                m_updateIndicator.SetPressed (true);
                m_host.HideIndicatorTip();
                m_host.RequestRepaint();
            }

            break;

        case DxuiMouseEventKind::Up:
            isTaken = m_updateIndicator.IsPressed();

            if (isTaken)
            {
                m_updateIndicator.SetPressed (false);
                m_host.RequestRepaint();
            }

            if (isTaken && isInside)
            {
                if (m_hasUpdateRelease)
                {
                    OpenUpdateDialog();
                }
                else
                {
                    CheckForUpdatesNow();
                }
            }

            break;

        default:
            break;
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::StopUpdateService
//
//  Shutdown: cancel and join the update workers while everything they use
//  still exists, and take the indicator out of the caption before the
//  member it points at is destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::StopUpdateService()
{
    if (m_updateRuntime != nullptr)
    {
        m_updateRuntime->GetService().Stop();
    }

    m_host.SetCaptionAccessory (nullptr);

    m_updateRuntime.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::RefitUpdateIndicator
//
//  Sizes the indicator for the caption it has: its text when there is room
//  beside a title of at least the minimum width, the arrow alone when not.
//  Runs when the indicator is shown and whenever the client width changes;
//  `force` skips the width-unchanged shortcut.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::RefitUpdateIndicator (bool force)
{
    constexpr int  kBaseDpi = 96;



    IndicatorFit  fit;
    int           widthPx   = 0;
    UINT          dpi       = m_host.GetDpi();



    if (!m_host.TryGetClientWidthPx (widthPx))
    {
        return;
    }

    if (!force && widthPx == m_indicatorClientPx)
    {
        return;
    }

    m_indicatorClientPx = widthPx;

    fit = UpdateIndicatorModel::Fit (MulDiv (widthPx, kBaseDpi, (int) dpi),
                                     m_host.GetCaptionReservedWidthDip(),
                                     m_updateIndicatorLine);

    m_updateIndicator.SetShowsText (fit.showsText);

    if (fit.widthDip != m_indicatorWidthDip)
    {
        m_indicatorWidthDip = fit.widthDip;
        m_host.SetCaptionAccessoryWidth (fit.widthDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::TickUpdateIndicator
//
//  Once per UI frame: refit on a width change, follow the system animation
//  setting, and advance the shimmer. True while the shimmer needs frames.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellUpdater::TickUpdateIndicator (int64_t nowMs)
{
    if (!m_updateIndicator.IsVisible())
    {
        return false;
    }

    RefitUpdateIndicator (false);
    m_updateIndicator.SetAnimationsEnabled (DxuiSystemSettings::Instance().AreAnimationsEnabled());

    return m_updateIndicator.TickShimmer (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::GetInstallDirectory
//
//  The folder Casso.exe runs from.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellUpdater::GetInstallDirectory()
{
    wchar_t       path[MAX_PATH] = {};
    DWORD         length         = GetModuleFileNameW (nullptr, path, MAX_PATH);
    std::wstring  dir (path, length);
    size_t        separator      = dir.find_last_of (L"\\/");



    return (separator == std::wstring::npos) ? std::wstring() : dir.substr (0, separator);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::HandlePendingUpdateAtLaunch
//
//  Carries out what PendingUpdateModel decides about an update that was
//  left to apply when Casso closed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::HandlePendingUpdateAtLaunch()
{
    PendingUpdate            pending;
    PendingLaunchAction      action;
    Win32UpdateFileSystem    fileSystem;
    UpdateService          * service     = GetUpdateService();
    HRESULT                  hr          = S_OK;
    bool                     isOtherOpen = false;



    //  The other app's staged files and pending record are not this launch's to
    //  judge. The first launch with no other app open reports on them.
    isOtherOpen = m_updateRuntime != nullptr && m_updateRuntime->IsOtherInstanceRunning();
    BAIL_OUT_IF (isOtherOpen, S_OK);

    pending.version = m_host.GetPrefs().pendingUpdateVersion;
    pending.kind    = m_host.GetPrefs().pendingUpdateKind;
    pending.failure = (UpdateFailure) m_host.GetPrefs().pendingUpdateFailure;

    action = PendingUpdateModel::DecideAtLaunch (pending, GetRunningVersion());

    if (action.showUpdated)
    {
        m_host.ShowNotice (UpdateDialogModel::MakeUpdatedNotice (pending.version));
    }

    if (action.failure != UpdateFailure::None)
    {
        m_host.ShowNotification (UpdateDialogModel::MakeUpdateFailedText (action.failure));
    }

    // The process that ran the old files exited before this one started.
    if (action.removeOldFiles && service != nullptr)
    {
        hr = service->StartCleanup (0);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (action.discardStaged)
    {
        hr = ZipUpdateInstaller::DiscardStaged (fileSystem, GetInstallDirectory());
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (action.clearPending)
    {
        ClearPendingUpdatePrefs();
    }
    else if (!pending.version.empty())
    {
        m_isUpdatePending   = true;
        m_pendingInstallType = InstallType::Msix;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::SetUpdatePending
//
//  Update when closed finished its download and checks: remember what to
//  apply, in the preferences too so the next launch can tell what happened.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::SetUpdatePending (const UpdateResult & result)
{
    bool  isZip = result.installType == InstallType::Zip;



    m_isUpdatePending    = true;
    m_pendingInstallType = result.installType;
    m_pendingInstallDir  = result.installDir;
    m_pendingPaths       = result.stagedPaths;
    m_pendingBundlePath  = result.bundlePath;

    AdoptStoredUpdateFields();

    m_host.GetPrefs().pendingUpdateVersion = m_updateRelease.version.ToString();
    m_host.GetPrefs().pendingUpdateKind    = isZip ? PendingUpdate::kpszZip : PendingUpdate::kpszMsix;
    m_host.GetPrefs().pendingUpdateFailure = 0;
    m_host.SavePrefs();

    if (m_updateDialog != nullptr)
    {
        m_updateDialog->ShowPending();
    }

    ShowUpdateIndicator (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::ClearPendingUpdatePrefs
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::ClearPendingUpdatePrefs()
{
    bool  isSet = false;



    AdoptStoredUpdateFields();

    isSet = !m_host.GetPrefs().pendingUpdateVersion.empty() || m_host.GetPrefs().pendingUpdateFailure != 0;

    m_host.GetPrefs().pendingUpdateVersion.clear();
    m_host.GetPrefs().pendingUpdateKind.clear();
    m_host.GetPrefs().pendingUpdateFailure = 0;

    if (isSet)
    {
        m_host.SavePrefs();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::ApplyPendingUpdateNow
//
//  Update now, for an update already waiting: a zip copy swaps its staged
//  files in and relaunches; a packaged one deploys the bundle at once, after
//  the same flush as any MSIX update.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::ApplyPendingUpdateNow()
{
    UpdateService  * service = GetUpdateService();
    HRESULT          hr      = S_OK;
    UpdateResult     ready;



    CBRA (service != nullptr);

    if (m_pendingInstallType == InstallType::Zip)
    {
        hr = service->StartCommitPending (m_pendingInstallDir, m_pendingPaths);
    }
    else if (!m_pendingBundlePath.empty())
    {
        ready.kind        = UpdateResultKind::ReadyToDeploy;
        ready.installType = InstallType::Msix;
        ready.bundlePath  = m_pendingBundlePath;
        HandleUpdateApplyResult (ready);
    }
    else
    {
        hr = E_UNEXPECTED;
    }

    if (FAILED (hr) && m_updateDialog != nullptr)
    {
        m_updateDialog->ShowFailure (UpdateFailure::InstallFailed);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellUpdater::CommitPendingUpdateAtExit
//
//  The swap an update applied when Casso closes was waiting for, run as the
//  shell shuts down, after the disks and the preferences are flushed. No
//  new Casso is started. A failure puts the old files back, and is saved
//  for the next launch to report; that launch also tells a success apart.
//  An MSIX update needs nothing here: Windows registers it once Casso exits.
//
////////////////////////////////////////////////////////////////////////////////

void ShellUpdater::CommitPendingUpdateAtExit()
{
    HRESULT                hr          = S_OK;
    UpdateFailure          failure     = UpdateFailure::None;
    Win32UpdateFileSystem  fileSystem;
    AuthenticodeVerifier   verifier;
    Win32UpdateHost        host;
    ZipUpdateInstaller     installer (fileSystem, verifier);
    bool                   isOtherOpen = false;



    BAIL_OUT_IF (!m_isUpdatePending || m_pendingInstallType != InstallType::Zip || m_pendingPaths.empty(), S_OK);

    isOtherOpen = host.IsOtherInstanceRunning();
    CBRF (!isOtherOpen, failure = UpdateFailure::OtherInstanceRunning);

    hr = installer.Commit (m_pendingInstallDir, m_pendingPaths, failure);
    CHRF (hr, failure = (failure == UpdateFailure::None) ? UpdateFailure::InstallFailed : failure);

Error:
    if (failure != UpdateFailure::None)
    {
        hr = ZipUpdateInstaller::DiscardStaged (fileSystem, m_pendingInstallDir);
        IGNORE_RETURN_VALUE (hr, S_OK);

        AdoptStoredUpdateFields();

        m_host.GetPrefs().pendingUpdateFailure = (int) failure;
        m_host.SavePrefs();
    }

    m_isUpdatePending = false;
}