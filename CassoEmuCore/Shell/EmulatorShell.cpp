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

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shcore.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
//  Builds only what needs no configuration. Anything that depends on the
//  resolved asset directory or the chosen machine waits for Initialize --
//  DiskManager in particular, because it needs a UserConfigStore that does
//  not exist until the asset base dir is known.
//
//  The Prng is the deterministic stand-in for indeterminate //e DRAM at
//  power-on (FR-035), shared by every device that re-seeds in PowerCycle. Its
//  seed mixes two weakly-correlated host sources so consecutive launches land
//  on different power-on patterns instead of the same one every time; tests
//  pin the seed through the harness rather than coming through this path.
//
//  Debug builds log that seed, because a fault caused by uninitialized DRAM
//  is otherwise unreproducible. Re-running with the same seed reproduces
//  byte-identical DRAM at every PowerCycle, which is the first thing needed
//  to chase a flaky illegal-opcode fault.
//
//  VideoTiming is owned at the SHELL level rather than per machine so all
//  three machine kinds share one 17,030-cycle frame counter behind $C019
//  (RDVBLBAR); a per-machine copy would restart the beam on every switch
//  (FR-033 / T055).
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::EmulatorShell()
{
    // / FR-035. The Prng is the deterministic stand-in for
    // indeterminate //e DRAM at power-on, shared across every device that
    // re-seeds in PowerCycle. The seed is derived from a couple of
    // weakly-correlated host sources so consecutive launches hit
    // different patterns; tests pin the seed directly via the test
    // harness instead of going through this path.
    uint64_t    seed = static_cast<uint64_t> (time (nullptr));



    seed ^= static_cast<uint64_t> (GetCurrentProcessId()) << 32;

    m_machine.SetPrng (make_unique<Prng> (seed));

#ifdef _DEBUG
    // Log the per-boot DRAM seed so when an illegal-opcode (or any
    // other non-deterministic) fault fires later, the user can grep
    // the debug output for "[Casso] Cold boot seed:" and capture the
    // value into a bug report. Re-running with the same seed gives
    // byte-identical DRAM at every PowerCycle, which is the first
    // requirement for reproducing flaky CPU faults.
    DEBUGMSG (L"[Casso] Cold boot seed: 0x%016llX\n",
              (unsigned long long) seed);
#endif

    // / FR-033 / T055. //e video timing model — owned at the
    // shell level so all three machine kinds (][/][+/]e) share the same
    // 17,030-cycle frame counter for $C019 (RDVBLBAR) reads.
    m_machine.SetVideoTiming (make_unique<VideoTiming>());

    m_clipboardManager = std::make_unique<ClipboardManager> (m_hostClipboard,
                                                              m_machine.GetMemoryBus(),
                                                              m_cpuManager.GetCommandMutex(),
                                                              m_cpuManager.GetPasteBuffer(),
                                                              m_framebufferMutex,
                                                              m_uiFramebuffer,
                                                              kFramebufferWidth,
                                                              kFramebufferHeight,
                                                              &m_machine.GetRefs().keyboard);

    // DiskManager construction is deferred to Initialize -- it needs
    // a UserConfigStore reference and that's created at Initialize
    // time once the asset base dir is resolved.

    MachineBuildServices  services;

    services.diskAudioSources       = &m_diskAudioSources;
    services.driveAudioMixer        = &m_driveAudioMixer;
    services.mockingboardAudioMixer = &m_mockingboardAudioMixer;
    services.printerAudio           = &m_printerAudio;
    services.wasapiAudio            = &m_wasapiAudio;
    services.printerWorker          = &m_printerWorker;
    services.printerAutoOpenActivity = &m_printerAutoOpenActivity;
    services.drivePan               = m_drivePan;
    services.driveMotorVolume       = &m_driveMotorVolume;
    services.driveHeadVolume        = &m_driveHeadVolume;
    services.driveDoorVolume        = &m_driveDoorVolume;
    services.traceCapacity          = &m_traceCapacity;
    services.imageWatchDisabled     = &m_imageWatchDisabled;
    services.requestPowerCycle      = [this] () { m_machineManager->PowerCycle(); };

    m_machineBuilder = std::make_unique<MachineBuilder> (m_machine, services);

    m_machineManager = std::make_unique<MachineManager> (*this);

    m_windowCommandManager = std::make_unique<WindowCommandManager> (*this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~EmulatorShell
//
//  Teardown is ordered by who points at whom, and every step below is placed
//  against a dangling reference it would otherwise create.
//
//  The CPU thread stops first: nothing can be safely destroyed while it is
//  still executing instructions against these devices.
//
//  Debug panels are unwired before they are destroyed. Each was registered as
//  an event SINK on live machine devices (disk controller, drive audio,
//  keyboard, //e soft switches, game port), and those devices outlive the
//  panel -- they die later, with the machine's owned devices. Resetting a
//  panel without
//  first revoking its sinks leaves the devices calling into freed memory. The
//  disk panel revokes controller then audio, mirroring the attachment order
//  in OpenDisk2DebugDialog (Spec-006 / FR-024).
//
//  Dirty disks are flushed before anything owning them unwinds, so a clean
//  quit never loses user writes (T097 / FR-025).
//
//  Adopted chrome is released explicitly. m_mainMenu and m_driveChrome
//  are registered into m_host->GetRoot() as RAW pointers via
//  DxuiPanel::Adopt, and they are members of this object -- so field-by-field
//  destruction below would leave the host's panel tree holding pointers into
//  a partially destroyed shell. ClearAdopted cuts those links while every
//  member is still whole. (The caption bar is host-owned, not adopted, and is
//  deliberately not in that set.)
//
//  OLE is uninitialized last, and only if this object initialized it.
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::~EmulatorShell()
{
    HRESULT             hrFlush    = S_OK;
    Disk2Controller *   controller = nullptr;



    // Before anything else tears down: a notification arriving mid-teardown
    // must not reach a half-destroyed shell.
    SetNotifyFunction (nullptr);
    s_pNotifyShell = nullptr;

    m_cpuManager.Stop();

    // Spec-006 / FR-024. Revoke BOTH sinks BEFORE the dialog tears
    // down its ring (and before the controller / audio source itself
    // is destroyed, which happens via the machine's owned devices and
    // m_diskAudioSources
    // below). Controller sink first, then audio sink, matching the
    // attachment order in OpenDisk2DebugDialog.
    if (m_disk2DebugPanel != nullptr)
    {
        controller = m_diskManager->FindSlot6Controller();

        if (controller != nullptr)
        {
            controller->SetEventSink (nullptr);
        }

        for (auto & diskAudioSource : m_diskAudioSources)
        {
            if (diskAudioSource != nullptr)
            {
                diskAudioSource->SetAudioEventSink (nullptr);
            }
        }

        m_disk2DebugPanel.reset();
    }

    if (m_inputDebugPanel != nullptr)
    {
        Apple2eSoftSwitchBank * iieSwitches = nullptr;

        if (m_machine.GetRefs().keyboard != nullptr)
        {
            m_machine.GetRefs().keyboard->SetInputEventSink (nullptr);
        }

        iieSwitches = m_machine.GetRefs().iieSoftSwitches;
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (nullptr);
        }

        if (m_machine.GetRefs().gamePort != nullptr)
        {
            m_machine.GetRefs().gamePort->SetInputEventSink (nullptr);
        }

        m_inputDebugPanel.reset();
    }

    // The printer panel holds no machine sinks -- just close its window.
    m_printerPanel.reset();

    // / T097 / FR-025. Final auto-flush of any dirty disks on
    // process shutdown — matches the "graceful exit" requirement from
    // audit §7 so a crash-free quit never loses user writes.
    //
    // THE SHUTDOWN VARIANT, because this runs after the message loop has
    // exited and the CPU thread has stopped, so a posted question would never
    // be delivered and its answer would never be acted on. It runs on this
    // thread with OLE still initialized -- OleUninitialize is below -- so the
    // store asks through a blocking file dialog instead.
    hrFlush = m_machine.GetDiskStore().FlushAllForShutdown();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    // Same idea for a preference change still inside its debounce window:
    // quitting right after a volume nudge would otherwise lose it.
    FlushDeferredGlobalPrefs();

    // Native-only ownership teardown.
    m_uiShell.Shutdown();
    m_dragDropTarget.Shutdown();
    m_driveWidgets.UnloadDocument();
    m_mainMenu.Hide();
    m_mainMenu.SetPopupHost (nullptr);

    // Drop the host's adopted-chrome references before the chrome
    // members or m_host itself go out of scope. The chrome controls
    // (m_mainMenu, m_driveChrome) are raw-pointer-
    // registered into m_host->GetRoot() via DxuiPanel::Adopt; releasing
    // the adoption here keeps the panel from ever holding a dangling
    // pointer during the field-by-field destruction below. (The caption
    // is host-owned, not adopted, so it is not in this set.)
    if (m_host)
    {
        m_host->GetRoot().ClearAdopted();
    }

    m_d3dRenderer.Shutdown();

    if (m_fOleInitialized)
    {
        OleUninitialize();
        m_fOleInitialized = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Full startup, in the only order that works. Most of the sequence is forced
//  by a dependency, and the ones that are not obvious are called out here.
//
//  OLE comes up early because RegisterDragDrop and IFileDialog (drive-widget
//  click-to-browse) both need the UI thread already in an STA. OleInitialize
//  implies CoInitializeEx(STA), and its S_FALSE "already initialized" result
//  still takes a reference -- which is why m_fOleInitialized is set on both
//  success codes and paired with OleUninitialize at shutdown. A hard failure
//  means something already claimed this thread's apartment with a conflicting
//  model, which is a startup-ordering bug on our side, so it asserts.
//
//  The window is created before the devices because the renderer needs an
//  HWND, and the devices need the renderer's device / context.
//
//  Video watch pages are marked once, here, rather than per machine: a write
//  into text pages 1/2 ($0400-$0BFF) or hi-res pages 1/2 ($2000-$5FFF) raises
//  the bus video-dirty flag that drives the render-skip gate. Watching the
//  PAGE INDEX covers main and aux together, since the //e MMU re-points those
//  same indices, and the page layout is identical on every Apple II variant --
//  so this survives an in-session machine switch untouched.
//
//  ReconcileInitialClientSize runs after ShowWindow (the non-client frame is
//  not fully materialized before that) but before UpdateWindowTitle, so a
//  wrong-size window never flashes on screen.
//
//  PowerCycle MUST precede MountCommandLineDisks. It seeds DRAM from the
//  shared Prng and runs the 6502 /RESET sequence (FR-034) -- without it the
//  CPU starts at PC=0 and executes uninitialized RAM into a beep loop instead
//  of the firmware prompt. It also ejects every drive and re-binds the engine
//  to the controller's empty internal disk, so mounting first and power-
//  cycling second silently discards the user's image: the engine keeps
//  ticking but AdvanceOneBit exits immediately on an empty track.
//
//  Startup disks reach the MRU the same way every other mount does, through
//  the mount-completion hook, so a --disk1 the loader refuses is reported and
//  is not offered back by the picker next launch. Both the report and the MRU
//  write are posted rather than run here: this is before the message loop, and
//  a modal raised from it would sit in front of a machine nothing is running.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Initialize (
    HINSTANCE             hInstance,
    const wstring       & machineName,
    const MachineConfig & config,
    const string        & disk1Path,
    const string        & disk2Path)
{
    HRESULT  hr = S_OK;



    m_machine.SetCurrentMachineName (machineName);
    m_machine.GetConfig()             = config;
    m_cyclesPerFrame     = config.cyclesPerFrame;

    // wWinMain installed the sink already; this just gives it a shell to
    // forward to. Anything reported before now is sitting in the queue and
    // is replayed once the window exists.
    s_pNotifyShell = this;

    RegisterChromeDock();
    InitAssetPathsAndStores();

    // Bring up OLE on the UI thread before any RegisterDragDrop / IFileDialog
    // (drive-widget click-to-browse) needs the STA apartment. OleInitialize
    // implies CoInitializeEx(STA); S_FALSE (already initialized) still owns a
    // reference we pair with OleUninitialize at shutdown. A hard failure here
    // means the UI thread's apartment was already claimed with a conflicting
    // model -- a startup-ordering bug on our side, so assert.
    hr = OleInitialize (nullptr);
    CHRA (hr);

    m_fOleInitialized = true;

    AllocateFramebuffers();

    PrimeChromeThemeEarly();

    hr = CreateEmulatorWindow (hInstance);
    CHR (hr);

    hr = BuildMachineDevices (config);
    CHR (hr);

    // Mark the display pages so a write into them raises the bus video-dirty
    // flag that drives the render-skip gate: text pages 1/2 ($0400-$0BFF) and
    // hi-res pages 1/2 ($2000-$5FFF). Aux writes share these page indices (the
    // //e MMU re-points them), so watching the index covers main and aux.
    // The page layout is identical across every Apple II variant, so this is
    // set once and survives an in-session machine switch.
    for (int page = 0x04; page <= 0x0B; page++)
    {
        m_machine.GetMemoryBus().SetVideoWatchPage (page, true);
    }

    for (int page = 0x20; page <= 0x5F; page++)
    {
        m_machine.GetMemoryBus().SetVideoWatchPage (page, true);
    }

    hr = InitializeRenderer();
    CHR (hr);

    hr = InitializeUiShell();
    CHR (hr);

    // Native-only bootstrap baseline: legacy chrome overlay retired
    // ahead of the native painter. Keep existing command/menu path active.

    // WASAPI audio is initialized on the CPU thread (COM apartment requirement)

    // Show window. A placement saved while maximized restores the state,
    // not just the normal rect it was created with: showing maximized
    // directly (instead of SW_SHOW then SW_MAXIMIZE) avoids a one-frame
    // flash of the restored-size window.

    // HONOR WHAT THE LAUNCHER ASKED FOR. Windows carries a requested show
    // state through CreateProcess into wWinMain, which is how
    // Start-Process -WindowStyle Minimized and every scripted launch says
    // "come up, but do not take the screen". Casso discarded it and always
    // activated, so a build-and-run in the background stole focus from
    // whatever the user was doing.
    //
    // Only a PARTICULAR request wins. SW_SHOWDEFAULT / SW_SHOW / normal is
    // what an ordinary double-click carries and means nothing in
    // particular, and the remembered placement -- which knows whether the
    // window was maximized -- is the better answer for it.
    {
        int   show = m_startMaximized ? SW_SHOWMAXIMIZED : SW_SHOW;

        switch (m_startShowCmd)
        {
            case SW_HIDE:
            case SW_MINIMIZE:
            case SW_SHOWMINIMIZED:
            case SW_SHOWMINNOACTIVE:
            case SW_SHOWNOACTIVATE:
            case SW_SHOWNA:
                show = m_startShowCmd;
                break;

            default:
                break;
        }

        ShowWindow (m_hwnd, show);
    }

    UpdateWindow (m_hwnd);

    // Reconcile actual client size against the desired framebuffer-sized
    // client now that the window is shown and its NC frame has fully
    // materialized. Done before UpdateWindowTitle so the user never sees
    // the wrong-size window flash.
    ReconcileInitialClientSize();

    UpdateWindowTitle();

    // / FR-034. Cold power-on: seed DRAM via the shared Prng and
    // run the 6502 /RESET sequence. Without this, the CPU starts at PC=0
    // and executes uninitialized RAM, leading to garbage on screen and
    // a beep loop instead of the firmware prompt. Mirrors what the
    // headless test harness does after BuildAppleII* construction.
    //
    // Must run BEFORE MountCommandLineDisks: PowerCycle ejects every
    // drive and re-binds the engine to the controller's empty internal
    // disk. Mounting first then power-cycling silently throws away the
    // user's freshly-mounted image (the engine ticks but AdvanceOneBit
    // exits early because trackBits[0] == 0).

    PowerCycle();

    // Every mount reports its outcome through here, not just this one:
    // the recent-disks entry, the damage check, and the failure report all
    // hang off it. Installed before the command-line disks go in so those
    // are covered too.
    m_diskManager->SetMountCompletedCallback (
        [this] (int drive, const std::string & path, HRESULT mountResult,
                const MountDiagnosis & diagnosis)
        {
            OnMountCompleted (drive, path, mountResult, diagnosis);
        });

    // The toggle runs on the CPU thread and the notice is Dxui, so the text
    // is posted to the window rather than shown here.
    m_diskManager->SetWriteProtectChangedCallback (
        [this] (const std::wstring & text)
        {
            PostNotice (text);
        });

    m_diskManager->MountCommandLineDisks (disk1Path, disk2Path);

    ApplyPersistedAudioPrefs();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterChromeDock
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RegisterChromeDock()
{
    // Register the chrome bands + center with the dock layout once --
    // their thicknesses are refreshed from DPI + live drive-bar state
    // on every ComputeViewportRect / GetClientSizeForCenterPx call.
    m_chromeDock.SetDock (m_titleBand,   DxuiDock::Top);
    m_chromeDock.SetDock (m_navBand,     DxuiDock::Top);
    m_chromeDock.SetDock (m_toolbarBand, DxuiDock::Top);

    // Under the toolbar and above the picture, because a notice about the disk
    // in the drive belongs with the controls rather than over the screen.
    m_chromeDock.SetDock (m_changeBand,  DxuiDock::Top);

    // Under the change notice, so a capture that starts while a disk question
    // stands does not push the question off the top of the chrome.
    m_chromeDock.SetDock (m_captureBand, DxuiDock::Top);
    m_chromeDock.SetDock (m_driveBand,   DxuiDock::Bottom);
    // Registered AFTER the drive band so the dock peels the drive bar off the
    // very bottom first and the //c switch strip lands just above it (between
    // the viewport and the joystick/paddle/mouse bar).
    m_chromeDock.SetDock (m_switchBand,  DxuiDock::Bottom);
    m_chromeDock.SetDock (m_centerBand,  DxuiDock::Fill);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitAssetPathsAndStores
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InitAssetPathsAndStores()
{
    fs::path  assetBaseDir = AssetBootstrap::GetAssetBaseDirectory();



    m_machine.SetAssetBaseDir (assetBaseDir.wstring());
    m_userConfigStore = std::make_unique<UserConfigStore> (assetBaseDir.wstring());

    m_diskManager = std::make_unique<DiskManager> (m_machine,
                                                   m_machine.GetDiskStore(),
                                                   m_diskAudioSources,
                                                   m_wasapiAudio,
                                                   m_driveWidgets,
                                                   m_driveWidgetState,
                                                   m_driveChrome,
                                                   m_cpuManager,
                                                   m_machine.GetCurrentMachineName(),
                                                   *m_userConfigStore,
                                                   m_uiFs,
                                                   m_userWriteProtect);

    //  The store owns the watch lifecycle; this only decides which watcher it
    //  gets. --no-image-watch installs one that refuses every watch, so the
    //  check made before every write can be measured on its own.
    m_diskManager->InstallSharedImageSupport (m_imageWatchDisabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AllocateFramebuffers
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AllocateFramebuffers()
{
    size_t  framebufferSize = static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight;



    // Create framebuffers (CPU renders to one, UI reads the other)
    m_cpuFramebuffer.resize (framebufferSize, 0);
    m_textOverlay.resize (framebufferSize, 0);
    m_uiFramebuffer.resize (framebufferSize, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrimeChromeThemeEarly
//
//  Primes the chrome-affecting theme state BEFORE creating the window so
//  the initial ClientSizeForCenter inside CreateEmulatorWindow reads the
//  right drive-bar thickness. Without this, a user whose persisted
//  activeTheme is compact (DarkModern or RetroTerminal) would get a window
//  sized for the full skeuomorphic strip on first paint, then immediately
//  shrink as soon as ThemeManager::Activate fires its listener later in
//  startup. UserConfigStore needs only assetBaseDir + the UI-thread
//  filesystem, both of which are already live here.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PrimeChromeThemeEarly()
{
    HRESULT                      hr = S_OK;
    UserConfigStore::LoadReport  report;
    std::wstring                 message;
    std::wstring                 skipped;



    // A missing UserPrefs.json (first run) is reported by LoadAll as success
    // with defaults. A corrupt one is the user's file being unreadable, so we
    // tell them where it broke and reset to defaults so Casso still boots --
    // silently starting over just reads as "Casso lost my settings".
    //
    // Two outcomes to report, and the difference matters to the reader. With a
    // preserved path, LoadAll moved every byte to that file and settings save
    // normally from here. Without one, the file is still where it was and
    // saving is refused until it reads, so the message must not suggest the
    // session will keep anything.
    //
    // Non-asserting on purpose. A malformed prefs file is bad DATA, not a
    // coding error: there is no bug for a developer to break into, and it
    // would stop the debugger every time someone hand-edits their JSON.
    // Assert or notify -- not both.
    //
    // CHRF rather than CHRN because this needs two actions, notify AND reset,
    // and the -N family is CHRF with its action fixed to one EhmNotifyUser
    // call. EhmNotifyUser rather than a themed dialog: this runs before the
    // chrome theme or main window exist, and it auto-detects GUI vs console.
    hr = m_userConfigStore->LoadAll (m_globalPrefs, m_uiFs, report);
    CHRF (hr,
          message = UserConfigStore::ComposeLoadFailureMessage (
                        m_machine.GetAssetBaseDir(), m_userConfigStore->GetUserPrefsFilePath(), report);
          EhmNotifyUser (message.c_str());
          m_globalPrefs = GlobalUserPrefs {});

    // A migration that carried forward what it could and left the rest is the
    // one degraded outcome that reports SUCCESS, so nothing else will mention
    // it. The unified file exists from here on, which closes the gate that
    // would have retried those files, so this is the only chance to say so.
    skipped = UserConfigStore::ComposeSkippedLegacyMessage (m_machine.GetAssetBaseDir(), report);

    if (!skipped.empty())
    {
        EhmNotifyUser (skipped.c_str());
    }

Error:
    m_chromeTheme = CassoTheme::MakeByName (m_globalPrefs.activeTheme);
    ApplyThemeToChrome (m_chromeTheme);
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildMachineDevices
//
//  Constructs one machine's device graph from its config, in wiring order:
//  memory, then the banking layers over it, then video, then the CPU that
//  reads through all of it.
//
//  WireBankedRom must follow WireLanguageCard, not replace it. The
//  language card leaves a flat bank-0 split in place; the //c layer then adds
//  the $C028 bank-switch coordinator and the no-slots $Cxxx routing on top.
//  Skipping it on the initial-launch path (SwitchMachine already did it) is
//  what left a cold-booted //c with no ROM banking and no SetNoExternalSlots,
//  so $C800 floated and the firmware derailed into a garbage screen. It is a
//  no-op on machines with no banked ROM (romBankSize == 0).
//
//  The bus is validated between the memory devices and the CPU, so an
//  overlapping address range is reported as a config error at build time
//  rather than as mysterious reads once code is running.
//
//  The page table is wired last: it caches decisions made by every layer
//  above, so building it earlier would snapshot an incomplete map.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::BuildMachineDevices (const MachineConfig & config)
{
    return m_machineBuilder->Build (config);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeUiShell
//
//  Native UI runtime bootstrap. UiShell owns the painter, text renderer,
//  hit-tester, focus manager, and input translator; the host panel-tree
//  pump composites the chrome on top of the emulator frame. Infrastructure
//  bring-up that genuinely fails -- D2D/text, theme enumeration -- aborts
//  startup through CHR (Main surfaces it); corrupt user prefs recover to
//  defaults rather than abort.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::InitializeUiShell()
{
    HRESULT  hr = S_OK;



    hr = m_uiShell.Initialize (&m_d3dRenderer);
    CHR (hr);

    hr = WireUiShellChromeAndThemes();
    CHR (hr);

    RestoreColorTextPref();
    RecordActiveMachineSelection();

    SubscribeAndActivateTheme();

    ApplyPersistedChromePrefs();

    // Seed the CRT override keys once the machine is settled. Done here
    // rather than inside ApplyPersistedChromePrefs, which returns early for
    // a machine carrying no $cassoUiPrefs object and would leave the keys
    // empty for it.
    RefreshCrtOverrideKeys();

    hr = FinishUiShellLayout();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireUiShellChromeAndThemes
//
//  Connects the chrome controls to UiShell's shared services and brings up
//  the theme manager.
//
//  UiShell no longer paints the caption or the chrome -- the host panel tree
//  does. What it still owns is input routing, hit-testing, and the theme /
//  viewport metrics the settings panel reads, which is why only those are
//  wired here.
//
//  The shared text renderer is INJECTED into each control rather than passed
//  to Layout. Chrome controls have to measure their own label strings while
//  laying out, so handing them the renderer once lets them satisfy the plain
//  IDxuiControl::Layout contract instead of forcing a renderer parameter onto
//  every Layout call in the framework.
//
//  Global prefs are deliberately NOT re-loaded here; PrimeChromeThemeEarly
//  already did that, and a second LoadAll would just re-read the same file.
//  Discover treats an empty or missing themes directory as success -- the
//  built-in themes work without any on-disk ones -- so only a real
//  enumeration failure propagates and blocks startup.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::WireUiShellChromeAndThemes()
{
    HRESULT   hr        = S_OK;
    fs::path  themesDir = fs::path (m_machine.GetAssetBaseDir()) / fs::path ("Themes");



    // The caption is host-owned now, and chrome paints through the
    // host panel tree; UiShell only routes input, hit-tests, and
    // supplies the theme / viewport metrics the settings panel reads.
    m_uiShell.SetMainMenu (&m_mainMenu);
    m_uiShell.SetTheme    (&m_chromeTheme);

    // Inject the shared text renderer into chrome controls that
    // need to measure label strings during Layout. Mirrors the
    // UiShell-owned painter / text renderer pair so the chrome
    // controls participate in the standard IDxuiControl::Layout
    // contract without needing the renderer passed as a Layout
    // parameter on every call.
    m_mainMenu.SetTextRendererForMeasure (&m_uiShell.GetTextRenderer());
    m_switchBar.SetTextRenderer          (&m_uiShell.GetTextRenderer());
    m_toolbar.SetTextRenderer            (&m_uiShell.GetTextRenderer());
    m_inputCluster.SetTextRenderer       (&m_uiShell.GetTextRenderer());

    // Global prefs are already loaded by PrimeChromeThemeEarly, so there is
    // no second LoadAll here. Discover scans the themes directory (an empty
    // or absent one returns S_OK -- the built-in themes still work), so only
    // a genuine enumeration failure propagates.
    m_themeManager = std::make_unique<ThemeManager> (m_uiFs, themesDir.wstring());
    hr             = m_themeManager->Discover();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireToolbarPickers
//
//  The command bar's theme and monitor-color pickers, wired to the same
//  live-apply channels the Settings panel drives so the two behave alike.
//
//  A HIGHLIGHT PREVIEWS: it activates the theme (or the color treatment)
//  without writing anything down, because moving the pointer down a list is
//  not a choice. The toolbar replays that same sink with the row the list
//  opened on when the list is dismissed, which is what snaps the chrome and
//  the picture back.
//
//  A PICK COMMITS, which is where the choice reaches the prefs file: the
//  theme into GlobalUserPrefs, the color mode into the machine's UI prefs.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::WireToolbarPickers()
{
    m_toolbar.SetPopupHost (m_host.get());

    m_toolbar.SetDropDownSinks (EmulatorCommands::kIdTheme,
        [this] (int index)
        {
            HRESULT  hrTheme = S_OK;
            bool     inRange = index >= 0 && index < (int) m_toolbarThemeIds.size();

            if (inRange)
            {
                hrTheme = ApplyThemeLive (m_toolbarThemeIds[index]);
                IGNORE_RETURN_VALUE (hrTheme, S_OK);
            }
        },
        [this] (int index)
        {
            HRESULT  hrTheme = S_OK;
            bool     inRange = index >= 0 && index < (int) m_toolbarThemeIds.size();

            if (inRange)
            {
                m_mainMenu.GetCommands().SetThemeIndex (index);
                hrTheme = ApplyAndPersistTheme (m_toolbarThemeIds[index]);
                IGNORE_RETURN_VALUE (hrTheme, S_OK);
            }
        });

    m_toolbar.SetDropDownSinks (EmulatorCommands::kIdColor,
        [this] (int index)
        {
            SetColorModeLive (index);
        },
        [this] (int index)
        {
            m_mainMenu.GetCommands().SetMonitorColorIndex (index);
            SetColorModeLive              (index);
            PersistColorModeForMachine    (index);
        });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishUiShellLayout
//
//  Settles the chrome for the machine that was just built, before the window
//  is shown. This is the boot-time counterpart to what OnSize does on every
//  later resize.
//
//  The live monitor DPI is pushed into UiShell here so the FIRST D2D
//  BindBackBuffer uses it. Skipping this leaves the initial bind at the m_dpi
//  default of 0 (treated as 96), and chrome text paints tiny on a high-DPI
//  display until the user happens to resize the window and trigger a real
//  layout.
//
//  Drive widgets are collapsed rather than skipped when there is nothing to
//  show -- a stripped Apple II config with no Slot 6 controller hides both, a
//  //c with no external drive connected hides only the second. The joystick
//  button still paints in every case, since joystick input does not depend on
//  disk presence.
//
//  Hit-test rects are registered from the widgets' final geometry, and only
//  for widgets that are actually visible, so a hidden drive cannot be clicked.
//
//  Chrome no longer composites through an after-blit hook; it paints through
//  the host's panel-tree pump over the Apple ][ framebuffer. The per-frame
//  drive tick and door-animation redraw that used to live in that hook now run
//  in RunMessageLoop.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::FinishUiShellLayout()
{
    HRESULT  hr         = S_OK;
    UINT     initialDpi = GetDpiForWindow (m_hwnd);
    bool     fHasDisk   = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();



    // Propagate the live monitor DPI into UiShell so the first
    // D2D BindBackBuffer uses the right DPI for text. Without
    // this the initial paint binds at the m_dpi default (0->96)
    // and chrome text renders tiny on high-DPI displays until
    // the user resizes the window.
    hr = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                             m_d3dRenderer.GetBackBufferHeight(),
                             initialDpi);
    CHR (hr);

    // Chrome no longer composites via an after-blit hook: it
    // paints through the host's panel-tree pump (the adopted
    // chrome controls) on top of the Apple ][ framebuffer. The
    // per-frame drive-widget tick + door-animation redraw that
    // used to live in that hook now run in RunMessageLoop.
    if (DeskSceneActive())
    {
        // The 3D scene owns the drives: widgets hidden, drop-target rects
        // from the composition's projected drive bounds.
        SyncSceneDriveChrome();
    }
    else if (!fHasDisk)
    {
        // No Slot 6 controller (stripped Apple II config) --
        // collapse the drive widgets so they paint nothing
        // and the bottom command bar is clear of drive UI.
        // The joystick-mode button still paints, since
        // joystick input is independent of disk presence.
        m_driveChrome[0].Hide();
        m_driveChrome[1].Hide();
    }
    else if (!ShouldShowExternalDrive())
    {
        // //c with the optional external drive not connected: the
        // internal drive (widget 0) shows, the external (widget 1)
        // stays collapsed until the user connects it in Settings.
        m_driveChrome[1].Hide();
    }

    if (!DeskSceneActive())
    {
        m_uiShell.GetHitTester().Clear();
        if (fHasDisk)
        {
            m_uiShell.GetHitTester().Register (DxuiHitRect { m_driveChrome[0].GetBodyRect(), DxuiHitSlot::Custom, 0 });
            if (ShouldShowExternalDrive())
            {
                m_uiShell.GetHitTester().Register (DxuiHitRect { m_driveChrome[1].GetBodyRect(), DxuiHitSlot::Custom, 1 });
            }
        }
    }

    if (m_fOleInitialized)
    {
        InstallDragDropTarget();
    }

    InstallChangeReporting();
    InstallIntentMessageFilter();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstallDragDropTarget
//
//  Registers the window as an OLE drop target for disk images, and opens the
//  UIPI holes that make dropping work across an integrity-level boundary.
//
//  A failed registration is survivable and deliberately non-fatal: File > Open
//  and the drive widgets' click-to-browse mount the same images, so losing
//  drag-and-drop costs a convenience, not a capability, and must not prevent
//  launch.
//
//  The message filters are the non-obvious half. When Casso runs at a HIGHER
//  integrity level than the drag source -- the common case being an elevated
//  Casso and a normal Explorer window -- UIPI silently drops the messages OLE
//  uses to marshal the payload across the boundary, and the drop simply does
//  nothing with no error anywhere. Allowing the three messages OLE actually
//  uses for drop targets (WM_DROPFILES, WM_COPYDATA, and the undocumented but
//  real WM_COPYGLOBALDATA) makes Explorer-to-elevated-Casso drags work without
//  lowering Casso's own integrity level.
//
//  Only m_hwnd needs the filter: the window is a single top-level HWND now
//  that the legacy CassoRenderSurface child is gone.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InstallDragDropTarget()
{
    HRESULT     hrDrop            = S_OK;
    const UINT  kWmCopyGlobalData = 0x0049;   // undocumented but real



    // Drag-drop is an optional convenience -- File > Open and the drive
    // widgets' click-to-browse cover the same mounts -- so a failed
    // registration disables drop but must not prevent launch.
    hrDrop = m_dragDropTarget.Initialize (m_hwnd, &m_uiShell.GetHitTester(), [this] (int tag, const std::wstring & path) { Mount (6, tag, path); }, IsSupportedDiskImageExtension);
    IGNORE_RETURN_VALUE (hrDrop, S_OK);

    // UIPI whitelist. When Casso runs at a higher integrity
    // level than the source (e.g. user launched Casso
    // elevated and is dragging from a non-elevated Explorer),
    // UIPI silently blocks the messages OLE uses to marshal
    // the dragged payload across the IL boundary. The fix
    // is ChangeWindowMessageFilterEx for the three messages
    // OLE actually uses for drop targets:
    //   WM_DROPFILES       (0x0233)
    //   WM_COPYDATA        (0x004A)
    //   WM_COPYGLOBALDATA  (0x0049, undocumented but real)
    // Allowing these lets Explorer -> elevated-Casso drag
    // work without lowering Casso's IL. The window is now a
    // single top-level HWND (the legacy CassoRenderSurface
    // child is gone), so only m_hwnd needs the filter.
    (void) ChangeWindowMessageFilterEx (m_hwnd, WM_DROPFILES,      MSGFLT_ALLOW, nullptr);
    (void) ChangeWindowMessageFilterEx (m_hwnd, WM_COPYDATA,       MSGFLT_ALLOW, nullptr);
    (void) ChangeWindowMessageFilterEx (m_hwnd, kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentMachineNameNarrow
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetCurrentMachineNameNarrow() const
{
    std::string  narrow;



    narrow.reserve (m_machine.GetCurrentMachineName().size());
    for (wchar_t c : m_machine.GetCurrentMachineName())
    {
        narrow.push_back ((char) (unsigned char) c);
    }

    return narrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MachineHasCaseSwitches
//
//  Whether this machine's case carries switches the user can reach.
//
//  The shell used to answer this by testing whether a //c ROM bank had been
//  wired, which was true and beside the point: it made the question "is this a
//  //c" when what the chrome needs to know is whether there is a switch panel
//  to lay out. The machine answers it, so a later model with a switch panel
//  needs no arm added here.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::MachineHasCaseSwitches() const
{
    const MachineDefinition *  definition = MachineDefinitions::Find (m_machine.GetConfig().machineId);



    return (definition != nullptr && definition->hasCaseSwitches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MachineHasBuiltInDrive
//
//  Whether the machine's drive is soldered in rather than plugged into a card,
//  which is what decides the drive the desk scene draws.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::MachineHasBuiltInDrive() const
{
    const MachineDefinition *  definition = MachineDefinitions::Find (m_machine.GetConfig().machineId);



    return (definition != nullptr && definition->hasBuiltInDrive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetAuxRamBuffer
//
////////////////////////////////////////////////////////////////////////////////

const Byte * EmulatorShell::GetAuxRamBuffer() const
{
    return m_machineBuilder != nullptr ? m_machineBuilder->GetAuxRamBuffer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchMachine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::SwitchMachine(const wstring & machineName)
{
    return m_machineManager->SwitchMachine(machineName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostCommand
//
//  Thin wrapper that hands the command id and payload to the CpuManager
//  queue. Retained on EmulatorShell so call sites that already speak
//  the "post a menu id" idiom do not need to know the manager exists.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostCommand (WORD id, const string & payload)
{
    m_cpuManager.PostCommand (id, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepInstructionWhilePaused
//
//  Runs one CPU instruction directly from the UI thread. Caller MUST
//  have verified the CPU thread is paused -- this is a quiet contract;
//  we don't re-check here. A paused CPU thread still drains its command
//  queue, so a machine switch can be under way: the step takes the
//  machine's lifetime lock shared and is skipped while the switch holds
//  it exclusively.
//
//  Steps the CPU, ticks the disk controller in step, then runs one
//  full video frame and publishes the framebuffer so the main UI
//  loop sees the framebuffer-dirty flag next iteration and presents.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StepInstructionWhilePaused()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);



    if (!lifetime.owns_lock())
    {
        return;
    }

    m_machine.StepOne();

    RunOneFrame();
    PublishFramebuffer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SoftReset()
{
    m_machineManager->SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PowerCycle()
{
    m_machineManager->PowerCycle();
}
