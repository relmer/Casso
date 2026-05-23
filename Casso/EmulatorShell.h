#pragma once

#include "Pch.h"
#include "Window.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Core/MachineConfig.h"
#include "Core/InterruptController.h"
#include "Core/ComponentRegistry.h"
#include "D3DRenderer.h"
#include "MenuSystem.h"
#include "DebugConsole.h"
#include "Ui/Chrome/TitleBar.h"
#include "Ui/Chrome/NavLayer.h"
#include "Ui/Chrome/ChromeTheme.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DragDropTarget.h"
#include "Ui/IDriveCommandSink.h"
#include "Ui/SettingsPanel.h"
#include "Ui/ThemeManager.h"
#include "Ui/UiShell.h"
#include "Config/Win32FileSystem.h"
#include "Config/UserConfigStore.h"
#include "Config/GlobalUserPrefs.h"
#include "Video/VideoOutput.h"
#include "Video/CharacterRomData.h"
#include "Video/VideoTiming.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Audio/DriveAudioMixer.h"
#include "Audio/DiskIIAudioSource.h"
#include "WasapiAudio.h"
#include "DiskIIDebugDialog.h"
#include "Shell/ClipboardManager.h"
#include "Shell/CpuManager.h"
#include "Shell/WindowManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell : public Window, public IDriveCommandSink
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
    // DiskIIAudioSource. On subsequent calls: show + bring to front.
    void OpenDiskIIDebugDialog();

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

        if (m_diskIIDebugDialog != nullptr)
        {
            m_diskIIDebugDialog->SetUptimeAnchor (m_uptimeAnchor);
            // Spec-006 bug-fix. Clear stale rows from the pre-reset
            // boot so the post-reset uptime anchor doesn't end up
            // formatting events that pre-date its own zero point.
            m_diskIIDebugDialog->ClearEvents();
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

    // Static window procs for child windows
    static LRESULT CALLBACK s_RenderSurfaceWndProc (
        HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    // Window message handler overrides
    bool    OnChar (WPARAM ch, LPARAM lParam) override;
    bool    OnCommand (HWND hwnd, int id) override;
    LRESULT OnCreate (HWND hwnd, CREATESTRUCT * pcs) override;
    bool    OnDestroy (HWND hwnd) override;
    bool    OnDrawItem (HWND hwnd, int idCtl, DRAWITEMSTRUCT * pdis) override;
    bool    OnKeyDown (WPARAM vk, LPARAM lParam) override;
    bool    OnKeyUp (WPARAM vk, LPARAM lParam) override;
    bool    OnMouseMove (WPARAM wParam, LPARAM lParam) override;
    bool    OnLButtonDown (WPARAM wParam, LPARAM lParam) override;
    bool    OnLButtonUp (WPARAM wParam, LPARAM lParam) override;
    bool    OnMove (HWND hwnd, int x, int y) override;
    bool    OnNotify (HWND hwnd, WPARAM wParam, LPARAM lParam) override;
    bool    OnSize (HWND hwnd, UINT width, UINT height) override;
    bool    OnTimer (HWND hwnd, UINT_PTR timerId) override;
    bool    OnInitMenuPopup (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu) override;

    // P4 custom-chrome overrides — borderless window + WM_NCHITTEST
    // delegated to TitleBarHitTest, system-button click routing on
    // WM_NCLBUTTONUP.
    bool    OnNcCalcSize  (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outResult) override;
    LRESULT OnNcHitTest   (HWND hwnd, int xScreen, int yScreen) override;
    bool    OnNcLButtonUp (HWND hwnd, LRESULT hitTest, int xScreen, int yScreen) override;

    // Command group handlers
    void OnFileCommand (int id);
    void OnEditCommand (int id);
    void OnMachineCommand (int id);
    void OnViewCommand (int id);
    void OnDiskCommand (int id);
    void OnHelpCommand (int id);

    // CPU thread entry point and helpers
    void RunOneFrame();
    void ExecuteCpuSlices();
    void RenderFramebuffer();
    void DispatchCpuCommand (const EmulatorCommand & cmd);
    void OnCpuThreadStart();
    void OnCpuThreadStop();
    void PublishFramebuffer();
    void UpdateWindowTitle();
    void SelectVideoMode();

    // Initialization helpers
    HRESULT CreateEmulatorWindow (HINSTANCE hInstance);
    HRESULT CreateMemoryDevices (const MachineConfig & config);
    void    WireLanguageCard();
    void    WirePageTable();
    void    RebuildBankingPages();
    void    MountCommandLineDisks (const string & disk1Path, const string & disk2Path);
    HRESULT MountDiskInSlot6 (int drive, const string & path);
    void    EjectDiskInSlot6 (int drive);
    void    RemountSlot6Disks();
    class DiskIIController * FindSlot6Controller();
    void    CreateVideoModes();
    HRESULT CreateCpu (const MachineConfig & config);

    // Pumps the per-drive widget state from the CPU-side disk
    // controller (motor + R/W nibble deltas) into m_driveWidgetState
    // then asks the DriveWidgetController to push the result into the
    // chrome drive-widget classes. Cheap; safe to call every frame.
    void    UpdateDriveWidgets();
    int64_t NowMs() const;

    Byte * GetAuxRamBuffer();

    // Machine switching
    void    ShowMachinePicker();
    HRESULT SwitchMachine (const wstring & machineName);

    HRESULT CreateRenderSurface ();
    HRESULT PromptForDiskImage (int drive);

    // Queue a command for the CPU thread
    void PostCommand (WORD id, const string & payload = "");

    HACCEL              m_accelTable      = nullptr;
    HWND                m_renderHwnd      = nullptr;

    MemoryBus           m_memoryBus;
    ComponentRegistry   m_registry;
    InterruptController m_interruptController;
    unique_ptr<EmuCpu> m_cpu;
    unique_ptr<class Prng> m_prng;

    D3DRenderer         m_d3dRenderer;
    MenuSystem          m_menuSystem;
    WasapiAudio         m_wasapiAudio;
    DebugConsole        m_debugConsole;

    // UI-thread filesystem and chrome ownership. The painter pass
    // and shell composition is reintroduced in a later phase; for now
    // only the per-window filesystem stays here so the settings panel
    // and config store can resolve paths on the UI thread.
    Win32FileSystem     m_uiFs;

    // Chrome surfaces. TitleBar owns the per-button rect cache that
    // the WM_NCHITTEST helper queries. NavLayer owns the parity
    // table for legacy IDM_* commands. Both run alongside the
    // existing Win32 menu bar until the painter retires the latter.
    TitleBar            m_titleBar;
    NavLayer            m_navLayer;
    ChromeTheme         m_chromeTheme;
    std::array<DriveWidget, 2> m_driveChrome;

    // Drive widget state pump. The controller channel publishes
    // per-drive door/spin sync events the chrome painter will consume
    // once reintroduced. The drag-drop target registers a single
    // IDropTarget on the main HWND. Per-drive UI/CPU bridge state
    // lives in m_driveWidgetState; the CPU thread's motor + nibble
    // counters are sampled once per UI frame and pushed through the
    // controller.
    DriveWidgetController                m_driveWidgets;
    DragDropTarget                       m_dragDropTarget;

    // Native UI shell. Owns the painter, text renderer, hit-tester,
    // focus manager, animation broker, and input translator. Wired
    // onto D3DRenderer's after-blit hook so chrome composites every
    // frame between the emulator blit and Present.
    UiShell             m_uiShell;

    // Consolidated settings panel. Lazily constructed pieces so we
    // can defer their I/O until first Show() on the panel.
    // ThemeManager + UserConfigStore + GlobalUserPrefs are owned
    // here so SettingsPanel can be a pure view layer.
    std::unique_ptr<ThemeManager>        m_themeManager;
    std::unique_ptr<UserConfigStore>     m_userConfigStore;
    GlobalUserPrefs                      m_globalPrefs;
    SettingsPanel                        m_settingsPanel;

    std::array<DriveWidgetState, 2>      m_driveWidgetState;

    // Per-drive read/write nibble counter snapshots from the previous
    // UI frame. The CPU thread doesn't publish a "disk active" signal
    // directly; we derive it by comparing the engine's lifetime nibble
    // counters between two consecutive UI frames -- if either counter
    // increased, the LED stays in the Active state for the next frame.
    std::array<uint64_t, 2>              m_lastReadNibbles  {};
    std::array<uint64_t, 2>              m_lastWriteNibbles {};

    // Set true once OleInitialize has succeeded on the UI thread so
    // shutdown can pair the call with OleUninitialize. RegisterDragDrop
    // requires OLE (STA) on the registering thread.
    bool                                 m_fOleInitialized = false;

    // Drive audio. Mixer is always
    // allocated; per-drive sources are populated only when the
    // active machine config carries a Disk II controller (FR-015).
    // Cold-boot flag suppresses OnDiskInserted during startup
    // mounts so command-line / autoload paths don't trigger the
    // door-close sound at app launch (FR-013).
    DriveAudioMixer            m_driveAudioMixer;
    vector<unique_ptr<DiskIIAudioSource>> m_diskAudioSources;
    bool                       m_coldBootMountWindow = true;

    // Owned devices
    vector<unique_ptr<MemoryDevice>> m_ownedDevices;

    // Video
    vector<unique_ptr<VideoOutput>> m_videoModes;
    CharacterRomData    m_charRom;

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
        class AppleSpeaker *          speaker          = nullptr;
        class RamDevice *             mainRamDev       = nullptr;
        class DiskIIController *      diskController   = nullptr;
        class VideoOutput *           activeVideoMode  = nullptr;
    };

    MachineRefs                   m_refs;

    unique_ptr<class AppleIIeMmu> m_mmu;
    unique_ptr<VideoTiming>       m_videoTiming;

    // / T097 / FR-025. The store coordinates auto-flush of dirty
    // disk images on Eject / SwitchMachine / Shutdown / PowerCycle. Each
    // mounted disk's DiskImage is owned by the store; the slot 6 disk
    // controller sees it via DiskIIController::SetExternalDisk.
    DiskImageStore                m_diskStore;

    // Emulation state
    MachineConfig   m_config;

    // Machine config file name (without ".json" extension, e.g.,
    // "apple2e", "apple2plus", "apple2"). Used as a registry-key
    // suffix so per-machine UI state (e.g., last-mounted disks) can
    // round-trip between sessions without one machine's setting
    // clobbering another's.
    wstring         m_currentMachineName;

    // CPU-thread lifecycle, run/pause/step transitions, the UI -> CPU
    // command queue, and the paste buffer all live on CpuManager. The
    // shell wires its per-frame and per-command callbacks at startup
    // and otherwise reads the manager's transition state through the
    // IsRunning() / IsPaused() / GetSpeedMode() accessors.
    CpuManager         m_cpuManager;

    // Atomic flags (UI writes, CPU reads)
    atomic<ColorMode>  m_colorMode{ColorMode::Color};

    // Double framebuffer (CPU renders, UI presents, protected by m_fbMutex)
    mutex              m_fbMutex;
    vector<uint32_t>   m_cpuFramebuffer;
    vector<uint32_t>   m_textOverlay;
    vector<uint32_t>   m_uiFramebuffer;
    bool                    m_fbReady = false;

    uint32_t        m_cyclesPerFrame  = 17050;
    double          m_sampleRemainder = 0.0;

    // Spec-006 / FR-001 / FR-004a. Owned by the shell so the dialog
    // can be lazy-created on first Ctrl+Shift+D and reused across
    // opens. The uptime anchor lives on the shell (not the dialog)
    // so resets re-zero it even while the dialog is closed.
    std::unique_ptr<class DiskIIDebugDialog>  m_diskIIDebugDialog;
    std::chrono::steady_clock::time_point     m_uptimeAnchor { std::chrono::steady_clock::now() };

    // Extracted shell-side managers. WindowManager is stateless today
    // (per-monitor placement persistence still lives in the registry);
    // ClipboardManager holds references back to the shared CPU/UI
    // state it operates on plus a pointer-to-pointer for the active
    // keyboard so machine switches do not require re-wiring.
    WindowManager                             m_windowManager;
    std::unique_ptr<ClipboardManager>         m_clipboardManager;
};





