#pragma once

#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Core/MachineConfig.h"
#include "Core/ComponentRegistry.h"
#include "Shell/D3DRenderer.h"
#include "Shell/MenuSystem.h"
#include "Video/VideoOutput.h"





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

    HRESULT Initialize (HINSTANCE hInstance, const MachineConfig & config,
                        const std::string & basePath,
                        const std::string & disk1Path,
                        const std::string & disk2Path);

    int RunMessageLoop ();

    // Window procedure (static + instance)
    static LRESULT CALLBACK StaticWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void HandleCommand (WORD commandId);

    // Keyboard input from WM_KEYDOWN/WM_CHAR
    void OnKeyDown (WPARAM vk, LPARAM lParam);
    void OnKeyUp   (WPARAM vk, LPARAM lParam);
    void OnChar    (WPARAM ch);

    // Frame execution
    void RunOneFrame ();

    // State
    bool IsRunning () const { return m_running; }
    bool IsPaused  () const { return m_paused; }

    // Access bus for test wiring
    MemoryBus & GetBus () { return m_memoryBus; }

private:
    void UpdateWindowTitle ();
    void SelectVideoMode   ();

    HWND                m_hwnd;
    HINSTANCE           m_hInstance;
    HACCEL              m_accelTable;

    MemoryBus           m_memoryBus;
    ComponentRegistry   m_registry;
    std::unique_ptr<EmuCpu> m_cpu;

    D3DRenderer         m_d3dRenderer;
    MenuSystem          m_menuSystem;

    // Owned devices
    std::vector<std::unique_ptr<MemoryDevice>> m_ownedDevices;

    // Video
    std::vector<std::unique_ptr<VideoOutput>> m_videoModes;
    VideoOutput *       m_activeVideoMode;
    std::vector<uint32_t> m_framebuffer;

    // Soft switch state (read by video mode selection)
    bool    m_graphicsMode;
    bool    m_mixedMode;
    bool    m_page2;
    bool    m_hiresMode;
    bool    m_col80Mode;
    bool    m_doubleHiRes;

    // Device pointers (non-owning, for quick access)
    class AppleKeyboard *         m_keyboard;
    class AppleSoftSwitchBank *   m_softSwitches;

    // Emulation state
    MachineConfig   m_config;
    bool            m_running;
    bool            m_paused;
    SpeedMode       m_speedMode;
    ColorMode       m_colorMode;

    std::string     m_basePath;

    // Frame timing
    LARGE_INTEGER   m_perfFreq;
    LARGE_INTEGER   m_lastFrameTime;
    uint32_t        m_cyclesPerFrame;
};
