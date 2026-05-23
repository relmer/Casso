#include "Pch.h"

#include "EmulatorShell.h"
#include "AssetBootstrap.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/AppleIIeKeyboard.h"
#include "Devices/AppleSoftSwitchBank.h"
#include "Devices/AppleIIeSoftSwitchBank.h"
#include "Devices/AppleSpeaker.h"
#include "Devices/DiskIIController.h"
#include "Devices/LanguageCard.h"
#include "Devices/AppleIIeMmu.h"
#include "Core/Prng.h"

#include "RegistrySettings.h"
#include "DiskSettings.h"
#include "UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Video/AppleTextMode.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Win11DwmHelpers.h"
#include "Ui/TitleBarHitTest.h"
#include "Ui/DriveWidgetController.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Embed Common Controls v6 dependency in the binary's activation
// context. Without this, SetWindowSubclass / TOOLINFO / and a host
// of other modern comctl32 APIs fall back to no-op v5 behavior --
// in particular our drive-bay tooltips never appear because
// SetWindowSubclass returns FALSE silently.
#pragma comment(linker, "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

static constexpr LPCWSTR kLastMachineValue = L"LastMachine";





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int    kFramebufferWidth  = 560;
static constexpr int    kFramebufferHeight = 384;
static constexpr LPCWSTR kWindowClass      = L"CassoWindow";

// Drive activity LED refresh timer (~20 Hz). Cheap and responsive enough
// for the eye to register motor on/off bursts without consuming UI cycles.
static constexpr int      s_kNavStripHeightDp    = 28;

static WORD ResolveMachineSpeedCommand (const JsonValue & mergedJson)
{
    const JsonValue * uiPrefs = nullptr;
    std::string       speed;


    if (mergedJson.GetType() != JsonType::Object)
    {
        return 0;
    }

    if (FAILED (mergedJson.GetObject ("$cassoUiPrefs", uiPrefs)) || uiPrefs == nullptr)
    {
        return 0;
    }

    if (FAILED (uiPrefs->GetString ("speedMode", speed)))
    {
        return 0;
    }

    if (speed == "authentic") return IDM_MACHINE_SPEED_1X;
    if (speed == "double")    return IDM_MACHINE_SPEED_2X;
    if (speed == "maximum")   return IDM_MACHINE_SPEED_MAX;

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Window placement helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    int ComputeChromeTopInsetPx (UINT dpi)
    {
        int   navStrip = MulDiv (s_kNavStripHeightDp, static_cast<int> (dpi), 96);



        return TitleBarLayout::DefaultTitleHeight ((UINT) dpi) + navStrip;
    }


    BOOL RegisterRenderSurfaceClass (HINSTANCE hInstance)
    {
        WNDCLASSEXW wcex = { sizeof (wcex) };



        if (GetClassInfoExW (hInstance, L"CassoRenderSurface", &wcex))
        {
            return TRUE;
        }

        wcex.style         = 0;
        wcex.lpfnWndProc   = EmulatorShell::s_RenderSurfaceWndProc;
        wcex.cbClsExtra    = 0;
        wcex.cbWndExtra    = 0;
        wcex.hInstance     = hInstance;
        wcex.hIcon         = nullptr;
        wcex.hCursor       = nullptr;
        wcex.hbrBackground = nullptr;
        wcex.lpszMenuName  = nullptr;
        wcex.lpszClassName = L"CassoRenderSurface";
        wcex.hIconSm       = nullptr;

        return RegisterClassExW (&wcex) != 0;
    }


    bool GetCursorMonitorWorkArea (RECT & outWork, HMONITOR & outMonitor)
    {
        POINT          pt       = {};
        HMONITOR       hMon     = nullptr;
        MONITORINFOEXW mi       = { sizeof (mi) };



        if (!GetCursorPos (&pt))
        {
            pt.x = 0;
            pt.y = 0;
        }

        hMon = MonitorFromPoint (pt, MONITOR_DEFAULTTONEAREST);
        if (hMon == nullptr)
        {
            return false;
        }

        if (!GetMonitorInfoW (hMon, &mi))
        {
            return false;
        }

        outWork    = mi.rcWork;
        outMonitor = hMon;
        return true;
    }


    void CenterInWorkArea (
        const RECT & work,
        int          windowW,
        int          windowH,
        LONG       & outX,
        LONG       & outY)
    {
        outX = work.left + (work.right - work.left - windowW) / 2;
        outY = work.top  + (work.bottom - work.top - windowH) / 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::EmulatorShell()
{
    // / FR-035. The Prng is the deterministic stand-in for
    // indeterminate //e DRAM at power-on, shared across every device that
    // re-seeds in PowerCycle. The seed is derived from a couple of
    // weakly-correlated host sources so consecutive launches hit
    // different patterns; tests pin the seed directly via the test
    // harness instead of going through this path.
    uint64_t    seed = static_cast<uint64_t> (time (nullptr));

    seed ^= static_cast<uint64_t> (GetCurrentProcessId()) << 32;

    m_prng = make_unique<Prng> (seed);

    // / FR-033 / T055. //e video timing model — owned at the
    // shell level so all three machine kinds (][/][+/]e) share the same
    // 17,030-cycle frame counter for $C019 (RDVBLBAR) reads.
    m_videoTiming = make_unique<VideoTiming>();

    m_clipboardManager = std::make_unique<ClipboardManager> (m_memoryBus,
                                                              m_cpuManager.GetCommandMutex(),
                                                              m_cpuManager.GetPasteBuffer(),
                                                              m_fbMutex,
                                                              m_uiFramebuffer,
                                                              kFramebufferWidth,
                                                              kFramebufferHeight,
                                                              &m_refs.keyboard);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::~EmulatorShell()
{
    HRESULT             hrFlush    = S_OK;
    DiskIIController *  controller = nullptr;



    m_cpuManager.Stop();

    // Spec-006 / FR-024. Revoke BOTH sinks BEFORE the dialog tears
    // down its ring (and before the controller / audio source itself
    // is destroyed, which happens via m_ownedDevices / m_diskAudioSources
    // below). Controller sink first, then audio sink, matching the
    // attachment order in OpenDiskIIDebugDialog.
    if (m_diskIIDebugDialog != nullptr)
    {
        controller = FindSlot6Controller();

        if (controller != nullptr)
        {
            controller->SetEventSink (nullptr);
        }

        for (size_t i = 0; i < m_diskAudioSources.size(); i++)
        {
            if (m_diskAudioSources[i] != nullptr)
            {
                m_diskAudioSources[i]->SetAudioEventSink (nullptr);
            }
        }

        m_diskIIDebugDialog->Destroy();
        m_diskIIDebugDialog.reset();
    }

    // / T097 / FR-025. Final auto-flush of any dirty disks on
    // process shutdown — matches the "graceful exit" requirement from
    // audit §7 so a crash-free quit never loses user writes.
    hrFlush = m_diskStore.FlushAll();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    // Native-only ownership teardown.
    m_d3dRenderer.SetAfterBlitHook (nullptr);
    m_uiShell.Shutdown();
    m_dragDropTarget.Shutdown();
    m_driveWidgets.UnloadDocument();
    m_navLayer.Hide();
    m_titleBar.Hide();

    m_d3dRenderer.Shutdown();

    if (m_fOleInitialized)
    {
        OleUninitialize();
        m_fOleInitialized = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Initialize (
    HINSTANCE             hInstance,
    const wstring       & machineName,
    const MachineConfig & config,
    const string        & disk1Path,
    const string        & disk2Path)
{
    HRESULT hr     = S_OK;
    size_t  fbSize = 0;



    m_currentMachineName = machineName;
    m_config             = config;
    m_cyclesPerFrame     = config.cyclesPerFrame;

    // P6 -- bring up OLE on the UI thread before any RegisterDragDrop
    // call lands; IFileDialog (click-to-browse, used by the drive
    // widgets) also requires the apartment to be live. OleInitialize
    // implies CoInitializeEx(STA) on this thread. Non-fatal on
    // failure -- drag-drop and the file dialog will degrade quietly.
    {
        HRESULT  hrOle = OleInitialize (nullptr);

        if (SUCCEEDED (hrOle))
        {
            m_fOleInitialized = true;
        }
        else
        {
            OutputDebugStringA ("[EmulatorShell] OleInitialize failed; drag-drop disabled.\n");
        }
    }

    // Register built-in device factories
    ComponentRegistry::RegisterBuiltinDevices (m_registry);

    // Create framebuffers (CPU renders to one, UI reads the other)
    fbSize = static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight;
    m_cpuFramebuffer.resize (fbSize, 0);
    m_textOverlay.resize (fbSize, 0);
    m_uiFramebuffer.resize (fbSize, 0);

    hr = CreateEmulatorWindow (hInstance);
    CHR (hr);

    hr = CreateMemoryDevices (config);
    CHR (hr);

    WireLanguageCard();
    CreateVideoModes();

    // Validate memory bus for overlapping device address ranges
    hr = m_memoryBus.Validate();
    CHR (hr);

    hr = CreateCpu (config);
    CHR (hr);

    WirePageTable();

    hr = CreateRenderSurface();
    CHR (hr);

    // Initialize D3D11
    hr = m_d3dRenderer.Initialize (m_renderHwnd, kFramebufferWidth, kFramebufferHeight);
    CHR (hr);

    // Native UI runtime bootstrap. UiShell owns the painter, text
    // renderer, hit-tester, focus manager, and input translator;
    // wiring it onto the after-blit hook lets it composite chrome on
    // top of the emulator frame without ever pausing the render loop.
    {
        HRESULT  hrUi = m_uiShell.Initialize (&m_d3dRenderer);
        IGNORE_RETURN_VALUE (hrUi, S_OK);
        m_uiShell.SetChrome (&m_titleBar, &m_navLayer, &m_driveChrome, &m_chromeTheme);

        if (SUCCEEDED (hrUi))
        {
            m_d3dRenderer.SetAfterBlitHook ([this] () { UpdateDriveWidgets(); m_uiShell.Render(); });
            m_uiShell.HitTest().Clear();
            m_uiShell.HitTest().Register (HitRect { m_driveChrome[0].BodyRect(), HitSlot::Custom, 0 });
            m_uiShell.HitTest().Register (HitRect { m_driveChrome[1].BodyRect(), HitSlot::Custom, 1 });
            if (m_fOleInitialized)
            {
                HRESULT hrDrop = m_dragDropTarget.Initialize (m_hwnd, &m_uiShell.HitTest(), [this] (int tag, const std::wstring & path) { Mount (6, tag, path); });
                IGNORE_RETURN_VALUE (hrDrop, S_OK);
            }
        }
    }

    // Native-only bootstrap baseline: legacy chrome overlay retired
    // ahead of the native painter. Keep existing command/menu path active.

    // WASAPI audio is initialized on the CPU thread (COM apartment requirement)

    // Show window
    ShowWindow (m_hwnd, SW_SHOW);
    UpdateWindow (m_hwnd);

    UpdateWindowTitle();

    // / FR-034. Cold power-on: seed DRAM via the shared Prng and
    // run the 6502 /RESET sequence. Without this, the CPU starts at PC=0
    // and executes uninitialized RAM, leading to garbage on screen and
    // a beep loop instead of the firmware prompt. Mirrors what the
    // headless test harness does after BuildAppleII* construction.
    //
    // Must run BEFORE MountCommandLineDisks: PowerCycle ejects every
    // drive and re-binds the engine to the controller's empty internal
    // disk. Mounting first then power-cycling silently throws away the
    // user's freshly-mounted image (the engine ticks but AdvanceOneBit
    // exits early because trackBits[0] == 0).
    PowerCycle();

    MountCommandLineDisks (disk1Path, disk2Path);

    // Seed the mixer state
    // from the per-machine registry before the audio thread first
    // calls SetEnabled / SetMechanism. Default is enabled + Shugart
    // when nothing has been persisted yet.
    {
        wstring  subkey;
        DWORD    enabledDw = 1;
        wstring  mechanism;
        HRESULT  hrReg     = S_OK;

        subkey = wstring (L"Machines\\") + m_currentMachineName;

        hrReg = RegistrySettings::ReadDword (subkey.c_str(),
                                             L"DriveAudioEnabled",
                                             enabledDw);
        IGNORE_RETURN_VALUE (hrReg, S_OK);

        m_driveAudioMixer.SetEnabled (enabledDw != 0);

        hrReg = RegistrySettings::ReadString (subkey.c_str(),
                                              L"DiskIIMechanism",
                                              mechanism);
        IGNORE_RETURN_VALUE (hrReg, S_OK);

        if (!mechanism.empty() && m_driveAudioMixer.IsValidMechanism (mechanism))
        {
            HRESULT  hrMech = m_driveAudioMixer.SetMechanism (mechanism);
            IGNORE_RETURN_VALUE (hrMech, S_OK);
        }
    }

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
    HRESULT   hr          = S_OK;
    UINT      dpi         = 0;
    int       scale       = 0;
    int       clientW     = 0;
    int       clientH     = 0;
    RECT      rc          = {};
    DWORD     style       = 0;
    BOOL      fSuccess    = FALSE;
    RECT      work        = {};
    HMONITOR  activeMon   = nullptr;
    LONG      windowX     = CW_USEDEFAULT;
    LONG      windowY     = CW_USEDEFAULT;
    int       windowW     = 0;
    int       windowH     = 0;



    // Register and create the window via the base class
    m_kpszWndClass  = kWindowClass;
    m_hbrBackground = reinterpret_cast<HBRUSH> (GetStockObject (BLACK_BRUSH));

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
    clientH = kFramebufferHeight * scale + ComputeChromeTopInsetPx (dpi);

    rc    = { 0, 0, clientW, clientH };
    // Borderless custom-chrome recipe. WS_THICKFRAME keeps
    // Aero Snap + edge resize live; WS_CAPTION stays in the style
    // mask so DWM still grants us a drop shadow + snap-layouts
    // animations even though we draw zero chrome ourselves. Sizing
    // math through AdjustWindowRectExForDpi accounts for the
    // (invisible) non-client frame so the requested client area is
    // preserved.
    style    = WS_POPUP | WS_CAPTION | WS_THICKFRAME |
               WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU |
               WS_CLIPCHILDREN;
    // No menu bar -> bMenu = FALSE in window-rect math.
    fSuccess = AdjustWindowRectExForDpi (&rc, style, FALSE, 0, dpi);
    CWRA (fSuccess);

    windowW = rc.right - rc.left;
    windowH = rc.bottom - rc.top;

    if (GetCursorMonitorWorkArea (work, activeMon))
    {
        CenterInWorkArea (work, windowW, windowH, windowX, windowY);
    }

    m_windowManager.TryLoadSavedWindowPlacement (activeMon, windowX, windowY, windowW, windowH);
    hr = Window::Create (0,
                         L"Casso",
                         style,
                         windowX, windowY,
                         windowW, windowH,
                         nullptr);
    CHR (hr);

    // DWM gating. Rounded corners + dark immersive caption
    // are best-effort and runtime-gated to the right Win10/11 build.
    // Mica stays opt-in: it'll be toggled per-theme in P5 via
    // theme.json `useMicaBackdrop`.
    Win11DwmHelpers::ExtendFrameIntoClientArea (m_hwnd, 1);
    Win11DwmHelpers::ApplyRoundedCorners       (m_hwnd, true);
    Win11DwmHelpers::ApplyImmersiveDarkMode    (m_hwnd, true);

    // Legacy Win32 menu bar is retired (FR-026). All menu
    // commands now route through `NavLayer` + the native nav strip;
    // keyboard accelerators (loaded below) keep working independently
    // of the menu bar. `m_menuSystem` is intentionally left in place
    // to cache `SpeedMode` / `ColorMode` for any downstream reader,
    // but no `HMENU` is ever created or attached to the window.

    // Prime the title-bar layout cache so the WM_NCHITTEST helper has
    // valid button rects even before the first WM_SIZE arrives.
    m_chromeTheme = ChromeTheme::Skeuomorphic();
    m_titleBar.UpdateGeometry (clientW, dpi);
    m_navLayer.Layout (0, m_titleBar.GetTitleHeight(), clientW, dpi);
    m_navLayer.SetDispatch ([this] (WORD commandId) { HandleCommand (commandId); });
    m_driveChrome[0].Initialize (6, 0, this);
    m_driveChrome[1].Initialize (6, 1, this);
    m_driveChrome[0].Layout (24, ComputeChromeTopInsetPx (dpi) + 18, dpi);
    m_driveChrome[1].Layout (230, ComputeChromeTopInsetPx (dpi) + 18, dpi);
    m_d3dRenderer.SetTopInsetPx (ComputeChromeTopInsetPx (dpi));

    // Load accelerator table
    m_accelTable = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR));
    CWRA (m_accelTable);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateRenderSurface
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::CreateRenderSurface ()
{
    HRESULT  hr       = S_OK;
    BOOL     fSuccess = FALSE;
    RECT     rcClient = {};



    fSuccess = GetClientRect (m_hwnd, &rcClient);
    CWRA (fSuccess);

    fSuccess = RegisterRenderSurfaceClass (m_hInstance);
    CWRA (fSuccess);

    m_renderHwnd = CreateWindowExW (0,
                                    L"CassoRenderSurface",
                                    nullptr,
                                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                    0, 0,
                                    rcClient.right, rcClient.bottom,
                                    m_hwnd,
                                    nullptr,
                                    m_hInstance,
                                    nullptr);
    CWRA (m_renderHwnd);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_RenderSurfaceWndProc
//
//  Window proc for the custom render surface child window class. Suppresses
//  background erase and paint paths at the class level to prevent white
//  flash during resize. Chains all other messages to DefWindowProc to
//  preserve normal child window behavior and parent message routing.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK EmulatorShell::s_RenderSurfaceWndProc (
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND        parent = nullptr;
    PAINTSTRUCT ps     = {};



    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            BeginPaint (hwnd, &ps);
            EndPaint (hwnd, &ps);
            return 0;

        case WM_PRINTCLIENT:
            return 0;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            parent = GetParent (hwnd);
            if (parent != nullptr)
            {
                return SendMessage (parent, uMsg, wParam, lParam);
            }
            return DefWindowProc (hwnd, uMsg, wParam, lParam);

        // Keep resize cursors in sync with the parent NC hit-test math.
        case WM_SETCURSOR:
            parent = GetParent (hwnd);
            if (parent != nullptr)
            {
                POINT    pt     = {};
                LRESULT  hit    = HTCLIENT;
                LPCWSTR  cursor = IDC_ARROW;



                if (GetCursorPos (&pt))
                {
                    hit = SendMessage (parent, WM_NCHITTEST, 0, MAKELPARAM (pt.x, pt.y));

                    switch (hit)
                    {
                        case HTTOPLEFT:
                        case HTBOTTOMRIGHT:
                            cursor = IDC_SIZENWSE;
                            break;

                        case HTTOPRIGHT:
                        case HTBOTTOMLEFT:
                            cursor = IDC_SIZENESW;
                            break;

                        case HTTOP:
                        case HTBOTTOM:
                            cursor = IDC_SIZENS;
                            break;

                        case HTLEFT:
                        case HTRIGHT:
                            cursor = IDC_SIZEWE;
                            break;
                    }
                }

                SetCursor (LoadCursorW (nullptr, cursor));
                return TRUE;
            }

            SetCursor (LoadCursorW (nullptr, IDC_ARROW));
            return TRUE;

        // For NC regions returned by the parent hit-test, return HTTRANSPARENT
        // so the parent receives the follow-up NC mouse messages.
        case WM_NCHITTEST:
            parent = GetParent (hwnd);
            if (parent != nullptr)
            {
                LRESULT  hit = SendMessage (parent, uMsg, wParam, lParam);

                if (hit != HTCLIENT && hit != HTNOWHERE)
                {
                    return HTTRANSPARENT;
                }

                return hit;
            }
            return DefWindowProc (hwnd, uMsg, wParam, lParam);

        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCDESTROY:
        case WM_NCMOUSEMOVE:
            parent = GetParent (hwnd);
            if (parent != nullptr)
            {
                return SendMessage (parent, uMsg, wParam, lParam);
            }
            return DefWindowProc (hwnd, uMsg, wParam, lParam);

        default:
            return DefWindowProc (hwnd, uMsg, wParam, lParam);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnMove
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnMove (HWND hwnd, int x, int y)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);

    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNotify
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnNotify (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CreateMemoryDevices
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::CreateMemoryDevices (const MachineConfig & config)
{
    HRESULT  hr      = S_OK;
    bool     romOk   = false;

    wstring  wideError;
    string   error;



    // Load character generator ROM (used by video renderers, not on bus)
    if (!config.characterRom.resolvedPath.empty())
    {
        HRESULT hrChar = m_charRom.LoadFromFile (config.characterRom.resolvedPath);

        if (FAILED (hrChar))
        {
            DEBUGMSG (L"Failed to load character ROM '%hs', using fallback\n",
                      config.characterRom.resolvedPath.c_str());
            m_charRom.LoadEmbeddedFallback();
        }
    }
    else
    {
        m_charRom.LoadEmbeddedFallback();
    }

    // RAM regions. Skip aux-bank entries: the AppleIIeMmu owns the
    // auxiliary 64 KiB internally. Track the main RAM RamDevice for
    // MMU page-table wiring.
    for (const auto & region : config.ram)
    {
        if (!region.bank.empty())
        {
            continue;
        }

        Word start = region.address;
        Word end   = static_cast<Word> (region.address + region.size - 1);

        auto device = make_unique<RamDevice> (start, end);

        if (m_refs.mainRamDev == nullptr)
        {
            m_refs.mainRamDev = device.get();
        }

        m_memoryBus.AddDevice (device.get());
        m_ownedDevices.push_back (move (device));
    }

    // System ROM (single, file size determines end address)
    {
        Word romStart = config.systemRom.address;
        Word romEnd   = static_cast<Word> (config.systemRom.address + config.systemRom.fileSize - 1);

        auto device = RomDevice::CreateFromFile (romStart,
                                                 romEnd,
                                                 config.systemRom.resolvedPath,
                                                 error);

        romOk = (device != nullptr);

        if (!romOk)
        {
            wideError.assign (error.begin(), error.end());
            CBRN (false, wideError.c_str());
        }

        m_memoryBus.AddDevice (device.get());
        m_ownedDevices.push_back (move (device));
    }

    // Internal motherboard devices
    for (const auto & idev : config.internalDevices)
    {
        DeviceConfig devCfg;
        devCfg.type = idev.type;

        // The //e MMU is a coordinator object, not a bus device — it owns
        // the auxiliary 64 KiB and rebinds the page table on every
        // banking-changed event. Instantiate it directly here; full wiring
        // (siblings, Initialize) happens after the device pass.
        if (devCfg.type == "apple2e-mmu")
        {
            m_mmu = make_unique<AppleIIeMmu>();
            continue;
        }

        auto device = m_registry.Create (devCfg.type, devCfg, m_memoryBus);

        if (!device)
        {
            DEBUGMSG (L"Warning: Unknown device type '%hs'\n", devCfg.type.c_str());
            continue;
        }

        // Track specific device pointers for quick access
        if (devCfg.type == "apple2-keyboard" ||
            devCfg.type == "apple2e-keyboard")
        {
            m_refs.keyboard = static_cast<AppleKeyboard *> (device.get());
        }
        else if (devCfg.type == "apple2-softswitches" ||
                 devCfg.type == "apple2e-softswitches")
        {
            m_refs.softSwitches = static_cast<AppleSoftSwitchBank *> (device.get());
        }
        else if (devCfg.type == "apple2-speaker")
        {
            m_refs.speaker = static_cast<AppleSpeaker *> (device.get());
        }

        m_memoryBus.AddDevice (device.get());
        m_ownedDevices.push_back (move (device));
    }

    // Wire IIe keyboard <-> softswitch sibling so $C00C-$C00F reaches the
    // softswitch (the keyboard's range $C000-$C063 would otherwise eat it).
    {
        auto * iieKbd = dynamic_cast<AppleIIeKeyboard *>     (m_refs.keyboard);
        auto * iieSw  = dynamic_cast<AppleIIeSoftSwitchBank *> (m_refs.softSwitches);

        if (iieKbd != nullptr && iieSw != nullptr)
        {
            iieKbd->SetSoftSwitchSibling (iieSw);
            iieSw->SetKeyboard           (iieKbd);
        }

        if (iieKbd != nullptr && m_refs.speaker != nullptr)
        {
            iieKbd->SetSpeakerSibling (m_refs.speaker);
        }

        if (iieKbd != nullptr && m_mmu != nullptr)
        {
            iieKbd->SetMmu (m_mmu.get());
        }

        if (iieKbd != nullptr && m_videoTiming != nullptr)
        {
            iieKbd->SetVideoTiming (m_videoTiming.get());
        }

        if (iieSw != nullptr && m_videoTiming != nullptr)
        {
            iieSw->SetVideoTiming (m_videoTiming.get());
        }

        if (iieSw != nullptr && m_mmu != nullptr)
        {
            iieSw->SetMmu (m_mmu.get());
        }
    }

    // Initialize the //e MMU once main RAM exists. The MMU rebinds the
    // page table for $0000-$BFFF based on RAMRD/RAMWRT/ALTZP/80STORE.
    if (m_mmu != nullptr && m_refs.mainRamDev != nullptr)
    {
        auto * iieSw = dynamic_cast<AppleIIeSoftSwitchBank *> (m_refs.softSwitches);

        HRESULT hrMmu = m_mmu->Initialize (
            &m_memoryBus,
            m_refs.mainRamDev,
            nullptr,
            nullptr,
            nullptr,
            iieSw);

        if (FAILED (hrMmu))
        {
            DEBUGMSG (L"AppleIIeMmu::Initialize failed (hr=0x%08x)\n", hrMmu);
        }
    }

    // Slot devices and slot ROMs
    for (const auto & slot : config.slots)
    {
        // Slot device (e.g., disk-ii)
        if (!slot.device.empty())
        {
            DeviceConfig devCfg;
            devCfg.type    = slot.device;
            devCfg.slot    = slot.slot;
            devCfg.hasSlot = true;

            auto device = m_registry.Create (devCfg.type, devCfg, m_memoryBus);

            if (!device)
            {
                DEBUGMSG (L"Warning: Unknown slot device type '%hs'\n", devCfg.type.c_str());
            }
            else
            {
                m_memoryBus.AddDevice (device.get());
                m_ownedDevices.push_back (move (device));
            }
        }

        // Slot ROM at $Cs00-$CsFF
        if (!slot.rom.empty())
        {
            Word romStart = static_cast<Word> (0xC000 + slot.slot * 0x100);
            Word romEnd   = static_cast<Word> (romStart + slot.romSize - 1);

            auto device = RomDevice::CreateFromFile (romStart,
                                                     romEnd,
                                                     slot.resolvedRomPath,
                                                     error);

            if (device == nullptr)
            {
                wideError.assign (error.begin(), error.end());
                CBRN (false, wideError.c_str());
            }

            // On //e the AppleIIeMmu owns the $C100-$CFFF router and
            // dispatches between internal ROM and slot ROMs based on
            // INTCXROM/SLOTC3ROM/INTC8ROM. On ][/][+, the slot ROM is
            // bus-resident as before (no INTCXROM concept).
            if (m_mmu != nullptr)
            {
                vector<Byte> bytes (slot.romSize);

                for (size_t i = 0; i < slot.romSize; i++)
                {
                    bytes[i] = device->Read (static_cast<Word> (romStart + i));
                }

                m_mmu->AttachSlotRom (slot.slot, move (bytes));
            }
            else
            {
                m_memoryBus.AddDevice (device.get());
            }

            m_ownedDevices.push_back (move (device));
        }
    }

    // Cache DiskIIController pointer for the status-bar drive activity
    // indicator. We pick the first one we find (typically slot 6).
    m_refs.diskController = nullptr;
    for (auto & dev : m_ownedDevices)
    {
        DiskIIController *  dc = dynamic_cast<DiskIIController *> (dev.get());

        if (dc != nullptr)
        {
            m_refs.diskController = dc;
            break;
        }
    }

    // Drive-audio wiring (spec 005-disk-ii-audio FR-008 / FR-012 /
    // FR-015 / FR-016). Allocate one DiskIIAudioSource per drive on
    // the discovered controller (if any), register each with the
    // mixer, and route the controller's audio-sink events into
    // drive 0's source (single sink covers both drives; the head /
    // motor events themselves are not currently drive-tagged in
    // DiskIIController -- a follow-up could split per-drive sinks).
    //
    // Pan policy: single-drive profiles play centered (equal-power
    // center). Two-drive profiles place Drive 1 left-of-center and
    // Drive 2 right-of-center using kDrivePanOffset radians.
    m_diskAudioSources.clear();
    m_driveAudioMixer.UnregisterAllSources();

    if (m_refs.diskController != nullptr)
    {
        int  driveCount = DiskIIController::kDriveCount;
        int  drive      = 0;

        for (drive = 0; drive < driveCount; drive++)
        {
            auto  src = make_unique<DiskIIAudioSource>();

            if (driveCount <= 1)
            {
                src->SetPan (DriveAudioMixer::kSpeakerCenter,
                             DriveAudioMixer::kSpeakerCenter);
            }
            else if (drive == 0)
            {
                // Drive 1 (UI numbering; index 0): left bias.
                // theta measured from the right speaker per FR-012,
                // so panL = sin(theta), panR = cos(theta). At
                // theta = pi/4 + pi/8 = 3*pi/8 this is roughly
                // 0.924 L / 0.383 R -- halfway between the left
                // speaker and center.
                float  theta = DriveAudioMixer::kCenterAngle + DriveAudioMixer::kDrivePanOffset;

                src->SetPan (sinf (theta), cosf (theta));
            }
            else
            {
                // Drive 2 (UI numbering; index 1): mirror, right bias.
                float  theta = DriveAudioMixer::kCenterAngle - DriveAudioMixer::kDrivePanOffset;

                src->SetPan (sinf (theta), cosf (theta));
            }

            m_driveAudioMixer.RegisterSource (src.get());
            src->SetDriveIndex (drive);
            m_diskAudioSources.push_back (std::move (src));
        }

        if (!m_diskAudioSources.empty())
        {
            m_refs.diskController->SetAudioSink (m_diskAudioSources[0].get());
        }
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

    // Re-add slot ROM ($C100-$CFFF) if original extended below $D000.
    // $C000-$C0FF is I/O space and must not be shadowed by ROM.
    if (romStart < 0xD000)
    {
        Word   slotRomStart = static_cast<Word> (max (static_cast<int> (romStart), 0xC100));
        size_t dataOffset   = slotRomStart - romStart;
        size_t lowerSize    = 0xD000 - slotRomStart;

        UNREFERENCED_PARAMETER (dataOffset);

        vector<Byte> lowerData (lowerSize);

        for (size_t i = 0; i < lowerSize; i++)
        {
            lowerData[i] = romDevice->Read (static_cast<Word> (slotRomStart + i));
        }

        // On //e: hand to the MMU's CxxxRomRouter (audit C8 carryover).
        // On ][/][+: keep the legacy bus-resident ROM device.
        if (m_mmu != nullptr)
        {
            m_mmu->AttachInternalCxxxRom (move (lowerData));
        }
        else
        {
            auto lowerRom = RomDevice::CreateFromData (
                slotRomStart, static_cast<Word> (0xCFFF),
                lowerData.data(), lowerData.size());

            m_memoryBus.AddDevice (lowerRom.get());
            m_ownedDevices.push_back (move (lowerRom));
        }
    }

    // Bank device intercepts $D000–$FFFF, routing to LC RAM or ROM
    auto lcBank = make_unique<LanguageCardBank> (*lc);
    m_memoryBus.AddDevice (lcBank.get());
    m_ownedDevices.push_back (move (lcBank));

    // //e wiring: LC needs the MMU (for ALTZP routing) and the keyboard
    // sibling needs the LC pointer for $C011/$C012 status reads.
    if (m_mmu != nullptr)
    {
        lc->SetMmu (m_mmu.get());
    }

    auto * iieKbd = dynamic_cast<AppleIIeKeyboard *> (m_refs.keyboard);

    if (iieKbd != nullptr)
    {
        iieKbd->SetLanguageCard (lc);
    }

    auto * iieSw = dynamic_cast<AppleIIeSoftSwitchBank *> (m_refs.softSwitches);

    if (iieSw != nullptr)
    {
        iieSw->SetLanguageCard (lc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WirePageTable
//
//  Sets up the MemoryBus page table to point each $0000-$BFFF page at the
//  CPU's main RAM buffer (memory[]). This is the baseline mapping; the IIe
//  may later swap pages to aux RAM via 80STORE/PAGE2 banking.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::WirePageTable()
{
    if (!m_cpu)
    {
        return;
    }

    Byte * mainRam = const_cast<Byte *> (m_cpu->GetMemory());

    // Map all RAM pages ($0000-$BFFF) to main memory
    for (int page = 0x00; page < 0xC0; page++)
    {
        Byte * pagePtr = mainRam + (page * 0x100);
        m_memoryBus.SetReadPage  (page, pagePtr);
        m_memoryBus.SetWritePage (page, pagePtr);
    }

    // Register banking-change callback so soft switches can trigger remapping
    m_memoryBus.SetBankingChangedCallback ([this]()
    {
        RebuildBankingPages();
    });

    // Initial state
    RebuildBankingPages();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAuxRamBuffer
//
//  Returns the //e auxiliary 64 KiB buffer (owned by AppleIIeMmu) or
//  nullptr when no MMU is wired (Apple ][ / ][+).
//
////////////////////////////////////////////////////////////////////////////////

Byte * EmulatorShell::GetAuxRamBuffer()
{
    return m_mmu != nullptr ? m_mmu->GetAuxBuffer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildBankingPages
//
//  When the //e MMU is present, it owns all $0000-$BFFF page-table
//  routing (RAMRD/RAMWRT/ALTZP/80STORE+PAGE2/HIRES) and is invoked
//  directly by the soft-switch bank on every banking-changed event.
//  This shim only handles the legacy fallback where no MMU exists
//  (][/][+) — those machines never set 80STORE so all pages stay
//  bound to main RAM.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RebuildBankingPages()
{
    if (!m_cpu)
    {
        return;
    }

    if (m_mmu != nullptr)
    {
        return;
    }

    Byte * mainRam = const_cast<Byte *> (m_cpu->GetMemory());

    for (int page = 0x04; page <= 0x07; page++)
    {
        Byte * p = mainRam + (page * 0x100);
        m_memoryBus.SetReadPage  (page, p);
        m_memoryBus.SetWritePage (page, p);
    }
    for (int page = 0x20; page <= 0x3F; page++)
    {
        Byte * p = mainRam + (page * 0x100);
        m_memoryBus.SetReadPage  (page, p);
        m_memoryBus.SetWritePage (page, p);
    }
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
    HRESULT  hr = S_OK;
    string   resolvedDisk1 = disk1Path;
    string   resolvedDisk2 = disk2Path;

    // Per-machine remembered disks. Command-line wins; otherwise fall
    // back to whatever this machine had mounted last session, so the
    // user doesn't have to re-pick the .dsk every launch (and so the
    // test harness can flip-test the same image without re-clicking
    // the file dialog).
    //
    // / FR-047. If the remembered file no longer exists on disk
    // (user moved or deleted the image since last session), clear the
    // stale registry entry on the spot so we don't keep tripping over
    // it every time this machine is loaded, then leave the slot empty.
    if (resolvedDisk1.empty() && !m_currentMachineName.empty())
    {
        wstring  saved;
        HRESULT  hrRead = DiskSettings::ReadSavedDiskPath (0, m_currentMachineName, saved);

        if (hrRead == S_OK && !saved.empty())
        {
            if (fs::exists (fs::path (saved)))
            {
                resolvedDisk1 = fs::path (saved).string();
            }
            else
            {
                OutputDebugStringW (L"[EmulatorShell] FR-047: drive 0 last-mounted image missing; clearing.\n");
                HRESULT  hrClear = DiskSettings::WriteSavedDiskPath (
                    0, m_currentMachineName, wstring());
                IGNORE_RETURN_VALUE (hrClear, S_OK);
            }
        }
    }

    if (resolvedDisk2.empty() && !m_currentMachineName.empty())
    {
        wstring  saved;
        HRESULT  hrRead = DiskSettings::ReadSavedDiskPath (1, m_currentMachineName, saved);

        if (hrRead == S_OK && !saved.empty())
        {
            if (fs::exists (fs::path (saved)))
            {
                resolvedDisk2 = fs::path (saved).string();
            }
            else
            {
                OutputDebugStringW (L"[EmulatorShell] FR-047: drive 1 last-mounted image missing; clearing.\n");
                HRESULT  hrClear = DiskSettings::WriteSavedDiskPath (
                    1, m_currentMachineName, wstring());
                IGNORE_RETURN_VALUE (hrClear, S_OK);
            }
        }
    }

    if (resolvedDisk1.empty() && resolvedDisk2.empty())
    {
        return;
    }

    if (!resolvedDisk1.empty())
    {
        hr = MountDiskInSlot6 (0, resolvedDisk1);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (!resolvedDisk2.empty())
    {
        hr = MountDiskInSlot6 (1, resolvedDisk2);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSlot6Controller
//
//  / T097. Scans the owned-device list for the Disk II
//  controller. Returns nullptr if none is wired (e.g., a machine config
//  without a disk slot).
//
////////////////////////////////////////////////////////////////////////////////

DiskIIController * EmulatorShell::FindSlot6Controller()
{
    DiskIIController *  result = nullptr;



    for (auto & dev : m_ownedDevices)
    {
        result = dynamic_cast<DiskIIController *> (dev.get());

        if (result != nullptr)
        {
            break;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiskInSlot6
//
//  / T097 / FR-025. Routes the mount through the DiskImageStore
//  so dirty writes auto-flush back to the host filesystem on Eject,
//  SwitchMachine, PowerCycle, and Shutdown. The Disk II controller's
//  nibble engine is then re-pointed at the store-owned DiskImage via
//  SetExternalDisk so the controller drives the same image bytes the
//  store will eventually serialize.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::MountDiskInSlot6 (int drive, const string & path)
{
    HRESULT              hr         = S_OK;
    DiskIIController  *  controller = FindSlot6Controller();
    DiskImage         *  external   = nullptr;

    CBR (controller != nullptr);

    hr = m_diskStore.Mount (6, drive, path);
    CHR (hr);

    external = m_diskStore.GetImage (6, drive);
    controller->SetExternalDisk (drive, external);

    // Spec-006 bug 14b. The store-based mount path bypasses the
    // controller's own MountDisk method, so fire the IDiskIIEventSink
    // hook explicitly here so the debug window sees the insert. Cold
    // boot mounts still fire on the controller side (the debug window
    // is rarely open at app launch and the user wants to see the
    // mount that ran without their click); the cold-boot suppression
    // below is audio-only (FR-013).
    controller->NotifyDiskInserted (drive);

    // Persist this drive's mount path so the next launch / next time
    // this machine is selected auto-mounts the same disk. Don't pollute
    // hr with the registry result -- a missing key is non-fatal.
    if (!m_currentMachineName.empty())
    {
        wstring  wPath = fs::path (path).wstring();
        HRESULT  hrReg = DiskSettings::WriteSavedDiskPath (drive, m_currentMachineName, wPath);
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

    // Drive-audio door-close (FR-013). Cold-boot mounts (command-line,
    // last-session restoration, autoload) MUST be suppressed -- they
    // happen before the user has interacted with the running //e and
    // shouldn't audibly slam the drive door at app launch. Post-startup
    // mounts (user-initiated mid-session) always fire.
    if (!m_coldBootMountWindow &&
        drive >= 0 &&
        static_cast<size_t> (drive) < m_diskAudioSources.size() &&
        m_diskAudioSources[drive] != nullptr)
    {
        m_wasapiAudio.RecordDriveDoorSyncEvent (drive, NowMs());
        m_driveWidgets.PublishSyncEvent (drive,
                                         DriveWidgetController::SyncAction::DoorClose,
                                         NowMs());
        m_diskAudioSources[drive]->OnDiskInserted();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDiskInSlot6
//
//  / T097 / FR-025. Auto-flushes dirty bits via the store and
//  detaches the controller's external disk.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::EjectDiskInSlot6 (int drive)
{
    DiskIIController *  controller = FindSlot6Controller();



    m_diskStore.Eject (6, drive);

    if (controller != nullptr)
    {
        controller->SetExternalDisk (drive, nullptr);

        // Spec-006 bug 14b. Mirror the insert path: fire the
        // controller-level event sink so the debug window logs the
        // eject. Fires AFTER the external disk is detached so any
        // sink that inspects the controller sees the post-eject
        // state.
        controller->NotifyDiskEjected (drive);
    }

    // Clear the per-machine remembered path so the next launch comes up
    // empty in this slot.
    if (!m_currentMachineName.empty())
    {
        HRESULT  hrReg = DiskSettings::WriteSavedDiskPath (drive, m_currentMachineName, L"");
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

    // Drive-audio door-open (FR-014). Eject events always fire (no
    // cold-boot eject case in practice -- the app launches with the
    // drive bay closed).
    if (drive >= 0 &&
        static_cast<size_t> (drive) < m_diskAudioSources.size() &&
        m_diskAudioSources[drive] != nullptr)
    {
        m_wasapiAudio.RecordDriveDoorSyncEvent (drive, NowMs());
        m_driveWidgets.PublishSyncEvent (drive,
                                         DriveWidgetController::SyncAction::DoorOpen,
                                         NowMs());
        m_diskAudioSources[drive]->OnDiskEjected();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemountSlot6Disks
//
//  Re-reads every currently mounted slot-6 disk image from the host
//  filesystem so that an external regeneration of the .dsk file (e.g. a
//  developer iterating on a demo image) is picked up by the next boot.
//  Used by the Reset and Power Cycle menu commands; without this, the
//  controller keeps serving the original byte buffer the disk was
//  loaded with at mount time, and any external rewrite of the .dsk is
//  invisible until the user manually ejects and re-inserts the image.
//
//  Snapshots paths first because the re-mount path goes through Eject
//  + Mount internally, which transiently blanks the source-path slot.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RemountSlot6Disks()
{
    string   savedDisk[DiskImageStore::kDriveCount];
    HRESULT  hrMount = S_OK;
    int      drive   = 0;



    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        savedDisk[drive] = m_diskStore.GetSourcePath (6, drive);
    }

    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        if (!savedDisk[drive].empty())
        {
            hrMount = MountDiskInSlot6 (drive, savedDisk[drive]);
            IGNORE_RETURN_VALUE (hrMount, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount  (IDriveCommandSink)
//
//  Called from the UI thread by the drive widget's click-to-
//  browse path and by the drag-drop target. Routes through the existing
//  IDM_DISK_INSERT* command queue so the actual mount runs on the CPU
//  thread, mirroring the menu-driven path. Only slot 6 is supported
//  today (the integrated Disk II).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Mount (int slot, int drive, const std::wstring & path)
{
    HRESULT  hr      = S_OK;
    WORD     command = 0;



    if (slot != 6)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    if (drive == 0)
    {
        command = IDM_DISK_INSERT1;
    }
    else if (drive == 1)
    {
        command = IDM_DISK_INSERT2;
    }
    else
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    PostCommand (command, fs::path (path).string());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject  (IDriveCommandSink)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::Eject (int slot, int drive)
{
    WORD  command = 0;



    if (slot != 6)
    {
        return;
    }

    if (drive == 0)
    {
        command = IDM_DISK_EJECT1;
    }
    else if (drive == 1)
    {
        command = IDM_DISK_EJECT2;
    }
    else
    {
        return;
    }

    PostCommand (command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
//  Steady-clock millisecond timestamp used by the drive-widget door
//  animation FSM. Monotonic so an NTP step doesn't strand a half-open
//  door. Window cap is well under 2^31 so int64 is plenty.
//
////////////////////////////////////////////////////////////////////////////////

int64_t EmulatorShell::NowMs() const
{
    auto  duration = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDriveWidgets
//
//  Per-UI-frame sync from the (CPU-thread-owned) Disk II
//  controller state into the (UI-thread-only) DriveWidgetState. Reads
//  the engine's lifetime nibble counters and treats any forward
//  movement since the previous frame as "disk active" (FR-025 active
//  LED). Motor-on tracks the controller's IsMotorOn() directly --
//  that bool is sampled by the audio system the same way already
//  (no new sync primitive introduced; constitution check gate).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateDriveWidgets()
{
    DiskIIController *  controller = m_refs.diskController;
    int64_t             nowMs      = NowMs();
    std::vector<DriveWidgetController::DriveSyncEvent> syncEvents = m_driveWidgets.ConsumeSyncEvents();
    int                 drive      = 0;



    for (drive = 0; drive < static_cast<int> (m_driveWidgetState.size()); drive++)
    {
        DriveWidgetState &  st = m_driveWidgetState[drive];

        for (const auto & evt : syncEvents)
        {
            if (evt.driveId == drive)
            {
                st.lastSyncEventId = evt.eventId;
            }
        }

        // mountedImagePath -- single writer (UI thread), source of
        // truth is the DiskImageStore. Reflect door FSM transitions
        // when mount state changes.
        const string &  src    = m_diskStore.GetSourcePath (6, drive);
        wstring         wPath  (src.begin(), src.end());

        if (wPath != st.mountedImagePath)
        {
            if (wPath.empty())
            {
                st.BeginEject (nowMs);
            }
            else
            {
                st.BeginInsert (wPath, nowMs);
            }
        }

        st.TickDoorAnimation (nowMs);

        // motorOn + diskActive sampling. The controller's engine is
        // owned by the device, which the CPU thread mutates; we read
        // the bool + monotonic counters with relaxed atomics
        // semantics (existing audio-system pattern).
        bool      motorOn  = false;
        bool      active   = false;
        uint64_t  reads    = 0;
        uint64_t  writes   = 0;

        if (controller != nullptr)
        {
            auto &  engine = controller->GetEngine (drive);
            motorOn = engine.IsMotorOn();
            reads   = engine.GetReadNibbles();
            writes  = engine.GetWriteNibbles();

            if (reads != m_lastReadNibbles[drive] ||
                writes != m_lastWriteNibbles[drive])
            {
                active = true;
            }
        }

        m_lastReadNibbles[drive]  = reads;
        m_lastWriteNibbles[drive] = writes;

        st.motorOn.store    (motorOn, std::memory_order_relaxed);
        st.diskActive.store (active,  std::memory_order_relaxed);
    }

    m_driveWidgets.SyncFromStates (m_driveWidgetState);
    m_driveChrome[0].SyncFromState (m_driveWidgetState[0]);
    m_driveChrome[1].SyncFromState (m_driveWidgetState[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateVideoModes
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CreateVideoModes()
{
    auto textMode = make_unique<AppleTextMode> (m_memoryBus, m_charRom);
    m_refs.activeVideoMode = textMode.get();
    m_videoModes.push_back (move (textMode));

    auto loResMode = make_unique<AppleLoResMode> (m_memoryBus);
    m_videoModes.push_back (move (loResMode));

    auto hiResMode = make_unique<AppleHiResMode> (m_memoryBus);
    m_videoModes.push_back (move (hiResMode));

    auto doubleHiResMode = make_unique<AppleDoubleHiResMode> (m_memoryBus);
    m_videoModes.push_back (move (doubleHiResMode));

    // Index 4: 80-column text (used on //e). Wired with aux memory from
    // the AppleIIeMmu when present.
    auto text80 = make_unique<Apple80ColTextMode> (m_memoryBus, m_charRom);

    Byte * auxBuf = GetAuxRamBuffer();

    if (auxBuf != nullptr)
    {
        text80->SetAuxMemory (auxBuf);

        // DHR also needs aux memory access (FR-019). Index 3 = AppleDoubleHiResMode.
        auto * dhr = static_cast<AppleDoubleHiResMode *> (m_videoModes[3].get());
        dhr->SetAuxMemory (auxBuf);
    }

    m_videoModes.push_back (move (text80));
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

    // / FR-033 / T056. Wire the //e video timing model into the
    // EmuCpu cycle fan-out. Every AddCycles call now ticks VideoTiming
    // so $C019 (RDVBLBAR) tracks the 17,030-cycle frame. Null-safe for
    // tests/builds that haven't constructed a timing model.
    if (m_videoTiming != nullptr)
    {
        m_cpu->SetVideoTiming (m_videoTiming.get());
    }

    // Wire the InterruptController to the CPU. wiring registers
    // zero asserters today — the //e card slots (1/3/4/5/6) will allocate
    // tokens here in later phases as their devices are added. The
    // controller exists now so Apple ][ / ][+ / //e all share the same
    // IRQ aggregation seam.
    m_interruptController.SetCpu (m_cpu->GetCpu());

    // The base Cpu class uses an internal memory[] array. Copy system ROM
    // and slot ROMs into that array so PeekByte/disassembly can see them.
    {
        // System ROM
        if (!config.systemRom.resolvedPath.empty())
        {
            romFile.open (config.systemRom.resolvedPath, ios::binary);

            if (romFile.good())
            {
                addr = config.systemRom.address;

                while (romFile.good() && addr < config.systemRom.address + config.systemRom.fileSize)
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
        }

        // Slot ROMs
        for (const auto & slot : config.slots)
        {
            if (slot.rom.empty() || slot.resolvedRomPath.empty())
            {
                continue;
            }

            romFile.open (slot.resolvedRomPath, ios::binary);

            if (!romFile.good())
            {
                continue;
            }

            addr = static_cast<Word> (0xC000 + slot.slot * 0x100);

            while (romFile.good() && addr < 0xC000 + slot.slot * 0x100 + slot.romSize)
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
    }

    m_cpu->InitForEmulation();

    // Connect speaker to CPU cycle counter for audio timestamps
    if (m_refs.speaker != nullptr)
    {
        m_refs.speaker->SetCycleCounter (m_cpu->GetCycleCounterPtr());
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowMachinePicker
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowMachinePicker()
{
    // Legacy `MachinePickerDialog` is retired (FR-027). The
    // consolidated Settings panel hosts the machine selector
    // and routes the actual switch through `SwitchMachine` /
    // `SettingsPanelState::Apply` on commit. Old entry points
    // (`IDM_FILE_OPEN`, status-bar machine cell) funnel here so they
    // keep working with no behavioural surprise to the user.
    OutputDebugStringA ("[EmulatorShell] ShowMachinePicker unavailable in native-only baseline.\n");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchMachine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::SwitchMachine (const wstring & machineName)
{
    HRESULT             hr             = S_OK;
    HRESULT             hrReg          = S_OK;
    vector<fs::path>    searchPaths;
    fs::path            configRelPath;
    fs::path            configPath;
    ifstream            configFile;
    bool                configGood     = false;
    stringstream        ss;
    string              jsonText;
    vector<fs::path>    romSearchPaths;
    string              error;
    MachineConfig       newConfig;
    string              machineNameNarrow = fs::path (machineName).string();
    JsonValue           defaultJson;
    JsonValue           mergedJson;
    JsonParseError      parseErr;
    WORD                speedCmd = 0;



    // Find and load the new machine config. ROM/disk-audio asset
    // bootstrap happens on the UI thread in ShowMachinePicker before
    // the switch command is enqueued; by the time we're here, every
    // asset the new machine needs is already on disk.
    searchPaths   = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                     PathResolver::GetWorkingDirectory());
    configRelPath = fs::path ("Machines") / machineNameNarrow
                                          / (machineNameNarrow + ".json");
    configPath    = PathResolver::FindFile (searchPaths, configRelPath);

    CBRN (!configPath.empty(),
          format (L"Machine config not found: {}", machineName).c_str());

    configFile.open (configPath);
    configGood = configFile.good();
    CBRN (configGood,
          format (L"Cannot open machine config:\n{}", configPath.wstring()).c_str());

    ss << configFile.rdbuf();
    jsonText = ss.str();

    if (SUCCEEDED (JsonParser::Parse (jsonText, defaultJson, parseErr)) &&
        m_userConfigStore != nullptr)
    {
        if (SUCCEEDED (m_userConfigStore->Load (machineNameNarrow,
                                                defaultJson,
                                                m_uiFs,
                                                mergedJson)))
        {
            speedCmd = ResolveMachineSpeedCommand (mergedJson);
        }
    }

    romSearchPaths.push_back (configPath.parent_path().parent_path().parent_path());

    for (const auto & p : searchPaths)
    {
        if (p != romSearchPaths[0])
        {
            romSearchPaths.push_back (p);
        }
    }

    hr = MachineConfigLoader::Load (jsonText,
                                    machineNameNarrow,
                                    romSearchPaths,
                                    newConfig,
                                    error);
    CHRN (hr, format (L"Failed to load machine config:\n{}",
                      wstring (error.begin(), error.end())).c_str());

    // / T097 / FR-025. Auto-flush every dirty disk before
    // tearing down the previous machine so user writes survive the
    // machine switch.
    {
        HRESULT  hrFlush = m_diskStore.FlushAll();
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
    }

    // Tear down current machine. The Disk II debug dialog (if open)
    // holds a raw pointer into the old CPU's cycle counter; revoke it
    // before the CPU is reset so the dialog can't dereference dangling
    // memory between here and CreateCpu below.
    if (m_diskIIDebugDialog != nullptr)
    {
        m_diskIIDebugDialog->SetCycleCounter (nullptr);
    }

    // Tear down ALL per-machine state in one atomic move. m_refs is
    // a struct of observer pointers into the owning collections
    // (m_ownedDevices, m_videoModes); resetting it as a whole keeps
    // the "every observer must be invalidated when its owner goes
    // away" invariant from rotting as new observers are added. m_mmu
    // is a unique_ptr that survives across switches and is only
    // reassigned when the new config carries an apple2e-mmu device;
    // it must be explicitly reset here or it'll keep its stale
    // RamDevice pointer alive across a //e → ][ switch.
    m_cpu.reset();
    m_ownedDevices.clear();
    m_videoModes.clear();
    m_memoryBus = MemoryBus();
    m_refs      = {};
    m_mmu.reset();

    // Initialize with new config
    m_currentMachineName = machineName;
    m_config              = newConfig;
    m_cyclesPerFrame      = newConfig.cyclesPerFrame;

    hr = CreateMemoryDevices (newConfig);
    CHR (hr);

    // CreateMemoryDevices unregistered the old disk-audio sources and
    // built new ones. They've been registered with the mixer but no
    // sample data is loaded yet — SetMechanism is what triggers
    // LoadSamples on each registered source. Without this re-poke,
    // the new machine's drive plays in eerie silence (FR-009).
    {
        wstring   currentMechanism = m_driveAudioMixer.GetMechanism();
        HRESULT   hrMech           = m_driveAudioMixer.SetMechanism (currentMechanism);
        IGNORE_RETURN_VALUE (hrMech, S_OK);
    }

    WireLanguageCard();
    CreateVideoModes();

    hr = m_memoryBus.Validate();
    CHR (hr);

    hr = CreateCpu (newConfig);
    CHR (hr);

    WirePageTable();

    // Re-attach the new CPU's cycle counter to the debug dialog (the
    // pointer was revoked above before the old CPU was destroyed).
    if (m_diskIIDebugDialog != nullptr && m_cpu != nullptr)
    {
        m_diskIIDebugDialog->SetCycleCounter (m_cpu->GetCycleCounterPtr());
    }

    // Spec-006 bug 15. Re-wire the debug dialog onto the freshly
    // built controller + audio source. Without this the dialog goes
    // silent after a machine switch even though it's still on screen.
    AttachDebugSinksIfOpen();

    UpdateWindowTitle();

    // Save to registry (don't pollute hr with the result)
    hrReg = RegistrySettings::WriteString (kLastMachineValue, machineName);
    IGNORE_RETURN_VALUE (hrReg, S_OK);

    // / FR-034. Same cold-power-on sequence as Initialize() —
    // seed DRAM and run the 6502 /RESET sequence. Without this the
    // newly-built machine starts with a random PC into uninitialized
    // RAM. Mounts persist across the switch (they were flushed above
    // and re-mounted by the new config); aux RAM, LC RAM, and CPU
    // registers are all reseeded.
    //
    // Must run BEFORE the per-machine remount: PowerCycle ejects every
    // drive and rebinds the controller's engine to its empty internal
    // disk, which would silently throw away whatever we just mounted.
    PowerCycle();

    // Remount per-machine disks if any were saved last time this
    // machine was active. Empty paths fall through harmlessly so a
    // never-used machine won't try to mount anything.
    MountCommandLineDisks (string(), string());

    if (speedCmd != 0)
    {
        HandleCommand (speedCmd);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunMessageLoop
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::RunMessageLoop()
{
    MSG      msg = {};
    HRESULT  hr  = S_OK;



    hr = m_cpuManager.Start (
        [this] { OnCpuThreadStart(); },
        [this] (const EmulatorCommand & cmd) { DispatchCpuCommand (cmd); },
        [this] { RunOneFrame(); PublishFramebuffer(); },
        [this] { OnCpuThreadStop(); });
    CHRA (hr);

    // Cold-boot mount window is closed once the UI message loop is
    // ready to deliver user input -- any mount issued from here on
    // is treated as a real, user-initiated swap and fires the
    // drive-audio door-close (FR-013).
    m_coldBootMountWindow = false;

    // UI thread loop: process messages, present latest framebuffer with vsync
    while (m_cpuManager.IsRunning())
    {
        // Process all pending messages
        while (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_cpuManager.Stop();
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
                m_fbReady = false;

            }
        }

        // / FR-038. Push the latest CRT params (brightness slider,
        // scanlines/bloom/color-bleed toggles + magnitudes) to the
        // renderer every UI frame so user edits land on the very next
        // present. The active theme's `crtDefaults` only apply when the
        // user hasn't customised anything yet (see MakeCrtParams).
        {
            const ThemeCrtDefaults *  themeDefaults = nullptr;
            if (m_themeManager != nullptr)
            {
                const LoadedTheme *  active = m_themeManager->GetActiveTheme();
                if (active != nullptr)
                {
                    themeDefaults = &active->crtDefaults;
                }
            }
            CrtParams  params = MakeCrtParams (m_globalPrefs.crt,
                                               themeDefaults,
                                               (float) m_d3dRenderer.GetBackBufferWidth(),
                                               (float) m_d3dRenderer.GetBackBufferHeight());
            m_d3dRenderer.SetCrtParams (params);
        }

        m_d3dRenderer.UploadAndPresent (m_uiFramebuffer.data());
    }

    m_cpuManager.Stop();

Error:
    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCpuThreadStart
//
//  CPU-thread-side initialization callback invoked by CpuManager once
//  the worker thread is alive and COM is initialized. Brings up the
//  WASAPI client and seeds the drive-audio mixer with the per-machine
//  sample set so subsequent SetMechanism() switches reload from disk.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnCpuThreadStart()
{
    HRESULT  hr = S_OK;


    // Initialize WASAPI audio (non-fatal if it fails)
    hr = m_wasapiAudio.Initialize();
    IGNORE_RETURN_VALUE (hr, S_OK);

    // Drive-audio sample loading (spec 005-disk-ii-audio FR-009,
    // NFR-005, FR-019, FR-006). The mixer holds the asset-load
    // context so any later runtime mechanism switch (Options dialog,
    // ) can reload every registered source through one
    // entry point. Default mechanism is Shugart unless the
    // per-machine registry already overrode it during Initialize.
    if (m_wasapiAudio.IsInitialized() && !m_diskAudioSources.empty())
    {
        vector<fs::path>  searchPaths;
        fs::path          baseDir;
        wstring           devicesDir;

        // Use the same install-root resolution that Main.cpp /
        // AssetBootstrap used when writing the WAVs so the read
        // path agrees with the write path. Re-resolving via the
        // exe directory alone is wrong in dev builds, where the
        // exe sits under x64/Debug while the Devices/ tree lives
        // at the repo root.
        searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(), PathResolver::GetWorkingDirectory());
        baseDir     = AssetBootstrap::GetAssetBaseDirectory (searchPaths, PathResolver::GetExecutableDirectory());
        devicesDir  = (baseDir / L"Devices" / L"DiskII").wstring();

        m_driveAudioMixer.SetSampleLoadContext (devicesDir, m_wasapiAudio.GetSampleRate());

        HRESULT  hrLoad = m_driveAudioMixer.SetMechanism (m_driveAudioMixer.GetMechanism());
        IGNORE_RETURN_VALUE (hrLoad, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCpuThreadStop
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnCpuThreadStop()
{
    m_wasapiAudio.Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchCpuCommand
//
//  Single-command dispatcher invoked by CpuManager once per drained
//  EmulatorCommand. All branches run on the CPU thread, where it is
//  safe to touch CPU, bus, and device state.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DispatchCpuCommand (const EmulatorCommand & cmd)
{
    switch (cmd.id)
    {
        case IDM_FILE_OPEN:
        {
            wstring wideName (cmd.payload.begin(), cmd.payload.end());
            HRESULT hrSwitch = SwitchMachine (wideName);

            if (FAILED (hrSwitch))
            {
                DEBUGMSG (L"SwitchMachine failed: 0x%08X\n", hrSwitch);
            }
            break;
        }

        case IDM_MACHINE_RESET:
        {
            // Re-read disks from the host filesystem first so an
            // externally-regenerated .dsk (typical dev workflow:
            // hack on a demo, regenerate the disk image, hit
            // Reset) is picked up by the post-reset boot.
            RemountSlot6Disks();
            SoftReset();
            break;
        }

        case IDM_MACHINE_POWERCYCLE:
        {
            // EmulatorShell::PowerCycle preserves DiskImageStore
            // mounts but DiskIIController::PowerCycle unbinds the
            // controller's external-disk pointer (it re-points
            // each engine at its empty internal sentinel), so
            // without an explicit re-mount the drives come up
            // empty and the boot ROM has nothing to read.
            // RemountSlot6Disks both re-binds the engines AND
            // re-reads the host file (so external regenerations
            // are picked up).
            PowerCycle();
            RemountSlot6Disks();
            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (m_cpu)
            {
                m_cpu->StepOne();

                if (m_refs.diskController != nullptr)
                {
                    m_refs.diskController->Tick (m_cpu->GetLastInstructionCycles());
                }
            }
            break;
        }

        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
        {
            int      drive   = (cmd.id == IDM_DISK_INSERT1) ? 0 : 1;
            HRESULT  hrMount = S_OK;

            hrMount = MountDiskInSlot6 (drive, cmd.payload);
            IGNORE_RETURN_VALUE (hrMount, S_OK);
            break;
        }

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
        {
            int   drive = (cmd.id == IDM_DISK_EJECT1) ? 0 : 1;

            EjectDiskInSlot6 (drive);
            break;
        }

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostCommand
//
//  Thin wrapper that hands the command id and payload to the CpuManager
//  queue. Retained on EmulatorShell so call sites that already speak
//  the "post a menu id" idiom do not need to know the manager exists.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostCommand (WORD id, const string & payload)
{
    m_cpuManager.PostCommand (id, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishFramebuffer
//
//  Copies the freshly-rendered CPU framebuffer into the UI-visible
//  framebuffer under m_fbMutex so the next UI message-loop iteration
//  can pick it up and present.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PublishFramebuffer()
{
    lock_guard<mutex>  lock (m_fbMutex);

    m_uiFramebuffer = m_cpuFramebuffer;
    m_fbReady       = true;
}





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
    SpeedMode speed           = m_cpuManager.GetSpeedMode();
    bool      audioActive     = (m_refs.speaker != nullptr && m_wasapiAudio.IsInitialized());
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
        m_refs.speaker->BeginFrame();
    }

    for (uint32_t executed = 0; executed < targetCycles; )
    {
        // Feed next paste character if available
        m_clipboardManager->DrainPasteBuffer();

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

            // Pump the Disk II nibble engine in lockstep with EACH
            // instruction's cycles, not once per slice. The boot ROM
            // sits in a tight LDA $C0EC / BPL loop reading the data
            // latch; if the engine only advances at slice boundaries
            // the CPU sees one valid nibble per ~1000 cycles instead
            // of ~32, and the boot ROM never accumulates enough sync
            // bytes to find a sector header.
            if (m_refs.diskController != nullptr)
            {
                m_refs.diskController->Tick (cycles);
            }
        }

        executed += sliceActual;

        if (audioActive)
        {
            exactSamples = static_cast<double> (sliceActual) / cyclesPerSample + m_sampleRemainder;
            numSamples   = static_cast<uint32_t> (exactSamples);

            m_sampleRemainder = exactSamples - static_cast<double> (numSamples);

            m_wasapiAudio.SubmitFrame (m_refs.speaker->GetToggleTimestamps(),
                                       sliceActual,
                                       m_refs.speaker->GetFrameInitialState(),
                                       numSamples,
                                       &m_driveAudioMixer,
                                       m_cpu->GetTotalCycles());

            m_refs.speaker->ClearTimestamps();
            m_refs.speaker->BeginFrame();
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



    SelectVideoMode();

    if (m_refs.activeVideoMode != nullptr)
    {
        // Pass nullptr for videoRam so the renderer reads through MemoryBus.
        // The bus's page table reflects the current MMU banking state
        // (main vs aux for $0400-$07FF / $2000-$3FFF under 80STORE+PAGE2/HIRES);
        // CPU memory[] alone does not, since the //e MMU re-points pages at
        // the RamDevice / aux RAM buffers it owns.
        m_refs.activeVideoMode->Render (nullptr,
                                   m_cpuFramebuffer.data(),
                                   kFramebufferWidth,
                                   kFramebufferHeight);
    }

    // Mixed mode: overlay text on the bottom 4 rows (rows 20-23) via the
    // composed renderer (FR-017a / FR-020). When 80COL is active on the //e
    // we route through Apple80ColTextMode::RenderRowRange; otherwise through
    // AppleTextMode::RenderRowRange. Both share a single composed code path
    // (no branched duplicated render logic).
    if (m_mixedMode && m_graphicsMode && !m_videoModes.empty())
    {
        static constexpr int kMixedFirstRow = 20;
        static constexpr int kMixedLastRow  = 24;

        auto * iieSwitches = dynamic_cast<AppleIIeSoftSwitchBank *> (m_refs.softSwitches);
        bool   use80Col    = iieSwitches != nullptr && iieSwitches->Is80ColMode();

        if (use80Col && m_videoModes.size() > 4)
        {
            auto * text80 = static_cast<Apple80ColTextMode *> (m_videoModes[4].get());

            text80->SetPage2 (false);
            text80->RenderRowRange (kMixedFirstRow, kMixedLastRow,
                                    nullptr,
                                    m_cpuFramebuffer.data(),
                                    kFramebufferWidth,
                                    kFramebufferHeight);
        }
        else
        {
            auto * text40 = static_cast<AppleTextMode *> (m_videoModes[0].get());

            text40->SetPage2 (m_page2);
            text40->RenderRowRange (kMixedFirstRow, kMixedLastRow,
                                    nullptr,
                                    m_cpuFramebuffer.data(),
                                    kFramebufferWidth,
                                    kFramebufferHeight);
        }
    }

    // Apply monochrome tint via Video/MonochromeTint.h helpers (kept
    // out-of-line in CassoEmuCore so the BGRA arithmetic is unit-
    // testable independent of the Win32 shell).
    if (color != ColorMode::Color)
    {
        for (auto & pixel : m_cpuFramebuffer)
        {
            switch (color)
            {
                case ColorMode::GreenMono:
                    pixel = Casso::Video::TintGreenMono (pixel);
                    break;

                case ColorMode::AmberMono:
                    pixel = Casso::Video::TintAmberMono (pixel);
                    break;

                case ColorMode::WhiteMono:
                    pixel = Casso::Video::TintWhiteMono (pixel);
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
//  HandleCommand
//
//  Public command-pump entry point. Used by the NavLayer so
//  click routing from the chrome funnels through the same
//  dispatch path as a Win32 menu pick. Intentionally a thin wrapper —
//  OnCommand owns the real id-range demux.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleCommand (WORD commandId)
{
    OnCommand (m_hwnd, (int) commandId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnCommand (HWND hwnd, int id)
{
    UNREFERENCED_PARAMETER (hwnd);

    if      (id >= IDM_EDIT_COPY_TEXT && id <= IDM_EDIT_PASTE)       { OnEditCommand (id); }
    else if (id >= IDM_FILE_OPEN     && id <= IDM_FILE_EXIT)          { OnFileCommand (id); }
    else if (id >= IDM_MACHINE_RESET && id <= IDM_MACHINE_INFO)       { OnMachineCommand (id); }
    else if (id >= IDM_DISK_INSERT1  && id <= IDM_DISK_WRITEPROTECT2) { OnDiskCommand (id); }
    else if (id >= IDM_VIEW_COLOR    && id <= IDM_VIEW_DISKII_DEBUG)  { OnViewCommand (id); }
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



    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());

    // P6 -- revoke the IDropTarget before the HWND is destroyed.
    // RevokeDragDrop requires a valid window handle.
    m_dragDropTarget.Shutdown();

    m_cpuManager.Stop();
    PostQuitMessage (0);
    
    return false;
}









////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    int   x        = ((int) (short) LOWORD (lParam));
    int   y        = ((int) (short) HIWORD (lParam));
    bool  leftDown = (wParam & MK_LBUTTON) != 0;



    m_uiShell.OnMouseMove (x, y, leftDown);
    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    int  x = ((int) (short) LOWORD (lParam));
    int  y = ((int) (short) HIWORD (lParam));



    UNREFERENCED_PARAMETER (wParam);

    SetCapture (m_hwnd);
    m_uiShell.OnLButtonDown (x, y);
    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    int                x      = ((int) (short) LOWORD (lParam));
    int                y      = ((int) (short) HIWORD (lParam));
    DriveWidgetRegion  region = DriveWidgetRegion::None;



    UNREFERENCED_PARAMETER (wParam);

    ReleaseCapture();
    m_uiShell.OnLButtonUp (x, y);

    for (DriveWidget & drive : m_driveChrome)
    {
        region = drive.HitTest (x, y);
        if (region == DriveWidgetRegion::Body)
        {
            HRESULT  hrBrowse = PromptForDiskImage (drive.Drive());
            IGNORE_RETURN_VALUE (hrBrowse, S_OK);
            return false;
        }

        if (region == DriveWidgetRegion::Eject)
        {
            Eject (6, drive.Drive());
            return false;
        }
    }

    return false;
}////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    bool ctrlHeld = false;
    bool altHeld  = false;

    if (m_refs.keyboard == nullptr)
    {
        return false;
    }

    ctrlHeld = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    altHeld  = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    if (m_uiShell.HandleKey (vk))
    {
        return false;
    }

    if (altHeld && vk >= 0x20 && vk <= 0x7E && m_navLayer.HandleAltKey ((wchar_t) vk))
    {
        return false;
    }

    // Ctrl+V — paste from clipboard (host-meta convenience, not a //e
    // hardware key). Consumed before reaching the emulated keyboard.
    if (vk == 'V' && ctrlHeld && !altHeld)
    {
        m_clipboardManager->PasteFromClipboard (m_hwnd);
        return false;
    }

    // Ctrl+R — //e Reset key + Ctrl modifier. Drives the CPU /RESET
    // line via the existing soft-reset path. The emulated keyboard's
    // Open Apple state (set by the Alt-key handlers below) is what the
    // firmware reads at $C061 to decide warm vs autoboot, so:
    //   Ctrl+R         -> warm reset (no Open Apple)         -> ] prompt
    //   Ctrl+Alt+R     -> Open Apple held during reset       -> autoboot
    //   Ctrl+Shift+R   -> stays a host-meta IDM_MACHINE_POWERCYCLE
    //                     accelerator (full DRAM re-seed, no //e equiv).
    // Consumed; not pumped to the //e keyboard as a Ctrl-R character.
    if (vk == 'R' && ctrlHeld && !(GetKeyState (VK_SHIFT) & 0x8000))
    {
        PostCommand (IDM_MACHINE_RESET);
        return false;
    }

    m_refs.keyboard->SetKeyDown (true);

    // / T063 / FR-013. //e modifier-key wiring (host -> emulator):
    //   left  Alt   -> Open Apple   ($C061)
    //   right Alt   -> Closed Apple ($C062)
    //   Shift       -> Shift        ($C063)
    // Ignored on ][/][+ (the dynamic_cast yields nullptr). Both VK_MENU
    // (which Windows delivers for plain Alt) and VK_L/RMENU (which it
    // can deliver for some Alt+key combos) drive the same path —
    // GetKeyState gives the canonical left/right state.
    {
        auto * iieKbd = dynamic_cast<AppleIIeKeyboard *> (m_refs.keyboard);

        if (iieKbd != nullptr)
        {
            if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU)
            {
                bool lAlt = (GetKeyState (VK_LMENU) & 0x8000) != 0;
                bool rAlt = (GetKeyState (VK_RMENU) & 0x8000) != 0;
                iieKbd->SetOpenApple   (lAlt);
                iieKbd->SetClosedApple (rAlt);
            }
            else if (vk == VK_SHIFT)
            {
                iieKbd->SetShift (true);
            }
        }
    }

    switch (vk)
    {
        case VK_LEFT:
            m_refs.keyboard->KeyPress (kAppleKeyLeft);
            break;
            
        case VK_RIGHT:
            m_refs.keyboard->KeyPress (kAppleKeyRight);
            break;

        case VK_UP:
            m_refs.keyboard->KeyPress (kAppleKeyUp);
            break;

        case VK_DOWN:
            m_refs.keyboard->KeyPress (kAppleKeyDown);
            break;
            
        case VK_ESCAPE:
            m_refs.keyboard->KeyPress (kAppleKeyEscape);
            break;

        case VK_DELETE:
            m_refs.keyboard->KeyPress (kAppleKeyDelete);
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
    UNREFERENCED_PARAMETER (lParam);

    if (m_refs.keyboard == nullptr)
    {
        return false;
    }

    m_refs.keyboard->SetKeyDown (false);

    // / T063: release //e modifiers when the host releases the
    // physical key. Both VK_MENU and VK_L/RMENU events drive a re-query
    // of the canonical left/right state via GetKeyState — the modifier
    // remains asserted on the //e side as long as either physical Alt
    // is still down.
    auto * iieKbd = dynamic_cast<AppleIIeKeyboard *> (m_refs.keyboard);

    if (iieKbd != nullptr)
    {
        if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU)
        {
            bool lAlt = (GetKeyState (VK_LMENU) & 0x8000) != 0;
            bool rAlt = (GetKeyState (VK_RMENU) & 0x8000) != 0;
            iieKbd->SetOpenApple   (lAlt);
            iieKbd->SetClosedApple (rAlt);
        }
        else if (vk == VK_SHIFT)
        {
            iieKbd->SetShift (false);
        }
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

    if (m_refs.keyboard == nullptr)
    {
        return false;
    }

    if (ch >= 1 && ch <= 127)
    {
        m_refs.keyboard->KeyPress (static_cast<Byte> (ch));
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
    int       renderH   = static_cast<int> (height);
    HRESULT   hrPresent = S_OK;



    UNREFERENCED_PARAMETER (hwnd);

    if (m_renderHwnd != nullptr)
    {
        MoveWindow (m_renderHwnd, 0, 0,
                    static_cast<int> (width), renderH, FALSE);
    }

    m_d3dRenderer.Resize (static_cast<int> (width), renderH);

    {
        UINT     dpi   = GetDpiForWindow (m_hwnd);
        HRESULT  hrUiR = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                                             m_d3dRenderer.GetBackBufferHeight(),
                                             dpi);
        IGNORE_RETURN_VALUE (hrUiR, S_OK);
        m_titleBar.UpdateGeometry (static_cast<int> (width), dpi);
        m_navLayer.Layout (0, m_titleBar.GetTitleHeight(), static_cast<int> (width), dpi);
        m_driveChrome[0].Layout (24, ComputeChromeTopInsetPx (dpi) + 18, dpi);
        m_driveChrome[1].Layout (230, ComputeChromeTopInsetPx (dpi) + 18, dpi);
        m_uiShell.HitTest().Clear();
        m_uiShell.HitTest().Register (HitRect { m_driveChrome[0].BodyRect(), HitSlot::Custom, 0 });
        m_uiShell.HitTest().Register (HitRect { m_driveChrome[1].BodyRect(), HitSlot::Custom, 1 });
        m_d3dRenderer.SetTopInsetPx (ComputeChromeTopInsetPx (dpi));
    }

    {
        lock_guard<mutex> lock (m_fbMutex);

        if (!m_uiFramebuffer.empty())
        {
            const ThemeCrtDefaults *  themeDefaults = nullptr;
            const LoadedTheme       *  active       = nullptr;
            CrtParams                  params       = {};

            if (m_themeManager != nullptr)
            {
                active = m_themeManager->GetActiveTheme();
                if (active != nullptr)
                {
                    themeDefaults = &active->crtDefaults;
                }
            }

            params = MakeCrtParams (m_globalPrefs.crt,
                                    themeDefaults,
                                    (float) m_d3dRenderer.GetBackBufferWidth(),
                                    (float) m_d3dRenderer.GetBackBufferHeight());
            m_d3dRenderer.SetCrtParams (params);

            IGNORE_RETURN_VALUE (hrPresent, m_d3dRenderer.UploadAndPresent (m_uiFramebuffer.data()));
        }
    }

    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());
    return false;
}



////////////////////////////////////////////////////////////////////////////////
//
//  OnDrawItem
//
//  Owner-draw dispatch for status-bar drive indicators. The status bar
//  forwards WM_DRAWITEM to its parent (this window) for any part marked
//  SBT_OWNERDRAW. itemID is the part index, itemData is the lParam we
//  passed to SB_SETTEXT (the drive index).
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnDrawItem (HWND hwnd, int idCtl, DRAWITEMSTRUCT * pdis)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (idCtl);
    UNREFERENCED_PARAMETER (pdis);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnTimer
//
//  Periodic refresh of the drive-activity indicators. Motor on/off and
//  drive-select events are bursty (millisecond timescales); a 50 ms
//  refresh is plenty for visible activity feedback without consuming
//  noticeable UI cycles.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnTimer (HWND hwnd, UINT_PTR timerId)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (timerId);

    return true;
}//  OnFileCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileCommand (int id)
{
    switch (id)
    {
        case IDM_FILE_OPEN:
        {
            ShowMachinePicker();
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
//  OnEditCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnEditCommand (int id)
{
    switch (id)
    {
        case IDM_EDIT_COPY_TEXT:
        {
            m_clipboardManager->CopyScreenText (m_hwnd);
            break;
        }

        case IDM_EDIT_COPY_SCREENSHOT:
        {
            m_clipboardManager->CopyScreenshot (m_hwnd);
            break;
        }

        case IDM_EDIT_PASTE:
        {
            m_clipboardManager->PasteFromClipboard (m_hwnd);
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
            paused = m_cpuManager.TogglePaused();
            m_menuSystem.SetPaused (paused);
            UpdateWindowTitle();
            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (m_cpuManager.IsPaused())
            {
                PostCommand (static_cast<WORD> (id));
            }
            break;
        }

        case IDM_MACHINE_SPEED_1X:
        {
            m_cpuManager.SetSpeedMode (SpeedMode::Authentic);
            m_menuSystem.SetSpeedMode (SpeedMode::Authentic);
            break;
        }

        case IDM_MACHINE_SPEED_2X:
        {
            m_cpuManager.SetSpeedMode (SpeedMode::Double);
            m_menuSystem.SetSpeedMode (SpeedMode::Double);
            break;
        }

        case IDM_MACHINE_SPEED_MAX:
        {
            m_cpuManager.SetSpeedMode (SpeedMode::Maximum);
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
                (m_config.ram.size() + 1 + m_config.slots.size()),
                (m_config.internalDevices.size() + m_config.slots.size()));

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

                rc    = { 0, 0,
                          kFramebufferWidth * scale,
                          kFramebufferHeight * scale + ComputeChromeTopInsetPx (dpi) };
                style = static_cast<DWORD> (GetWindowLong (m_hwnd, GWL_STYLE));
                AdjustWindowRectExForDpi (&rc, style, FALSE, 0, dpi);

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

        case IDM_VIEW_DISKII_DEBUG:
        {
            OpenDiskIIDebugDialog();
            break;
        }

        case IDM_VIEW_SETTINGS:
        {
            ShowMachinePicker();
            break;
        }
    }
}









////////////////////////////////////////////////////////////////////////////////
//
//  PromptForDiskImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::PromptForDiskImage (int drive)
{
    HRESULT                         hr         = S_OK;
    ComPtr<IFileOpenDialog>         dialog;
    ComPtr<IShellItem>              item;
    PWSTR                           pszPath    = nullptr;
    COMDLG_FILTERSPEC               filters[1] = { { L"Disk Images", L"*.dsk;*.nib;*.woz;*.po" } };



    hr = CoCreateInstance (CLSID_FileOpenDialog,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes (1, filters);
    CHR (hr);

    hr = dialog->Show (m_hwnd);
    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        hr = S_FALSE;
        goto Error;
    }
    CHR (hr);

    hr = dialog->GetResult (&item);
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_FILESYSPATH, &pszPath);
    CHR (hr);

    hr = Mount (6, drive, pszPath);
    CHR (hr);

Error:
    if (pszPath != nullptr)
    {
        CoTaskMemFree (pszPath);
    }

    return hr;
}////////////////////////////////////////////////////////////////////////////////
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
                            (m_config.internalDevices.size() + m_config.slots.size())));
                }
            }
            break;
        }

        case IDM_HELP_KEYMAP:
        {
            MessageBoxW (m_hwnd,
                L"PC Key Mapping:\n\n"
                L"Arrow Keys -> Apple ][ cursor movement\n"
                L"Enter -> Return\n"
                L"Escape -> Escape\n"
                L"Delete -> Delete\n"
                L"Ctrl+Reset -> Warm Reset\n"
                L"Left Alt -> Open Apple (//e)\n"
                L"Right Alt -> Closed Apple (//e)\n\n"
                L"Emulator Controls:\n"
                L"Ctrl+R -> Reset\n"
                L"Ctrl+Alt+R -> Autoboot Reset (cold boot from disk)\n"
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
                L"Casso Apple ][ Emulator\n"
                L"Version " _CRT_WIDE (VERSION_STRING) L"\n"
                L"Built " _CRT_WIDE (VERSION_BUILD_TIMESTAMP) L"\n\n"
                L"An Apple ][ / ][+ / //e platform emulator built on\n"
                L"the Casso 6502 assembler/emulator project.\n\n"
                L"https://github.com/relmer/Casso",
                L"About Casso", MB_ICONINFORMATION | MB_OK);
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

    title = L"Casso " _CRT_WIDE (VERSION_STRING);

    if (!m_config.name.empty())
    {
        title += L' ';
        title += s_kchEmDash;
        title += L' ';

        // Convert machine name to wide string
        wideName = fs::path (m_config.name).wstring();
        title += wideName;
    }

    if (m_cpuManager.IsPaused())
    {
        title += L" [Paused]";
    }
    else if (m_cpuManager.IsRunning())
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
    if (m_refs.softSwitches)
    {
        m_graphicsMode = m_refs.softSwitches->IsGraphicsMode();
        m_mixedMode    = m_refs.softSwitches->IsMixedMode();
        m_page2        = m_refs.softSwitches->IsPage2();
        m_hiresMode    = m_refs.softSwitches->IsHiresMode();
    }

    // When 80STORE is active on the //e, $C054/$C055 control aux/main memory
    // selection — not page 1/page 2. Suppress page2 for video rendering.
    auto * iieSoftSwitches = dynamic_cast<AppleIIeSoftSwitchBank *> (m_refs.softSwitches);

    if (iieSoftSwitches != nullptr && iieSoftSwitches->Is80Store())
    {
        m_page2 = false;
    }

    bool is80ColMode  = iieSoftSwitches != nullptr && iieSoftSwitches->Is80ColMode();
    bool altCharSet   = iieSoftSwitches != nullptr && iieSoftSwitches->IsAltCharSet();

    // Select video mode based on soft switch state
    if (!m_graphicsMode)
    {
        // Text mode: use 80-col on //e if enabled, else 40-col
        if (is80ColMode && m_videoModes.size() > 4)
        {
            m_refs.activeVideoMode = m_videoModes[4].get();
        }
        else
        {
            m_refs.activeVideoMode = m_videoModes[0].get();
        }
    }
    else if (!m_hiresMode)
    {
        // Lo-res graphics
        m_refs.activeVideoMode = m_videoModes[1].get();
    }
    else
    {
        // Hi-res graphics — use DHR (index 3) when DHIRES + 80COL are
        // both active on the //e (FR-019, audit M8). Otherwise standard
        // hi-res (index 2).
        bool useDhr = iieSoftSwitches != nullptr
                   && iieSoftSwitches->IsDoubleHiRes()
                   && is80ColMode
                   && m_videoModes.size() > 3;

        if (useDhr)
        {
            m_refs.activeVideoMode = m_videoModes[3].get();
        }
        else
        {
            m_refs.activeVideoMode = m_videoModes[2].get();
        }
    }

    // Pass page2 state to the active renderer
    if (m_refs.activeVideoMode != nullptr)
    {
        m_refs.activeVideoMode->SetPage2 (m_page2);
    }

    // Keep text mode page2-aware for mixed-mode overlay rendering
    m_videoModes[0]->SetPage2 (m_page2);

    // Propagate ALTCHARSET to both text-mode renderers (audit M13 closure).
    static_cast<AppleTextMode *> (m_videoModes[0].get())->SetAltCharSet (altCharSet);

    if (m_videoModes.size() > 4)
    {
        static_cast<Apple80ColTextMode *> (m_videoModes[4].get())->SetAltCharSet (altCharSet);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  / FR-034. Drives the //e /RESET path: every device clears its
//  reset-sensitive state (audit S10 [CRITICAL] - 80COL/ALTCHARSET no
//  longer survive), the MMU returns to the post-reset banking flags, and
//  the CPU re-loads PC from $FFFC. User RAM is preserved.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SoftReset()
{
    m_memoryBus.SoftResetAll();

    if (m_mmu != nullptr)
    {
        m_mmu->OnSoftReset();
    }

    m_interruptController.SoftReset();

    if (m_videoTiming != nullptr)
    {
        m_videoTiming->SoftReset();
    }

    if (m_cpu != nullptr)
    {
        m_cpu->SoftReset();
    }

    // Spec-006 / FR-004a. Re-zero the Disk II Debug Uptime column on
    // every reset so the user sees a clean 00:00 anchor after each
    // Ctrl+R / Ctrl+Shift+R.
    ResetUptimeAnchor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  / FR-035. Reseeds every DRAM-owning device from m_prng then
//  runs the SoftReset sequence. The Prng is constructed once (host
//  process lifetime) so consecutive cycles within a single session
//  continue producing fresh patterns rather than repeating the seed.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PowerCycle()
{
    HRESULT  hrFlush = S_OK;



    if (m_prng == nullptr)
    {
        return;
    }

    // / T097 / FR-025 / FR-035. Auto-flush dirty disks before
    // reseeding device state so writes don't get lost across a power
    // cycle. Mounts persist (matches DiskImageStore::SoftReset semantics
    // — see comment block on DiskImageStore::PowerCycle, which is the
    // unmount-everything variant tests can opt into directly).
    hrFlush = m_diskStore.FlushAll();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_memoryBus.PowerCycleAll (*m_prng);

    if (m_mmu != nullptr)
    {
        m_mmu->OnPowerCycle (*m_prng);
    }

    m_interruptController.PowerCycle();

    if (m_videoTiming != nullptr)
    {
        m_videoTiming->PowerCycle (*m_prng);
    }

    if (m_cpu != nullptr)
    {
        m_cpu->PowerCycle (*m_prng);
    }

    // Spec-006 / FR-004a. Re-zero the Disk II Debug Uptime column on
    // every power-cycle as well as soft-reset.
    ResetUptimeAnchor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDiskIIDebugDialog
//
//  Spec-006 / FR-001 / FR-017 / FR-024. View -> Disk II Debug...
//  command handler and Ctrl+Shift+D accelerator target. Lazy-creates
//  the modeless dialog on first open, wires it as the controller's
//  event sink AND as the active DiskIIAudioSource's audio-event
//  sink, applies the uptime anchor and the multi-controller title
//  hint, then shows + foregrounds the window. Subsequent calls
//  short-circuit to Show + SetForegroundWindow.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenDiskIIDebugDialog()
{
    HRESULT             hr           = S_OK;
    DiskIIController *  controller   = nullptr;
    int                 diskIICount  = 0;
    HINSTANCE           hInstance    = nullptr;
    size_t              i            = 0;



    controller = FindSlot6Controller();

    // FR-001a should have grayed the menu item; the accelerator
    // bypasses that gate so we defend in depth.
    CBR (controller != nullptr);

    for (const SlotConfig & slot : m_config.slots)
    {
        if (slot.device == "disk-ii")
        {
            diskIICount++;
        }
    }

    if (m_diskIIDebugDialog == nullptr)
    {
        m_diskIIDebugDialog = std::make_unique<DiskIIDebugDialog>();

        hInstance = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));

        hr = m_diskIIDebugDialog->Create (hInstance, m_hwnd);
        CHRF (hr, m_diskIIDebugDialog.reset());

        m_diskIIDebugDialog->SetUptimeAnchor (m_uptimeAnchor);
        m_diskIIDebugDialog->SetMultiControllerHint (diskIICount > 1);

        if (m_cpu != nullptr)
        {
            m_diskIIDebugDialog->SetCycleCounter (m_cpu->GetCycleCounterPtr());
        }

        // FR-024: both sinks attached together, dialog implements
        // both interfaces. Audio sink is a no-op if the mixer has no
        // source registered (e.g., audio subsystem disabled).
        controller->SetEventSink (m_diskIIDebugDialog.get());

        for (i = 0; i < m_diskAudioSources.size(); i++)
        {
            if (m_diskAudioSources[i] != nullptr)
            {
                m_diskAudioSources[i]->SetAudioEventSink (m_diskIIDebugDialog.get());
            }
        }
    }
    else
    {
        m_diskIIDebugDialog->SetMultiControllerHint (diskIICount > 1);
    }

    m_diskIIDebugDialog->Show();
    SetForegroundWindow (m_diskIIDebugDialog->GetHwnd());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachDebugSinksIfOpen
//
//  Spec-006 bug 15. SwitchMachine tears down the old controller and
//  audio source then constructs new ones via CreateMemoryDevices,
//  but the dialog's sink wiring only ran inside OpenDiskIIDebugDialog
//  on first open -- the new controller starts with m_eventSink ==
//  nullptr and the new audio source with m_audioEventSink == nullptr,
//  so the debug window goes silent post-switch. Re-attach both
//  sinks if the dialog is still open. No-op when the dialog has
//  never been opened.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AttachDebugSinksIfOpen()
{
    HRESULT             hr         = S_OK;
    DiskIIController *  controller = nullptr;
    size_t              i          = 0;

    CBR (m_diskIIDebugDialog != nullptr);

    controller = FindSlot6Controller();

    if (controller != nullptr)
    {
        controller->SetEventSink (m_diskIIDebugDialog.get());
    }

    for (i = 0; i < m_diskAudioSources.size(); i++)
    {
        if (m_diskAudioSources[i] != nullptr)
        {
            m_diskAudioSources[i]->SetAudioEventSink (m_diskIIDebugDialog.get());
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnInitMenuPopup
//
//  Spec-006 / FR-001a. Re-evaluate menu items whose enabled state
//  depends on runtime machine config (currently just
//  IDM_VIEW_DISKII_DEBUG). Win32 fires this every time the user
//  opens a popup, so a SwitchMachine between menu opens picks up
//  the controller delta on the next click.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnInitMenuPopup (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (hMenu);
    UNREFERENCED_PARAMETER (itemIndex);
    UNREFERENCED_PARAMETER (isWindowMenu);

    m_menuSystem.UpdateDynamicMenuItems (m_config);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcCalcSize
//
//  Collapse the entire non-client area into the client rect so
//  the chrome we draw owns every pixel. When wParam is TRUE we return
//  0 with NCCALCSIZE_PARAMS.rgrc[0] untouched, telling Windows that
//  the proposed window rect IS the new client rect.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnNcCalcSize (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outResult)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (lParam);

    if (wParam == FALSE)
    {
        outResult = 0;
        return false;
    }

    // Keep rgrc[0] as-is: client area == full window rect. Windows
    // still respects WS_THICKFRAME for the resize cursor.
    outResult = 0;
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcHitTest
//
//  Routes through the unit-tested pure-logic helper. Pull the current
//  title-bar geometry from the cached layout (kept in sync by
//  TitleBar::UpdateGeometry in OnSize).
//
////////////////////////////////////////////////////////////////////////////////

LRESULT EmulatorShell::OnNcHitTest (HWND hwnd, int xScreen, int yScreen)
{
    static constexpr int  s_kMinResizeBorderPx = 8;

    POINT                 pt   = { xScreen, yScreen };
    RECT                  rcClient = {};
    RECT                  rcTitle  = {};
    RECT                  rcMin    = {};
    RECT                  rcMax    = {};
    RECT                  rcClose  = {};
    TitleBarHitTestInput  in       = {};
    LRESULT               result   = HTNOWHERE;
    UINT                  dpi      = 0;
    int                   framePx  = 0;
    int                   padPx    = 0;
    int                   borderPx = 0;



    if (!ScreenToClient (hwnd, &pt))
    {
        return HTNOWHERE;
    }

    if (!GetClientRect (hwnd, &rcClient))
    {
        return HTNOWHERE;
    }

    rcTitle = m_titleBar.GetTitleBarRect();
    rcMin   = m_titleBar.GetButtonRect (SystemButton::Minimize);
    rcMax   = m_titleBar.GetButtonRect (SystemButton::Maximize);
    rcClose = m_titleBar.GetButtonRect (SystemButton::Close);

    dpi      = GetDpiForWindow (hwnd);
    framePx  = GetSystemMetricsForDpi (SM_CXSIZEFRAME, dpi);
    padPx    = GetSystemMetricsForDpi (SM_CXPADDEDBORDER, dpi);
    borderPx = framePx + padPx;
    if (borderPx < s_kMinResizeBorderPx)
    {
        borderPx = s_kMinResizeBorderPx;
    }

    in.clientWidth   = rcClient.right - rcClient.left;
    in.clientHeight  = rcClient.bottom - rcClient.top;
    in.mouseX        = pt.x;
    in.mouseY        = pt.y;
    in.titleLeft     = rcTitle.left;
    in.titleTop      = rcTitle.top;
    in.titleRight    = rcTitle.right;
    in.titleBottom   = rcTitle.bottom;
    in.minLeft       = rcMin.left;     in.minTop    = rcMin.top;
    in.minRight      = rcMin.right;    in.minBottom = rcMin.bottom;
    in.maxLeft       = rcMax.left;     in.maxTop    = rcMax.top;
    in.maxRight      = rcMax.right;    in.maxBottom = rcMax.bottom;
    in.closeLeft     = rcClose.left;   in.closeTop  = rcClose.top;
    in.closeRight    = rcClose.right;  in.closeBottom = rcClose.bottom;
    in.resizeBorderPx = borderPx;

    result = TitleBarHitTest::Test (in);

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonUp
//
//  Dispatch system-button clicks. HTCLOSE → WM_CLOSE,
//  HTMINBUTTON → minimize, HTMAXBUTTON → toggle maximize. Everything
//  else falls through to DefWindowProc so caption double-clicks,
//  system-menu, snap layouts, etc. all keep working.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnNcLButtonUp (HWND hwnd, LRESULT hitTest, int xScreen, int yScreen)
{
    UNREFERENCED_PARAMETER (xScreen);
    UNREFERENCED_PARAMETER (yScreen);

    WINDOWPLACEMENT  wp = { sizeof (wp) };


    switch (hitTest)
    {
        case HTCLOSE:
            PostMessage (hwnd, WM_CLOSE, 0, 0);
            return true;

        case HTMINBUTTON:
            ShowWindow (hwnd, SW_MINIMIZE);
            return true;

        case HTMAXBUTTON:
            if (GetWindowPlacement (hwnd, &wp))
            {
                ShowWindow (hwnd,
                            (wp.showCmd == SW_MAXIMIZE) ? SW_RESTORE : SW_MAXIMIZE);
            }
            return true;
    }

    return false;
}






