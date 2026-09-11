#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Cassque/Model/KnownFolderStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Mount  (IDriveCommandSink)
//
//  IDriveCommandSink override delegates straight through to the
//  DiskManager so the chrome / drag-drop entry points and the manager
//  share a single mount path.
//
//  The HRESULT it returns says only that the mount was queued, never that it
//  worked: the mount itself runs later, on the CPU thread. Recording the disk
//  here used to read that as success and put a file the loader would go on to
//  refuse into the picker's recent list. The recording moved to the mount's
//  own completion, which is the first place the answer is known.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Mount (int slot, int drive, const std::wstring & path)
{
    HRESULT  hr = S_OK;



    hr = m_diskManager->Mount (slot, drive, path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordRecentDisk
//
//  Push a successfully-mounted disk image onto the recent-disks MRU
//  and persist the updated prefs. Best-effort; failures are swallowed
//  so an MRU write hiccup never blocks a successful mount.
//
//  The mount's own HRESULT goes to DiskMru rather than being tested here,
//  so the "only a mount that happened counts" rule lives with the list it
//  protects instead of with whoever remembered to check.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordRecentDisk (const std::wstring & path, HRESULT mountResult)
{
    HRESULT                    hr         = S_OK;
    DiskMru                    mru;
    std::filesystem::path      fsPath;
    std::vector<std::string>   serialized;
    std::vector<std::int64_t>  loadedAt;
    std::int64_t               nowUnix    = 0;



    BAIL_OUT_IF (path.empty(), S_OK);

    nowUnix = (std::int64_t) std::chrono::duration_cast<std::chrono::seconds> (
                  std::chrono::system_clock::now().time_since_epoch()).count();

    fsPath = std::filesystem::path (path);
    mru    = DiskMru::FromUtf8 (m_globalPrefs.recentDisks, m_globalPrefs.recentDiskLoadedAt);
    mru.RecordMountResult (mountResult, fsPath, nowUnix);
    mru.ToUtf8 (serialized, loadedAt);
    m_globalPrefs.recentDisks        = std::move (serialized);
    m_globalPrefs.recentDiskLoadedAt = std::move (loadedAt);

    SaveGlobalPrefs();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnMountCompleted
//
//  Every attempted mount ends here with its own HRESULT, and the only job
//  this half has is getting that onto the UI thread.
//
//  It posts rather than acting, in both directions. A mount the user started
//  ran on the CPU thread, and the reaction raises Dxui modals, which assert
//  UI-thread affinity. A command-line mount ran on the UI thread but did so
//  inside Initialize, before the message loop exists to service a modal, so
//  acting there would park startup behind a dialog with nothing running
//  behind it. Posting covers both, and the posted messages arrive in mount
//  order, which is what puts the boot disk at the top of the recent list.
//
//  With no window, or with the post refused, the fallback is to handle it
//  inline: ShowNotification queues rather than shows when there is nothing to
//  parent a dialog to, so the report survives either way.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnMountCompleted (int drive, const std::string & path, HRESULT mountResult,
                                      const MountDiagnosis & diagnosis)
{
    MountCompletion *  carried  = nullptr;
    MountCompletion    fallback;
    bool               isPosted = false;



    fallback.path      = path;
    fallback.diagnosis = diagnosis;
    fallback.result    = mountResult;
    fallback.drive     = drive;

    if (m_hwnd != nullptr)
    {
        carried = new (std::nothrow) MountCompletion (fallback);
    }

    if (carried != nullptr)
    {
        isPosted = (PostMessageW (m_hwnd, WM_APP_MOUNT_COMPLETED, 0,
                                  reinterpret_cast<LPARAM> (carried)) != FALSE);

        if (!isPosted)
        {
            delete carried;
        }
    }

    if (!isPosted)
    {
        HandleMountCompletion (fallback);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HandleMountCompletion
//
//  The UI-thread half. A mount that worked joins the recent-disks list and is
//  checked for a damaged image; a mount that did not is reported to the user
//  and joins nothing.
//
//  The failure report goes through EhmNotifyUser like every other user-facing
//  refusal in the tree, and for the same reason: an image the loader will not
//  take is bad input, not a bug in Casso, so it earns a sentence and not an
//  assert.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleMountCompletion (const MountCompletion & completion)
{
    std::wstring  message;
    HWND          replyTo   = nullptr;
    bool          handedOff = m_intentReplies.TryTakeInsert (completion.drive, completion.path, replyTo);



    RecordRecentDisk (fs::path (completion.path).wstring(), completion.result);

    if (SUCCEEDED (completion.result))
    {
        if (handedOff)
        {
            RecordKnownFolder (completion.path);
        }

        if (replyTo != nullptr)
        {
            SendIntentReply (replyTo, IntentReplyTracker::MakeInsertReply (completion.result, std::string()));
        }

        ReportDamagedMount (completion.drive);
        return;
    }

    message = DiskImageStore::FormatMountFailureMessage (completion.path, completion.diagnosis);

    //  A tool that asked is told why instead; the dialog would sit behind the
    //  window the user is looking at.
    if (replyTo != nullptr)
    {
        SendIntentReply (replyTo, IntentReplyTracker::MakeInsertReply (completion.result,
                                                                       TextEncoding::WideToNarrow (message)));
        return;
    }

    EhmNotifyUser (message.c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SendIntentReply
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SendIntentReply (HWND target, const Win32IntentChannel::Reply & reply)
{
    bool  delivered = Win32IntentChannel::SendTo (target, m_hwnd, Win32IntentChannel::GetReplyMessageId(),
                                                  Win32IntentChannel::EncodeReply (reply));



    IGNORE_RETURN_VALUE (delivered, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PostIntentReply
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostIntentReply (HWND target, const Win32IntentChannel::Reply & reply)
{
    IntentReplyPost *  carried = new (std::nothrow) IntentReplyPost { target, reply };



    if (carried == nullptr)
    {
        return;
    }

    if (m_hwnd == nullptr || !PostMessageW (m_hwnd, WM_APP_INTENT_REPLY, 0, (LPARAM) carried))
    {
        delete carried;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RecordKnownFolder
//
//  Best effort: a hand-off that mounted is not undone because the list could
//  not be written.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordKnownFolder (const std::string & imagePath)
{
    Win32FileSystem   fsKnown;
    KnownFolderStore  store (fsKnown, AssetBootstrap::GetAssetBaseDirectory().wstring());
    std::wstring      folder  = fs::path (imagePath).parent_path().wstring();
    int64_t           nowUnix = (int64_t) std::chrono::duration_cast<std::chrono::seconds> (
                                    std::chrono::system_clock::now().time_since_epoch()).count();
    HRESULT           hr      = S_OK;



    BAIL_OUT_IF (folder.empty(), S_OK);

    hr = store.Append (folder, nowUnix);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InstallIntentReplies
//
//  A reload a tool asked for is decided on the thread that owns disk writes;
//  the answer is composed there and carried to the UI thread to be sent.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InstallIntentReplies()
{
    m_machine.GetDiskStore().SetDecisionSink ([this] (const std::string & path, ChangeAction action,
                                                      bool guestCopyPreserved, const std::string & preservedPath)
    {
        HWND                       replyTo = nullptr;
        Win32IntentChannel::Reply  reply;

        if (!m_intentReplies.TryTakeReload (path, replyTo) || replyTo == nullptr)
        {
            return;
        }

        if (IntentReplyTracker::TryMakeReloadReply (action, guestCopyPreserved, preservedPath, reply))
        {
            PostIntentReply (replyTo, reply);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject  (IDriveCommandSink)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::Eject (int slot, int drive)
{
    m_diskManager->Eject (slot, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowseForDisk
//
//  UI helper: start the drive-door-open animation and show the disk
//  picker IN PARALLEL. The picker's modal GetMessage loop owns the UI
//  thread, so RunMessageLoop stops driving frames; the host's modal
//  keep-alive tick (the same one that carries chrome through an OS
//  move / size loop) drives TryPresentUiFrame for the dialog's whole
//  lifetime, so the door visibly opens behind the picker instead of
//  stalling until it is dismissed. This replaces a blocking pre-dialog
//  wait that pumped the full animation before the picker appeared --
//  dead time at best, and a frozen door whenever the present gate
//  declined the frames.
//
//  Mount-on-success runs through DiskManager::Mount, which queues
//  to the CPU thread and posts a DoorClose sync event picked up
//  by UpdateDriveWidgets -- so we don't need to touch the door on
//  the success path here; BeginInsert closes it naturally. Cancel
//  restores the door to match the mount state: a mounted drive
//  closes back, an empty drive rests open (matches a real Disk II).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseForDisk (int drive)
{
    DriveWidgetState *  pSt          = nullptr;
    HRESULT             hrBrowse     = S_OK;
    bool                mountStarted = false;



    using std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto  nowMs = []() -> int64_t {
        return (int64_t) duration_cast<milliseconds> (steady_clock::now().time_since_epoch()).count();
    };



    if (drive < 0 || drive >= (int) m_driveWidgetState.size())
    {
        return;
    }

    pSt = &m_driveWidgetState[drive];

    // The time base MUST match DiskManager::GetNowMs (steady_clock ms):
    // TickDoorAnimation diffs the current frame time against
    // animationStartTimeMs. An empty drive rests with its door already
    // Open, so StartDoorTransition is a no-op there.
    pSt->StartDoorTransition (DriveWidgetState::Door::Opening, nowMs());
    m_d3dRenderer.MarkRedrawNeeded();

    // The keep-alive spans the whole modal picker (including its nested
    // IFileOpenDialog when the user clicks Browse...), animating the door
    // and keeping the printer preview live behind the dialog.
    m_host->BeginModalKeepAlive();

    hrBrowse = m_windowCommandManager->PromptInsertDiskMru (drive + 1, mountStarted);
    IGNORE_RETURN_VALUE (hrBrowse, S_OK);

    m_host->EndModalKeepAlive();

    // No-mount path (cancel or failure): the door follows the mount
    // state -- a mounted drive closes back, an empty drive rests open
    // (matches a real Disk II). mountStarted is the discriminator
    // because a cancel returns S_OK by design. When a mount DID start,
    // the door is left alone: the queued mount completes on the CPU
    // thread and BeginInsert runs the close choreography.
    if (!mountStarted && pSt->IsMounted())
    {
        pSt->StartDoorTransition (DriveWidgetState::Door::Closing, nowMs());
        m_d3dRenderer.MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowSalvageDialog
//
//  Shows a dialog whose body is a caller-built panel instead of wrapped text
//  runs. The salvage dialog needs a figures table and a warning banner, and
//  neither survives being expressed as a string: a table spaced with padding
//  characters comes apart at any font or DPI other than the author's.
//
//  Wider than the standard dialog because the table has three columns.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowSalvageDialog (const DialogDefinition             &  def,
                                      std::unique_ptr<SalvageDialogContent>  content)
{
    constexpr int  s_kDialogWidthDip  = 520;
    constexpr int  s_kChromeHeightDip = 108;   // caption + content pad*2 + button row
    constexpr int  s_kMinHeightDip    = 160;
    constexpr int  s_kMaxHeightDip    = 620;



    MessageDialog                       dlg;
    DxuiWindow::CreateParams            params;
    std::vector<MessageDialog::Button>  buttons;
    HRESULT                             hr        = S_OK;
    int                                 heightDip = 0;
    int                                 result    = -1;



    CBRAEx (content != nullptr, E_INVALIDARG);

    // The panel measures itself once laid out; until then its preferred
    // height is the estimate it reported for the width we are about to give
    // it, which is why the width is fixed above rather than derived.
    heightDip = std::clamp (s_kChromeHeightDip + content->GetPreferredHeightDip(),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    for (const DialogButton & button : def.buttons)
    {
        buttons.push_back ({ button.label, button.resultCode, button.isDefault, button.isCancel });
    }

    dlg.Configure (std::move (content), std::move (buttons), def.closeBoxResult.value_or (-1));

    params.title                    = def.title;
    params.hInstance                = m_hInstance;
    params.ownerHwnd                = m_hwnd;
    params.initialSizeDip           = { s_kDialogWidthDip, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    // Centered on the emulator window rather than on the OS cascade, which
    // ignores the owner. A dialog asking what to do about the disk in the
    // machine belongs over the machine, not wherever the cascade reached.
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dlg.Create (params);
    CHRA (hr);

    dlg.SetTheme (&m_chromeTheme);

    result = dlg.TranslateResult (dlg.ShowModalDialog (dlg.GetDefaultCommandId()));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsSalvageOffered
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsSalvageOffered (int drive)
{
    // Reads the verdict reached at mount rather than re-deriving it. This runs
    // from the menu's enable query, so it runs on every draw of that menu:
    // assessing here cost 11 ms for an ordinary disk and 154 ms for a
    // copy-protected one, per drive, on the UI thread.
    return m_machine.GetDiskStore().IsSalvageOffered (6, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RunSalvageFlow
//
//  Assess, show the figures, write on confirmation, then offer to insert the
//  copy. The assessment is shown BEFORE anything is written: a lossy copy is
//  the user's decision to make with the numbers in front of them, not one to
//  learn about afterwards.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunSalvageFlow (int drive)
{
    SalvageAssessment                       assessment;
    DenibblizeReport                        report;
    DialogDefinition                        def;
    std::unique_ptr<SalvageDialogContent>   content;
    HRESULT                                 hr          = S_OK;
    int                                     choice      = 0;
    bool                                    isOffThread = false;
    std::wstring                            sourcePath;
    std::wstring                            destName;
    std::wstring                            summary;



    // The Disk menu dispatches on the CPU thread and this builds a Dxui modal.
    // Reached from the damage prompt it is already on the UI thread; reached
    // from the menu it was not, so the dialog never appeared and the command
    // looked like it did nothing at all.
    isOffThread = (m_hwnd != nullptr) &&
                  (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

    if (isOffThread)
    {
        PostMessageW (m_hwnd, WM_APP_RUN_SALVAGE, (WPARAM) drive, 0);
        return;
    }

    hr = m_machine.GetDiskStore().AssessSalvage (6, drive, assessment);
    if (FAILED (hr))
    {
        return;
    }

    if (!assessment.isOffered)
    {
        return;
    }

    sourcePath = fs::path (m_machine.GetDiskStore().GetSourcePath (6, drive)).wstring();
    destName   = fs::path (assessment.suggestedPath).filename().wstring();

    content = std::make_unique<SalvageDialogContent>();
    content->SetAssessment (sourcePath, destName, assessment);

    def.title = L"Salvage readable sectors";
    def.buttons.push_back (DialogButton { L"Salvage", 1, true,  false, false });
    def.buttons.push_back (DialogButton { L"Cancel",  0, false, true,  false });

    choice = ShowSalvageDialog (def, std::move (content));
    if (choice != 1)
    {
        return;
    }

    hr = m_machine.GetDiskStore().SalvageToFile (6, drive, assessment.suggestedPath, report);

    if (FAILED (hr))
    {
        DialogDefinition  failed;

        failed.title = L"Could not write the salvaged copy";
        failed.icon  = DialogIcon::Error;
        failed.body.push_back (DialogTextRun {
            fs::path (assessment.suggestedPath).wstring() + L"\n\n"
            L"The original disk image was not changed.\n\n" +
            WindowCommandManager::FormatSystemError (hr), false, std::wstring() });
        failed.buttons.push_back (DialogButton { L"OK", 0, true, true, false });

        ShowModalDialog (failed);
        return;
    }

    // Result first, then the question: the counts are what happened, not part
    // of the prompt.
    summary = L"Salvaged copy written to:\n\n" +
              fs::path (assessment.suggestedPath).wstring() + L"\n\n" +
              std::to_wstring (report.sectorsRecovered) + L" sectors recovered, " +
              std::to_wstring (report.sectorsLost) + L" lost. The original disk "
              L"image was not changed.\n\n"
              L"Insert the salvaged copy into drive " + std::to_wstring (drive + 1) + L"?";

    def = DialogDefinition();
    def.title = L"Salvage complete";
    def.body.push_back (DialogTextRun { summary, false, std::wstring() });
    def.buttons.push_back (DialogButton { L"Insert",  1, true,  false, false });
    def.buttons.push_back (DialogButton { L"Not now", 0, false, true,  false });

    choice = ShowModalDialog (def);

    if (choice == 1)
    {
        HRESULT  hrMount = m_diskManager->MountDiskInSlot6 (drive, assessment.suggestedPath);

        IGNORE_RETURN_VALUE (hrMount, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReportDamagedMount
//
//  The damage report, with salvage offered inline so the dialog is not a dead
//  end. Reached after every mount; silent unless the image failed its stored
//  checksum.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReportDamagedMount (int drive)
{
    DiskImage          * image       = m_machine.GetDiskStore().GetImage (6, drive);
    SalvageAssessment    assessment;
    DialogDefinition     def;
    HRESULT              hr          = S_OK;
    int                  choice      = 0;
    bool                 isOffThread = false;



    if (image == nullptr)
    {
        return;
    }

    // Mounts run on the CPU thread -- the picker and the menu both route
    // through it so a flush never races the drive engine -- and this raises a
    // modal. Bounce to the UI thread rather than building a dialog from there.
    isOffThread = (m_hwnd != nullptr) &&
                  (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

    if (isOffThread)
    {
        PostMessageW (m_hwnd, WM_APP_REPORT_DAMAGE, (WPARAM) drive, 0);
        return;
    }

    if (!image->HasSourceCrcMismatch())
    {
        return;
    }

    def.title = L"Disk image is damaged";
    def.icon  = DialogIcon::Warning;
    // The sentence leads and the path follows it: naming the file before
    // saying anything about it makes the reader hold a path in mind with no
    // reason to yet.
    def.body.push_back (DialogTextRun {
        L"This disk image's stored checksum does not match its contents. "
        L"The file is damaged or was written by a tool that miscomputed it.\n\n" +
        fs::path (m_machine.GetDiskStore().GetSourcePath (6, drive)).wstring() + L"\n\n"
        L"Casso has loaded it so you can read it, and has write-protected it "
        L"for this session. Rewriting the file would give it a newly computed "
        L"checksum, silently hiding the damaged sectors.",
        false, std::wstring() });

    hr = m_machine.GetDiskStore().AssessSalvage (6, drive, assessment);

    if (SUCCEEDED (hr) && assessment.isOffered)
    {
        def.buttons.push_back (DialogButton { L"Salvage readable sectors...", 1,
                                              false, false, false });
    }

    def.buttons.push_back (DialogButton { L"OK", 0, true, true, false });

    choice = ShowModalDialog (def);

    if (choice == 1)
    {
        RunSalvageFlow (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsWriteProtectToggleOffered
//
//  Whether the Disk menu should offer the write-protect toggle for a drive.
//  Reads the bay, then defers to the pure predicate so the rule itself stays
//  testable without a mounted store behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsWriteProtectToggleOffered (int drive)
{
    const DiskImage *  image   = m_machine.GetDiskStore().GetImage (6, drive);
    bool               mounted = m_machine.GetDiskStore().IsMounted (6, drive);



    if (image == nullptr)
    {
        return false;
    }

    return ShouldEnableWriteProtectMenuItem (mounted, image->GetWriteProtectInfo());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveUserWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveUserWriteProtect (int drive, bool wp)
{
    DiskImage *  image = nullptr;



    if (drive < 0 || drive >= (int) m_userWriteProtect.size())
    {
        return;
    }

    m_userWriteProtect[(size_t) drive] = wp;

    // Apply to whatever is mounted right now so the toggle takes effect
    // without a remount; a later mount re-applies the standing preference
    // via DiskManager::MountDiskInSlot6.
    image = m_machine.GetDiskStore().GetImage (6, drive);

    if (image != nullptr)
    {
        image->SetUserWriteProtected (wp);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InstallChangeReporting
//
//  Gives the image store the two ways it has of reaching the user.
//
//  BOTH BOUNCE TO THE UI THREAD, because both are called from the thread that
//  owns disk writes -- the store decides there, at a moment with nothing in
//  flight -- and neither a panel-tree edit nor a modal may be built from it.
//
//  NEITHER SINK DECIDES ANYTHING. What to say, which answers exist and what
//  each one means all arrive composed; the shell shows them and reports which
//  was chosen.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InstallChangeReporting()
{
    m_machine.GetDiskStore().SetChangeReportSink ([this] (int slot, int drive, const ChangePrompt & prompt)
    {
        ChangeNotice *  carried = new ChangeNotice { slot, drive, prompt };

        if (m_hwnd == nullptr ||
            !PostMessageW (m_hwnd, WM_APP_CHANGE_REPORT, 0, (LPARAM) carried))
        {
            delete carried;
        }
    });

    //  THE POST IS THE DELIVERY, so whether it succeeded is what the store is
    //  told. This sink is installed before the window exists and posting can
    //  fail on a full queue, and a bay left believing a question is on screen
    //  that nobody ever saw is a bay nothing acts on again until it is ejected.
    m_machine.GetDiskStore().SetAskSink ([this] (int slot, int drive, const ChangePrompt & prompt) -> bool
    {
        ChangeNotice *  carried = new ChangeNotice { slot, drive, prompt };

        if (m_hwnd == nullptr ||
            !PostMessageW (m_hwnd, WM_APP_CHANGE_ASK, 0, (LPARAM) carried))
        {
            delete carried;

            return false;
        }

        return true;
    });

    //  THE LAST-CHANCE ROUTE, and the only one the store has once the message
    //  loop has gone. It runs on this thread, inside the apartment OleInitialize
    //  set up, so the picker works exactly as it does from a question -- and
    //  unlike a question, this returns the answer rather than posting for it.
    m_machine.GetDiskStore().SetRescueSink ([this] (const std::string & imagePath,
                                       std::string & outPath) -> bool
    {
        std::wstring  chosen;

        if (!AskWhereToSaveLostDisk (imagePath, chosen))
        {
            return false;
        }

        outPath = fs::path (chosen).string();

        return true;
    });

    m_changeBanner.SetSeverity (DxuiInfoBanner::Severity::Info);
    m_changeBanner.SetVisible  (false);

    //  EVERY BUTTON ON THE STRIP DISMISSES IT, and that is all any of them
    //  does. Only questions carry answers worth acting on, and questions go to
    //  a dialog -- the strip routes nothing back, which is why a lost-file
    //  prompt sent here had two buttons that did nothing. Dismissing it and
    //  its countdown running out are the same thing.
    m_changeBanner.SetOnAction ([this] (size_t index)
    {
        UNREFERENCED_PARAMETER (index);

        HideChangeBanner();
    });

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowChangeBanner
//
//  Raises the non-modal notice over the running machine.
//
//  NOTHING IN THE TREE HOSTED ONE BEFORE. The banner appears inside dialogs and
//  on a settings page; over a running machine it has to be positioned against
//  the emulator viewport and left there, which is what this does.
//
//  A NOTICE WITH NOTHING TO OFFER IS NOT SHOWN. The restart-already-happened
//  report carries no action, and a strip that only says what already occurred
//  would sit over the picture until dismissed for no gain.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowChangeBanner (const ChangeNotice & notice)
{
    std::vector<std::wstring>  labels;
    size_t                     i = 0;



    for (i = 0; i < notice.prompt.answers.size(); i++)
    {
        labels.push_back (notice.prompt.answers[i].label);
    }

    m_changeBanner.SetText    (notice.prompt.message);
    m_changeBanner.SetActions (labels);

    //  RE-ARMED ON EVERY CHANGE, not only the first. A later change re-words
    //  the strip already on screen, and a countdown left running from the
    //  previous one would take the new wording away early.
    m_changeBannerHideAtMs = notice.prompt.selfDismisses
                                 ? (ChangeBannerNowMs() + s_kChangeBannerHoldMs)
                                 : 0;
    m_changeBannerTickMs   = ChangeBannerNowMs();

    //  Replaced rather than stacked: a standing report absorbs later changes,
    //  so a second one re-words the strip already on screen.
    m_changeBanner.SetVisible (!labels.empty());

    //  The band just changed height, so everything below it moves and the
    //  picture is rescaled into what is left. Nothing here positions the
    //  notice: the dock does, and this is the pass that runs it.
    ReflowChromeForChangeBand();

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReflowChromeForChangeBand
//
//  Re-docks everything after the notice appears or goes.
//
//  THE WINDOW KEEPS ITS SIZE. The machine-change reflow beside this one grows
//  and shrinks the window, because a machine with no disk drives genuinely
//  needs less of it and the user keeps that size for the session. A notice is
//  transient: the picture gives up the height while it is up and takes it back
//  when it goes, which is what makes the strip read as sliding in over the
//  scene rather than shoving the window about.
//
//  RUN THROUGH OnSize, which is the one authoritative layout pass. A second
//  path that re-docked some of the chrome would be a second answer to where
//  everything goes.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReflowChromeForChangeBand()
{
    RECT  client = {};



    DXUI_ASSERT_UI_THREAD();   // chrome layout: never from the CPU thread

    //  NEVER FROM INSIDE THE PASS IT RUNS. Losing the pointer capture re-docks,
    //  and the capture is dropped from OnCancelMode / OnKillFocus, which a
    //  resize itself can raise -- so the layout would call itself.
    if (m_inChromeLayout || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        return;
    }

    {
        DxuiMessageResult  sized = OnSize (client.right - client.left,
                                           client.bottom - client.top);

        IGNORE_RETURN_VALUE (sized, DxuiMessageResult::Handled);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetChangeBandThicknessPx
//
//  How tall the notice's band is.
//
//  ZERO WHEN NOTHING IS BEING REPORTED, which is what makes this cost the
//  ordinary session nothing: the dock hands the Fill center the whole space and
//  every other band lands where it always did.
//
//  MEASURED AGAINST THE CLIENT WIDTH, because that is the width the band gets.
//  Measuring against the emulator viewport is what put the text off the screen:
//  the picture keeps its own aspect and can be wider than the window it is in.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::GetChangeBandThicknessPx (int clientWidthPx) const
{
    float  height = 0.0f;



    if (!m_changeBanner.IsVisible() || clientWidthPx <= 0)
    {
        return 0;
    }

    height = m_changeBanner.GetPreferredHeightPx ((float) clientWidthPx, m_scaler);

    return (int) height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LayoutChangeBanner
//
//  Lays the notice into the band the dock gave it.
//
//  IT TAKES THE BAND'S BOUNDS RATHER THAN COMPUTING ITS OWN. The band already
//  spans the client and already has the height this asked for, so anything
//  computed here a second time would be a second answer to a settled question.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutChangeBanner()
{
    RECT  bounds = m_changeBand.GetBounds();



    if (!m_changeBanner.IsVisible() || bounds.right <= bounds.left)
    {
        return;
    }

    m_changeBanner.Layout (bounds, m_scaler);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::AskAboutChange
//
//  Puts the store's question to the user.
//
//  THE TEXT AND THE ANSWERS COME FROM CORE. This builds a dialog out of them
//  and nothing else -- it does not decide which answers exist, what they say,
//  or what any of them means.
//
//  THE ANSWER GOES BACK BY COMMAND. Acting on it swaps an image, which belongs
//  to the thread that owns disk writes, so it travels the same route every
//  other mount-path action does rather than being carried out here.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AskAboutChange (const ChangeNotice & notice)
{
    DialogDefinition  def;
    size_t            i          = 0;
    int               choice     = 0;
    ChangeAction      chosen     = ChangeAction::Ignore;
    std::string       saveTarget;



    if (notice.prompt.answers.empty())
    {
        return;
    }

    def.title = notice.prompt.title;
    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { notice.prompt.message, false, std::wstring() });

    for (i = 0; i < notice.prompt.answers.size(); i++)
    {
        bool  isSafe = (i == notice.prompt.safeAnswer);

        //  THE PROMPT CARRIES THE INDEX OF ITS OWN SAFE ANSWER, and that is
        //  the default and the close-box result: dismissing a question about a
        //  disk must not cost the user anything. Which answer that is differs
        //  by question, and taking it to be the last one threw away a disk that
        //  had no file left to go back to.
        def.buttons.push_back (DialogButton { notice.prompt.answers[i].label,
                                              (int) i, isSafe, isSafe, false });
    }

    def.closeBoxResult = (int) notice.prompt.safeAnswer;

    //  THE QUESTION STANDS UNTIL IT IS ANSWERED. Saving needs a destination,
    //  and only this thread can ask for one; but a picker the user backs out of
    //  is not an answer to the question. It used to be taken as declining --
    //  which on the lost-file notice is Discard, so cancelling a file dialog
    //  threw the disk away. Now it returns to the question, and only a
    //  completed save or an explicit other answer closes it.
    for (;;)
    {
        std::wstring  savePath;

        choice = ShowModalDialog (def);

        if (choice < 0 || choice >= (int) notice.prompt.answers.size())
        {
            choice = (int) notice.prompt.safeAnswer;
        }

        chosen = notice.prompt.answers[choice].action;

        if (chosen != ChangeAction::PreserveCopy)
        {
            break;
        }

        if (AskWhereToSaveLostDisk (m_machine.GetDiskStore().GetSourcePath (notice.slot, notice.drive),
                                    savePath))
        {
            saveTarget = fs::path (savePath).string();
            break;
        }
    }

    //  A path can contain spaces, so it goes last and the reader takes the
    //  rest of the line.
    PostCommand (IDM_DISK_RESOLVE_CHANGE,
                 std::format ("{} {} {} {}", notice.slot, notice.drive,
                              (int) chosen, saveTarget));

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::AskWhereToSaveLostDisk
//
//  Where to put the contents of a disk whose file has gone.
//
//  SEEDED WITH THE TIMESTAMPED PRESERVED NAME, in the folder the disk came
//  from -- `work.20260831-004512-01.dsk`, the same shape every other preserved
//  version gets. It used to offer the ORIGINAL name, which is wrong twice: it
//  invites the user to recreate the very file they deleted, and it makes this
//  the one rescue in the feature whose result cannot be told from an ordinary
//  disk by looking at the folder.
//
//  THEY CAN STILL TYPE ANYTHING. This is the default, not the rule.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::AskWhereToSaveLostDisk (const std::string & imagePath,
                                            std::wstring & outPath)
{
    HRESULT         hr       = S_OK;
    FileDialogSpec  spec;
    fs::path        original (imagePath);
    fs::path        chosen;
    bool            picked   = false;



    spec.filters       = { { L"Disk image", L"*.dsk;*.do;*.po;*.woz" } };
    spec.initialFolder = original.parent_path();

    if (!original.filename().empty())
    {
        std::string  suggested = PreservedCopy::MakePath (
                                     imagePath,
                                     PreservedCopy::MakeStamp (time (nullptr)),
                                     0);

        spec.defaultFileName = fs::path (suggested).filename().wstring();
    }

    //  A cancelled dialog is not a problem, and leaves through the same exit
    //  as a failure: with nothing chosen.
    hr = m_hostDialogs.PickFileToSave (m_hwnd, spec, chosen, picked);

    if (SUCCEEDED (hr) && picked)
    {
        outPath = chosen.wstring();
    }

    return SUCCEEDED (hr) && picked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HideChangeBanner
//
//  Closes the change band and gives its height back to the picture.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HideChangeBanner()
{
    m_changeBanner.SetVisible (false);
    m_changeBannerHideAtMs = 0;

    ReflowChromeForChangeBand();

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ExpireChangeBannerIfDue
//
//  Closes the band once its time is up.
//
//  HOVERING SUSPENDS THE COUNTDOWN RATHER THAN RESTARTING IT. Brushing across
//  the strip on the way to something else should not buy it another thirty
//  seconds, and reading it should not be interrupted. Moving the deadline
//  along with the clock while the pointer is on it does both.
//
//  DRIVEN OFF THE UI FRAME rather than a timer, because the band is only worth
//  taking away while there are frames to draw it in.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ExpireChangeBannerIfDue()
{
    int64_t  now     = ChangeBannerNowMs();
    int64_t  elapsed = now - m_changeBannerTickMs;
    POINT    cursor  = {};
    RECT     bounds  = {};
    bool     hovered = false;



    m_changeBannerTickMs = now;

    if (m_changeBannerHideAtMs == 0 || !m_changeBanner.IsVisible())
    {
        return;
    }

    bounds = m_changeBanner.GetBounds();

    if (GetCursorPos (&cursor) && ScreenToClient (m_hwnd, &cursor))
    {
        hovered = (cursor.x >= bounds.left && cursor.x < bounds.right
                && cursor.y >= bounds.top  && cursor.y < bounds.bottom);
    }

    if (hovered)
    {
        m_changeBannerHideAtMs += elapsed;
        return;
    }

    if (now >= m_changeBannerHideAtMs)
    {
        HideChangeBanner();
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OfferMouseToChangeBanner
//
//  Hands the message bar a mouse event.
//
//  IT HAS TO BE OFFERED EXPLICITLY. This shell hit-tests its chrome by name --
//  the toolbar, the joystick selector, the //c switch strip -- rather than
//  walking the host's panel tree, so a control that nobody asks is a control
//  that is painted and never pressed. Measured exactly that way: the bar drew
//  correctly, and its Dismiss did nothing.
//
//  ONLY WHILE IT IS UP, and only inside it. The band collapses to nothing when
//  no notice is showing, so there is nothing to hit the rest of the time.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OfferMouseToChangeBanner (DxuiMouseEventKind kind, int x, int y)
{
    RECT             bounds = m_changeBanner.GetBounds();
    DxuiMouseEvent   ev     = {};
    bool             inside = false;



    if (!m_changeBanner.IsVisible())
    {
        return false;
    }

    inside = (x >= bounds.left && x < bounds.right
           && y >= bounds.top  && y < bounds.bottom);

    if (!inside)
    {
        return false;
    }

    ev.kind        = kind;
    ev.button      = DxuiMouseButton::Left;
    ev.positionDip = POINT { x, y };

    return m_changeBanner.OnMouse (ev);
}

