#pragma once

#include "Pch.h"

#include "Audio/Disk2AudioSource.h"
#include "Audio/DriveAudioMixer.h"
#include "Audio/PrinterAudioSource.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/ComponentRegistry.h"
#include "Core/EmuCpu.h"
#include "Core/InterruptController.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "D3DRenderer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/IAciaEndpoint.h"
#include "Print/PrinterWorker.h"
#include "Shell/ClipboardManager.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "Shell/MachineManager.h"
#include "Shell/WindowCommandManager.h"
#include "Shell/WindowManager.h"
#include "Ui/Chrome/Apple2cSwitchBar.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Chrome/InputDeviceSelector.h"
#include "Ui/Chrome/CommandToolbar.h"
#include "Widgets/DxuiHudNotice.h"
#include "Widgets/DxuiShadowedText.h"
#include "Widgets/DxuiOrbitControl.h"
#include "Ui/Chrome/MainMenu.h"
#include "Ui/ColorUtil.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Disk2DebugPanel.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/IDriveCommandSink.h"
#include "Ui/InputDebugPanel.h"
#include "Ui/Scene/DeskScene.h"
#include "Ui/Scene/DeskSceneHitTester.h"
#include "Ui/Scene/FullscreenStripState.h"
#include "Ui/ThemeManager.h"
#include "Ui/UiShell.h"
#include "Widgets/DxuiTooltip.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiSurface.h"
#include "UiCommandTypes.h"
#include "Video/CharacterRomData.h"
#include "Video/VideoOutput.h"
#include "Video/VideoTiming.h"
#include "WasapiAudio.h"
#include "Window/DxuiHwndSource.h"
#include "Widgets/DxuiActionBanner.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Window/IDxuiHostClient.h"
#include "Core/DxuiAbsoluteLayout.h"
#include "Core/DxuiDockLayout.h"
#include "Core/DxuiViewport.h"



class DxuiHwndSource;
class SettingsSheet;
class JsonValue;
class SalvageDialogContent;
struct MonitorSpec;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBand
//
//  Zero-render IDxuiControl whose only job is to carry a docked chrome
//  band's pixel thickness in its GetBounds() so DxuiDockLayout can arrange
//  the emulator viewport around the title bar, nav strip, and drive bar.
//  Never painted -- EmulatorShell / the host own chrome rendering; these
//  bands exist purely to feed the dock's inset math (replacing the old
//  LayoutManager edge-contributor model).
//
////////////////////////////////////////////////////////////////////////////////

class ChromeBand : public IDxuiControl
{
public:
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        UNREFERENCED_PARAMETER (scaler);
        SetBounds (boundsDip);
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        UNREFERENCED_PARAMETER (painter);
        UNREFERENCED_PARAMETER (text);
        UNREFERENCED_PARAMETER (theme);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
//  The application: window, chrome, devices, and the CPU thread that runs the
//  emulated machine.
//
//  It implements three framework interfaces rather than owning three
//  collaborators, and each is a different conversation. IDxuiHostClient is the
//  window's message and paint lifecycle; IDriveCommandSink is what the drive
//  chrome calls to mount and eject; IDxuiViewportInputSink is where guest
//  keystrokes arrive after the framework has routed them. Implementing them
//  here is what keeps the shell the single place those three meet.
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

class EmulatorShell : public IDxuiHostClient, public IDriveCommandSink, public IDxuiViewportInputSink
{
public:
    EmulatorShell();
    ~EmulatorShell();

    // The show state Windows handed wWinMain. Set before Initialize; the
    // first ShowWindow honors it when the launcher asked for something
    // particular, and falls back to the saved placement when it did not.
    void  SetStartupShowCommand (int nCmdShow) { m_startShowCmd = nCmdShow; }

    HRESULT Initialize (
        HINSTANCE              hInstance,
        const wstring        & machineName,
        const MachineConfig  & config,
        const string    & disk1Path,
        const string    & disk2Path);

    int RunMessageLoop();

    // Runs ONE UI-thread render cycle: latch the newest emulator framebuffer,
    // push CRT params, advance chrome / panel animation, refresh the printer
    // indicator + live preview (which also paces the printer audio), and -- if
    // anything needs presenting -- drive a synchronous WM_PAINT. Returns true iff
    // it presented (caller idle-sleeps when false). Factored out of RunMessageLoop
    // so the host's OnModalLoopTick can pump it while the OS modal move / size
    // loop owns the thread (otherwise the preview + sound freeze on a title-bar
    // hold, then jump on release). The host owns the keep-alive timer; the shell
    // only supplies this per-frame work.
    bool TryPresentUiFrame();

    void HandleCommand (WORD commandId);

    // State
    bool IsRunning() const { return m_cpuManager.IsRunning(); }
    bool IsPaused() const { return m_cpuManager.IsPaused(); }

    // Access bus for test wiring
    MemoryBus & GetBus() { return m_memoryBus; }

    // Main window HWND. Owned by m_host (DxuiHwndSource in full-
    // ownership mode); EmulatorShell caches it after Create for
    // hot-path callers like the dialog primitive owner-window
    // handoff and the settings panel.
    HWND  GetHwnd () const { return m_hwnd; }

    // Execution trace (--trace switch). SetTraceCapacity must be called
    // before Initialize so the CPU's ring is allocated when the machine
    // is built. DumpTrace writes the recorded ring to a timestamped text
    // file in the working directory, showing a progress window; it is
    // called both on graceful exit and from the crash handler, and is a
    // no-op (and self-guards against a double dump) when tracing is off.
    void SetTraceCapacity (size_t capacityEntries) { m_traceCapacity = capacityEntries; }

    // Runs with change notification deliberately broken, so the check made
    // before every write can be measured on its own. Undocumented; set from
    // --no-image-watch and read by the two places that install notification.
    void SetImageWatchDisabled (bool disabled) { m_imageWatchDisabled = disabled; }
    bool IsImageWatchDisabled  () const        { return m_imageWatchDisabled; }
    bool IsTracing        () const { return m_traceCapacity > 0; }
    void DumpTrace        (const wstring & reason);

    // / FR-034 / FR-035: split-reset entry points exposed for the
    // menu commands (IDM_MACHINE_RESET / IDM_MACHINE_POWERCYCLE) and any
    // future programmatic callers. SoftReset preserves user RAM and
    // re-runs the 6502 /RESET sequence. PowerCycle re-seeds every DRAM-
    // owning device from the shared Prng before SoftReset (audit S10).
    void SoftReset();
    void PowerCycle();

    // Spec-006 / FR-001 / FR-024. View -> Disk II Debug... command
    // entry point. On first call: lazy-create the modeless dialog,
    // attach it as the sink on the active Disk II controller
    // (controller #0 per FR-017) AND on that controller's
    // Disk2AudioSource. On subsequent calls: show + bring to front.
    void OpenDisk2DebugDialog();
    void OpenInputDebugDialog();
    void OpenSettings();

    // Spec-006 bug 15. SwitchMachine destroys and recreates the
    // controller + audio source while the modeless debug dialog
    // (if open) holds raw pointers into the now-defunct old
    // components. Call this AFTER the new components are wired
    // up so the dialog re-attaches as the controller event sink
    // and the active drive's audio-event sink on the new objects.
    // No-op when the dialog has never been opened.
    void AttachDebugSinksIfOpen();

    // Spec-006 / FR-004a. Re-zero the Uptime column anchor on every
    // //e SoftReset / PowerCycle. The anchor is shell-owned (lives
    // across dialog opens) but read by the dialog via
    // GetUptimeAnchor() on each WM_TIMER drain.
    void ResetUptimeAnchor() noexcept
    {
        m_uptimeAnchor = std::chrono::steady_clock::now();

        if (m_disk2DebugPanel != nullptr)
        {
            // ResetUptimeAnchor runs on the CPU thread. Touching the
            // panel's event deque / DxuiListView rows here would race the
            // render thread's per-frame drain and corrupt the row Cells,
            // so marshal the re-anchor + clear onto the render thread.
            m_disk2DebugPanel->RequestResetAnchor (m_uptimeAnchor);
        }

        if (m_inputDebugPanel != nullptr)
        {
            m_inputDebugPanel->RequestResetAnchor (m_uptimeAnchor);
        }
    }

    std::chrono::steady_clock::time_point GetUptimeAnchor() const noexcept
    {
        return m_uptimeAnchor;
    }

    // IDriveCommandSink
    // UI-thread entry points the drive widgets call into when the user
    // drops a file, clicks-to-browse, or clicks the eject affordance.
    // Both forms route through the existing IDM_DISK_* command queue so
    // the actual mount/eject runs on the CPU thread same as the menu
    // path. `Mount` accepts only slot 6 (the integrated Disk II);
    // unknown slots are E_INVALIDARG and the mount is dropped.
    HRESULT Mount  (int slot, int drive, const std::wstring & path) override;
    void    Eject  (int slot, int drive) override;

    // UI helper: open the drive door for visual feedback, show the
    // file-open dialog, then close the door again. Mount-on-success
    // is handled by the existing PromptForDiskImage path; this
    // method just owns the door visual.
    void    BrowseForDisk (int drive);

private:
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
    DxuiMessageResult  OnSetCursor     (WORD hitTest) override;
    DxuiMessageResult  OnActivateApp   (bool active) override;
    DxuiMessageResult  OnKillFocus     () override;

    // Release the guest keyboard latch + auto-repeat + modifiers. Called on
    // focus loss: the matching key-ups will never arrive once focus moves.
    void               ReleaseGuestKeys ();
    DxuiMessageResult  OnCancelMode    () override;
    DxuiMessageResult  OnMove          (int x, int y) override;
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
    void RenderFramebuffer();
    void DispatchCpuCommand (const EmulatorCommand & cmd);

    // Presentation pacing + render-skip gate (rationale in the .cpp).
    // ShouldPublishFrame throttles rasterize/publish to ~60 Hz at Maximum
    // speed; the Compute* signatures feed the dirty-tracked render gate that
    // skips re-rasterizing an unchanged screen (video RAM dirty + mode +
    // flash phase + color).
    bool      ShouldPublishFrame  ();
    uint32_t  ComputeVideoModeSig ();
    bool      ComputeFlashOn      ();
    uint64_t  ComputeColorSig     ();

    // Stores the live drive-audio gains and applies them to every
    // registered Disk2AudioSource. Must run on the CPU thread (the same
    // thread that mixes audio), so callers marshal through the command
    // queue (IDM_AUDIO_DRIVE_VOLUMES) rather than calling it directly.
    void SetDriveAudioVolumes (float motor, float head, float door);

    // Stores a live per-drive stereo pan (drive 0/1, value -1..+1) and
    // applies it to the matching Disk2AudioSource. Like the volumes, this
    // must run on the CPU thread, so callers marshal through the command
    // queue (IDM_AUDIO_DRIVE_PAN).
    void SetDriveAudioPan (int drive, float pan);

    // Auditions a drive sound on demand (settings play buttons). drive =
    // 0/1, kind matches Disk2AudioSource::TestSoundKind. CPU-thread only,
    // marshaled via IDM_AUDIO_DRIVE_TEST.
    void PlayDriveTestSound (int drive, int kind);

    // Decodes the drive, printer and PSG sounds to the host device's sample
    // rate. CPU thread only.
    void LoadAudioAssetsForDeviceRate();

    void OnCpuThreadStart();
    void OnCpuThreadStop();
    void PublishFramebuffer();
    void WaitForFrameOrMessage();
    void DestroyFrameReadyEvent();
    void UpdateWindowTitle();

    // Initialization helpers
    HRESULT CreateEmulatorWindow (HINSTANCE hInstance);
    void    ReconcileInitialClientSize ();

    // Initialize() decomposition -- one single-purpose step each, called
    // in order from Initialize. HRESULT-returning steps propagate genuine
    // infrastructure failure and abort startup via CHR; the void ones have
    // no failable work, or recover in place -- asserting in debug so a dev
    // catches it -- (e.g. corrupt user prefs reset to defaults) rather than
    // abort.
    void    RegisterChromeDock              ();
    void    InitAssetPathsAndStores         ();
    void    AllocateFramebuffers            ();
    void    PrimeChromeThemeEarly           ();
    HRESULT BuildMachineDevices             (const MachineConfig & config);
    HRESULT InitializeRenderer              ();
    HRESULT InitializeUiShell               ();
    HRESULT WireUiShellChromeAndThemes      ();
    void    RestoreInputAndColorPrefs       ();
    void    RecordActiveMachineSelection    ();
    void    SubscribeAndActivateTheme       ();
    HRESULT FinishUiShellLayout             ();
    void    InstallDragDropTarget           ();

    // Persisted per-machine $cassoUiPrefs. LoadMachineUiPrefs reads +
    // merges the machine JSON, handing back the "$cassoUiPrefs" object in
    // outUiPrefs -- or null when it is absent OR unreadable/corrupt, both
    // recovered to defaults, never fatal. Each Apply* helper loads its own
    // copy and seeds one subsystem (chrome vs audio).
    void    LoadMachineUiPrefs            (JsonValue & outDoc, const JsonValue * & outUiPrefs);

    // The monitor this machine ships with, from its config rather than from
    // its name. Both the desk scene's mesh and the screen's default color
    // come from the one answer, so they cannot disagree about what is
    // standing on the desk.
    const MonitorSpec &  ResolveMonitorForCurrentMachine();
    void    ApplyPersistedChromePrefs     ();
    void    ApplyPersistedAudioPrefs      ();

    // Truncating wide->narrow of m_currentMachineName (machine config
    // names are ASCII): the config-store key + lastSelectedMachine pref.
    std::string GetCurrentMachineNameNarrow () const;

    // Drives the host's root panel layout for the Apple ][ viewport
    // child. Computes the framebuffer rectangle (client minus chrome
    // bands) via the DxuiDockLayout and invokes m_viewport->Layout,
    // which fires OnViewportBoundsChanged when the rectangle differs
    // from the last value reported.
    void    UpdateViewportLayout          (int widthPx, int heightPx);

    // Chrome-band sizing via DxuiDockLayout (replaces LayoutManager).
    // SyncChromeBands stamps each band's GetBounds() with its DPI-scaled
    // pixel thickness. ComputeViewportRect docks the bands + center and
    // returns the middle (emulator viewport) rect. GetClientSizeForCenterPx
    // is the inverse: given a desired center size in px, the client size
    // that hosts it. GetClientSizeForFramebufferPx DPI-scales a DIP
    // framebuffer grid first, then adds the chrome insets.
    void    SyncChromeBands               ();
    RECT    ComputeViewportRect           (int widthPx, int heightPx);

    // The emulator viewport (CRT output area) in *screen* pixels: the middle
    // rect from ComputeViewportRect at the current back-buffer size, mapped
    // through the main window's client origin. The Settings live-preview
    // compositor (#8) intersects this with the (composited) sheet window to
    // punch a see-through hole revealing the running emulator behind the sheet.
    RECT    GetEmulatorContentScreenRect  ();

    // Re-run the chrome layout at the current client size after a machine
    // switch: adding/removing the Disk ][ controller changes the drive band +
    // widgets + hit-test map, but no WM_SIZE fires when the window size itself
    // is unchanged, so OnSize would never re-evaluate it. See the
    // WM_APP_DXUI_UPDATE_TITLE handler (the switch-completion signal).
    void    ReflowChromeForMachineChange  ();

    // Whether the second (external) drive-mount widget should be visible.
    // Always true for machines whose second drive is fixed hardware; on the
    // //c (banked system ROM) the external drive is an optional add-on, shown
    // only when m_externalDriveConnected. The drive-layout paths consult this
    // to hide m_driveChrome[1] and skip its hit rect when disconnected.
    bool    ShouldShowExternalDrive       () const;

    SIZE    GetClientSizeForCenterPx      (int centerWidthPx, int centerHeightPx);
    SIZE    GetClientSizeForFramebufferPx (int framebufferWidthDp, int framebufferHeightDp);

    // Bounds-changed callback wired onto m_viewport. Stores the new
    // pixel rectangle and forwards it to m_d3dRenderer.SetTargetBounds
    // so the framebuffer compositor can track where to draw once the
    // swap-chain restructure completes later in Phase 11d.
    void    OnViewportBoundsChanged       (const RECT & boundsPx);

    // WM_KEYDOWN/WM_KEYUP helpers. HandleHostMetaShortcut consumes host-meta
    // keys (menu navigation, paste, reset); ApplyAppleModifierKeys mirrors
    // the host Alt/Shift state onto the //e Open/Closed-Apple and Shift soft
    // switches; MapVkToAppleControlCode and IsArrowVk are pure VK classifiers.
    bool        HandleHostMetaShortcut  (WPARAM vk, bool ctrlHeld, bool altHeld);
    void        ApplyAppleModifierKeys  (WPARAM vk, bool keyDown);
    static Byte MapVkToAppleControlCode (WPARAM vk);
    static bool IsArrowVk               (WPARAM vk);

    // Stage the emulated joystick axes from the host arrow keys.
    void    UpdateJoystickAxesFromKeys ();

    // Stage the emulated joystick fire buttons from the host X / Y keys.
    void    UpdateJoystickButtonsFromKeys ();

    // Set the host input mapping mode (Off / Joystick / Paddle): persists
    // it, re-syncs the game port (resolving joystick axes / buttons from
    // current keys, centering on leave), and starts or stops mouse capture
    // for Paddle mode.
    // Split input model. SetArrowsJoystick / SetPointerMapping set
    // the two orthogonal axes independently (menu items); SetInputMappingMode
    // applies a combined PRESET (button cycle + legacy callers): Joystick =
    // keys-only, Paddle/Mouse = pointer-only, Off = both off. Paddle<->Mouse
    // stay exclusive (both claim the host pointer).
    void    SetInputMappingMode (InputMappingMode mode);
    void    SetArrowsJoystick   (bool on);
    void    SetPointerMapping   (InputMappingMode pointer);   // Off/Paddle/Mouse

    // The single mode the legacy toggle button displays: the pointer
    // mapping when active, else Joystick when the keys mapping is on.
    InputMappingMode  GetDisplayInputMode() const
    {
        return (m_pointerMode != InputMappingMode::Off) ? m_pointerMode
             : (m_arrowsJoystick ? InputMappingMode::Joystick : InputMappingMode::Off);
    }

    // With a connected mouse and no pointer mapping chosen, the //c
    // defaults Pointer to Mouse (runtime nudge, not persisted; invisible
    // until mouse software runs thanks to the firmware-live gate).
    void    ApplyDefaultPointerForMachine();

private:
    // Window-placement and chrome-layout helpers. Every reader is an
    // EmulatorShell method, so they belong to the class rather than to
    // the translation unit.
    static void  LayoutDriveWidgetsInCommandBar (
        std::array<DriveWidget, 2>  & driveChrome,
        int                           bottomInsetPx,
        int                           clientW,
        int                           clientH,
        UINT                          dpi,
        float                         sceneScale);

    static bool  TryGetCursorMonitorWorkArea (RECT & outWork, HMONITOR & outMonitor);

    static void  CenterInWorkArea (
        const RECT & work,
        int          windowW,
        int          windowH,
        LONG       & outX,
        LONG       & outY);

    static HRESULT  LoadIconAsPremulBgra (
        HINSTANCE               hInstance,
        int                     iconResourceId,
        int                     sizePx,
        std::vector<uint32_t> & outPixels,
        int                   & outW,
        int                   & outH);

    void    SyncInputModeUi();
    void    SyncSelectorState();

    // Apple //c case-switch strip. IsApple2c gates its chrome band + input;
    // LayoutSwitchBar positions the strip in its band rect; SyncSwitchBarState
    // pushes the live switch / indicator state onto the control each layout.
    // HandleSwitchBarClick actions a release over one of its parts.
    bool    IsApple2c              () const { return m_apple2cRomBank != nullptr; }
    void    LayoutSwitchBar        (UINT dpi);
    void    SyncSwitchBarState     ();
    void    HandleSwitchBarClick   (Apple2cSwitchBar::Part part);
    // Persist one case-switch latch ("eightyColumnSwitch" / "keyboardDvorak")
    // into the current machine's $cassoUiPrefs so it survives across runs.
    void    PersistSwitchState     (const char * key, bool value);
public:

    // Radio-group toggle for the Machine-menu items: selects `target`, or
    // turns mapping Off if `target` is already the active mode.
    void    ToggleInputMappingMode (InputMappingMode target);

    // //c mouse mode. True while Mouse mode is selected AND the
    // current machine has the IOU mouse — every runtime consumer guards on
    // this, so a persisted Mouse mode on a mouse-less machine is inert.
    bool    IsGuestMouseActive     () const;

    // True when guest software has actually turned the mouse on: the
    // firmware's SETMOUSE programs ENBXY through the IOU for every active
    // mode, a hardware sequence ($C079 -> $C059 -> $C078) that garbage RAM
    // cannot fake. Gates the cursor-hide and button capture so the host
    // pointer never vanishes (or gets swallowed) while nothing mouse-aware
    // is running — which in turn makes Mouse mode safe to leave on.
    bool    IsGuestMouseLive       () const;

    // Absolute host→guest mapping: the host position inside the emulator
    // viewport maps proportionally into the firmware's live clamp window
    // (read from the slot-7 screen holes along with the current position),
    // and the delta is queued as movement units. Self-correcting — any units
    // the firmware clamps away are re-derived from the holes on the next
    // move. No-op until the guest app has initialized the mouse firmware
    // (garbage holes fail the sanity checks).
    void    UpdateGuestMouseFromHost (int xPx, int yPx);

    // Advance the input mapping mode Off -> Joystick -> Paddle -> Off,
    // routed from the drive-bar widget, the Machine menu, and Ctrl+Shift+J.
    void    CycleInputMappingMode ();

    // Paddle-mode mouse capture. Start hides + confines the cursor and
    // begins relative tracking (no-op unless the mode is Paddle and the
    // window is focused); Stop restores the cursor and releases the clip.
    // UpdatePaddleFromMouse maps one WM_MOUSEMOVE into the held paddle
    // axes via the recenter-on-move trick. PushPaddleButtons stages the
    // mouse buttons onto the emulated fire buttons.
    void    StartPaddleCapture     ();
    void    StopPaddleCapture      ();
    void    UpdatePaddleFromMouse  (int xClient, int yClient);
    void    PushPaddlePosition     ();
    void    PushPaddleButton       (int index, bool pressed);

    // Queue a command for the CPU thread. Public so non-friend
    // adapters (e.g. SettingsPanel's internal apply sink) can post
    // without needing friend status -- this is already a thin
    // wrapper over the CpuManager queue.
public:
    void PostCommand (WORD id, const string & payload = "");

    // Single-step the CPU from the UI thread. Only safe when the
    // CPU thread is paused (provably idle on pauseCV.wait); the
    // caller must enforce that precondition. Bypasses PostCommand
    // because the CPU thread can't drain its queue while paused.
    void StepInstructionWhilePaused ();

    // The failure-path counterpart of FlushPendingNotifications. Only
    // CreateEmulatorWindow drains the queue, and a startup that fails before
    // it never gets there. wWinMain calls this on its failure exit; a system
    // box is the only surface left. Static because it runs after the shell
    // has given up.
    static void  ShowPendingNotificationsWithoutWindow ();

private:
    // Machine switching delegated to MachineManager. Kept as a
    // public delegator so the existing IDM_FILE_OPEN command-queue
    // path can call the shell without learning the manager.
    HRESULT SwitchMachine (const std::wstring & machineName);
    void    ShowMachinePicker();
    const std::wstring &  GetCurrentMachineName () const { return m_currentMachineName; }

    // One-line printer summary for the Settings > Printing info banner: what
    // printer this machine emulates and how it connects, or that it has none.
    std::wstring  GetPrinterBannerMessage () const;

    // //e/c auxiliary 64 KiB RAM bank (nullptr on ][/][+). Used by the clipboard
    // text scrape to read the aux half of an 80-column screen.
    const Byte *  GetAuxRamBuffer() const;

    // Accessor used by the Settings → Theme preview to copy the live
    // emulator framebuffer into the mock window. The UI framebuffer is
    // the post-CRT-effects pixel buffer the chrome composes on top of;
    // returning a raw pointer is safe because the chrome composition
    // pass runs synchronously after the framebuffer is published.
    const uint32_t *  GetUiFramebufferPixels () const
    {
        return m_uiFramebuffer.empty() ? nullptr : m_uiFramebuffer.data();
    }

    // Accessor for the Settings → Theme preview so it can render the
    // basename label with the actual filename of whatever disk image is
    // currently mounted in each drive (or an empty string if the drive
    // is empty). Index 0 is drive 1, index 1 is drive 2.
    const std::wstring &  GetMountedImagePath (int driveIndex) const
    {
        static const std::wstring  s_kEmpty;

        if (driveIndex < 0 || driveIndex >= (int) m_driveWidgetState.size())
        {
            return s_kEmpty;
        }

        return m_driveWidgetState[(size_t) driveIndex].mountedImagePath;
    }

    // Write-protect breakdown for a drive, read from the live per-drive
    // widget state (refreshed each frame by DiskManager::UpdateDriveWidgets).
    // Used by the Settings → Theme preview so its sample drive shows the
    // padlock cue for whatever is actually mounted. Index 0 is drive 1.
    WriteProtectInfo  GetDriveWriteProtect (int driveIndex) const
    {
        if (driveIndex < 0 || driveIndex >= (int) m_driveWidgetState.size())
        {
            return WriteProtectInfo();
        }

        return m_driveWidgetState[(size_t) driveIndex].writeProtect;
    }

    // Base directory for user preferences. SettingsPanel.CommitApply
    // uses this as the fallback save path when the unified store is not
    // available.
    const std::wstring &  GetAssetBaseDir () const { return m_assetBaseDir; }

    // Per-machine pending-strip directory (FR-026):
    // <assetBase>/Machines/<current machine>/PendingPrint.
    fs::path  GetPendingPrintDir () const
    {
        return fs::path (m_assetBaseDir) / L"Machines" / fs::path (m_currentMachineName) / L"PendingPrint";
    }

    // Live channel for the Settings → Display monitor dropdown. The
    // dropdown calls this on every selection so the user sees the
    // color-treatment change as they hover/select; Cancel restores
    // the baseline by calling this again with the entry-state value.
    // Bypasses the IDM command queue so the change is visible on the
    // next CPU frame rather than waiting for queue drain.
    void  SetColorModeLive (int settingsColorModeIndex);

    // Live-set the text color used on the Color monitor (0xAARRGGBB),
    // resolved from a ColorMonitorTextMode + custom color. Like
    // SetColorModeLive, the Settings panel calls this on hover / select so
    // the change shows on the next CPU frame, and on Cancel to restore.
    void  SetColorMonitorTextArgbLive (uint32_t argb);

    // Records the user's per-drive write-protect preference and applies
    // it to the currently mounted image (if any) so the change takes
    // effect immediately. Called on the CPU thread from the
    // IDM_DISK_WRITEPROTECT command handler, which the Settings apply
    // path and the write-protect menu items post. The preference also
    // survives an eject/remount because MountDiskInSlot6 re-applies it.
    void  SetDriveUserWriteProtect (int drive, bool wp);

    // Activates the named theme in ThemeManager (which notifies the
    // chrome cache listener) and persists the choice into GlobalUserPrefs.
    // No-op if the name is empty; falls back to Skeuomorphic if unknown.
    HRESULT ApplyAndPersistTheme  (const std::string & themeName);

    // Activates the named theme LIVE (reskins the chrome via the
    // ThemeManager listener) WITHOUT persisting it to GlobalUserPrefs.
    // Used by the Settings Theme page's "Apply now" affordance so the
    // user can preview a theme on the real chrome; a subsequent Cancel
    // re-activates the baseline theme, and OK persists via
    // ApplyAndPersistTheme. No-op if empty; falls back to Skeuomorphic.
    HRESULT ApplyThemeLive        (const std::string & themeName);

    // Pushes a freshly-activated CassoTheme into the layout-affecting
    // chrome state: drive bar thickness, per-drive compact flag, and
    // (if the bottom inset changed) a window resize that preserves the
    // emulator pixel grid. Called from the ThemeManager listener.
    void    ApplyThemeToChrome    (const CassoTheme & theme);

    // Settings > Theme opt in/out for the CRT monitor. Applies live -- relays
    // out the chrome in place -- and persists to GlobalUserPrefs.
    void    SetCrtMonitorEnabled (bool enabled);

    // Settings > Theme antialiasing, in SAMPLES (1 / 2 / 4). Applies to the
    // next frame and persists to GlobalUserPrefs; ApplySceneAntiAliasing is
    // the startup half, which pushes the stored value without re-saving it.
    void    SetSceneAntiAliasing   (int samples);
    void    ApplySceneAntiAliasing ();

    // The 3D scene renders whenever a skeuo theme is active and the models
    // loaded. The DRIVES are not optional -- they are 3D objects in every
    // skeuo presentation; compact themes keep their flat widgets.
    bool    DeskSceneActive      () const
    {
        return !m_chromeTheme.compactDrives && m_deskSceneReady;
    }

    // ...and the monitor on top of that, which the user CAN turn off: the
    // picture then sits on a flat rect at classic sizes with the 3D drive
    // row still composed in the band below it. Everything keyed off the
    // curved glass -- the glass-fill fullscreen, the inverse-projected
    // pointer mapping, the Ctrl+0 solve -- follows this, not DeskSceneActive.
    bool    CrtMonitorActive     () const
    {
        return DeskSceneActive() && m_globalPrefs.crtMonitor;
    }

    // A left-button orbit that has not yet travelled far enough to BE one.
    // The press arms it over anything the scene shows; only movement past
    // the slop turns it into a rotation, and a release before that lets the
    // click chain run as though nothing had been armed at all.
    static constexpr int  s_kSceneOrbitSlopPx = 4;

    bool  m_sceneOrbitMoved = false;

    // The most drives the desk scene ever composes -- DeskSceneComposition
    // sizes its world matrices to the same number.
    static constexpr int  s_kSceneDriveMax = 2;

    // Each drive's door hit box, posed to the openness that drive is showing.
    // Filled for every slot, degenerate where there is no drive or no door,
    // which the hit tester reads as "no door target here".
    void  BuildDriveDoorBoxes (DeskRegionBox (& out)[s_kSceneDriveMax]) const;

    // Whether a point falls inside the scene's OWN rect -- the band between
    // the chrome bands, which is what the composition is solved into. A
    // press outside it is on the toolbar, the status bar or the menu strip,
    // and a gesture that begins on the scene must not begin there.
    bool    PointInSceneRect     (int x, int y) const
    {
        const RECT &  vp = m_deskScene.Composition().viewportPx;

        return x >= vp.left && x < vp.right && y >= vp.top && y < vp.bottom;
    }

    // Initializes the desk scene renderer against the host device and loads
    // the embedded device models. Failure leaves the scene off (asserting
    // in debug -- a broken embedded asset is a build defect) and the 2D
    // chrome paths carry on.
    HRESULT InitializeDeskScene  ();

    // Loads the monitor + drive pair the active machine wore (//c gets its
    // own platinum set, everything else the beige Monitor II over Disk IIs).
    // Called again on a machine switch.
    HRESULT LoadDeskSceneModelsForMachine ();

    // Resolves a client-px position against the composed scene (glass /
    // drive region / nothing).
    SceneHitResult  DeskSceneHit (int xPx, int yPx) const;

    // Resolves against the fullscreen strip's drives-only composition
    // (glass excluded -- its monitor placement is meaningless).
    SceneHitResult  StripHit     (int xPx, int yPx) const;

    // How many drives the scene composes: the machine's Disk II presence and
    // the //c external-drive connection, the same gates the 2D widgets use.
    int     DeskSceneDriveCount  () const;

    // Zoom by `factor` about a client point, so whatever is under the cursor
    // stays under it. Zooming about the viewport CENTER instead would push
    // the thing being inspected off toward an edge exactly as it got big
    // enough to look at.
    void    ZoomSceneAt          (POINT clientPt, float factor);
    DxuiMessageResult  PanSceneByNotch (float notch, bool horizontal);
    void    OrbitSceneBy         (float yawRad, float pitchRad);
    void    BeginSceneOrbit      (int x, int y);
    void    UpdateSceneOrbit     (int x, int y);
    float   OrbitRadPerPx        () const;

    // Put the framing back to the fitted composition.
    void    ResetSceneView       ();

    // Clamp pan so the scene cannot be dragged entirely off-screen, and drop
    // the pan to zero once zoomed back out -- at 1.0 the composition already
    // fits, so an offset there is only ever a way to lose it.
    void    ClampSceneView       ();

    // Re-solve the composition for the current client size and repaint. The
    // framing feeds the same solve the viewport does, so there is no separate
    // "just the camera" path to keep in step with it.
    void    InvalidateSceneComposition ();

    // How far in and out the framing goes. The far end is where the model
    // stops rewarding a closer look -- past roughly 8x the mesh's own facets
    // are the subject -- and the near end is 1, the fitted composition, since
    // zooming out past a view that already contains everything only shrinks
    // it into the middle of an empty viewport.
    // Below 1 the fitted composition shrinks into the window with margin
    // around it -- the step-back look. Pan slack stays zero down there (see
    // ClampSceneView), so zooming back in cannot strand the scene off-center.
    static constexpr float  s_kSceneZoomMin  = 0.5f;
    static constexpr float  s_kSceneZoomMax  = 8.0f;

    // One wheel notch. Geometric, so the same flick covers the same visual
    // proportion at every zoom -- a fixed additive step feels fast when close
    // in and useless when far out.
    static constexpr float  s_kSceneZoomStep = 1.15f;

    // How far one wheel notch pans, in units of the pan range -- which runs
    // -1..1 across the viewport, the same units the touch pan works in.
    static constexpr float  s_kScenePanStep  = 0.12f;

    // The inspection orbit's feel: a drag across the full viewport sweeps
    // this many radians (per-pixel is DERIVED from the viewport, because the
    // pixel coordinates the handlers see are DPI-scaled -- see
    // OrbitRadPerPx), and a Shift+slide turns this much per wheel notch. The
    // stored pitch is clamped a shade past the layout's own elevation limit
    // -- the layout clamps the TOTAL, seat included, so this one only stops
    // the value winding up unboundedly while pinned.
    static constexpr float  s_kOrbitDragSweepRad = 3.6f;
    static constexpr float  s_kOrbitRadPerNotch  = 0.06f;
    static constexpr float  s_kOrbitPitchLimit   = 1.6f;

    // While the scene owns the drives, the 2D widgets stay hidden (they keep
    // mirroring state for the //c switch strip) and the drag-drop hit rects
    // come from the composition's projected drive bounds.
    void    SyncSceneDriveChrome ();

    // Re-hangs the mounted-image basename strip under each projected drive.
    void    SyncSceneDriveLabels ();


    // Fullscreen presentation (FR-014): every chrome element collapses to
    // nothing -- host caption, menu bar, toolbar, joystick row, drive band,
    // //c switch strip -- so the glass-fill scene owns the whole client.
    void    SetChromeHiddenForFullscreenScene (bool hidden);

    // The pointer-capture banner and the fullscreen top-edge toolbar reveal,
    // both driven from the per-frame UI upkeep.
    void    SyncCaptureBanner    ();
    void    SyncFrameRateReadout ();

    // The scene pose across the middle of the picture, so a screenshot of a
    // render fault carries the angle it was taken from.
    void    SyncSceneViewReadout ();
    void    TickFullscreenToolbar();

    // Builds/refreshes the CASSO_SCENE_DEBUG=2 texel-calibration texture.
    void  EnsureSceneCalibration (const RECT & fittedRect);

    // Position the printer status indicator in the command-bar dead space to
    // the right of the centered drive widgets, or Hide() it when the machine
    // has no printer card. Does not affect drive centering.

    // Open (creating if needed) the printer panel / print preview window, and
    // push it a fresh snapshot of the current strip. `activate` false shows it
    // without stealing focus from the guest (used by the auto-open path).
    void    ShowPrinterPanel (bool activate = true);

    // Owner HWND for printer confirmation / notice message boxes: the preview
    // panel when it is open and visible (so the box centers on the dialog the
    // user is acting in), otherwise the main window.
    HWND    GetPrinterDialogOwner () const;

    // Force-refresh the printer panel from the drain worker (race-free, without
    // stopping it): the panel snapshots and renders only its visible ~1-page
    // viewport span. Non-destructive: the live interpreter keeps running, so
    // refreshing mid-print can never disturb the job's state or the output.
    void    SnapshotStripToPanel ();

    // Per-frame: sample the worker's status signals, recompute the indicator
    // state, and mark a redraw only when it changes (so a static screen still
    // repaints the LED on a transition).
    void    UpdatePrinterStatus ();

    // Delivery outcome -> the printer status LED: failed=true lights the red
    // error state until a success / discard clears it or the guest prints
    // something new. Called from the delivery paths (WindowCommandManager).
    void    NotePrinterDeliveryResult (bool failed)
    {
        m_printerDeliveryError = failed;
        m_printerErrorActivity = m_printerWorker.GetActivityCount ();
    }

    // Per-frame: auto-open the preview when a new print begins (activity resuming
    // after an idle gap) and refresh the strip live as bytes flow, throttled by an
    // interval that grows with strip height so a busy print does not re-render the
    // whole strip every frame (nor O(rows^2) over a long banner).
    void    UpdatePrinterPreview ();

    // Attach the Casso app icon (IDI_CASSO) to a child DxuiWindow so it shows the
    // Casso motif in Alt-Tab / the taskbar. The borderless Dxui panels do not
    // inherit the WNDCLASS icon, and Alt-Tab reads WM_GETICON, so the big+small
    // icons are handed over explicitly (as the main window does).
    void    ApplyAppIconToWindow (HWND target);

    // Keyboard chrome-focus ring (see m_chromeFocusIndex). SetChromeFocusIndex
    // updates the index and refreshes which widget paints its focus visual;
    // HandleChromeFocusKey owns all keydown handling while the ring is active
    // (returns true when the key was consumed); UpdateChromeFocusVisuals
    // pushes the current index into the MainMenu / button / drive widgets.
    void    SetChromeFocusIndex   (int index);
    void    UpdateChromeFocusVisuals ();
    bool    HandleChromeFocusKey  (WPARAM vk);

    // Flushes the in-memory GlobalUserPrefs to UserPrefs.json. Used as
    // the WindowManager save callback so per-monitor window placement
    // edits land on disk immediately after the user moves/resizes the
    // window. Safe to call before m_userConfigStore exists -- the no-op
    // path lets the in-class WindowManager initializer not race the
    // shell's Initialize sequence.
    void    SaveGlobalPrefs      ();

    // Shows the supplied dialog modally as a MessageDialog (a DxuiWindow
    // shown via ShowModalDialog). Returns the resultCode of the chosen button,
    // or -1 on close-gesture.
    int     ShowModalDialog      (const DialogDefinition & def);

    // Whether the Disk menu offers the write-protect toggle for a drive.
    bool    IsWriteProtectToggleOffered (int drive);

    // Whether the Disk menu offers salvage for a drive: only a damaged image
    // with ordinary 16-sector structure can be rebuilt from its sectors.
    bool    IsSalvageOffered (int drive);

    // Shows a dialog whose body is a caller-built panel rather than text runs,
    // for content a string cannot carry (here: an aligned figures table and a
    // warning banner).
    int     ShowSalvageDialog (const DialogDefinition & def,
                               std::unique_ptr<SalvageDialogContent> content);

    // The whole salvage interaction: assess, show the figures, write the copy
    // on confirmation, then offer to insert it.
    void    RunSalvageFlow (int drive);

    // What one bay's external change wants said, carried from the thread that
    // owns disk writes to the one that owns the screen.
    struct ChangeNotice
    {
        int           slot  = 0;
        int           drive = 0;
        ChangePrompt  prompt;
    };

    // Installs the two sinks the image store reports through: the non-blocking
    // banner, and the question. Both bounce to the UI thread.
    void    InstallChangeReporting ();

    // Raises the non-modal banner over the running machine for a bay.
    void    ShowChangeBanner  (const ChangeNotice & notice);

    // Puts the store's question to the user and routes the answer back to the
    // thread that owns disk writes.
    void    AskAboutChange    (const ChangeNotice & notice);

    // Lays the notice into the band the dock gave it.
    void    LayoutChangeBanner ();

    // Offers a mouse event to the message bar, if one is up.
    //
    // THE SHELL HIT-TESTS ITS CHROME BY NAME rather than walking the panel
    // tree -- the toolbar, the joystick selector and the //c switch strip are
    // each asked in turn -- so a control that is not on that list is painted
    // and never clicked. Measured: the bar drew correctly and its button could
    // not be pressed.
    bool    OfferMouseToChangeBanner (DxuiMouseEventKind kind, int x, int y);

    //  Closes the change band and gives its height back to the picture.
    void    HideChangeBanner ();

    //  Closes it once its time is up, unless the pointer is resting on it.
    void    ExpireChangeBannerIfDue ();

    // How tall the notice's band is right now: zero when nothing is being
    // reported, and the height its wrapped text needs when something is.
    int     GetChangeBandThicknessPx (int clientWidthPx) const;

    // Re-docks the chrome after the notice's band appears or goes.
    //
    // IT DOES NOT RESIZE THE WINDOW, unlike the machine-change reflow beside
    // it. A notice is transient and the user did not ask for a bigger window
    // to hold it: the picture gives up the height and takes it back.
    void    ReflowChromeForChangeBand ();

    // Opens the integrity-level hole a stated intent arrives through.
    //
    // SEPARATE FROM InstallDragDropTarget, WHICH ALSO INSTALLS IT. That one is
    // called only where OLE initialization succeeded, so on a machine where it
    // did not, every intent from a normal-integrity CassoCli to an elevated
    // Casso would be dropped by the system without a word. Installing it here
    // as well costs a call and removes the dependency.
    void    InstallIntentMessageFilter ();

    // Asks where to save the contents of a disk whose file has gone.
    //
    // THE SAVE DIALOG, NOT THE DISK PICKER. The user is saving a disk here,
    // not choosing one to mount, and the two look similar enough that reaching
    // for the wrong one would be easy and baffling.
    //
    // Returns false where the user cancelled, which is the same outcome as
    // declining: the drive is emptied either way and nothing is written.
    bool    AskWhereToSaveLostDisk (const std::string & imagePath, std::wstring & outPath);

    // Reports a freshly mounted image that failed its stored checksum, with
    // salvage offered inline. Raised here rather than by the loader because a
    // dialog with an action on it is the shell's business, and EhmNotifyUser
    // carries a string and nothing else.
    void    ReportDamagedMount (int drive);

    // One attempted mount's outcome, carried from the thread that ran the
    // mount to the UI thread that reacts to it. Plain data, and used only as
    // a parameter, so it rides along in this header.
    //
    // The path stays in the narrow form the store and the DiskManager use.
    // Widening it here and narrowing it again for the message would be a
    // round-trip through the platform encoding for no gain, and that is the
    // trip that mangles a non-ASCII filename.
    struct MountCompletion
    {
        std::string     path;
        MountDiagnosis  diagnosis;
        HRESULT         result = S_OK;
        int             drive  = 0;
    };

    // The DiskManager mount-completion hook. Runs on whichever thread ran the
    // mount -- the CPU thread for anything the user started, the UI thread for
    // the command-line disks -- and does nothing but get the outcome onto the
    // UI thread, where the MRU and the dialogs live.
    void    OnMountCompleted (int drive, const std::string & path, HRESULT mountResult,
                              const MountDiagnosis & diagnosis);

    // The UI-thread half: a successful mount enters the recent-disks list and
    // is checked for damage, a failed one is reported to the user. Posting to
    // get here is also what keeps a failed --disk1 from raising a modal inside
    // Initialize, before the message loop that would service it is running.
    void    HandleMountCompletion (const MountCompletion & completion);

    // The EHM user-notification sink, installed with SetNotifyFunction so
    // every CHRN / CBRN in the tree reports through Casso's own themed
    // dialog. Nothing had ever installed one, so they all fell through to
    // EhmNotifyUser's built-in path and became raw Win32 message boxes.
    //
    // Static because SetNotifyFunction takes a plain function pointer; it
    // forwards to the one live shell.
    static void  NotifyUser (const wchar_t * message);

    // Holds a report raised before there is a window to show it in. Public
    // and static because wWinMain installs the sink before the shell exists.
    static void  QueueNotification (const std::wstring & message);

    // Shows one notification, marshaling as needed. Callable from any
    // thread: a flush that fails on the CPU thread reports through here.
    void         ShowNotification (const std::wstring & message);

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

    // Push a freshly mounted disk image onto the recent-disks MRU
    // and persist user prefs. Best-effort; never propagates failures
    // back into the mount path. Takes the mount's own HRESULT and hands
    // it to DiskMru, which drops anything that did not actually mount.
    void    RecordRecentDisk     (const std::wstring & path, HRESULT mountResult);

    // MachineManager and WindowCommandManager touch enough shell
    // state during construction and command dispatch that friend
    // declarations are the pragmatic seam; no new global state is
    // introduced.
    friend class MachineManager;
    friend class WindowCommandManager;
    friend class SettingsSheet;
    friend class SettingsApplyController;
    friend class SettingsDisplayCrtBridge;
    friend class SettingsMachineCatalog;

    HACCEL     m_accelTable            = nullptr;
    HINSTANCE  m_hInstance             = nullptr;
    HWND       m_hwnd                  = nullptr;
    bool       m_initialSizeReconciled = false;

    // Dragging a bezel tilt mark. The gesture is the mark's, but the motion
    // is the pointer's: how far the mouse has travelled vertically since the
    // press is the whole input, so which mark started it only decides that a
    // drag started at all.
    bool       m_bezelTilting          = false;
    POINT      m_bezelTiltStartPx      = {};
    float      m_bezelTiltStartRad     = 0.0f;

    // How much tilt a pixel of drag is worth. The assembly's whole travel is
    // about eleven degrees each way, so this spends it over a couple of
    // hundred pixels -- far enough that the limit is reached deliberately
    // rather than by flinching.
    static constexpr float  kBezelTiltRadPerPx = 0.0022f;

    // A compass arrow click's fixed turn. Yaw takes more than pitch for the
    // same reason the free orbit allows more of it: the interesting sides
    // of the machines are around them, not above.
    static constexpr float  kCompassStepYawRad   = 0.2618f;   // 15 degrees
    static constexpr float  kCompassStepPitchRad = 0.1745f;   // 10 degrees

    void  ApplySavedBezelTilt ();
    void  PersistBezelTilt    ();
    bool       m_startMaximized        = false;

    // Authoritative per-window DPI scaler. Mirrors the one inside
    // DxuiHwndSource; updated from OnDpiChanged and seeded after
    // m_host->Create() returns. The chrome-band dock scales its band
    // thicknesses through this member.
    DxuiDpiScaler       m_scaler;

    MemoryBus               m_memoryBus;
    ComponentRegistry       m_registry;
    InterruptController     m_interruptController;
    unique_ptr<EmuCpu>      m_cpu;
    unique_ptr<class Prng>  m_prng;
    size_t                 m_traceCapacity = 0;       // --trace ring size (entries); 0 = off
    bool                   m_imageWatchDisabled = false;  // --no-image-watch (undocumented)
    std::atomic<bool>      m_traceDumped { false };   // one-shot guard for DumpTrace
   
    D3DRenderer            m_d3dRenderer;
    WasapiAudio            m_wasapiAudio;

    // UI-thread filesystem and chrome ownership. The painter pass
    // and shell composition is reintroduced in a later phase; for now
    // only the per-window filesystem stays here so the settings panel
    // and config store can resolve paths on the UI thread.
    Win32FileSystem        m_uiFs;

    // Chrome surfaces. MainMenu owns the parity table for legacy IDM_*
    // commands and runs alongside the existing Win32 menu bar until the
    // painter retires the latter. The caption (title + icon + min/max/
    // close) is owned and rendered by the DxuiHwndSource, not here.
    MainMenu                    m_mainMenu;
    CassoTheme                  m_chromeTheme = CassoTheme::MakeSkeuomorphic();
    std::array<DriveWidget, 2>  m_driveChrome;

    // The command toolbar (spec 015 DCR-2): the strip below the menu bar with
    // Settings / Printer (+status LED) / master Volume + Mute / Screenshot /
    // Reset / Power. Its printer button carries the status light (the old
    // standalone PrinterIndicator is deleted).
    CommandToolbar      m_toolbar;

    // The pure model deriving the printer LED state from the worker's live
    // signals, plus the last state pushed to the toolbar so a transition
    // repaints exactly once.
    PrinterStatusModel  m_printerStatus;
    PrinterStatus       m_printerStatusShown = PrinterStatus::Idle;

    // Delivery-failure latch feeding the status model's error input (the
    // toolbar LED's red). Set by the delivery paths in WindowCommandManager;
    // cleared by a successful delivery, a discard, or fresh guest print
    // activity (the user has moved on -- red must not mask the new print).
    bool                m_printerDeliveryError = false;
    uint64_t            m_printerErrorActivity = 0;

    // The 3D desk scene (spec 018): Monitor //c + drives rendered from the
    // before-present hook on the host device, with the CRT chain's offscreen
    // output on the curved glass. Gated by the deskScene opt-out pref.
    DeskScene                  m_deskScene;
    bool                       m_deskSceneReady = false;

    // Which machine family the loaded models belong to, so a switch that
    // does not cross the //c boundary skips the reload.
    bool  m_deskSceneMachineIsC = false;
    int   m_deskSceneDebug      = 0;   // CASSO_SCENE_DEBUG: 1=layout rects, 2=+calibration texture

    // Fullscreen drive overlay strip (FR-015): the pure FSM plus this
    // frame's composed band. The hotkey edge arrives via the accelerator;
    // m_stripBrowseOpen pins the strip while a browse it opened is up, and
    // m_stripSuppressGuestMouse is the "released capture" of a hotkey summon
    // in mouse mode (paddle mode releases its real capture instead).
    FullscreenStripState       m_stripState;
    DeskSceneComposition       m_stripComp;
    RECT                       m_stripRectPx             = {};
    bool                       m_stripHotkeyPending      = false;
    bool                       m_stripBrowseOpen         = false;
    bool                       m_stripSuppressGuestMouse = false;

    // CASSO_SCENE_DEBUG=2: a synthetic stripe pattern standing in for the
    // CRT output, to verify the glass texel mapping end to end.
    ComPtr<ID3D11Texture2D>           m_sceneCalibTex;
    ComPtr<ID3D11ShaderResourceView>  m_sceneCalibSrv;
    RECT                              m_sceneCalibRect = {};

    // Set when a Ctrl+letter host-meta shortcut claims a keydown whose
    // synthesized WM_CHAR must not reach the guest keyboard latch (the ^V
    // of a paste would land in the input line ahead of the pasted text).
    bool                       m_swallowMetaChar = false;

    // Desk-scene zoom: the monitor's SceneScale from the last layout. The
    // drive widgets and the (scaled part of the) drive band follow it so the
    // whole scene zooms together when the window resizes. 1.0 for compact
    // themes and at the 100%-zoom default window size.
    float                      m_chromeSceneScale = 1.0f;

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

    // Apple ][ framebuffer viewport inside the host's root panel.
    // Sized by EmulatorShell whenever chrome layout changes; the
    // bounds-changed callback forwards the new rectangle to
    // m_d3dRenderer.SetTargetBounds so the renderer knows where to
    // composite the framebuffer once the swap-chain restructure
    // completes later in Phase 11d. Non-owning pointer; the panel
    // tree owns the DxuiViewport instance.
    DxuiViewport *                   m_viewport          = nullptr;
    RECT                             m_viewportBoundsPx  = {};

    // Per-frame framebuffer pointer staged by RunMessageLoop and read
    // by the host's before-present hook (DxuiHwndSource::PaintPump ->
    // D3DRenderer::UploadAndComposite). Points into m_uiFramebuffer
    // when the emulator produced a new frame this iteration, or nullptr
    // to re-composite the last upload (chrome-only repaints). Touched
    // only on the UI thread.
    const uint32_t *                 m_pendingFramebuffer = nullptr;

    // Joystick-mode toggle button (mirrors IDM_MACHINE_ARROWS_JOYSTICK),
    // centered in the drive bar above the drive widgets, with its own
    // hover tooltip.
    // Non-modal notice over the running machine: a disk changed outside Casso.
    //
    // IT DOES NOT CLEAR ITSELF, and that is the design rather than an
    // oversight: the action it carries is the restart, which is what the user
    // reaches for once the program starts misbehaving, and a notice that faded
    // would take that action with it.
    DxuiActionBanner            m_changeBanner;
    int                         m_changeBannerDrive = -1;

    //  When the change band closes itself, and the frame that last looked.
    //  Zero means it stands until dismissed. Hovering does not extend the
    //  wait, it suspends it: the deadline moves with the clock while the
    //  pointer is over the band, so what is left when the pointer leaves is
    //  what was left when it arrived.
    int64_t                     m_changeBannerHideAtMs = 0;
    int64_t                     m_changeBannerTickMs   = 0;

    // What each of the banner's buttons means, in the order they were drawn.
    // The labels and the meanings are both core's; keeping the meanings beside
    // the buttons is what stops the shell from inventing one.
    std::vector<ChangeAction>   m_changeBannerActions;

    DxuiTooltip          m_toolbarTooltip;   // labels for the toolbar's icon-only mode

    // Apple //c case-switch strip (reset button + 80/40 and keyboard latching
    // switches + disk-use / power LEDs), painted in its own chrome band between
    // the emulator viewport and the drive bar. Present only on the //c; its
    // band collapses to zero height on every other machine. Manually
    // hit-tested / actioned by the mouse handlers, like the other chrome.
    Apple2cSwitchBar  m_switchBar;
    DxuiTooltip       m_switchBarTooltip;

    // Hover tooltip for the drive widgets, surfaced when the pointer
    // rests over a write-protected drive. Explains that the disk is
    // write-protected and names the source(s) -- image flag, user
    // setting, or an unwritable backing file. Shares the host popup pool
    // with the other chrome tooltips (the hover regions are mutually exclusive).
    DxuiTooltip               m_driveTooltip;

    // Live per-drive user write-protect preference (Settings > Disk
    // checkbox / write-protect menu). Seeded from $cassoUiPrefs at
    // startup and re-applied to each freshly mounted image so the guest
    // sees the disk as protected and dirty writes never flush. Distinct
    // from the image's own embedded flag and from the backing file's
    // read-only state; all three are surfaced independently in the UI.
    std::array<bool, 2>   m_userWriteProtect { { false, false } };

    // Solid background for the bottom drive-bar band. The CRT composite
    // writes the whole back buffer (emulator frame + black), so the chrome
    // bands need an opaque surface painted on top; the title and menu bars
    // cover their own bands, this covers the drive bar.
    DxuiSurface           m_driveBandSurface;

    // The mounted image's basename under each 3D drive -- the label strip the
    // 2D widget carried below its body, kept on screen rather than demoted to
    // a hover tooltip. Positioned from the composition's projected drive
    // bounds; empty (and invisible) when that drive holds no disk.
    // The mounted image's name under each drive. CHROME, not scene geometry:
    // it is read at a fixed size wherever the desk is posed.
    std::array<DxuiShadowedText, 2>  m_sceneDriveLabel;

    // Where each of those strips landed, empty when a drive shows no name.
    // The write-protect tooltip belongs to the strip now that the padlock
    // does -- see SyncSceneDriveLabels.
    std::array<RECT, 2>       m_sceneDriveLabelRect = {};

    // The source path each label was last built from, so mounts and ejects
    // re-hang it without a layout pass and an unchanged frame does no
    // filesystem parsing or text measurement.
    std::array<std::string, 2>  m_sceneLabelPath;

    // "Paddle Mode -- press Esc to release the mouse", on screen for as long
    // as the capture holds. The joystick button carries the same words, but
    // it is chrome: fullscreen hides it, and a captured pointer with the
    // cursor gone and no way out shown is how a user ends up killing the
    // process. This rides above the picture in both presentations.
    DxuiHudNotice              m_captureBanner;

    // The frames-per-second readout. Shadowed rather than a notice: it
    // wants a corner, not the centered band a notification takes.
    DxuiShadowedText           m_fpsReadout;
    DxuiShadowedText           m_sceneViewReadout;

    // The scene compass: the visible way to turn the scene, for everyone
    // who will never guess that dragging does it. Laid out into the scene
    // viewport's corner by SyncSceneDriveChrome, which already runs at
    // every moment the viewport moves.
    DxuiOrbitControl           m_sceneCompass;

    void  LayoutSceneCompass ();

    // The fullscreen toolbar reveal, the drive strip's bargain mirrored
    // along the top edge: shown while the pointer is up there, hidden once
    // it leaves and the grace expires.
    bool                       m_fsToolbarShown    = false;
    int64_t                    m_fsToolbarLeftMs   = 0;
    int64_t                    m_fsToolbarAnimMs   = 0;   // slide start

    // Chrome layout via DxuiDockLayout. The three bands carry the title
    // bar, nav strip, and drive bar pixel thicknesses in their GetBounds();
    // m_centerBand (Fill) captures the emulator viewport rect the dock
    // leaves in the middle. m_driveBarThicknessDp is the live drive-bar
    // thickness the theme mutates (compact vs full).
    static constexpr int  s_kTitleBarBandDp     = 32;
    static constexpr int  s_kNavStripBandDp     = 32;
    // (The command toolbar band's thickness comes from m_toolbar.GetBandDp() --
    // it varies with the responsive mode planned for the window width.)
    static constexpr int  s_kInitialDriveBandDp = 256;

    // //c switch strip band thickness (dp). Zero-height on non-//c machines
    // (SyncChromeBands gates it on IsApple2c()); it docks below the drive band
    // so it lands between the viewport and the joystick/paddle/mouse bar.
    static constexpr int  s_kSwitchBandDp       = 40;

    DxuiDockLayout           m_chromeDock;
    ChromeBand               m_titleBand;
    ChromeBand               m_navBand;
    ChromeBand               m_toolbarBand;

    // The client width the bands were last laid out for, so the notice's
    // height can be measured against the width it is about to be given.
    int                      m_lastClientWidthPx = 0;

    // The external-change notice's own band, docked under the toolbar.
    //
    // A BAND RATHER THAN AN OVERLAY, and the difference is not cosmetic. Drawn
    // over the viewport it covered the top of the picture, took its width from
    // a rect that follows the emulator's aspect rather than the window, and ran
    // its text and its action off the client edge. As a band the dock gives it
    // the client width, the Fill center shrinks by exactly its height, and the
    // scene rescales into what is left -- the same way the //c switch strip and
    // the drive bar already work.
    //
    // Zero height when nothing is being reported, so every other machine and
    // every quiet session is laid out exactly as before.
    ChromeBand               m_changeBand;
    ChromeBand               m_driveBand;
    ChromeBand               m_switchBand;
    ChromeBand               m_centerBand;
    int                      m_driveBarThicknessDp = s_kInitialDriveBandDp;

    // Whether the current WINDOW height was sized for a Disk ][ controller
    // being present. Written by OnSize (the authoritative layout, WM_SIZE-only)
    // to the disk-presence it just laid out; ReflowChromeForMachineChange reads
    // this pre-switch value to grow/shrink the window by the drive-band delta
    // (so the viewport keeps its size + the top-left stays put) rather than
    // re-centering inside a fixed window.
    bool                     m_chromeSizedForHasDisk = true;

    // Companion to m_chromeSizedForHasDisk for the //c switch band: whether the
    // current WINDOW height was sized with the switch strip present. Recorded by
    // OnSize; ReflowChromeForMachineChange folds the switch-band delta into the
    // window resize so switching to / from the //c keeps the viewport its size.
    bool                     m_chromeSizedForApple2c = false;

    // The user's own framing of the desk scene: how far they have zoomed in
    // and where they have dragged it to. Deliberately NOT persisted -- it is
    // a way of looking at the scene for a moment, like leaning toward a
    // screen, and a saved zoom would have people opening Casso to a view they
    // set once and forgot. Ctrl+0 puts it back.
    DeskSceneView            m_sceneView;

    // Set while a pan drag is in flight, with the anchor the drag started
    // from. The anchor is the SCENE's pan at mouse-down plus the cursor
    // position, so the scene tracks the cursor exactly however far it moves
    // and a slow drag cannot accumulate rounding drift.
    bool                     m_scenePanning     = false;
    POINT                    m_scenePanStartPx  = {};
    float                    m_scenePanStartX   = 0.0f;
    float                    m_scenePanStartY   = 0.0f;

    // The orbit drag mirrors the pan drag: anchored at the press, absolute
    // from there, one flag per button so a left-orbit (Shift+drag) and the
    // right-drag never fight over state.
    bool                     m_sceneOrbiting      = false;
    bool                     m_sceneOrbitLeftBtn  = false;
    POINT                    m_sceneOrbitStartPx  = {};
    float                    m_sceneOrbitStartYaw = 0.0f;
    float                    m_sceneOrbitStartPit = 0.0f;
    int64_t                  m_sceneOrbitTapMs    = 0;

    // Touch gesture tracking. Windows reports a pinch as an ABSOLUTE
    // separation between the two fingers and a pan as an ABSOLUTE point, so
    // both need their previous value kept to turn into a step -- there is no
    // delta in the message. Reset on GF_BEGIN, or the first step of a new
    // gesture would be measured against wherever the last one ended.
    ULONGLONG                m_gestureZoomLast  = 0;
    POINT                    m_gesturePanLastPx = {};

    // //c only: whether the optional external drive is "connected". Mirrors
    // the per-machine $cassoUiPrefs.externalDriveConnected pref; seeded at
    // machine build and flipped live by IDM_DRIVE_EXTERNAL_CONNECT/DISCONNECT.
    // Gates the second drive-mount widget (m_driveChrome[1]) via
    // ShouldShowExternalDrive(). No effect on machines whose second drive is
    // fixed hardware (they have no banked ROM, so the gate is always open).
    bool                     m_externalDriveConnected = false;

    // //c only: whether the mouse peripheral is plugged into the DB-9 port
    // Mirrors $cassoUiPrefs.mouseConnected (default CONNECTED);
    // flipped live by IDM_MOUSE_CONNECT/DISCONNECT. Disconnected = the IOU
    // silicon stays but IsGuestMouseActive() is false (no host input feeds
    // the device) and the input-mode cycle hides Mouse -- indistinguishable
    // from an unplugged DB-9 on real hardware.
    bool                     m_mouseConnected = true;

    // Drive widget state pump. The controller channel publishes
    // per-drive door/spin sync events the chrome painter will consume
    // once reintroduced. The drag-drop target registers a single
    // IDropTarget on the main HWND. Per-drive UI/CPU bridge state
    // lives in m_driveWidgetState; the CPU thread's motor + nibble
    // counters are sampled once per UI frame and pushed through the
    // controller.
    DriveWidgetController  m_driveWidgets;
    DxuiDragDropTarget     m_dragDropTarget;

    // Native UI shell. Owns the painter, text renderer, hit-tester,
    // focus manager, animation broker, and input translator. Wired
    // onto D3DRenderer's after-blit hook so chrome composites every
    // frame between the emulator blit and Present.
    UiShell                    m_uiShell;

    // Settings-dialog dependencies. ThemeManager + UserConfigStore +
    // GlobalUserPrefs are owned here and handed to the SettingsSheet each
    // time it opens (OpenSettings).
    std::unique_ptr<ThemeManager>        m_themeManager;
    std::unique_ptr<UserConfigStore>     m_userConfigStore;
    GlobalUserPrefs                      m_globalPrefs;

    // The Settings dialog, shown modeless so the emulator keeps running behind
    // it (FR-041). Heap-owned + null when closed; OpenSettings creates it and
    // the close callback flags m_settingsSheetClosePending so RunMessageLoop
    // destroys it at a safe point (not from inside its own EndDialog handler).
    std::unique_ptr<SettingsSheet>       m_settingsSheet;
    bool                                 m_settingsSheetClosePending = false;

    std::array<DriveWidgetState, 2>      m_driveWidgetState;

    // Set true once OleInitialize has succeeded on the UI thread so
    // shutdown can pair the call with OleUninitialize. RegisterDragDrop
    // requires OLE (STA) on the registering thread.
    bool                                 m_fOleInitialized = false;

    // SW_SHOWDEFAULT means "the launcher expressed no preference", which is
    // what a normal double-click amounts to.
    int                                  m_startShowCmd    = SW_SHOWDEFAULT;

    // Whether the window is maximized as far as the last SAVED placement
    // is concerned. A change here is a user maximizing or restoring, which
    // is worth persisting even though it never enters the OS drag loop.
    // Set when the user issues a maximize / restore and cleared by the
    // OnSize that carries it out, which is where the new placement is
    // actually readable.
    bool                                 m_userStateChange = false;

    // Drive audio. Mixer is always allocated; per-drive sources are
    // populated only when the active machine config carries a
    // Disk II controller (FR-015).
    DriveAudioMixer                      m_driveAudioMixer;
    vector<unique_ptr<Disk2AudioSource>> m_diskAudioSources;

    // Emulated ImageWriter II mechanical audio (Option A: driven by the paced
    // on-screen carriage, not the raw guest stream). A single persistent source
    // on the shared drive-audio bus (FR-016), re-registered by MachineManager on
    // every build. Its grains load once in OnCpuThreadStart.
    PrinterAudioSource                   m_printerAudio;

    // Mockingboard audio. Its own mixer so the "Mockingboard" Options
    // toggle is independent of the Drive Audio toggle. The PSG audio
    // sources are owned by the MockingboardCard device; the mixer holds
    // borrowed pointers, re-registered by MachineManager on every build.
    DriveAudioMixer                      m_mockingboardAudioMixer;

    // Live per-sound drive-audio gains (0..1), seeded from $cassoUiPrefs
    // at startup and updated via SetDriveAudioVolumes. Stored on the shell
    // so they survive machine resets (MachineManager re-seeds fresh
    // sources from these).
    float                                m_driveMotorVolume = Disk2AudioSource::kMotorVolume;
    float                                m_driveHeadVolume  = Disk2AudioSource::kHeadVolume;
    float                                m_driveDoorVolume  = Disk2AudioSource::kDoorVolume;

    // Live per-drive stereo pan in [-1, +1] (-1 = hard left, +1 = hard
    // right), index 0 = Drive 1, 1 = Drive 2. Seeded from $cassoUiPrefs at
    // startup and updated via SetDriveAudioPan; survives machine resets
    // (MachineManager re-seeds fresh sources from these).
    float                                m_drivePan[2] = { DriveAudioMixer::kDefaultDriveOnePan,
                                                           DriveAudioMixer::kDefaultDriveTwoPan };

    // Owned devices
    vector<unique_ptr<MemoryDevice>>     m_ownedDevices;

    // Serial-port endpoints (//c 6551 ACIAs). Owned separately from
    // m_ownedDevices because an IAciaEndpoint is not a MemoryDevice; each is
    // bound to its ACIA via SetEndpoint. The loopback endpoints hold a raw
    // Acia6551* but are never called during teardown, so destruction order
    // relative to m_ownedDevices is immaterial.
    vector<unique_ptr<IAciaEndpoint>>    m_ownedAciaEndpoints;

    // Video
    vector<unique_ptr<VideoOutput>>      m_videoModes;
    CharacterRomData                     m_charRom;

    // Soft switch state (read by video mode selection)
    bool    m_graphicsMode = false;
    bool    m_mixedMode    = false;
    bool    m_page2        = false;
    bool    m_hiresMode    = false;
    bool    m_col80Mode    = false;
    bool    m_doubleHiRes  = false;

    // Per-machine observer pointers. Every entry is a raw pointer
    // into one of the unique_ptr-owning collections above
    // (m_ownedDevices, m_videoModes). They are caches for "quick
    // access" only — never own anything — so they MUST be reset
    // every time the owning collection is rebuilt, or they'll
    // dangle into freed memory. Bundling them in one struct makes
    // that a single assignment (`m_refs = {};`) in SwitchMachine's
    // teardown block, instead of a checklist of individual
    // nullptr assignments that the next field to be added will
    // inevitably miss.
    struct MachineRefs
    {
        class AppleKeyboard *         keyboard         = nullptr;
        class AppleSoftSwitchBank *   softSwitches     = nullptr;
        class AppleGamePort *         gamePort         = nullptr;
        class AppleSpeaker *          speaker          = nullptr;
        class RamDevice *             mainRamDev       = nullptr;
        class Disk2Controller *       diskController   = nullptr;
        class MockingboardCard *      mockingboard     = nullptr;
        class VideoOutput *           activeVideoMode  = nullptr;
        class PrinterCard *           printerCard      = nullptr;

        // The //e-and-later halves of the two devices above: the same
        // objects as `softSwitches` / `keyboard` when the machine has the
        // //e variants, null on a ][ or ][+. Both are resolved once at build
        // time from the configured device type, in the same statement that
        // sets the base pointer -- so each is non-null in exactly the cases
        // where downcasting the base would have succeeded, and callers that
        // need the derived surface (80COL, 80STORE, DHIRES, ALTCHARSET) read
        // a pointer instead of re-deriving it at every use. Null is the
        // ][ / ][+ answer, which every caller already had to handle.
        class Apple2eSoftSwitchBank * iieSoftSwitches  = nullptr;
        class Apple2eKeyboard *       iieKeyboard      = nullptr;

        // Video modes, addressed by name. All five exist on every machine --
        // SelectVideoMode switches between them per frame from soft-switch
        // state and cannot afford to construct one mid-render -- so these are
        // non-null together, from CreateVideoModes until teardown.
        //
        // These replaced positional lookups into m_videoModes. That vector
        // still OWNS the modes, but nothing outside CreateVideoModes indexes
        // it: an index carries no type, so every use site had to restate
        // which slot meant which mode and downcast to match, and a mode
        // inserted anywhere but the end would have silently re-pointed all
        // of them at the wrong renderer.
        class AppleTextMode *         text40           = nullptr;
        class AppleLoResMode *        loRes            = nullptr;
        class AppleHiResMode *        hiRes            = nullptr;
        class AppleDoubleHiResMode *  doubleHiRes      = nullptr;
        class Apple80ColTextMode *    text80           = nullptr;
    };

    MachineRefs                   m_refs;

    // Background printer drain (ring -> interpreter -> raster). Declared after
    // m_ownedDevices so it is torn down (thread joined) before the card it
    // drains.
    PrinterWorker                 m_printerWorker;

    unique_ptr<class Apple2eMmu>  m_mmu;
    // Apple //c firmware-bank coordinator ($C028). Null on every other
    // machine. Owned here (not in m_ownedDevices) because it is not a bus
    // device; reset during machine teardown before the LC/MMU it references.
    unique_ptr<class Apple2cRomBank>  m_apple2cRomBank;
    // Apple //c IOU mouse. Null on every other machine. Owned here
    // (not in m_ownedDevices) because it is not a bus device: the keyboard
    // and soft-switch bank forward its register surface, and the EmuCpu
    // cycle fan-out ticks it (VBL-edge latch + paced movement interrupts).
    unique_ptr<class AppleMouse>  m_mouse;
    unique_ptr<VideoTiming>       m_videoTiming;

    // / T097 / FR-025. The store coordinates auto-flush of dirty
    // disk images on Eject / SwitchMachine / Shutdown / PowerCycle. Each
    // mounted disk's DiskImage is owned by the store; the slot 6 disk
    // controller sees it via Disk2Controller::SetExternalDisk.
    DiskImageStore                m_diskStore;

    // Emulation state
    MachineConfig                 m_config;

    // Machine config file name (without ".json" extension, e.g.,
    // "apple2e", "apple2plus", "apple2"). Used as a registry-key
    // suffix so per-machine UI state (e.g., last-mounted disks) can
    // round-trip between sessions without one machine's setting
    // clobbering another's.
    wstring                       m_currentMachineName;
    wstring                       m_assetBaseDir;

    // CPU-thread lifecycle, run/pause/step transitions, the UI -> CPU
    // command queue, and the paste buffer all live on CpuManager. The
    // shell wires its per-frame and per-command callbacks at startup
    // and otherwise reads the manager's transition state through the
    // IsRunning() / IsPaused() / GetSpeedMode() accessors.
    CpuManager                    m_cpuManager;

    // Atomic flags (UI writes, CPU reads)
    atomic<ColorMode>             m_colorMode{ColorMode::Color};

    // Resolved text color (0xAARRGGBB) for the Color monitor. UI writes via
    // SetColorMonitorTextArgbLive; RenderFramebuffer reads it when the Color
    // monitor is active. Defaults to white.
    atomic<uint32_t>              m_colorMonitorTextArgb{ColorUtil::kWhiteArgb};

    // Double framebuffer (CPU renders, UI presents, protected by m_framebufferMutex)
    mutex                         m_framebufferMutex;
    vector<uint32_t>              m_cpuFramebuffer;
    vector<uint32_t>              m_textOverlay;
    vector<uint32_t>              m_uiFramebuffer;
    bool                          m_framebufferReady = false;

    // Auto-reset event the CPU thread signals after publishing a new frame so
    // the idle UI loop blocks on MsgWaitForMultipleObjects instead of spin-
    // polling with Sleep(1). Created/destroyed by RunMessageLoop.
    HANDLE                        m_frameReadyEvent = nullptr;

    // Render-skip gate: the signatures of the last rendered frame's inputs
    // (video mode/soft-switches, flash phase, color mode + text color). Each
    // CPU-thread frame compares the live inputs plus the bus video-dirty flag
    // against these and skips the whole rasterize + publish when nothing that
    // affects the picture has changed. CPU-thread-only (paused during a step).
    uint32_t                      m_lastRenderModeSig  = 0;
    bool                          m_lastRenderFlashOn  = false;
    uint64_t                      m_lastRenderColorSig = 0;

    // Which video mode composed the previous frame. AppleTextMode's dirty-row
    // cache may only reuse a row when the framebuffer still holds that row's
    // text -- so a change of active mode (the buffer last held graphics or
    // another mode) forces a full text re-raster on the next frame.
    class VideoOutput *           m_prevActiveVideoMode = nullptr;

    // Wall-clock pacing for the presentation side at Maximum speed: the CPU
    // runs flat-out, but frames are rasterized + published only ~60x a second
    // so we don't burn cores rendering frames no one will ever see.
    chrono::steady_clock::time_point  m_lastPublishSteady = {};

    // Previous UI frame's "any drive live" state, so the loop can force one
    // final present on the live->idle edge and clear the activity LED.
    // The drives' visible state as of the last UI frame, and whether it moved
    // between the two before that -- the present vote asks whether the lamps
    // and doors CHANGED, not whether a motor happens to be energized. See
    // TryPresentUiFrame.
    uint32_t                      m_lastDriveSig     = 0;
    bool                          m_driveSigSettling = false;

    uint32_t                      m_cyclesPerFrame  = 17050;
    double                        m_sampleRemainder = 0.0;

    // Host sample rate the loaded sounds were decoded at, 0 before the first
    // load. Compared against the live device rate each frame so a reopen onto
    // a device with a different mix format re-decodes them.
    uint32_t                      m_audioAssetSampleRate = 0;

    // Last arrow key pressed for each emulated joystick axis pair (0 if
    // none). Lets opposing directions resolve last-pressed-wins so a
    // rolling reversal flips the axis instead of canceling to center.
    WPARAM          m_lastHorizontalArrowVk = 0;
    WPARAM          m_lastVerticalArrowVk   = 0;

    // How host arrow / pointer input is mapped onto the emulated game
    // port (Off / Joystick / Paddle). Mirrors
    // GlobalUserPrefs (split model) and is cycled via the Machine
    // menu's "Cycle Input Mode" item, Ctrl+Shift+J, and the drive-bar widget.
    InputMappingMode  m_pointerMode    = InputMappingMode::Off;   // Off/Paddle/Mouse
    bool              m_arrowsJoystick = false;                    // Keys axis

    // Paddle-mode mouse capture. While captured, the cursor is hidden and
    // confined, relative motion drives the paddle axes (held, no recenter),
    // and the mouse buttons drive the fire buttons. m_paddleAxis* are float
    // accumulators (0..255) so sub-unit motion isn't lost between events.
    bool              m_paddleCaptured = false;
    float             m_paddleAxisX    = 127.0f;
    float             m_paddleAxisY    = 127.0f;

    // Keyboard focus ring across the painted chrome ("Z" Tab order, left
    // to right, top to bottom): -1 = guest (//e has focus), 0..6 = the
    // seven menu titles File..Help, 7 = joystick-mode button, 8/9 = drive
    // widgets 1/2. Entered via F10 or a mouse click on a chrome element;
    // exited via Esc/F10 or a click in the emulator viewport. While active
    // (>= 0) every keydown is consumed so letters never leak to the //e.
    int             m_chromeFocusIndex      = -1;

    // Spec-011 / US7. DX-themed panel for the Disk II debug window.
    // Lazy-created on first Ctrl+Shift+D and reused across opens.
    // The uptime anchor lives on the shell (not the panel) so resets
    // re-zero it even while the panel is closed.
    std::unique_ptr<class Disk2DebugPanel>    m_disk2DebugPanel;
    std::unique_ptr<class InputDebugPanel>    m_inputDebugPanel;
    std::unique_ptr<class PrinterPanel>       m_printerPanel;

    // Live-preview bookkeeping (UpdatePrinterPreview). Auto-open fires once when a
    // *new* print begins -- activity resuming after an idle gap -- so it opens even
    // when a prior pending strip is still loaded, yet a mid-print manual close does
    // not fight a re-open (activity never goes idle mid-print). Refresh pacing and
    // change detection live in the panel's viewport (PrinterPanel::RefreshLive).
    bool                                      m_printerAutoOpenArmed    = true;
    uint64_t                                  m_printerAutoOpenActivity = 0;
    int64_t                                   m_printerActiveLastMs     = 0;
    std::chrono::steady_clock::time_point     m_uptimeAnchor { std::chrono::steady_clock::now() };

    // Extracted shell-side managers. WindowManager owns the per-monitor
    // placement persistence (now backed by GlobalUserPrefs JSON).
    // ClipboardManager holds references back to the shared CPU/UI
    // state it operates on plus a pointer-to-pointer for the active
    // keyboard so machine switches do not require re-wiring.
    WindowManager                             m_windowManager { m_globalPrefs, [this] { SaveGlobalPrefs(); } };
    std::unique_ptr<ClipboardManager>         m_clipboardManager;
    std::unique_ptr<DiskManager>              m_diskManager;
    std::unique_ptr<MachineManager>           m_machineManager;
    std::unique_ptr<WindowCommandManager>     m_windowCommandManager;
};




