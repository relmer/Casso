#include "Pch.h"

#include "EmulatorShell.h"
#include "Core/PathResolver.h"
#include "resource.h"
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

EmulatorShell::EmulatorShell()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::~EmulatorShell()
{
    m_running.store (false, memory_order_release);

    if (m_cpuThread.joinable())
    {
        m_cpuThread.join();
    }

    m_d3dRenderer.Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Initialize (
    HINSTANCE             hInstance,
    const MachineConfig & config,
    const string        & disk1Path,
    const string        & disk2Path)
{
    HRESULT hr     = S_OK;
    size_t  fbSize = 0;



    m_config         = config;
    m_cyclesPerFrame = config.cyclesPerFrame;

    // Register built-in device factories
    ComponentRegistry::RegisterBuiltinDevices (m_registry);

    // Create framebuffers (CPU renders to one, UI reads the other)
    fbSize = static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight;
    m_cpuFramebuffer.resize (fbSize, 0);
    m_uiFramebuffer.resize (fbSize, 0);

    hr = CreateEmulatorWindow (hInstance);
    CHR (hr);

    hr = CreateMemoryDevices (config);
    CHR (hr);

    WireLanguageCard();
    MountCommandLineDisks (disk1Path, disk2Path);
    CreateVideoModes();

    // Validate memory bus for overlapping device address ranges
    hr = m_memoryBus.Validate();
    CHR (hr);

    hr = CreateCpu (config);
    CHR (hr);

    // Initialize D3D11
    hr = m_d3dRenderer.Initialize (m_hwnd, kFramebufferWidth, kFramebufferHeight);
    CHR (hr);

    // WASAPI audio is initialized on the CPU thread (COM apartment requirement)

    // Show window
    ShowWindow (m_hwnd, SW_SHOW);
    UpdateWindow (m_hwnd);

    UpdateWindowTitle();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateEmulatorWindow
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::CreateEmulatorWindow (HINSTANCE hInstance)
{
    HRESULT hr      = S_OK;
    UINT    dpi     = 0;
    int     scale   = 0;
    int     clientW = 0;
    int     clientH = 0;
    RECT    rc      = {};
    DWORD   style   = 0;



    // Enable Per-Monitor V2 DPI awareness
    SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Register and create the window via the base class
    m_kpszWndClass  = kWindowClass;
    m_hbrBackground = reinterpret_cast<HBRUSH> (COLOR_WINDOW + 1);

    hr = Window::Initialize (hInstance);
    CHR (hr);

    // Calculate window size for desired client area, scaled for DPI
    dpi   = GetDpiForSystem();
    scale = (dpi + 48) / 96;  // 96=1x, 144=1.5x→2, 192=2x→2, 240=2.5x→3
    if (scale < 1)
    {
        scale = 1;
    }

    clientW = kFramebufferWidth * scale;
    clientH = kFramebufferHeight * scale;

    rc    = { 0, 0, clientW, clientH };
    style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect (&rc, style, TRUE);  // TRUE = has menu

    // Create window
    hr = Window::Create (0,
                         L"Casso65",
                         style,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rc.right - rc.left, rc.bottom - rc.top,
                         nullptr);
    CHR (hr);

    // Create menu bar
    hr = m_menuSystem.CreateMenuBar (m_hwnd);
    CHR (hr);

    // Recalculate window size after menu added
    AdjustWindowRect (&rc, style, TRUE);
    SetWindowPos (m_hwnd, nullptr, 0, 0,
                  rc.right - rc.left, rc.bottom - rc.top,
                  SWP_NOMOVE | SWP_NOZORDER);

    // Load accelerator table
    m_accelTable = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateMemoryDevices
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::CreateMemoryDevices (const MachineConfig & config)
{
    HRESULT hr    = S_OK;
    bool    romOk = false;

    wstring wideError;



    // Create memory-mapped regions (RAM and ROM)
    for (const auto & region : config.memoryRegions)
    {
        if (region.type == "ram")
        {
            auto device = make_unique<RamDevice> (region.start, region.end);
            m_memoryBus.AddDevice (device.get());
            m_ownedDevices.push_back (move (device));
        }
        else if (region.type == "rom" && !region.file.empty())
        {
            string romPath = region.resolvedPath;
            string error;

            auto device = RomDevice::CreateFromFile (region.start, region.end, romPath, error);

            romOk = (device != nullptr);

            if (!romOk)
            {
                wideError.assign (error.begin(), error.end());
                CBRN (false, wideError.c_str());
            }

            m_memoryBus.AddDevice (device.get());
            m_ownedDevices.push_back (move (device));
        }
    }

    // Create named devices from config (keyboard, speaker, soft switches, etc.)
    for (const auto & devConfig : config.devices)
    {
        auto device = m_registry.Create (devConfig.type, devConfig, m_memoryBus);

        if (!device)
        {
            DEBUGMSG (L"Warning: Unknown device type '%hs'\n", devConfig.type.c_str());
            continue;
        }

        // Track specific device pointers for quick access
        if (devConfig.type == "apple2-keyboard" ||
            devConfig.type == "apple2e-keyboard")
        {
            m_keyboard = static_cast<AppleKeyboard *> (device.get());
        }
        else if (devConfig.type == "apple2-softswitches" ||
                 devConfig.type == "apple2e-softswitches")
        {
            m_softSwitches = static_cast<AppleSoftSwitchBank *> (device.get());
        }
        else if (devConfig.type == "apple2-speaker")
        {
            m_speaker = static_cast<AppleSpeaker *> (device.get());
        }

        m_memoryBus.AddDevice (device.get());
        m_memoryBus.AddDevice (device.get());
        m_ownedDevices.push_back (move (device));
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  WireLanguageCard
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::WireLanguageCard()
{
    LanguageCard * lc        = nullptr;
    RomDevice    * romDevice = nullptr;



    // Find the LanguageCard device
    for (auto & dev : m_ownedDevices)
    {
        lc = dynamic_cast<LanguageCard *> (dev.get());

        if (lc != nullptr)
        {
            break;
        }
    }

    if (lc == nullptr)
    {
        return;
    }

    // Find a ROM device covering $D000–$FFFF
    for (const auto & entry : m_memoryBus.GetEntries())
    {
        auto * rom = dynamic_cast<RomDevice *> (entry.device);

        if (rom != nullptr && entry.start <= 0xD000 && entry.end >= 0xFFFF)
        {
            romDevice = rom;
            break;
        }
    }

    if (romDevice == nullptr)
    {
        return;
    }

    Word romStart = romDevice->GetStart();

    // Copy $D000–$FFFF ROM data to language card
    vector<Byte> lcRomData (0x3000);

    for (size_t i = 0; i < 0x3000; i++)
    {
        lcRomData[i] = romDevice->Read (static_cast<Word> (0xD000 + i));
    }

    lc->SetRomData (lcRomData);
    m_memoryBus.RemoveDevice (romDevice);

    // Re-add lower ROM ($C100–$CFFF) if original extended below $D000
    if (romStart < 0xD000)
    {
        size_t lowerSize = 0xD000 - romStart;
        vector<Byte> lowerData (lowerSize);

        for (size_t i = 0; i < lowerSize; i++)
        {
            lowerData[i] = romDevice->Read (static_cast<Word> (romStart + i));
        }

        auto lowerRom = RomDevice::CreateFromData (
            romStart, static_cast<Word> (0xCFFF),
            lowerData.data(), lowerData.size());

        m_memoryBus.AddDevice (lowerRom.get());
        m_ownedDevices.push_back (move (lowerRom));
    }

    // Bank device intercepts $D000–$FFFF, routing to LC RAM or ROM
    auto lcBank = make_unique<LanguageCardBank> (*lc);
    m_memoryBus.AddDevice (lcBank.get());
    m_ownedDevices.push_back (move (lcBank));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountCommandLineDisks
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::MountCommandLineDisks (
    const string & disk1Path,
    const string & disk2Path)
{
    if (disk1Path.empty() && disk2Path.empty())
    {
        return;
    }

    for (auto & dev : m_ownedDevices)
    {
        auto * diskCtrl = dynamic_cast<DiskIIController *> (dev.get());

        if (diskCtrl)
        {
            if (!disk1Path.empty())
            {
                diskCtrl->MountDisk (0, disk1Path);
            }

            if (!disk2Path.empty())
            {
                diskCtrl->MountDisk (1, disk2Path);
            }

            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateVideoModes
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CreateVideoModes()
{
    auto textMode = make_unique<AppleTextMode> (m_memoryBus);
    m_activeVideoMode = textMode.get();
    m_videoModes.push_back (move (textMode));

    auto loResMode = make_unique<AppleLoResMode> (m_memoryBus);
    m_videoModes.push_back (move (loResMode));

    auto hiResMode = make_unique<AppleHiResMode> (m_memoryBus);
    m_videoModes.push_back (move (hiResMode));

    auto doubleHiResMode = make_unique<AppleDoubleHiResMode> (m_memoryBus);
    m_videoModes.push_back (move (doubleHiResMode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateCpu
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::CreateCpu (const MachineConfig & config)
{
    HRESULT  hr      = S_OK;
    ifstream romFile;
    Word     addr    = 0;
    char     byte    = 0;



    m_cpu = make_unique<EmuCpu> (m_memoryBus);

    // The base Cpu class uses an internal memory[] array for opcode fetch
    // and instruction execution. Copy ROM data into that array.
    for (const auto & region : config.memoryRegions)
    {
        if (region.type != "rom" || region.resolvedPath.empty())
        {
            continue;
        }

        romFile.open (region.resolvedPath, ios::binary);
        if (!romFile.good())
        {
            continue;
        }

        addr = region.start;
        
        while (romFile.good() && addr <= region.end)
        {
            romFile.read (&byte, 1);

            if (romFile.gcount() == 1)
            {
                m_cpu->PokeByte (addr, static_cast<Byte> (byte));
                addr++;
            }
        }

        romFile.close();
    }

    m_cpu->InitForEmulation();

    // Connect speaker to CPU cycle counter for audio timestamps
    if (m_speaker != nullptr)
    {
        m_speaker->SetCycleCounter (m_cpu->GetCycleCounterPtr());
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunMessageLoop
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::RunMessageLoop()
{
    MSG msg = {};



    // Start the CPU thread
    m_cpuThread = thread (&EmulatorShell::CpuThreadProc, this);

    // UI thread loop: process messages, present latest framebuffer with vsync
    while (m_running.load (memory_order_acquire))
    {
        // Process all pending messages
        while (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_running.store (false, memory_order_release);

                if (m_cpuThread.joinable())
                {
                    m_cpuThread.join();
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

        // Copy latest framebuffer under lock, then present with vsync
        {
            lock_guard<mutex> lock (m_fbMutex);

            if (m_fbReady)
            {
                m_uiFramebuffer = m_cpuFramebuffer;
                m_fbReady = false;
            }
        }

        m_d3dRenderer.UploadAndPresent (m_uiFramebuffer.data());
    }

    if (m_cpuThread.joinable())
    {
        m_cpuThread.join();
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuThreadProc
//
//  Runs on a dedicated thread.  Owns the 6502 execution loop, audio
//  generation/submission (WASAPI), and software framebuffer rendering.
//  Communicates with the UI thread via atomics and mutex-protected buffers.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CpuThreadProc()
{
    HANDLE        hTimer  = nullptr;
    LARGE_INTEGER dueTime = {};
    SpeedMode     speed   = SpeedMode::Authentic;



    // Initialize COM on this thread for WASAPI
    CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    // Initialize WASAPI audio (non-fatal if it fails)
    m_wasapiAudio.Initialize();

    // Create a high-resolution waitable timer for 60fps frame pacing
    hTimer = CreateWaitableTimerEx (nullptr, nullptr,
                                    CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                    TIMER_ALL_ACCESS);

    if (hTimer == nullptr)
    {
        hTimer = CreateWaitableTimer (nullptr, FALSE, nullptr);
    }

    while (m_running.load (memory_order_acquire))
    {
        if (m_paused.load (memory_order_acquire))
        {
            Sleep (10);
            continue;
        }

        // Process deferred commands from the UI thread
        ProcessCommands();

        // Arm the timer for this frame
        if (hTimer != nullptr)
        {
            dueTime.QuadPart = -166667;  // 16.6667ms
            SetWaitableTimer (hTimer, &dueTime, 0, nullptr, nullptr, FALSE);
        }

        // Execute one frame of CPU + audio + video
        RunOneFrame();

        // Publish the framebuffer for the UI thread
        {
            lock_guard<mutex> lock (m_fbMutex);
            m_fbReady = true;
        }

        // Wait for the remainder of the frame period
        speed = m_speedMode.load (memory_order_acquire);

        if (speed != SpeedMode::Maximum && hTimer != nullptr)
        {
            WaitForSingleObject (hTimer, INFINITE);
        }
    }

    if (hTimer != nullptr)
    {
        CloseHandle (hTimer);
    }

    m_wasapiAudio.Shutdown();
    CoUninitialize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProcessCommands
//
//  Drains the command queue and executes each command on the CPU thread,
//  where it is safe to touch CPU, bus, and device state.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ProcessCommands()
{
    vector<EmulatorCommand> cmds;

    {
        lock_guard<mutex> lock (m_cmdMutex);
        cmds.swap (m_commandQueue);
    }

    for (const auto & cmd : cmds)
    {
        switch (cmd.id)
        {
            case IDM_MACHINE_RESET:
            {
                if (m_cpu)
                {
                    Word resetVec = m_cpu->ReadWord (0xFFFC);
                    m_cpu->SetPC (resetVec);
                }
                break;
            }

            case IDM_MACHINE_POWERCYCLE:
            {
                m_memoryBus.Reset();

                if (m_cpu)
                {
                    m_cpu->InitForEmulation();
                }
                break;
            }

            case IDM_MACHINE_STEP:
            {
                if (m_cpu)
                {
                    m_cpu->StepOne();
                }
                break;
            }

            case IDM_DISK_INSERT1:
            case IDM_DISK_INSERT2:
            {
                int drive = (cmd.id == IDM_DISK_INSERT1) ? 0 : 1;

                for (auto & dev : m_ownedDevices)
                {
                    auto * diskCtrl = dynamic_cast<DiskIIController *> (dev.get());

                    if (diskCtrl)
                    {
                        diskCtrl->MountDisk (drive, cmd.payload);
                        break;
                    }
                }
                break;
            }

            case IDM_DISK_EJECT1:
            case IDM_DISK_EJECT2:
            {
                int drive = (cmd.id == IDM_DISK_EJECT1) ? 0 : 1;

                for (auto & dev : m_ownedDevices)
                {
                    auto * diskCtrl = dynamic_cast<DiskIIController *> (dev.get());

                    if (diskCtrl)
                    {
                        diskCtrl->EjectDisk (drive);
                        break;
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostCommand (WORD id, const string & payload)
{
    lock_guard<mutex> lock (m_cmdMutex);
    m_commandQueue.push_back ({ id, payload });
}





////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
//  RunOneFrame
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunOneFrame()
{
    ExecuteCpuSlices();
    RenderFramebuffer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteCpuSlices
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ExecuteCpuSlices()
{
    static constexpr uint32_t kSliceCycles = 1023;

    uint32_t  targetCycles    = m_cyclesPerFrame;
    SpeedMode speed           = m_speedMode.load (memory_order_acquire);
    bool      audioActive     = (m_speaker != nullptr && m_wasapiAudio.IsInitialized());
    double    cyclesPerSample = 0.0;
    uint32_t  sliceTarget     = 0;
    uint32_t  sliceActual     = 0;
    Byte      cycles          = 0;
    double    exactSamples    = 0.0;
    uint32_t  numSamples      = 0;



    if (speed == SpeedMode::Double)
    {
        targetCycles *= 2;
    }

    if (audioActive)
    {
        cyclesPerSample = static_cast<double> (m_config.clockSpeed) /
                          static_cast<double> (m_wasapiAudio.GetSampleRate());
        m_speaker->BeginFrame();
    }

    for (uint32_t executed = 0; executed < targetCycles; )
    {
        sliceTarget = targetCycles - executed;

        if (sliceTarget > kSliceCycles)
        {
            sliceTarget = kSliceCycles;
        }

        sliceActual = 0;

        while (sliceActual < sliceTarget)
        {
            m_cpu->StepOne();

            cycles = m_cpu->GetLastInstructionCycles();

            m_cpu->AddCycles (cycles);
            sliceActual += cycles;
        }

        executed += sliceActual;

        if (audioActive)
        {
            exactSamples = static_cast<double> (sliceActual) / cyclesPerSample + m_sampleRemainder;
            numSamples   = static_cast<uint32_t> (exactSamples);

            m_sampleRemainder = exactSamples - static_cast<double> (numSamples);

            m_wasapiAudio.SubmitFrame (m_speaker->GetToggleTimestamps(),
                                       sliceActual,
                                       m_speaker->GetFrameInitialState(),
                                       numSamples);

            m_speaker->ClearTimestamps();
            m_speaker->BeginFrame();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFramebuffer
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RenderFramebuffer()
{
    ColorMode color = m_colorMode.load (memory_order_acquire);
    Byte      r     = 0;
    Byte      g     = 0;
    Byte      b     = 0;
    Byte      lum   = 0;



    SelectVideoMode();

    if (m_activeVideoMode != nullptr)
    {
        m_activeVideoMode->Render (m_cpu->GetMemory(),
                                   m_cpuFramebuffer.data(),
                                   kFramebufferWidth,
                                   kFramebufferHeight);
    }

    // Mixed mode: overlay text on the bottom 4 rows (rows 20-23)
    if (m_mixedMode && m_graphicsMode && !m_videoModes.empty())
    {
        static constexpr int kMixedCharH  = 8;
        static constexpr int kMixedScaleY = 2;
        int mixedFbY = 20 * kMixedCharH * kMixedScaleY;

        vector<uint32_t> textBuf (static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight, 0);

        m_videoModes[0]->Render (m_cpu->GetMemory(),
                                 textBuf.data(),
                                 kFramebufferWidth,
                                 kFramebufferHeight);

        size_t rowBytes = static_cast<size_t> (kFramebufferWidth) * sizeof (uint32_t);

        for (int y = mixedFbY; y < kFramebufferHeight; y++)
        {
            memcpy (&m_cpuFramebuffer[static_cast<size_t> (y) * kFramebufferWidth],
                    &textBuf[static_cast<size_t> (y) * kFramebufferWidth],
                    rowBytes);
        }
    }

    // Apply monochrome tint
    if (color != ColorMode::Color)
    {
        for (auto & pixel : m_cpuFramebuffer)
        {
            r   = (pixel >>  0) & 0xFF;
            g   = (pixel >>  8) & 0xFF;
            b   = (pixel >> 16) & 0xFF;
            lum = static_cast<Byte> (0.299f * r + 0.587f * g + 0.114f * b);

            switch (color)
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
}




////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnCommand (HWND hwnd, int id)
{
    UNREFERENCED_PARAMETER (hwnd);

    if      (id >= IDM_FILE_OPEN     && id <= IDM_FILE_EXIT)          { OnFileCommand (id); }
    else if (id >= IDM_MACHINE_RESET && id <= IDM_MACHINE_INFO)       { OnMachineCommand (id); }
    else if (id >= IDM_DISK_INSERT1  && id <= IDM_DISK_WRITEPROTECT2) { OnDiskCommand (id); }
    else if (id >= IDM_VIEW_COLOR    && id <= IDM_VIEW_RESET_SIZE)    { OnViewCommand (id); }
    else if (id >= IDM_HELP_KEYMAP   && id <= IDM_HELP_ABOUT)         { OnHelpCommand (id); }

    return false;
}





////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

LRESULT EmulatorShell::OnCreate (HWND hwnd, CREATESTRUCT * pcs)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (pcs);

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnDestroy (HWND hwnd)
{
    UNREFERENCED_PARAMETER (hwnd);



    m_running.store (false, memory_order_release);
    PostQuitMessage (0);
    
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);



    if (m_keyboard == nullptr)
    {
        return false;
    }

    m_keyboard->SetKeyDown (true);

    switch (vk)
    {
        case VK_LEFT:
            m_keyboard->KeyPress (kAppleKeyLeft);
            break;
            
        case VK_RIGHT:
            m_keyboard->KeyPress (kAppleKeyRight);
            break;

        case VK_UP:
            m_keyboard->KeyPress (kAppleKeyUp);
            break;

        case VK_DOWN:
            m_keyboard->KeyPress (kAppleKeyDown);
            break;
            
        case VK_ESCAPE:
            m_keyboard->KeyPress (kAppleKeyEscape);
            break;

        case VK_DELETE:
            m_keyboard->KeyPress (kAppleKeyDelete);
            break;

        default:
            break;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyUp
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (vk);
    UNREFERENCED_PARAMETER (lParam);

    if (m_keyboard)
    {
        m_keyboard->SetKeyDown (false);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnChar (WPARAM ch, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    if (m_keyboard == nullptr)
    {
        return false;
    }

    if (ch >= 1 && ch <= 127)
    {
        m_keyboard->KeyPress (static_cast<Byte> (ch));
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnSize (HWND hwnd, UINT width, UINT height)
{
    UNREFERENCED_PARAMETER (hwnd);

    m_d3dRenderer.Resize (static_cast<int> (width), static_cast<int> (height));
    return false;
}
//
//  OnFileCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileCommand (int id)
{
    switch (id)
    {
        case IDM_FILE_OPEN:
        {
            // TODO: Implement hot-swap machine config loading.
            break;
        }

        case IDM_FILE_EXIT:
        {
            DestroyWindow (m_hwnd);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnMachineCommand (int id)
{
    bool paused = false;

    switch (id)
    {
        case IDM_MACHINE_RESET:
        case IDM_MACHINE_POWERCYCLE:
        {
            PostCommand (static_cast<WORD> (id));
            break;
        }

        case IDM_MACHINE_PAUSE:
        {
            paused = !m_paused.load (memory_order_acquire);
            m_paused.store (paused, memory_order_release);
            m_menuSystem.SetPaused (paused);
            UpdateWindowTitle();
            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (m_paused.load (memory_order_acquire))
            {
                PostCommand (static_cast<WORD> (id));
            }
            break;
        }

        case IDM_MACHINE_SPEED_1X:
        {
            m_speedMode.store (SpeedMode::Authentic, memory_order_release);
            m_menuSystem.SetSpeedMode (SpeedMode::Authentic);
            break;
        }

        case IDM_MACHINE_SPEED_2X:
        {
            m_speedMode.store (SpeedMode::Double, memory_order_release);
            m_menuSystem.SetSpeedMode (SpeedMode::Double);
            break;
        }

        case IDM_MACHINE_SPEED_MAX:
        {
            m_speedMode.store (SpeedMode::Maximum, memory_order_release);
            m_menuSystem.SetSpeedMode (SpeedMode::Maximum);
            break;
        }

        case IDM_MACHINE_INFO:
        {
            wstring info = format (
                L"Machine: {}\n"
                L"CPU: {}\n"
                L"Clock Speed: {} Hz\n"
                L"Memory Regions: {}\n"
                L"Devices: {}",
                wstring (m_config.name.begin(), m_config.name.end()),
                wstring (m_config.cpu.begin(), m_config.cpu.end()),
                m_config.clockSpeed,
                m_config.memoryRegions.size(),
                m_config.devices.size());

            MessageBoxW (m_hwnd, info.c_str(), L"Machine Info", MB_ICONINFORMATION | MB_OK);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnViewCommand (int id)
{
    UINT        dpi   = 0;
    int         scale = 0;
    RECT        rc    = {};
    DWORD       style = 0;
    HMONITOR    hMon  = nullptr;
    MONITORINFO mi    = { sizeof (mi) };
    int         w     = 0;
    int         h     = 0;
    int         x     = 0;
    int         y     = 0;

    switch (id)
    {
        case IDM_VIEW_COLOR:
        {
            m_colorMode.store (ColorMode::Color, memory_order_release);
            m_menuSystem.SetColorMode (ColorMode::Color);
            break;
        }

        case IDM_VIEW_GREEN:
        {
            m_colorMode.store (ColorMode::GreenMono, memory_order_release);
            m_menuSystem.SetColorMode (ColorMode::GreenMono);
            break;
        }

        case IDM_VIEW_AMBER:
        {
            m_colorMode.store (ColorMode::AmberMono, memory_order_release);
            m_menuSystem.SetColorMode (ColorMode::AmberMono);
            break;
        }

        case IDM_VIEW_WHITE:
        {
            m_colorMode.store (ColorMode::WhiteMono, memory_order_release);
            m_menuSystem.SetColorMode (ColorMode::WhiteMono);
            break;
        }

        case IDM_VIEW_FULLSCREEN:
        {
            m_d3dRenderer.ToggleFullscreen (m_hwnd);
            break;
        }

        case IDM_VIEW_RESET_SIZE:
        {
            if (!m_d3dRenderer.IsFullscreen())
            {
                dpi   = GetDpiForWindow (m_hwnd);
                scale = (dpi + 48) / 96;

                if (scale < 1)
                {
                    scale = 1;
                }

                rc    = { 0, 0, kFramebufferWidth * scale, kFramebufferHeight * scale };
                style = static_cast<DWORD> (GetWindowLong (m_hwnd, GWL_STYLE));
                AdjustWindowRect (&rc, style, TRUE);

                w = rc.right - rc.left;
                h = rc.bottom - rc.top;

                hMon = MonitorFromWindow (m_hwnd, MONITOR_DEFAULTTONEAREST);
                GetMonitorInfo (hMon, &mi);

                x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
                y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

                SetWindowPos (m_hwnd, nullptr, x, y, w, h, SWP_NOZORDER);
            }
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnDiskCommand (int id)
{
    WCHAR           filePath[MAX_PATH] = {};
    OPENFILENAMEW   ofn                = {};

    switch (id)
    {
        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
        {
            ofn.lStructSize = sizeof (ofn);
            ofn.hwndOwner   = m_hwnd;
            ofn.lpstrFilter = L"Disk Images (*.dsk)\0*.dsk\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile   = filePath;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrTitle  = (id == IDM_DISK_INSERT1) ?
                L"Insert Disk in Drive 1" : L"Insert Disk in Drive 2";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW (&ofn))
            {
                PostCommand (static_cast<WORD> (id), fs::path (filePath).string());
            }
            break;
        }

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
        {
            PostCommand (static_cast<WORD> (id));
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHelpCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnHelpCommand (int id)
{
    switch (id)
    {
        case IDM_HELP_DEBUG:
        {
            if (m_debugConsole.IsVisible())
            {
                m_debugConsole.Hide();
            }
            else
            {
                if (m_debugConsole.Show (m_hInstance))
                {
                    m_debugConsole.LogConfig (
                        format ("Machine: {}\nCPU: {}\nClock: {} Hz\nDevices: {}",
                            m_config.name, m_config.cpu, m_config.clockSpeed,
                            m_config.devices.size()));
                }
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
    }
}
////////////////////////////////////////////////////////////////////////////////
//
//  UpdateWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateWindowTitle()
{
    wstring title;
    wstring wideName;



    if (m_hwnd == nullptr)
    {
        return;
    }

    title = L"Casso65";

    if (!m_config.name.empty())
    {
        title += L" — ";

        // Convert machine name to wide string
        wideName.assign (m_config.name.begin(), m_config.name.end());
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

    SetWindowText (m_hwnd, title.c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectVideoMode
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SelectVideoMode()
{
    if (m_videoModes.size() < 3)
    {
        return;
    }

    // Read soft switch state
    if (m_softSwitches)
    {
        m_graphicsMode = m_softSwitches->IsGraphicsMode();
        m_mixedMode    = m_softSwitches->IsMixedMode();
        m_page2        = m_softSwitches->IsPage2();
        m_hiresMode    = m_softSwitches->IsHiresMode();
    }

    // Select video mode based on soft switch state
    if (!m_graphicsMode)
    {
        // Text mode
        m_activeVideoMode = m_videoModes[0].get();
    }
    else if (!m_hiresMode)
    {
        // Lo-res graphics
        m_activeVideoMode = m_videoModes[1].get();
    }
    else
    {
        // Hi-res graphics
        m_activeVideoMode = m_videoModes[2].get();
    }

    // Pass page2 state to the active renderer
    if (m_activeVideoMode != nullptr)
    {
        m_activeVideoMode->SetPage2 (m_page2);
    }

    // Keep text mode page2-aware for mixed-mode overlay rendering
    m_videoModes[0]->SetPage2 (m_page2);
}





