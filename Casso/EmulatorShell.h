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
#include "Video/VideoOutput.h"
#include "Video/CharacterRomData.h"
#include "WasapiAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommand
//
//  Commands queued from the UI thread for the CPU thread to execute.
//
////////////////////////////////////////////////////////////////////////////////

struct EmulatorCommand
{
    WORD  id;
    string payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell : public Window
{
public:
    EmulatorShell ();
    ~EmulatorShell ();

    HRESULT Initialize (
        HINSTANCE              hInstance,
        const MachineConfig  & config,
        const string    & disk1Path,
        const string    & disk2Path);

    int RunMessageLoop ();

    void HandleCommand (WORD commandId);

    // State
    bool IsRunning () const { return m_running.load (memory_order_acquire); }
    bool IsPaused  () const { return m_paused.load (memory_order_acquire); }

    // Access bus for test wiring
    MemoryBus & GetBus () { return m_memoryBus; }

private:
    // Window message handler overrides
    bool    OnChar    (WPARAM ch, LPARAM lParam) override;
    bool    OnCommand (HWND hwnd, int id) override;
    LRESULT OnCreate  (HWND hwnd, CREATESTRUCT * pcs) override;
    bool    OnDestroy (HWND hwnd) override;
    bool    OnKeyDown (WPARAM vk, LPARAM lParam) override;
    bool    OnKeyUp   (WPARAM vk, LPARAM lParam) override;
    bool    OnNotify  (HWND hwnd, WPARAM wParam, LPARAM lParam) override;
    bool    OnSize    (HWND hwnd, UINT width, UINT height) override;

    // Command group handlers
    void OnFileCommand    (int id);
    void OnEditCommand    (int id);
    void OnMachineCommand (int id);
    void OnViewCommand    (int id);
    void OnDiskCommand    (int id);
    void OnHelpCommand    (int id);

    // CPU thread entry point and helpers
    void CpuThreadProc     ();
    void RunOneFrame       ();
    void ExecuteCpuSlices   ();
    void RenderFramebuffer  ();
    void ProcessCommands   ();
    void UpdateWindowTitle ();
    void SelectVideoMode   ();

    // Initialization helpers
    HRESULT CreateEmulatorWindow   (HINSTANCE hInstance);
    HRESULT CreateMemoryDevices    (const MachineConfig & config);
    void    WireLanguageCard       ();
    void    WirePageTable          ();
    void    RebuildBankingPages    ();
    void    MountCommandLineDisks  (const string & disk1Path, const string & disk2Path);
    void    CreateVideoModes       ();
    HRESULT CreateCpu              (const MachineConfig & config);

    Byte * GetAuxRamBuffer ();

    // Machine switching
    void    ShowMachinePicker      ();
    HRESULT SwitchMachine          (const wstring & machineName);

    void CopyScreenText     ();
    void CopyScreenshot     ();
    void PasteFromClipboard ();
    void DrainPasteBuffer   ();

    // Status bar
    void    CreateStatusBar        ();
    void    UpdateStatusBar        ();
    void    ShowDevicePopup        ();

    // Queue a command for the CPU thread
    void PostCommand (WORD id, const string & payload = "");

    HACCEL              m_accelTable = nullptr;
    HWND                m_statusBar  = nullptr;
    HWND                m_renderHwnd = nullptr;

    MemoryBus           m_memoryBus;
    ComponentRegistry   m_registry;
    InterruptController m_interruptController;
    unique_ptr<EmuCpu> m_cpu;

    D3DRenderer         m_d3dRenderer;
    MenuSystem          m_menuSystem;
    WasapiAudio         m_wasapiAudio;
    DebugConsole        m_debugConsole;

    // Owned devices
    vector<unique_ptr<MemoryDevice>> m_ownedDevices;

    // Video
    vector<unique_ptr<VideoOutput>> m_videoModes;
    VideoOutput *       m_activeVideoMode = nullptr;
    CharacterRomData    m_charRom;

    // Soft switch state (read by video mode selection)
    bool    m_graphicsMode = false;
    bool    m_mixedMode    = false;
    bool    m_page2        = false;
    bool    m_hiresMode    = false;
    bool    m_col80Mode    = false;
    bool    m_doubleHiRes  = false;

    // Device pointers (non-owning, for quick access)
    class AppleKeyboard *         m_keyboard     = nullptr;
    class AppleSoftSwitchBank *   m_softSwitches = nullptr;
    class AppleSpeaker *          m_speaker      = nullptr;
    class RamDevice *             m_mainRamDev   = nullptr;

    unique_ptr<class AppleIIeMmu> m_mmu;

    // Emulation state
    MachineConfig   m_config;

    // -- Threading --
    thread     m_cpuThread;

    // Pause synchronization
    mutex              m_pauseMutex;
    condition_variable m_pauseCV;

    // Atomic flags (UI writes, CPU reads)
    atomic<bool>       m_running{true};
    atomic<bool>       m_paused{false};
    atomic<SpeedMode>  m_speedMode{SpeedMode::Authentic};
    atomic<ColorMode>  m_colorMode{ColorMode::Color};

    // Command queue (UI → CPU, protected by m_cmdMutex)
    mutex                    m_cmdMutex;
    vector<EmulatorCommand>  m_commandQueue;

    // Paste queue (UI -> CPU, protected by m_cmdMutex)
    string             m_pasteBuffer;

    // Double framebuffer (CPU renders, UI presents, protected by m_fbMutex)
    mutex              m_fbMutex;
    vector<uint32_t>   m_cpuFramebuffer;
    vector<uint32_t>   m_textOverlay;
    vector<uint32_t>   m_uiFramebuffer;
    bool                    m_fbReady = false;

    uint32_t        m_cyclesPerFrame  = 17050;
    double          m_sampleRemainder = 0.0;
};





