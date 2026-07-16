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
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Chrome/InputDeviceSelector.h"
#include "Ui/Chrome/PrinterIndicator.h"
#include "Ui/Chrome/MainMenu.h"
#include "Ui/ColorUtil.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Disk2DebugPanel.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/IDriveCommandSink.h"
#include "Ui/InputDebugPanel.h"
#include "Ui/ThemeManager.h"
#include "Ui/UiShell.h"
#include "Widgets/DxuiTooltip.h"
#include "Widgets/DxuiSurface.h"
#include "UiCommandTypes.h"
#include "Video/CharacterRomData.h"
#include "Video/VideoOutput.h"
#include "Video/VideoTiming.h"
#include "WasapiAudio.h"
#include "Window/DxuiHwndSource.h"
#include "Window/IDxuiHostClient.h"
#include "Core/DxuiAbsoluteLayout.h"
#include "Core/DxuiDockLayout.h"
#include "Core/DxuiViewport.h"



class DxuiHwndSource;
class SettingsSheet;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBand
//
//  Zero-render IDxuiControl whose only job is to carry a docked chrome
//  band's pixel thickness in its Bounds() so DxuiDockLayout can arrange
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
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell : public IDxuiHostClient, public IDriveCommandSink, public IDxuiViewportInputSink
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

    // IDxuiViewportInputSink -- the emulator viewport routes its raw
    // keyboard input here (SetWantsAllKeys(true) so even Esc/Tab/arrows
    // arrive). The chrome / settings / meta pre-checks run in OnKeyDown /
    // OnChar before the event reaches the viewport, so these apply the
    // keystroke straight to the Apple ][ keyboard + game port.
    bool  OnViewportKey   (const DxuiKeyEvent   & ev) override;
    bool  OnViewportMouse (const DxuiMouseEvent & ev) override;
    DxuiMessageResult  OnMouseMove     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseLeave    () override;
    DxuiMessageResult  OnLButtonDown   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnLButtonUp     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonDown   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonUp     (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnSetCursor     (WORD hitTest) override;
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
    // bands) via the DxuiDockLayout and invokes m_viewport->Layout,
    // which fires OnViewportBoundsChanged when the rectangle differs
    // from the last value reported.
    void    UpdateViewportLayout         (int widthPx, int heightPx);

    // Chrome-band sizing via DxuiDockLayout (replaces LayoutManager).
    // SyncChromeBands stamps each band's Bounds() with its DPI-scaled
    // pixel thickness. ComputeViewportRect docks the bands + center and
    // returns the middle (emulator viewport) rect. ClientSizeForCenterPx
    // is the inverse: given a desired center size in px, the client size
    // that hosts it. ClientSizeForFramebufferPx DPI-scales a DIP
    // framebuffer grid first, then adds the chrome insets.
    void    SyncChromeBands              ();
    RECT    ComputeViewportRect          (int widthPx, int heightPx);

    // The emulator viewport (CRT output area) in *screen* pixels: the middle
    // rect from ComputeViewportRect at the current back-buffer size, mapped
    // through the main window's client origin. The Settings live-preview
    // compositor (#8) intersects this with the (composited) sheet window to
    // punch a see-through hole revealing the running emulator behind the sheet.
    RECT    EmulatorContentScreenRect    ();

    // Re-run the chrome layout at the current client size after a machine
    // switch: adding/removing the Disk ][ controller changes the drive band +
    // widgets + hit-test map, but no WM_SIZE fires when the window size itself
    // is unchanged, so OnSize would never re-evaluate it. See the
    // WM_APP_DXUI_UPDATE_TITLE handler (the switch-completion signal).
    void    ReflowChromeForMachineChange ();

    // Whether the second (external) drive-mount widget should be visible.
    // Always true for machines whose second drive is fixed hardware; on the
    // //c (banked system ROM) the external drive is an optional add-on, shown
    // only when m_externalDriveConnected. The drive-layout paths consult this
    // to hide m_driveChrome[1] and skip its hit rect when disconnected.
    bool    ShouldShowExternalDrive      () const;

    SIZE    ClientSizeForCenterPx        (int centerWidthPx, int centerHeightPx);
    SIZE    ClientSizeForFramebufferPx   (int framebufferWidthDp, int framebufferHeightDp);

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
    InputMappingMode  DisplayInputMode() const
    {
        return (m_pointerMode != InputMappingMode::Off) ? m_pointerMode
             : (m_arrowsJoystick ? InputMappingMode::Joystick : InputMappingMode::Off);
    }

    // With a connected mouse and no pointer mapping chosen, the //c
    // defaults Pointer to Mouse (runtime nudge, not persisted; invisible
    // until mouse software runs thanks to the firmware-live gate).
    void    ApplyDefaultPointerForMachine();

private:
    void    SyncInputModeUi();
    void    SyncSelectorState();
public:

    // Radio-group toggle for the Machine-menu items: selects `target`, or
    // turns mapping Off if `target` is already the active mode.
    void    ToggleInputMappingMode (InputMappingMode target);

    // //c mouse mode. True while Mouse mode is selected AND the
    // current machine has the IOU mouse — every runtime consumer guards on
    // this, so a persisted Mouse mode on a mouse-less machine is inert.
    bool    GuestMouseActive       () const;

    // True when guest software has actually turned the mouse on: the
    // firmware's SETMOUSE programs ENBXY through the IOU for every active
    // mode, a hardware sequence ($C079 -> $C059 -> $C078) that garbage RAM
    // cannot fake. Gates the cursor-hide and button capture so the host
    // pointer never vanishes (or gets swallowed) while nothing mouse-aware
    // is running — which in turn makes Mouse mode safe to leave on.
    bool    GuestMouseLive         () const;

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

    // Per-machine pending-strip directory (FR-026):
    // <assetBase>/Machines/<current machine>/PendingPrint.
    fs::path  PendingPrintDir () const
    {
        return fs::path (m_assetBaseDir) / L"Machines" / fs::path (m_currentMachineName) / L"PendingPrint";
    }

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

    // Activates the named theme LIVE (reskins the chrome via the
    // ThemeManager listener) WITHOUT persisting it to GlobalUserPrefs.
    // Used by the Settings Theme page's "Apply now" affordance so the
    // user can preview a theme on the real chrome; a subsequent Cancel
    // re-activates the baseline theme, and OK persists via
    // ApplyAndPersistTheme. No-op if empty; falls back to Skeuomorphic.
    HRESULT ApplyThemeLive       (const std::string & themeName);

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

    // Position the printer status indicator in the command-bar dead space to
    // the right of the centred drive widgets, or Hide() it when the machine
    // has no printer card. Does not affect drive centring.
    void    LayoutPrinterIndicator (int bottomInsetPx, int clientW, int clientH, UINT dpi);

    // Open (creating if needed) the printer panel / print preview window, and
    // push it a fresh snapshot of the current strip. `activate` false shows it
    // without stealing focus from the guest (used by the auto-open path).
    void    ShowPrinterPanel (bool activate = true);

    // Owner HWND for printer confirmation / notice message boxes: the preview
    // panel when it is open and visible (so the box centers on the dialog the
    // user is acting in), otherwise the main window.
    HWND    PrinterDialogOwner () const;

    // Force-refresh the printer panel from the drain worker (race-free, without
    // stopping it): the panel snapshots and renders only its visible ~1-page
    // viewport span. Non-destructive: the live interpreter keeps running, so
    // refreshing mid-print can never disturb the job's state or the output.
    void    SnapshotStripToPanel ();

    // Per-frame: sample the worker's status signals, recompute the indicator
    // state, and mark a redraw only when it changes (so a static screen still
    // repaints the LED on a transition).
    void    UpdatePrinterIndicator ();

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

    // Render a "simple" dialog (text + buttons + an optional Info /
    // Warning / Error glyph icon -- no custom body, tick, hyperlinks,
    // app-bitmap icon, or resizable mode) as a MessageDialog (DxuiWindow
    // shown via ShowModalDialog). Returns the chosen button's resultCode (or
    // def.closeBoxResult / -1 on a close gesture).
    int          ShowSimpleDialogViaDxui (const DialogDefinition & def);

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
    friend class SettingsSheet;
    friend class SettingsApplyController;
    friend class SettingsDisplayCrtBridge;
    friend class SettingsMachineCatalog;

    HACCEL              m_accelTable      = nullptr;
    HINSTANCE           m_hInstance       = nullptr;
    HWND                m_hwnd            = nullptr;
    bool                m_initialSizeReconciled = false;

    // Authoritative per-window DPI scaler. Mirrors the one inside
    // DxuiHwndSource; updated from OnDpiChanged and seeded after
    // m_host->Create() returns. The chrome-band dock scales its band
    // thicknesses through this member.
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

    // UI-thread filesystem and chrome ownership. The painter pass
    // and shell composition is reintroduced in a later phase; for now
    // only the per-window filesystem stays here so the settings panel
    // and config store can resolve paths on the UI thread.
    Win32FileSystem        m_uiFs;

    // Chrome surfaces. MainMenu owns the parity table for legacy IDM_*
    // commands and runs alongside the existing Win32 menu bar until the
    // painter retires the latter. The caption (title + icon + min/max/
    // close) is owned and rendered by the DxuiHwndSource, not here.
    MainMenu            m_mainMenu;
    CassoTheme         m_chromeTheme    = CassoTheme::Skeuomorphic();
    std::array<DriveWidget, 2> m_driveChrome;

    // Chrome printer status indicator (right of the centred drive widgets) and
    // the pure model that derives its state from the worker's live signals.
    PrinterIndicator    m_printerIndicator;
    PrinterStatusModel  m_printerStatus;

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
    InputDeviceSelector   m_joystickButton;   // Segmented device selector
    DxuiTooltip               m_joystickTooltip;

    // Solid background for the bottom drive-bar band. The CRT composite
    // writes the whole back buffer (emulator frame + black), so the chrome
    // bands need an opaque surface painted on top; the title and menu bars
    // cover their own bands, this covers the drive bar.
    DxuiSurface           m_driveBandSurface;

    // Last geometry passed to LayoutJoystickButton, cached so
    // RelayoutJoystickButton can resize the button in place when the
    // input mode (and thus the label width) changes between layout passes.
    int   m_joyBtnClientW    = 0;
    int   m_joyBtnBandTop    = 0;
    int   m_joyBtnBandHeight = 0;
    UINT  m_joyBtnDpi        = 96;

    // Chrome layout via DxuiDockLayout. The three bands carry the title
    // bar, nav strip, and drive bar pixel thicknesses in their Bounds();
    // m_centerBand (Fill) captures the emulator viewport rect the dock
    // leaves in the middle. m_driveBarThicknessDp is the live drive-bar
    // thickness the theme mutates (compact vs full).
    static constexpr int  s_kTitleBarBandDp     = 32;
    static constexpr int  s_kNavStripBandDp     = 32;
    static constexpr int  s_kInitialDriveBandDp = 256;

    DxuiDockLayout           m_chromeDock;
    ChromeBand               m_titleBand;
    ChromeBand               m_navBand;
    ChromeBand               m_driveBand;
    ChromeBand               m_centerBand;
    int                      m_driveBarThicknessDp = s_kInitialDriveBandDp;

    // Whether the current WINDOW height was sized for a Disk ][ controller
    // being present. Written by OnSize (the authoritative layout, WM_SIZE-only)
    // to the disk-presence it just laid out; ReflowChromeForMachineChange reads
    // this pre-switch value to grow/shrink the window by the drive-band delta
    // (so the viewport keeps its size + the top-left stays put) rather than
    // re-centring inside a fixed window.
    bool                     m_chromeSizedForHasDisk = true;

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
    // silicon stays but GuestMouseActive() is false (no host input feeds
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
    DriveWidgetController                m_driveWidgets;
    DxuiDragDropTarget                       m_dragDropTarget;

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




