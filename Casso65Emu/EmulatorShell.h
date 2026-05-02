#pragma once

#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Core/MachineConfig.h"
#include "Core/ComponentRegistry.h"
#include "D3DRenderer.h"
#include "MenuSystem.h"
#include "DebugConsole.h"
#include "Video/VideoOutput.h"
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

class EmulatorShell
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

    // Window procedure (static + instance)
    static LRESULT CALLBACK StaticWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void HandleCommand (WORD commandId);

    // Keyboard input from WM_KEYDOWN/WM_CHAR
    void OnKeyDown (WPARAM vk, LPARAM lParam);
    void OnKeyUp   (WPARAM vk, LPARAM lParam);
    void OnChar    (WPARAM ch);

    // State
    bool IsRunning () const { return m_running.load (memory_order_acquire); }
    bool IsPaused  () const { return m_paused.load (memory_order_acquire); }

    // Access bus for test wiring
    MemoryBus & GetBus () { return m_memoryBus; }

private:
    // CPU thread entry point and helpers
    void CpuThreadProc     ();
    void RunOneFrame       ();
    void ProcessCommands   ();
    void UpdateWindowTitle ();
    void SelectVideoMode   ();

    // Queue a command for the CPU thread
    void PostCommand (WORD id, const string & payload = "");

    HWND                m_hwnd       = nullptr;
    HINSTANCE           m_hInstance  = nullptr;
    HACCEL              m_accelTable = nullptr;

    MemoryBus           m_memoryBus;
    ComponentRegistry   m_registry;
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

    // Emulation state
    MachineConfig   m_config;

    // -- Threading --
    thread     m_cpuThread;

    // Atomic flags (UI writes, CPU reads)
    atomic<bool>       m_running{true};
    atomic<bool>       m_paused{false};
    atomic<SpeedMode>  m_speedMode{SpeedMode::Authentic};
    atomic<ColorMode>  m_colorMode{ColorMode::Color};

    // Command queue (UI → CPU, protected by m_cmdMutex)
    mutex                    m_cmdMutex;
    vector<EmulatorCommand>  m_commandQueue;

    // Double framebuffer (CPU renders, UI presents, protected by m_fbMutex)
    mutex              m_fbMutex;
    vector<uint32_t>   m_cpuFramebuffer;
    vector<uint32_t>   m_uiFramebuffer;
    bool                    m_fbReady = false;

    uint32_t        m_cyclesPerFrame  = 17050;
    double          m_sampleRemainder = 0.0;
};





