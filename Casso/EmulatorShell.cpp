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
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shcore.lib")

// Embed Common Controls v6 dependency in the binary's activation
// context. Without this, SetWindowSubclass / TOOLINFO / and a host
// of other modern comctl32 APIs fall back to no-op v5 behavior --
// in particular our drive-bay tooltips never appear because
// SetWindowSubclass returns FALSE silently.
#pragma comment(linker, "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int     kFramebufferWidth       = ChromeMetrics::kFramebufferWidthPx;
static constexpr int     kFramebufferHeight      = ChromeMetrics::kFramebufferHeightPx;
static constexpr LPCWSTR kWindowClass           = L"CassoWindow";
static constexpr int     s_kBaseDpi             = ChromeMetrics::kBaseDpi;
static constexpr int     s_kDriveWidgetGapDp    = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  Window placement helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    void LayoutDriveWidgetsInCommandBar (
        std::array<DriveWidget, 2>  & driveChrome,
        const LayoutManagerResult    & layout,
        int                           clientW,
        int                           clientH,
        UINT                          dpi)
    {
        int     bottomInset   = layout.bottomInsetPx;
        int     commandBarTop = std::max (0, clientH - bottomInset);
        int     commandBarH   = std::max (0, clientH - commandBarTop);
        int     gap           = MulDiv (s_kDriveWidgetGapDp, static_cast<int> (dpi), s_kBaseDpi);
        RECT    probe         = {};
        int     widgetW       = 0;
        int     widgetH       = 0;
        int     totalW        = 0;
        int     x             = 0;
        int     y             = 0;
        size_t  i             = 0;



        driveChrome[0].Layout (0, 0, dpi);
        probe   = driveChrome[0].BodyRect();
        widgetW = probe.right  - probe.left;
        widgetH = probe.bottom - probe.top;
        totalW  = widgetW * static_cast<int> (driveChrome.size()) + gap * (static_cast<int> (driveChrome.size()) - 1);
        x       = std::max (0, (clientW - totalW) / 2);
        y       = commandBarTop + (commandBarH - widgetH) / 2;

        for (i = 0; i < driveChrome.size(); i++)
        {
            int  widgetX       = x + static_cast<int> (i) * (widgetW + gap);
            int  widgetCenterX = widgetX + widgetW / 2;
            int  vanishingX    = clientW / 2;
            // Shrink factor matches the case-top depth ratio (back
            // edge is ~20% narrower than the front, so back center
            // shifts ~20% of the way toward the shared vanishing
            // point). Numerator chosen to match s_kCaseBackInsetPx
            // ratio in DriveWidget.cpp.
            int  skewPx        = MulDiv (vanishingX - widgetCenterX, 27, 100);

            driveChrome[i].SetPerspectiveSkewPx (skewPx);
            driveChrome[i].Layout (widgetX, y, dpi);
        }
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


    // Loads an HICON resource into a CPU-side premultiplied BGRA8
    // pixel buffer suitable for the DwriteTextRenderer::DrawIconBitmap
    // path. Uses a GDI memory DC + 32-bit DIB section to capture the
    // icon's alpha-channelled pixels (LoadImageW preserves alpha when
    // LR_DEFAULTCOLOR is set on a Vista+ icon). Premultiplies the
    // pixels in place because D2D's DrawBitmap expects premultiplied
    // sources.
    bool LoadIconAsPremulBgra (
        HINSTANCE             hInstance,
        int                   iconResourceId,
        int                   sizePx,
        std::vector<uint32_t> & outPixels,
        int                  & outW,
        int                  & outH)
    {
        HICON       hIcon       = nullptr;
        HDC         screenDc    = nullptr;
        HDC         memDc       = nullptr;
        HBITMAP     dib         = nullptr;
        HBITMAP     oldBitmap   = nullptr;
        void      * dibBits     = nullptr;
        BITMAPINFO  bmi         = {};
        bool        success     = false;
        size_t      pixelCount  = (size_t) sizePx * (size_t) sizePx;



        hIcon = (HICON) LoadImageW (hInstance,
                                    MAKEINTRESOURCEW (iconResourceId),
                                    IMAGE_ICON,
                                    sizePx, sizePx,
                                    LR_DEFAULTCOLOR);
        if (hIcon == nullptr)
        {
            return false;
        }

        screenDc = GetDC (nullptr);
        memDc    = CreateCompatibleDC (screenDc);

        bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = sizePx;
        bmi.bmiHeader.biHeight      = -sizePx;   // top-down DIB
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        dib = CreateDIBSection (memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);

        if (dib != nullptr && dibBits != nullptr)
        {
            oldBitmap = (HBITMAP) SelectObject (memDc, dib);

            // Clear the DIB to transparent so the icon's alpha channel
            // composites against zero instead of the screen DC's
            // garbage contents.
            memset (dibBits, 0, pixelCount * sizeof (uint32_t));

            if (DrawIconEx (memDc, 0, 0, hIcon, sizePx, sizePx, 0, nullptr, DI_NORMAL))
            {
                uint32_t  * src  = (uint32_t *) dibBits;
                size_t      i    = 0;

                outPixels.assign (pixelCount, 0);

                // Premultiply each BGRA pixel. DIB layout is 0xAARRGGBB
                // in little-endian uint32 (B,G,R,A in memory order).
                for (i = 0; i < pixelCount; i++)
                {
                    uint32_t  px = src[i];
                    uint8_t   a  = (uint8_t) ((px >> 24) & 0xFF);
                    uint8_t   r  = (uint8_t) ((px >> 16) & 0xFF);
                    uint8_t   g  = (uint8_t) ((px >>  8) & 0xFF);
                    uint8_t   b  = (uint8_t) ( px        & 0xFF);

                    r = (uint8_t) ((r * a) / 255);
                    g = (uint8_t) ((g * a) / 255);
                    b = (uint8_t) ((b * a) / 255);

                    outPixels[i] = ((uint32_t) a << 24) | ((uint32_t) r << 16) |
                                   ((uint32_t) g <<  8) |  (uint32_t) b;
                }

                outW    = sizePx;
                outH    = sizePx;
                success = true;
            }

            SelectObject (memDc, oldBitmap);
        }

        if (dib != nullptr)      { DeleteObject (dib); }
        if (memDc != nullptr)    { DeleteDC (memDc); }
        if (screenDc != nullptr) { ReleaseDC (nullptr, screenDc); }
        DestroyIcon (hIcon);

        return success;
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

#ifdef _DEBUG
    // Log the per-boot DRAM seed so when an illegal-opcode (or any
    // other non-deterministic) fault fires later, the user can grep
    // the debug output for "[Casso] Cold boot seed:" and capture the
    // value into a bug report. Re-running with the same seed gives
    // byte-identical DRAM at every PowerCycle, which is the first
    // requirement for reproducing flaky CPU faults.
    DEBUGMSG (L"[Casso] Cold boot seed: 0x%016llX\n",
              (unsigned long long) seed);
#endif

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

    // DiskManager construction is deferred to Initialize -- it needs
    // a UserConfigStore reference and that's created at Initialize
    // time once the asset base dir is resolved.

    m_machineManager = std::make_unique<MachineManager> (*this);

    m_windowCommandManager = std::make_unique<WindowCommandManager> (*this);
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
        controller = m_diskManager->FindSlot6Controller();

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
    HRESULT          hr           = S_OK;
    size_t           fbSize       = 0;
    fs::path         assetBaseDir;
    fs::path         machinesDir;
    fs::path         themesDir;



    m_currentMachineName = machineName;
    m_config             = config;
    m_cyclesPerFrame     = config.cyclesPerFrame;

    // Register chrome regions with the layout planner once -- their
    // pointers stay registered for the lifetime of the shell. Theme
    // changes that resize the drive bar mutate m_driveBarSlot in
    // place; LayoutManager reads the live thickness on every Resolve.
    m_layout.Register (&m_titleBarSlot);
    m_layout.Register (&m_navStripSlot);
    m_layout.Register (&m_driveBarSlot);

    assetBaseDir = AssetBootstrap::GetAssetBaseDirectory();
    machinesDir  = assetBaseDir / fs::path ("Machines") / fs::path (m_currentMachineName);
    themesDir    = assetBaseDir / fs::path ("Themes");
    m_assetBaseDir = assetBaseDir.wstring();
    m_userConfigStore = std::make_unique<UserConfigStore> (assetBaseDir.wstring());

    m_diskManager = std::make_unique<DiskManager> (m_ownedDevices,
                                                    m_diskStore,
                                                    m_diskAudioSources,
                                                    m_wasapiAudio,
                                                    m_driveWidgets,
                                                    m_driveWidgetState,
                                                    m_driveChrome,
                                                    m_cpuManager,
                                                    m_currentMachineName,
                                                    *m_userConfigStore,
                                                    m_uiFs);

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

    // Prime the chrome-affecting theme state BEFORE creating the
    // window so the initial ClientSizeForCenter inside
    // CreateEmulatorWindow reads the right drive-bar thickness. Without
    // this, a user whose persisted activeTheme is compact (DarkModern
    // or RetroTerminal) would get a window sized for the full
    // skeuomorphic strip on first paint, then immediately shrink as
    // soon as ThemeManager::Activate fires its listener later in
    // Initialize. UserConfigStore needs only assetBaseDir + the
    // UI-thread filesystem, both of which are already live here.
    {
        HRESULT  hrPrefsEarly = m_userConfigStore->LoadAll (m_globalPrefs, m_uiFs);

        IGNORE_RETURN_VALUE (hrPrefsEarly, S_OK);
        m_chromeTheme = ChromeTheme::ForName (m_globalPrefs.activeTheme);
        ApplyThemeToChrome (m_chromeTheme);
    }

    hr = CreateEmulatorWindow (hInstance);
    CHR (hr);

    hr = m_machineManager->CreateMemoryDevices (config);
    CHR (hr);

    m_machineManager->WireLanguageCard();
    m_machineManager->CreateVideoModes();

    // Validate memory bus for overlapping device address ranges
    hr = m_memoryBus.Validate();
    CHR (hr);

    hr = m_machineManager->CreateCpu (config);
    CHR (hr);

    m_machineManager->WirePageTable();

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
        HRESULT  hrUi       = m_uiShell.Initialize (&m_d3dRenderer);
        HRESULT  hrSettings = S_OK;
        HRESULT  hrTheme    = S_OK;
        HRESULT  hrPrefs    = S_OK;



        IGNORE_RETURN_VALUE (hrUi, S_OK);
        m_uiShell.SetChrome (&m_titleBar, &m_navLayer, &m_driveChrome, &m_chromeTheme);

        m_themeManager    = std::make_unique<ThemeManager> (m_uiFs, themesDir.wstring());
        hrTheme           = m_themeManager->Discover();
        IGNORE_RETURN_VALUE (hrTheme, S_OK);
        hrPrefs = m_userConfigStore->LoadAll (m_globalPrefs, m_uiFs);
        IGNORE_RETURN_VALUE (hrPrefs, S_OK);

        // Record the currently-active machine so the next launch boots
        // it by default (Main resolves the value via this same field).
        {
            std::string  narrow;

            narrow.reserve (m_currentMachineName.size());
            for (wchar_t c : m_currentMachineName)
            {
                narrow.push_back ((char) (unsigned char) c);
            }
            if (m_globalPrefs.lastSelectedMachine != narrow)
            {
                m_globalPrefs.lastSelectedMachine = narrow;
                SaveGlobalPrefs();
            }
        }

        // Subscribe the chrome theme cache to ThemeManager BEFORE we
        // activate, so the initial Activate() fires the listener and
        // primes m_chromeTheme from the persisted user choice. Without
        // this the chrome would still paint Skeuomorphic until the
        // user re-picked the theme in Settings.
        m_themeManager->AddChangeListener ([this] (const LoadedTheme & t)
        {
            m_chromeTheme = ChromeTheme::ForName (t.name);
            ApplyThemeToChrome (m_chromeTheme);
            m_settingsPanel.SetTheme (&m_chromeTheme);
        });

        // Tell the theme manager which machine is active BEFORE the
        // first Activate so its listener notification carries the
        // correctly-resolved (per-variant) theme.
        m_themeManager->SetActiveMachineName (m_config.name);

        HRESULT  hrActivate = m_themeManager->Activate (m_globalPrefs.activeTheme);
        if (hrActivate != S_OK)
        {
            // Persisted theme name is unknown (renamed, deleted, or
            // first-run with a stale default) -- fall back to the
            // canonical built-in so the listener still fires.
            IGNORE_RETURN_VALUE (hrActivate, m_themeManager->Activate ("Skeuomorphic"));
        }

        // Apply the persisted per-machine colorMode (and any other
        // live-effect settings) at boot. Without this the initial
        // emulator state defaults to Color regardless of what the user
        // last saved; the missing path was that MachineManager::
        // SwitchMachine has the colorMode-apply logic but it only fires
        // on USER-INITIATED machine switches, not the boot path.
        {
            std::string         machineNameNarrow;
            JsonValue           defaultJson;
            JsonValue           mergedJson;
            JsonParseError      parseErr;
            std::ifstream       configFile;
            std::stringstream   ss;
            std::string         jsonText;
            std::wstring        configRelPath = std::wstring (L"Machines\\") + m_currentMachineName +
                                                L"\\" + m_currentMachineName + L".json";
            fs::path            configPath    = PathResolver::FindFile (PathResolver::BuildSearchPaths (
                                                    PathResolver::GetExecutableDirectory(),
                                                    PathResolver::GetWorkingDirectory()),
                                                    configRelPath);

            machineNameNarrow.reserve (m_currentMachineName.size());
            for (wchar_t c : m_currentMachineName)
            {
                machineNameNarrow.push_back ((char) (unsigned char) c);
            }

            if (!configPath.empty())
            {
                configFile.open (configPath);
                if (configFile.good())
                {
                    ss << configFile.rdbuf();
                    jsonText = ss.str();

                    if (SUCCEEDED (JsonParser::Parse (jsonText, defaultJson, parseErr)) &&
                        SUCCEEDED (m_userConfigStore->Load (machineNameNarrow,
                                                            defaultJson,
                                                            m_uiFs,
                                                            mergedJson)) &&
                        mergedJson.GetType() == JsonType::Object)
                    {
                        const JsonValue *  uiPrefs   = nullptr;
                        std::string        colorMode;

                        if (SUCCEEDED (mergedJson.GetObject ("$cassoUiPrefs", uiPrefs)) &&
                            uiPrefs != nullptr &&
                            SUCCEEDED (uiPrefs->GetString ("colorMode", colorMode)))
                        {
                            int  modeIdx = -1;
                            if      (colorMode == "color") { modeIdx = 0; }
                            else if (colorMode == "green") { modeIdx = 1; }
                            else if (colorMode == "amber") { modeIdx = 2; }
                            else if (colorMode == "white") { modeIdx = 3; }

                            if (modeIdx >= 0)
                            {
                                SetColorModeLive (modeIdx);
                            }
                        }
                    }
                }
            }
        }

        hrSettings = m_settingsPanel.Initialize (m_uiShell,
                                                 *m_userConfigStore,
                                                 m_globalPrefs,
                                                 *m_themeManager,
                                                 *this,
                                                 m_uiFs);
        IGNORE_RETURN_VALUE (hrSettings, S_OK);
        m_uiShell.SetSettingsPanel (nullptr);
        m_uiShell.SetDragSource    (&m_dragDropTarget);

        if (SUCCEEDED (hrUi))
        {
            UINT  initialDpi = GetDpiForWindow (m_hwnd);

            // Propagate the live monitor DPI into UiShell so the first
            // D2D BindBackBuffer uses the right DPI for text. Without
            // this the initial paint binds at the m_dpi default (0->96)
            // and chrome text renders tiny on high-DPI displays until
            // the user resizes the window.
            HRESULT  hrUiResize = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                                                     m_d3dRenderer.GetBackBufferHeight(),
                                                     initialDpi);
            IGNORE_RETURN_VALUE (hrUiResize, S_OK);

            m_d3dRenderer.SetAfterBlitHook ([this] () { m_diskManager->UpdateDriveWidgets(); m_uiShell.Render(); });

            {
                bool  fHasDisk = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();

                if (!fHasDisk)
                {
                    // No Slot 6 controller (stripped Apple II config) --
                    // collapse the drive widgets so they paint nothing
                    // and the bottom command bar is clear of drive UI.
                    m_driveChrome[0].Hide();
                    m_driveChrome[1].Hide();
                }

                m_uiShell.HitTest().Clear();
                if (fHasDisk)
                {
                    m_uiShell.HitTest().Register (HitRect { m_driveChrome[0].BodyRect(), HitSlot::Custom, 0 });
                    m_uiShell.HitTest().Register (HitRect { m_driveChrome[1].BodyRect(), HitSlot::Custom, 1 });
                }
            }

            if (m_fOleInitialized)
            {
                HRESULT hrDrop = m_dragDropTarget.Initialize (m_hwnd, &m_uiShell.HitTest(), [this] (int tag, const std::wstring & path) { Mount (6, tag, path); });
                IGNORE_RETURN_VALUE (hrDrop, S_OK);

                // CassoRenderSurface is a child HWND that occludes the
                // parent's client area for the emulator framebuffer. OLE
                // hit-tests the topmost window under the cursor, so the
                // child needs its own RegisterDragDrop pointing at the
                // same IDropTarget -- otherwise the user sees the
                // not-allowed cursor anywhere over the emulator content.
                if (m_renderHwnd != nullptr)
                {
                    HRESULT hrDropChild = m_dragDropTarget.AttachAdditionalWindow (m_renderHwnd);
                    IGNORE_RETURN_VALUE (hrDropChild, S_OK);
                }

                // UIPI whitelist. When Casso runs at a higher integrity
                // level than the source (e.g. user launched Casso
                // elevated and is dragging from a non-elevated Explorer),
                // UIPI silently blocks the messages OLE uses to marshal
                // the dragged payload across the IL boundary. The fix
                // is ChangeWindowMessageFilterEx for the three messages
                // OLE actually uses for drop targets:
                //   WM_DROPFILES       (0x0233)
                //   WM_COPYDATA        (0x004A)
                //   WM_COPYGLOBALDATA  (0x0049, undocumented but real)
                // Allowing these on both registered HWNDs lets Explorer
                // -> elevated-Casso drag work without lowering Casso's
                // IL.
                {
                    const UINT  s_kWmCopyGlobalData = 0x0049;
                    HWND        hwnds[2] = { m_hwnd, m_renderHwnd };
                    size_t      i        = 0;

                    for (i = 0; i < sizeof (hwnds) / sizeof (hwnds[0]); i++)
                    {
                        if (hwnds[i] == nullptr)
                        {
                            continue;
                        }

                        (void) ChangeWindowMessageFilterEx (hwnds[i], WM_DROPFILES,       MSGFLT_ALLOW, nullptr);
                        (void) ChangeWindowMessageFilterEx (hwnds[i], WM_COPYDATA,        MSGFLT_ALLOW, nullptr);
                        (void) ChangeWindowMessageFilterEx (hwnds[i], s_kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
                    }
                }
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

    m_diskManager->MountCommandLineDisks (disk1Path, disk2Path);

    // Seed the mixer state from the per-machine $cassoUiPrefs JSON
    // before the audio thread first calls SetEnabled / SetMechanism.
    // Default is enabled + Shugart when nothing has been persisted yet.
    {
        std::string         machineNameNarrow;
        JsonValue           defaultJson;
        JsonValue           mergedJson;
        JsonParseError      parseErr;
        std::ifstream       configFile;
        std::stringstream   ss;
        std::string         jsonText;
        std::wstring        configRelPath = std::wstring (L"Machines\\") + m_currentMachineName +
                                            L"\\" + m_currentMachineName + L".json";
        fs::path            configPath    = PathResolver::FindFile (PathResolver::BuildSearchPaths (
                                                PathResolver::GetExecutableDirectory(),
                                                PathResolver::GetWorkingDirectory()),
                                                configRelPath);


        machineNameNarrow.reserve (m_currentMachineName.size());
        for (wchar_t c : m_currentMachineName)
        {
            machineNameNarrow.push_back ((char) (unsigned char) c);
        }

        if (!configPath.empty())
        {
            configFile.open (configPath);
            if (configFile.good())
            {
                HRESULT  hrLoad;

                ss << configFile.rdbuf();
                jsonText = ss.str();

                hrLoad = JsonParser::Parse (jsonText, defaultJson, parseErr);
                if (SUCCEEDED (hrLoad))
                {
                    hrLoad = m_userConfigStore->Load (machineNameNarrow,
                                                     defaultJson,
                                                     m_uiFs,
                                                     mergedJson);
                }
                if (SUCCEEDED (hrLoad) && mergedJson.GetType() == JsonType::Object)
                {
                    const JsonValue *  uiPrefs   = nullptr;

                    if (SUCCEEDED (mergedJson.GetObject ("$cassoUiPrefs", uiPrefs)) &&
                        uiPrefs != nullptr)
                    {
                        bool         enabled = true;
                        std::string  mechNarrow;

                        if (SUCCEEDED (uiPrefs->GetBool ("floppySoundEnabled", enabled)))
                        {
                            m_driveAudioMixer.SetEnabled (enabled);
                        }
                        if (SUCCEEDED (uiPrefs->GetString ("floppyMechanism", mechNarrow)) && !mechNarrow.empty())
                        {
                            std::wstring  mechWide (mechNarrow.begin(), mechNarrow.end());

                            if (m_driveAudioMixer.IsValidMechanism (mechWide))
                            {
                                HRESULT  hrMech = m_driveAudioMixer.SetMechanism (mechWide);
                                IGNORE_RETURN_VALUE (hrMech, S_OK);
                            }
                        }
                    }
                }
            }
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
    HRESULT   hr                = S_OK;
    UINT      dpi               = 0;
    int       clientW           = 0;
    int       clientH           = 0;
    RECT      rc                = {};
    DWORD     style             = 0;
    DWORD     adjustStyle       = 0;
    BOOL      fSuccess          = FALSE;
    RECT      work              = {};
    HMONITOR  activeMon         = nullptr;
    LONG      windowX           = CW_USEDEFAULT;
    LONG      windowY           = CW_USEDEFAULT;
    int       windowW           = 0;
    int       windowH           = 0;
    bool      hadSavedPlacement = false;



    // Register and create the window via the base class
    m_kpszWndClass  = kWindowClass;
    m_hbrBackground = reinterpret_cast<HBRUSH> (GetStockObject (BLACK_BRUSH));
    m_idIcon        = IDI_CASSO;
    m_idIconSmall   = IDI_CASSO;

    hr = Window::Initialize (hInstance);
    CHR (hr);

    // Calculate window size for desired client area, scaled for the
    // monitor we will actually open on. With per-monitor DPI v2,
    // CreateWindowEx uses the requested size *as physical pixels* on
    // the destination monitor -- there's no automatic logical->physical
    // mapping. So if the cursor monitor is at 150% scale, requesting
    // 560-px logical means we get a 560-physical-pixel window that
    // looks half-size next to anything else on that display. Resolve
    // the destination monitor's DPI up front and pre-scale.
    if (GetCursorMonitorWorkArea (work, activeMon))
    {
        UINT  dpiX = 0;
        UINT  dpiY = 0;


        if (SUCCEEDED (GetDpiForMonitor (activeMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX > 0)
        {
            dpi = dpiX;
        }
    }

    if (dpi == 0)
    {
        dpi = GetDpiForSystem();
    }

    // Seed the Window's authoritative DPI so the LayoutManager (which
    // queries it) returns coherent sizes during the pre-Create math.
    // WM_NCCREATE will overwrite this with GetDpiForWindow once the
    // HWND exists; that value wins if it disagrees.
    SetInitialDpi (dpi);

    {
        SIZE  client = m_layout.ClientSizeForFramebuffer (kFramebufferWidth, kFramebufferHeight);

        clientW = (int) client.cx;
        clientH = (int) client.cy;
    }

    rc    = { 0, 0, clientW, clientH };
    // Custom-chrome recipe modeled on microsoft/terminal's
    // NonClientIslandWindow: keep WS_OVERLAPPEDWINDOW (which includes
    // WS_CAPTION + WS_SYSMENU + WS_THICKFRAME + WS_MINIMIZEBOX +
    // WS_MAXIMIZEBOX) so DefWindowProc has the full caption
    // infrastructure for drag-to-move, edge resize, snap layouts, and
    // single-click min/max/close semantics. The visual caption is
    // hidden by collapsing the NC area in WM_NCCALCSIZE; our
    // WM_NCHITTEST returns HTMINBUTTON/HTMAXBUTTON/HTCLOSE for the
    // button rects and HTCAPTION for the drag region, so the OS
    // dispatches the right system action and our OnNcLButtonUp
    // dispatches the action for the captioned buttons.
    style    = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    // Strip WS_CAPTION for the rect-adjust math because our
    // WM_NCCALCSIZE handler restores the original top edge -- it does
    // *not* carve a caption out of the client area. If we passed the
    // full WS_OVERLAPPEDWINDOW style here, AdjustWindowRectExForDpi
    // would add the caption height to windowH but NCCALCSIZE would
    // hand that height back as client space, leaving the actual
    // client taller than requested by the caption height. That extra
    // vertical slack makes the aspect-fit content area shorter-than-
    // framebuffer ratio, producing the pillarbox the user reported.
    // Sizing math has to mirror what NCCALCSIZE actually carves out:
    // left + right borders, bottom border. Top edge is preserved.
    // No menu bar -> bMenu = FALSE in window-rect math.
    adjustStyle = style & ~WS_CAPTION;
    fSuccess = AdjustWindowRectExForDpi (&rc, adjustStyle, FALSE, 0, dpi);
    CWRA (fSuccess);

    windowW = rc.right - rc.left;
    windowH = rc.bottom - rc.top;

    if (GetCursorMonitorWorkArea (work, activeMon))
    {
        CenterInWorkArea (work, windowW, windowH, windowX, windowY);
    }

    hadSavedPlacement = m_windowManager.TryLoadSavedWindowPlacement (activeMon, windowX, windowY, windowW, windowH);
    hr = Window::Create (0,
                         L"Casso",
                         style,
                         windowX, windowY,
                         windowW, windowH,
                         nullptr);
    CHR (hr);

    // Force the app icon onto the window itself (not just the class).
    // Win32 MessageBox-style dialogs and the task bar pick the icon up
    // via WM_GETICON on the parent HWND, NOT WNDCLASS::hIcon; without
    // explicit WM_SETICON the dialog title bar shows no icon and the
    // taskbar falls back to the generic Windows logo.
    {
        int    iconBigSize   = GetSystemMetrics (SM_CXICON);
        int    iconSmallSize = GetSystemMetrics (SM_CXSMICON);
        HICON  hIconBig      = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO),
                                                   IMAGE_ICON, iconBigSize, iconBigSize,
                                                   LR_DEFAULTCOLOR | LR_SHARED);
        HICON  hIconSm       = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO),
                                                   IMAGE_ICON, iconSmallSize, iconSmallSize,
                                                   LR_DEFAULTCOLOR | LR_SHARED);

        if (hIconBig != nullptr)
        {
            SendMessageW (m_hwnd, WM_SETICON, ICON_BIG,   (LPARAM) hIconBig);
        }
        if (hIconSm != nullptr)
        {
            SendMessageW (m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hIconSm);
        }
    }

    // Reconcile actual client size against desired client size. Two
    // sources of drift to handle:
    //   1. The pre-Create DPI estimate was based on cursor monitor;
    //      after CreateWindowEx, the window's actual DPI (now in
    //      m_scaler via WM_NCCREATE) may differ -- recompute the
    //      desired client size from that authoritative DPI.
    //   2. AdjustWindowRectExForDpi minus WS_CAPTION is a best-guess
    //      at what our WM_NCCALCSIZE hands back as client area;
    //      DefWindowProc's border carve-out varies by Windows version.
    //      Measure the actual client and nudge by the residual delta.
    // Skip if saved placement was restored -- the user chose that
    // size deliberately.
    if (!hadSavedPlacement)
    {
        SIZE  desired        = m_layout.ClientSizeForFramebuffer (kFramebufferWidth, kFramebufferHeight);
        RECT  rcActualClient = {};
        RECT  rcActualWindow = {};
        int   actualClientW  = 0;
        int   actualClientH  = 0;
        int   deltaW         = 0;
        int   deltaH         = 0;
        int   fixedW         = 0;
        int   fixedH         = 0;


        clientW = (int) desired.cx;
        clientH = (int) desired.cy;

        if (GetClientRect (m_hwnd, &rcActualClient) && GetWindowRect (m_hwnd, &rcActualWindow))
        {
            actualClientW = rcActualClient.right  - rcActualClient.left;
            actualClientH = rcActualClient.bottom - rcActualClient.top;
            deltaW        = clientW - actualClientW;
            deltaH        = clientH - actualClientH;

            if (deltaW != 0 || deltaH != 0)
            {
                fixedW = (rcActualWindow.right  - rcActualWindow.left) + deltaW;
                fixedH = (rcActualWindow.bottom - rcActualWindow.top)  + deltaH;
                SetWindowPos (m_hwnd, nullptr, 0, 0, fixedW, fixedH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }

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
    // valid button rects even before the first WM_SIZE arrives. Read
    // the actual client size from the HWND rather than the requested
    // clientW, since TryLoadSavedWindowPlacement above may have
    // restored a different size for this monitor topology -- using
    // the stale request would leave the chrome painted only to the
    // default width until the user resized the window.
    {
        RECT  rcActual = {};


        if (GetClientRect (m_hwnd, &rcActual))
        {
            clientW = rcActual.right  - rcActual.left;
            clientH = rcActual.bottom - rcActual.top;
        }

        // Re-resolve DPI against the live HWND. The 'dpi' we used to
        // size the window was the *cursor* monitor's at request time;
        // Windows may have placed the window on a different monitor
        // (per-monitor v2) or honored saved placement that lives on
        // another monitor. The actual chrome metrics need to match
        // the monitor the window is actually on so the framebuffer
        // aspect-fit produces no pillarbox at default size.
        UINT  windowDpi = GetDpiForWindow (m_hwnd);
        if (windowDpi != 0)
        {
            dpi = windowDpi;
        }
    }
    m_titleBar.UpdateGeometry (clientW, dpi);
    m_navLayer.Layout (0, m_titleBar.GetTitleHeight(), clientW, dpi, &m_uiShell.Text());
    m_navLayer.SetDispatch ([this] (WORD commandId) { HandleCommand (commandId); });

    // Load the app icon (IDI_CASSO) into a premultiplied BGRA8 pixel
    // buffer so the title bar can blit it left of the caption text.
    // Loaded at 32x32 (high enough to look crisp at typical title-bar
    // sizes when D2D linearly downscales it); failure is non-fatal --
    // the title bar simply omits the icon if the load misses.
    {
        std::vector<uint32_t>  iconPixels;
        int                    iconW = 0;
        int                    iconH = 0;

        if (LoadIconAsPremulBgra (hInstance, IDI_CASSO, 32, iconPixels, iconW, iconH))
        {
            m_titleBar.SetAppIcon (std::move (iconPixels), iconW, iconH);
        }
    }
    m_driveChrome[0].Initialize (6, 0, this);
    m_driveChrome[1].Initialize (6, 1, this);
    {
        LayoutManagerResult  layout = m_layout.Resolve (clientW, clientH);

        LayoutDriveWidgetsInCommandBar (m_driveChrome, layout, clientW, clientH, dpi);
        m_d3dRenderer.SetTopInsetPx    (layout.topInsetPx);
        m_d3dRenderer.SetBottomInsetPx (layout.bottomInsetPx);
    }

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
//  Mount  (IDriveCommandSink)
//
//  IDriveCommandSink override delegates straight through to the
//  DiskManager so the chrome / drag-drop entry points and the manager
//  share a single mount path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Mount (int slot, int drive, const std::wstring & path)
{
    return m_diskManager->Mount (slot, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject  (IDriveCommandSink)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::Eject (int slot, int drive)
{
    m_diskManager->Eject (slot, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowMachinePicker
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowMachinePicker()
{
    m_machineManager->ShowMachinePicker();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyAndPersistTheme
//
//  Activates the named theme via ThemeManager (which fires our chrome
//  cache listener) and writes the new choice into GlobalUserPrefs so
//  the next launch starts in the same theme. Activation failure on an
//  unknown name falls back to Skeuomorphic rather than leaving the
//  chrome in a stale state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::ApplyAndPersistTheme (const std::string & themeName)
{
    HRESULT  hr           = S_OK;
    HRESULT  hrActivate   = S_OK;
    HRESULT  hrSave       = S_OK;
    std::string  resolved = themeName;



    if (themeName.empty() || m_themeManager == nullptr)
    {
        return S_FALSE;
    }

    hrActivate = m_themeManager->Activate (themeName);
    if (hrActivate != S_OK)
    {
        resolved   = "Skeuomorphic";
        hrActivate = m_themeManager->Activate (resolved);
    }
    CHR (hrActivate);

    m_globalPrefs.activeTheme = resolved;
    if (m_userConfigStore != nullptr)
    {
        hrSave = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    }
    else
    {
        hrSave = m_globalPrefs.Save (m_assetBaseDir, m_uiFs);
    }
    IGNORE_RETURN_VALUE (hrSave, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveGlobalPrefs
//
//  Flushes the in-memory GlobalUserPrefs to UserPrefs.json. Used as the
//  WindowManager save callback so per-monitor window placement edits
//  land on disk immediately after the user moves/resizes the window.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveGlobalPrefs()
{
    HRESULT  hr = S_OK;


    if (m_userConfigStore == nullptr)
    {
        return;
    }

    hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyThemeToChrome
//
//  Push freshly-activated theme into the chrome regions whose layout
//  depends on theme state. Currently that's the drive bar:
//      * Compact themes shrink the bottom inset and switch the per-
//        drive widget to the small flat card paint path.
//      * Skeuomorphic restores the full 192dp inset and the
//        Apple ][-style realistic widgets.
//  When the bottom inset changes, the HWND is resized by the delta so
//  the emulator pixel grid is preserved across the theme swap (i.e.
//  the user's window grows or shrinks instead of the framebuffer
//  pillarboxing). The actual painter re-layout happens inside OnResize
//  via the existing WM_SIZE path.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyThemeToChrome (const ChromeTheme & theme)
{
    constexpr int  s_kFullDriveBarDp    = 192;
    constexpr int  s_kCompactDriveBarDp = 64;

    int   desiredThicknessDp = theme.compactDrives ? s_kCompactDriveBarDp : s_kFullDriveBarDp;
    int   priorThicknessDp   = m_driveBarSlot.DesiredThicknessDp();
    RECT  rcClient           = {};
    RECT  rcWindow           = {};
    int   centerW            = 0;
    int   centerH            = 0;



    m_driveChrome[0].SetCompact (theme.compactDrives);
    m_driveChrome[1].SetCompact (theme.compactDrives);

    if (m_hwnd == nullptr || desiredThicknessDp == priorThicknessDp)
    {
        m_driveBarSlot.SetThicknessDp (desiredThicknessDp);
        return;
    }

    // Skip the auto-resize for windows that are min/max/fullscreen --
    // the user explicitly chose those window states and shouldn't see
    // the window resize from under them on a theme swap. The new
    // chrome thickness still gets applied to the contributor below
    // so the next normal-state resize uses the right math.
    if (IsIconic (m_hwnd) || IsZoomed (m_hwnd) || m_d3dRenderer.IsFullscreen())
    {
        m_driveBarSlot.SetThicknessDp (desiredThicknessDp);
        return;
    }

    if (!GetClientRect (m_hwnd, &rcClient) || !GetWindowRect (m_hwnd, &rcWindow))
    {
        m_driveBarSlot.SetThicknessDp (desiredThicknessDp);
        return;
    }

    // Capture the current center (emulator viewport) size BEFORE
    // mutating the contributor. The user may have resized the window
    // manually since boot; preserving "the emu viewport stays the
    // same size, the drive bar grows/shrinks around it" is the
    // intuitive contract on a theme swap.
    {
        LayoutManagerResult  before = m_layout.Resolve (rcClient.right  - rcClient.left,
                                                        rcClient.bottom - rcClient.top);

        centerW = before.centerRect.right  - before.centerRect.left;
        centerH = before.centerRect.bottom - before.centerRect.top;
    }

    m_driveBarSlot.SetThicknessDp (desiredThicknessDp);

    {
        SIZE  newClient   = m_layout.ClientSizeForCenter (centerW, centerH);
        int   ncOverheadH = (rcWindow.bottom - rcWindow.top) - (rcClient.bottom - rcClient.top);
        int   ncOverheadW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
        int   newWindowW  = (int) newClient.cx + ncOverheadW;
        int   newWindowH  = (int) newClient.cy + ncOverheadH;

        SetWindowPos (m_hwnd, nullptr, 0, 0, newWindowW, newWindowH,
                      SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchMachine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::SwitchMachine(const wstring & machineName)
{
    return m_machineManager->SwitchMachine(machineName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetColorModeLive
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetColorModeLive (int settingsColorModeIndex)
{
    ColorMode  mode = ColorMode::Color;


    switch (settingsColorModeIndex)
    {
        case 0:  mode = ColorMode::Color;     break;
        case 1:  mode = ColorMode::GreenMono; break;
        case 2:  mode = ColorMode::AmberMono; break;
        case 3:  mode = ColorMode::WhiteMono; break;
        default: return;
    }

    m_colorMode.store (mode, std::memory_order_release);
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
    m_diskManager->SetColdBootMountWindow (false);

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
        bool  fbDirtyThisFrame = false;
        {
            lock_guard<mutex> lock (m_fbMutex);

            if (m_fbReady)
            {
                m_fbReady        = false;
                fbDirtyThisFrame = true;
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
            CrtParams  params = MakeCrtParams (m_globalPrefs.crtByMode[(int) m_colorMode.load(std::memory_order_acquire)],
                                               (size_t) m_colorMode.load(std::memory_order_acquire),
                                               themeDefaults,
                                               (float) m_d3dRenderer.GetBackBufferWidth(),
                                               (float) m_d3dRenderer.GetBackBufferHeight());
            m_d3dRenderer.SetCrtParams (params);
        }

        // Skip the entire upload + 9-pass post-process when neither the
        // emulator framebuffer nor any CRT param changed (and the
        // persistence trail isn't still decaying). Saves ~20%% GPU at a
        // BASIC prompt. PeekMessage above still drains messages; the
        // brief sleep keeps this thread from spinning.
        //
        // FORCE PRESENT when the nav layer has an open menu so menu
        // hover / open / close transitions paint. Without this, a
        // paused machine produces no fb changes -> no Present -> menus
        // open in state-only and never repaint, looking dead.
        m_settingsPanel.UpdatePreviewOverlap (m_d3dRenderer.GetEmulatorContentScreenRect());
        IGNORE_RETURN_VALUE (hr, m_settingsPanel.RenderPopup());
        if (m_navLayer.IsOpen())
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
        if (!m_d3dRenderer.NeedsPresent (fbDirtyThisFrame))
        {
            Sleep (1);
            continue;
        }

        m_d3dRenderer.UploadAndPresent (fbDirtyThisFrame ? m_uiFramebuffer.data() : nullptr);
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
        fs::path          baseDir;
        wstring           devicesDir;

        // Use the same user-writable asset root that Main.cpp /
        // AssetBootstrap used when writing the WAVs so the read
        // path agrees with the write path.
        baseDir     = AssetBootstrap::GetAssetBaseDirectory();
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
            m_diskManager->RemountSlot6Disks();
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
            m_diskManager->RemountSlot6Disks();
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

            hrMount = m_diskManager->MountDiskInSlot6 (drive, cmd.payload);
            IGNORE_RETURN_VALUE (hrMount, S_OK);
            break;
        }

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
        {
            int   drive = (cmd.id == IDM_DISK_EJECT1) ? 0 : 1;

            m_diskManager->EjectDiskInSlot6 (drive);
            break;
        }

        case IDM_AUDIO_DRIVE_ENABLE:
        case IDM_AUDIO_DRIVE_DISABLE:
        {
            m_driveAudioMixer.SetEnabled (cmd.id == IDM_AUDIO_DRIVE_ENABLE);
            break;
        }

        case IDM_AUDIO_DRIVE_MECHANISM:
        {
            // Payload is "shugart" or "alps" (canonical lower-case
            // from SettingsPanelState). DriveAudioMixer wants the
            // mixed-case directory name; map here so the mixer's
            // validator accepts it and LoadSamples finds the right
            // <devicesDir>/Audio/<Mechanism>/ subdir.
            std::wstring  mech;

            if (cmd.payload == "alps")
            {
                mech = L"Alps";
            }
            else
            {
                mech = L"Shugart";
            }

            HRESULT  hrMech = m_driveAudioMixer.SetMechanism (mech);
            IGNORE_RETURN_VALUE (hrMech, S_OK);
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
//  StepInstructionWhilePaused
//
//  Runs one CPU instruction directly from the UI thread. Caller MUST
//  have verified the CPU thread is paused (blocked on pauseCV.wait)
//  -- this is a quiet contract; we don't re-check here.
//
//  Steps the CPU, ticks the disk controller in step, then runs one
//  full video frame and publishes the framebuffer so the main UI
//  loop sees fbDirty next iteration and presents.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StepInstructionWhilePaused ()
{
    if (m_cpu == nullptr)
    {
        return;
    }

    m_cpu->StepOne();

    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->Tick (m_cpu->GetLastInstructionCycles());
    }

    RunOneFrame();
    PublishFramebuffer();
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



    m_machineManager->SelectVideoMode();

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
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleCommand (WORD commandId)
{
    m_windowCommandManager->HandleCommand (commandId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnCommand (HWND hwnd, int id)
{
    return m_windowCommandManager->OnCommand (hwnd, id);
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



    if (m_uiShell.OnMouseMove (x, y, leftDown))
    {
        return false;
    }

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
    if (m_uiShell.OnLButtonDown (x, y))
    {
        return false;
    }

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
    if (m_uiShell.OnLButtonUp (x, y))
    {
        return false;
    }

    // If we just finished an OLE drop on a drive widget, the OS posts
    // a WM_LBUTTONUP that lands here on top of the drive. Swallow it
    // so the user doesn't see the file-open dialog pop up immediately
    // after the dropped image mounts.
    if (m_dragDropTarget.ConsumeSuppressedClick())
    {
        return false;
    }

    for (DriveWidget & drive : m_driveChrome)
    {
        region = drive.HitTest (x, y);
        if (region == DriveWidgetRegion::Body)
        {
            HRESULT  hrBrowse = m_windowCommandManager->PromptForDiskImage (drive.Drive());
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

    if (m_uiShell.HandleKey (vk))
    {
        return false;
    }

    if (m_refs.keyboard == nullptr)
    {
        return false;
    }

    ctrlHeld = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    altHeld  = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    if (altHeld && vk >= 0x20 && vk <= 0x7E && m_navLayer.HandleAltKey ((wchar_t) vk))
    {
        return false;
    }

    // F10 — focus the menu bar (Windows convention). Opens the File
    // menu so subsequent Left/Right (or Tab) keys cycle between
    // top-level menus while Up/Down/Enter navigate within.
    if (vk == VK_F10 && !ctrlHeld && !altHeld)
    {
        if (!m_navLayer.IsOpen())
        {
            m_navLayer.Open (NavMenu::File, true);
        }
        else
        {
            m_navLayer.Close();
        }
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

    // Suppress the WM_CHAR that Windows synthesizes from a WM_KEYDOWN
    // already consumed by overlay UI (settings panel / open menu).
    // Without this, hitting Enter on the settings OK button closes the
    // panel and then drops a CR into the //e keyboard.
    if (m_uiShell.IsCapturingInput())
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

    // Release the D2D target bitmap before resizing the swap chain.
    // ResizeBuffers fails with DXGI_ERROR_INVALID_CALL (0x887a0001) while
    // any outside reference (D2D bitmap, IDXGISurface, RTV) still holds
    // the back buffer. UiShell rebinds on the next Render() because
    // OnResize below marks the text target dirty.
    m_uiShell.Text().UnbindBackBuffer();

    m_d3dRenderer.Resize (static_cast<int> (width), renderH);

    {
        UINT     dpi   = GetDpiForWindow (m_hwnd);
        HRESULT  hrUiR = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                                             m_d3dRenderer.GetBackBufferHeight(),
                                             dpi);
        IGNORE_RETURN_VALUE (hrUiR, S_OK);
        m_titleBar.UpdateGeometry (static_cast<int> (width), dpi);
        m_navLayer.Layout (0, m_titleBar.GetTitleHeight(), static_cast<int> (width), dpi, &m_uiShell.Text());

        {
            LayoutManagerResult  layout = m_layout.Resolve (static_cast<int> (width), renderH);
            bool                fHasDisk = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();

            if (fHasDisk)
            {
                LayoutDriveWidgetsInCommandBar (m_driveChrome, layout, static_cast<int> (width), renderH, dpi);
            }
            else
            {
                // Machine has no Disk II controller (e.g. stripped Apple II
                // config). Collapse the drive widget rects so DriveWidget
                // paints nothing and the drag-drop overlay's empty-rect
                // path treats the whole window as the drop target.
                m_driveChrome[0].Hide();
                m_driveChrome[1].Hide();
            }

            m_uiShell.HitTest().Clear();
            if (fHasDisk)
            {
                m_uiShell.HitTest().Register (HitRect { m_driveChrome[0].BodyRect(), HitSlot::Custom, 0 });
                m_uiShell.HitTest().Register (HitRect { m_driveChrome[1].BodyRect(), HitSlot::Custom, 1 });
            }
            m_d3dRenderer.SetTopInsetPx    (layout.topInsetPx);
            m_d3dRenderer.SetBottomInsetPx (layout.bottomInsetPx);
        }
    }

    {
        lock_guard<mutex> lock (m_fbMutex);

        if (!m_uiFramebuffer.empty())
        {
            const ThemeCrtDefaults *  themeDefaults = nullptr;
            LoadedTheme                resolvedTheme;
            CrtParams                  params       = {};
            bool                       fHaveTheme   = false;

            if (m_themeManager != nullptr && m_themeManager->GetActiveTheme() != nullptr)
            {
                resolvedTheme = m_themeManager->GetActiveResolvedTheme();
                themeDefaults = &resolvedTheme.crtDefaults;
                fHaveTheme    = true;
            }

            (void) fHaveTheme;
            params = MakeCrtParams (m_globalPrefs.crtByMode[(int) m_colorMode.load(std::memory_order_acquire)],
                                    (size_t) m_colorMode.load(std::memory_order_acquire),
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
//  SoftReset
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SoftReset()
{
    m_machineManager->SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PowerCycle()
{
    m_machineManager->PowerCycle();
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



    controller = m_diskManager->FindSlot6Controller();

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

    controller = m_diskManager->FindSlot6Controller();

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
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnInitMenuPopup (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu)
{
    return m_windowCommandManager->OnInitMenuPopup (hwnd, hMenu, itemIndex, isWindowMenu);
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
    NCCALCSIZE_PARAMS *  pParams      = nullptr;
    LRESULT              defResult    = 0;
    LONG                 originalTop  = 0;



    if (wParam == FALSE)
    {
        outResult = 0;
        return false;
    }

    pParams = reinterpret_cast<NCCALCSIZE_PARAMS *> (lParam);
    if (pParams == nullptr)
    {
        outResult = 0;
        return false;
    }

    // Mirror microsoft/terminal NonClientIslandWindow::_OnNcCalcSize:
    // let DefWindowProc compute the default frame (gives Windows the
    // correct resize-border math at the left/right/bottom edges, plus
    // Aero Snap awareness) then re-apply the original top edge so the
    // visual title-bar area collapses into our client rect for the
    // custom-painted chrome. Drag and edge resize keep working because
    // the OS still sees a captioned window with thick frames.
    originalTop = pParams->rgrc[0].top;
    defResult   = DefWindowProc (hwnd, WM_NCCALCSIZE, wParam, lParam);
    if (defResult != 0)
    {
        outResult = defResult;
        return false;
    }

    pParams->rgrc[0].top = originalTop;
    outResult            = 0;
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





