#include "Pch.h"

#include "EmulatorShell.h"
#include "AssetBootstrap.h"
#include "Print/PrintJobStore.h"
#include "Devices/Printer/PrinterCard.h"
#include "Ui/PrinterPanel.h"

#include "Core/PathResolver.h"
#include "Version.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/Apple2eKeyboard.h"
#include "Devices/AppleSoftSwitchBank.h"
#include "Devices/AppleGamePort.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Devices/AppleSpeaker.h"
#include "Devices/Disk2Controller.h"
#include "Devices/LanguageCard.h"
#include "Devices/Apple2eMmu.h"
#include "Core/Prng.h"

#include "DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Video/AppleTextMode.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)

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
static constexpr int     s_kLabelBottomGapDp    = 2;

// Vertical extent (dp) of the joystick-mode button's band -- the strip
// at the top of the bottom drive-bar inset that hosts only the button.
// Sized as ~8 dp top gap + a ~27 dp button (s_kPadYDp*2 + ~15 dp content)
// + ~8 dp bottom gap. Bumping the button's font / padding requires
// updating this and s_kFullDriveBarDp / s_kCompactDriveBarDp to match.
static constexpr int     s_kJoystickButtonBandDp = 43;
static constexpr int     s_kPaddleNoticeMs       = 8000;   // auto-dismiss for the paddle-mode tooltip

// Minimum emulator-viewport (center) the window must always host, plus a
// small pad past the last menu title, so the bottom drive bar can never be
// driven up into the menu strip / title (NC) area and menu titles never
// clip. The drive-bar and title / nav insets are added live by the
// chrome-band dock around this center.
static constexpr int     s_kMinCenterWidthDp  = 420;
static constexpr int     s_kMinCenterHeightDp = 160;
static constexpr int     s_kMenuRightPadDp    = 12;

// WM_KEYDOWN / WM_CHAR lParam bit 30: "previous key state" — set when the
// key was already down, i.e. this event is a Windows OS auto-repeat. We
// gate the emulated keyboard strobe on this so holding a key delivers a
// single //e keypress instead of flooding $C000 at the host repeat rate.
static constexpr LPARAM  s_kPreviousKeyDownLParamBit = 0x40000000;

// Emulated joystick axis extremes. The PREAD model reads 0..255; an axis
// deflected to a key maps to a rail, neutral sits at s_knPaddleCenter.
static constexpr Byte    s_kPaddleAxisMin            = 0;
static constexpr Byte    s_kPaddleAxisMax            = 255;

// Host letter keys that double as the emulated joystick fire buttons in
// "Map Arrows to Joystick" mode: X -> button 0 ($C061 / Open-Apple),
// Z -> button 1 ($C062 / Closed-Apple).
static constexpr WPARAM  s_kJoystickButton0Vk        = 'X';
static constexpr WPARAM  s_kJoystickButton1Vk        = 'Z';

// Paddle-mode mouse capture tuning. Relative motion over s_kPaddleSweepInches
// of physical mouse travel (DPI-scaled) sweeps the paddle across its full
// s_kPaddleRange; the value is held (no spring return) the way a real
// paddle's dial is. s_knPaddleCenter mirrors the device default.
static constexpr float   s_kPaddleSweepInches        = 4.0f;
static constexpr float   s_kPaddleRange              = 255.0f;
static constexpr float   s_kPaddleMinF               = 0.0f;
static constexpr float   s_kPaddleMaxF               = 255.0f;
static constexpr Byte    s_kPaddleCenterByte         = 127;

// Lit-pixel source color for the monochrome monitors: the text renderer
// keeps green here and the post-render tint recolors the whole frame to the
// selected phosphor. The Color monitor's text color is user-selectable and
// lives in m_colorMonitorTextArgb instead.
static constexpr uint32_t s_kMonoSourceTextBgra       = 0xFF00FF00;   // green

// Chrome keyboard-focus ring indices (see EmulatorShell::m_chromeFocusIndex).
// -1 = guest (//e has focus); 0..6 = the seven menu titles File..Help; 7 =
// the joystick-mode toggle button; 8/9 = drive widgets 1/2. The ring wraps
// modulo s_kChromeFocusCount when traversed with Tab.
static constexpr int     s_kChromeFocusNone          = -1;
static constexpr int     s_kChromeFocusMenuFirst     = 0;
static constexpr int     s_kChromeFocusMenuLast      = 6;
static constexpr int     s_kChromeFocusButton        = 7;
static constexpr int     s_kChromeFocusDrive0        = 8;
static constexpr int     s_kChromeFocusDrive1        = 9;
static constexpr int     s_kChromeFocusCount         = 10;





////////////////////////////////////////////////////////////////////////////////
//
//  Window placement helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    void LayoutDriveWidgetsInCommandBar (
        std::array<DriveWidget, 2>  & driveChrome,
        int                           bottomInsetPx,
        int                           clientW,
        int                           clientH,
        UINT                          dpi)
    {
        int            bottomInset   = bottomInsetPx;
        int            commandBarTop = std::max (0, clientH - bottomInset);
        int            gap           = MulDiv (s_kDriveWidgetGapDp, static_cast<int> (dpi), s_kBaseDpi);
        int            bottomGap     = 0;
        RECT           probe         = {};
        int            widgetW       = 0;
        int            widgetH       = 0;
        int            totalW        = 0;
        int            x             = 0;
        int            y             = 0;
        size_t         i             = 0;
        DxuiDpiScaler  scaler;
        RECT           anchor        = {};



        scaler.SetDpi (dpi);
        anchor = { 0, 0, 0, 0 };
        driveChrome[0].Layout (anchor, scaler);
        probe   = driveChrome[0].OuterRect();
        widgetW = probe.right  - probe.left;
        widgetH = probe.bottom - probe.top;
        totalW  = widgetW * static_cast<int> (driveChrome.size()) + gap * (static_cast<int> (driveChrome.size()) - 1);
        x       = std::max (0, (clientW - totalW) / 2);
        // Anchor the widget to the bottom so the margin between the
        // basename label and the window edge mirrors the gap between
        // the drive body and the label (s_kLabelStripGapPx, scaled).
        bottomGap = MulDiv (s_kLabelBottomGapDp, static_cast<int> (dpi), s_kBaseDpi);
        y         = std::max (commandBarTop, clientH - widgetH - bottomGap);

        for (i = 0; i < driveChrome.size(); i++)
        {
            int   widgetX       = x + static_cast<int> (i) * (widgetW + gap);
            int   widgetCenterX = widgetX + widgetW / 2;
            int   vanishingX    = clientW / 2;
            // Shrink factor matches the case-top depth ratio (back
            // edge is ~20% narrower than the front, so back center
            // shifts ~20% of the way toward the shared vanishing
            // point). Numerator chosen to match s_kCaseBackInsetPx
            // ratio in DriveWidget.cpp.
            int   skewPx        = MulDiv (vanishingX - widgetCenterX, 27, 100);
            RECT  widgetAnchor  = { widgetX, y, widgetX, y };

            driveChrome[i].SetPerspectiveSkewPx (skewPx);
            driveChrome[i].Layout (widgetAnchor, scaler);
        }
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
    // pixel buffer suitable for the DxuiTextRenderer::DrawIconBitmap
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
    Disk2Controller *   controller = nullptr;



    m_cpuManager.Stop();

    // Spec-006 / FR-024. Revoke BOTH sinks BEFORE the dialog tears
    // down its ring (and before the controller / audio source itself
    // is destroyed, which happens via m_ownedDevices / m_diskAudioSources
    // below). Controller sink first, then audio sink, matching the
    // attachment order in OpenDisk2DebugDialog.
    if (m_disk2DebugPanel != nullptr)
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

        m_disk2DebugPanel.reset();
    }

    if (m_inputDebugPanel != nullptr)
    {
        if (m_refs.keyboard != nullptr)
        {
            m_refs.keyboard->SetInputEventSink (nullptr);
        }

        auto * iieSwitches = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (nullptr);
        }

        if (m_refs.gamePort != nullptr)
        {
            m_refs.gamePort->SetInputEventSink (nullptr);
        }

        m_inputDebugPanel.reset();
    }

    // The printer panel holds no machine sinks -- just close its window.
    m_printerPanel.reset();

    // / T097 / FR-025. Final auto-flush of any dirty disks on
    // process shutdown — matches the "graceful exit" requirement from
    // audit §7 so a crash-free quit never loses user writes.
    hrFlush = m_diskStore.FlushAll();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    // Native-only ownership teardown.
    m_uiShell.Shutdown();
    m_dragDropTarget.Shutdown();
    m_driveWidgets.UnloadDocument();
    m_mainMenu.Hide();
    m_mainMenu.SetPopupHost (nullptr);

    // Drop the host's adopted-chrome references before the chrome
    // members or m_host itself go out of scope. The chrome controls
    // (m_mainMenu, m_driveChrome, m_joystickButton) are raw-pointer-
    // registered into m_host->Root() via DxuiPanel::Adopt; releasing
    // the adoption here keeps the panel from ever holding a dangling
    // pointer during the field-by-field destruction below. (The caption
    // is host-owned, not adopted, so it is not in this set.)
    if (m_host)
    {
        m_host->Root().ClearAdopted();
    }

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

    // Register the chrome bands + center with the dock layout once --
    // their thicknesses are refreshed from DPI + live drive-bar state
    // on every ComputeViewportRect / ClientSizeForCenterPx call.
    m_chromeDock.SetDock (m_titleBand,  DxuiDock::Top);
    m_chromeDock.SetDock (m_navBand,    DxuiDock::Top);
    m_chromeDock.SetDock (m_driveBand,  DxuiDock::Bottom);
    m_chromeDock.SetDock (m_centerBand, DxuiDock::Fill);

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
        m_chromeTheme = CassoTheme::ForName (m_globalPrefs.activeTheme);
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

    // Initialize the Apple ][ framebuffer renderer against the host's
    // D3D11 device + DXGI swap chain (full host ownership). The host
    // owns Present; the renderer composites the framebuffer into the
    // host back buffer from the before-present hook wired below. The
    // initial target rect is the DxuiViewport bounds computed during
    // CreateEmulatorWindow.
    hr = m_d3dRenderer.Initialize (m_host->GetDevice(),
                                   m_host->GetContext(),
                                   m_host->GetSwapChain(),
                                   kFramebufferWidth,
                                   kFramebufferHeight,
                                   m_viewportBoundsPx);
    CHR (hr);

    // Composite the Apple ][ framebuffer into the host's back buffer
    // before the host paints chrome on top (DxuiHwndSource::PaintPump).
    // m_pendingFramebuffer is staged each UI frame by RunMessageLoop;
    // nullptr means "no new emulator frame" (re-composite last upload).
    m_host->SetBeforePresentHook ([this] ()
    {
        HRESULT  hrComposite = m_d3dRenderer.UploadAndComposite (m_host->GetBackBufferRtv(),
                                                                 m_pendingFramebuffer);

        IGNORE_RETURN_VALUE (hrComposite, S_OK);
    });

    // Native UI runtime bootstrap. UiShell owns the painter, text
    // renderer, hit-tester, focus manager, and input translator;
    // wiring it onto the after-blit hook lets it composite chrome on
    // top of the emulator frame without ever pausing the render loop.
    {
        HRESULT  hrUi       = m_uiShell.Initialize (&m_d3dRenderer);
        HRESULT  hrTheme    = S_OK;
        HRESULT  hrPrefs    = S_OK;



        IGNORE_RETURN_VALUE (hrUi, S_OK);
        // The caption is host-owned now, and chrome paints through the
        // host panel tree; UiShell only routes input, hit-tests, and
        // supplies the theme / viewport metrics the settings panel reads.
        m_uiShell.SetMainMenu (&m_mainMenu);
        m_uiShell.SetTheme    (&m_chromeTheme);

        // Inject the shared text renderer into chrome controls that
        // need to measure label strings during Layout. Mirrors the
        // UiShell-owned painter / text renderer pair so the chrome
        // controls participate in the standard IDxuiControl::Layout
        // contract without needing the renderer passed as a Layout
        // parameter on every call.
        m_mainMenu.SetTextRendererForMeasure (&m_uiShell.Text());
        m_joystickButton.SetTextRenderer     (&m_uiShell.Text());

        m_themeManager    = std::make_unique<ThemeManager> (m_uiFs, themesDir.wstring());
        hrTheme           = m_themeManager->Discover();
        IGNORE_RETURN_VALUE (hrTheme, S_OK);
        hrPrefs = m_userConfigStore->LoadAll (m_globalPrefs, m_uiFs);
        IGNORE_RETURN_VALUE (hrPrefs, S_OK);

        // Paddle is an active mouse-capture mode, not a passive remap; never
        // restore it on launch (it would light the LED / widen the widget
        // while the mouse is NOT actually captured). Fall back to Off; the
        // passive Joystick remap is safe to restore.
        m_inputMode = (m_globalPrefs.inputMappingMode == InputMappingMode::Paddle)
                          ? InputMappingMode::Off
                          : m_globalPrefs.inputMappingMode;
        m_globalPrefs.inputMappingMode = m_inputMode;
        m_joystickButton.SetMode (m_inputMode);

        SetColorMonitorTextArgbLive (
            ColorUtil::ResolveColorMonitorTextArgb (m_globalPrefs.colorMonitorTextMode,
                                                    m_globalPrefs.colorMonitorTextCustomArgb));

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
            m_chromeTheme = CassoTheme::ForName (t.name);
            ApplyThemeToChrome (m_chromeTheme);
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

            // Chrome no longer composites via an after-blit hook: it
            // paints through the host's panel-tree pump (the adopted
            // chrome controls) on top of the Apple ][ framebuffer. The
            // per-frame drive-widget tick + door-animation redraw that
            // used to live in that hook now run in RunMessageLoop.

            {
                bool  fHasDisk = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();

                if (!fHasDisk)
                {
                    // No Slot 6 controller (stripped Apple II config) --
                    // collapse the drive widgets so they paint nothing
                    // and the bottom command bar is clear of drive UI.
                    // The joystick-mode button still paints, since
                    // joystick input is independent of disk presence.
                    m_driveChrome[0].Hide();
                    m_driveChrome[1].Hide();
                }

                m_uiShell.HitTest().Clear();
                if (fHasDisk)
                {
                    m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[0].BodyRect(), DxuiHitSlot::Custom, 0 });
                    m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[1].BodyRect(), DxuiHitSlot::Custom, 1 });
                }
            }

            if (m_fOleInitialized)
            {
                HRESULT hrDrop = m_dragDropTarget.Initialize (m_hwnd, &m_uiShell.HitTest(), [this] (int tag, const std::wstring & path) { Mount (6, tag, path); }, IsSupportedDiskImageExtension);
                IGNORE_RETURN_VALUE (hrDrop, S_OK);

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
                // Allowing these lets Explorer -> elevated-Casso drag
                // work without lowering Casso's IL. The window is now a
                // single top-level HWND (the legacy CassoRenderSurface
                // child is gone), so only m_hwnd needs the filter.
                {
                    const UINT  s_kWmCopyGlobalData = 0x0049;

                    (void) ChangeWindowMessageFilterEx (m_hwnd, WM_DROPFILES,        MSGFLT_ALLOW, nullptr);
                    (void) ChangeWindowMessageFilterEx (m_hwnd, WM_COPYDATA,         MSGFLT_ALLOW, nullptr);
                    (void) ChangeWindowMessageFilterEx (m_hwnd, s_kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
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

    // Reconcile actual client size against the desired framebuffer-sized
    // client now that the window is shown and its NC frame has fully
    // materialized. Done before UpdateWindowTitle so the user never sees
    // the wrong-size window flash.
    ReconcileInitialClientSize();

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

    // A disk mounted at startup (boot-disk picker result or --disk1 /
    // --disk2) belongs in the recent-disks MRU just like one mounted via
    // the chrome, so it surfaces in the disk picker on the next launch.
    // Record disk 2 first so the primary boot disk (drive 1) ends up the
    // most-recent entry.
    RecordRecentDisk (std::filesystem::path (disk2Path).wstring());
    RecordRecentDisk (std::filesystem::path (disk1Path).wstring());

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
                            // DriveAudioMixer matches mechanism names case-
                            // insensitively, so the persisted lower-case token
                            // ("alps"/"shugart") can be handed over as-is.
                            std::wstring  mechWide (mechNarrow.begin(), mechNarrow.end());
                            HRESULT       hrMech = m_driveAudioMixer.SetMechanism (mechWide);

                            IGNORE_RETURN_VALUE (hrMech, S_OK);
                        }

                        // Seed the per-sound drive-audio gains. Missing keys
                        // leave the defaults untouched (JsonValue getters do
                        // not write the out-param on failure).
                        {
                            double   motorV = Disk2AudioSource::kMotorVolume;
                            double   headV  = Disk2AudioSource::kHeadVolume;
                            double   doorV  = Disk2AudioSource::kDoorVolume;
                            HRESULT  hrVol  = S_OK;

                            hrVol = uiPrefs->GetNumber ("driveMotorVolume", motorV);
                            IGNORE_RETURN_VALUE (hrVol, S_OK);
                            hrVol = uiPrefs->GetNumber ("driveHeadVolume",  headV);
                            IGNORE_RETURN_VALUE (hrVol, S_OK);
                            hrVol = uiPrefs->GetNumber ("driveDoorVolume",  doorV);
                            IGNORE_RETURN_VALUE (hrVol, S_OK);

                            SetDriveAudioVolumes ((float) motorV, (float) headV, (float) doorV);
                        }

                        // Seed the per-drive stereo pans. Missing keys
                        // leave the defaults untouched.
                        {
                            double   pan0  = DriveAudioMixer::kDefaultDriveOnePan;
                            double   pan1  = DriveAudioMixer::kDefaultDriveTwoPan;
                            HRESULT  hrPan = S_OK;

                            hrPan = uiPrefs->GetNumber ("driveOnePan", pan0);
                            IGNORE_RETURN_VALUE (hrPan, S_OK);
                            hrPan = uiPrefs->GetNumber ("driveTwoPan", pan1);
                            IGNORE_RETURN_VALUE (hrPan, S_OK);

                            SetDriveAudioPan (0, (float) pan0);
                            SetDriveAudioPan (1, (float) pan1);
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
    HRESULT                       hr                = S_OK;
    UINT                          dpi               = 0;
    int                           clientW           = 0;
    int                           clientH           = 0;
    RECT                          rc                = {};
    DWORD                         style             = 0;
    DWORD                         adjustStyle       = 0;
    BOOL                          fSuccess          = FALSE;
    RECT                          work              = {};
    HMONITOR                      activeMon         = nullptr;
    LONG                          windowX           = CW_USEDEFAULT;
    LONG                          windowY           = CW_USEDEFAULT;
    int                           windowW           = 0;
    int                           windowH           = 0;
    bool                          hadSavedPlacement = false;
    int                           iconBigSize       = 0;
    int                           iconSmallSize     = 0;
    HICON                         hIconBig          = nullptr;
    HICON                         hIconSm           = nullptr;
    DxuiHwndSource::CreateParams  params;



    m_hInstance = hInstance;

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

    // Seed our authoritative DPI so the chrome-band dock (which scales
    // band thicknesses through it) returns coherent sizes during the
    // pre-Create math.
    // WM_NCCREATE will overwrite this with GetDpiForWindow once the
    // HWND exists; that value wins if it disagrees.
    m_scaler.SetDpi (dpi);

    {
        SIZE  client = ClientSizeForFramebufferPx (kFramebufferWidth, kFramebufferHeight);

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
    // dispatches the action for the captioned buttons. The style
    // mirrors what DxuiHwndSource::Create uses internally for
    // borderless + resizable windows so the AdjustWindowRectExForDpi
    // math below produces the right window-pixel rect for the same
    // NC layout the host will create.
    style    = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    // Strip WS_CAPTION for the rect-adjust math because the
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

    // Preload the app icons so DxuiHwndSource::Create can attach them
    // via WM_SETICON before the window is shown. The taskbar and
    // Win32 MessageBox dialogs pick the icon up from WM_GETICON, not
    // WNDCLASS::hIcon, so the explicit handoff is required.
    iconBigSize   = GetSystemMetrics (SM_CXICON);
    iconSmallSize = GetSystemMetrics (SM_CXSMICON);
    hIconBig      = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO),
                                        IMAGE_ICON, iconBigSize, iconBigSize,
                                        LR_DEFAULTCOLOR | LR_SHARED);
    hIconSm       = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO),
                                        IMAGE_ICON, iconSmallSize, iconSmallSize,
                                        LR_DEFAULTCOLOR | LR_SHARED);

    // Hand the pre-computed window-pixel placement and chrome flags
    // to DxuiHwndSource. createSwapChain = true so the host owns the
    // D3D11 device + DXGI flip-discard swap chain and runs the panel-
    // tree paint pump; the Apple ][ framebuffer renderer composites
    // into that same back buffer via the before-present hook (wired in
    // Initialize), and chrome paints on top via the adopted controls.
    // The legacy CassoRenderSurface child HWND is gone -- a single
    // window proc now owns all mouse / NC / cursor handling.
    params.title                  = L"Casso";
    params.hInstance              = hInstance;
    params.ownerHwnd              = nullptr;
    params.borderless             = true;
    params.resizable              = true;
    params.roundedCorners         = true;
    params.darkMode               = true;
    params.backdrop               = DxuiHwndSourceBackdrop::None;
    params.resizeBorderDip        = 6.0f;
    params.classNameOverride      = kWindowClass;
    params.useInitialWindowRectPx = true;
    params.initialWindowRectPx    = { windowX, windowY, windowX + windowW, windowY + windowH };
    params.appIconBig             = hIconBig;
    params.appIconSmall           = hIconSm;
    params.createSwapChain        = true;
    params.captionStyle           = DxuiCaptionStyle::Standard;

    m_host = std::make_unique<DxuiHwndSource>();

    // Install ourselves as the IDxuiHostClient BEFORE Create so the
    // WM_NCCREATE / WM_CREATE / WM_SIZE / WM_MOVE sequence that fires
    // synchronously inside CreateWindowExW dispatches through our
    // OnXxx handlers (matches the legacy Window::Create behavior).
    m_host->SetClient (this);

    hr = m_host->Create (params);
    CHR (hr);

    m_hwnd = m_host->Hwnd();
    m_scaler.SetDpi (GetDpiForWindow (m_hwnd));

    // The caption (title + icon + min/max/close) is owned and rendered
    // by the host (CreateParams::captionStyle == Standard), which also
    // classifies the caption / system-button / resize-edge NC hits --
    // so no SetHitTestDelegate is installed. The host's DxuiSystemButton
    // children dispatch min/max/close themselves.

    // Stand up the host root panel as a DxuiAbsoluteLayout container
    // and add a single DxuiViewport child representing the Apple ][
    // framebuffer region. EmulatorShell hand-computes the viewport
    // rectangle (client minus chrome bands) every time chrome layout
    // changes; the viewport's OnBoundsChanged callback forwards the
    // new rect to D3DRenderer::SetTargetBounds. Full DxuiDockLayout
    // wiring lands in Phase 12.
    m_host->Root().SetLayout (std::make_unique<DxuiAbsoluteLayout>());
    m_viewport = &m_host->Root().Add<DxuiViewport>();
    m_viewport->SetOnBoundsChanged ([this] (const RECT & boundsPx)
    {
        this->OnViewportBoundsChanged (boundsPx);
    });

    // Route the guest's raw keyboard through the viewport's input sink
    // (FR-034). SetWantsAllKeys makes it a greedy surface so even the
    // Dxui-reserved navigation keystrokes (Esc / Tab / arrows) reach the
    // //e -- the chrome's own keyboard escape routes are enforced by the
    // pre-checks in OnKeyDown / OnChar, upstream of this forward.
    m_viewport->SetInputSink (this);
    m_viewport->SetConsumesInput (true);
    m_viewport->SetWantsAllKeys (true);

    // Adopt the chrome controls (menu bar / drive widgets / joystick
    // toggle) into the host's root panel so they participate in the
    // host-owned paint, input, focus, theme, tick, and DPI walks.
    // Lifetime stays with EmulatorShell (chrome controls are members);
    // the panel just registers raw pointers. The host's WM_PAINT pump
    // (createSwapChain = true) now paints these adopted controls on top
    // of the Apple ][ framebuffer each frame. The title bar is NOT here:
    // the host owns the caption strip itself.
    m_host->Root().Adopt (m_mainMenu);
    m_host->Root().Adopt (m_driveBandSurface);
    m_host->Root().Adopt (m_driveChrome[0]);
    m_host->Root().Adopt (m_driveChrome[1]);
    m_host->Root().Adopt (m_printerIndicator);
    m_host->Root().Adopt (m_joystickButton);

    // Give the host the chrome theme so its paint pump renders the
    // adopted chrome -- PaintPump no-ops when no theme is set.
    // m_chromeTheme is reassigned in place on theme switches, so this
    // pointer stays valid and the host reads the updated palette on the
    // next paint.
    m_host->SetTheme (&m_chromeTheme);

    // Route the menu bar's open submenu through the host popup pool so
    // the dropdown renders as a real top-level window (escapes the
    // client area + occludes). The strip stays in-window. The
    // full-ownership host owns the device, so its pool makes real popups.
    m_mainMenu.SetPopupHost (m_host.get());

    // The joystick-button hover tooltip renders through the host popup
    // pool too; its dwell timer is driven from the main frame loop's
    // Tick. SetTheme seeds the tooltip surface colours.
    m_joystickTooltip.SetPopupHost (m_host.get());
    m_joystickTooltip.SetTheme     (m_chromeTheme);

    // Defer the size reconcile until after ShowWindow. The NC frame
    // (border carve-out from DefWindowProc + DWM rounded corners +
    // thick frame) doesn't materialize until the window is shown,
    // so measuring NC overhead now returns 0 and the reconcile would
    // shrink the window to match the (wrong) measurement. The flag
    // tells ReconcileInitialClientSize whether to run; saved
    // placement deliberately bypasses the reset-to-default sizing.
    m_initialSizeReconciled = hadSavedPlacement;

    // Legacy Win32 menu bar is retired (FR-026). All menu
    // commands now route through `MainMenu` + the native nav strip;
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
            m_scaler.SetDpi (dpi);
        }
    }
    {
        RECT  menuBarBounds = { 0, m_host->CaptionHeightPx(), clientW, m_host->CaptionHeightPx() };

        m_mainMenu.Layout (menuBarBounds, m_scaler);
    }
    m_mainMenu.SetDispatch ([this] (WORD commandId) { HandleCommand (commandId); });
    m_mainMenu.SetCheckQuery ([this] (WORD commandId) -> bool
    {
        switch (commandId)
        {
            case IDM_MACHINE_ARROWS_JOYSTICK: return m_inputMode == InputMappingMode::Joystick;
            case IDM_MACHINE_ARROWS_PADDLE:   return m_inputMode == InputMappingMode::Paddle;
            default:                          return false;
        }
    });

    // Load the app icon (IDI_CASSO) into a premultiplied BGRA8 pixel
    // buffer and hand it to the host caption (like WM_SETICON for the
    // window glyph). Loaded at 32x32 (high enough to look crisp at
    // typical caption sizes when D2D linearly downscales it); failure
    // is non-fatal -- the caption simply omits the icon if it misses.
    {
        std::vector<uint32_t>  iconPixels;
        int                    iconW = 0;
        int                    iconH = 0;

        if (LoadIconAsPremulBgra (hInstance, IDI_CASSO, 32, iconPixels, iconW, iconH))
        {
            m_host->SetCaptionIcon (std::move (iconPixels), iconW, iconH);
        }
    }
    m_driveChrome[0].Initialize (6, 0, this);
    m_driveChrome[1].Initialize (6, 1, this);
    {
        RECT  vr            = ComputeViewportRect (clientW, clientH);
        int   bottomInsetPx = clientH - vr.bottom;

        LayoutDriveWidgetsInCommandBar (m_driveChrome, bottomInsetPx, clientW, clientH, dpi);
        LayoutPrinterIndicator (bottomInsetPx, clientW, clientH, dpi);
        {
            int  bandTop    = clientH - bottomInsetPx;
            int  bandHeight = MulDiv (s_kJoystickButtonBandDp, static_cast<int> (dpi), s_kBaseDpi);

            m_driveBandSurface.SetBounds (RECT{ 0, bandTop, clientW, clientH });
            LayoutJoystickButton (clientW, bandTop, bandHeight, dpi);
        }
    }

    UpdateViewportLayout (clientW, clientH);

    // Load accelerator table
    m_accelTable = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR));
    CWRA (m_accelTable);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateViewportLayout
//
//  Computes the Apple ][ viewport rectangle from the current client
//  width / height via the chrome-band DxuiDockLayout (top + bottom
//  insets), then invokes DxuiViewport::Layout on the host root panel's
//  viewport child. The viewport's bounds-changed callback fires when
//  the rectangle differs from the last value reported, forwarding
//  the new rect to D3DRenderer::SetTargetBounds via
//  OnViewportBoundsChanged.
//
//  Skipped silently when the viewport has not yet been wired (early
//  init paths, or when the host root panel was torn down).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateViewportLayout (int widthPx, int heightPx)
{
    HRESULT  hr           = S_OK;
    RECT     viewportRect = {};


    BAIL_OUT_IF (m_viewport == nullptr, S_OK);

    viewportRect = ComputeViewportRect (widthPx, heightPx);
    m_viewport->Layout (viewportRect, m_scaler);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncChromeBands
//
//  Stamps each chrome band's Bounds() height with its current DPI-scaled
//  pixel thickness so DxuiDockLayout reads the right slab extents. Only
//  the docked axis (height, for the Top/Bottom bands) is meaningful; the
//  bands are never painted.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncChromeBands ()
{
    // When the machine has no Disk ][ controller, remove the drive-widget area
    // entirely (#84 Phase D): the drive band collapses to just the joystick-mode
    // button band (joystick input is independent of disk presence), reclaiming
    // the ~180 dp the drive widgets + their in-use indicators would occupy so
    // the emulator viewport grows into it. The widgets are already hidden and
    // un-hit-tested by the resize path when there is no controller.
    bool  hasDisk     = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
    int   driveBandDp = hasDisk ? m_driveBarThicknessDp : s_kJoystickButtonBandDp;

    m_titleBand.SetBounds (RECT{ 0, 0, 0, m_scaler.Px (s_kTitleBarBandDp) });
    m_navBand.SetBounds   (RECT{ 0, 0, 0, m_scaler.Px (s_kNavStripBandDp) });
    m_driveBand.SetBounds (RECT{ 0, 0, 0, m_scaler.Px (driveBandDp) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ComputeViewportRect
//
//  Docks the chrome bands (title + nav on top, drive on the bottom)
//  around a Fill center over the client rect and returns the center
//  (emulator viewport) rect the dock leaves in the middle.
//
////////////////////////////////////////////////////////////////////////////////

RECT EmulatorShell::ComputeViewportRect (int widthPx, int heightPx)
{
    IDxuiControl *  kids[] = { &m_titleBand, &m_navBand, &m_driveBand, &m_centerBand };



    SyncChromeBands();
    m_chromeDock.Arrange (RECT{ 0, 0, widthPx, heightPx }, m_scaler, kids);

    return m_centerBand.Bounds();
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::EmulatorContentScreenRect
//
//  The emulator viewport in screen pixels, for the Settings live-preview
//  compositor's see-through reveal (#8). Recompute the viewport at the live
//  back-buffer size (client == device pixels; per-monitor-DPI aware) and map
//  the two corners through the main window's client origin into screen space.
//  Empty until the window + swap chain exist.
//
////////////////////////////////////////////////////////////////////////////////

RECT EmulatorShell::EmulatorContentScreenRect ()
{
    RECT   result = {};
    POINT  tl     = {};
    POINT  br     = {};

    if (m_hwnd == nullptr)
    {
        return result;
    }

    int  widthPx  = (int) m_d3dRenderer.GetBackBufferWidth();
    int  heightPx = (int) m_d3dRenderer.GetBackBufferHeight();
    if (widthPx <= 0 || heightPx <= 0)
    {
        return result;
    }

    RECT  vr = ComputeViewportRect (widthPx, heightPx);   // main-window client px
    tl = POINT{ vr.left,  vr.top    };
    br = POINT{ vr.right, vr.bottom };
    ClientToScreen (m_hwnd, &tl);
    ClientToScreen (m_hwnd, &br);

    result = RECT{ tl.x, tl.y, br.x, br.y };
    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReflowChromeForMachineChange
//
//  A machine switch may add or remove the Disk ][ controller, which changes the
//  drive-band thickness (Phase D), the drive-widget visibility, and the hit-test
//  map. When disk presence changes, grow/shrink the WINDOW by the band delta so
//  the emulator viewport keeps its size and the top-left corner stays put -- NOT
//  hold the window size and re-centre the viewport. The resulting WM_SIZE drives
//  OnSize, which re-lays the bands / widgets / hit rects. When presence is
//  unchanged (e.g. a swap between two controller-equipped machines) there is no
//  band delta, so just re-run OnSize at the current size to refresh the widgets.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReflowChromeForMachineChange ()
{
    RECT  rcWindow = {};

    if (m_hwnd == nullptr || !GetWindowRect (m_hwnd, &rcWindow))
    {
        return;
    }

    bool  newHasDisk      = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
    bool  presenceChanged = (newHasDisk != m_chromeSizedForHasDisk);

    // Resize the window by the band delta -- but not for min/max/fullscreen
    // windows, where the user explicitly chose the size (mirrors
    // ApplyThemeToChrome). Those just relayout inside the fixed frame.
    if (presenceChanged &&
        !IsIconic (m_hwnd) && !IsZoomed (m_hwnd) && !m_d3dRenderer.IsFullscreen())
    {
        int  oldBandDp = m_chromeSizedForHasDisk ? m_driveBarThicknessDp : s_kJoystickButtonBandDp;
        int  newBandDp = newHasDisk              ? m_driveBarThicknessDp : s_kJoystickButtonBandDp;
        int  deltaPx   = m_scaler.Px (newBandDp) - m_scaler.Px (oldBandDp);

        m_chromeSizedForHasDisk = newHasDisk;

        // The drive band is bottom-docked full-width, so only the height moves.
        // SWP_NOMOVE pins the top-left corner; the WM_SIZE it generates drives
        // OnSize to re-lay the bands, widgets, and hit-test map.
        SetWindowPos (m_hwnd, nullptr, 0, 0,
                      rcWindow.right  - rcWindow.left,
                      (rcWindow.bottom - rcWindow.top) + deltaPx,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    m_chromeSizedForHasDisk = newHasDisk;

    // No band delta (unchanged presence, or a fixed-state window): relayout at
    // the current client size so widget visibility + hit rects still refresh.
    {
        RECT  rcClient = {};

        if (GetClientRect (m_hwnd, &rcClient) &&
            rcClient.right > rcClient.left && rcClient.bottom > rcClient.top)
        {
            (void) OnSize (static_cast<UINT> (rcClient.right  - rcClient.left),
                           static_cast<UINT> (rcClient.bottom - rcClient.top));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ClientSizeForCenterPx
//
//  Inverse of ComputeViewportRect: given a desired center (emulator
//  viewport) size in physical pixels, return the client size that hosts
//  it with the current chrome-band thicknesses.
//
////////////////////////////////////////////////////////////////////////////////

SIZE EmulatorShell::ClientSizeForCenterPx (int centerWidthPx, int centerHeightPx)
{
    IDxuiControl *  bands[] = { &m_titleBand, &m_navBand, &m_driveBand };



    SyncChromeBands();

    return m_chromeDock.ContainerSizeForFill (SIZE{ centerWidthPx, centerHeightPx }, bands);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ClientSizeForFramebufferPx
//
//  Framebuffer scale policy: linear DPI scaling. The Apple ][ pixel grid
//  (given in DIPs) scales at the same rate as the chrome dp, so the
//  framebuffer and chrome insets stay in proportion at every DPI. Both
//  the initial window size and Ctrl+0 reset go through here.
//
////////////////////////////////////////////////////////////////////////////////

SIZE EmulatorShell::ClientSizeForFramebufferPx (int framebufferWidthDp, int framebufferHeightDp)
{
    return ClientSizeForCenterPx (m_scaler.Px (framebufferWidthDp),
                                  m_scaler.Px (framebufferHeightDp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportBoundsChanged
//
//  Bounds-changed callback for the DxuiViewport child of the host's
//  root panel. Stores the new pixel rectangle and forwards it to
//  D3DRenderer::SetTargetBounds. Today the rect is parked on the
//  renderer (no behavior change); the renderer consumes it once the
//  swap-chain restructure completes later in Phase 11d.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnViewportBoundsChanged (const RECT & boundsPx)
{
    m_viewportBoundsPx = boundsPx;
    m_d3dRenderer.SetTargetBounds (boundsPx);
    m_d3dRenderer.MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReconcileInitialClientSize
//
//  Run once after ShowWindow to size the window so its client area
//  matches what the chrome-band dock wants for the framebuffer. Must
//  run POST-ShowWindow because the NC frame (DefWindowProc border carve-
//  out + DWM rounded corners) doesn't materialize until the window
//  is visible; measuring NC overhead before that returns 0 and the
//  reconcile would shrink the window to match the (wrong) measurement.
//  Idempotent via m_initialSizeReconciled.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReconcileInitialClientSize()
{
    SIZE  desired        = {};
    RECT  rcActualClient = {};
    RECT  rcActualWindow = {};
    int   ncOverheadW    = 0;
    int   ncOverheadH    = 0;
    int   desiredClientW = 0;
    int   desiredClientH = 0;
    int   fixedW         = 0;
    int   fixedH         = 0;



    if (m_initialSizeReconciled || m_hwnd == nullptr)
    {
        return;
    }

    m_initialSizeReconciled = true;

    desired         = ClientSizeForFramebufferPx (kFramebufferWidth, kFramebufferHeight);
    desiredClientW  = (int) desired.cx;
    desiredClientH  = (int) desired.cy;

    // Force a fresh WM_NCCALCSIZE so DefWindowProc carves the actual
    // thick-frame borders into the client rect. Without this, the
    // post-ShowWindow GetClientRect returns the full window rect
    // (NC overhead = 0) and the reconcile math thinks no resize is
    // needed -- leaving the emulator pixel grid undersized by the
    // border width on the eventual first NCCALCSIZE.
    SetWindowPos (m_hwnd, nullptr, 0, 0, 0, 0,
                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    if (!GetClientRect (m_hwnd, &rcActualClient) || !GetWindowRect (m_hwnd, &rcActualWindow))
    {
        return;
    }

    ncOverheadW = (rcActualWindow.right  - rcActualWindow.left)
                  - (rcActualClient.right  - rcActualClient.left);
    ncOverheadH = (rcActualWindow.bottom - rcActualWindow.top)
                  - (rcActualClient.bottom - rcActualClient.top);

    fixedW = desiredClientW + ncOverheadW;
    fixedH = desiredClientH + ncOverheadH;

    if (fixedW != (rcActualWindow.right  - rcActualWindow.left) ||
        fixedH != (rcActualWindow.bottom - rcActualWindow.top))
    {
        // Recenter on the current monitor's work area using the final
        // size. The initial Create centered using a pre-reconcile size
        // estimate; without this re-center the reconcile resize would
        // grow the window from its top-left and leave it visually off
        // center vs Ctrl+0 reset (which centers with the final size).
        HMONITOR    hMon = MonitorFromWindow (m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi   = { sizeof (mi) };
        int         x    = 0;
        int         y    = 0;
        UINT        flags = SWP_NOZORDER | SWP_NOACTIVATE;

        if (hMon != nullptr && GetMonitorInfo (hMon, &mi))
        {
            x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - fixedW) / 2;
            y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - fixedH) / 2;
        }
        else
        {
            flags |= SWP_NOMOVE;
        }

        SetWindowPos (m_hwnd, nullptr, x, y, fixedW, fixedH, flags);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMove (int x, int y)
{
    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);

    if (m_mainMenu.IsOpen())
    {
        m_mainMenu.Hide();
    }

    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNotify
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnNotify (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    return DxuiMessageResult::NotHandled;
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
    HRESULT  hr = S_OK;



    hr = m_diskManager->Mount (slot, drive, path);
    CHR (hr);

    RecordRecentDisk (path);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordRecentDisk
//
//  Push a successfully-mounted disk image onto the recent-disks MRU
//  and persist the updated prefs. Best-effort; failures are swallowed
//  so an MRU write hiccup never blocks a successful mount.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordRecentDisk (const std::wstring & path)
{
    HRESULT                    hr     = S_OK;
    DiskMru                    mru;
    std::filesystem::path      fsPath;
    std::vector<std::string>   serialized;
    std::vector<std::int64_t>  loadedAt;
    std::int64_t               nowUnix = 0;



    BAIL_OUT_IF (path.empty(), S_OK);

    nowUnix = (std::int64_t) std::chrono::duration_cast<std::chrono::seconds> (
                  std::chrono::system_clock::now().time_since_epoch()).count();

    fsPath = std::filesystem::path (path);
    mru    = DiskMru::FromUtf8 (m_globalPrefs.recentDisks, m_globalPrefs.recentDiskLoadedAt);
    mru.RecordMount (fsPath, nowUnix);
    mru.ToUtf8 (serialized, loadedAt);
    m_globalPrefs.recentDisks        = std::move (serialized);
    m_globalPrefs.recentDiskLoadedAt = std::move (loadedAt);

    SaveGlobalPrefs();

Error:
    return;
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
//  BrowseForDisk
//
//  UI helper: open the drive door for visual feedback, show the
//  file-open dialog, then restore the door to match the mount
//  state. Empty drives leave the door open (matches real Disk II
//  empty-drive visual); mounted drives close it back.
//
//  Mount-on-success runs through DiskManager::Mount, which queues
//  to the CPU thread and posts a DoorClose sync event picked up
//  by UpdateDriveWidgets -- so we don't need to touch the door on
//  the success path here; BeginInsert closes it naturally.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseForDisk (int drive)
{
    using std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto                   nowMs      = []() -> int64_t {
        return (int64_t) duration_cast<milliseconds> (steady_clock::now().time_since_epoch()).count();
    };
    DriveWidgetState *     pSt        = nullptr;
    int64_t                now        = 0;
    int64_t                deadline   = 0;
    HRESULT                hrBrowse   = S_OK;
    MSG                    msg        = {};



    if (drive < 0 || drive >= (int) m_driveWidgetState.size())
    {
        return;
    }

    pSt = &m_driveWidgetState[drive];
    now = nowMs();

    // Only the closed / closing door will visibly animate open. An empty
    // drive rests with its door already Open (Door::Open is the default),
    // so StartDoorTransition is a no-op there and blocking for the
    // animation would just be dead time before the picker appears. Gate
    // the wait on whether an animation will actually play.
    const bool  doorWillAnimate =
        (pSt->doorState == DriveWidgetState::Door::Closed ||
         pSt->doorState == DriveWidgetState::Door::Closing);

    pSt->StartDoorTransition (DriveWidgetState::Door::Opening, now);
    m_d3dRenderer.MarkRedrawNeeded();

    // Let the door animation finish so the user actually sees it open
    // before the modal picker covers the drive. The host paint pump runs
    // on the UI thread (driven by WM_PAINT), so we both pump Windows
    // messages AND request a frame here -- the CPU emulation thread is
    // separate, and the chrome / framebuffer composite only happens from
    // the host pump on the UI thread we're currently blocking. The time
    // base MUST match DiskManager::NowMs (steady_clock ms) because
    // TickDoorAnimation diffs the current frame time against
    // animationStartTimeMs. Skipped entirely when the door is already
    // open (nothing to animate) so the picker opens immediately.
    if (doorWillAnimate)
    {
        deadline = now + DriveWidgetState::kDoorAnimationMs;

        while (nowMs() < deadline)
        {
            while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage (&msg);
                DispatchMessageW (&msg);
            }

            // Tick the door animation and repaint through the host pump
            // (the after-blit hook that used to do this is gone). A nullptr
            // framebuffer re-composites the last emulator upload with the
            // chrome (animating door) painted on top.
            m_diskManager->UpdateDriveWidgets();
            m_pendingFramebuffer = nullptr;
            InvalidateRect (m_hwnd, nullptr, FALSE);
            UpdateWindow   (m_hwnd);
            Sleep (8);
        }
    }

    hrBrowse = m_windowCommandManager->PromptInsertDiskMru (drive + 1);
    IGNORE_RETURN_VALUE (hrBrowse, S_OK);

    // Cancel / error path: door follows mount state. Mounted drive
    // closes back, empty drive stays open. The success path is
    // handled asynchronously by BeginInsert when the queued mount
    // completes.
    if (hrBrowse != S_OK && pSt->IsMounted())
    {
        pSt->StartDoorTransition (DriveWidgetState::Door::Closing, nowMs());
        m_d3dRenderer.MarkRedrawNeeded();
    }
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
//  EmulatorShell::ApplyThemeLive
//
//  Activates the named theme via ThemeManager (which fires our chrome
//  cache listener and reskins the live chrome) but does NOT write the
//  choice into GlobalUserPrefs -- so a Settings Cancel can revert to the
//  baseline theme without a persisted trace. Mirrors ApplyAndPersistTheme
//  minus the save. Unknown names fall back to Skeuomorphic.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::ApplyThemeLive (const std::string & themeName)
{
    HRESULT  hr         = S_OK;
    HRESULT  hrActivate = S_OK;


    if (themeName.empty() || m_themeManager == nullptr)
    {
        return S_FALSE;
    }

    hrActivate = m_themeManager->Activate (themeName);
    if (hrActivate != S_OK)
    {
        hrActivate = m_themeManager->Activate ("Skeuomorphic");
    }
    CHR (hrActivate);

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
//  EmulatorShell::ShowModalDialog
//
//  Shows the supplied dialog modally through the Dxui ShowModal host
//  path and blocks until the user dismisses it. Returns the chosen
//  button's resultCode, or -1 when the user closes via window gesture.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowModalDialog (const DialogDefinition & def)
{
    return ShowSimpleDialogViaDxui (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowSimpleDialogViaDxui
//
//  Translates a renderable DialogDefinition into a MessageDialog whose
//  content is a DialogBodyContent (wrapped body labels + hyperlink links)
//  plus the action buttons, and shows it modally via ShowModalDialog. The
//  dialog height is derived from the content's preferred (line-count)
//  height so short messages stay compact and long ones grow (clamped).
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowSimpleDialogViaDxui (const DialogDefinition & def)
{
    constexpr int      s_kDialogWidthDip  = 440;
    constexpr int      s_kChromeHeightDip = 108;   // caption + content pad*2 + button row
    constexpr int      s_kMinHeightDip    = 120;
    constexpr int      s_kMaxHeightDip    = 620;
    constexpr int      s_kIconSrcPx       = 256;
    constexpr int      s_kDefaultIconDip  = 48;
    constexpr int      s_kGlyphSizeDip    = 32;
    constexpr wchar_t  s_kchGlyphInfo     = L'\uE946';   // MDL2 Info
    constexpr wchar_t  s_kchGlyphWarning  = L'\uE7BA';   // MDL2 Warning
    constexpr wchar_t  s_kchGlyphError    = L'\uEA39';   // MDL2 ErrorBadge
    constexpr uint32_t s_kGlyphArgbInfo   = 0xFF4A9EDB;
    constexpr uint32_t s_kGlyphArgbWarning = 0xFFF5A623;
    constexpr uint32_t s_kGlyphArgbError  = 0xFFE5424D;

    std::unique_ptr<DialogBodyContent>  content   = std::make_unique<DialogBodyContent>();
    MessageDialog                       dlg;
    DxuiWindow::CreateParams            params;
    std::vector<MessageDialog::Button>  buttons;
    HRESULT                             hr        = S_OK;
    int                                 heightDip = 0;
    int                                 result    = -1;


    content->SetRuns (def.body);

    if (def.icon == DialogIcon::AppPhotoreal || def.icon == DialogIcon::AppFlat)
    {
        std::vector<uint32_t>  iconPixels;
        int                    iconW   = 0;
        int                    iconH   = 0;
        int                    iconRes = (def.icon == DialogIcon::AppPhotoreal) ? IDI_CASSO_PHOTOREAL : IDI_CASSO_FLAT_COLOR_HEAD;
        int                    iconDip = (def.iconSizeOverrideDp > 0.0f) ? (int) def.iconSizeOverrideDp : s_kDefaultIconDip;


        if (LoadIconAsPremulBgra (m_hInstance, iconRes, s_kIconSrcPx, iconPixels, iconW, iconH))
        {
            content->SetIcon (std::move (iconPixels), iconW, iconH, iconDip);
        }
    }
    else if (def.icon == DialogIcon::Info)
    {
        content->SetGlyphIcon (s_kchGlyphInfo, s_kGlyphArgbInfo, s_kGlyphSizeDip);
    }
    else if (def.icon == DialogIcon::Warning)
    {
        content->SetGlyphIcon (s_kchGlyphWarning, s_kGlyphArgbWarning, s_kGlyphSizeDip);
    }
    else if (def.icon == DialogIcon::Error)
    {
        content->SetGlyphIcon (s_kchGlyphError, s_kGlyphArgbError, s_kGlyphSizeDip);
    }

    heightDip = std::clamp (s_kChromeHeightDip + content->PreferredHeightDip(),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    for (const DialogButton & button : def.buttons)
    {
        buttons.push_back ({ button.label, button.resultCode, button.isDefault, button.isCancel });
    }

    dlg.Configure (std::move (content), std::move (buttons), def.closeBoxResult.value_or (-1));

    params.title                    = def.title;
    params.hInstance                = m_hInstance;
    params.ownerHwnd                = m_hwnd;
    params.initialSizeDip           = { s_kDialogWidthDip, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    hr = dlg.Create (params);
    CHRA (hr);

    dlg.SetTheme (&m_chromeTheme);

    result = dlg.TranslateResult (dlg.ShowModalDialog (dlg.DefaultCommandId()));

Error:
    return result;
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

void EmulatorShell::ApplyThemeToChrome (const CassoTheme & theme)
{
    // Drive-bar slot thickness. Skeuomorphic shows the full 3D drive
    // body (160 px) plus ~30 px of vertical slack and the basename
    // Bottom drive-bar thickness, full and compact. Layout: drive widget
    // (body + label strip + 2 dp bottom margin) bottom-anchored, with a
    // ~43 dp band above for the joystick-mode toggle button (8 dp gap +
    // ~27 dp button + 8 dp gap). Drive widget total height is body 160 +
    // label-strip gap 2 + label strip 18 = 180 dp (full) / 60 dp (compact).
    constexpr int  s_kFullDriveBarDp    = 225;
    constexpr int  s_kCompactDriveBarDp = 105;

    int   desiredThicknessDp = theme.compactDrives ? s_kCompactDriveBarDp : s_kFullDriveBarDp;
    int   priorThicknessDp   = m_driveBarThicknessDp;
    RECT  rcClient           = {};
    RECT  rcWindow           = {};
    int   centerW            = 0;
    int   centerH            = 0;



    m_driveChrome[0].SetCompact (theme.compactDrives);
    m_driveChrome[1].SetCompact (theme.compactDrives);

    // Push the nav/dropdown palette onto the menu bar so both the
    // in-window strip and the popup-backed dropdown render with chrome
    // colours (the old per-frame apply path is dead post-T129).
    m_mainMenu.ApplyChromeColors (theme);

    if (m_hwnd == nullptr || desiredThicknessDp == priorThicknessDp)
    {
        m_driveBarThicknessDp = desiredThicknessDp;
        return;
    }

    // Skip the auto-resize for windows that are min/max/fullscreen --
    // the user explicitly chose those window states and shouldn't see
    // the window resize from under them on a theme swap. The new
    // chrome thickness still gets applied to the contributor below
    // so the next normal-state resize uses the right math.
    if (IsIconic (m_hwnd) || IsZoomed (m_hwnd) || m_d3dRenderer.IsFullscreen())
    {
        m_driveBarThicknessDp = desiredThicknessDp;
        return;
    }

    if (!GetClientRect (m_hwnd, &rcClient) || !GetWindowRect (m_hwnd, &rcWindow))
    {
        m_driveBarThicknessDp = desiredThicknessDp;
        return;
    }

    // Capture the current center (emulator viewport) size BEFORE
    // mutating the drive-bar thickness. The user may have resized the
    // window manually since boot; preserving "the emu viewport stays the
    // same size, the drive bar grows/shrinks around it" is the
    // intuitive contract on a theme swap.
    {
        RECT  before = ComputeViewportRect (rcClient.right  - rcClient.left,
                                            rcClient.bottom - rcClient.top);

        centerW = before.right  - before.left;
        centerH = before.bottom - before.top;
    }

    m_driveBarThicknessDp = desiredThicknessDp;

    {
        SIZE  newClient   = ClientSizeForCenterPx (centerW, centerH);
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
//  EmulatorShell::LayoutJoystickButton
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutJoystickButton (int clientW,
                                          int bandTopPx,
                                          int bandHeightPx,
                                          UINT dpi)
{
    int            centerX = clientW / 2;
    int            centerY = bandTopPx + bandHeightPx / 2;
    DxuiDpiScaler  scaler;
    RECT           anchor  = { centerX, centerY, centerX, centerY };



    m_joyBtnClientW    = clientW;
    m_joyBtnBandTop    = bandTopPx;
    m_joyBtnBandHeight = bandHeightPx;
    m_joyBtnDpi        = dpi;

    scaler.SetDpi (dpi);
    m_joystickButton.SetMode (m_inputMode);
    m_joystickButton.Layout (anchor, scaler);
    // m_joystickTooltip is a deferred popup: it derives its DPI from its
    // popup host (set via SetPopupHost) at show time, so no SetDpi here.
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RelayoutJoystickButton
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RelayoutJoystickButton ()
{
    if (m_joyBtnClientW <= 0)
    {
        return;
    }

    LayoutJoystickButton (m_joyBtnClientW, m_joyBtnBandTop, m_joyBtnBandHeight, m_joyBtnDpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LayoutPrinterIndicator
//
//  Anchors the printer status indicator in the command-bar dead space at the
//  right edge, vertically centred in the band. Positioned independently of the
//  centred drive widgets so it never shifts their layout. Hidden entirely when
//  the machine has no printer card.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutPrinterIndicator (int bottomInsetPx, int clientW, int clientH, UINT dpi)
{
    DxuiDpiScaler  scaler;
    int            desiredW      = 0;
    int            desiredH      = 0;
    int            driveRight    = 0;
    int            rightPad      = 0;
    int            avail         = 0;
    int            w             = 0;
    int            h             = 0;
    int            commandBarTop = 0;
    int            x             = 0;
    int            y             = 0;

    if (m_refs.printerCard == nullptr)
    {
        m_printerIndicator.Hide ();
        return;
    }

    scaler.SetDpi (dpi);

    desiredW = scaler.Px (76);
    desiredH = scaler.Px (58);

    // Fit inside the dead space to the right of the (centred) drive widgets so
    // the printer never overlaps a drive or shifts their centring. When the
    // drives are hidden the right edge is 0 and the full desired size is used.
    rightPad   = scaler.Px (10);
    driveRight = std::max (0, (int) m_driveChrome[1].OuterRect ().right);
    avail      = clientW - rightPad - driveRight - scaler.Px (10);

    w = std::min (desiredW, std::max (scaler.Px (40), avail));
    h = (w * desiredH) / std::max (1, desiredW);   // keep the wide-low aspect

    // Bottom-align to the same shelf as the drive bodies (their outer bottom is
    // clientH - the label bottom gap), so the low printer sits beside the tall
    // drives rather than floating.
    x             = std::max (0, clientW - rightPad - w);
    y             = (clientH - scaler.Px (s_kLabelBottomGapDp)) - h;
    commandBarTop = std::max (0, clientH - bottomInsetPx);

    if (y < commandBarTop)
    {
        y = commandBarTop;
    }

    m_printerIndicator.Layout (RECT{ x, y, x + w, y + h }, scaler);
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowPrinterPanel
//
//  Lazily creates the printer panel / print preview window, wires its toolbar
//  callbacks to the existing delivery commands, pushes a fresh strip snapshot,
//  and brings it to the foreground. Mirrors ShowDisk2Debug's create pattern.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowPrinterPanel ()
{
    HRESULT     hr        = S_OK;
    HINSTANCE   hInstance = nullptr;

    if (m_printerPanel == nullptr || m_printerPanel->Hwnd () == nullptr)
    {
        hInstance      = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_printerPanel = std::make_unique<PrinterPanel> ();

        hr = m_printerPanel->Create (hInstance,
                                     m_hwnd,
                                     m_d3dRenderer.GetDevice (),
                                     m_d3dRenderer.GetContext (),
                                     &m_chromeTheme);
        CHRF (hr, m_printerPanel.reset ());

        // Toolbar actions route through the existing command path (which
        // quiesces the worker, delivers/clears, and resumes), then re-snapshot.
        m_printerPanel->SetOnFinish ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_EJECT);
            SnapshotStripToPanel ();
        });
        m_printerPanel->SetOnCopy ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_COPY);
            SnapshotStripToPanel ();
        });
        m_printerPanel->SetOnDiscard ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_DISCARD);
            SnapshotStripToPanel ();
        });
        m_printerPanel->SetOnRefresh ([this] ()
        {
            SnapshotStripToPanel ();
        });
    }

    SnapshotStripToPanel ();

    m_printerPanel->Show ();
    SetForegroundWindow (m_printerPanel->Hwnd ());

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SnapshotStripToPanel
//
//  Reads the strip raster race-free: stop the drain worker, flush any tail
//  bytes, copy the raster, then resume the worker on the same page (reseeded).
//  Non-destructive -- the guest keeps printing onto the same strip.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SnapshotStripToPanel ()
{
    PrinterJob *  job = nullptr;
    PrintRaster   snapshot;

    if (m_printerPanel == nullptr || !m_printerPanel->IsOpen ())
    {
        return;
    }

    if (m_refs.printerCard == nullptr)
    {
        m_printerPanel->SetStrip (snapshot);   // empty
        return;
    }

    m_printerWorker.Stop ();

    {
        vector<PrinterEvent>   events;
        m_printerWorker.FlushNow (events);
    }

    job = m_printerWorker.Job ();

    if (job != nullptr)
    {
        snapshot = job->Raster ();   // copy
    }

    // Resume on the same page (seed copies into the worker; snapshot survives).
    m_printerWorker.Start (m_refs.printerCard->ByteRing (), snapshot);

    m_printerPanel->SetStrip (snapshot);
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdatePrinterIndicator
//
//  Samples the worker's thread-safe status signals, recomputes the indicator
//  state through the pure PrinterStatusModel, and marks a redraw only on a
//  change so a static screen still repaints the LED on a transition.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePrinterIndicator ()
{
    int64_t        nowMs  = 0;
    PrinterStatus  status = PrinterStatus::Idle;

    if (m_refs.printerCard == nullptr)
    {
        return;   // indicator is hidden; nothing to sample
    }

    nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                std::chrono::steady_clock::now ().time_since_epoch ()).count ();

    m_printerStatus.Update (m_printerWorker.ActivityCount (),
                            (double) nowMs,
                            m_printerWorker.HasContent (),
                            false /* error state not surfaced yet */);

    status = m_printerStatus.Status ();

    if (status != m_printerIndicator.Status ())
    {
        m_printerIndicator.SetStatus (status);
        m_d3dRenderer.MarkRedrawNeeded ();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetChromeFocusIndex
//
//  Move the keyboard chrome-focus ring to a new slot (-1 = guest) and refresh
//  which widget paints its focus visual.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetChromeFocusIndex (int index)
{
    m_chromeFocusIndex = index;
    UpdateChromeFocusVisuals();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdateChromeFocusVisuals
//
//  Push the current ring index into the MainMenu (focused-closed menu title),
//  the joystick-mode button, and the two drive widgets so exactly one of them
//  paints a focus ring.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateChromeFocusVisuals()
{
    int  index = m_chromeFocusIndex;


    if (index >= s_kChromeFocusMenuFirst && index <= s_kChromeFocusMenuLast)
    {
        m_mainMenu.SetFocusedMenu ((MainMenuId) index);
    }
    else
    {
        m_mainMenu.ClearFocus ();
    }

    m_joystickButton.SetFocused (index == s_kChromeFocusButton);
    m_driveChrome[0].SetFocused (index == s_kChromeFocusDrive0);
    m_driveChrome[1].SetFocused (index == s_kChromeFocusDrive1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HandleChromeFocusKey
//
//  Own every keydown while the chrome keyboard-focus ring is active. Tab /
//  Shift+Tab traverse the whole ring (menu titles -> button -> drives,
//  wrapping); Left/Right move among the menu titles; Enter/Space/Down open a
//  dropdown or activate the focused button/drive; Esc/F10 leave the ring. When
//  a dropdown is open, keys delegate to MainMenu and the index is reconciled
//  with whatever the menu did. Always consumes the key.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::HandleChromeFocusKey (WPARAM vk)
{
    bool  shift  = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    int   dir    = shift ? -1 : 1;
    int   index  = m_chromeFocusIndex;
    bool  exitVk = (vk == VK_ESCAPE || vk == VK_F10);


    // An open dropdown owns navigation; delegate and reconcile the ring.
    if (m_mainMenu.IsOpen())
    {
        bool  ringOwned = (m_chromeFocusIndex != s_kChromeFocusNone);
        int   openIdx   = (int) m_mainMenu.OpenMenu();

        m_mainMenu.HandleKey (vk);

        if (m_mainMenu.IsOpen())
        {
            // Still open: a ring-owned menu tracks the (possibly switched)
            // title. A menu opened outside the ring (Alt mnemonic / mouse)
            // stays un-owned so closing it returns to the guest rather than
            // stranding focus on a title the user never Tab'd to.
            if (ringOwned)
            {
                SetChromeFocusIndex ((int) m_mainMenu.OpenMenu());
            }
        }
        else if (exitVk && ringOwned)
        {
            // Esc/F10 closed a ring-opened dropdown: keep the title focused.
            SetChromeFocusIndex (openIdx);
        }
        else
        {
            // Dispatched a command (or closed a menu the ring never owned):
            // hand focus back to the guest.
            SetChromeFocusIndex (s_kChromeFocusNone);
        }

        return true;
    }

    if (exitVk)
    {
        SetChromeFocusIndex (s_kChromeFocusNone);
        return true;
    }

    if (vk == VK_TAB)
    {
        SetChromeFocusIndex ((index + dir + s_kChromeFocusCount) % s_kChromeFocusCount);
        return true;
    }

    // A menu title is focused with its dropdown closed.
    if (index >= s_kChromeFocusMenuFirst && index <= s_kChromeFocusMenuLast)
    {
        if (vk == VK_LEFT)
        {
            SetChromeFocusIndex ((index == s_kChromeFocusMenuFirst) ? s_kChromeFocusMenuLast : index - 1);
        }
        else if (vk == VK_RIGHT)
        {
            SetChromeFocusIndex ((index == s_kChromeFocusMenuLast) ? s_kChromeFocusMenuFirst : index + 1);
        }
        else if (vk == VK_DOWN || vk == VK_RETURN || vk == VK_SPACE)
        {
            m_mainMenu.Open ((MainMenuId) index, true);
        }

        return true;
    }

    // The joystick-mode button or a drive widget is focused. Left/Right also
    // walk the ring so horizontal arrows feel natural along the bottom bar.
    if (vk == VK_LEFT)
    {
        SetChromeFocusIndex ((index - 1 + s_kChromeFocusCount) % s_kChromeFocusCount);
        return true;
    }

    if (vk == VK_RIGHT)
    {
        SetChromeFocusIndex ((index + 1) % s_kChromeFocusCount);
        return true;
    }

    if (vk == VK_RETURN || vk == VK_SPACE)
    {
        if (index == s_kChromeFocusButton)
        {
            CycleInputMappingMode();
        }
        else if (index == s_kChromeFocusDrive0)
        {
            BrowseForDisk (m_driveChrome[0].Drive());
        }
        else if (index == s_kChromeFocusDrive1)
        {
            BrowseForDisk (m_driveChrome[1].Drive());
        }
    }

    return true;
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
//  EmulatorShell::SetColorMonitorTextArgbLive
//
//  Updates the Color-monitor text color read by RenderFramebuffer on the
//  next frame. Forces opaque alpha so a stray transparent value can't blank
//  the text.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetColorMonitorTextArgbLive (uint32_t argb)
{
    m_colorMonitorTextArgb.store (0xFF000000u | (argb & 0x00FFFFFFu), std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
// Posted (not sent) to the shell HWND to marshal a window-title refresh
// onto the UI thread when UpdateWindowTitle is called from the CPU thread
// (SwitchMachine). Drained by RunMessageLoop before DispatchMessage.
//
////////////////////////////////////////////////////////////////////////////////

#define WM_APP_DXUI_UPDATE_TITLE (WM_APP + 0x21)





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
        // Destroy a closed modeless settings sheet at a safe point: its
        // EndDialog callback ran deep inside DispatchMessage, so deferring the
        // reset here avoids tearing the window down from its own message handler.
        if (m_settingsSheetClosePending)
        {
            m_settingsSheet.reset();
            m_settingsSheetClosePending = false;
        }

        // Process all pending messages
        while (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_cpuManager.Stop();
                return static_cast<int> (msg.wParam);
            }

            // Title refresh marshaled from a non-UI thread (SwitchMachine
            // runs on the CPU thread; DxuiHwndSource::SetTitle is UI-only).
            // This message is posted only by a completed machine switch, so
            // it doubles as the signal to reflow the chrome for a possible
            // Disk ][ controller add/remove (the window size is unchanged, so
            // no WM_SIZE / OnSize would otherwise re-evaluate it).
            if (msg.message == WM_APP_DXUI_UPDATE_TITLE)
            {
                UpdateWindowTitle();
                ReflowChromeForMachineChange();
                continue;
            }

            // Modeless Settings: let the sheet claim its dialog-navigation keys
            // (Tab / Enter / Escape) first (Dxui's IsDialogMessage equivalent).
            if (m_settingsSheet != nullptr && m_settingsSheet->ProcessDialogMessage (msg))
            {
                continue;
            }

            // Suppress the emulator's accelerators while the settings sheet is
            // the active window, so keystrokes meant for it (the colour-picker
            // hex field, Ctrl chords) never leak into emulator menu commands.
            bool  settingsActive = (m_settingsSheet != nullptr &&
                                    m_settingsSheet->Hwnd() == GetActiveWindow());

            if (settingsActive ||
                m_accelTable == nullptr ||
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
        // Per-UI-frame chrome upkeep that used to live in the after-blit
        // hook: advance drive-door animations and force a present while a
        // door is mid-transition so the chrome keeps repainting even when
        // the emulator framebuffer is static.
        if (m_diskManager != nullptr)
        {
            m_diskManager->UpdateDriveWidgets();
        }
        for (const DriveWidgetState & st : m_driveWidgetState)
        {
            if (st.doorState == DriveWidgetState::Door::Opening ||
                st.doorState == DriveWidgetState::Door::Closing)
            {
                m_d3dRenderer.MarkRedrawNeeded();
                break;
            }
        }

        if (m_disk2DebugPanel != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, m_disk2DebugPanel->RenderFrame());
        }
        if (m_inputDebugPanel != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, m_inputDebugPanel->RenderFrame());
        }
        if (m_printerPanel != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, m_printerPanel->RenderFrame());
        }
        if (m_mainMenu.IsOpen())
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }

        // While the modeless Settings sheet is open, force a present every UI
        // frame so live Display edits (brightness / contrast / scanlines / text
        // color) reflect in the emulator instantly. The retired SettingsWindow
        // was rendered inline in this loop each frame, which coupled the
        // emulator's present cadence to the settings edits; the standalone
        // sheet decoupled it, so between framebuffer changes (e.g. a cursor
        // blink) a CRT-param edit would otherwise wait for the next
        // NeedsPresent trigger and appear laggy.
        if (m_settingsSheet != nullptr)
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }

        // Drive the joystick-button tooltip dwell timer; it shows / hides
        // its popup once the open / close delay elapses after a hover.
        {
            int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                 std::chrono::steady_clock::now().time_since_epoch()).count();

            m_joystickTooltip.Tick (nowMs);
        }

        // Refresh the printer status LED; marks a redraw itself on a change so
        // a static screen (e.g. a pending page at the BASIC prompt) repaints.
        UpdatePrinterIndicator ();

        if (!m_d3dRenderer.NeedsPresent (fbDirtyThisFrame))
        {
            Sleep (1);
            continue;
        }

        // Drive the host paint pump for this frame. Stage the emulator
        // framebuffer for the before-present hook, then request a
        // synchronous WM_PAINT: the host clears, the hook composites the
        // framebuffer, the chrome paints on top, and the host presents.
        m_pendingFramebuffer = fbDirtyThisFrame ? m_uiFramebuffer.data() : nullptr;
        InvalidateRect (m_hwnd, nullptr, FALSE);
        UpdateWindow   (m_hwnd);
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
            // mounts but Disk2Controller::PowerCycle unbinds the
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

                if (m_refs.keyboard != nullptr)
                {
                    m_refs.keyboard->Tick (m_cpu->GetLastInstructionCycles());
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
            // Payload is "shugart" or "alps" (canonical lower-case from
            // SettingsPanelState). DriveAudioMixer matches case-insensitively
            // and canonicalizes internally, so hand the token over as-is.
            std::wstring  mechWide (cmd.payload.begin(), cmd.payload.end());
            HRESULT       hrMech = m_driveAudioMixer.SetMechanism (mechWide);

            IGNORE_RETURN_VALUE (hrMech, S_OK);
            break;
        }

        case IDM_AUDIO_DRIVE_VOLUMES:
        {
            // Payload is "motor,head,door" as integer percents (0..100).
            int  motorPct = 0;
            int  headPct  = 0;
            int  doorPct  = 0;

            if (sscanf_s (cmd.payload.c_str(), "%d,%d,%d", &motorPct, &headPct, &doorPct) == 3)
            {
                SetDriveAudioVolumes ((float) motorPct / 100.0f,
                                      (float) headPct  / 100.0f,
                                      (float) doorPct  / 100.0f);
            }
            break;
        }

        case IDM_AUDIO_DRIVE_PAN:
        {
            // Payload is "pan0,pan1" as integer percents (-100..100),
            // -100 = hard left, +100 = hard right.
            int  pan0 = 0;
            int  pan1 = 0;

            if (sscanf_s (cmd.payload.c_str(), "%d,%d", &pan0, &pan1) == 2)
            {
                SetDriveAudioPan (0, (float) pan0 / 100.0f);
                SetDriveAudioPan (1, (float) pan1 / 100.0f);
            }
            break;
        }

        case IDM_AUDIO_DRIVE_TEST:
        {
            // Payload is "drive,kind" (drive 0/1; kind 0=motor 1=head
            // 2=door), auditioning a single sound at current settings.
            int  drive = 0;
            int  kind  = 0;

            if (sscanf_s (cmd.payload.c_str(), "%d,%d", &drive, &kind) == 2)
            {
                PlayDriveTestSound (drive, kind);
            }
            break;
        }

        default:
            break;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveAudioVolumes
//
//  Stores the live drive-audio gains and pushes them to every registered
//  Disk2AudioSource. Runs on the CPU thread (the mixing thread), so it is
//  safe to mutate the sources' gains here without synchronization.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveAudioVolumes (float motor, float head, float door)
{
    m_driveMotorVolume = motor;
    m_driveHeadVolume  = head;
    m_driveDoorVolume  = door;

    for (auto & src : m_diskAudioSources)
    {
        src->SetVolumes (motor, head, door);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveAudioPan
//
//  Stores a live per-drive stereo pan and applies it to the matching
//  Disk2AudioSource via equal-power panning. Runs on the CPU thread (the
//  mixing thread), so mutating the source's pan here needs no locking.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveAudioPan (int drive, float pan)
{
    HRESULT  hr   = S_OK;
    float    panL = DriveAudioMixer::kSpeakerCenter;
    float    panR = DriveAudioMixer::kSpeakerCenter;



    BAIL_OUT_IF (drive < 0 || drive >= (int) std::size (m_drivePan), S_OK);

    m_drivePan[drive] = std::clamp (pan, -1.0f, 1.0f);

    DriveAudioMixer::PanToStereo (m_drivePan[drive], panL, panR);

    if (drive < (int) m_diskAudioSources.size())
    {
        m_diskAudioSources[(size_t) drive]->SetPan (panL, panR);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlayDriveTestSound
//
//  Auditions a single drive sound on demand. Runs on the CPU thread (the
//  mixing thread), so triggering the source's test channel is lock-free.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PlayDriveTestSound (int drive, int kind)
{
    HRESULT                          hr       = S_OK;
    Disk2AudioSource::TestSoundKind  testKind = Disk2AudioSource::TestSoundKind::Motor;
    bool                             valid    = true;



    BAIL_OUT_IF (drive < 0 || drive >= (int) m_diskAudioSources.size(), S_OK);

    switch (kind)
    {
        case 0:  testKind = Disk2AudioSource::TestSoundKind::Motor; break;
        case 1:  testKind = Disk2AudioSource::TestSoundKind::Head;  break;
        case 2:  testKind = Disk2AudioSource::TestSoundKind::Door;  break;
        default: valid    = false;                                  break;
    }

    BAIL_OUT_IF (!valid, S_OK);

    m_diskAudioSources[(size_t) drive]->PlayTestSound (testKind);

Error:
    return;
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

void EmulatorShell::StepInstructionWhilePaused()
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

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->Tick (m_cpu->GetLastInstructionCycles());
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

            if (m_refs.keyboard != nullptr)
            {
                m_refs.keyboard->Tick (cycles);
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



    // A color monitor renders text white; the monochrome monitors keep the
    // text renderer's green here and the post-render tint below recolors the
    // whole frame to the selected phosphor. m_videoModes[0] is the 40-col
    // text mode and [4] (when present) the 80-col mode.
    {
        uint32_t textOnColor = (color == ColorMode::Color)
                                   ? m_colorMonitorTextArgb.load (memory_order_acquire)
                                   : s_kMonoSourceTextBgra;

        if (!m_videoModes.empty())
        {
            static_cast<AppleTextMode *> (m_videoModes[0].get())->SetOnColor (textOnColor);
        }

        if (m_videoModes.size() > 4)
        {
            static_cast<Apple80ColTextMode *> (m_videoModes[4].get())->SetOnColor (textOnColor);
        }
    }

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

        auto * iieSwitches = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
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

////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand  (IDxuiHostClient)
//
//  Forwards the command id to the existing WindowCommandManager.
//  WindowCommandManager::OnCommand returns the legacy Window-base
//  polarity (`true` = call DefWindowProc, `false` = consumed); we
//  translate to the typed DxuiMessageResult at the return site.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnCommand (WORD commandId)
{
    bool  callDefWndProc = m_windowCommandManager->OnCommand (m_hwnd, (int) commandId);

    return callDefWndProc ? DxuiMessageResult::NotHandled : DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnDestroy ()
{
    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());

    // P6 -- revoke the IDropTarget before the HWND is destroyed.
    // RevokeDragDrop requires a valid window handle.
    m_dragDropTarget.Shutdown();

    // Join the printer drain thread before teardown frees the card.
    m_printerWorker.Stop ();

    // Persist the pending strip on clean exit (FR-026); empty clears any stale
    // sidecar. Loss on abnormal termination is acceptable per the spec.
    if (!m_currentMachineName.empty ())
    {
        PrinterJob *   printJob = m_printerWorker.Job ();

        if (printJob != nullptr && printJob->HasContent ())
        {
            HRESULT   hrSave = PrintJobStore::Save (PendingPrintDir (), printJob->Raster ());
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }
        else
        {
            PrintJobStore::Clear (PendingPrintDir ());
        }
    }

    m_cpuManager.Stop();

    // IDxuiHostClient::OnDestroy is notification-only — the host
    // does NOT call PostQuitMessage. EmulatorShell is the
    // application's main window, so it owns that call.
    PostQuitMessage (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    int      x        = ((int) (short) LOWORD (lParam));
    int      y        = ((int) (short) HIWORD (lParam));
    bool     leftDown = (wParam & MK_LBUTTON) != 0;
    bool     overBtn  = false;
    int64_t  nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                            std::chrono::steady_clock::now().time_since_epoch()).count();



    // Paddle mode owns the pointer while captured: relative motion drives
    // the held paddle axes and the cursor is snapped back to center, so the
    // chrome never sees the move.
    if (m_paddleCaptured)
    {
        UpdatePaddleFromMouse (x, y);
        return DxuiMessageResult::Handled;
    }

    // A fresh hover over a drive widget replays its basename marquee, so
    // the full filename can be re-read on demand.
    for (DriveWidget & drive : m_driveChrome)
    {
        RECT  outer  = drive.OuterRect();
        bool  inside = x >= outer.left && x < outer.right &&
                       y >= outer.top  && y < outer.bottom;

        drive.UpdateMarqueeHover (inside, nowMs);
    }

    if (m_uiShell.OnMouseMove (x, y, leftDown))
    {
        return DxuiMessageResult::Handled;
    }

    overBtn = m_joystickButton.HitTest (x, y);
    m_joystickButton.SetHovered (overBtn);

    if (overBtn)
    {
        m_joystickTooltip.RequestShow (m_joystickButton.Bounds(), m_joystickButton.TooltipText(), nowMs);
    }
    else
    {
        m_joystickTooltip.RequestHide (nowMs);
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseLeave
//
//  Routes through UiShell so chrome painters (title-bar caption
//  buttons, nav strip) drop their hot-button / hover state when the
//  cursor exits the window via the non-client area.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseLeave ()
{
    int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                         std::chrono::steady_clock::now().time_since_epoch()).count();

    m_uiShell.OnMouseLeave();

    // Drop drive marquee-hover state so re-entering the window re-triggers
    // the basename scroll.
    for (DriveWidget & drive : m_driveChrome)
    {
        drive.UpdateMarqueeHover (false, nowMs);
    }

    m_joystickButton.SetHovered (false);
    m_joystickButton.SetPressed (false);
    m_joystickTooltip.RequestHide (nowMs);
    return DxuiMessageResult::NotHandled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    int   x        = ((int) (short) LOWORD (lParam));
    int   y        = ((int) (short) HIWORD (lParam));
    bool  consumed = false;



    UNREFERENCED_PARAMETER (wParam);

    // While paddle-captured, the left button is fire button 0 and the
    // pointer is hidden/confined, so nothing else acts on the press.
    if (m_paddleCaptured)
    {
        PushPaddleButton (0, true);
        return DxuiMessageResult::Handled;
    }

    SetCapture (m_hwnd);

    // A mouse press drops the keyboard chrome-focus ring: clicking anywhere
    // hands focus back to the pointer, so the painted focus visual should
    // not linger. A click that opens a menu is then tracked via IsOpen().
    if (m_chromeFocusIndex != s_kChromeFocusNone)
    {
        SetChromeFocusIndex (s_kChromeFocusNone);
    }

    // A press outside the menu strip dismisses any open menu. The strip
    // itself toggles / hover-switches via the menu bar's own mouse
    // handling, and the popup-backed dropdown receives row clicks
    // directly; the popup takes no capture, so the owner drives this.
    if (m_mainMenu.IsOpen())
    {
        RECT  strip = m_mainMenu.Bounds();

        if (x < strip.left || x >= strip.right || y < strip.top || y >= strip.bottom)
        {
            m_mainMenu.Hide();
        }
    }

    m_joystickButton.SetPressed (m_joystickButton.HitTest (x, y));

    // The UI shell (debug panels, on-screen buttons) gets first crack at
    // the press. Its return is moot here: nothing else in this handler
    // depends on whether the click was consumed, and we always report the
    // message as not fully handled.
    consumed = m_uiShell.OnLButtonDown (x, y);
    IGNORE_RETURN_VALUE (consumed, false);

    return DxuiMessageResult::NotHandled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    int                x      = ((int) (short) LOWORD (lParam));
    int                y      = ((int) (short) HIWORD (lParam));
    DriveWidgetRegion  region = DriveWidgetRegion::None;



    UNREFERENCED_PARAMETER (wParam);

    // While paddle-captured, the left button is fire button 0; release it
    // and keep the capture (the transient click-capture path is bypassed).
    if (m_paddleCaptured)
    {
        PushPaddleButton (0, false);
        return DxuiMessageResult::Handled;
    }

    ReleaseCapture();
    m_joystickButton.SetPressed (false);
    if (m_uiShell.OnLButtonUp (x, y))
    {
        return DxuiMessageResult::NotHandled;
    }

    // Cycling the input-mode button routes through the same path as the
    // Machine menu command so the leave-time neutralization of held
    // arrow / X / Z inputs runs.
    if (m_joystickButton.HitTest (x, y))
    {
        CycleInputMappingMode();
        return DxuiMessageResult::NotHandled;
    }

    // If we just finished an OLE drop on a drive widget, the OS posts
    // a WM_LBUTTONUP that lands here on top of the drive. Swallow it
    // so the user doesn't see the file-open dialog pop up immediately
    // after the dropped image mounts.
    if (m_dragDropTarget.ConsumeSuppressedClick())
    {
        return DxuiMessageResult::NotHandled;
    }

    for (DriveWidget & drive : m_driveChrome)
    {
        region = drive.HitTest (x, y);
        if (region == DriveWidgetRegion::Body)
        {
            BrowseForDisk (drive.Drive());
            return DxuiMessageResult::NotHandled;
        }

        if (region == DriveWidgetRegion::Eject)
        {
            Eject (6, drive.Drive());
            BrowseForDisk (drive.Drive());
            return DxuiMessageResult::NotHandled;
        }
    }

    // A click on the printer status indicator opens the printer panel /
    // print preview (US4 / FR-020).
    if (!m_printerIndicator.Hidden ())
    {
        RECT  pr = m_printerIndicator.OuterRect ();

        if (x >= pr.left && x < pr.right && y >= pr.top && y < pr.bottom)
        {
            ShowPrinterPanel ();
            return DxuiMessageResult::NotHandled;
        }
    }

    // A bare left-click on the emulator screen (no chrome / widget / drive
    // hit) in Paddle mode re-grabs the pointer after an Esc release.
    if (m_inputMode == InputMappingMode::Paddle && !m_paddleCaptured)
    {
        StartPaddleCapture();
        return DxuiMessageResult::Handled;
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonDown / OnRButtonUp
//
//  In Paddle mode the right mouse button is fire button 1; otherwise the
//  message falls through to the default handler.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnRButtonDown (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;


    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    if (m_paddleCaptured)
    {
        PushPaddleButton (1, true);
        result = DxuiMessageResult::Handled;
    }

    return result;
}





DxuiMessageResult EmulatorShell::OnRButtonUp (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;


    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    if (m_paddleCaptured)
    {
        PushPaddleButton (1, false);
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnActivateApp / OnKillFocus / OnCancelMode
//
//  Safety net that releases a live paddle-mode mouse capture whenever the
//  app loses the foreground (Alt-Tab, taskbar, minimize), focus, or the OS
//  cancels capture (Ctrl-Alt-Del / UAC secure desktop, workstation lock,
//  modal takeover). The OS force-releases capture and the cursor clip in
//  the secure-desktop cases too; this keeps our hidden-cursor / captured
//  state in sync so the pointer reappears. Re-grab is an explicit click on
//  the emulator screen.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnActivateApp (bool active)
{
    if (!active)
    {
        StopPaddleCapture();
    }

    return DxuiMessageResult::NotHandled;
}





DxuiMessageResult EmulatorShell::OnKillFocus ()
{
    StopPaddleCapture();
    return DxuiMessageResult::NotHandled;
}





DxuiMessageResult EmulatorShell::OnCancelMode ()
{
    StopPaddleCapture();
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnGetMinMax
//
//  Clamps the window's minimum track size so the bottom drive bar can
//  never be dragged up into the menu strip / NC area. The floor is the
//  client size for a minimum emulator viewport (the chrome-band dock
//  adds the live title / nav / drive-bar insets), widened so no menu
//  title clips, then translated to a window size by the live NC overhead.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnGetMinMax (MINMAXINFO * info)
{
    HRESULT            hr          = S_OK;
    DxuiMessageResult  result      = DxuiMessageResult::NotHandled;
    RECT               rcClient    = {};
    RECT               rcWindow    = {};
    SIZE               minClient   = {};
    int                menuWidthPx = 0;
    int                ncOverheadW = 0;
    int                ncOverheadH = 0;



    BAIL_OUT_IF (info == nullptr || m_hwnd == nullptr, S_OK);

    // Client size for the minimum center: the chrome-band dock adds the
    // live title / nav / drive-bar insets around the requested viewport.
    minClient = ClientSizeForCenterPx (m_scaler.Px (s_kMinCenterWidthDp),
                                       m_scaler.Px (s_kMinCenterHeightDp));

    // Never narrower than the menu strip's content so every title stays
    // on-strip. The width is physical client px, the same space as minClient.
    menuWidthPx = m_mainMenu.MenuStripContentWidthPx() + m_scaler.Px (s_kMenuRightPadDp);

    if (minClient.cx < menuWidthPx)
    {
        minClient.cx = menuWidthPx;
    }

    // Translate the client floor to a window floor via the live NC overhead
    // (the custom chrome keeps this small -- just the resize borders -- but
    // it is non-zero).
    if (GetClientRect (m_hwnd, &rcClient) && GetWindowRect (m_hwnd, &rcWindow))
    {
        ncOverheadW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
        ncOverheadH = (rcWindow.bottom - rcWindow.top)  - (rcClient.bottom - rcClient.top);
    }

    info->ptMinTrackSize.x = minClient.cx + ncOverheadW;
    info->ptMinTrackSize.y = minClient.cy + ncOverheadH;

    result = DxuiMessageResult::Handled;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenSettings
//
//  Opens the Settings dialog (View > Settings / Ctrl+,). The bespoke
//  SettingsPanel + SettingsWindow were retired in T162 slice 3d; this shows
//  the DxuiPropertySheet-based SettingsSheet MODELESS (FR-041) so the emulator
//  keeps running behind it. The sheet is heap-owned; its close callback flags
//  a deferred destroy handled by RunMessageLoop. A second invocation while it
//  is already open just re-focuses the existing sheet.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenSettings ()
{
    HINSTANCE  hInst = (HINSTANCE) GetWindowLongPtrW (m_hwnd, GWLP_HINSTANCE);


    if (m_settingsSheet != nullptr)
    {
        HWND  existing = m_settingsSheet->Hwnd();
        if (existing != nullptr)
        {
            SetForegroundWindow (existing);
        }
        return;
    }

    m_settingsSheet = std::make_unique<SettingsSheet>();
    m_settingsSheet->SetOnDialogEnd ([this] (int) { m_settingsSheetClosePending = true; });

    (void) m_settingsSheet->OpenModeless (hInst, m_hwnd,
                                          *m_userConfigStore, m_globalPrefs, *m_themeManager,
                                          *this, m_uiFs);
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleHostMetaShortcut
//
//  Consume host-meta keys that never reach the emulated //e keyboard: menu
//  mnemonic navigation, F10 menu focus, Ctrl+V paste, and Ctrl+Shift+R reset.
//  Returns true when the key was claimed. An unmatched Alt+key deliberately
//  falls through so combos like Ctrl+Alt+R still reach the reset path.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::HandleHostMetaShortcut (WPARAM vk, bool ctrlHeld, bool altHeld)
{
    if (altHeld && vk >= 0x20 && vk <= 0x7E && m_mainMenu.HandleAltKey ((wchar_t) vk))
    {
        return true;
    }

    if (vk == VK_F10 && !ctrlHeld && !altHeld)
    {
        // F10 enters the chrome keyboard-focus ring at the first menu title
        // (dropdown closed). Exiting the ring is handled inside
        // HandleChromeFocusKey, which intercepts F10 once the ring is active.
        SetChromeFocusIndex (s_kChromeFocusMenuFirst);
        return true;
    }

    if (vk == 'V' && ctrlHeld && !altHeld)
    {
        m_clipboardManager->PasteFromClipboard (m_hwnd);
        return true;
    }

    if (vk == 'R' && ctrlHeld && !(GetKeyState (VK_SHIFT) & 0x8000))
    {
        PostCommand (IDM_MACHINE_RESET);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAppleModifierKeys
//
//  Mirror the host modifier state onto the //e soft switches: left Alt ->
//  Open Apple ($C061), right Alt -> Closed Apple ($C062), Shift -> Shift
//  ($C063). GetKeyState gives the canonical left/right state, so a modifier
//  stays asserted while either physical key is still down. A no-op on the
//  ][/][+ where the keyboard is not an Apple //e keyboard.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyAppleModifierKeys (WPARAM vk, bool keyDown)
{
    HRESULT   hr     = S_OK;
    auto    * iieKbd = dynamic_cast<Apple2eKeyboard *> (m_refs.keyboard);
    bool      lAlt   = false;
    bool      rAlt   = false;



    CBR (iieKbd != nullptr);

    if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU)
    {
        lAlt = (GetKeyState (VK_LMENU) & 0x8000) != 0;
        rAlt = (GetKeyState (VK_RMENU) & 0x8000) != 0;
        iieKbd->SetOpenApple   (lAlt);
        iieKbd->SetClosedApple (rAlt);
    }
    else if (vk == VK_SHIFT)
    {
        iieKbd->SetShift (keyDown);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapVkToAppleControlCode
//
//  Translate a host arrow/Escape/Delete virtual key into its //e control
//  code. Returns 0 for keys that have no direct //e control-code mapping.
//
////////////////////////////////////////////////////////////////////////////////

Byte EmulatorShell::MapVkToAppleControlCode (WPARAM vk)
{
    Byte  appleCode = 0;



    switch (vk)
    {
        case VK_LEFT:
            appleCode = kAppleKeyLeft;
            break;

        case VK_RIGHT:
            appleCode = kAppleKeyRight;
            break;

        case VK_UP:
            appleCode = kAppleKeyUp;
            break;

        case VK_DOWN:
            appleCode = kAppleKeyDown;
            break;

        case VK_ESCAPE:
            appleCode = kAppleKeyEscape;
            break;

        case VK_DELETE:
            appleCode = kAppleKeyDelete;
            break;

        default:
            break;
    }

    return appleCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsArrowVk
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsArrowVk (WPARAM vk)
{
    return vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    HRESULT  hr            = S_OK;
    bool     consumed      = false;
    bool     ctrlHeld      = false;
    bool     altHeld       = false;
    bool     isRepeat      = (lParam & s_kPreviousKeyDownLParamBit) != 0;



    // 0. Esc exits paddle mode: releases the mouse capture (cursor
    //    reappears) and returns the input mapping to Off, matching the
    //    "Esc to exit" hint on the widget.
    if (m_inputMode == InputMappingMode::Paddle && vk == VK_ESCAPE)
    {
        SetInputMappingMode (InputMappingMode::Off);
        BAIL_OUT_IF (true, S_OK);
    }

    // Chrome keyboard-focus ring. While a menu title / button / drive
    //    has keyboard focus (or a dropdown is open from any source), the
    //    ring owns every keydown so letters never leak through to the //e.
    if (m_chromeFocusIndex != s_kChromeFocusNone || m_mainMenu.IsOpen())
    {
        HandleChromeFocusKey (vk);
        BAIL_OUT_IF (true, S_OK);
    }

    CBR (m_refs.keyboard != nullptr);

    ctrlHeld = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    altHeld  = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    consumed = HandleHostMetaShortcut (vk, ctrlHeld, altHeld);
    BAIL_OUT_IF (consumed, S_OK);

    // The chrome / settings / meta pre-checks above already skimmed off
    // every keystroke that belongs to the shell. Everything left is the
    // guest's: build a Down event and hand it to the viewport, which (with
    // SetConsumesInput + SetWantsAllKeys) forwards it to OnViewportKey for
    // the //e keyboard + game port. Routing through the viewport keeps a
    // single Dxui input path (FR-034) rather than the shell reaching into
    // m_refs.keyboard directly.
    if (m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind   = DxuiKeyEventKind::Down;
        ev.vk     = vk;
        ev.repeat = isRepeat;
        ev.ctrl   = ctrlHeld;
        ev.alt    = altHeld;
        ev.shift  = (GetKeyState (VK_SHIFT) & 0x8000) != 0;

        (void) m_viewport->OnKey (ev);
    }

Error:
    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    // Key-up is deliberately unconditional (no chrome / settings gate): a
    // release must always reach the //e so a modifier or repeat can never
    // stick when focus moved to the chrome mid-press. The viewport forwards
    // it to OnViewportKey, which performs the release.
    if (m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Up;
        ev.vk   = vk;

        (void) m_viewport->OnKey (ev);
    }

    return DxuiMessageResult::Handled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportKey
//
//  IDxuiViewportInputSink. Applies a viewport-forwarded keystroke to the
//  Apple ][ keyboard latch and game port. The chrome / settings / meta
//  pre-checks already ran in OnKeyDown / OnChar, and the viewport is set
//  to SetWantsAllKeys(true), so every remaining keystroke -- Esc / Tab /
//  arrows included -- lands here and belongs to the guest. Always returns
//  true: nothing here bubbles back to the framework (the shell's own
//  chrome escape routes live in the pre-checks, not the sink).
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportKey (const DxuiKeyEvent & ev)
{
    // Arrow keys double as the emulated joystick axes / the X / Z keys as
    // fire buttons when "Map Arrows to Joystick" is on AND a game-port
    // paddle bank is present. Recomputed per event so a mode change between
    // press and release is always honored.
    bool  driveJoystick = m_inputMode == InputMappingMode::Joystick &&
                          (dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches) != nullptr ||
                           m_refs.gamePort != nullptr);

    if (m_refs.keyboard == nullptr)
    {
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        WPARAM  vk        = ev.vk;
        Byte    appleCode = 0;

        m_refs.keyboard->SetKeyDown (true);
        ApplyAppleModifierKeys (vk, true);

        // Arrow / Escape / Delete map to //e control codes. Gated on the
        // auto-repeat bit so the host OS repeat never reaches the latch; a
        // fresh press arms the $C000 strobe once and registers the key for
        // the emulator's own authentic //e auto-repeat cadence (Tick). With
        // "Map Arrows to Joystick" on (and a game-port paddle bank present),
        // arrow keys are withheld from the keyboard latch so a held
        // direction cannot flood $C000 and starve a joystick game's reads.
        if (!ev.repeat)
        {
            appleCode = MapVkToAppleControlCode (vk);

            if (driveJoystick && IsArrowVk (vk))
            {
                appleCode = 0;
            }

            if (appleCode != 0)
            {
                m_refs.keyboard->KeyPress (appleCode);
                m_refs.keyboard->BeginKeyRepeat (appleCode);
            }
        }

        // Record the last-pressed direction per axis so opposing keys
        // resolve last-pressed-wins, then re-resolve both axes from the
        // current key state.
        if (driveJoystick && IsArrowVk (vk))
        {
            if (vk == VK_LEFT || vk == VK_RIGHT)
            {
                m_lastHorizontalArrowVk = vk;
            }
            else
            {
                m_lastVerticalArrowVk = vk;
            }

            UpdateJoystickAxesFromKeys();
        }

        // Re-resolve the joystick fire buttons on every key event in
        // joystick mode (not just on X / Z) so that an Alt press/release
        // re-applies its Open/Closed-Apple mapping without clobbering a
        // still-held X / Z, and a released X / Z can't leave a button stuck
        // while Alt is down. The matching X / Z WM_CHAR is suppressed in
        // OnChar so the letters don't also type into the //e keyboard latch.
        if (driveJoystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
    else if (ev.kind == DxuiKeyEventKind::Up)
    {
        WPARAM  vk = ev.vk;

        m_refs.keyboard->SetKeyDown (false);

        // Disarm auto-repeat on release. The //e latch holds a single key,
        // so a key-up always ends the current repeat; this also clears any
        // stale armed key so a later non-character press (e.g. a bare
        // modifier) can never resurrect the previous character's repeat.
        m_refs.keyboard->BeginKeyRepeat (0);

        // Release the //e Open/Closed-Apple and Shift modifiers as the host
        // releases the physical keys.
        ApplyAppleModifierKeys (vk, false);

        if (m_inputMode == InputMappingMode::Joystick && IsArrowVk (vk))
        {
            UpdateJoystickAxesFromKeys();
        }

        if (m_inputMode == InputMappingMode::Joystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
    else // DxuiKeyEventKind::Char
    {
        WPARAM  ch = ev.vk;

        if (ch >= 1 && ch <= 127)
        {
            m_refs.keyboard->KeyPress (static_cast<Byte> (ch));
            m_refs.keyboard->BeginKeyRepeat (static_cast<Byte> (ch));
        }
    }

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportMouse
//
//  IDxuiViewportInputSink. The Apple ][ has no viewport-rect mouse mapping
//  in the current build: paddle input is a captured relative-motion mode
//  driven directly from OnMouseMove (SetCapture snaps the cursor to
//  centre), and the joystick maps to arrow keys -- neither fits the
//  viewport's absolute-rect forwarding. Returns false so any future
//  in-viewport click continues to bubble to the chrome hit-testing that
//  owns it today.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportMouse (const DxuiMouseEvent & ev)
{
    UNREFERENCED_PARAMETER (ev);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateJoystickAxesFromKeys
//
//  Host UI thread. Resolves the four arrow keys into the two emulated
//  joystick axes and stages them on the game port: the //e soft-switch
//  bank (Apple2eSoftSwitchBank) or the ][/][+ AppleGamePort, whichever is
//  present. The PREAD timer ($C070 / $C064-$C067) turns them into analog
//  readings. No-op only when neither device is present.
//
//  Reads real-time physical key state via GetAsyncKeyState rather than the
//  per-thread GetKeyState table, which can desync (and leave an axis stuck)
//  if a key-up is lost to a focus change. Opposing keys resolve
//  last-pressed-wins so a rolling reversal flips the axis instead of
//  cancelling to center.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateJoystickAxesFromKeys()
{
    HRESULT  hr       = S_OK;
    auto   * iieSw    = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
    auto   * gamePort = m_refs.gamePort;
    bool     left     = false;
    bool     right    = false;
    bool     up       = false;
    bool     down     = false;
    Byte     x        = Apple2eSoftSwitchBank::s_knPaddleCenter;
    Byte     y        = Apple2eSoftSwitchBank::s_knPaddleCenter;



    BAIL_OUT_IF (iieSw == nullptr && gamePort == nullptr, S_OK);

    left  = (GetAsyncKeyState (VK_LEFT)  & 0x8000) != 0;
    right = (GetAsyncKeyState (VK_RIGHT) & 0x8000) != 0;
    up    = (GetAsyncKeyState (VK_UP)    & 0x8000) != 0;
    down  = (GetAsyncKeyState (VK_DOWN)  & 0x8000) != 0;

    if (left && right)
    {
        x = (m_lastHorizontalArrowVk == VK_RIGHT) ? s_kPaddleAxisMax : s_kPaddleAxisMin;
    }
    else if (left)
    {
        x = s_kPaddleAxisMin;
    }
    else if (right)
    {
        x = s_kPaddleAxisMax;
    }

    if (up && down)
    {
        y = (m_lastVerticalArrowVk == VK_DOWN) ? s_kPaddleAxisMax : s_kPaddleAxisMin;
    }
    else if (up)
    {
        y = s_kPaddleAxisMin;
    }
    else if (down)
    {
        y = s_kPaddleAxisMax;
    }

    if (iieSw != nullptr)
    {
        iieSw->SetPaddle (0, x);
        iieSw->SetPaddle (1, y);
    }

    if (gamePort != nullptr)
    {
        gamePort->SetPaddle (0, x);
        gamePort->SetPaddle (1, y);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateJoystickButtonsFromKeys
//
//  Host UI thread. Resolves the X / Z letter keys into the two emulated
//  joystick fire buttons. On the //e they stage on the keyboard's
//  Open/Closed-Apple state (button 0 reads at $C061, button 1 at $C062);
//  on the ][/][+ they stage on the AppleGamePort pushbuttons (PB0/$C061,
//  PB1/$C062). No-op only when neither device is present.
//
//  Reads real-time physical key state via GetAsyncKeyState, matching the
//  axis helper so a key-up lost to a focus change can't wedge a button on.
//  The fire state is OR'd with the host left/right Alt keys so the X / Z
//  mapping coexists with the existing Alt->button mapping instead of
//  clobbering a held Alt.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateJoystickButtonsFromKeys()
{
    HRESULT  hr       = S_OK;
    auto   * iieKbd   = dynamic_cast<Apple2eKeyboard *> (m_refs.keyboard);
    auto   * gamePort = m_refs.gamePort;
    bool     button0  = false;
    bool     button1  = false;



    BAIL_OUT_IF (iieKbd == nullptr && gamePort == nullptr, S_OK);

    button0 = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton0Vk)) & 0x8000) != 0 ||
              (GetKeyState      (VK_LMENU)                                & 0x8000) != 0;
    button1 = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton1Vk)) & 0x8000) != 0 ||
              (GetKeyState      (VK_RMENU)                                & 0x8000) != 0;

    if (iieKbd != nullptr)
    {
        iieKbd->SetOpenApple   (button0);
        iieKbd->SetClosedApple (button1);
    }

    if (gamePort != nullptr)
    {
        gamePort->SetButton (0, button0);
        gamePort->SetButton (1, button1);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputMappingMode
//
//  Sets the host input mapping mode (Off / Joystick / Paddle) and persists
//  it. Leaving Paddle drops the mouse capture. Joystick resolves the axes
//  and fire buttons from the current key state so a held arrow / X / Z
//  takes effect immediately. Off and Paddle both neutralize the
//  key-derived stick and buttons so the game port reads neutral; Paddle
//  then centers the paddle and grabs the mouse.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetInputMappingMode (InputMappingMode mode)
{
    auto             * iieSw    = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
    auto             * iieKbd   = dynamic_cast<Apple2eKeyboard *>       (m_refs.keyboard);
    auto             * gamePort = m_refs.gamePort;
    InputMappingMode   prev     = m_inputMode;



    if (prev == InputMappingMode::Paddle && mode != InputMappingMode::Paddle)
    {
        StopPaddleCapture();
    }

    m_inputMode                    = mode;
    m_globalPrefs.inputMappingMode = mode;
    m_joystickButton.SetMode (mode);

    // The frame width tracks the current label (Paddle is wider), so
    // resize the button now rather than waiting for the next layout pass.
    RelayoutJoystickButton();

    SaveGlobalPrefs();

    if (mode == InputMappingMode::Joystick)
    {
        UpdateJoystickAxesFromKeys();
        UpdateJoystickButtonsFromKeys();
        return;
    }

    // Off / Paddle: center the axes and drop the X / Z fire mapping (leaving
    // only the host Alt keys) so a held key can't stay stuck.
    if (iieSw != nullptr)
    {
        iieSw->SetPaddle (0, Apple2eSoftSwitchBank::s_knPaddleCenter);
        iieSw->SetPaddle (1, Apple2eSoftSwitchBank::s_knPaddleCenter);
    }

    if (gamePort != nullptr)
    {
        gamePort->SetPaddle (0, AppleGamePort::s_knPaddleCenter);
        gamePort->SetPaddle (1, AppleGamePort::s_knPaddleCenter);
        gamePort->SetButton (0, false);
        gamePort->SetButton (1, false);
    }

    if (iieKbd != nullptr)
    {
        iieKbd->SetOpenApple   ((GetKeyState (VK_LMENU) & 0x8000) != 0);
        iieKbd->SetClosedApple ((GetKeyState (VK_RMENU) & 0x8000) != 0);
    }

    if (mode == InputMappingMode::Paddle)
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_paddleAxisX = (float) s_kPaddleCenterByte;
        m_paddleAxisY = (float) s_kPaddleCenterByte;

        // Entering paddle mode captures the mouse, so the hover that would
        // normally dismiss the tooltip never fires. Show the paddle notice
        // and let it auto-dismiss after a few seconds.
        m_joystickTooltip.ShowTimed (m_joystickButton.Bounds(),
                                     m_joystickButton.TooltipText(),
                                     nowMs,
                                     s_kPaddleNoticeMs);

        StartPaddleCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleInputMappingMode
//
//  Advances the input mapping mode Off -> Joystick -> Paddle -> Off.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CycleInputMappingMode ()
{
    InputMappingMode  next = InputMappingMode::Off;



    switch (m_inputMode)
    {
        case InputMappingMode::Off:
            next = InputMappingMode::Joystick;
            break;

        case InputMappingMode::Joystick:
            next = InputMappingMode::Paddle;
            break;

        case InputMappingMode::Paddle:
        default:
            next = InputMappingMode::Off;
            break;
    }

    SetInputMappingMode (next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleInputMappingMode
//
//  Radio-group selection for the Machine-menu Joystick / Paddle items:
//  picks `target`, or turns mapping Off when `target` is already active so
//  re-selecting the current mode clears it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ToggleInputMappingMode (InputMappingMode target)
{
    SetInputMappingMode (m_inputMode == target ? InputMappingMode::Off : target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartPaddleCapture
//
//  Hides and confines the cursor to the client area, parks it at center,
//  and begins relative tracking. No-op unless the mode is Paddle, the
//  window owns the foreground, and capture isn't already active. The
//  current (held) paddle position is pushed so a re-grab after an Esc
//  release resumes from where the dial was left.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StartPaddleCapture ()
{
    HRESULT  hr      = S_OK;
    RECT     client  = {};
    POINT    topLeft = {};
    POINT    botRt   = {};
    RECT     clip    = {};
    POINT    center  = {};



    BAIL_OUT_IF (m_inputMode != InputMappingMode::Paddle, S_OK);
    BAIL_OUT_IF (m_paddleCaptured,                        S_OK);
    BAIL_OUT_IF (m_hwnd == nullptr,                       S_OK);
    BAIL_OUT_IF (GetForegroundWindow() != m_hwnd,         S_OK);

    m_paddleCaptured = true;

    SetCapture (m_hwnd);

    // Drive the per-thread ShowCursor counter negative so the arrow hides.
    while (ShowCursor (FALSE) >= 0)
    {
    }

    GetClientRect (m_hwnd, &client);

    topLeft.x = client.left;
    topLeft.y = client.top;
    botRt.x   = client.right;
    botRt.y   = client.bottom;
    ClientToScreen (m_hwnd, &topLeft);
    ClientToScreen (m_hwnd, &botRt);

    clip.left   = topLeft.x;
    clip.top    = topLeft.y;
    clip.right  = botRt.x;
    clip.bottom = botRt.y;
    ClipCursor (&clip);

    center.x = (client.right  - client.left) / 2;
    center.y = (client.bottom - client.top)  / 2;
    ClientToScreen (m_hwnd, &center);
    SetCursorPos (center.x, center.y);

    PushPaddlePosition();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopPaddleCapture
//
//  Releases the mouse capture and cursor clip, restores the cursor, and
//  clears the fire buttons so a held mouse button doesn't stick. No-op
//  when not captured. Leaves the input mode unchanged, so the dial holds
//  its position for a later re-grab.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StopPaddleCapture ()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    m_paddleCaptured = false;

    ClipCursor (nullptr);

    if (GetCapture() == m_hwnd)
    {
        ReleaseCapture();
    }

    while (ShowCursor (TRUE) < 0)
    {
    }

    PushPaddleButton (0, false);
    PushPaddleButton (1, false);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePaddleFromMouse
//
//  Maps one WM_MOUSEMOVE while paddle-captured: the motion relative to the
//  client center is scaled (s_kPaddleSweepInches of DPI-scaled travel =
//  full range) into the held paddle axes, then the cursor is snapped back
//  to center for unbounded relative motion. The zero-delta move our own
//  recenter generates is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePaddleFromMouse (int xClient, int yClient)
{
    HRESULT  hr         = S_OK;
    RECT     client     = {};
    int      centerX    = 0;
    int      centerY    = 0;
    int      dx         = 0;
    int      dy         = 0;
    UINT     dpi        = 96;
    float    unitsPerPx = 0.0f;
    POINT    center     = {};



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    GetClientRect (m_hwnd, &client);
    centerX = (client.right  - client.left) / 2;
    centerY = (client.bottom - client.top)  / 2;
    dx      = xClient - centerX;
    dy      = yClient - centerY;

    // The SetCursorPos recenter below re-enters here with a zero delta.
    BAIL_OUT_IF (dx == 0 && dy == 0, S_OK);

    dpi = GetDpiForWindow (m_hwnd);
    if (dpi == 0)
    {
        dpi = 96;
    }

    unitsPerPx    = s_kPaddleRange / (s_kPaddleSweepInches * (float) dpi);
    m_paddleAxisX = std::clamp (m_paddleAxisX + (float) dx * unitsPerPx, s_kPaddleMinF, s_kPaddleMaxF);
    m_paddleAxisY = std::clamp (m_paddleAxisY + (float) dy * unitsPerPx, s_kPaddleMinF, s_kPaddleMaxF);

    PushPaddlePosition();

    center.x = centerX;
    center.y = centerY;
    ClientToScreen (m_hwnd, &center);
    SetCursorPos (center.x, center.y);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushPaddlePosition
//
//  Stages the held paddle axes onto whichever game port is present (the
//  //e soft-switch bank and / or the ][/][+ AppleGamePort).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PushPaddlePosition ()
{
    auto * iieSw    = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
    auto * gamePort = m_refs.gamePort;
    Byte   x        = (Byte) (m_paddleAxisX + 0.5f);
    Byte   y        = (Byte) (m_paddleAxisY + 0.5f);



    if (iieSw != nullptr)
    {
        iieSw->SetPaddle (0, x);
        iieSw->SetPaddle (1, y);
    }

    if (gamePort != nullptr)
    {
        gamePort->SetPaddle (0, x);
        gamePort->SetPaddle (1, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushPaddleButton
//
//  Stages a paddle / joystick fire button (0 or 1) onto the game port:
//  the AppleGamePort pushbuttons on the ][/][+ and the //e keyboard's
//  Open/Closed-Apple state. No-op for indices without a mapping.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PushPaddleButton (int index, bool pressed)
{
    auto * iieKbd   = dynamic_cast<Apple2eKeyboard *> (m_refs.keyboard);
    auto * gamePort = m_refs.gamePort;



    if (gamePort != nullptr)
    {
        gamePort->SetButton (index, pressed);
    }

    if (iieKbd != nullptr)
    {
        if (index == 0)
        {
            iieKbd->SetOpenApple (pressed);
        }
        else if (index == 1)
        {
            iieKbd->SetClosedApple (pressed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnChar (WPARAM ch, LPARAM lParam)
{
    bool isRepeat = (lParam & s_kPreviousKeyDownLParamBit) != 0;

    if (m_refs.keyboard == nullptr)
    {
        return DxuiMessageResult::Handled;
    }

    // Suppress the WM_CHAR that Windows synthesizes from a WM_KEYDOWN
    // already consumed by overlay UI (settings panel / open menu) or by the
    // chrome keyboard-focus ring. Without this, a letter typed while a menu
    // title / button / drive is focused would also drop into the //e latch.
    if (m_uiShell.IsCapturingInput() || m_chromeFocusIndex != s_kChromeFocusNone)
    {
        return DxuiMessageResult::Handled;
    }

    // Drop Windows OS auto-repeat: the host repeat rate would flood
    // $C000 and confuse real-time games that poll it. A fresh press is
    // latched once and registered for the emulator's own authentic //e
    // auto-repeat cadence (driven in CPU time by AppleKeyboard::Tick).
    if (isRepeat)
    {
        return DxuiMessageResult::Handled;
    }

    // In joystick mode the X / Z keys are fire buttons (handled in OnKeyDown
    // / OnKeyUp), so swallow their WM_CHAR to keep the letters from also
    // typing into the //e keyboard latch -- mirroring how arrow keys are
    // withheld from the latch.
    if (m_inputMode == InputMappingMode::Joystick &&
        (dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches) != nullptr ||
         m_refs.gamePort != nullptr) &&
        (ch == L'x' || ch == L'X' || ch == L'z' || ch == L'Z'))
    {
        return DxuiMessageResult::Handled;
    }

    // The pre-checks above owned every WM_CHAR the shell should eat; a
    // surviving printable character is the guest's. Route it through the
    // viewport so the //e keyboard latch is fed on the same Dxui path as
    // the key transitions (FR-034).
    if (m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Char;
        ev.vk   = ch;

        (void) m_viewport->OnKey (ev);
    }

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnSize (UINT widthPx, UINT heightPx)
{
    int       width     = static_cast<int> (widthPx);
    int       renderH   = static_cast<int> (heightPx);



    UNREFERENCED_PARAMETER (widthPx);

    // A resize restretches the window; drop any open menu so its
    // window-anchored popup is not left stranded.
    if (m_mainMenu.IsOpen())
    {
        m_mainMenu.Hide();
    }

    // The host (DxuiHwndSource::HandleSize) already resized its swap
    // chain and recreated the back-buffer RTV + D2D target before this
    // OnSize fired. The renderer no longer owns the swap chain; it just
    // needs the new back-buffer dimensions for the CRT post-process.
    m_d3dRenderer.SetBackBufferSize (static_cast<int> (width), renderH);

    {
        UINT     dpi             = GetDpiForWindow (m_hwnd);
        HRESULT  hrUiR           = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                                                       m_d3dRenderer.GetBackBufferHeight(),
                                                       dpi);
        RECT     menuBarBounds   = { 0, m_host->CaptionHeightPx(), static_cast<int> (width), m_host->CaptionHeightPx() };

        IGNORE_RETURN_VALUE (hrUiR, S_OK);
        m_mainMenu.Layout (menuBarBounds, m_scaler);

        {
            RECT  vr            = ComputeViewportRect (static_cast<int> (width), renderH);
            int   bottomInsetPx = renderH - vr.bottom;
            bool  fHasDisk      = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();

            if (fHasDisk)
            {
                LayoutDriveWidgetsInCommandBar (m_driveChrome, bottomInsetPx, static_cast<int> (width), renderH, dpi);
            }
            else
            {
                // Machine has no Disk II controller (e.g. stripped Apple II
                // config). Collapse the drive widget rects so DriveWidget
                // paints nothing and the drag-drop overlay's empty-rect
                // path treats the whole window as the drop target. The
                // joystick-mode button still paints -- joystick input is
                // independent of disk presence.
                m_driveChrome[0].Hide();
                m_driveChrome[1].Hide();
            }

            LayoutPrinterIndicator (bottomInsetPx, static_cast<int> (width), renderH, dpi);

            // OnSize is the authoritative layout (only fires on a real WM_SIZE,
            // never per-frame), so record the disk-presence this window size now
            // accounts for. ReflowChromeForMachineChange reads this pre-switch
            // value to grow/shrink the window by the band delta.
            m_chromeSizedForHasDisk = fHasDisk;

            {
                int  bandTop    = renderH - bottomInsetPx;
                int  bandHeight = MulDiv (s_kJoystickButtonBandDp, static_cast<int> (dpi), s_kBaseDpi);

                m_driveBandSurface.SetBounds (RECT{ 0, bandTop, static_cast<int> (width), renderH });
                LayoutJoystickButton (static_cast<int> (width), bandTop, bandHeight, dpi);
            }

            m_uiShell.HitTest().Clear();
            if (fHasDisk)
            {
                m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[0].BodyRect(), DxuiHitSlot::Custom, 0 });
                m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[1].BodyRect(), DxuiHitSlot::Custom, 1 });
            }
        }
    }

    UpdateViewportLayout (static_cast<int> (width), renderH);

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

            m_pendingFramebuffer = m_uiFramebuffer.data();
        }
    }

    // Repaint immediately at the new size through the host pump. The
    // host already resized its swap chain in HandleSize; driving a
    // synchronous WM_PAINT here avoids a stale / black frame during an
    // interactive drag-resize (when RunMessageLoop is blocked in the OS
    // modal resize loop).
    InvalidateRect (m_hwnd, nullptr, FALSE);
    UpdateWindow   (m_hwnd);

    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());
    return DxuiMessageResult::NotHandled;
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

LRESULT EmulatorShell::OnDrawItem (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Legacy stub: no owner-drawn items active in the current chrome.
    // Defer to DefWindowProc for any unexpected WM_DRAWITEM so behavior
    // matches the legacy Window-base path (which returned `true` =
    // call DefWndProc).
    return DefWindowProc (hwnd, msg, wParam, lParam);
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

DxuiMessageResult EmulatorShell::OnTimer (UINT_PTR timerId)
{
    UNREFERENCED_PARAMETER (timerId);

    return DxuiMessageResult::NotHandled;
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

    // SwitchMachine calls this on the CPU thread; DxuiHwndSource::SetTitle
    // mutates the caption bar and asserts the UI thread. Bounce off-thread
    // callers through the message loop (WM_APP_DXUI_UPDATE_TITLE handler above).
    if (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId())
    {
        PostMessageW (m_hwnd, WM_APP_DXUI_UPDATE_TITLE, 0, 0);
        return;
    }

    title = L"Casso";

    if (!m_config.name.empty())
    {
        wideName = fs::path (m_config.name).wstring();
        title += L" - ";
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

    m_host->SetTitle (title);
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
//  TraceProgressWindow
//
//  Minimal GDI-painted progress window for the --trace file dump. Drawn
//  entirely in WM_PAINT (no common controls) so it stays robust even when
//  raised from inside the unhandled-exception filter on the CPU thread.
//  Shows the reason, the full output path, and "N of M instructions (P%)"
//  with a fill bar. SetProgress repaints synchronously and pumps pending
//  messages so the window keeps redrawing during a multi-second write.
//
////////////////////////////////////////////////////////////////////////////////

class TraceProgressWindow
{
public:
    bool Create (const std::wstring & reason, const std::wstring & path, uint64_t total)
    {
        HRESULT     hr      = S_OK;
        WNDCLASSEXW wc      = { sizeof (wc) };
        HINSTANCE   hInst   = GetModuleHandleW (nullptr);
        DWORD       style   = WS_POPUP | WS_BORDER | WS_CAPTION;
        RECT        wr      = {};
        int         winW    = 0;
        int         winH    = 0;
        int         screenW = GetSystemMetrics (SM_CXSCREEN);
        int         screenH = GetSystemMetrics (SM_CYSCREEN);

        m_reason = reason;
        m_path   = path;
        m_total  = total;
        m_done   = 0;

        wc.lpfnWndProc   = &TraceProgressWindow::WndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW (nullptr, IDC_WAIT);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wc.lpszClassName = s_kpszClass;
        RegisterClassExW (&wc);            // benign if already registered

        // Create at a provisional position so the window's monitor DPI is
        // known, then size the client area to fit the DPI-scaled content
        // and recentre. Sizing the *client* rect (via AdjustWindowRect)
        // keeps a uniform margin around the content at any DPI -- a fixed
        // window height let the caption eat the client area and clipped
        // the progress bar against the bottom edge on high-DPI displays.
        m_hwnd = CreateWindowExW (WS_EX_TOPMOST,
                                  s_kpszClass,
                                  L"Casso \x2014 Writing trace",
                                  style,
                                  0, 0, s_kClientWPx, s_kClientHPx,
                                  nullptr, nullptr, hInst, this);
        CWR (m_hwnd);

        m_dpi = GetDpiForWindow (m_hwnd);
        if (m_dpi == 0)
        {
            m_dpi = 96;
        }

        // Use the OS themed message font (Segoe UI on Win10/11) sized for
        // this window's DPI. Without an explicit font GDI falls back to the
        // ancient bitmap SYSTEM_FONT, which looks aliased, ignores DPI, and
        // can't render the U+2026 ellipsis.
        {
            NONCLIENTMETRICSW  ncm = { sizeof (ncm) };

            if (SystemParametersInfoForDpi (SPI_GETNONCLIENTMETRICS, sizeof (ncm), &ncm, 0, m_dpi))
            {
                m_font = CreateFontIndirectW (&ncm.lfMessageFont);
            }
        }

        wr.left   = 0;
        wr.top    = 0;
        wr.right  = Scaled (s_kClientWPx);
        wr.bottom = Scaled (s_kClientHPx);
        AdjustWindowRectExForDpi (&wr, style, FALSE, WS_EX_TOPMOST, m_dpi);

        winW = wr.right - wr.left;
        winH = wr.bottom - wr.top;

        SetWindowPos (m_hwnd, HWND_TOPMOST,
                      (screenW - winW) / 2, (screenH - winH) / 2,
                      winW, winH, SWP_NOACTIVATE);

        ShowWindow   (m_hwnd, SW_SHOW);
        UpdateWindow (m_hwnd);

    Error:
        return m_hwnd != nullptr;
    }

    void SetProgress (uint64_t done, uint64_t total)
    {
        HRESULT  hr  = S_OK;
        MSG      msg = {};



        m_done  = done;
        m_total = total;

        BAIL_OUT_IF (m_hwnd == nullptr, S_OK);

        InvalidateRect (m_hwnd, nullptr, FALSE);
        UpdateWindow   (m_hwnd);

        while (PeekMessageW (&msg, m_hwnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage (&msg);
            DispatchMessageW  (&msg);
        }

    Error:
        return;
    }

    void Destroy()
    {
        if (m_hwnd != nullptr)
        {
            DestroyWindow (m_hwnd);
            m_hwnd = nullptr;
        }

        if (m_font != nullptr)
        {
            DeleteObject (m_font);
            m_font = nullptr;
        }
    }

private:
    static LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        TraceProgressWindow *  self   = reinterpret_cast<TraceProgressWindow *> (
            GetWindowLongPtrW (hwnd, GWLP_USERDATA));
        LRESULT                result = 0;

        if (msg == WM_CREATE)
        {
            CREATESTRUCTW *  cs = reinterpret_cast<CREATESTRUCTW *> (lParam);
            SetWindowLongPtrW (hwnd, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR> (cs->lpCreateParams));
        }
        else if (msg == WM_PAINT && self != nullptr)
        {
            self->OnPaint (hwnd);
        }
        else
        {
            result = DefWindowProcW (hwnd, msg, wParam, lParam);
        }

        return result;
    }

    void OnPaint (HWND hwnd)
    {
        PAINTSTRUCT  ps      = {};
        HDC          hdc     = BeginPaint (hwnd, &ps);
        RECT         rc      = {};
        RECT         bar     = {};
        int          pad     = Scaled (s_kPadPx);
        int          line    = Scaled (s_kLinePx);
        int          barH    = Scaled (s_kBarPx);
        int          gap     = Scaled (s_kGapSmallPx);
        int          pct     = (m_total > 0) ? (int) ((m_done * 100) / m_total) : 0;
        wchar_t      line1[128] = {};
        wchar_t      line3[160] = {};
        HBRUSH       fill    = CreateSolidBrush (RGB (0x2D, 0x7D, 0x46));

        GetClientRect (hwnd, &rc);

        HFONT  oldFont = (m_font != nullptr) ? (HFONT) SelectObject (hdc, m_font) : nullptr;

        // SetProgress invalidates without erasing (bErase = FALSE) and the
        // text is drawn transparently, so wipe the client area first --
        // otherwise each new progress value piles up on the previous one.
        FillRect (hdc, &rc, (HBRUSH) (COLOR_WINDOW + 1));

        swprintf_s (line1, L"Writing execution trace (%s)\x2026", m_reason.c_str());
        swprintf_s (line3, L"%llu of %llu instructions  (%d%%)",
                    (unsigned long long) m_done, (unsigned long long) m_total, pct);

        SetBkMode   (hdc, TRANSPARENT);

        RECT  r1 = { pad, pad, rc.right - pad, pad + line };
        DrawTextW (hdc, line1, -1, &r1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT  r2 = { pad, r1.bottom + gap, rc.right - pad, r1.bottom + gap + line };
        DrawTextW (hdc, m_path.c_str(), -1, &r2, DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS);

        RECT  r3 = { pad, r2.bottom + gap, rc.right - pad, r2.bottom + gap + line };
        DrawTextW (hdc, line3, -1, &r3, DT_LEFT | DT_SINGLELINE);

        // Anchor the bar to the bottom with a margin equal to the top pad,
        // so the bottom whitespace mirrors the caption-to-text gap and the
        // bar can't be clipped by the window edge at any DPI.
        bar.left   = pad;
        bar.right  = rc.right - pad;
        bar.bottom = rc.bottom - pad;
        bar.top    = bar.bottom - barH;
        FrameRect (hdc, &bar, (HBRUSH) GetStockObject (GRAY_BRUSH));

        RECT  filled = bar;
        filled.right = bar.left + (LONG) (((bar.right - bar.left) * (LONGLONG) pct) / 100);
        FillRect (hdc, &filled, fill);

        DeleteObject (fill);

        if (oldFont != nullptr)
        {
            SelectObject (hdc, oldFont);
        }

        EndPaint (hwnd, &ps);
    }

    int Scaled (int px) const { return MulDiv (px, (int) m_dpi, 96); }

    static constexpr const wchar_t *  s_kpszClass   = L"CassoTraceProgress";
    static constexpr int              s_kClientWPx  = 556;
    static constexpr int              s_kPadPx      = 16;
    static constexpr int              s_kLinePx     = 22;
    static constexpr int              s_kBarPx      = 22;
    static constexpr int              s_kGapSmallPx = 4;
    static constexpr int              s_kGapBarPx   = 8;

    // Client height kept in lockstep with the OnPaint layout: top pad +
    // three text lines (each followed by a small gap) + the larger gap
    // above the bar + the bar + an equal bottom pad.
    static constexpr int              s_kClientHPx  = s_kPadPx
                                                    + 3 * s_kLinePx
                                                    + 2 * s_kGapSmallPx
                                                    + s_kGapBarPx
                                                    + s_kBarPx
                                                    + s_kPadPx;

    HWND          m_hwnd  = nullptr;
    HFONT         m_font  = nullptr;
    UINT          m_dpi   = 96;
    std::wstring  m_reason;
    std::wstring  m_path;
    uint64_t      m_done  = 0;
    uint64_t      m_total = 0;
};




////////////////////////////////////////////////////////////////////////////////
//
//  DumpTrace
//
//  Write the CPU execution-trace ring to a timestamped text file in the
//  working directory, showing a progress window. Best-effort and one-shot:
//  guarded so the graceful-exit path and the crash handler cannot both
//  write, and a no-op when --trace is off. Safe to call from the crash
//  handler on the CPU thread (the thread that owns the ring) since the
//  process is already halted at the fault.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DumpTrace (const wstring & reason)
{
    bool          expected = false;
    SYSTEMTIME    st       = {};
    wchar_t       name[64] = {};
    wchar_t       cwd[MAX_PATH] = {};
    std::wstring  path;
    uint64_t      total    = 0;

    if (!m_traceDumped.compare_exchange_strong (expected, true))
    {
        return;
    }

    if (m_traceCapacity == 0 || m_cpu == nullptr || !m_cpu->IsTraceEnabled())
    {
        return;
    }

    GetLocalTime (&st);
    swprintf_s (name, L"casso-trace-%04u%02u%02u-%02u%02u%02u.txt",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    if (GetCurrentDirectoryW (MAX_PATH, cwd) > 0)
    {
        path = std::wstring (cwd) + L"\\" + name;
    }
    else
    {
        path = name;
    }

    total = m_cpu->GetTraceCount();

    TraceProgressWindow  win;
    win.Create (reason, path, total);

    m_cpu->DumpTraceToFile (path, [&win] (uint64_t done, uint64_t tot)
    {
        win.SetProgress (done, tot);
    });

    win.Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDisk2DebugDialog
//
//  Spec-006 / FR-001 / FR-017 / FR-024. View -> Disk II Debug...
//  command handler and Ctrl+Shift+D accelerator target. Lazy-creates
//  the modeless dialog on first open, wires it as the controller's
//  event sink AND as the active Disk2AudioSource's audio-event
//  sink, applies the uptime anchor and the multi-controller title
//  hint, then shows + foregrounds the window. Subsequent calls
//  short-circuit to Show + SetForegroundWindow.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenDisk2DebugDialog()
{
    HRESULT             hr           = S_OK;
    Disk2Controller *   controller   = nullptr;
    int                 Disk2Count  = 0;
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
            Disk2Count++;
        }
    }

    if (m_disk2DebugPanel == nullptr || m_disk2DebugPanel->Hwnd() == nullptr)
    {
        hInstance          = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_disk2DebugPanel = std::make_unique<Disk2DebugPanel>();

        hr = m_disk2DebugPanel->Create (hInstance,
                                         m_hwnd,
                                         m_d3dRenderer.GetDevice(),
                                         m_d3dRenderer.GetContext(),
                                         &m_chromeTheme);
        CHRF (hr, m_disk2DebugPanel.reset());

        m_disk2DebugPanel->SetUptimeAnchor (m_uptimeAnchor);
        m_disk2DebugPanel->SetMultiControllerHint (Disk2Count > 1);

        if (m_cpu != nullptr)
        {
            m_disk2DebugPanel->SetCycleCounter (m_cpu->GetCycleCounterPtr());
        }

        controller->SetEventSink (m_disk2DebugPanel.get());

        for (i = 0; i < m_diskAudioSources.size(); i++)
        {
            if (m_diskAudioSources[i] != nullptr)
            {
                m_diskAudioSources[i]->SetAudioEventSink (m_disk2DebugPanel.get());
            }
        }
    }
    else
    {
        m_disk2DebugPanel->SetMultiControllerHint (Disk2Count > 1);
    }

    m_disk2DebugPanel->Show();
    SetForegroundWindow (m_disk2DebugPanel->Hwnd());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenInputDebugDialog
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenInputDebugDialog()
{
    HRESULT    hr        = S_OK;
    HINSTANCE  hInstance = nullptr;



    CBR (m_refs.keyboard != nullptr);

    if (m_inputDebugPanel == nullptr || m_inputDebugPanel->Hwnd() == nullptr)
    {
        hInstance         = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_inputDebugPanel = std::make_unique<InputDebugPanel>();

        hr = m_inputDebugPanel->Create (hInstance,
                                         m_hwnd,
                                         m_d3dRenderer.GetDevice(),
                                         m_d3dRenderer.GetContext(),
                                         &m_chromeTheme);
        CHRF (hr, m_inputDebugPanel.reset());

        m_inputDebugPanel->SetUptimeAnchor (m_uptimeAnchor);

        if (m_cpu != nullptr)
        {
            m_inputDebugPanel->SetCycleCounter (m_cpu->GetCycleCounterPtr());
        }

        m_refs.keyboard->SetInputEventSink (m_inputDebugPanel.get());

        auto * iieSwitches = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (m_inputDebugPanel.get());
        }

        if (m_refs.gamePort != nullptr)
        {
            m_refs.gamePort->SetInputEventSink (m_inputDebugPanel.get());
        }
    }

    m_inputDebugPanel->Show();
    SetForegroundWindow (m_inputDebugPanel->Hwnd());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachDebugSinksIfOpen
//
//  Spec-006 bug 15. SwitchMachine tears down the old controller and
//  audio source then constructs new ones via CreateMemoryDevices,
//  but the panel's sink wiring only ran inside OpenDisk2DebugDialog
//  on first open -- the new controller starts with m_eventSink ==
//  nullptr and the new audio source with m_audioEventSink == nullptr,
//  so the debug window goes silent post-switch. Re-attach both
//  sinks if the panel is still open. No-op when the panel has
//  never been opened.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AttachDebugSinksIfOpen()
{
    HRESULT             hr         = S_OK;
    Disk2Controller *   controller = nullptr;
    size_t              i          = 0;

    CBR (m_disk2DebugPanel != nullptr);

    controller = m_diskManager->FindSlot6Controller();

    if (controller != nullptr)
    {
        controller->SetEventSink (m_disk2DebugPanel.get());
    }

    for (i = 0; i < m_diskAudioSources.size(); i++)
    {
        if (m_diskAudioSources[i] != nullptr)
        {
            m_diskAudioSources[i]->SetAudioEventSink (m_disk2DebugPanel.get());
        }
    }

    if (m_inputDebugPanel != nullptr && m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetInputEventSink (m_inputDebugPanel.get());

        auto * iieSwitches = dynamic_cast<Apple2eSoftSwitchBank *> (m_refs.softSwitches);
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (m_inputDebugPanel.get());
        }

        if (m_refs.gamePort != nullptr)
        {
            m_refs.gamePort->SetInputEventSink (m_inputDebugPanel.get());
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

DxuiMessageResult EmulatorShell::OnInitMenuPopup (HMENU hMenu, UINT itemIndex, bool isWindowMenu)
{
    bool  callDefWndProc = m_windowCommandManager->OnInitMenuPopup (m_hwnd, hMenu, itemIndex, isWindowMenu);

    return callDefWndProc ? DxuiMessageResult::NotHandled : DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDpiChanged
//
//  Mirror the host's new DPI into our local DxuiDpiScaler so the
//  chrome-band dock (which scales band thicknesses through m_scaler)
//  returns coherent sizes for any post-DPI-change relayout. The host has
//  already applied the OS-suggested rect via SetWindowPos before
//  this fires; subsequent WM_SIZE will drive the visible relayout.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnDpiChanged (UINT newDpi)
{
    m_scaler.SetDpi (newDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcMouseMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnNcMouseMove (LRESULT hitTest, int xScreen, int yScreen)
{
    (void) hitTest;
    (void) xScreen;
    (void) yScreen;

    // The host owns caption / system-button hover now. Our only stake in
    // a non-client move is dropping a latched menu hover: when the
    // pointer leaves the menu upward into the caption the client
    // mouse-move stream stops, so this is the one signal that clears it.
    m_mainMenu.ClearHover();
    InvalidateRect (m_hwnd, nullptr, FALSE);
    return DxuiMessageResult::NotHandled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnNcMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnNcMouseLeave()
{
    // Caption-button hover teardown is the host's job; nothing to do here.
    return DxuiMessageResult::NotHandled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnNcLButtonDown (LRESULT hitTest, int xScreen, int yScreen)
{
    (void) hitTest;
    (void) xScreen;
    (void) yScreen;

    // Any non-client press (caption drag, system button, system menu,
    // snap) dismisses an open menu -- its popup is anchored to the window
    // and a move / system action would strand it. The host then routes
    // the press to its own DxuiSystemButton (press state) or to
    // DefWindowProc (caption drag), so we never claim the message.
    if (m_mainMenu.IsOpen())
    {
        m_mainMenu.Hide();
    }
    return DxuiMessageResult::NotHandled;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonUp
//
//  System-button clicks (min / max / close) dispatch through the host's
//  DxuiSystemButton children; caption double-clicks, the system menu,
//  and snap layouts fall through to DefWindowProc. Nothing to claim here.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnNcLButtonUp (LRESULT hitTest, int xScreen, int yScreen)
{
    (void) hitTest;
    (void) xScreen;
    (void) yScreen;
    return DxuiMessageResult::NotHandled;
}




