#include "Pch.h"

#include "EmulatorShell.h"
#include "Resources/resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int    kFramebufferWidth  = 560;
static constexpr int    kFramebufferHeight = 384;
static constexpr LPCWSTR kWindowClass      = L"Casso65EmuWindow";





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::EmulatorShell ()
    : m_hwnd            (nullptr),
      m_hInstance       (nullptr),
      m_accelTable      (nullptr),
      m_activeVideoMode (nullptr),
      m_graphicsMode    (false),
      m_mixedMode       (false),
      m_page2           (false),
      m_hiresMode       (false),
      m_col80Mode       (false),
      m_doubleHiRes     (false),
      m_running         (true),
      m_paused          (false),
      m_speedMode       (SpeedMode::Authentic),
      m_colorMode       (ColorMode::Color),
      m_cyclesPerFrame  (17050)
{
    m_perfFreq.QuadPart      = 0;
    m_lastFrameTime.QuadPart = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::~EmulatorShell ()
{
    m_d3dRenderer.Shutdown ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Initialize (
    HINSTANCE hInstance,
    const MachineConfig & config,
    const std::string & basePath,
    const std::string & disk1Path,
    const std::string & disk2Path)
{
    HRESULT hr = S_OK;

    UNREFERENCED_PARAMETER (disk1Path);
    UNREFERENCED_PARAMETER (disk2Path);

    m_hInstance = hInstance;
    m_config    = config;
    m_basePath  = basePath;

    // Register built-in device factories
    ComponentRegistry::RegisterBuiltinDevices (m_registry);

    // Calculate cycles per frame
    m_cyclesPerFrame = config.clockSpeed / 60;

    // Initialize performance counter
    QueryPerformanceFrequency (&m_perfFreq);
    QueryPerformanceCounter (&m_lastFrameTime);

    // Create framebuffer
    m_framebuffer.resize (static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight, 0);

    // Register window class
    {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof (WNDCLASSEX);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = StaticWndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor (nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH> (COLOR_WINDOW + 1);
        wc.lpszClassName = kWindowClass;
        wc.hIcon         = LoadIcon (nullptr, IDI_APPLICATION);

        CWRA (RegisterClassEx (&wc));
    }

    // Calculate window size for desired client area
    {
        RECT rc = { 0, 0, kFramebufferWidth, kFramebufferHeight };
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRect (&rc, style, TRUE);  // TRUE = has menu

        // Create window
        m_hwnd = CreateWindowEx (
            0,
            kWindowClass,
            L"Casso65",
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance,
            this);

        CPRA (m_hwnd);

        // Create menu bar
        CHR (m_menuSystem.CreateMenuBar (m_hwnd));

        // Recalculate window size after menu added
        AdjustWindowRect (&rc, style, TRUE);
        SetWindowPos (m_hwnd, nullptr, 0, 0,
            rc.right - rc.left, rc.bottom - rc.top,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    // Load accelerator table
    m_accelTable = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR));

    // Create memory devices from config
    for (const auto & region : config.memoryRegions)
    {
        if (region.type == "ram")
        {
            auto device = std::make_unique<RamDevice> (region.start, region.end);
            m_memoryBus.AddDevice (device.get ());
            m_ownedDevices.push_back (std::move (device));
        }
        else if (region.type == "rom" && !region.file.empty ())
        {
            std::string romPath = basePath + "/roms/" + region.file;
            std::string error;

            auto device = RomDevice::CreateFromFile (region.start, region.end, romPath, error);

            if (device)
            {
                m_memoryBus.AddDevice (device.get ());
                m_ownedDevices.push_back (std::move (device));
            }
            else
            {
                DEBUGMSG (L"Warning: %hs\n", error.c_str ());
            }
        }
    }

    // Create named devices from config
    for (const auto & devConfig : config.devices)
    {
        auto device = m_registry.Create (devConfig.type, devConfig, m_memoryBus);

        if (device)
        {
            m_memoryBus.AddDevice (device.get ());
            m_ownedDevices.push_back (std::move (device));
        }
        else
        {
            DEBUGMSG (L"Warning: Unknown device type '%hs'\n", devConfig.type.c_str ());
        }
    }

    // Create CPU
    m_cpu = std::make_unique<EmuCpu> (m_memoryBus);
    m_cpu->InitForEmulation ();

    // Initialize D3D11
    CHR (m_d3dRenderer.Initialize (m_hwnd, kFramebufferWidth, kFramebufferHeight));

    // Show window
    ShowWindow (m_hwnd, SW_SHOW);
    UpdateWindow (m_hwnd);

    UpdateWindowTitle ();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunMessageLoop
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::RunMessageLoop ()
{
    MSG msg = {};

    while (m_running)
    {
        while (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_running = false;
                return static_cast<int> (msg.wParam);
            }

            if (m_accelTable == nullptr ||
                !TranslateAccelerator (m_hwnd, m_accelTable, &msg))
            {
                TranslateMessage (&msg);
                DispatchMessage (&msg);
            }
        }

        if (!m_paused)
        {
            RunOneFrame ();
        }
        else
        {
            // When paused, sleep to avoid busy-waiting
            Sleep (16);
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunOneFrame
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunOneFrame ()
{
    // Execute CPU cycles for one frame
    uint32_t targetCycles = m_cyclesPerFrame;

    if (m_speedMode == SpeedMode::Double)
    {
        targetCycles *= 2;
    }

    for (uint32_t i = 0; i < targetCycles; i++)
    {
        m_cpu->StepOne ();
    }

    // Select video mode based on soft switch state
    SelectVideoMode ();

    // Render video
    if (m_activeVideoMode != nullptr)
    {
        // Read video RAM through the bus — pass a proxy pointer
        // For now, render with the framebuffer
        m_activeVideoMode->Render (
            nullptr,  // Video RAM read via bus
            m_framebuffer.data (),
            kFramebufferWidth,
            kFramebufferHeight);
    }

    // Apply monochrome tint if needed
    if (m_colorMode != ColorMode::Color)
    {
        for (auto & pixel : m_framebuffer)
        {
            Byte r = (pixel >>  0) & 0xFF;
            Byte g = (pixel >>  8) & 0xFF;
            Byte b = (pixel >> 16) & 0xFF;

            Byte lum = static_cast<Byte> (0.299f * r + 0.587f * g + 0.114f * b);

            switch (m_colorMode)
            {
                case ColorMode::GreenMono:
                    pixel = (0xFF << 24) | (0 << 16) | (lum << 8) | 0;
                    break;
                case ColorMode::AmberMono:
                    pixel = (0xFF << 24) | (0 << 16) | (static_cast<Byte> (lum * 0.75f) << 8) | lum;
                    break;
                case ColorMode::WhiteMono:
                    pixel = (0xFF << 24) | (lum << 16) | (lum << 8) | lum;
                    break;
                default:
                    break;
            }
        }
    }

    // Upload and present
    m_d3dRenderer.UploadAndPresent (m_framebuffer.data ());

    // Frame timing synchronization
    if (m_speedMode != SpeedMode::Maximum)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter (&now);

        double elapsed   = static_cast<double> (now.QuadPart - m_lastFrameTime.QuadPart) /
                           static_cast<double> (m_perfFreq.QuadPart);
        double frameTime = 1.0 / 60.0;

        if (m_speedMode == SpeedMode::Double)
        {
            frameTime = 1.0 / 120.0;
        }

        if (elapsed < frameTime)
        {
            DWORD sleepMs = static_cast<DWORD> ((frameTime - elapsed) * 1000.0);

            if (sleepMs > 0)
            {
                Sleep (sleepMs);
            }
        }

        QueryPerformanceCounter (&m_lastFrameTime);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StaticWndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK EmulatorShell::StaticWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EmulatorShell * self = nullptr;

    if (msg == WM_CREATE)
    {
        CREATESTRUCT * cs = reinterpret_cast<CREATESTRUCT *> (lParam);
        self = static_cast<EmulatorShell *> (cs->lpCreateParams);
        SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (self));
    }
    else
    {
        self = reinterpret_cast<EmulatorShell *> (GetWindowLongPtr (hwnd, GWLP_USERDATA));
    }

    if (self != nullptr)
    {
        return self->WndProc (hwnd, msg, wParam, lParam);
    }

    return DefWindowProc (hwnd, msg, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT EmulatorShell::WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_COMMAND:
        {
            HandleCommand (LOWORD (wParam));
            return 0;
        }

        case WM_KEYDOWN:
        {
            OnKeyDown (wParam, lParam);
            return 0;
        }

        case WM_KEYUP:
        {
            OnKeyUp (wParam, lParam);
            return 0;
        }

        case WM_CHAR:
        {
            OnChar (wParam);
            return 0;
        }

        case WM_DESTROY:
        {
            m_running = false;
            PostQuitMessage (0);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProc (hwnd, msg, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleCommand (WORD commandId)
{
    switch (commandId)
    {
        case IDM_FILE_EXIT:
        {
            DestroyWindow (m_hwnd);
            break;
        }

        case IDM_MACHINE_RESET:
        {
            // Warm reset — fetch reset vector, preserve RAM
            if (m_cpu)
            {
                Word resetVec = m_cpu->ReadWord (0xFFFC);
                m_cpu->SetPC (resetVec);
            }
            break;
        }

        case IDM_MACHINE_POWERCYCLE:
        {
            // Cold boot — clear RAM, reset all devices
            m_memoryBus.Reset ();

            if (m_cpu)
            {
                m_cpu->InitForEmulation ();
            }
            break;
        }

        case IDM_MACHINE_PAUSE:
        {
            m_paused = !m_paused;
            m_menuSystem.SetPaused (m_paused);
            UpdateWindowTitle ();
            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (m_paused && m_cpu)
            {
                m_cpu->StepOne ();
            }
            break;
        }

        case IDM_MACHINE_SPEED_1X:
        {
            m_speedMode = SpeedMode::Authentic;
            m_menuSystem.SetSpeedMode (m_speedMode);
            break;
        }

        case IDM_MACHINE_SPEED_2X:
        {
            m_speedMode = SpeedMode::Double;
            m_menuSystem.SetSpeedMode (m_speedMode);
            break;
        }

        case IDM_MACHINE_SPEED_MAX:
        {
            m_speedMode = SpeedMode::Maximum;
            m_menuSystem.SetSpeedMode (m_speedMode);
            break;
        }

        case IDM_VIEW_COLOR:
        {
            m_colorMode = ColorMode::Color;
            m_menuSystem.SetColorMode (m_colorMode);
            break;
        }

        case IDM_VIEW_GREEN:
        {
            m_colorMode = ColorMode::GreenMono;
            m_menuSystem.SetColorMode (m_colorMode);
            break;
        }

        case IDM_VIEW_AMBER:
        {
            m_colorMode = ColorMode::AmberMono;
            m_menuSystem.SetColorMode (m_colorMode);
            break;
        }

        case IDM_VIEW_WHITE:
        {
            m_colorMode = ColorMode::WhiteMono;
            m_menuSystem.SetColorMode (m_colorMode);
            break;
        }

        case IDM_VIEW_FULLSCREEN:
        {
            m_d3dRenderer.ToggleFullscreen (m_hwnd);
            break;
        }

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (vk);
    UNREFERENCED_PARAMETER (lParam);
    // Keyboard device integration added in Phase 3 (T031)
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyUp
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (vk);
    UNREFERENCED_PARAMETER (lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnChar (WPARAM ch)
{
    UNREFERENCED_PARAMETER (ch);
    // Keyboard device integration added in Phase 3 (T031)
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateWindowTitle ()
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    std::wstring title = L"Casso65";

    if (!m_config.name.empty ())
    {
        title += L" \u2014 ";

        // Convert machine name to wide string
        std::wstring wideName (m_config.name.begin (), m_config.name.end ());
        title += wideName;
    }

    if (m_paused)
    {
        title += L" [Paused]";
    }
    else if (m_running)
    {
        title += L" [Running]";
    }
    else
    {
        title += L" [Stopped]";
    }

    SetWindowText (m_hwnd, title.c_str ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectVideoMode
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SelectVideoMode ()
{
    // Default to first mode (text)
    if (!m_videoModes.empty () && m_activeVideoMode == nullptr)
    {
        m_activeVideoMode = m_videoModes[0].get ();
    }

    // Video mode selection based on soft switch state — implemented in Phase 3+
}
