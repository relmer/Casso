#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Config/UserConfigStore.h"
#include "Ui/ThemeManager.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/WindowManager.h"
#include "Shell/DiskManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellPrinter.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/Components/ShellUpdater.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerProfileStore.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
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
#include "Devices/Tape/TapeImageLoader.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
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
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Seams/Win32DiskFileIo.h"

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

    SetPrngSeed (seed);

    m_settings      = std::make_unique<ShellSettings> (*this);
    m_windowManager = std::make_unique<WindowManager> (m_settings->GetPrefs(), [this] { m_settings->SaveGlobalPrefs(); });
    m_updater       = std::make_unique<ShellUpdater> (*this);
    m_audio         = std::make_unique<ShellAudio>();
    m_tapeDeck      = std::make_unique<ShellTapeDeck> (*this);
    m_printer       = std::make_unique<ShellPrinter> (*this);
    m_disks         = std::make_unique<ShellDisks> (*this);
    m_scene         = std::make_unique<ShellDeskScene> (*this);
    m_chrome        = std::make_unique<ShellChrome> (*this);

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

    m_audio->BindBuildServices (services);

    m_printer->BindBuildServices (services);

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
    HRESULT  hrFlush = S_OK;



    // Before anything else tears down: a notification arriving mid-teardown
    // must not reach a half-destroyed shell.
    SetNotifyFunction (nullptr);
    s_pNotifyShell = nullptr;

    // The update workers next: a download in flight is canceled, and every
    // worker is joined before anything it posts to or reads from goes.
    m_updater->StopUpdateService();

    //  THE CONTROLLER STACK GOES BY HAND, HERE, for the same reason. Its
    //  members are declared after the window, so member-order destruction
    //  leaves the HWND alive and dispatching messages after the service is
    //  gone -- and WM_ACTIVATEAPP is exactly the kind that arrives while a
    //  window is being torn down. OnActivateApp guards on the pointer, but a
    //  unique_ptr does not null itself as it destroys, so the guard reads a
    //  stale non-null pointer and calls into freed memory. The crash lands
    //  on the service's mutex, which is the first thing it touches.
    //
    //  In order: the thread stops and joins first, so nothing is inside the
    //  service when it goes; then the service, then the backend it reads.
    m_controllerThread.reset();

    // What automatic calibration learned this session, while the service that
    // holds it still exists and nothing is reading into it.
    SaveControllerPrefs();

    m_controllerService.reset();
    m_controllerBackend.reset();

    m_cpuManager.Stop();

    // The printer panel holds no machine sinks -- just close its window.
    m_printer->ClosePrinterPanel();

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
    m_settings->FlushDeferredGlobalPrefs();

    // An update applied when Casso closes, now that the disks and the
    // preferences are written.
    m_updater->CommitPendingUpdateAtExit();

    // Native-only ownership teardown.
    m_uiShell.Shutdown();
    m_dragDropTarget.Shutdown();
    m_disks->GetDriveWidgets().UnloadDocument();
    m_chrome->m_mainMenu.Hide();
    m_chrome->m_mainMenu.SetPopupHost (nullptr);

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
//  GetUpdater
//
////////////////////////////////////////////////////////////////////////////////

ShellUpdater & EmulatorShell::GetUpdater()
{
    return *m_updater;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTapeDeck
//
////////////////////////////////////////////////////////////////////////////////

ShellTapeDeck & EmulatorShell::GetTapeDeck()
{
    return *m_tapeDeck;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPrinter
//
////////////////////////////////////////////////////////////////////////////////

ShellPrinter & EmulatorShell::GetPrinter()
{
    return *m_printer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDisks
//
////////////////////////////////////////////////////////////////////////////////

ShellDisks & EmulatorShell::GetDisks()
{
    return *m_disks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDeskScene
//
////////////////////////////////////////////////////////////////////////////////

ShellDeskScene & EmulatorShell::GetDeskScene()
{
    return *m_scene;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetChrome
//
////////////////////////////////////////////////////////////////////////////////

ShellChrome & EmulatorShell::GetChrome()
{
    return *m_chrome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneActive
//
//  The 3D scene renders whenever a skeuo theme is active and the models
//  loaded. The DRIVES are not optional -- they are 3D objects in every skeuo
//  presentation; compact themes keep their flat widgets.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::DeskSceneActive() const
{
    return !m_chrome->m_chromeTheme.compactDrives && m_scene->m_deskSceneReady;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSettings
//
////////////////////////////////////////////////////////////////////////////////

ShellSettings & EmulatorShell::GetSettings()
{
    return *m_settings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtMonitorActive
//
//  The desk scene, and the monitor on top of it, which the user CAN turn off:
//  the picture then sits on a flat rect at classic sizes with the 3D drive row
//  still composed in the band below it. Everything keyed off the curved glass
//  -- the glass-fill fullscreen, the inverse-projected pointer mapping, the
//  Ctrl+0 solve -- follows this, not DeskSceneActive.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::CrtMonitorActive() const
{
    return DeskSceneActive() && m_settings->GetPrefs().crtMonitor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutDriveRowForTest
//
//  The flat drive band's row, for the band layout tests: lays the row out in
//  a client of the given size with no band below it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutDriveRowForTest (int clientW, int clientH, UINT dpi, int visibleCount)
{
    m_chrome->LayoutDriveWidgetsInCommandBar (m_disks->GetDriveChrome(), 0, clientW, clientH, dpi, 1.0f, visibleCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriveRectForTest
//
////////////////////////////////////////////////////////////////////////////////

RECT EmulatorShell::GetDriveRectForTest (size_t drive) const
{
    return m_disks->GetDriveChrome()[drive].GetOuterRect();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTapeAnchorForTest
//
////////////////////////////////////////////////////////////////////////////////

RECT EmulatorShell::GetTapeAnchorForTest() const
{
    return m_tapeDeck->GetAnchor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRecorderAttachedForTest
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetRecorderAttachedForTest (bool attached)
{
    m_tapeDeck->SetRecorderConnected (attached);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTapeVolume
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetTapeVolume (float gain)
{
    m_audio->GetTapeSource().SetVolume (gain);
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
//  The video watch pages that drive the render-skip gate are not marked here:
//  MachineBuilder::Build marks them on every build, because a machine switch
//  replaces the bus and its marks with it.
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
    const string        & disk2Path,
    const string        & tapePath)
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

    // Every paddle and pushbutton write goes through the mixer from here on.
    // Writes happen on this (the UI) thread; a submission from any other
    // thread posts a flush back to the window.
    m_gamePortSink = std::make_unique<MachineGamePortSink> (m_machine.GetLifetimeLock(), [this]
    {
        GamePortTargets            targets;
        const MachineDefinition  * definition = MachineDefinitions::Find (m_machine.GetConfig().machineId);

        targets.gamePort    = m_machine.GetRefs().gamePort;
        targets.iieSwitches = m_machine.GetRefs().iieSoftSwitches;
        targets.iieKeyboard = m_machine.GetRefs().iieKeyboard;
        targets.joyport     = m_machine.GetJoyport();

        if (definition != nullptr)
        {
            targets.axisCount = static_cast<size_t> (definition->gamePortAxisCount);
        }

        return targets;
    });

    m_gamePortMixer.SetApplyThread (std::this_thread::get_id(), [hwnd = m_hwnd]
    {
        PostMessageW (hwnd, WM_APP_GAMEPORT_FLUSH, 0, 0);
    });

    m_gamePortMixer.SetSink (m_gamePortSink.get());

    // Physical controllers. The backend is initialized on the controller
    // thread, which owns the window its device notifications arrive at, so
    // nothing here touches a device.
    m_controllerBackend = std::make_unique<Win32ControllerBackend>();
    m_controllerService = std::make_unique<ControllerInputService> (*m_controllerBackend, m_gamePortMixer);

    // Saved controller settings, calibrations and players, before the thread
    // starts reading, so its first evaluation already knows who was picked
    // and who last held each slot.
    LoadControllerPrefs();

    m_controllerThread  = std::make_unique<ControllerInputThread>();

    // A controller that held a slot and left is said over the picture, and so
    // is one Automatic gave a slot that another controller held last time;
    // the command bar's picker already shows what is playing after either.
    m_controllerService->SetSlotsChangedFn (
        [this] (const ControllerInputService::SlotsChange & change)
        {
            {
                std::lock_guard<std::mutex>  lock (m_controllerPickMutex);

                for (const std::wstring & description : change.departedDescriptions)
                {
                    m_controllerPickNotices.push_back (description + L" disconnected.");
                }

                m_controllerPickNotices.insert (m_controllerPickNotices.end(), change.notices.begin(), change.notices.end());
                m_controllerPickHasEntries = m_controllerPickHasEntries || change.haveEntriesChanged;
                m_controllerPickHasHolders = m_controllerPickHasHolders || change.haveLastHoldersChanged;
            }

            PostMessageW (m_hwnd, WM_APP_CONTROLLER_PICK, 0, 0);
        });

    // A connect or disconnect changes who owns the axes but tells the user
    // nothing, so it takes the same trip to the UI thread without a notice.
    m_controllerService->SetStateChangedFn ([this]
    {
        PostMessageW (m_hwnd, WM_APP_CONTROLLER_PICK, 0, 0);
    });

    // The Controllers page asking for a controller to be read wakes the
    // thread, which may be waiting on an event-driven controller that is not
    // moving.
    m_controllerService->SetWakeFn ([this]
    {
        m_controllerThread->Wake();
    });

    hr = m_controllerThread->Start (m_controllerBackend.get(), m_controllerService.get(),
                                    [this] { return m_controllerService->Tick(); });
    IGNORE_RETURN_VALUE (hr, S_OK);

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

    // Every mount reports its outcome through the disks, not just these:
    // installed before the command-line disks go in so those are covered too.
    m_disks->InstallMountReporting();

    m_disks->GetManager()->MountCommandLineDisks (disk1Path, disk2Path);

    m_tapeDeck->InsertStartupTape (tapePath);

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
    m_chrome->m_chromeDock.SetDock (m_chrome->m_titleBand,   DxuiDock::Top);
    m_chrome->m_chromeDock.SetDock (m_chrome->m_navBand,     DxuiDock::Top);
    m_chrome->m_chromeDock.SetDock (m_chrome->m_toolbarBand, DxuiDock::Top);

    // Under the toolbar and above the picture, because a notice about the disk
    // in the drive belongs with the controls rather than over the screen.
    m_chrome->m_chromeDock.SetDock (m_chrome->m_changeBand,  DxuiDock::Top);

    // Under the change notice, so a capture that starts while a disk question
    // stands does not push the question off the top of the chrome.
    m_chrome->m_chromeDock.SetDock (m_chrome->m_standInBand, DxuiDock::Top);
    m_chrome->m_chromeDock.SetDock (m_chrome->m_driveBand,   DxuiDock::Bottom);
    // Registered AFTER the drive band so the dock peels the drive bar off the
    // very bottom first and the //c switch strip lands just above it (between
    // the viewport and the joystick/paddle/mouse bar).
    m_chrome->m_chromeDock.SetDock (m_chrome->m_switchBand,  DxuiDock::Bottom);
    m_chrome->m_chromeDock.SetDock (m_chrome->m_centerBand,  DxuiDock::Fill);
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
    m_settings->CreateConfigStore (assetBaseDir.wstring());

    m_disks->Initialize (*m_settings->GetConfigStore(), m_settings->GetFileSystem(), m_imageWatchDisabled);

    m_audio->AttachTape (&m_machine.GetTapeDeck(),
                         [this] () { return m_machine.GetCpu() != nullptr ? *m_machine.GetCpu()->GetBusCyclePtr() : 0; });

    m_tapeDeck->Initialize (*m_settings->GetConfigStore(), m_settings->GetFileSystem());
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
    hr = m_settings->GetConfigStore()->LoadAll (m_settings->GetPrefs(), m_settings->GetFileSystem(), report);
    CHRF (hr,
          message = UserConfigStore::ComposeLoadFailureMessage (
                        m_machine.GetAssetBaseDir(), m_settings->GetConfigStore()->GetUserPrefsFilePath(), report);
          EhmNotifyUser (message.c_str());
          m_settings->GetPrefs() = GlobalUserPrefs {});

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
    m_scene->ApplyChromeThemeByName (m_settings->GetPrefs().activeTheme);
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
    m_settings->RecordActiveMachineSelection();

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
    m_uiShell.SetMainMenu (&m_chrome->m_mainMenu);
    m_uiShell.SetTheme    (&m_chrome->m_chromeTheme);

    // Inject the shared text renderer into chrome controls that
    // need to measure label strings during Layout. Mirrors the
    // UiShell-owned painter / text renderer pair so the chrome
    // controls participate in the standard IDxuiControl::Layout
    // contract without needing the renderer passed as a Layout
    // parameter on every call.
    m_chrome->m_mainMenu.SetTextRendererForMeasure (&m_uiShell.GetTextRenderer());
    m_chrome->m_switchBar.SetTextRenderer          (&m_uiShell.GetTextRenderer());
    m_chrome->m_toolbar.SetTextRenderer            (&m_uiShell.GetTextRenderer());

    // Global prefs are already loaded by PrimeChromeThemeEarly, so there is
    // no second LoadAll here. Discover scans the themes directory (an empty
    // or absent one returns S_OK -- the built-in themes still work), so only
    // a genuine enumeration failure propagates.
    m_settings->CreateThemeManager (themesDir.wstring());

    hr = m_settings->GetThemeManager()->Discover();
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
    m_chrome->m_toolbar.SetPopupHost (m_host.get());

    //  A CLICK ON A MENU TITLE SWITCHES TO THAT MENU, rather than being spent
    //  closing the drop-down that was open. The drop-down holds capture, so
    //  the title never sees the click on its own -- and every other
    //  application on the desktop rolls from one menu to the next on it.
    //
    //  Only the titles. Anywhere else the click closes the picker and stops
    //  there, which is what a menu does everywhere: dismissing is not a
    //  reason to fire the button that happened to be underneath.
    m_chrome->m_toolbar.SetDropDownClickOutsideFn (
        [this] (POINT screenPx)
        {
            POINT  client = screenPx;

            if (m_hwnd == nullptr || !ScreenToClient (m_hwnd, &client))
            {
                return;
            }

            for (int i = 0; i < m_chrome->m_mainMenu.GetMenuCount(); i++)
            {
                RECT  title = m_chrome->m_mainMenu.GetMenuRect (i);

                if (PtInRect (&title, client))
                {
                    m_chrome->m_mainMenu.Open (i, false);
                    return;
                }
            }
        });

    m_chrome->m_toolbar.SetDropDownSinks (EmulatorCommands::kIdTheme,
        [this] (int index)
        {
            HRESULT  hrTheme = S_OK;
            bool     inRange = index >= 0 && index < (int) m_chrome->m_toolbarThemeIds.size();

            if (inRange)
            {
                hrTheme = m_settings->ApplyThemeLive (m_chrome->m_toolbarThemeIds[index]);
                IGNORE_RETURN_VALUE (hrTheme, S_OK);
            }
        },
        [this] (int index)
        {
            HRESULT  hrTheme = S_OK;
            bool     inRange = index >= 0 && index < (int) m_chrome->m_toolbarThemeIds.size();

            if (inRange)
            {
                m_chrome->m_mainMenu.GetCommands().SetThemeIndex (index);
                hrTheme = m_settings->ApplyAndPersistTheme (m_chrome->m_toolbarThemeIds[index]);
                IGNORE_RETURN_VALUE (hrTheme, S_OK);
            }
        });

    m_chrome->m_toolbar.SetDropDownSinks (EmulatorCommands::kIdColor,
        [this] (int index)
        {
            SetColorModeLive (index);
        },
        [this] (int index)
        {
            m_chrome->m_mainMenu.GetCommands().SetMonitorColorIndex (index);
            SetColorModeLive              (index);
            m_settings->PersistColorModeForMachine    (index);
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
    bool     fHasDisk   = (m_disks->GetManager() != nullptr) && m_disks->GetManager()->HasSlot6Controller();



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
        m_scene->SyncSceneDriveChrome();
    }
    else if (!fHasDisk)
    {
        // No Slot 6 controller (stripped Apple II config) --
        // collapse the drive widgets so they paint nothing
        // and the bottom command bar is clear of drive UI.
        // The joystick-mode button still paints, since
        // joystick input is independent of disk presence.
        m_disks->GetDriveChrome()[0].Hide();
        m_disks->GetDriveChrome()[1].Hide();
    }
    else if (!m_disks->ShouldShowExternalDrive())
    {
        // //c with the optional external drive not connected: the
        // internal drive (widget 0) shows, the external (widget 1)
        // stays collapsed until the user connects it in Settings.
        m_disks->GetDriveChrome()[1].Hide();
    }

    if (!DeskSceneActive())
    {
        m_uiShell.GetHitTester().Clear();
        if (fHasDisk)
        {
            m_uiShell.GetHitTester().Register (DxuiHitRect { m_disks->GetDriveChrome()[0].GetBodyRect(), DxuiHitSlot::Custom, 0 });
            if (m_disks->ShouldShowExternalDrive())
            {
                m_uiShell.GetHitTester().Register (DxuiHitRect { m_disks->GetDriveChrome()[1].GetBodyRect(), DxuiHitSlot::Custom, 1 });
            }
        }

        m_tapeDeck->RegisterTapeDropTarget();
    }

    if (m_fOleInitialized)
    {
        InstallDragDropTarget();
    }

    m_disks->InstallChangeReporting();
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
    // Disks and tapes both; OnFileDropped sends each only to what can take it.
    hrDrop = m_dragDropTarget.Initialize (m_hwnd,
                                          &m_uiShell.GetHitTester(),
                                          [this] (int tag, const std::wstring & path) { OnFileDropped (tag, path); },
                                          [] (const std::wstring & path)
                                          {
                                              return IsSupportedDiskImageExtension (path) ||
                                                     TapeImageLoader::IsTapeFileExtension (path);
                                          });
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
//  EmulatorShell::GetAuxRamBuffer
//
////////////////////////////////////////////////////////////////////////////////

const Byte * EmulatorShell::GetAuxRamBuffer() const
{
    return m_machineBuilder != nullptr ? m_machineBuilder->GetAuxRamBuffer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetMainRamBuffer
//
////////////////////////////////////////////////////////////////////////////////

const Byte * EmulatorShell::GetMainRamBuffer() const
{
    return m_machineBuilder != nullptr ? m_machineBuilder->GetMainRamBuffer() : nullptr;
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
//  SetPrngSeed
//
//  Replaces the power-on DRAM Prng with one seeded from `seed`. The seed is
//  kept so the trace file can record it: the same seed gives byte-identical
//  DRAM at the first power-on, which is what replaying a startup fault needs.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetPrngSeed (uint64_t seed)
{
    m_prngSeed = seed;
    m_machine.SetPrng (make_unique<Prng> (seed));

    DEBUGMSG (L"[Casso] Cold boot seed: 0x%016llX\n", (unsigned long long) seed);
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
