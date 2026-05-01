#include "Pch.h"

#include "EmulatorShell.h"
#include "Core/PathResolver.h"
#include "Resources/resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/AppleSoftSwitchBank.h"
#include "Devices/AppleSpeaker.h"
#include "Devices/DiskIIController.h"
#include "Devices/LanguageCard.h"
#include "Video/AppleTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"

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
    const std::string & disk1Path,
    const std::string & disk2Path)
{
    HRESULT hr = S_OK;

    m_hInstance= hInstance;
    m_config    = config;

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

        ATOM regResult = RegisterClassEx (&wc);
        CWRA (regResult);
    }

    // Calculate window size for desired client area
    {
        RECT rc = { 0, 0, kFramebufferWidth, kFramebufferHeight };
        DWORD style = WS_OVERLAPPEDWINDOW;
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
        hr = m_menuSystem.CreateMenuBar (m_hwnd);
        CHR (hr);

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
            std::string romPath = region.resolvedPath;
            std::string error;

            auto device = RomDevice::CreateFromFile (region.start, region.end, romPath, error);

            {
                bool romOk = (device != nullptr);

                if (!romOk)
                {
                    std::wstring wideError (error.begin (), error.end ());
                    CBRN (false, wideError.c_str ());
                }
            }

            m_memoryBus.AddDevice (device.get ());
            m_ownedDevices.push_back (std::move (device));
        }
    }

    // Create named devices from config
    for (const auto & devConfig : config.devices)
    {
        auto device = m_registry.Create (devConfig.type, devConfig, m_memoryBus);

        if (device)
        {
            // Track specific device pointers for quick access
            if (devConfig.type == "apple2-keyboard" ||
                devConfig.type == "apple2e-keyboard")
            {
                m_keyboard = static_cast<AppleKeyboard *> (device.get ());
            }
            else if (devConfig.type == "apple2-softswitches" ||
                     devConfig.type == "apple2e-softswitches")
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

    // Wire Language Card bank switching if a language card is present
    {
        LanguageCard * lc = nullptr;

        for (auto & dev : m_ownedDevices)
        {
            if (lc == nullptr)
            {
                lc = dynamic_cast<LanguageCard *> (dev.get ());
            }
        }

        if (lc != nullptr)
        {
            // Find a ROM device covering $D000-$FFFF
            RomDevice * romDevice = nullptr;

            for (const auto & entry : m_memoryBus.GetEntries ())
            {
                auto * rom = dynamic_cast<RomDevice *> (entry.device);

                if (rom != nullptr && entry.start <= 0xD000 && entry.end >= 0xFFFF)
                {
                    romDevice = rom;
                    break;
                }
            }

            if (romDevice != nullptr)
            {
                Word romStart = romDevice->GetStart ();

                // Copy $D000-$FFFF ROM data to language card
                std::vector<Byte> lcRomData (0x3000);

                for (size_t i = 0; i < 0x3000; i++)
                {
                    lcRomData[i] = romDevice->Read (static_cast<Word> (0xD000 + i));
                }

                lc->SetRomData (lcRomData);
                m_memoryBus.RemoveDevice (romDevice);

                // Re-add lower ROM ($C100-$CFFF) if original extended below $D000
                if (romStart < 0xD000)
                {
                    size_t lowerSize = 0xD000 - romStart;
                    std::vector<Byte> lowerData (lowerSize);

                    for (size_t i = 0; i < lowerSize; i++)
                    {
                        lowerData[i] = romDevice->Read (static_cast<Word> (romStart + i));
                    }

                    auto lowerRom = RomDevice::CreateFromData (
                        romStart, static_cast<Word> (0xCFFF),
                        lowerData.data (), lowerData.size ());

                    m_memoryBus.AddDevice (lowerRom.get ());
                    m_ownedDevices.push_back (std::move (lowerRom));
                }
            }

            // Bank device intercepts $D000-$FFFF, routing to LC RAM or ROM
            auto lcBank = std::make_unique<LanguageCardBank> (*lc);
            m_memoryBus.AddDevice (lcBank.get ());
            m_ownedDevices.push_back (std::move (lcBank));
        }
    }

    // Mount command-line disk images if a Disk II controller was created
    if (!disk1Path.empty () || !disk2Path.empty ())
    {
        for (auto & dev : m_ownedDevices)
        {
            auto * diskCtrl = dynamic_cast<DiskIIController *> (dev.get ());

            if (diskCtrl)
            {
                if (!disk1Path.empty ())
                {
                    diskCtrl->MountDisk (0, disk1Path);
                }

                if (!disk2Path.empty ())
                {
                    diskCtrl->MountDisk (1, disk2Path);
                }

                break;
            }
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

        auto doubleHiResMode = std::make_unique<AppleDoubleHiResMode> (m_memoryBus);
        m_videoModes.push_back (std::move (doubleHiResMode));
    }

    // Validate memory bus for overlapping device address ranges
    hr = m_memoryBus.Validate ();
    CHRN (hr, L"Memory bus validation failed: overlapping device address ranges");

    // Create CPU
    m_cpu = std::make_unique<EmuCpu> (m_memoryBus);

    // The base Cpu class uses an internal memory[] array for opcode fetch
    // and instruction execution. We must copy ROM data into that array so
    // the CPU can execute it. The MemoryBus is used for I/O devices and
    // video RAM reads, but the CPU's execution path reads from memory[].
    for (const auto & region : config.memoryRegions)
    {
        if (region.type == "rom" && !region.resolvedPath.empty ())
        {
            std::ifstream romFile (region.resolvedPath, std::ios::binary);

            if (romFile.good ())
            {
                Word addr = region.start;

                while (romFile.good () && addr <= region.end)
                {
                    char byte;
                    romFile.read (&byte, 1);

                    if (romFile.gcount () == 1)
                    {
                        m_cpu->PokeByte (addr, static_cast<Byte> (byte));
                        addr++;
                    }
                }
            }
        }
    }

    m_cpu->InitForEmulation ();

    // Connect speaker to CPU cycle counter for audio timestamps
    if (m_speaker != nullptr)
    {
        m_speaker->SetCycleCounter (m_cpu->GetCycleCounterPtr ());
    }

    // Initialize D3D11
    hr = m_d3dRenderer.Initialize (m_hwnd, kFramebufferWidth, kFramebufferHeight);
    CHR (hr);

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

    // Create a high-resolution waitable timer for frame pacing (~60fps)
    HANDLE hTimer = CreateWaitableTimerEx (nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    if (hTimer == nullptr)
    {
        // Fallback to standard timer if high-res not available
        hTimer = CreateWaitableTimer (nullptr, FALSE, nullptr);
    }

    if (hTimer != nullptr)
    {
        // Set periodic timer: 16.67ms (~60fps) in 100-nanosecond intervals
        LARGE_INTEGER dueTime = {};
        dueTime.QuadPart = -166667;  // Negative = relative, 16.6667ms
        SetWaitableTimer (hTimer, &dueTime, 16, nullptr, nullptr, FALSE);
    }

    while (m_running)
    {
        // Wait for either the frame timer or a Windows message
        DWORD waitResult = WAIT_TIMEOUT;

        if (hTimer != nullptr)
        {
            waitResult = MsgWaitForMultipleObjects (
                1, &hTimer, FALSE,
                m_paused ? INFINITE : 100,
                QS_ALLINPUT);
        }
        else
        {
            waitResult = MsgWaitForMultipleObjects (
                0, nullptr, FALSE, 16, QS_ALLINPUT);
        }

        // Process all pending messages
        while (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_running = false;

                if (hTimer != nullptr)
                {
                    CloseHandle (hTimer);
                }

                return static_cast<int> (msg.wParam);
            }

            if (m_accelTable == nullptr ||
                !TranslateAccelerator (m_hwnd, m_accelTable, &msg))
            {
                TranslateMessage (&msg);
                DispatchMessage (&msg);
            }
        }

        // Run a frame when the timer fires (not when paused)
        if (!m_paused && waitResult == WAIT_OBJECT_0)
        {
            RunOneFrame ();
        }
    }

    if (hTimer != nullptr)
    {
        CloseHandle (hTimer);
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
    // Start speaker frame timing so timestamps are frame-relative
    if (m_speaker != nullptr)
    {
        m_speaker->BeginFrame ();
    }

    // Execute CPU cycles for one frame
    uint32_t targetCycles = m_cyclesPerFrame;

    if (m_speedMode == SpeedMode::Double)
    {
        targetCycles *= 2;
    }

    // Cycle counts per opcode (NMOS 6502 base cycles, no page-crossing penalty)
    static const Byte kCycleCounts[256] =
    {
        7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,0, // 00-0F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, // 10-1F
        6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0, // 20-2F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, // 30-3F
        6,6,0,0,0,3,5,0,3,2,2,0,3,4,6,0, // 40-4F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, // 50-5F
        6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0, // 60-6F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, // 70-7F
        0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0, // 80-8F
        2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0, // 90-9F
        2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0, // A0-AF
        2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0, // B0-BF
        2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, // C0-CF
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, // D0-DF
        2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, // E0-EF
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0  // F0-FF
    };

    for (uint32_t i = 0; i < targetCycles; )
    {
        Byte opcode = m_cpu->PeekByte (m_cpu->GetPC ());
        Byte cycles = kCycleCounts[opcode];

        if (cycles == 0)
        {
            cycles = 2;  // Illegal opcode fallback
        }

        m_cpu->StepOne ();
        m_cpu->AddCycles (cycles);
        i += cycles;
    }

    // Select video mode based on soft switch state
    SelectVideoMode ();

    // Render video — pass CPU memory[] for video RAM reads
    // (CpuOperations::Store writes to memory[] directly, not through the bus)
    if (m_activeVideoMode != nullptr)
    {
        m_activeVideoMode->Render (
            m_cpu->GetMemory (),
            m_framebuffer.data (),
            kFramebufferWidth,
            kFramebufferHeight);
    }

    // Mixed mode: overlay text on the bottom 4 rows (rows 20-23)
    if (m_mixedMode && m_graphicsMode && !m_videoModes.empty ())
    {
        static constexpr int kMixedCharH  = 8;
        static constexpr int kMixedScaleY = 2;
        int mixedFbY = 20 * kMixedCharH * kMixedScaleY;

        std::vector<uint32_t> textBuf (static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight, 0);

        m_videoModes[0]->Render (
            m_cpu->GetMemory (),
            textBuf.data (),
            kFramebufferWidth,
            kFramebufferHeight);

        size_t rowBytes = static_cast<size_t> (kFramebufferWidth) * sizeof (uint32_t);

        for (int y = mixedFbY; y < kFramebufferHeight; y++)
        {
            memcpy (&m_framebuffer[static_cast<size_t> (y) * kFramebufferWidth],
                    &textBuf[static_cast<size_t> (y) * kFramebufferWidth],
                    rowBytes);
        }
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
            m_speaker->GetFrameInitialState ());

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

        case WM_SIZE:
        {
            if (wParam != SIZE_MINIMIZED)
            {
                int width  = LOWORD (lParam);
                int height = HIWORD (lParam);
                m_d3dRenderer.Resize (width, height);
            }
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
            // TODO: Implement hot-swap machine config loading.
            // Requires: tear down current CPU/bus/devices, parse new JSON,
            // rebuild everything, re-initialize. Menu item is grayed out
            // until this is implemented.
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

        case IDM_VIEW_RESET_SIZE:
        {
            if (!m_d3dRenderer.IsFullscreen ())
            {
                RECT rc = { 0, 0, kFramebufferWidth, kFramebufferHeight };
                DWORD style = static_cast<DWORD> (GetWindowLong (m_hwnd, GWL_STYLE));
                AdjustWindowRect (&rc, style, TRUE);

                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                // Center on the current monitor
                HMONITOR hMon = MonitorFromWindow (m_hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof (mi) };
                GetMonitorInfo (hMon, &mi);

                int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
                int y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

                SetWindowPos (m_hwnd, nullptr, x, y, w, h, SWP_NOZORDER);
            }
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
                std::string narrowPath = PathResolver::WideToNarrow (std::wstring (filePath));
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
                m_debugConsole.Show (m_hInstance);
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
                L"Arrow Keys -> Apple II cursor movement\n"
                L"Enter -> Return\n"
                L"Escape -> Escape\n"
                L"Delete -> Delete\n"
                L"Ctrl+Reset -> Warm Reset\n"
                L"Left Alt -> Open Apple (IIe)\n"
                L"Right Alt -> Closed Apple (IIe)\n\n"
                L"Emulator Controls:\n"
                L"Ctrl+R -> Reset\n"
                L"Ctrl+Shift+R -> Power Cycle\n"
                L"Pause -> Pause/Resume\n"
                L"F11 -> Step (when paused)\n"
                L"Alt+Enter -> Fullscreen\n"
                L"Ctrl+0 -> Reset Window Size\n"
                L"Ctrl+D -> Debug Console",
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

    // Pass page2 state to the active renderer
    if (m_activeVideoMode != nullptr)
    {
        m_activeVideoMode->SetPage2 (m_page2);
    }

    // Keep text mode page2-aware for mixed-mode overlay rendering
    m_videoModes[0]->SetPage2 (m_page2);
}
