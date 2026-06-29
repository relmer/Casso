#pragma once

#include "Pch.h"

#include "Audio/Disk2AudioSource.h"
#include "Audio/DriveAudioMixer.h"
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
#include "Shell/ClipboardManager.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "Shell/MachineManager.h"
#include "Shell/WindowCommandManager.h"
#include "Shell/WindowManager.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Chrome/JoystickToggleButton.h"
#include "Ui/Chrome/LayoutManager.h"
#include "Ui/Chrome/MainMenu.h"
#include "Ui/Chrome/TitleBar.h"
#include "Ui/ColorUtil.h"
#include "Ui/Dialog/DialogDefinition.h"
#include "Ui/Dialog/DialogPrimitive.h"
#include "Ui/Disk2DebugPanel.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/IDriveCommandSink.h"
#include "Ui/InputDebugPanel.h"
#include "Ui/Settings/SettingsPanel.h"
#include "Ui/ThemeManager.h"
#include "Ui/UiShell.h"
#include "Widgets/DxuiTooltip.h"
#include "UiCommandTypes.h"
#include "Video/CharacterRomData.h"
#include "Video/VideoOutput.h"
#include "Video/VideoTiming.h"
#include "WasapiAudio.h"
#include "Window/DxuiHostWindow.h"
#include "Window/IDxuiHostClient.h"
#include "Core/DxuiAbsoluteLayout.h"
#include "Core/DxuiViewport.h"



class DxuiHostWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell : public IDxuiHostClient, public IDriveCommandSink
{
public:
    EmulatorShell();
    ~EmulatorShell();

    HRESULT Initialize (
        HINSTANCE              hInstance,
        const wstring        & machineName,
        const MachineConfig  & config,
        const string    & disk1Path,
        const string    & disk2Path);

    int RunMessageLoop();

    void HandleCommand (WORD commandId);

    // State
    bool IsRunning() const { return m_cpuManager.IsRunning(); }
    bool IsPaused() const { return m_cpuManager.IsPaused(); }

    // Access bus for test wiring
    MemoryBus & GetBus() { return m_memoryBus; }

    // Main window HWND. Owned by m_host (DxuiHostWindow in full-
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

    // ---- IDriveCommandSink --------------------------------------
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
    DxuiMessageResult  OnMouseMove     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseLeave    () override;
    DxuiMessageResult  OnLButtonDown   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnLButtonUp     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonDown   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonUp     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnActivateApp   (bool active) override;
    DxuiMessageResult  OnKillFocus     () override;
    DxuiMessageResult  OnCancelMode    () override;
    DxuiMessageResult  OnMove          (int x, int y) override;
    DxuiMessageResult  OnNotify        (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnSize          (UINT widthPx, UINT heightPx) override;
    DxuiMessageResult  OnGetMinMax     (MINMAXINFO * info) override;
    DxuiMessageResult  OnTimer         (UINT_PTR timerId) override;
    DxuiMessageResult  OnInitMenuPopup (HMENU hMenu, UINT itemIndex, bool isWindowMenu) override;
    DxuiMessageResult  OnNcMouseMove   (LRESULT hitTest, int xScreen, int yScreen) override;
    DxuiMessageResult  OnNcMouseLeave() override;
    DxuiMessageResult  OnNcLButtonDown (LRESULT hitTest, int xScreen, int yScreen) override;
    DxuiMessageResult  OnNcLButtonUp   (LRESULT hitTest, int xScreen, int yScreen) override;
    LRESULT            OnDrawItem      (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    void               OnDestroy       () override;
    void               OnDpiChanged    (UINT newDpi) override;

    // Bespoke NC hit-test classifier for the legacy chrome surfaces
    // (TitleBar + system-button rect cache). Plugged into the
    // DxuiHostWindow via SetHitTestDelegate in OnCreate so the
    // adopt-mode shim consults it before falling back to the
    // framework resize-edge / panel-tree classifier. Operates in
    // screen coordinates to match the WM_NCHITTEST LPARAM packing.
    LRESULT ClassifyHitForLegacyChrome (POINT ptScreen);

    // CPU thread entry point and helpers
    void RunOneFrame();
    void ExecuteCpuSlices();
    void RenderFramebuffer();
    void DispatchCpuCommand (const EmulatorCommand & cmd);

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

    void OnCpuThreadStart();
    void OnCpuThreadStop();
    void PublishFramebuffer();
    void UpdateWindowTitle();

    // Initialization helpers
    HRESULT CreateEmulatorWindow (HINSTANCE hInstance);
    void    ReconcileInitialClientSize ();

    // Drives the host's root panel layout for the Apple ][ viewport
    // child. Computes the framebuffer rectangle (client minus chrome
    // bands) from the current LayoutManager result and invokes
    // m_viewport->Layout, which fires OnViewportBoundsChanged when
    // the rectangle differs from the last value reported.
    void    UpdateViewportLayout         (int widthPx, int heightPx);

    // Bounds-changed callback wired onto m_viewport. Stores the new
    // pixel rectangle and forwards it to m_d3dRenderer.SetTargetBounds
    // so the framebuffer compositor can track where to draw once the
    // swap-chain restructure completes later in Phase 11d.
    void    OnViewportBoundsChanged      (const RECT & boundsPx);

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
    void    SetInputMappingMode (InputMappingMode mode);

    // Advance the input mapping mode Off -> Joystick -> Paddle -> Off,
    // routed from the drive-bar widget, the Machine menu, and Ctrl+J.
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

private:
    // Machine switching delegated to MachineManager. Kept as a
    // public delegator so the existing IDM_FILE_OPEN command-queue
    // path can call the shell without learning the manager.
    HRESULT SwitchMachine (const std::wstring & machineName);
    void    ShowMachinePicker();
    const std::wstring &  CurrentMachineName () const { return m_currentMachineName; }

    // Accessor used by the Settings → Theme preview to copy the live
    // emulator framebuffer into the mock window. The UI framebuffer is
    // the post-CRT-effects pixel buffer the chrome composes on top of;
    // returning a raw pointer is safe because the chrome composition
    // pass runs synchronously after the framebuffer is published.
    const uint32_t *  UiFramebufferPixels () const
    {
        return m_uiFramebuffer.empty() ? nullptr : m_uiFramebuffer.data();
    }

    // Accessor for the Settings → Theme preview so it can render the
    // basename label with the actual filename of whatever disk image is
    // currently mounted in each drive (or an empty string if the drive
    // is empty). Index 0 is drive 1, index 1 is drive 2.
    const std::wstring &  MountedImagePath (int driveIndex) const
    {
        static const std::wstring  s_kEmpty;

        if (driveIndex < 0 || driveIndex >= (int) m_driveWidgetState.size())
        {
            return s_kEmpty;
        }

        return m_driveWidgetState[(size_t) driveIndex].mountedImagePath;
    }

    // Base directory for user preferences. SettingsPanel.CommitApply
    // uses this as the fallback save path when the unified store is not
    // available.
    const std::wstring &  AssetBaseDir () const { return m_assetBaseDir; }

    // Live channel for the Settings → Display monitor dropdown. The
    // dropdown calls this on every selection so the user sees the
    // colour-treatment change as they hover/select; Cancel restores
    // the baseline by calling this again with the entry-state value.
    // Bypasses the IDM command queue so the change is visible on the
    // next CPU frame rather than waiting for queue drain.
    void  SetColorModeLive (int settingsColorModeIndex);

    // Live-set the text color used on the Color monitor (0xAARRGGBB),
    // resolved from a ColorMonitorTextMode + custom color. Like
    // SetColorModeLive, the Settings panel calls this on hover / select so
    // the change shows on the next CPU frame, and on Cancel to restore.
    void  SetColorMonitorTextArgbLive (uint32_t argb);

    // Activates the named theme in ThemeManager (which notifies the
    // chrome cache listener) and persists the choice into GlobalUserPrefs.
    // No-op if the name is empty; falls back to Skeuomorphic if unknown.
    HRESULT ApplyAndPersistTheme (const std::string & themeName);

    // Pushes a freshly-activated CassoTheme into the layout-affecting
    // chrome state: drive bar thickness, per-drive compact flag, and
    // (if the bottom inset changed) a window resize that preserves the
    // emulator pixel grid. Called from the ThemeManager listener.
    void    ApplyThemeToChrome   (const CassoTheme & theme);

    // Positions the joystick-mode toggle button vertically centered in the
    // empty band above the drive widgets (the top portion of the bottom
    // drive-bar inset) and centered horizontally in the window. bandTopPx
    // and bandHeightPx bracket that band; the caller derives them from
    // the layout result so the button sits the same whether or not a
    // Slot 6 controller is mounted.
    void    LayoutJoystickButton (int clientW,
                                  int bandTopPx,
                                  int bandHeightPx,
                                  UINT dpi);

    // Re-run the input-mode button layout from the last cached geometry,
    // so the frame resizes immediately when the mode (and thus the label
    // width) changes between resize / DPI events. No-op until the first
    // LayoutJoystickButton has cached valid geometry.
    void    RelayoutJoystickButton ();

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

    // Lazily registers the DialogPrimitive window class on first call
    // and shows the supplied dialog modally. Returns the resultCode
    // of the chosen button, or -1 on close-gesture.
    int     ShowModalDialog      (const DialogDefinition & def);

    // Push a freshly mounted disk image onto the recent-disks MRU
    // and persist user prefs. Best-effort; never propagates failures
    // back into the mount path.
    void    RecordRecentDisk     (const std::wstring & path);

    // MachineManager and WindowCommandManager touch enough shell
    // state during construction and command dispatch that friend
    // declarations are the pragmatic seam; no new global state is
    // introduced.
    friend class MachineManager;
    friend class WindowCommandManager;
    friend class SettingsPanel;
    friend class SettingsApplyController;
    friend class SettingsDisplayCrtBridge;
    friend class SettingsMachineCatalog;

    HACCEL              m_accelTable      = nullptr;
    HINSTANCE           m_hInstance       = nullptr;
    HWND                m_hwnd            = nullptr;
    bool                m_initialSizeReconciled = false;

    // Authoritative per-window DPI scaler. Mirrors the one inside
    // DxuiHostWindow; updated from OnDpiChanged and seeded after
    // m_host->Create() returns. LayoutManager holds a const ref to
    // this member.
    DxuiDpiScaler       m_scaler;

    MemoryBus           m_memoryBus;
    ComponentRegistry   m_registry;
    InterruptController m_interruptController;
    unique_ptr<EmuCpu> m_cpu;
    unique_ptr<class Prng> m_prng;
    size_t                 m_traceCapacity = 0;       // --trace ring size (entries); 0 = off
    std::atomic<bool>      m_traceDumped { false };   // one-shot guard for DumpTrace
   
    D3DRenderer            m_d3dRenderer;
    WasapiAudio            m_wasapiAudio;
    DialogPrimitive        m_dialogPrimitive;

    // UI-thread filesystem and chrome ownership. The painter pass
    // and shell composition is reintroduced in a later phase; for now
    // only the per-window filesystem stays here so the settings panel
    // and config store can resolve paths on the UI thread.
    Win32FileSystem        m_uiFs;

    // Chrome surfaces. TitleBar owns the per-button rect cache that
    // the legacy-chrome NC hit-test classifier queries. MainMenu owns
    // the parity table for legacy IDM_* commands. Both run alongside
    // the existing Win32 menu bar until the painter retires the latter.
    TitleBar            m_titleBar;
    MainMenu            m_mainMenu;
    CassoTheme         m_chromeTheme    = CassoTheme::Skeuomorphic();
    std::array<DriveWidget, 2> m_driveChrome;

    // DxuiHostWindow running in full-ownership mode. Owns the main
    // HWND (registers WNDCLASS "CassoWindow", calls CreateWindowExW,
    // and applies DwM rounded-corners / immersive-dark / extended
    // frame). Created with `createSwapChain = true` so the host owns
    // the D3D11 device + DXGI swap chain and runs the panel-tree paint
    // pump; the Apple ][ framebuffer renderer composites into that back
    // buffer via the host's before-present hook, and chrome paints on
    // top via the adopted controls. The bespoke TitleBar + system-button
    // NC hit-test classifier is plugged in via SetHitTestDelegate
    // (see ClassifyHitForLegacyChrome below). EmulatorShell is the
    // IDxuiHostClient so all consumer-side Win32 messages
    // (WM_KEYDOWN, WM_COMMAND, WM_SIZE, ...) dispatch through the
    // OnXxx overrides above.
    std::unique_ptr<DxuiHostWindow>  m_host;

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
    // by the host's before-present hook (DxuiHostWindow::PaintPump ->
    // D3DRenderer::UploadAndComposite). Points into m_uiFramebuffer
    // when the emulator produced a new frame this iteration, or nullptr
    // to re-composite the last upload (chrome-only repaints). Touched
    // only on the UI thread.
    const uint32_t *                 m_pendingFramebuffer = nullptr;

    // Joystick-mode toggle button (mirrors IDM_MACHINE_ARROWS_JOYSTICK),
    // centered in the drive bar above the drive widgets, with its own
    // hover tooltip.
    JoystickToggleButton  m_joystickButton;
    DxuiTooltip               m_joystickTooltip;

    // Last geometry passed to LayoutJoystickButton, cached so
    // RelayoutJoystickButton can resize the button in place when the
    // input mode (and thus the label width) changes between layout passes.
    int   m_joyBtnClientW    = 0;
    int   m_joyBtnBandTop    = 0;
    int   m_joyBtnBandHeight = 0;
    UINT  m_joyBtnDpi        = 96;

    // Chrome layout planner. Owns the canonical inset math for the
    // title bar, nav strip, and drive bar; replaces the historical
    // ChromeMetrics constants that drifted between EmulatorShell and
    // WindowCommandManager. Edge contributors below are pointer-tied
    // to this layout and report their desired thickness on demand.
    LayoutManager            m_layout { m_scaler };
    SimpleEdgeContributor   m_titleBarSlot { ChromeEdge::Top,    32 };
    SimpleEdgeContributor   m_navStripSlot { ChromeEdge::Top,    32 };
    SimpleEdgeContributor   m_driveBarSlot { ChromeEdge::Bottom, 256 };

    // Drive widget state pump. The controller channel publishes
    // per-drive door/spin sync events the chrome painter will consume
    // once reintroduced. The drag-drop target registers a single
    // IDropTarget on the main HWND. Per-drive UI/CPU bridge state
    // lives in m_driveWidgetState; the CPU thread's motor + nibble
    // counters are sampled once per UI frame and pushed through the
    // controller.
    DriveWidgetController                m_driveWidgets;
    DxuiDragDropTarget                       m_dragDropTarget;

    // Native UI shell. Owns the painter, text renderer, hit-tester,
    // focus manager, animation broker, and input translator. Wired
    // onto D3DRenderer's after-blit hook so chrome composites every
    // frame between the emulator blit and Present.
    UiShell                    m_uiShell;

    // Consolidated settings panel. Lazily constructed pieces so we
    // can defer their I/O until first Show() on the panel.
    // ThemeManager + UserConfigStore + GlobalUserPrefs are owned
    // here so SettingsPanel can be a pure view layer.
    std::unique_ptr<ThemeManager>        m_themeManager;
    std::unique_ptr<UserConfigStore>     m_userConfigStore;
    GlobalUserPrefs                      m_globalPrefs;
    SettingsPanel                        m_settingsPanel;

    std::array<DriveWidgetState, 2>      m_driveWidgetState;

    // Set true once OleInitialize has succeeded on the UI thread so
    // shutdown can pair the call with OleUninitialize. RegisterDragDrop
    // requires OLE (STA) on the registering thread.
    bool                                 m_fOleInitialized = false;

    // Drive audio. Mixer is always allocated; per-drive sources are
    // populated only when the active machine config carries a
    // Disk II controller (FR-015).
    DriveAudioMixer                      m_driveAudioMixer;
    vector<unique_ptr<Disk2AudioSource>> m_diskAudioSources;

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
        class VideoOutput *           activeVideoMode  = nullptr;
    };

    MachineRefs                   m_refs;

    unique_ptr<class Apple2eMmu>  m_mmu;
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

    // Double framebuffer (CPU renders, UI presents, protected by m_fbMutex)
    mutex                         m_fbMutex;
    vector<uint32_t>              m_cpuFramebuffer;
    vector<uint32_t>              m_textOverlay;
    vector<uint32_t>              m_uiFramebuffer;
    bool                          m_fbReady = false;

    uint32_t                      m_cyclesPerFrame  = 17050;
    double                        m_sampleRemainder = 0.0;

    // Last arrow key pressed for each emulated joystick axis pair (0 if
    // none). Lets opposing directions resolve last-pressed-wins so a
    // rolling reversal flips the axis instead of cancelling to center.
    WPARAM          m_lastHorizontalArrowVk = 0;
    WPARAM          m_lastVerticalArrowVk   = 0;

    // How host arrow / pointer input is mapped onto the emulated game
    // port (Off / Joystick / Paddle). Mirrors
    // GlobalUserPrefs::inputMappingMode and is cycled via the Machine
    // menu's "Cycle Input Mode" item, Ctrl+J, and the drive-bar widget.
    InputMappingMode  m_inputMode = InputMappingMode::Off;

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





