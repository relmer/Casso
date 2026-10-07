#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Ui/Dialogs/UpdateDialog.h"
#include "Update/UpdateDialogModel.h"
#include "Update/UpdateSchedule.h"
#include "Ui/Chrome/UpdateIndicatorModel.h"
#include "Version.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetRunningVersion
//
////////////////////////////////////////////////////////////////////////////////

ReleaseVersion EmulatorShell::GetRunningVersion()
{
    return ReleaseVersion { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetRandomIndex
//
//  A uniform index below `count`, for picking the update dialog's remark.
//
////////////////////////////////////////////////////////////////////////////////

size_t EmulatorShell::GetRandomIndex (size_t count)
{
    std::random_device                     device;
    std::uniform_int_distribution<size_t>  pick (0, (count > 0) ? count - 1 : 0);



    return pick (device);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MakeUpdateHeader
//
//  The dialog's header with its one closing remark. The running build's
//  date, when the notes gave one, adds the age remarks to the pick.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::MakeUpdateHeader (const std::string & runningReleaseDate)
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
                                               &EmulatorShell::GetRandomIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetUpdateService
//
//  Built on first use, once there is a window to post results to.
//
////////////////////////////////////////////////////////////////////////////////

UpdateService * EmulatorShell::GetUpdateService()
{
    if (m_updateRuntime == nullptr && m_hwnd != nullptr)
    {
        m_updateRuntime = std::make_unique<UpdateRuntime> (m_hwnd, (UINT) WM_APP_UPDATE_RESULT);
    }

    return (m_updateRuntime != nullptr) ? &m_updateRuntime->GetService() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetAutoUpdateCheck
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetAutoUpdateCheck (bool enabled)
{
    m_globalPrefs.autoUpdateCheck = enabled;
    SaveGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::StartAutomaticUpdateCheck
//
//  Runs once, after the first frame is on screen, so nothing about updates
//  delays startup. Puts the indicator in the caption (hidden), starts the
//  removal of a zip update's old files when this launch came from one, and
//  then either starts the daily check or, when it is not due, shows the
//  indicator from what the last check found.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StartAutomaticUpdateCheck()
{
    UpdateService  * service  = nullptr;
    HRESULT          hr       = S_OK;
    ReleaseVersion   running  = GetRunningVersion();
    bool             isDue    = false;
    bool             isShown  = false;



    m_updateCheckStarted = true;

    service = GetUpdateService();
    BAIL_OUT_IF (service == nullptr, S_OK);

    if (m_host != nullptr)
    {
        m_host->SetCaptionAccessory (&m_updateIndicator);
    }

    if (m_cleanupOldPid != 0)
    {
        hr = service->StartCleanup (m_cleanupOldPid);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_wasLaunchedByUpdate)
    {
        ShowNotice (L"Casso was updated to version " _CRT_WIDE (VERSION_STRING) L".");
    }

    isDue = UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic,
                                        m_globalPrefs.autoUpdateCheck,
                                        m_globalPrefs.lastUpdateCheckUtc,
                                        UpdateRuntime::GetUtcNow());

    if (isDue)
    {
        hr = service->StartCheck (UpdateCheckTrigger::Automatic, running, m_globalPrefs.skippedVersion);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        isShown = m_globalPrefs.autoUpdateCheck &&
                  UpdateSchedule::ShouldShowIndicator (running, m_globalPrefs.latestKnownVersion, m_globalPrefs.skippedVersion);
        ShowUpdateIndicator (isShown);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::CheckForUpdatesNow
//
//  Help > Check for updates, and a click on the indicator before this
//  session has a release record. A check already in flight answers for
//  this one too; its result is then reported as a manual check's would be.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CheckForUpdatesNow()
{
    UpdateService  * service = GetUpdateService();
    HRESULT          hr      = S_OK;



    CBRA (service != nullptr);

    m_isManualCheckPending = true;

    hr = service->StartCheck (UpdateCheckTrigger::Manual, GetRunningVersion(), m_globalPrefs.skippedVersion);

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
//  EmulatorShell::HandleUpdateResult
//
//  The UI-thread end of every piece of update work.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleUpdateResult (UpdateResult & result)
{
    UpdateService  * service  = GetUpdateService();
    HRESULT          hrImages = S_OK;



    switch (result.kind)
    {
        case UpdateResultKind::Check:
            HandleUpdateCheckResult (result);
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
//  EmulatorShell::HandleUpdateCheckResult
//
//  A failed automatic check says nothing. A successful check is recorded
//  (time and version) whatever it found, so the daily limit and the
//  indicator at the next start both work from it. A manual check always
//  reports: the dialog for a newer release, even a skipped one, or a
//  message for up to date and for failure.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleUpdateCheckResult (UpdateResult & result)
{
    bool  isManual  = result.trigger == UpdateCheckTrigger::Manual || m_isManualCheckPending;
    bool  isOffered = isManual ? result.isNewer : result.isOffered;



    m_isManualCheckPending = false;

    if (result.failure != UpdateFailure::None)
    {
        if (isManual)
        {
            ReportUpdateCheckFailure (result.failure);
        }

        return;
    }

    m_globalPrefs.lastUpdateCheckUtc = result.checkedAtUtc;
    m_globalPrefs.latestKnownVersion = result.release.version.ToString();
    SaveGlobalPrefs();

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
//  EmulatorShell::HandleUpdateApplyResult
//
//  A bundle ready to deploy gets the normal exit flush first, because a
//  successful deploy ends this process without running the destructor. A
//  finished zip update closes the window the ordinary way, which flushes or
//  asks about unsaved disks as any exit does; the new Casso is already
//  starting and waits for this one to go.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleUpdateApplyResult (UpdateResult & result)
{
    HRESULT          hr      = S_OK;
    HRESULT          hrFlush = S_OK;
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
            ShowNotification (message);
        }

        return;
    }

    if (result.kind == UpdateResultKind::ReadyToDeploy)
    {
        if (m_updateDialog != nullptr)
        {
            m_updateDialog->ShowInstalling();
        }

        hrFlush = m_machine.GetDiskStore().FlushAllForShutdown();
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
        FlushDeferredGlobalPrefs();

        CBRA (service != nullptr);

        hr = service->StartDeploy (result.bundlePath);

        if (FAILED (hr) && m_updateDialog != nullptr)
        {
            m_updateDialog->ShowFailure (UpdateFailure::InstallFailed);
        }

        return;
    }

    if (m_updateDialog != nullptr)
    {
        m_updateDialog->ShowRestarting();
        m_updateDialog->EndDialog (UpdateDialog::kIdPrimary);
    }

    if (result.installType == InstallType::Zip)
    {
        PostMessageW (m_hwnd, WM_CLOSE, 0, 0);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowUpdateIndicator
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowUpdateIndicator (bool isShown)
{
    std::string  version = m_globalPrefs.latestKnownVersion;



    if (m_hasUpdateRelease)
    {
        version = m_updateRelease.version.ToString();
    }

    if (m_updateIndicatorLine.empty())
    {
        m_updateIndicatorLine = UpdateIndicatorModel::PickLine (version, &EmulatorShell::GetRandomIndex);
    }

    if (isShown && !m_updateIndicator.IsVisible())
    {
        m_updateIndicator.StartShimmerClock ((int64_t) GetTickCount64());
    }

    m_updateIndicator.SetToolTipText (L"Update available: Casso " + std::wstring (version.begin(), version.end()));
    m_updateIndicator.SetText        (m_updateIndicatorLine);
    m_updateIndicator.SetVisible     (isShown);
    RefitUpdateIndicator (true);

    if (!isShown)
    {
        m_updateIndicator.SetHovered (false);
        m_updateIndicator.SetPressed (false);
    }

    if (m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OpenUpdateDialog
//
//  Opens the dialog for the release this session last found, and starts
//  fetching its notes. A download still running when the dialog closes is
//  canceled; an install already under way is left to finish.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenUpdateDialog()
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
        HRESULT  hrApply = service->StartApply (m_updateRelease, m_updateInstallType, arch);

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
                   UpdateDialogModel::PickOpener (&EmulatorShell::GetRandomIndex),
                   UpdateDialogModel::MakeAgeHeader (m_updateRelease.version, m_updateRelease.publishedDate, running, L""),
                   UpdateDialogModel::MakeUpdateLabel (m_updateRelease.version),
                   UpdateDialogModel::PickDeveloperNudge (&EmulatorShell::GetRandomIndex),
                   pageUrl,
                   std::move (callbacks));

    params.title                    = UpdateDialogModel::kpszTitle;
    params.hInstance                = m_hInstance;
    params.ownerHwnd                = m_hwnd;
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

    dlg.SetTheme (&m_chromeTheme);

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
//  EmulatorShell::SkipOfferedRelease
//
//  The skip is for this version only: a newer release shows the indicator
//  again.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SkipOfferedRelease()
{
    m_globalPrefs.skippedVersion = m_updateRelease.version.ToString();
    SaveGlobalPrefs();

    ShowUpdateIndicator (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReportUpdateCheckFailure
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReportUpdateCheckFailure (UpdateFailure failure)
{
    DialogDefinition  def;



    def.title = UpdateDialogModel::kpszTitle;
    def.icon  = DialogIcon::Warning;
    def.body.push_back ({ UpdateDialogModel::MakeCheckFailedText (failure), false, L"" });
    def.body.push_back ({ L"", false, L"" });
    def.body.push_back ({ L"Open the release page", true, UpdateService::kpszReleasesPage });
    def.buttons.push_back ({ L"OK", 0, true, true });

    (void) ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReportUpToDate
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReportUpToDate()
{
    DialogDefinition  def;



    def.title = UpdateDialogModel::kpszTitle;
    def.icon  = DialogIcon::Info;
    def.body.push_back ({ UpdateDialogModel::MakeUpToDateText (GetRunningVersion()), false, L"" });
    def.buttons.push_back ({ L"OK", 0, true, true });

    (void) ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OpenUrl
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenUrl (const std::wstring & url)
{
    constexpr INT_PTR  kShellExecOk = 0;



    INT_PTR  rc = (INT_PTR) ShellExecuteW (nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);



    IGNORE_RETURN_VALUE (rc, kShellExecOk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OfferMouseToUpdateIndicator
//
//  The caption does not route client input to its children, so the shell
//  drives the indicator: hover and its tooltip on a move, press on a down,
//  and the click on an up that lands where the press did. True when the
//  event was the indicator's.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OfferMouseToUpdateIndicator (DxuiMouseEventKind kind, int xPx, int yPx)
{
    constexpr int  kBaseDpi = 96;



    int64_t  nowMs      = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                              std::chrono::steady_clock::now().time_since_epoch()).count();
    UINT     dpi        = m_scaler.GetDpi();
    POINT    pointDip   = { MulDiv (xPx, kBaseDpi, (int) dpi), MulDiv (yPx, kBaseDpi, (int) dpi) };
    bool     hasCaption = m_host != nullptr && m_host->GetCaptionHeightPx() > 0;
    bool     isInside   = hasCaption && m_updateIndicator.ContainsDip (pointDip);
    bool     isTaken    = false;
    RECT     bounds     = m_updateIndicator.GetBounds();
    RECT     anchorPx   = { m_scaler.ToPx (bounds.left),  m_scaler.ToPx (bounds.top),
                            m_scaler.ToPx (bounds.right), m_scaler.ToPx (bounds.bottom) };



    switch (kind)
    {
        case DxuiMouseEventKind::Move:
            if (m_updateIndicator.SetHovered (isInside))
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);

                if (isInside)
                {
                    m_captionTooltip.RequestShow (anchorPx, m_updateIndicator.GetToolTipText().c_str(), nowMs);
                }
                else
                {
                    m_captionTooltip.RequestHide (nowMs);
                }
            }

            isTaken = isInside;
            break;

        case DxuiMouseEventKind::Down:
            isTaken = isInside;

            if (isInside)
            {
                m_updateIndicator.SetPressed (true);
                m_captionTooltip.HideImmediate();
                InvalidateRect (m_hwnd, nullptr, FALSE);
            }

            break;

        case DxuiMouseEventKind::Up:
            isTaken = m_updateIndicator.IsPressed();

            if (isTaken)
            {
                m_updateIndicator.SetPressed (false);
                InvalidateRect (m_hwnd, nullptr, FALSE);
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
//  EmulatorShell::StopUpdateService
//
//  Shutdown: cancel and join the update workers while everything they use
//  still exists, and take the indicator out of the caption before the
//  member it points at is destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StopUpdateService()
{
    if (m_updateRuntime != nullptr)
    {
        m_updateRuntime->GetService().Stop();
    }

    if (m_host != nullptr)
    {
        m_host->SetCaptionAccessory (nullptr);
    }

    m_updateRuntime.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RefitUpdateIndicator
//
//  Sizes the indicator for the caption it has: its text when there is room
//  beside a title of at least the minimum width, the arrow alone when not.
//  Runs when the indicator is shown and whenever the client width changes;
//  `force` skips the width-unchanged shortcut.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RefitUpdateIndicator (bool force)
{
    constexpr int  kBaseDpi = 96;



    RECT          client    = {};
    IndicatorFit  fit;
    int           widthPx   = 0;
    UINT          dpi       = m_scaler.GetDpi();



    if (m_host == nullptr || m_hwnd == nullptr || GetClientRect (m_hwnd, &client) == FALSE)
    {
        return;
    }

    widthPx = client.right - client.left;

    if (!force && widthPx == m_indicatorClientPx)
    {
        return;
    }

    m_indicatorClientPx = widthPx;

    fit = UpdateIndicatorModel::Fit (MulDiv (widthPx, kBaseDpi, (int) dpi),
                                     m_host->GetCaptionReservedWidthDip(),
                                     m_updateIndicatorLine);

    m_updateIndicator.SetShowsText (fit.showsText);

    if (fit.widthDip != m_indicatorWidthDip)
    {
        m_indicatorWidthDip = fit.widthDip;
        m_host->SetCaptionAccessoryWidth (fit.widthDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TickUpdateIndicator
//
//  Once per UI frame: refit on a width change, follow the system animation
//  setting, and advance the shimmer. True while the shimmer needs frames.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TickUpdateIndicator (int64_t nowMs)
{
    if (!m_updateIndicator.IsVisible())
    {
        return false;
    }

    RefitUpdateIndicator (false);
    m_updateIndicator.SetAnimationsEnabled (DxuiSystemSettings::Instance().AreAnimationsEnabled());

    return m_updateIndicator.TickShimmer (nowMs);
}