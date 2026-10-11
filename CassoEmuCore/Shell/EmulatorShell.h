#pragma once

#include "Pch.h"

#include "Seams/Win32Clipboard.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/IntentReplyTracker.h"
#include "Seams/Win32IntentChannel.h"
#include "Shell/Input/ShellKeyRouting.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/CpuManager.h"
#include "Shell/MachineHost.h"
#include "Ui/UiShell.h"
#include "Ui/UiCommandTypes.h"



class ClipboardManager;
class DxuiHwndSource;
class MachineBuilder;
class MachineManager;
class WindowCommandManager;
struct DialogDefinition;
struct MachineConfig;
class SettingsSheet;
class DriveWidget;
class ShellAudio;
class ShellChrome;
class ShellRenderer;
class ShellWindow;
class ShellDeskScene;
class ShellDisks;
class ShellPrinter;
class ShellSettings;
class ShellTapeDeck;
class ShellUpdater;
struct MonitorSpec;
struct ScreenshotFacts;
class DiskManager;
class ThemeManager;





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
//  The application: window, chrome, devices, and the CPU thread that runs the
//  emulated machine.
//
//  It implements two framework interfaces rather than owning two
//  collaborators, and each is a different conversation. IDxuiHostClient is the
//  window's message and paint lifecycle; IDxuiViewportInputSink is where guest
//  keystrokes arrive after the framework has routed them. What the drive
//  chrome calls to mount and eject is the disks component's.
//
//  TWO THREADS run against this object. The CPU thread executes instructions
//  and publishes frames; the UI thread drains messages, renders, and presents.
//  Everything shared between them is atomic or mutex-guarded, and several
//  methods exist purely to marshal work to the right one -- the rule is that
//  UI state is UI-thread-only and device state belongs to the CPU thread.
//
//  Devices are held in an owned list with a separate struct of observer
//  pointers into it, so a machine switch can tear the whole graph down and
//  rebuild it while the window, the chrome, and the renderer survive.
//
//  Much of the class is delegated to managers -- machine, disk, window,
//  clipboard, command -- so this header is largely the seam between them
//  rather than the implementation of any of it.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebugger;
class ShellInput;
struct DebuggerSliceHooks;

class EmulatorShell : public IDxuiHostClient,
                      public IDxuiViewportInputSink,
                      private ICpuMachineCommands
{
public:
    // The input component, for the Settings sheet's Controllers page.
    ShellInput       &  GetInput ()       { return *m_input; }
    const ShellInput &  GetInput () const { return *m_input; }


    EmulatorShell();
    ~EmulatorShell();

    HRESULT Initialize (
        HINSTANCE              hInstance,
        const wstring        & machineName,
        const MachineConfig  & config,
        const string    & disk1Path,
        const string    & disk2Path,
        const string    & tapePath = {});

    int RunMessageLoop();

    void HandleCommand (WORD commandId);

    // State
    bool IsRunning() const { return m_cpuManager.IsRunning(); }
    bool IsPaused() const { return m_cpuManager.IsPaused(); }

    // The emulated machine. Everything about the hardware -- bus, CPU,
    // devices, video modes, which machine this is -- is reached through
    // here rather than off the shell.
    MachineHost & GetMachine() { return m_machine; }

    // The debugger the shell holds.
    ShellDebugger & GetDebugger() { return *m_debugger; }

    // The CPU thread, for a debugger run that has to un-pause it and pause it
    // again from the inside.
    CpuManager & GetCpuManager() { return m_cpuManager; }

    // The CPU thread's handling of one command drained from the queue.
    void DispatchCpuCommand (const EmulatorCommand & cmd);

    // Access bus for test wiring
    MemoryBus & GetBus() { return m_machine.GetMemoryBus(); }

    // Main window HWND. Owned by m_host (DxuiHwndSource in full-
    // ownership mode); EmulatorShell caches it after Create for
    // hot-path callers like the dialog primitive owner-window
    // handoff and the settings panel.
    HWND  GetHwnd () const { return m_hwnd; }

    // Execution trace (--trace switch). SetTraceCapacity must be called
    // before Initialize so the CPU's ring is allocated when the machine
    // is built. The ring is written to a timestamped text file on the
    // desktop by Debug > Save CPU trace (SaveTrace) or by the crash handler
    // (DumpTrace, one-shot); both are no-ops when tracing is off.
    void SetTraceCapacity (size_t capacityEntries) { m_traceCapacity = capacityEntries; }

    // Power-on DRAM seed (--seed). Replaces the one the constructor drew
    // from the clock; must be called before Initialize.
    void     SetPrngSeed (uint64_t seed);
    uint64_t GetPrngSeed () const { return m_prngSeed; }

    // Runs with change notification deliberately broken, so the check made
    // before every write can be measured on its own. Undocumented; set from
    // --no-image-watch and read by the two places that install notification.
    void SetImageWatchDisabled (bool disabled) { m_imageWatchDisabled = disabled; }

    // --debugger: open the debug channel as the CPU thread starts. The machine
    // is not paused, so a client can attach to a program already running.
    void SetOpenDebuggerAtStart (bool open)   { m_openDebuggerAtStart = open; }
    bool IsImageWatchDisabled  () const        { return m_imageWatchDisabled; }

    // Update notification and self-update, including the launch-by-update
    // flags and the Settings > General update toggles.
    ShellUpdater &  GetUpdater();

    // The cassette recorder: its tape manager, flat widget, key latches and
    // tape settings.
    ShellTapeDeck &  GetTapeDeck();

    // The audio output and everything mixed into it.
    ShellAudio &  GetAudio();

    // The printer drain, print preview, status light and print dialog.
    ShellPrinter &  GetPrinter();

    // The Disk II drives: the DiskManager, the drive widgets and their state,
    // the recent-disks list, salvage and the external-change notice.
    ShellDisks &  GetDisks();

    // The 3D desk: the scene, the fullscreen strip, the baked labels, the
    // compass, the recorder's animation and the scene's framing.
    ShellDeskScene &  GetDeskScene();

    // The menu, toolbar, chrome bands, tooltips, notices and switch strip.
    ShellChrome &  GetChrome();

    // The picture: the framebuffer renderer, the framebuffers, the present,
    // the color mode and screenshots.
    ShellRenderer &  GetRenderer();

    // The main window's placement and size, title, icon, accelerators and
    // drag-drop target.
    ShellWindow &  GetWindow();

    // The show state Windows handed wWinMain. Set before Initialize; the
    // first ShowWindow honors it when the launcher asked for something
    // particular, and falls back to the saved placement when it did not.
    void  SetStartupShowCommand (int nCmdShow);

    // Text put in front of the window caption, so one of several open windows
    // can be told from the others at a glance. Undocumented; set from --title
    // and read by UpdateWindowTitle. Set before the window exists, so it does
    // not refresh the caption itself.
    void  SetWindowTitlePrefix (const wstring & prefix);

    // The preferences and the Settings dialog: the global preferences, the
    // config store, the theme catalog and the Settings sheet.
    ShellSettings &  GetSettings();

    bool IsTracing        () const { return m_traceCapacity > 0; }
    void    DumpTrace        (const wstring & reason);
    HRESULT WriteTrace       (const wstring & reason, std::wstring & path);
    static std::wstring GetTraceFolder();

    // / FR-034 / FR-035: split-reset entry points exposed for the
    // menu commands (IDM_MACHINE_RESET / IDM_MACHINE_POWERCYCLE) and any
    // future programmatic callers. SoftReset preserves user RAM and
    // re-runs the 6502 /RESET sequence. PowerCycle re-seeds every DRAM-
    // owning device from the shared Prng before SoftReset (audit S10).
    void SoftReset();
    void PowerCycle();

private:

    //  The shell the EHM notification sink forwards to. One per process; set
    //  by Initialize and cleared by the destructor so a late report cannot
    //  reach a dead object.
    static EmulatorShell *  s_pNotifyShell;
    DxuiMessageResult  OnChar          (WPARAM ch, LPARAM lParam) override;
    DxuiMessageResult  OnCommand       (WORD commandId) override;
    DxuiMessageResult  OnKeyDown       (WPARAM vk, LPARAM lParam) override;
    DxuiMessageResult  OnKeyUp         (WPARAM vk, LPARAM lParam) override;

    // IDxuiViewportInputSink -- the emulator viewport routes its raw
    // keyboard input here (SetWantsAllKeys(true) so even Esc/Tab/arrows
    // arrive). The chrome / settings / meta pre-checks run in OnKeyDown /
    // OnChar before the event reaches the viewport, so these apply the
    // keystroke straight to the Apple ][ keyboard + game port.
    bool  OnViewportKey   (const DxuiKeyEvent   & ev) override;
    bool  OnViewportMouse (const DxuiMouseEvent & ev) override;
    DxuiMessageResult  OnMouseWheel    (WPARAM wParam, LPARAM lParam, bool horizontal) override;
    DxuiMessageResult  OnGesture       (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseMove     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseLeave    () override;
    DxuiMessageResult  OnLButtonDown   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnLButtonUp     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonDown   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonUp     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnAppMessage    (UINT msg, WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnSetCursor     (WORD hitTest) override;
    DxuiMessageResult  OnActivateApp   (bool active) override;
    DxuiMessageResult  OnKillFocus     () override;

    DxuiMessageResult  OnCancelMode    () override;
    DxuiMessageResult  OnMove          (int x, int y) override;
    void               OnEnterSizeMove () override;
    void               OnExitSizeMove  () override;
    void               OnUserWindowStateCommand () override;
    DxuiMessageResult  OnNotify        (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnSize          (UINT widthPx, UINT heightPx) override;
    DxuiMessageResult  OnGetMinMax     (MINMAXINFO * info) override;
    DxuiMessageResult  OnTimer         (UINT_PTR timerId) override;
    void               OnModalLoopTick () override;
    DxuiMessageResult  OnInitMenuPopup (HMENU hMenu, UINT itemIndex, bool isWindowMenu) override;
    DxuiMessageResult  OnNcMouseMove   (LRESULT hitTest, int xScreen, int yScreen) override;
    DxuiMessageResult  OnNcMouseLeave() override;
    DxuiMessageResult  OnNcLButtonDown (LRESULT hitTest, int xScreen, int yScreen) override;
    DxuiMessageResult  OnNcLButtonUp   (LRESULT hitTest, int xScreen, int yScreen) override;
    LRESULT            OnDrawItem      (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // A writing tool stating what its change to a mounted image meant.
    DxuiMessageResult  OnCopyData      (WPARAM sender, LPARAM data) override;
    void               OnDestroy       () override;
    void               OnDpiChanged    (UINT newDpi) override;

    // CPU thread entry point and helpers
    void RunOneFrame();
    void RunCpuThreadFrame();
    void ExecuteCpuSlices();

    // One slice of a frame, watching input held back behind live.
    uint32_t  RunWatchedSlice (uint32_t sliceTarget, const DebuggerSliceHooks & hooks);


    // ICpuMachineCommands: the queued machine-wide commands. SwitchMachine,
    // SoftReset and PowerCycle are members of long standing that already have
    // the interface's signature; these were inline in the dispatch switch
    // before it became CpuCommandDispatcher. The disk, drive-sound and tape
    // commands go to ShellDisks, ShellAudio and ShellTapeDeck.
    void     StepInstruction           () override;
    void     SaveTrace                 () override;
    void     HoldAppleKeysThroughReset (bool openApple, bool closedApple) override;

    // Posts a reset with the Apple keys as they are at this moment, read on
    // the UI thread where the key state is valid. The strip, the toolbar and
    // the menu all come through here so a Ctrl-Open-Apple-Reset cold starts
    // from any of them.
    void     RequestReset ();
    // Machine state files. The two dialogs run on the UI thread and post the
    // chosen path; the save and the load run on the CPU thread. A load of a
    // state another machine saved opens that machine first, and history
    // starts again at the loaded point.
    void  ShowSaveStateDialog ();
    void  ShowLoadStateDialog ();
    void  SaveMachineState    (const std::filesystem::path & path) override;
    void  LoadMachineState    (const std::filesystem::path & path) override;

    static HRESULT       ReadStateFile    (const std::filesystem::path & path, std::vector<Byte> & outBytes);
    static std::wstring  FormatStateError (const wchar_t * verb, const struct MachineStateError & error);


    // Engages or releases the fast-load override from the tape governor.
    void ApplyTapeTurbo ();

    void OnCpuThreadStart();
    void OnCpuThreadStop();
    void WaitForFrameOrMessage();

    // Initialization helpers
    HRESULT CreateEmulatorWindow (HINSTANCE hInstance);

    // Initialize() decomposition -- one single-purpose step each, called
    // in order from Initialize. HRESULT-returning steps propagate genuine
    // infrastructure failure and abort startup via CHR; the void ones have
    // no failable work, or recover in place -- asserting in debug so a dev
    // catches it -- (e.g. corrupt user prefs reset to defaults) rather than
    // abort.
    void    RegisterChromeDock              ();
    void    InitAssetPathsAndStores         ();
    void    PrimeChromeThemeEarly           ();

    HRESULT BuildMachineDevices             (const MachineConfig & config);
    HRESULT InitializeUiShell               ();
    HRESULT WireUiShellChromeAndThemes      ();
    void    RestoreColorTextPref            ();
    void    SubscribeAndActivateTheme       ();
    HRESULT FinishUiShellLayout             ();

    void    ApplyPersistedChromePrefs     ();
    void    ApplyPersistedMachinePrefs    ();

    // Truncating wide->narrow of the machine's name (machine config
    // names are ASCII): the config-store key + lastSelectedMachine pref.
    std::string GetCurrentMachineNameNarrow () const;

    // Connecting and disconnecting storage devices, from the Storage menu and
    // the devices' right-click menus. Both are live, and saved with the
    // machine as Settings saves them.
    void    SetSecondDriveConnected   (bool connected);
    void    SetTapeRecorderConnected  (bool connected);
    void    SaveStorageDevices        ();

    // A drive's (0 or 1) or the recorder's (kStorageMenuRecorder) right-click
    // menu, at a client point.
    static constexpr int  kStorageMenuRecorder = 2;
    void    ShowStorageContextMenu    (int device, int x, int y);
    int     StorageDeviceAt           (int x, int y) const;

    // Bounds-changed callback wired onto m_viewport. Stores the new
    // pixel rectangle and forwards it to m_d3dRenderer.SetTargetBounds
    // so the framebuffer compositor can track where to draw once the
    // swap-chain restructure completes later in Phase 11d.
    void    OnViewportBoundsChanged       (const RECT & boundsPx);

    // WM_KEYDOWN/WM_KEYUP helpers. HandleHostMetaShortcut consumes host-meta
    // keys (menu navigation, paste); ApplyAppleModifierKeys mirrors the host
    // Alt/Shift state onto the //e Open/Closed-Apple and Shift soft switches;
    // The pure VK classifiers moved to AppleKeyMapping, where a test can
    // reach them without a window to press keys into.
    bool        HandleHostMetaShortcut       (WPARAM vk, bool ctrlHeld, bool altHeld);


private:


    // Apple //c case-switch strip. IsApple2c gates its chrome band + input;
    // LayoutSwitchBar positions the strip in its band rect; SyncSwitchBarState
    // pushes the live switch / indicator state onto the control each layout.
    // HandleSwitchBarClick actions a release over one of its parts.
    // Two presentation questions the shell used to answer by asking whether
    // the machine was a //c. It never was a question about the //c: it was
    // about whether the case has switches on it and whether the drive is built
    // in, and the machine is who knows that. Asking the model directly meant
    // every new machine with a switch panel would need another arm added here.
    bool    MachineHasCaseSwitches () const;


public:


    // Queue a command for the CPU thread. Public so non-friend
    // adapters (e.g. SettingsPanel's internal apply sink) can post
    // without needing friend status -- this is already a thin
    // wrapper over the CpuManager queue.
public:
    void PostCommand (WORD id, const string & payload = "");


    // The failure-path counterpart of FlushPendingNotifications. Only
    // CreateEmulatorWindow drains the queue, and a startup that fails before
    // it never gets there. wCassoMain calls this on its failure exit; a system
    // box is the only surface left. Static because it runs after the shell
    // has given up.
    static void  ShowPendingNotificationsWithoutWindow ();


private:
    // Machine switching delegated to MachineManager. Kept as a
    // public delegator so the existing IDM_FILE_OPEN command-queue
    // path can call the shell without learning the manager.
    HRESULT SwitchMachine (const std::wstring & machineName);
    void    ShowMachinePicker();
    const std::wstring &  GetCurrentMachineName () const { return m_machine.GetCurrentMachineName(); }


    // //e/c auxiliary 64 KiB RAM bank (nullptr on ][/][+). Used by the clipboard
    // text scrape to read the aux half of an 80-column screen.
    const Byte *  GetAuxRamBuffer() const;

    // //e/c main RAM as the display sees it, whatever the CPU's banking
    // (nullptr on ][/][+, where the bus serves it). Used by the clipboard text
    // scrape to read the main half.
    const Byte *  GetMainRamBuffer() const;

    // Base directory for user preferences. SettingsPanel.CommitApply
    // uses this as the fallback save path when the unified store is not
    // available.
    const std::wstring &  GetAssetBaseDir () const { return m_machine.GetAssetBaseDir(); }

    // Per-machine pending-strip directory (FR-026):
    // <assetBase>/Machines/<current machine>/PendingPrint.
    fs::path  GetPendingPrintDir () const
    {
        return m_machine.GetPendingPrintDir();
    }

    // The 3D scene renders whenever a skeuo theme is active and the models
    // loaded. The DRIVES are not optional -- they are 3D objects in every
    // skeuo presentation; compact themes keep their flat widgets.
    bool    DeskSceneActive      () const;

    // ...and the monitor on top of that, which the user CAN turn off: the
    // picture then sits on a flat rect at classic sizes with the 3D drive
    // row still composed in the band below it. Everything keyed off the
    // curved glass -- the glass-fill fullscreen, the inverse-projected
    // pointer mapping, the Ctrl+0 solve -- follows this, not DeskSceneActive.
    bool    CrtMonitorActive     () const;

    // A file dropped on a drive or the recorder.
    void    OnFileDropped      (int tag, const std::wstring & path);

    // The operating system's pickers, behind their seam. The shell owns the
    // Win32 implementation; whoever needs to put one up asks for the
    // interface, and a test hands its own in.
    IHostDialogs &  GetHostDialogs () noexcept { return m_hostDialogs; }

    // Keyboard chrome-focus ring (see m_chromeFocusIndex). SetChromeFocusIndex
    // updates the index and refreshes which widget paints its focus visual;
    // HandleChromeFocusKey owns all keydown handling while the ring is active
    // (returns true when the key was consumed); UpdateChromeFocusVisuals
    // pushes the current index into the MainMenu / button / drive widgets.
    void    SetChromeFocusIndex   (int index);
    void    UpdateChromeFocusVisuals ();
    bool    HandleChromeFocusKey  (WPARAM vk);

    // The chrome state a keydown's owner is decided over, and the hand-off to
    // whichever part of the chrome that owner names. Keeping the decision in
    // ShellKeyRouting is what lets OnChar suppress exactly what OnKeyDown
    // claimed without asking the same question a second time.
    ShellKeyRouting::State  GetKeyRoutingState () const;
    void                  DispatchShellKey (ShellKeyOwner owner, WPARAM vk);

public:

    //  How a keydown's owner is decided. Defaults to the real classifier; a
    //  test substitutes its own to drive a verdict the chrome would be
    //  laborious to arrange, and to see that OnKeyDown asks at all. Same
    //  shape as DxuiToolbar::SetClock.
    using KeyOwnerFn = std::function<ShellKeyOwner (const ShellKeyRouting::State &, WPARAM)>;

    void  SetKeyOwnerFn (KeyOwnerFn fn)  { m_keyOwnerFn = std::move (fn); }

private:

    // Shows the supplied dialog modally as a MessageDialog (a DxuiWindow
    // shown via ShowModalDialog). Returns the resultCode of the chosen button,
    // or -1 on close-gesture.
    int     ShowModalDialog      (const DialogDefinition & def);

    // An answer to a tool that asked, carried to the UI thread when the
    // outcome was known on another one.
    struct IntentReplyPost
    {
        HWND                       target = nullptr;
        Win32IntentChannel::Reply  reply;
    };

    // Sends one answer, from the UI thread. A tool that has gone away is
    // not an error: the send simply finds no window.
    void    SendIntentReply (HWND target, const Win32IntentChannel::Reply & reply);

    // Posts one answer to the UI thread from whichever thread knows it.
    void    PostIntentReply (HWND target, const Win32IntentChannel::Reply & reply);

    // Records the folder a handed-off disk lives in, in the known-folder
    // list the disk browser and the picker share.
    void    RecordKnownFolder (const std::string & imagePath);

    // Wires the store's decision report to the tools waiting on a reload.
    void    InstallIntentReplies ();

    // The EHM user-notification sink, installed with SetNotifyFunction so
    // every CHRN / CBRN in the tree reports through Casso's own themed
    // dialog. Nothing had ever installed one, so they all fell through to
    // EhmNotifyUser's built-in path and became raw Win32 message boxes.
    //
    // Static because SetNotifyFunction takes a plain function pointer; it
    // forwards to the one live shell.
    static void  NotifyUser (const wchar_t * message);

    // Holds a report raised before there is a window to show it in. Public
    // and static because wCassoMain installs the sink before the shell exists.
    static void  QueueNotification (const std::wstring & message);

    // Shows one notification, marshaling as needed. Callable from any
    // thread: a flush that fails on the CPU thread reports through here.
    void         ShowNotification (const std::wstring & message);

    // Hands a notification to the message pump instead of showing it here.
    // For callers that are themselves inside a message being handled -- a
    // dialog's OK handler, say -- where a modal opened inline would run a
    // nested loop against a window still mid-commit.
    void         PostNotification (const std::wstring & message);

    // Replays notifications raised before the window existed. Startup reports
    // a bad prefs file before there is anything to parent a dialog to, and a
    // queued message that appears a moment later beats a bare Win32 box.
    void         FlushPendingNotifications ();

    // Render a "simple" dialog (text + buttons + an optional Info /
    // Warning / Error glyph icon -- no custom body, tick, hyperlinks,
    // app-bitmap icon, or resizable mode) as a MessageDialog (DxuiWindow
    // shown via ShowModalDialog). Returns the chosen button's resultCode (or
    // def.closeBoxResult / -1 on a close gesture).
    int          ShowSimpleDialogViaDxui (const DialogDefinition & def);

    // The shell's components, each owned here and reached through its
    // getter. Held by pointer so this header needs only their names.
    std::unique_ptr<ShellUpdater>    m_updater;

    // MachineManager, WindowCommandManager and the shell's components touch
    // enough shell state during construction and command dispatch that
    // friend declarations are the pragmatic seam; no new global state is
    // introduced.
    friend class ShellChrome;
    friend class ShellRenderer;
    friend class ShellWindow;
    friend class ShellDeskScene;
    friend class ShellDisks;
    friend class ShellPrinter;
    friend class ShellSettings;
    friend class ShellTapeDeck;
    friend class ShellUpdater;
    friend class MachineManager;
    friend class WindowCommandManager;
    friend class SettingsSheet;
    friend class SettingsApplyController;
    friend class SettingsDisplayCrtBridge;
    friend class SettingsMachineCatalog;

    HINSTANCE  m_hInstance             = nullptr;
    HWND       m_hwnd                  = nullptr;

    // Authoritative per-window DPI scaler. Mirrors the one inside
    // DxuiHwndSource; updated from OnDpiChanged and seeded after
    // m_host->Create() returns. The chrome-band dock scales its band
    // thicknesses through this member.
    DxuiDpiScaler       m_scaler;

    //  The emulated machine: bus, CPU, devices, video modes, and the
    //  configuration and identity that say which machine this is. Everything
    //  the emulator wraps around a machine stays on this class and reaches
    //  the machine through here.
    MachineHost             m_machine;

    size_t                 m_traceCapacity = 0;       // --trace ring size (entries); 0 = off
    uint64_t               m_prngSeed      = 0;       // power-on DRAM seed; --seed overrides
    bool                   m_imageWatchDisabled = false;  // --no-image-watch (undocumented)
    bool                   m_openDebuggerAtStart = false; // --debugger
    std::atomic<bool>      m_traceDumped { false };   // one-shot guard for DumpTrace
   

    // The host audio output and every source mixed into it.
    std::unique_ptr<ShellAudio>  m_audio;

    // The preferences, the store that keeps them, the UI thread's file system,
    // the theme catalog and the Settings sheet. Declared early so it outlives
    // every component and manager that holds a reference into it.
    std::unique_ptr<ShellSettings>  m_settings;

    // The menu, toolbar, chrome bands, tooltips, notices and switch strip,
    // and the chrome theme they are painted in.
    std::unique_ptr<ShellChrome>  m_chrome;

    // The framebuffer renderer, the CPU and UI framebuffers, the render-skip
    // gate and frame clock, the color mode, the present and screenshots.
    // Declared ahead of the host and the CPU manager, so it outlives both.
    std::unique_ptr<ShellRenderer>  m_renderer;

    // The window's placement and size, title, icon, accelerators, OLE and
    // drag-drop target.
    std::unique_ptr<ShellWindow>  m_window;

    void  WireToolbarPickers               ();

protected:

    //  Reachable by a test subclass, the way TestCpu reaches Cpu's. These two
    //  are the whole observable surface of the keystroke path: whether a
    //  claimed key armed the swallow, and whether a character reached the
    //  guest. Everything else here stays private.

    // Set when OnKeyDown claims a keydown whose synthesized WM_CHAR must not
    // reach the guest keyboard latch -- an open picker, the focus ring, Esc
    // leaving paddle mode, or a host-meta shortcut (the ^V of a paste would
    // land in the input line ahead of the pasted text). One shot: OnChar
    // consumes it, and OnKeyDown clears it on the way in.
    bool                       m_swallowMetaChar = false;

    // The classifier OnKeyDown routes through. A member rather than a direct
    // call so a test can substitute one; see SetKeyOwnerFn.
    KeyOwnerFn                 m_keyOwnerFn      = &ShellKeyRouting::GetKeyOwner;

    // Apple ][ framebuffer viewport inside the host's root panel. Sized by
    // EmulatorShell whenever chrome layout changes; the bounds-changed
    // callback forwards the new rectangle to m_d3dRenderer.SetTargetBounds so
    // the renderer knows where to composite the framebuffer. Non-owning
    // pointer; the panel tree owns the DxuiViewport instance. Every key and
    // character the guest receives travels through it, which is why a test
    // needs to see it.
    DxuiViewport             * m_viewport        = nullptr;

    // The flat drive band's row, for the band layout tests: lay the row out
    // in a client of the given size with no band below it, and read back
    // where the drives and the recorder landed.
    void  LayoutDriveRowForTest   (int clientW, int clientH, UINT dpi, int visibleCount);
    RECT  GetDriveRectForTest     (size_t drive) const;
    RECT  GetTapeAnchorForTest    () const;
    void  SetRecorderAttachedForTest (bool attached);

private:

    // The 3D desk, its fullscreen strip, labels, compass and framing.
    std::unique_ptr<ShellDeskScene>  m_scene;

    // DxuiHwndSource running in full-ownership mode. Owns the main
    // HWND (registers WNDCLASS "CassoWindow", calls CreateWindowExW,
    // and applies DwM rounded-corners / immersive-dark / extended
    // frame). Created with `createSwapChain = true` so the host owns
    // the D3D11 device + DXGI swap chain and runs the panel-tree paint
    // pump; the Apple ][ framebuffer renderer composites into that back
    // buffer via the host's before-present hook, and chrome paints on
    // top via the adopted controls. The host owns the caption (title +
    // icon + min/max/close) itself and classifies caption / system-button
    // / resize-edge NC hits, so no SetHitTestDelegate is installed.
    // EmulatorShell is the IDxuiHostClient so all consumer-side Win32
    // messages (WM_KEYDOWN, WM_COMMAND, WM_SIZE, ...) dispatch through the
    // OnXxx overrides above.
    std::unique_ptr<DxuiHwndSource>  m_host;

    RECT                             m_viewportBoundsPx  = {};

    // True between WM_ENTERSIZEMOVE and WM_EXITSIZEMOVE. Inside the OS drag
    // loop the window changes once per pixel and the end of the loop is the
    // placement; outside it -- a snap layout, Win+arrow, a monitor removed --
    // one change is one placement, and nothing else reports those.
    bool                     m_inSizeMove     = false;
    // The last maximized size the OS offered, so the trace prints a change
    // rather than a line per pointer tick.
    POINT                    m_lastMaxSize    = { 0, 0 };


    // Tools waiting on an insert or a reload, and hand-offs whose folder is
    // recorded once the mount succeeds.
    IntentReplyTracker     m_intentReplies;

    // Native UI shell. Owns the painter, text renderer, hit-tester,
    // focus manager, animation broker, and input translator. Wired
    // onto D3DRenderer's after-blit hook so chrome composites every
    // frame between the emulator blit and Present.
    UiShell                    m_uiShell;


    // The printer drain, the print preview, the status light and the print
    // dialog. Declared after the machine so the drain thread is torn down
    // (joined) before the card it drains.
    std::unique_ptr<ShellPrinter> m_printer;

    // CPU-thread lifecycle, run/pause/step transitions, the UI -> CPU
    // command queue, and the paste buffer all live on CpuManager. The
    // shell wires its per-frame and per-command callbacks at startup
    // and otherwise reads the manager's transition state through the
    // IsRunning() / IsPaused() / GetSpeedMode() accessors.
    CpuManager                    m_cpuManager;

    // The debugger: its window and session, reverse execution and the
    // replays of history, and the debug channel. Declared after the CPU
    // manager it drives, so it is destroyed before it.
    class                           DebuggerHost;
    std::unique_ptr<DebuggerHost>   m_debuggerHost;   // what the debugger asks of the shell; outlives it
    std::unique_ptr<ShellDebugger>  m_debugger;

    // Host input to the guest. Declared after the debugger, whose gate it
    // asks behind live, so it is destroyed first.
    class                           InputHost;
    std::unique_ptr<InputHost>      m_inputHost;      // what the input component asks of the shell; outlives it
    std::unique_ptr<ShellInput>     m_input;

    uint32_t                      m_cyclesPerFrame  = 17050;


    // Keyboard focus ring across the painted chrome ("Z" Tab order, left
    // to right, top to bottom): -1 = guest (//e has focus), 0..6 = the
    // seven menu titles File..Help, 7 = joystick-mode button, 8/9 = drive
    // widgets 1/2. Entered via F10 or a mouse click on a chrome element;
    // exited via Esc/F10 or a click in the emulator viewport. While active
    // (>= 0) every keydown is consumed so letters never leak to the //e.
    int             m_chromeFocusIndex      = -1;

    // The host services the clipboard manager and the dialogs go through.
    // Declared ahead of the manager, which holds a reference to the clipboard.
    Win32Clipboard                            m_hostClipboard;
    Win32HostDialogs                          m_hostDialogs;


    std::unique_ptr<ClipboardManager>         m_clipboardManager;
    std::unique_ptr<ShellDisks>               m_disks;
    std::unique_ptr<ShellTapeDeck>            m_tapeDeck;
    std::unique_ptr<MachineBuilder>           m_machineBuilder;
    std::unique_ptr<MachineManager>           m_machineManager;
    std::unique_ptr<WindowCommandManager>     m_windowCommandManager;
};




