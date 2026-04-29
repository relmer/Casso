#include "Pch.h"

#include "EmulatorShell.h"
#include "Resources/resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/AppleSoftSwitchBank.h"
#include "Devices/AppleSpeaker.h"
#include "Devices/DiskIIController.h"
#include "Video/AppleTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"

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
      m_keyboard        (nullptr),
      m_softSwitches    (nullptr),
      m_speaker         (nullptr),
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
            // Track specific device pointers for quick access
            if (devConfig.type == "apple2-keyboard")
            {
                m_keyboard = static_cast<AppleKeyboard *> (device.get ());
            }
            else if (devConfig.type == "apple2-softswitches")
            {
                m_softSwitches = static_cast<AppleSoftSwitchBank *> (device.get ());
            }
            else if (devConfig.type == "apple2-speaker")
            {
                m_speaker = static_cast<AppleSpeaker *> (device.get ());
            }

            m_memoryBus.AddDevice (device.get ());
            m_ownedDevices.push_back (std::move (device));
        }
        else
        {
            DEBUGMSG (L"Warning: Unknown device type '%hs'\n", devConfig.type.c_str ());
        }
    }

    // Create video modes
    {
        auto textMode = std::make_unique<AppleTextMode> (m_memoryBus);
        m_activeVideoMode = textMode.get ();
        m_videoModes.push_back (std::move (textMode));

        auto loResMode = std::make_unique<AppleLoResMode> (m_memoryBus);
        m_videoModes.push_back (std::move (loResMode));

        auto hiResMode = std::make_unique<AppleHiResMode> (m_memoryBus);
        m_videoModes.push_back (std::move (hiResMode));
    }

    // Create CPU
    m_cpu = std::make_unique<EmuCpu> (m_memoryBus);
    m_cpu->InitForEmulation ();

    // Initialize D3D11
    CHR (m_d3dRenderer.Initialize (m_hwnd, kFramebufferWidth, kFramebufferHeight));

    // Initialize WASAPI audio (non-fatal if it fails)
    m_wasapiAudio.Initialize ();

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

    // Submit audio
    if (m_speaker && m_wasapiAudio.IsInitialized ())
    {
        m_wasapiAudio.SubmitFrame (
            m_speaker->GetToggleTimestamps (),
            targetCycles,
            m_speaker->GetSpeakerState ());

        m_speaker->ClearTimestamps ();
    }

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
        case IDM_FILE_OPEN:
        {
            // Open Machine Config dialog
            WCHAR filePath[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof (ofn);
            ofn.hwndOwner   = m_hwnd;
            ofn.lpstrFilter = L"JSON Config Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile   = filePath;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrTitle  = L"Open Machine Config";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW (&ofn))
            {
                // TODO: Tear down current machine and load new config
                MessageBoxW (m_hwnd, filePath, L"Selected Config", MB_OK);
            }
            break;
        }

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

        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
        {
            WCHAR filePath[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof (ofn);
            ofn.hwndOwner   = m_hwnd;
            ofn.lpstrFilter = L"Disk Images (*.dsk)\0*.dsk\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile   = filePath;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrTitle  = (commandId == IDM_DISK_INSERT1) ?
                L"Insert Disk in Drive 1" : L"Insert Disk in Drive 2";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW (&ofn))
            {
                // Convert wide path to narrow for DiskIIController
                std::wstring widePath (filePath);
                std::string narrowPath (widePath.begin (), widePath.end ());
                int drive = (commandId == IDM_DISK_INSERT1) ? 0 : 1;

                // Find disk controller and mount
                for (auto & dev : m_ownedDevices)
                {
                    auto * diskCtrl = dynamic_cast<DiskIIController *> (dev.get ());

                    if (diskCtrl)
                    {
                        diskCtrl->MountDisk (drive, narrowPath);
                        break;
                    }
                }
            }
            break;
        }

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
        {
            int drive = (commandId == IDM_DISK_EJECT1) ? 0 : 1;

            for (auto & dev : m_ownedDevices)
            {
                auto * diskCtrl = dynamic_cast<DiskIIController *> (dev.get ());

                if (diskCtrl)
                {
                    diskCtrl->EjectDisk (drive);
                    break;
                }
            }
            break;
        }

        case IDM_HELP_DEBUG:
        {
            if (m_debugConsole.IsVisible ())
            {
                m_debugConsole.Hide ();
            }
            else
            {
                m_debugConsole.Show ();
                m_debugConsole.LogConfig (
                    std::format ("Machine: {}\nCPU: {}\nClock: {} Hz\nDevices: {}",
                        m_config.name, m_config.cpu, m_config.clockSpeed,
                        m_config.devices.size ()));
            }
            break;
        }

        case IDM_HELP_KEYMAP:
        {
            MessageBoxW (m_hwnd,
                L"PC Key Mapping:\n\n"
                L"Arrow Keys → Apple II cursor movement\n"
                L"Enter → Return\n"
                L"Escape → Escape\n"
                L"Delete → Delete\n"
                L"Ctrl+Reset → Warm Reset\n"
                L"Left Alt → Open Apple (IIe)\n"
                L"Right Alt → Closed Apple (IIe)\n\n"
                L"Emulator Controls:\n"
                L"Ctrl+R → Reset\n"
                L"Ctrl+Shift+R → Power Cycle\n"
                L"Pause → Pause/Resume\n"
                L"F11 → Step (when paused)\n"
                L"Alt+Enter → Fullscreen\n"
                L"Ctrl+D → Debug Console",
                L"Keyboard Map", MB_ICONINFORMATION | MB_OK);
            break;
        }

        case IDM_HELP_ABOUT:
        {
            MessageBoxW (m_hwnd,
                L"Casso65 Apple II Emulator\n"
                L"Version 0.9.0\n\n"
                L"An Apple II platform emulator built on the\n"
                L"Casso65 6502 assembler/emulator project.\n\n"
                L"https://github.com/relmer/Casso65",
                L"About Casso65", MB_ICONINFORMATION | MB_OK);
            break;
        }

        case IDM_MACHINE_INFO:
        {
            std::wstring info = std::format (
                L"Machine: {}\n"
                L"CPU: {}\n"
                L"Clock Speed: {} Hz\n"
                L"Memory Regions: {}\n"
                L"Devices: {}",
                std::wstring (m_config.name.begin (), m_config.name.end ()),
                std::wstring (m_config.cpu.begin (), m_config.cpu.end ()),
                m_config.clockSpeed,
                m_config.memoryRegions.size (),
                m_config.devices.size ());

            MessageBoxW (m_hwnd, info.c_str (), L"Machine Info", MB_ICONINFORMATION | MB_OK);
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
    UNREFERENCED_PARAMETER (lParam);

    if (m_keyboard == nullptr)
    {
        return;
    }

    m_keyboard->SetKeyDown (true);

    // Handle special keys that don't generate WM_CHAR
    switch (vk)
    {
        case VK_LEFT:
            m_keyboard->KeyPress (0x08);    // Backspace
            break;
        case VK_RIGHT:
            m_keyboard->KeyPress (0x15);    // Forward
            break;
        case VK_UP:
            m_keyboard->KeyPress (0x0B);    // Up
            break;
        case VK_DOWN:
            m_keyboard->KeyPress (0x0A);    // Down
            break;
        case VK_ESCAPE:
            m_keyboard->KeyPress (0x1B);    // Escape
            break;
        case VK_DELETE:
            m_keyboard->KeyPress (0x7F);    // Delete
            break;
        default:
            break;
    }
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

    if (m_keyboard)
    {
        m_keyboard->SetKeyDown (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnChar (WPARAM ch)
{
    if (m_keyboard == nullptr)
    {
        return;
    }

    // WM_CHAR provides the ASCII character including Ctrl+key combos
    if (ch >= 1 && ch <= 127)
    {
        m_keyboard->KeyPress (static_cast<Byte> (ch));
    }
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
    if (m_videoModes.size () < 3)
    {
        return;
    }

    // Read soft switch state
    if (m_softSwitches)
    {
        m_graphicsMode = m_softSwitches->IsGraphicsMode ();
        m_mixedMode    = m_softSwitches->IsMixedMode ();
        m_page2        = m_softSwitches->IsPage2 ();
        m_hiresMode    = m_softSwitches->IsHiresMode ();
    }

    // Select video mode based on soft switch state
    if (!m_graphicsMode)
    {
        // Text mode
        m_activeVideoMode = m_videoModes[0].get ();
    }
    else if (!m_hiresMode)
    {
        // Lo-res graphics
        m_activeVideoMode = m_videoModes[1].get ();
    }
    else
    {
        // Hi-res graphics
        m_activeVideoMode = m_videoModes[2].get ();
    }
}
