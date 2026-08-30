#include "Pch.h"

#include "EmulatorShell.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Devices/Printer/PrinterCard.h"
#include "Ui/PrinterPanel.h"

#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
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
#include "Devices/Mockingboard/MockingboardCard.h"
#include "Devices/LanguageCard.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/Apple2cRomBank.h"
#include "Devices/AppleMouse.h"
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
#include "Ui/Dialogs/SalvageDialogContent.h"
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
//  User-notification marshaling
//
//  A notification raised off the UI thread is posted rather than shown: the
//  dialog is Dxui, and Dxui asserts UI-thread affinity. lParam carries an
//  owned wstring the message loop deletes.
//
////////////////////////////////////////////////////////////////////////////////

#define WM_APP_NOTIFY_USER     (WM_APP + 0x22)
#define WM_APP_REPORT_DAMAGE   (WM_APP + 0x23)
#define WM_APP_RUN_SALVAGE     (WM_APP + 0x24)
#define WM_APP_MOUNT_COMPLETED (WM_APP + 0x25)

// The shell the EHM notification sink forwards to. One shell per process;
// cleared in the destructor so a late report cannot touch a dead object.
static EmulatorShell *  s_pNotifyShell = nullptr;

// Reports raised before there is a window to parent a dialog to. File scope
// rather than a shell member because the sink is installed at the top of
// wWinMain, before the shell is constructed -- command-line and machine-config
// failures happen in that window and must not vanish. Drained once the window
// exists. The CPU thread can append, hence the lock.
static std::vector<std::wstring>  s_pendingNotifications;
static std::mutex                 s_pendingNotifyMutex;





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
//  File-scope tuning for the shell: framebuffer geometry, chrome band metrics
//  in design pixels, the idle-loop cadences, and the joystick / paddle rails.
//
//  The three timing values are separate on purpose and are not interchangeable:
//
//    publish interval  caps how often a Maximum-speed run rasterizes and
//                      publishes a frame, so the render side stays near 60 Hz
//                      instead of chasing thousands of frames nobody sees
//    animation tick    the wake cadence while a tooltip dwell timer is pending
//    idle upkeep       the CEILING on how long the idle loop may block. Drive
//                      activity can only be sampled by running
//                      UpdateDriveWidgets (it diffs nibble counters), so the
//                      loop has to wake this often or motor-on onset behind an
//                      otherwise static screen is missed
//
//  The dp band metrics are coupled to widget internals: changing the joystick
//  button's font or padding requires updating its band height and both drive-
//  bar heights together, since nothing recomputes them from the widget.
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

// Presentation pacing. At Maximum speed the CPU runs flat-out, but we only
// rasterize + publish a framebuffer this often (wall clock), so the render
// side stays ~60 Hz instead of chasing thousands of unseen frames a second.
static constexpr int64_t s_kMaxSpeedPublishIntervalUs = 16667;   // ~60 Hz

// Idle UI-loop wake cadence while a tooltip dwell timer is still pending.
static constexpr DWORD   s_kIdleAnimationTickMs       = 16;      // ~60 Hz

// Upper bound on how long the idle UI loop blocks between frames/messages.
// Drive-activity indicators can only be sampled by running UpdateDriveWidgets
// (it diffs nibble counters), so the loop must wake at least this often to
// catch motor-on onset behind an otherwise static screen -- 50 ms matches the
// long-standing drive-activity refresh target.
static constexpr DWORD   s_kIdleUpkeepMs              = 50;      // ~20 Hz

// Desk-scene composition: with the CRT monitor framing on, the drive widgets
// render at 80% of their design size so they sit in proportion under the
// monitor. Folded into m_chromeSceneScale (NOT into DriveWidget) so switching
// the monitor off returns the drives to their full classic size.
static constexpr float   s_kDeskDriveScale       = 0.8f;

// The desk band height and the monitor fit depend on each other (the band
// scales with the monitor's SceneScale, which depends on the center the band
// leaves). The dependency is a contraction, so a few relayout passes settle it.
static constexpr int     s_kSceneScaleSettlePasses = 3;

// Fullscreen drive overlay strip: the band's height and the bottom-edge
// dwell zone that reveals it while the host owns the pointer.
static constexpr int     s_kStripBandDp     = 150;
static constexpr int     s_kStripEdgeZoneDp = 8;

// The basename strip under each 3D drive: the 2D widget's label geometry
// (18 dp strip, 2 dp gap, 11 dip text), kept so the mounted image's name
// reads off the screen instead of only out of a tooltip.
static constexpr int     s_kSceneDriveLabelStripDp  = 18;
static constexpr int     s_kSceneDriveLabelGapDp    = 2;

// The name strip's width, CONSTANT rather than the drive's projected width:
// a projected box widens and narrows as the orbit turns it, and a label
// that keeps changing size while it moves reads as chrome coming unglued.
static constexpr int     s_kSceneDriveLabelWidthDp  = 200;

// The pointer-capture banner: how to get the mouse back, said for as long as
// it is held. Low on the picture, where a paddle game's action is not.
//
// The band is deliberately taller than the line it holds: the halo behind
// the text spreads past the ink, and a rect fitted to the glyphs would clip
// its own shadow against the edges.
static constexpr int     s_kCaptureBannerHeightDp   = 44;
static constexpr int     s_kCaptureBannerInsetDp    = 16;
static constexpr float   s_kCaptureBannerFontDip    = DxuiHudNotice::kFontDip;

static const std::wstring         s_kCaptureBanner =
    std::wstring (L"Paddle Mode ") + s_kchEmDash + L" press Esc to release the mouse";
static constexpr float   s_kSceneDriveLabelFontDip  = 11.0f;

// Padding around the 3D drive row when the CRT monitor is opted out and the
// row composes into the classic bottom band -- breathing room off the window
// edge, the way the 2D widgets' band padding sat around them. (Containment
// itself is exact: ComputeStrip solves the standoff in the gaze's frame.)
static constexpr int     s_kSceneDriveRowPadDp = 10;

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
//  Geometry for the bottom command bar's occupants -- the drive widgets and
//  the joystick-mode button -- plus the window-size reconciliation that runs
//  once the frame has materialized.
//
//  Two ideas recur through this block.
//
//  Desk-scene zoom is applied by folding the scene scale into the EFFECTIVE
//  DPI rather than by scaling rects afterwards. Widget geometry, fonts, and
//  the inter-widget gaps then zoom together for free, because every one of
//  them already derives from DPI.
//
//  The drives are laid out as objects on a desk, not as flat controls. Each
//  widget is skewed toward a shared vanishing point at the client center by a
//  factor matching the case-top depth ratio in DriveWidget, so two drives side
//  by side read as sitting on the same surface under the same monitor rather
//  than as two identical sprites.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutDriveWidgetsInCommandBar (
    std::array<DriveWidget, 2>  & driveChrome,
    int                           bottomInsetPx,
    int                           clientW,
    int                           clientH,
    UINT                          dpi,
    float                         sceneScale)
{
    int            bottomInset   = 0;
    int            commandBarTop = 0;
    int            gap           = 0;
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



    // Desk-scene zoom: the drives scale with the monitor. Fold the scale
    // into the effective DPI so widget geometry, fonts, and the inter-
    // widget gaps all zoom together.
    dpi = (UINT) lroundf ((float) dpi * sceneScale);

    bottomInset = bottomInsetPx;
    commandBarTop = std::max (0, clientH - bottomInset);
    gap = MulDiv (s_kDriveWidgetGapDp, static_cast<int> (dpi), s_kBaseDpi);



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

        // Visible again: the desk scene turns these off rather than just
        // collapsing them, and this is the one path that brings the flat
        // widgets back, so it is where they earn their visibility.
        driveChrome[i].SetVisible (true);
        driveChrome[i].SetPerspectiveSkewPx (skewPx);
        driveChrome[i].Layout (widgetAnchor, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetCursorMonitorWorkArea
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryGetCursorMonitorWorkArea (RECT & outWork, HMONITOR & outMonitor)
{
    POINT          pt       = {};
    HMONITOR       hMon     = nullptr;
    MONITORINFOEXW mi       = { sizeof (mi) };
    bool           hasWork  = false;



    if (!GetCursorPos (&pt))
    {
        pt.x = 0;
        pt.y = 0;
    }

    hMon    = MonitorFromPoint (pt, MONITOR_DEFAULTTONEAREST);
    hasWork = hMon != nullptr && GetMonitorInfoW (hMon, &mi);

    if (hasWork)
    {
        outWork    = mi.rcWork;
        outMonitor = hMon;
    }

    return hasWork;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::CenterInWorkArea
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CenterInWorkArea (
    const RECT & work,
    int          windowW,
    int          windowH,
    LONG       & outX,
    LONG       & outY)
{
    outX = work.left + (work.right - work.left - windowW) / 2;
    outY = work.top  + (work.bottom - work.top - windowH) / 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LoadIconAsPremulBgra
//
//  Loads an HICON resource into a CPU-side premultiplied BGRA8
//  pixel buffer suitable for the DxuiTextRenderer::DrawIconBitmap
//  path. Uses a GDI memory DC + 32-bit DIB section to capture the
//  icon's alpha-channelled pixels (LoadImageW preserves alpha when
//  LR_DEFAULTCOLOR is set on a Vista+ icon). Premultiplies the
//  pixels in place because D2D's DrawBitmap expects premultiplied
//  sources.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::LoadIconAsPremulBgra (
    HINSTANCE               hInstance,
    int                     iconResourceId,
    int                     sizePx,
    std::vector<uint32_t> & outPixels,
    int                   & outW,
    int                   & outH)
{
    HRESULT     hr          = S_OK;
    HICON       hIcon       = nullptr;
    HDC         screenDc    = nullptr;
    HDC         memDc       = nullptr;
    HBITMAP     dib         = nullptr;
    HBITMAP     oldBitmap   = nullptr;
    void      * dibBits     = nullptr;
    BITMAPINFO  bmi         = {};
    BOOL        drawn       = FALSE;
    uint32_t  * src         = nullptr;
    size_t      i           = 0;
    size_t      pixelCount  = (size_t) sizePx * (size_t) sizePx;
    HRESULT     hrGle       = E_FAIL;



    // Every failure here is a Win32 one with a real reason behind it -- a
    // missing resource id reads differently from an exhausted GDI heap -- so
    // the OS code is carried out rather than flattened to "no icon". The
    // handles are released at Error:, which every bail below routes through.
    //
    // GetLastError is read into hrGle BEFORE the check, never inside it: a
    // call in a macro condition is forbidden, and any intervening call could
    // clobber the thread's error code anyway.
    hIcon = (HICON) LoadImageW (hInstance,
                                MAKEINTRESOURCEW (iconResourceId),
                                IMAGE_ICON,
                                sizePx, sizePx,
                                LR_DEFAULTCOLOR);

    if (hIcon == nullptr)
    {
        hrGle = HRESULT_FROM_WIN32 (GetLastError());
    }

    CBREx (hIcon != nullptr, hrGle);

    screenDc = GetDC (nullptr);
    memDc    = CreateCompatibleDC (screenDc);
    CPR (memDc);

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sizePx;
    bmi.bmiHeader.biHeight      = -sizePx;   // top-down DIB
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib = CreateDIBSection (memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    CPR (dib);
    CPR (dibBits);

    oldBitmap = (HBITMAP) SelectObject (memDc, dib);

    // Clear the DIB to transparent so the icon's alpha channel composites
    // against zero instead of the screen DC's garbage contents.
    memset (dibBits, 0, pixelCount * sizeof (uint32_t));

    drawn = DrawIconEx (memDc, 0, 0, hIcon, sizePx, sizePx, 0, nullptr, DI_NORMAL);

    if (!drawn)
    {
        hrGle = HRESULT_FROM_WIN32 (GetLastError());
    }

    CBREx (drawn, hrGle);

    src = (uint32_t *) dibBits;
    outPixels.assign (pixelCount, 0);

    // Premultiply each BGRA pixel. DIB layout is 0xAARRGGBB in little-endian
    // uint32 (B,G,R,A in memory order).
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

    outW = sizePx;
    outH = sizePx;

Error:
    // Unwound in reverse acquisition order, each guarded: a bail from any of
    // the checks above lands here with only some of them owned.
    if (oldBitmap != nullptr) { SelectObject (memDc, oldBitmap); }
    if (dib != nullptr)       { DeleteObject (dib); }
    if (memDc != nullptr)     { DeleteDC (memDc); }
    if (screenDc != nullptr)  { ReleaseDC (nullptr, screenDc); }
    if (hIcon != nullptr)     { DestroyIcon (hIcon); }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
//  Builds only what needs no configuration. Anything that depends on the
//  resolved asset directory or the chosen machine waits for Initialize --
//  DiskManager in particular, because it needs a UserConfigStore that does
//  not exist until the asset base dir is known.
//
//  The Prng is the deterministic stand-in for indeterminate //e DRAM at
//  power-on (FR-035), shared by every device that re-seeds in PowerCycle. Its
//  seed mixes two weakly-correlated host sources so consecutive launches land
//  on different power-on patterns instead of the same one every time; tests
//  pin the seed through the harness rather than coming through this path.
//
//  Debug builds log that seed, because a fault caused by uninitialized DRAM
//  is otherwise unreproducible. Re-running with the same seed reproduces
//  byte-identical DRAM at every PowerCycle, which is the first thing needed
//  to chase a flaky illegal-opcode fault.
//
//  VideoTiming is owned at the SHELL level rather than per machine so all
//  three machine kinds share one 17,030-cycle frame counter behind $C019
//  (RDVBLBAR); a per-machine copy would restart the beam on every switch
//  (FR-033 / T055).
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
                                                              m_framebufferMutex,
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
//  Teardown is ordered by who points at whom, and every step below is placed
//  against a dangling reference it would otherwise create.
//
//  The CPU thread stops first: nothing can be safely destroyed while it is
//  still executing instructions against these devices.
//
//  Debug panels are unwired before they are destroyed. Each was registered as
//  an event SINK on live machine devices (disk controller, drive audio,
//  keyboard, //e soft switches, game port), and those devices outlive the
//  panel -- they die later, with m_ownedDevices. Resetting a panel without
//  first revoking its sinks leaves the devices calling into freed memory. The
//  disk panel revokes controller then audio, mirroring the attachment order
//  in OpenDisk2DebugDialog (Spec-006 / FR-024).
//
//  Dirty disks are flushed before anything owning them unwinds, so a clean
//  quit never loses user writes (T097 / FR-025).
//
//  Adopted chrome is released explicitly. m_mainMenu, m_driveChrome, and
//  m_joystickButton are registered into m_host->Root() as RAW pointers via
//  DxuiPanel::Adopt, and they are members of this object -- so field-by-field
//  destruction below would leave the host's panel tree holding pointers into
//  a partially destroyed shell. ClearAdopted cuts those links while every
//  member is still whole. (The caption bar is host-owned, not adopted, and is
//  deliberately not in that set.)
//
//  OLE is uninitialized last, and only if this object initialized it.
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::~EmulatorShell()
{
    HRESULT             hrFlush    = S_OK;
    Disk2Controller *   controller = nullptr;



    // Before anything else tears down: a notification arriving mid-teardown
    // must not reach a half-destroyed shell.
    SetNotifyFunction (nullptr);
    s_pNotifyShell = nullptr;

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

        for (auto & diskAudioSource : m_diskAudioSources)
        {
            if (diskAudioSource != nullptr)
            {
                diskAudioSource->SetAudioEventSink (nullptr);
            }
        }

        m_disk2DebugPanel.reset();
    }

    if (m_inputDebugPanel != nullptr)
    {
        Apple2eSoftSwitchBank * iieSwitches = nullptr;

        if (m_refs.keyboard != nullptr)
        {
            m_refs.keyboard->SetInputEventSink (nullptr);
        }

        iieSwitches = m_refs.iieSoftSwitches;
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
//  Full startup, in the only order that works. Most of the sequence is forced
//  by a dependency, and the ones that are not obvious are called out here.
//
//  OLE comes up early because RegisterDragDrop and IFileDialog (drive-widget
//  click-to-browse) both need the UI thread already in an STA. OleInitialize
//  implies CoInitializeEx(STA), and its S_FALSE "already initialized" result
//  still takes a reference -- which is why m_fOleInitialized is set on both
//  success codes and paired with OleUninitialize at shutdown. A hard failure
//  means something already claimed this thread's apartment with a conflicting
//  model, which is a startup-ordering bug on our side, so it asserts.
//
//  The window is created before the devices because the renderer needs an
//  HWND, and the devices need the renderer's device / context.
//
//  Video watch pages are marked once, here, rather than per machine: a write
//  into text pages 1/2 ($0400-$0BFF) or hi-res pages 1/2 ($2000-$5FFF) raises
//  the bus video-dirty flag that drives the render-skip gate. Watching the
//  PAGE INDEX covers main and aux together, since the //e MMU re-points those
//  same indices, and the page layout is identical on every Apple II variant --
//  so this survives an in-session machine switch untouched.
//
//  ReconcileInitialClientSize runs after ShowWindow (the non-client frame is
//  not fully materialized before that) but before UpdateWindowTitle, so a
//  wrong-size window never flashes on screen.
//
//  PowerCycle MUST precede MountCommandLineDisks. It seeds DRAM from the
//  shared Prng and runs the 6502 /RESET sequence (FR-034) -- without it the
//  CPU starts at PC=0 and executes uninitialized RAM into a beep loop instead
//  of the firmware prompt. It also ejects every drive and re-binds the engine
//  to the controller's empty internal disk, so mounting first and power-
//  cycling second silently discards the user's image: the engine keeps
//  ticking but AdvanceOneBit exits immediately on an empty track.
//
//  Startup disks reach the MRU the same way every other mount does, through
//  the mount-completion hook, so a --disk1 the loader refuses is reported and
//  is not offered back by the picker next launch. Both the report and the MRU
//  write are posted rather than run here: this is before the message loop, and
//  a modal raised from it would sit in front of a machine nothing is running.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Initialize (
    HINSTANCE             hInstance,
    const wstring       & machineName,
    const MachineConfig & config,
    const string        & disk1Path,
    const string        & disk2Path)
{
    HRESULT  hr = S_OK;



    m_currentMachineName = machineName;
    m_config             = config;
    m_cyclesPerFrame     = config.cyclesPerFrame;

    // wWinMain installed the sink already; this just gives it a shell to
    // forward to. Anything reported before now is sitting in the queue and
    // is replayed once the window exists.
    s_pNotifyShell = this;

    RegisterChromeDock();
    InitAssetPathsAndStores();

    // Bring up OLE on the UI thread before any RegisterDragDrop / IFileDialog
    // (drive-widget click-to-browse) needs the STA apartment. OleInitialize
    // implies CoInitializeEx(STA); S_FALSE (already initialized) still owns a
    // reference we pair with OleUninitialize at shutdown. A hard failure here
    // means the UI thread's apartment was already claimed with a conflicting
    // model -- a startup-ordering bug on our side, so assert.
    hr = OleInitialize (nullptr);
    CHRA (hr);

    m_fOleInitialized = true;

    // Register built-in device factories
    ComponentRegistry::RegisterBuiltinDevices (m_registry);

    AllocateFramebuffers();
    PrimeChromeThemeEarly();

    hr = CreateEmulatorWindow (hInstance);
    CHR (hr);

    hr = BuildMachineDevices (config);
    CHR (hr);

    // Mark the display pages so a write into them raises the bus video-dirty
    // flag that drives the render-skip gate: text pages 1/2 ($0400-$0BFF) and
    // hi-res pages 1/2 ($2000-$5FFF). Aux writes share these page indices (the
    // //e MMU re-points them), so watching the index covers main and aux.
    // The page layout is identical across every Apple II variant, so this is
    // set once and survives an in-session machine switch.
    for (int page = 0x04; page <= 0x0B; page++)
    {
        m_memoryBus.SetVideoWatchPage (page, true);
    }

    for (int page = 0x20; page <= 0x5F; page++)
    {
        m_memoryBus.SetVideoWatchPage (page, true);
    }

    hr = InitializeRenderer();
    CHR (hr);

    hr = InitializeUiShell();
    CHR (hr);

    // Native-only bootstrap baseline: legacy chrome overlay retired
    // ahead of the native painter. Keep existing command/menu path active.

    // WASAPI audio is initialized on the CPU thread (COM apartment requirement)

    // Show window. A placement saved while maximized restores the state,
    // not just the normal rect it was created with: showing maximized
    // directly (instead of SW_SHOW then SW_MAXIMIZE) avoids a one-frame
    // flash of the restored-size window.
    ShowWindow (m_hwnd, m_startMaximized ? SW_SHOWMAXIMIZED : SW_SHOW);
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

    // Every mount reports its outcome through here, not just this one:
    // the recent-disks entry, the damage check, and the failure report all
    // hang off it. Installed before the command-line disks go in so those
    // are covered too.
    m_diskManager->SetMountCompletedCallback (
        [this] (int drive, const std::string & path, HRESULT mountResult,
                const MountDiagnosis & diagnosis)
        {
            OnMountCompleted (drive, path, mountResult, diagnosis);
        });

    m_diskManager->MountCommandLineDisks (disk1Path, disk2Path);

    ApplyPersistedAudioPrefs();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterChromeDock
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RegisterChromeDock()
{
    // Register the chrome bands + center with the dock layout once --
    // their thicknesses are refreshed from DPI + live drive-bar state
    // on every ComputeViewportRect / ClientSizeForCenterPx call.
    m_chromeDock.SetDock (m_titleBand,   DxuiDock::Top);
    m_chromeDock.SetDock (m_navBand,     DxuiDock::Top);
    m_chromeDock.SetDock (m_toolbarBand, DxuiDock::Top);
    m_chromeDock.SetDock (m_driveBand,   DxuiDock::Bottom);
    // Registered AFTER the drive band so the dock peels the drive bar off the
    // very bottom first and the //c switch strip lands just above it (between
    // the viewport and the joystick/paddle/mouse bar).
    m_chromeDock.SetDock (m_switchBand,  DxuiDock::Bottom);
    m_chromeDock.SetDock (m_centerBand,  DxuiDock::Fill);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitAssetPathsAndStores
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InitAssetPathsAndStores()
{
    fs::path  assetBaseDir = AssetBootstrap::GetAssetBaseDirectory();



    m_assetBaseDir    = assetBaseDir.wstring();
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
                                                   m_uiFs,
                                                   m_userWriteProtect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AllocateFramebuffers
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AllocateFramebuffers()
{
    size_t  framebufferSize = static_cast<size_t> (kFramebufferWidth) * kFramebufferHeight;



    // Create framebuffers (CPU renders to one, UI reads the other)
    m_cpuFramebuffer.resize (framebufferSize, 0);
    m_textOverlay.resize (framebufferSize, 0);
    m_uiFramebuffer.resize (framebufferSize, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrimeChromeThemeEarly
//
//  Primes the chrome-affecting theme state BEFORE creating the window so
//  the initial ClientSizeForCenter inside CreateEmulatorWindow reads the
//  right drive-bar thickness. Without this, a user whose persisted
//  activeTheme is compact (DarkModern or RetroTerminal) would get a window
//  sized for the full skeuomorphic strip on first paint, then immediately
//  shrink as soon as ThemeManager::Activate fires its listener later in
//  startup. UserConfigStore needs only assetBaseDir + the UI-thread
//  filesystem, both of which are already live here.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PrimeChromeThemeEarly()
{
    HRESULT       hr = S_OK;
    std::wstring  parseDetail;



    // A missing UserPrefs.json (first run) is reported by LoadAll as success
    // with defaults. A corrupt one is the user's file being unreadable, so we
    // tell them where it broke and reset to defaults so Casso still boots --
    // silently starting over just reads as "Casso lost my settings".
    //
    // Non-asserting on purpose. A malformed prefs file is bad DATA, not a
    // coding error: there is no bug for a developer to break into, and it
    // would stop the debugger every time someone hand-edits their JSON.
    // Assert or notify -- not both.
    //
    // CHRF rather than CHRN because this needs two actions, notify AND reset,
    // and the -N family is CHRF with its action fixed to one EhmNotifyUser
    // call. EhmNotifyUser rather than a themed dialog: this runs before the
    // chrome theme or main window exist, and it auto-detects GUI vs console.
    hr = m_userConfigStore->LoadAll (m_globalPrefs, m_uiFs, parseDetail);
    CHRF (hr,
          EhmNotifyUser ((L"Casso could not read your settings and has started "
                          L"with defaults. Your file was left untouched.\n\n" +
                          parseDetail).c_str());
          m_globalPrefs = GlobalUserPrefs {});

Error:
    m_chromeTheme = CassoTheme::ForName (m_globalPrefs.activeTheme);
    ApplyThemeToChrome (m_chromeTheme);
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildMachineDevices
//
//  Constructs one machine's device graph from its config, in wiring order:
//  memory, then the banking layers over it, then video, then the CPU that
//  reads through all of it.
//
//  WireApple2cRomBank must follow WireLanguageCard, not replace it. The
//  language card leaves a flat bank-0 split in place; the //c layer then adds
//  the $C028 bank-switch coordinator and the no-slots $Cxxx routing on top.
//  Skipping it on the initial-launch path (SwitchMachine already did it) is
//  what left a cold-booted //c with no ROM banking and no SetNoExternalSlots,
//  so $C800 floated and the firmware derailed into a garbage screen. It is a
//  no-op on machines with no banked ROM (romBankSize == 0).
//
//  The bus is validated between the memory devices and the CPU, so an
//  overlapping address range is reported as a config error at build time
//  rather than as mysterious reads once code is running.
//
//  The page table is wired last: it caches decisions made by every layer
//  above, so building it earlier would snapshot an incomplete map.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::BuildMachineDevices (const MachineConfig & config)
{
    HRESULT  hr = S_OK;



    hr = m_machineManager->CreateMemoryDevices (config);
    CHR (hr);

    m_machineManager->WireLanguageCard();
    // //c banked ROM: layer the $C028 bank-switch coordinator + no-slots
    // $Cxxx routing on top of the flat bank-0 split WireLanguageCard just
    // did. Without this the initial-launch //c has no ROM banking (and no
    // SetNoExternalSlots), so $C800 floats and the firmware derails to a
    // garbage screen. SwitchMachine already does this; the initial build
    // path must match it. No-op for non-banked machines (romBankSize == 0).
    m_machineManager->WireApple2cRomBank();
    m_machineManager->CreateVideoModes();

    // Validate memory bus for overlapping device address ranges
    hr = m_memoryBus.Validate();
    CHR (hr);

    hr = m_machineManager->CreateCpu (config);
    CHR (hr);

    m_machineManager->WirePageTable();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeRenderer
//
//  Points the framebuffer renderer at the host's D3D resources and hooks it
//  into the host's paint pump.
//
//  The renderer owns NO device, swap chain, or Present call -- the host owns
//  all three. This is what lets Dxui chrome paint on top of the emulator
//  image in one pass: the renderer composites the Apple ][ framebuffer into
//  the host's back buffer from the before-present hook, then the host paints
//  its chrome over it and presents once (DxuiHwndSource::PaintPump).
//
//  The hook reads m_pendingFramebuffer, which RunMessageLoop stages each UI
//  frame. A null value means "no new emulator frame", and the last upload is
//  re-composited -- that is the case that makes the render-skip gate cheap:
//  chrome can repaint over a static emulator image without re-uploading it.
//
//  The composite result is dropped deliberately. A per-frame present hook has
//  no return channel; a transient failure corrects itself on the next frame,
//  and a persistent one (device lost) is both visible on screen and handled by
//  the renderer's own device-reset path, not from inside the hook.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::InitializeRenderer()
{
    HRESULT  hr = S_OK;



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

    // Desk scene (spec 018): shares the host device with the framebuffer
    // renderer. Failure (broken embedded asset) asserts in debug and leaves
    // the 2D chrome paths active.
    {
        HRESULT  hrScene = InitializeDeskScene();

        IGNORE_RETURN_VALUE (hrScene, S_OK);
    }

    // Composite the Apple ][ framebuffer before the host paints chrome on
    // top (DxuiHwndSource::PaintPump). m_pendingFramebuffer is staged each
    // UI frame by RunMessageLoop; nullptr means "no new emulator frame"
    // (re-composite last upload). With the desk scene active, the CRT chain
    // renders to the offscreen scene target and the 3D scene samples it on
    // the monitor glass -- the theme backdrop the host cleared stays visible
    // around the devices. Otherwise the classic direct composite runs.
    m_host->SetBeforePresentHook ([this] ()
    {
        HRESULT  hrComposite = S_OK;



        if (CrtMonitorActive())
        {
            // The CRT chain renders the picture into an exact-aspect rect
            // anchored at the texture origin -- sized to the picture's
            // MEASURED on-screen height so the glass samples ~1:1 texels
            // (over-rendering minifies, and the linear filter then averages
            // away the outermost pixel columns where the sag compresses the
            // edges) -- and NOT positioned by the projected bounding box,
            // whose keystone slop would shear the texel alignment.
            RECT  glassPx     = m_d3dRenderer.GetTargetBounds();
            int   measuredH   = (int) lroundf (DeskSceneLayout::MeasurePictureHeightPx (
                                    m_deskScene.Composition(), m_deskScene.MonitorModel().Surface(),
                                    kFramebufferWidth, kFramebufferHeight));
            int   pictureH    = (measuredH > 0) ? measuredH : (int) (glassPx.bottom - glassPx.top);
            int   pictureW    = 0;
            RECT  pictureRect = {};

            pictureH = std::min (pictureH, m_d3dRenderer.GetBackBufferHeight());
            pictureW = MulDiv (pictureH, kFramebufferWidth, kFramebufferHeight);

            if (pictureW > m_d3dRenderer.GetBackBufferWidth())
            {
                pictureW = m_d3dRenderer.GetBackBufferWidth();
                pictureH = MulDiv (pictureW, kFramebufferHeight, kFramebufferWidth);
            }

            // Anchored at the texture origin: the picture's edges coincide
            // with the texture's, so the CRT chain's neighbor-sampling
            // passes clamp onto the picture itself at the borders -- the
            // same behavior as the classic direct path. (The picture mesh's
            // boundary is the band boundary, so nothing ever samples across
            // the picture's texture edge.)
            pictureRect = RECT{ 0, 0, pictureW, pictureH };

            hrComposite = m_d3dRenderer.UploadAndCompositeOffscreen (m_pendingFramebuffer, pictureRect);

            if (SUCCEEDED (hrComposite))
            {
                // The chain aspect-fits within pictureRect; recompute the same
                // fit so the sampled subrect matches it exactly. The texture
                // IS pictureRect now, so this is very nearly the whole of it
                // -- only a rounding row or column of letterbox survives.
                RECT                       fitted     = ComputeAspectFitRectInRect (pictureRect,
                                                            kFramebufferWidth, kFramebufferHeight);
                CrtUvRect                  uv         = ComputeUvRectForFit (fitted,
                                                            pictureW, pictureH);
                ID3D11ShaderResourceView * displaySrv = m_d3dRenderer.GetSceneContentSrv();

                // Calibration mode: swap in the stripe pattern so the glass
                // texel mapping can be verified end to end.
                if (m_deskSceneDebug >= 2)
                {
                    EnsureSceneCalibration (fitted);

                    if (m_sceneCalibSrv != nullptr)
                    {
                        displaySrv = m_sceneCalibSrv.Get();
                    }
                }

                hrComposite = m_deskScene.Render (m_host->GetBackBufferRtv(),
                                                  displaySrv, uv,
                                                  kFramebufferWidth, kFramebufferHeight);

                // Fullscreen drive overlay strip: the slid band composed by
                // TryPresentUiFrame's FSM tick, plus the hidden-state
                // activity glimmer in the corner (FR-015).
                if (m_d3dRenderer.IsFullscreen())
                {
                    int   bbW = m_d3dRenderer.GetBackBufferWidth();
                    int   bbH = m_d3dRenderer.GetBackBufferHeight();

                    if (m_stripRectPx.bottom > m_stripRectPx.top)
                    {
                        HRESULT  hrStrip = m_deskScene.RenderStrip (m_host->GetBackBufferRtv(), m_stripComp);

                        IGNORE_RETURN_VALUE (hrStrip, S_OK);
                    }

                    if (m_stripState.ActivityIndicator())
                    {
                        RECT  glimmer = { bbW - 34, bbH - 14, bbW - 12, bbH - 8 };

                        m_deskScene.DrawDebugRect (glimmer, bbW, bbH, 0xFFB01818);
                    }
                }

                // Layout diagnosis overlay: scene viewport red, projected
                // glass green, drive band yellow, switch band magenta.
                if (m_deskSceneDebug)
                {
                    int   bbW = m_d3dRenderer.GetBackBufferWidth();
                    int   bbH = m_d3dRenderer.GetBackBufferHeight();

                    m_deskScene.DrawDebugRect (m_deskScene.Composition().viewportPx, bbW, bbH, 0xFFFF3030);
                    m_deskScene.DrawDebugRect (m_deskScene.Composition().glassRectPx, bbW, bbH, 0xFF30FF30);

                    // The projected drive bounds ARE the drop-target rects the
                    // hit registry carries, so drawing them shows whether a
                    // refused drag is a bad rect or something upstream.
                    for (int i = 0; i < m_deskScene.Composition().driveCount; i++)
                    {
                        m_deskScene.DrawDebugRect (m_deskScene.Composition().driveRectPx[i], bbW, bbH, 0xFFFFA030);
                    }

                    m_deskScene.DrawDebugRect (m_driveBand.Bounds(), bbW, bbH, 0xFFFFFF30);
                    m_deskScene.DrawDebugRect (m_switchBand.Bounds(), bbW, bbH, 0xFFFF30FF);
                    m_deskScene.DrawDebugRect (m_stripRectPx, bbW, bbH, 0xFF30FFFF);
                }
            }
        }
        else
        {
            // Monitor off: the picture composites straight to the back buffer
            // as it always did. The 3D drives still render -- from the
            // after-paint hook below, since this composite writes the whole
            // back buffer and the opaque drive-band surface paints after it.
            hrComposite = m_d3dRenderer.UploadAndComposite (m_host->GetBackBufferRtv(),
                                                            m_pendingFramebuffer);
        }

        // Per-frame present hook with no return channel to propagate to.
        // A transient composite failure self-corrects next frame; a
        // persistent one (device lost) shows on screen and is handled by
        // the renderer's own device-reset path, not from here.
        IGNORE_RETURN_VALUE (hrComposite, S_OK);
    });

    // With the monitor opted out the drives are the only scene objects, and
    // they render HERE -- after the chrome painted -- because the classic
    // composite blacks out the whole back buffer and the drive band's opaque
    // surface would otherwise paint straight over them. The drive row keeps
    // its own depth pass, so it composes onto the finished frame.
    m_host->SetAfterPaintHook ([this] (ID3D11RenderTargetView * rtv, int bbW, int bbH)
    {
        HRESULT  hrDrives = S_OK;


        if (CrtMonitorActive() || !DeskSceneActive())
        {
            return;
        }

        if (m_d3dRenderer.IsFullscreen())
        {
            // Fullscreen without the monitor: the picture owns the client and
            // the drives live in the slide-up overlay strip, same as they do
            // with the monitor on.
            if (m_stripRectPx.bottom > m_stripRectPx.top)
            {
                hrDrives = m_deskScene.RenderStrip (rtv, m_stripComp);
            }

            if (m_stripState.ActivityIndicator())
            {
                RECT  glimmer = { bbW - 34, bbH - 14, bbW - 12, bbH - 8 };

                m_deskScene.DrawDebugRect (glimmer, bbW, bbH, 0xFFB01818);
            }
        }
        else
        {
            hrDrives = m_deskScene.RenderStrip (rtv, m_deskScene.Composition());
        }

        IGNORE_RETURN_VALUE (hrDrives, S_OK);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LoadDeskSceneModelsForMachine
//
//  The desk wears what the machine wore. The //c gets its platinum Monitor //c
//  over the matching 5.25 drives; everything else gets the beige Monitor II
//  over Disk IIs. Pairing across the families is what reads wrong -- a //c
//  monitor standing on Disk IIs is two eras of Apple industrial design in one
//  stack, in two different shades of case plastic.
//
//  Called again on a machine switch, so the stack changes with the machine.
//  Reloading rebuilds every cached mesh (glow discs, contact shadows, badge
//  stamps), which is why the scene's own state is re-pushed afterward.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplySavedBezelTilt
//
//  Restores the tilt this MONITOR was left at. Keyed by the monitor rather
//  than by the machine, because the tilt is a property of the thing standing
//  on the desk: put the same tube in front of another machine and it is still
//  angled the way it was left.
//
//  A monitor nobody has touched has no entry, which reads as square-on -- and
//  the setter clamps whatever it finds, so a file carrying a tilt from a
//  bezel with more travel cannot push this one through its frame.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplySavedBezelTilt()
{
    const MonitorSpec &  monitor = ResolveMonitorForCurrentMachine();
    auto                 found   = m_globalPrefs.monitorTilt.find (std::string (monitor.configName));
    float                radians = (found != m_globalPrefs.monitorTilt.end()) ? found->second : 0.0f;



    m_deskScene.SetBezelTilt (radians);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PersistBezelTilt
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PersistBezelTilt()
{
    const MonitorSpec &  monitor = ResolveMonitorForCurrentMachine();



    m_globalPrefs.monitorTilt[std::string (monitor.configName)] = m_deskScene.BezelTiltRad();

    if (m_userConfigStore != nullptr)
    {
        HRESULT  hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ResolveMonitorForCurrentMachine
//
////////////////////////////////////////////////////////////////////////////////

const MonitorSpec & EmulatorShell::ResolveMonitorForCurrentMachine()
{
    JsonValue          doc;
    const JsonValue *  uiPrefs = nullptr;



    // The merged document, not the shipped one: a machine's monitor is
    // configuration like everything else in there, so a user copy that names a
    // different monitor is answered the same way the machine's own does.
    LoadMachineUiPrefs (doc, uiPrefs);

    return MonitorCatalog::ForMachineJson (doc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadDeskSceneModelsForMachine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::LoadDeskSceneModelsForMachine()
{
    // WHICH MONITOR IS A PROPERTY OF THE MACHINE'S CONFIG, not a question
    // asked about its name. The drives still follow the machine, because a
    // //c's drives are part of the machine rather than of what it is plugged
    // into.
    HRESULT              hr         = S_OK;
    bool                 isC        = IsApple2c();
    const MonitorSpec &  monitor    = ResolveMonitorForCurrentMachine();
    std::string          monitorObj = PrinterPanel::LoadTextResource (monitor.objResourceId);
    std::string          monitorMtl = PrinterPanel::LoadTextResource (monitor.mtlResourceId);
    std::string          driveObj   = PrinterPanel::LoadTextResource (isC ? IDR_MODEL_DISK2C_OBJ : IDR_MODEL_DISKII_OBJ);
    std::string          driveMtl   = PrinterPanel::LoadTextResource (isC ? IDR_MODEL_DISK2C_MTL : IDR_MODEL_DISKII_MTL);
    bool                 haveText   = false;



    haveText = !monitorObj.empty() && !monitorMtl.empty() && !driveObj.empty() && !driveMtl.empty();
    CBRA (haveText);

    hr = m_deskScene.LoadModels (monitor.sceneKind,
                                 monitorObj, monitorMtl, driveObj, driveMtl);
    CHRA (hr);

    m_deskSceneMachineIsC = isC;

    // The monitor that just loaded brings its own tilt with it.
    ApplySavedBezelTilt();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InitializeDeskScene
//
//  Loads the model pair the ACTIVE MACHINE wore and stands the scene renderer
//  up on the host device. Missing or unparseable model text is a build defect
//  (the resources are compiled into the exe), so the guards assert; the shell
//  then simply leaves m_deskSceneReady false and the 2D chrome carries on.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::InitializeDeskScene()
{
    HRESULT   hr = S_OK;



    hr = m_deskScene.Initialize (m_host->GetDevice(), m_host->GetContext());
    CHRA (hr);

    hr = LoadDeskSceneModelsForMachine();
    CHRA (hr);

    // A powered monitor's lamp is lit for as long as the machine exists;
    // drive activity arrives per frame from the drive state sync.
    m_deskScene.SetPowerLampOn (true);

    ApplySceneAntiAliasing();

    {
        wchar_t   debugValue[8] = {};

        if (GetEnvironmentVariableW (L"CASSO_SCENE_DEBUG", debugValue, ARRAYSIZE (debugValue)) > 0)
        {
            m_deskSceneDebug = _wtoi (debugValue);
        }
    }

    m_deskSceneReady = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::EnsureSceneCalibration
//
//  Builds the CASSO_SCENE_DEBUG=2 stripe texture: back-buffer sized, with a
//  pattern in the fitted picture region expressed in FRAMEBUFFER columns --
//  red at emulated column 0, green at the last column, white every 8th, blue
//  rows top and bottom. What survives to the screen tells exactly how the
//  glass maps texels.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::EnsureSceneCalibration (const RECT & fittedRect)
{
    HRESULT                  hr     = S_OK;
    int                      bbW    = m_d3dRenderer.GetBackBufferWidth();
    int                      bbH    = m_d3dRenderer.GetBackBufferHeight();
    D3D11_TEXTURE2D_DESC     desc   = {};
    D3D11_SUBRESOURCE_DATA   init   = {};
    std::vector<uint32_t>    pixels;



    BAIL_OUT_IF (bbW <= 0 || bbH <= 0, S_OK);
    BAIL_OUT_IF (m_sceneCalibTex != nullptr && EqualRect (&m_sceneCalibRect, &fittedRect), S_OK);

    pixels.assign ((size_t) bbW * bbH, 0xFF000000);

    for (LONG y = fittedRect.top; y < fittedRect.bottom && y < bbH; y++)
    {
        for (LONG x = fittedRect.left; x < fittedRect.right && x < bbW; x++)
        {
            int        fbx   = MulDiv ((int) (x - fittedRect.left), kFramebufferWidth,
                                       (int) (fittedRect.right - fittedRect.left));
            int        fby   = MulDiv ((int) (y - fittedRect.top), kFramebufferHeight,
                                       (int) (fittedRect.bottom - fittedRect.top));
            uint32_t   color = 0xFF000000;

            if (fbx == 0)                              { color = 0xFFFF0000; }
            else if (fbx == kFramebufferWidth - 1)     { color = 0xFF00FF00; }
            else if (fby <= 1 || fby >= kFramebufferHeight - 2) { color = 0xFF4080FF; }
            else if ((fbx % 8) == 0)                   { color = 0xFFFFFFFF; }

            pixels[(size_t) y * bbW + x] = color;
        }
    }

    m_sceneCalibTex.Reset();
    m_sceneCalibSrv.Reset();

    desc.Width            = (UINT) bbW;
    desc.Height           = (UINT) bbH;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    init.pSysMem     = pixels.data();
    init.SysMemPitch = (UINT) bbW * 4;

    hr = m_host->GetDevice()->CreateTexture2D (&desc, &init, m_sceneCalibTex.GetAddressOf());
    CHR (hr);

    hr = m_host->GetDevice()->CreateShaderResourceView (m_sceneCalibTex.Get(), nullptr,
                                                        m_sceneCalibSrv.GetAddressOf());
    CHR (hr);

    m_sceneCalibRect = fittedRect;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DeskSceneHit
//
//  One frame of truth: resolves against the composition the scene last
//  rendered with, so hover, clicks, and pixels can never disagree with what
//  is on screen. With the monitor opted out the composition holds drives
//  alone, so the glass is excluded -- the flat picture is hit-tested by its
//  viewport rect on the classic paths, as it was before the scene existed.
//
////////////////////////////////////////////////////////////////////////////////

SceneHitResult EmulatorShell::DeskSceneHit (int xPx, int yPx) const
{
    float          tiltWorld[16]               = {};
    float          monLo[3]                    = {};
    float          monHi[3]                    = {};
    float          drvLo[3]                    = {};
    float          drvHi[3]                    = {};
    DeskRegionBox  doorBoxes[s_kSceneDriveMax] = {};



    m_deskScene.BuildTiltedMonitorWorld (m_deskScene.Composition(), tiltWorld);
    m_deskScene.MonitorModel().BoundsMin (monLo);
    m_deskScene.MonitorModel().BoundsMax (monHi);
    m_deskScene.DriveModel().BoundsMin (drvLo);
    m_deskScene.DriveModel().BoundsMax (drvHi);
    BuildDriveDoorBoxes (doorBoxes);

    return DeskSceneHitTester::Classify (m_deskScene.Composition(),
                                         m_deskScene.MonitorModel().Surface(),
                                         m_deskScene.DriveModel().RegionBoxes(),
                                         (float) xPx,
                                         (float) yPx,
                                         kFramebufferWidth,
                                         kFramebufferHeight,
                                         CrtMonitorActive(),
                                         &m_deskScene.MonitorModel().TiltGrips(),
                                         tiltWorld,
                                         monLo, monHi, drvLo, drvHi,
                                         doorBoxes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::BuildDriveDoorBoxes
//
//  THE DOOR IS THE ONE PART OF A DRIVE THAT MOVES, so its click target is
//  built per frame from where the door actually is rather than read off the
//  model's fixed region list. The //c's latch travels up clear of the lid
//  when it opens, which put the very part a user is reaching for outside
//  every box the case owns -- and the target was small to begin with, since
//  the slot band alone is about seven millimeters tall.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BuildDriveDoorBoxes (DeskRegionBox (& out)[s_kSceneDriveMax]) const
{
    for (int drive = 0; drive < s_kSceneDriveMax; drive++)
    {
        float  lo[3] = {};
        float  hi[3] = {};

        out[drive]        = DeskRegionBox {};
        out[drive].region = DriveWidgetRegion::Eject;

        if (!m_deskScene.DoorHitBox (drive, lo, hi))
        {
            continue;
        }

        memcpy (out[drive].boxMin, lo, sizeof (lo));
        memcpy (out[drive].boxMax, hi, sizeof (hi));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::StripHit
//
////////////////////////////////////////////////////////////////////////////////

SceneHitResult EmulatorShell::StripHit (int xPx, int yPx) const
{
    float          drvLo[3]                     = {};
    float          drvHi[3]                     = {};
    DeskRegionBox  doorBoxes[s_kSceneDriveMax]  = {};



    m_deskScene.DriveModel().BoundsMin (drvLo);
    m_deskScene.DriveModel().BoundsMax (drvHi);
    BuildDriveDoorBoxes (doorBoxes);

    return DeskSceneHitTester::Classify (m_stripComp,
                                         m_deskScene.MonitorModel().Surface(),
                                         m_deskScene.DriveModel().RegionBoxes(),
                                         (float) xPx,
                                         (float) yPx,
                                         kFramebufferWidth,
                                         kFramebufferHeight,
                                         false,
                                         nullptr, nullptr, nullptr, nullptr,
                                         drvLo, drvHi,
                                         doorBoxes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DeskSceneDriveCount
//
//  The same gates the 2D widgets use: no Disk II controller means no drives
//  at all; a //c with the external drive disconnected shows only the
//  internal one.
//
//  The controller check stays first and stays separate from the config. A
//  machine can declare drive ports it cannot use -- the //c builds its IWM in
//  code from a banked-ROM test rather than from a slot -- so "is there a
//  controller" and "what is attached to it" are two questions, and answering
//  the second alone would put drives on a machine that has nowhere to run
//  them.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InvalidateSceneComposition
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InvalidateSceneComposition()
{
    RECT  client = {};



    if (m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        return;
    }

    UpdateViewportLayout (client.right - client.left, client.bottom - client.top);
    m_d3dRenderer.MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ClampSceneView
//
//  Keeps the framing somewhere a user can get back from.
//
//  Pan is bounded by how much slack the zoom actually created: at 2x the
//  scene is twice the viewport, so one viewport-width of offset is exactly
//  enough to reach any edge and no more. At 1x there is no slack, so the pan
//  is dropped outright -- the fitted composition already fits, and an offset
//  there can only move it somewhere worse.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ClampSceneView()
{
    float  slack = 0.0f;



    m_sceneView.zoom = std::clamp (m_sceneView.zoom, s_kSceneZoomMin, s_kSceneZoomMax);

    slack = std::max (0.0f, m_sceneView.zoom - 1.0f);

    m_sceneView.panX = std::clamp (m_sceneView.panX, -slack, slack);
    m_sceneView.panY = std::clamp (m_sceneView.panY, -slack, slack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ZoomSceneAt
//
//  Zooms about a client point rather than about the viewport center, so what
//  is under the cursor stays under it. Center-anchored zoom would push the
//  part being inspected toward an edge exactly as it grew big enough to see.
//
//  The pan solve falls out of the same mapping the projection uses:
//
//      ndc = zoom * u + pan          (u == the un-framed NDC of a point)
//
//  Holding the cursor's u fixed across a zoom by k gives
//
//      pan' = c - k * (c - pan)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ZoomSceneAt (POINT clientPt, float factor)
{
    // The box the composition was SOLVED into -- not the client rect and not
    // the glass rect. NDC is defined against this one, so anchoring the zoom
    // to anything else would drift the cursor off its target the further the
    // chrome pushed the scene around.
    RECT   box    = m_deskScene.Composition().viewportPx;
    float  width  = (float) (box.right - box.left);
    float  height = (float) (box.bottom - box.top);
    float  cx     = 0.0f;
    float  cy     = 0.0f;
    float  before = m_sceneView.zoom;
    float  k      = 1.0f;



    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    // Client point -> NDC. Y flips: client grows downward, NDC upward.
    cx = ((float) (clientPt.x - box.left) / width)  * 2.0f - 1.0f;
    cy = 1.0f - ((float) (clientPt.y - box.top) / height) * 2.0f;

    m_sceneView.zoom = std::clamp (before * factor, s_kSceneZoomMin, s_kSceneZoomMax);

    // The REALIZED ratio, not the requested one -- at a clamp the two differ,
    // and anchoring on the request would slide the scene under a cursor that
    // is no longer zooming.
    k = (before > 0.0f) ? (m_sceneView.zoom / before) : 1.0f;

    m_sceneView.panX = cx - k * (cx - m_sceneView.panX);
    m_sceneView.panY = cy - k * (cy - m_sceneView.panY);

    ClampSceneView();
    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ResetSceneView
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ResetSceneView()
{
    if (m_sceneView.IsIdentity())
    {
        return;
    }

    m_sceneView = DeskSceneView {};
    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DeskSceneDriveCount
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::DeskSceneDriveCount() const
{
    bool  hasDisk = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();



    if (!hasDisk)
    {
        return 0;
    }

    // The //c's drives are not carded -- one is soldered in and the second
    // hangs off the back-panel disk port -- so its slot list says nothing
    // about them and the internal drive is always there.
    if (m_config.slots.empty())
    {
        return ShouldShowExternalDrive() ? 2 : 1;
    }

    // A card with every port empty reports zero, which is the point of being
    // able to detach a drive at all.
    return m_config.AttachedDiskIiDriveCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetChromeHiddenForFullscreenScene
//
//  Visibility, not bounds: the adopted chrome controls paint from their own
//  cached layouts, so an empty rect is not a reliable hidden state --
//  SetVisible is. The host caption gets its explicit switch. Symmetric: the
//  windowed layout path calls this with `hidden = false` every pass, so
//  leaving fullscreen restores everything.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetChromeHiddenForFullscreenScene (bool hidden)
{
    // A borderless-fullscreen window fills the monitor and has nothing to
    // resize TO: its edges ARE the screen's, and leaving the resize borders
    // armed lets a drag at a corner pull the picture down to a fraction of
    // the screen with no caption left to put it right with.
    m_host->SetResizable (!hidden);

    m_host->SetCaptionVisible (!hidden);
    m_mainMenu.SetVisible (!hidden);
    // The toolbar comes back on its own in fullscreen, summoned by the top
    // edge -- so hiding the chrome parks it and TickFullscreenToolbar owns
    // it from there.
    m_toolbar.SetVisible (!hidden);

    if (hidden)
    {
        m_fsToolbarShown  = false;
        m_fsToolbarLeftMs = 0;
    }

    m_joystickButton.SetVisible (!hidden);
    // The band surface only exists for the 2D chrome; under the desk scene
    // the drives paint from the scene and the band would read as a leftover
    // bar along the window's bottom edge.
    m_driveBandSurface.SetVisible (!hidden && !DeskSceneActive());
    m_switchBar.SetVisible (!hidden);

    // Leaving fullscreen must not hand the flat widgets back to a scene that
    // has already retired them.
    m_driveChrome[0].SetVisible (!hidden && !DeskSceneActive());
    m_driveChrome[1].SetVisible (!hidden && !DeskSceneActive());
    m_sceneDriveLabel[0].SetVisible (!hidden);
    m_sceneDriveLabel[1].SetVisible (!hidden);

    if (hidden)
    {
        m_driveChrome[0].Hide();
        m_driveChrome[1].Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncCaptureBanner
//
//  A persistent way OUT, for as long as the pointer is held.
//
//  Paddle mode takes the mouse: the cursor is hidden and clipped to the
//  window, and the only release is a key the user has to already know. The
//  joystick button says so -- and fullscreen hides the joystick button, which
//  leaves a captured pointer, no cursor, and nothing on screen to read. This
//  says it over the picture instead, in both presentations, and goes away the
//  moment the capture does.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncCaptureBanner()
{
    RECT  client = {};
    RECT  rc     = {};



    if (!m_paddleCaptured || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        m_captureBanner.SetVisible (false);
        return;
    }

    // ABOVE THE BOTTOM CHROME, not on it. Hung from the client's own bottom
    // edge the notice straddled the switch bar, half over the scene and half
    // over a shell band, reading as neither. Measured off the switch band
    // rather than the drive band: under the desk scene the drive band is
    // empty -- the scene owns the drives -- so it reports nothing to sit
    // above. The picture is not available either; the CRT pass paints over
    // this chrome.
    {
        RECT  bar    = m_switchBand.Bounds();
        LONG  bottom = (!m_d3dRenderer.IsFullscreen() && bar.bottom > bar.top)
                     ? bar.top : client.bottom;

        rc.left   = client.left;
        rc.right  = client.right;
        rc.bottom = bottom - m_scaler.Px (s_kCaptureBannerInsetDp);
        rc.top    = rc.bottom - m_scaler.Px (s_kCaptureBannerHeightDp);
    }

    m_captureBanner.SetText        (s_kCaptureBanner);
    m_captureBanner.SetFontSizeDip (s_kCaptureBannerFontDip);
    m_captureBanner.SetDpi         (m_scaler.Dpi());
    m_captureBanner.Layout         (rc, m_scaler);
    m_captureBanner.SetVisible     (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MirroredSlideStart
//
//  A slide start time that PRESERVES the current position when the direction
//  reverses: a band caught half-way out and sent back should leave from where
//  it is, not jump to the far end and crawl. The elapsed time is mirrored
//  about the animation length, which is the same trick the drive strip's FSM
//  plays on itself.
//
////////////////////////////////////////////////////////////////////////////////

static int64_t MirroredSlideStart (int64_t nowMs, int64_t animStartMs)
{
    int64_t  elapsed = nowMs - animStartMs;



    if (elapsed >= FullscreenStripState::kSlideMs)
    {
        return nowMs;
    }

    return nowMs - (FullscreenStripState::kSlideMs - elapsed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TickFullscreenToolbar
//
//  The command toolbar on the same bargain the drive strip has at the bottom:
//  the pointer at the top edge slides it down, leaving slides it away.
//
//  Laid out here rather than by the chrome dock, because in fullscreen there
//  are no bands -- the scene owns the whole client, and the toolbar is an
//  overlay across its top rather than a strip the viewport makes room for.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::TickFullscreenToolbar()
{
    RECT     client = {};
    POINT    cursor = {};
    int      bandH  = 0;
    bool     want   = false;
    int64_t  nowMs  = 0;



    if (!m_d3dRenderer.IsFullscreen() || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        if (m_fsToolbarShown)
        {
            m_fsToolbarShown = false;
            m_toolbar.SetVisible (false);
        }

        return;
    }

    nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                std::chrono::steady_clock::now().time_since_epoch()).count();

    m_toolbar.PlanForWidth (client.right - client.left, m_scaler);
    bandH = m_scaler.Px (m_toolbar.BandDp());

    if (GetCursorPos (&cursor) && ScreenToClient (m_hwnd, &cursor) && PtInRect (&client, cursor))
    {
        // The edge zone summons; the whole band holds it open, so the
        // pointer can travel down onto the buttons without dismissing them.
        want = m_fsToolbarShown ? (cursor.y <= bandH)
                                : (cursor.y <= m_scaler.Px (s_kStripEdgeZoneDp));
    }

    // A menu opened from the toolbar keeps it up regardless of where the
    // pointer wandered to reach the menu's items.
    want = want || m_mainMenu.IsOpen();

    if (want)
    {
        m_fsToolbarLeftMs = 0;
    }
    else if (m_fsToolbarShown && m_fsToolbarLeftMs == 0)
    {
        m_fsToolbarLeftMs = nowMs;
    }

    if (!want && m_fsToolbarShown &&
        nowMs - m_fsToolbarLeftMs >= FullscreenStripState::kAutoHideGraceMs)
    {
        m_fsToolbarShown  = false;
        m_fsToolbarAnimMs = MirroredSlideStart (nowMs, m_fsToolbarAnimMs);
        m_d3dRenderer.MarkRedrawNeeded();
    }
    else if (want && !m_fsToolbarShown)
    {
        m_fsToolbarShown  = true;
        m_fsToolbarAnimMs = MirroredSlideStart (nowMs, m_fsToolbarAnimMs);
        m_d3dRenderer.MarkRedrawNeeded();
    }

    // The band SLIDES: it hangs off the top by the part of itself that has
    // not arrived, so it enters and leaves the way the drive strip does
    // rather than blinking into place. Reversing mid-slide keeps the current
    // position (see MirroredSlideStart) instead of snapping to the far end.
    {
        float  t        = std::clamp ((float) (nowMs - m_fsToolbarAnimMs) /
                                      (float) FullscreenStripState::kSlideMs, 0.0f, 1.0f);
        float  progress = m_fsToolbarShown ? t : 1.0f - t;
        int    top      = client.top - (int) ((1.0f - progress) * (float) bandH);

        if (progress <= 0.0f)
        {
            m_toolbar.SetVisible (false);
            return;
        }

        m_toolbar.Layout     (RECT{ client.left, top, client.right, top + bandH }, m_scaler);
        m_toolbar.SetVisible (true);

        if (t < 1.0f)
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LayoutSceneCompass
//
//  The compass sits in the scene viewport's BOTTOM-RIGHT corner, inset far
//  enough that it reads as furniture of the window rather than part of the
//  machines. Hidden wherever the scene is not the thing on screen --
//  fullscreen shows the picture, the 2D paths have no scene to turn.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutSceneCompass()
{
    RECT   vp       = m_deskScene.Composition().viewportPx;
    LONG   sidePx   = m_scaler.Px (72);
    LONG   marginPx = m_scaler.Px (10);
    bool   show     = DeskSceneActive() && !m_d3dRenderer.IsFullscreen() &&
                      (vp.right - vp.left) > sidePx * 3;
    RECT   rc       = {};



    if (!show)
    {
        m_sceneCompass.SetVisible (false);
        return;
    }

    rc.right  = vp.right  - marginPx;
    rc.bottom = vp.bottom - marginPx;
    rc.left   = rc.right  - sidePx;
    rc.top    = rc.bottom - sidePx;

    m_sceneCompass.SetDpi     (m_scaler.Dpi());
    m_sceneCompass.SetRect    (rc);
    m_sceneCompass.SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneDriveChrome
//
//  The scene owns the drives: the 2D widgets hide (still syncing state for
//  the //c switch strip and the door FSM), the drag-drop hit registry is
//  rebuilt from the composition's projected drive bounds so dropping a disk
//  image on a 3D drive keeps mounting into that drive, and each drive's
//  basename label is re-hung under those same bounds.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneDriveChrome()
{
    const DeskSceneComposition &  comp = m_deskScene.Composition();



    SyncSceneDriveLabels();
    LayoutSceneCompass();

    // INVISIBLE, not merely collapsed. Hide() only empties the bounds, and a
    // visible panel with empty bounds is one stray Layout away from painting:
    // a resize arranges the docked bands before this runs, so the retired 2D
    // widgets flashed along the bottom edge for a frame under the 3D drives.
    m_driveChrome[0].SetVisible (false);
    m_driveChrome[1].SetVisible (false);
    m_driveChrome[0].Hide();
    m_driveChrome[1].Hide();

    m_uiShell.HitTest().Clear();

    for (int i = 0; i < comp.driveCount; i++)
    {
        if (comp.driveRectPx[i].right > comp.driveRectPx[i].left)
        {
            m_uiShell.HitTest().Register (DxuiHitRect { comp.driveRectPx[i], DxuiHitSlot::Custom, i });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneDriveLabels
//
//  Hangs the mounted image's basename in a strip under each 3D drive, where
//  the 2D widget's label sat -- the name belongs on screen, not buried in a
//  hover tooltip. The strip spans the drive's projected width, so the layout
//  reserves its height above (both scene branches shrink the rect they
//  compose into by exactly that much) and the drives never sit on it.
//
//  Ellipsized through the shared pure truncation helper against the real text
//  measurement, so a long name ends in a single ellipsis instead of wrapping
//  out of the strip. The tooltip still carries the full name.
//
//  Fullscreen shows no labels: the picture owns the client and the drives are
//  only briefly on screen in the overlay strip, which has its own tooltip.
//
//  Windowed, the labels ride the ORBIT: they re-hang under each drive's
//  projected bounds on every composition pass, so they stay legible from
//  whatever angle the inspection orbit is showing rather than vanishing the
//  moment the camera moves.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneDriveLabels()
{
    // FULLSCREEN LABELS THE STRIP'S DRIVES, not the desk's: the overlay is
    // the only place drives appear there, and a drive worth revealing is
    // worth naming. Its composition is the strip's, and the labels come and
    // go with the slide.
    bool                          fs      = m_d3dRenderer.IsFullscreen();
    bool                          onStrip = fs && m_stripRectPx.bottom > m_stripRectPx.top &&
                                            m_stripComp.driveCount > 0;
    const DeskSceneComposition &  comp    = onStrip ? m_stripComp : m_deskScene.Composition();
    IDxuiTextRenderer *           text    = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    float                         fontPx  = s_kSceneDriveLabelFontDip * (float) m_scaler.Dpi() / (float) s_kBaseDpi;
    bool                          visible = !fs || onStrip;



    for (int i = 0; i < (int) m_sceneDriveLabel.size(); i++)
    {
        std::wstring  basename;
        RECT          strip = {};

        if (visible && i < comp.driveCount && comp.driveRectPx[i].right > comp.driveRectPx[i].left)
        {
            basename = std::filesystem::path (m_diskStore.GetSourcePath (6, i)).filename().wstring();

            // THE PADLOCK RIDES THE NAME, not the drive. It had been a brass
            // badge stamped on the faceplate -- on a case whose whole job is
            // to look like 1983 hardware, and no Disk II ever wore one. Here
            // it is what it actually is: a fact about the MOUNTED IMAGE,
            // sitting beside that image's name.
            //
            // Ahead of the truncation on purpose. Truncation eats the TAIL,
            // so a badge at the head survives however long the name is, and
            // nothing downstream has to keep it out of the ellipsis by hand.
            if (!basename.empty() && m_driveWidgetState[i].writeProtect.Any())
            {
                basename = std::wstring (s_kpszLock) + L" " + basename;
            }
        }

        m_sceneDriveLabelRect[i] = RECT{};

        if (basename.empty())
        {
            m_sceneDriveLabel[i].SetText    (L"");
            m_sceneDriveLabel[i].SetVisible (false);
            continue;
        }

        // Hung from the drive's projected FRONT-BOTTOM CENTER -- one fixed
        // model point -- at a constant width, so the label rides the drive
        // rigidly through the orbit instead of tracking a bounding box that
        // swells and swings as the case turns.
        {
            int  halfW = m_scaler.Px (s_kSceneDriveLabelWidthDp) / 2;

            strip.left   = comp.driveLabelPx[i].x - halfW;
            strip.right  = comp.driveLabelPx[i].x + halfW;
            strip.top    = comp.driveLabelPx[i].y + m_scaler.Px (s_kSceneDriveLabelGapDp);
            strip.bottom = strip.top + m_scaler.Px (s_kSceneDriveLabelStripDp);
        }

        // A strip that does not fit inside the client is not a label, it is a
        // stray string: the rect comes from PROJECTING the drive through the
        // frame's camera, and a layout caught mid-resize can hand back a
        // projection off the top of the window. The label would then paint its
        // lower half along the window's top edge, above the caption text,
        // flickering for as long as that pass was on screen.
        {
            RECT  client = {};

            if (m_hwnd == nullptr || !GetClientRect (m_hwnd, &client) ||
                strip.top < client.top || strip.bottom > client.bottom ||
                strip.right <= strip.left)
            {
                m_sceneDriveLabel[i].SetText    (L"");
                m_sceneDriveLabel[i].SetVisible (false);
                continue;
            }
        }

        if (text != nullptr)
        {
            basename = TruncateToWidth (basename, (float) (strip.right - strip.left),
                                        [text, fontPx] (std::wstring_view run) -> float
            {
                float    w  = 0.0f;
                float    h  = 0.0f;
                HRESULT  hr = text->MeasureString (std::wstring (run).c_str(), fontPx,
                                                   DxuiTheme::kBodyFace, w, h);

                return SUCCEEDED (hr) ? w : 0.0f;
            });
        }

        m_sceneDriveLabel[i].SetText        (basename);
        m_sceneDriveLabel[i].SetColor       (m_chromeTheme.driveLabel);
        m_sceneDriveLabel[i].SetFontSizeDip (s_kSceneDriveLabelFontDip);
        m_sceneDriveLabel[i].SetTextAlign   (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        m_sceneDriveLabel[i].SetDpi         (m_scaler.Dpi());
        m_sceneDriveLabel[i].SetRect        (strip);
        m_sceneDriveLabel[i].SetVisible     (true);

        // Remembered so the hover can tell the strip from the case: over the
        // name and its badge the tooltip explains the protection, over the
        // drive it just names the disk.
        m_sceneDriveLabelRect[i] = strip;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineUiPrefs
//
//  Reads the active machine's JSON config and merges the user overrides via
//  UserConfigStore, handing back the "$cassoUiPrefs" object in outUiPrefs.
//  Any problem collapses to outUiPrefs == nullptr so the caller keeps the
//  built-in defaults, and these cosmetic per-machine prefs never block
//  startup -- though corrupt content (as opposed to a simply-absent file or
//  key) asserts first so a debug build catches it. The returned pointer
//  aliases into outDoc, so outDoc must outlive every use of it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LoadMachineUiPrefs (
    JsonValue         & outDoc,
    const JsonValue * & outUiPrefs)
{
    HRESULT            hr                = S_OK;
    std::string        machineNameNarrow = CurrentMachineNameNarrow();
    JsonValue          defaultJson;
    JsonParseError     parseErr;
    std::ifstream      configFile;
    std::stringstream  ss;
    std::string        jsonText;
    std::wstring       configRelPath     = std::wstring (L"Machines\\") + m_currentMachineName +
                                           L"\\" + m_currentMachineName + L".json";
    fs::path           configPath        = PathResolver::FindFile (PathResolver::BuildSearchPaths (
                                               PathResolver::GetExecutableDirectory(),
                                               PathResolver::GetWorkingDirectory()),
                                               configRelPath);



    outUiPrefs = nullptr;

    // A missing file, or a missing "$cassoUiPrefs" key, is normal (first run
    // for this machine): recover to null so the caller keeps defaults, no
    // assert. Content that exists but is corrupt -- unparseable JSON or a
    // failed override merge -- means something is wrong, so CHRA asserts (a
    // debug build breaks for a dev to dig in) and then bails to that same
    // null-prefs recovery.
    BAIL_OUT_IF (configPath.empty(), S_OK);
    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), S_OK);

    ss << configFile.rdbuf();
    jsonText = ss.str();

    hr = JsonParser::Parse (jsonText, defaultJson, parseErr);
    CHRA (hr);

    hr = m_userConfigStore->Load (machineNameNarrow, defaultJson, m_uiFs, outDoc);
    CHRA (hr);

    BAIL_OUT_IF (outDoc.GetType() != JsonType::Object, S_OK);

    hr = outDoc.GetObject ("$cassoUiPrefs", outUiPrefs);
    if (FAILED (hr))
    {
        outUiPrefs = nullptr;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeUiShell
//
//  Native UI runtime bootstrap. UiShell owns the painter, text renderer,
//  hit-tester, focus manager, and input translator; the host panel-tree
//  pump composites the chrome on top of the emulator frame. Infrastructure
//  bring-up that genuinely fails -- D2D/text, theme enumeration -- aborts
//  startup through CHR (Main surfaces it); corrupt user prefs recover to
//  defaults rather than abort.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::InitializeUiShell()
{
    HRESULT  hr = S_OK;



    hr = m_uiShell.Initialize (&m_d3dRenderer);
    CHR (hr);

    hr = WireUiShellChromeAndThemes();
    CHR (hr);

    RestoreInputAndColorPrefs();
    RecordActiveMachineSelection();
    SubscribeAndActivateTheme();
    ApplyPersistedChromePrefs();

    hr = FinishUiShellLayout();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireUiShellChromeAndThemes
//
//  Connects the chrome controls to UiShell's shared services and brings up
//  the theme manager.
//
//  UiShell no longer paints the caption or the chrome -- the host panel tree
//  does. What it still owns is input routing, hit-testing, and the theme /
//  viewport metrics the settings panel reads, which is why only those are
//  wired here.
//
//  The shared text renderer is INJECTED into each control rather than passed
//  to Layout. Chrome controls have to measure their own label strings while
//  laying out, so handing them the renderer once lets them satisfy the plain
//  IDxuiControl::Layout contract instead of forcing a renderer parameter onto
//  every Layout call in the framework.
//
//  Global prefs are deliberately NOT re-loaded here; PrimeChromeThemeEarly
//  already did that, and a second LoadAll would just re-read the same file.
//  Discover treats an empty or missing themes directory as success -- the
//  built-in themes work without any on-disk ones -- so only a real
//  enumeration failure propagates and blocks startup.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::WireUiShellChromeAndThemes()
{
    HRESULT   hr        = S_OK;
    fs::path  themesDir = fs::path (m_assetBaseDir) / fs::path ("Themes");



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
    m_switchBar.SetTextRenderer          (&m_uiShell.Text());
    m_toolbar.SetTextRenderer            (&m_uiShell.Text());

    // Global prefs are already loaded by PrimeChromeThemeEarly, so there is
    // no second LoadAll here. Discover scans the themes directory (an empty
    // or absent one returns S_OK -- the built-in themes still work), so only
    // a genuine enumeration failure propagates.
    m_themeManager = std::make_unique<ThemeManager> (m_uiFs, themesDir.wstring());
    hr             = m_themeManager->Discover();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreInputAndColorPrefs
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RestoreInputAndColorPrefs()
{
    // Restore the split input mappings. Keys (arrows->
    // joystick) is a passive remap, safe to restore. Pointer: Paddle is
    // an active mouse-capture mode, never restored on launch (it would
    // light the LED while the mouse is NOT captured) -- falls back to
    // Off; Mouse is non-capturing and restores safely.
    m_arrowsJoystick = m_globalPrefs.arrowsToJoystick;
    m_pointerMode    = (m_globalPrefs.pointerMapping == InputMappingMode::Paddle)
                           ? InputMappingMode::Off
                           : m_globalPrefs.pointerMapping;
    m_globalPrefs.pointerMapping   = m_pointerMode;
    m_globalPrefs.inputMappingMode = DisplayInputMode();
    SyncSelectorState();

    SetColorMonitorTextArgbLive (
        ColorUtil::ResolveColorMonitorTextArgb (m_globalPrefs.colorMonitorTextMode,
                                                m_globalPrefs.colorMonitorTextCustomArgb));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordActiveMachineSelection
//
//  Records the currently-active machine so the next launch boots it by
//  default (Main resolves the value via this same GlobalUserPrefs field).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordActiveMachineSelection()
{
    std::string  narrow = CurrentMachineNameNarrow();



    if (m_globalPrefs.lastSelectedMachine != narrow)
    {
        m_globalPrefs.lastSelectedMachine = narrow;
        SaveGlobalPrefs();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubscribeAndActivateTheme
//
//  Two orderings here, both load-bearing.
//
//  The change listener is subscribed BEFORE the first Activate, so that
//  initial activation fires it and primes m_chromeTheme from the persisted
//  user choice. Subscribing afterwards leaves the chrome painting its
//  constructed Skeuomorphic default until the user happens to re-pick their
//  theme in Settings -- a bug that looks like the preference was not saved.
//
//  The active machine name is set before Activate for the same reason: themes
//  resolve per machine variant, so the listener notification would otherwise
//  carry a theme resolved against the wrong machine.
//
//  A failed Activate means the persisted theme name no longer exists -- it was
//  renamed, deleted, or is a stale default -- so it falls back to the canonical
//  built-in. If even that is missing from the discovered set, the chrome keeps
//  its constructed default and there is genuinely nothing to act on, which is
//  why the fallback's result is explicitly discarded rather than propagated
//  (this function returns void by design; a missing theme is not a startup
//  failure).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SubscribeAndActivateTheme()
{
    HRESULT  hrActivate = S_OK;



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

    hrActivate = m_themeManager->Activate (m_globalPrefs.activeTheme);
    if (FAILED (hrActivate))
    {
        // The persisted theme name is unknown -- renamed, deleted, or a stale
        // default. Fall back to the canonical built-in. If even that is not in
        // the discovered set, chrome keeps its constructed Skeuomorphic
        // default, so a failed fallback is genuinely nothing to act on. This
        // function returns void, hence the explicit discard rather than CHR.
        hrActivate = m_themeManager->Activate ("Skeuomorphic");
        IGNORE_RETURN_VALUE (hrActivate, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPersistedChromePrefs
//
//  Applies the persisted per-machine colorMode + speed mode + //c peripheral
//  state at boot. Without this the emulator defaults to Color / Authentic
//  regardless of what the user last saved (MachineManager::SwitchMachine
//  carries the apply logic but only fires on user-initiated switches, not the
//  boot path).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyPersistedChromePrefs()
{
    HRESULT            hr           = S_OK;
    HRESULT            hrOpt        = S_OK;
    JsonValue          doc;
    const JsonValue *  uiPrefs      = nullptr;
    const JsonValue *  wpArr        = nullptr;
    std::string        colorMode;
    std::string        speedMode;
    bool               extConnected = false;
    bool               mouseConn    = true;



    LoadMachineUiPrefs (doc, uiPrefs);
    BAIL_OUT_IF (uiPrefs == nullptr, S_OK);

    // Each key below is optional: a fresh machine omits it, which the
    // getter reports as a failing HRESULT while leaving the target
    // untouched. We probe with hrOpt and apply only on success, so a
    // missing key keeps the built-in default -- a genuine corrupt-file
    // error already propagated out of LoadMachineUiPrefs above.
    // NO SAVED COLOR MEANS THE MONITOR'S OWN. The machine names the monitor
    // it ships with and the monitor owns its phosphor, so an untouched //c
    // comes up green because a Monitor //c is green -- not because anything
    // wrote "green" into a preference file for it.
    {
        int  modeIdx = MonitorCatalog::PhosphorSettingsIndex (
                           MonitorCatalog::ForMachineJson (doc));

        hrOpt = uiPrefs->GetString ("colorMode", colorMode);

        if (SUCCEEDED (hrOpt))
        {
            if      (colorMode == "color") { modeIdx = 0; }
            else if (colorMode == "green") { modeIdx = 1; }
            else if (colorMode == "amber") { modeIdx = 2; }
            else if (colorMode == "white") { modeIdx = 3; }
        }

        SetColorModeLive (modeIdx);
    }

    // Speed mode (authentic / double / maximum) lives in the same UI prefs and,
    // like colorMode, must be pushed into CpuManager at boot. SwitchMachine
    // applies it for runtime machine switches, but the cold-boot path otherwise
    // leaves the CPU at its Authentic default while Settings shows the saved
    // value -- so a saved "maximum" never actually runs fast until re-picked.
    hrOpt = uiPrefs->GetString ("speedMode", speedMode);
    if (SUCCEEDED (hrOpt))
    {
        if      (speedMode == "authentic") { m_cpuManager.SetSpeedMode (SpeedMode::Authentic); }
        else if (speedMode == "double")    { m_cpuManager.SetSpeedMode (SpeedMode::Double);    }
        else if (speedMode == "maximum")   { m_cpuManager.SetSpeedMode (SpeedMode::Maximum);   }
    }

    // //c external drive + mouse: seed the connected states HERE -- before
    // FinishUiShellLayout gates on ShouldShowExternalDrive() -- so the first
    // paint matches the saved setup. External drive defaults not-connected;
    // mouse defaults CONNECTED.
    //
    // The drive's answer is the back-panel disk port. The legacy
    // externalDriveConnected boolean is still read as a FALLBACK because the
    // fold that retires it only runs on a version bump -- a config already at
    // the current stamp keeps its old key until Settings next saves, and
    // dropping the drive for that one launch would be a visible regression.
    {
        const PortConfig *  diskPort = m_config.FindPort ("disk");

        if (diskPort != nullptr)
        {
            m_externalDriveConnected = !diskPort->device.empty();
        }
        else
        {
            hrOpt = uiPrefs->GetBool ("externalDriveConnected", extConnected);
            if (SUCCEEDED (hrOpt))
            {
                m_externalDriveConnected = extConnected;
            }
        }
    }

    hrOpt = uiPrefs->GetBool ("mouseConnected", mouseConn);
    if (SUCCEEDED (hrOpt))
    {
        m_mouseConnected = mouseConn;
    }

    // Seed the per-drive user write-protect preference BEFORE the
    // command-line mount so the very first mount already re-asserts it
    // onto the image.
    hrOpt = uiPrefs->GetArray ("writeProtect", wpArr);
    if (SUCCEEDED (hrOpt) && wpArr != nullptr)
    {
        for (size_t wi = 0; wi < wpArr->ArraySize() && wi < m_userWriteProtect.size(); ++wi)
        {
            const JsonValue &  entry = wpArr->ArrayAt (wi);

            if (entry.GetType() == JsonType::Bool)
            {
                m_userWriteProtect[wi] = entry.GetBool();
            }
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishUiShellLayout
//
//  Settles the chrome for the machine that was just built, before the window
//  is shown. This is the boot-time counterpart to what OnSize does on every
//  later resize.
//
//  The live monitor DPI is pushed into UiShell here so the FIRST D2D
//  BindBackBuffer uses it. Skipping this leaves the initial bind at the m_dpi
//  default of 0 (treated as 96), and chrome text paints tiny on a high-DPI
//  display until the user happens to resize the window and trigger a real
//  layout.
//
//  Drive widgets are collapsed rather than skipped when there is nothing to
//  show -- a stripped Apple II config with no Slot 6 controller hides both, a
//  //c with no external drive connected hides only the second. The joystick
//  button still paints in every case, since joystick input does not depend on
//  disk presence.
//
//  Hit-test rects are registered from the widgets' final geometry, and only
//  for widgets that are actually visible, so a hidden drive cannot be clicked.
//
//  Chrome no longer composites through an after-blit hook; it paints through
//  the host's panel-tree pump over the Apple ][ framebuffer. The per-frame
//  drive tick and door-animation redraw that used to live in that hook now run
//  in RunMessageLoop.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::FinishUiShellLayout()
{
    HRESULT  hr         = S_OK;
    UINT     initialDpi = GetDpiForWindow (m_hwnd);
    bool     fHasDisk   = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();



    // Propagate the live monitor DPI into UiShell so the first
    // D2D BindBackBuffer uses the right DPI for text. Without
    // this the initial paint binds at the m_dpi default (0->96)
    // and chrome text renders tiny on high-DPI displays until
    // the user resizes the window.
    hr = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                             m_d3dRenderer.GetBackBufferHeight(),
                             initialDpi);
    CHR (hr);

    // Chrome no longer composites via an after-blit hook: it
    // paints through the host's panel-tree pump (the adopted
    // chrome controls) on top of the Apple ][ framebuffer. The
    // per-frame drive-widget tick + door-animation redraw that
    // used to live in that hook now run in RunMessageLoop.
    if (DeskSceneActive())
    {
        // The 3D scene owns the drives: widgets hidden, drop-target rects
        // from the composition's projected drive bounds.
        SyncSceneDriveChrome();
    }
    else if (!fHasDisk)
    {
        // No Slot 6 controller (stripped Apple II config) --
        // collapse the drive widgets so they paint nothing
        // and the bottom command bar is clear of drive UI.
        // The joystick-mode button still paints, since
        // joystick input is independent of disk presence.
        m_driveChrome[0].Hide();
        m_driveChrome[1].Hide();
    }
    else if (!ShouldShowExternalDrive())
    {
        // //c with the optional external drive not connected: the
        // internal drive (widget 0) shows, the external (widget 1)
        // stays collapsed until the user connects it in Settings.
        m_driveChrome[1].Hide();
    }

    if (!DeskSceneActive())
    {
        m_uiShell.HitTest().Clear();
        if (fHasDisk)
        {
            m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[0].BodyRect(), DxuiHitSlot::Custom, 0 });
            if (ShouldShowExternalDrive())
            {
                m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[1].BodyRect(), DxuiHitSlot::Custom, 1 });
            }
        }
    }

    if (m_fOleInitialized)
    {
        InstallDragDropTarget();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstallDragDropTarget
//
//  Registers the window as an OLE drop target for disk images, and opens the
//  UIPI holes that make dropping work across an integrity-level boundary.
//
//  A failed registration is survivable and deliberately non-fatal: File > Open
//  and the drive widgets' click-to-browse mount the same images, so losing
//  drag-and-drop costs a convenience, not a capability, and must not prevent
//  launch.
//
//  The message filters are the non-obvious half. When Casso runs at a HIGHER
//  integrity level than the drag source -- the common case being an elevated
//  Casso and a normal Explorer window -- UIPI silently drops the messages OLE
//  uses to marshal the payload across the boundary, and the drop simply does
//  nothing with no error anywhere. Allowing the three messages OLE actually
//  uses for drop targets (WM_DROPFILES, WM_COPYDATA, and the undocumented but
//  real WM_COPYGLOBALDATA) makes Explorer-to-elevated-Casso drags work without
//  lowering Casso's own integrity level.
//
//  Only m_hwnd needs the filter: the window is a single top-level HWND now
//  that the legacy CassoRenderSurface child is gone.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InstallDragDropTarget()
{
    HRESULT     hrDrop            = S_OK;
    const UINT  kWmCopyGlobalData = 0x0049;   // undocumented but real



    // Drag-drop is an optional convenience -- File > Open and the drive
    // widgets' click-to-browse cover the same mounts -- so a failed
    // registration disables drop but must not prevent launch.
    hrDrop = m_dragDropTarget.Initialize (m_hwnd, &m_uiShell.HitTest(), [this] (int tag, const std::wstring & path) { Mount (6, tag, path); }, IsSupportedDiskImageExtension);
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
    (void) ChangeWindowMessageFilterEx (m_hwnd, WM_DROPFILES,      MSGFLT_ALLOW, nullptr);
    (void) ChangeWindowMessageFilterEx (m_hwnd, WM_COPYDATA,       MSGFLT_ALLOW, nullptr);
    (void) ChangeWindowMessageFilterEx (m_hwnd, kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPersistedAudioPrefs
//
//  Seeds the drive-audio mixer + //c default pointer from the per-machine
//  $cassoUiPrefs JSON before the audio thread first calls SetEnabled /
//  SetMechanism. Default is enabled + Shugart when nothing is persisted.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyPersistedAudioPrefs()
{
    HRESULT            hr      = S_OK;
    HRESULT            hrOpt   = S_OK;
    JsonValue          doc;
    const JsonValue *  uiPrefs = nullptr;
    bool               enabled = true;
    std::string        mechNarrow;
    double             motorV  = Disk2AudioSource::kMotorVolume;
    double             headV   = Disk2AudioSource::kHeadVolume;
    double             doorV   = Disk2AudioSource::kDoorVolume;
    double             pan0    = DriveAudioMixer::kDefaultDriveOnePan;
    double             pan1    = DriveAudioMixer::kDefaultDriveTwoPan;



    LoadMachineUiPrefs (doc, uiPrefs);
    BAIL_OUT_IF (uiPrefs == nullptr, S_OK);

    hrOpt = uiPrefs->GetBool ("floppySoundEnabled", enabled);
    if (SUCCEEDED (hrOpt))
    {
        m_driveAudioMixer.SetEnabled (enabled);
    }

    hrOpt = uiPrefs->GetString ("floppyMechanism", mechNarrow);
    if (SUCCEEDED (hrOpt) && !mechNarrow.empty())
    {
        // DriveAudioMixer matches mechanism names case-insensitively, so
        // the persisted lower-case token ("alps"/"shugart") can be handed
        // over as-is. A stale/unknown name leaves the mixer on its default
        // mechanism, which is fine -- not worth aborting startup for.
        std::wstring  mechWide (mechNarrow.begin(), mechNarrow.end());

        hrOpt = m_driveAudioMixer.SetMechanism (mechWide);
        IGNORE_RETURN_VALUE (hrOpt, S_OK);
    }

    // Optional gains + pans: each value is pre-seeded to its built-in
    // default, and GetNumber leaves that default in place when the key is
    // absent, so the read result is intentionally discarded -- absence
    // simply means "keep the default". SetDriveAudio* then applies the
    // resolved values (default or persisted) uniformly.
    hrOpt = uiPrefs->GetNumber ("driveMotorVolume", motorV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs->GetNumber ("driveHeadVolume",  headV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs->GetNumber ("driveDoorVolume",  doorV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    SetDriveAudioVolumes ((float) motorV, (float) headV, (float) doorV);

    hrOpt = uiPrefs->GetNumber ("driveOnePan", pan0);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs->GetNumber ("driveTwoPan", pan1);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    SetDriveAudioPan (0, (float) pan0);
    SetDriveAudioPan (1, (float) pan1);

    // //c case-switch latches: restore the 80/40 and keyboard (Dvorak) switch
    // positions onto the keyboard device. Absent keys leave the hardware
    // default (both out), and only //c configs carry them, so this is a no-op
    // elsewhere. The switch strip re-reads the device on its next
    // SyncSwitchBarState.
    {
        Apple2eKeyboard *  iieKbd = m_refs.iieKeyboard;

        if (iieKbd != nullptr)
        {
            bool  eightyIn = false;
            bool  dvorak   = false;

            if (uiPrefs->HasBool ("eightyColumnSwitch", eightyIn))
            {
                iieKbd->SetEightyColumnSwitchIn (eightyIn);
            }

            if (uiPrefs->HasBool ("keyboardDvorak", dvorak))
            {
                iieKbd->SetKeyboardSwitchDvorak (dvorak);
            }
        }
    }

    // //c: default Pointer -> Mouse when connected and nothing else was
    // chosen. (The external-drive + mouse connected-states are seeded in
    // ApplyPersistedChromePrefs, before the drive-chrome layout, so the
    // first paint reflects the saved setup rather than the defaults.)
    ApplyDefaultPointerForMachine();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurrentMachineNameNarrow
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::CurrentMachineNameNarrow() const
{
    std::string  narrow;



    narrow.reserve (m_currentMachineName.size());
    for (wchar_t c : m_currentMachineName)
    {
        narrow.push_back ((char) (unsigned char) c);
    }

    return narrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateEmulatorWindow
//
//  Creates the main window: work out where and how big BEFORE creating it,
//  then stand up the host, the panel tree, and the adopted chrome.
//
//  DPI is resolved for the DESTINATION monitor up front and the size is
//  pre-scaled. Under per-monitor DPI v2, CreateWindowEx treats the requested
//  size as PHYSICAL pixels on the target display -- there is no logical-to-
//  physical mapping -- so asking for 560 on a 150%-scaled monitor produces a
//  560-physical-pixel window that opens looking half-size next to everything
//  else on that display. The scaler is seeded with that DPI too, so the
//  chrome-band dock returns coherent thicknesses during the pre-Create math;
//  WM_NCCREATE overwrites it with GetDpiForWindow once the HWND exists, and
//  that value wins if the two disagree.
//
//  The window style is the custom-chrome recipe modeled on microsoft/terminal's
//  NonClientIslandWindow: keep the full WS_OVERLAPPEDWINDOW so DefWindowProc
//  retains the caption infrastructure -- drag-to-move, edge resize, snap
//  layouts, single-click min/max/close -- and hide the VISUAL caption by
//  collapsing the NC area in WM_NCCALCSIZE. WM_NCHITTEST then reports the
//  button and drag regions so the OS still dispatches real system actions.
//
//  WS_CAPTION is stripped for the rect-adjust math only, and that subtraction
//  is load-bearing. The WM_NCCALCSIZE handler PRESERVES the top edge rather
//  than carving a caption out of the client area, so adjusting with the full
//  style would add the caption height to the window height and then have
//  NCCALCSIZE hand that same height back as client space. The client ends up
//  taller than requested by exactly the caption height, and the aspect-fit
//  content area pillarboxes.
//
//  Placement is computed, then clamped, in that order. A restored placement is
//  clamped to the work area as well, because prefs written by older builds can
//  hold a full-monitor rect (a fullscreen transition once saved its rect as
//  the windowed placement) that would restore a taskbar-covering "windowed"
//  window.
//
//  Icons are preloaded and handed to Create so they can be attached by
//  WM_SETICON before the window is shown: the taskbar and Win32 MessageBox
//  read the icon from WM_GETICON, not from WNDCLASS::hIcon.
//
//  The client is installed BEFORE Create, so the WM_NCCREATE / WM_CREATE /
//  WM_SIZE / WM_MOVE burst that fires synchronously inside CreateWindowExW
//  dispatches through the OnXxx handlers instead of being lost.
//
//  createSwapChain is true: the HOST owns the D3D11 device, the flip-discard
//  swap chain, and the panel-tree paint pump. The framebuffer renderer
//  composites into that same back buffer through the before-present hook
//  (wired in InitializeRenderer) and the chrome paints on top. There is no
//  child render-surface HWND any more -- a single window proc owns all mouse,
//  NC, and cursor handling.
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
    bool                          haveWork          = false;
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
    if (TryGetCursorMonitorWorkArea (work, activeMon))
    {
        UINT     dpiX  = 0;
        UINT     dpiY  = 0;
        HRESULT  hrDpi = S_OK;


        hrDpi = GetDpiForMonitor (activeMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

        if (SUCCEEDED (hrDpi) && dpiX > 0)
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

    haveWork = TryGetCursorMonitorWorkArea (work, activeMon);

    if (haveWork)
    {
        // The 100%-emulator + full monitor framing can want a window taller
        // than the display; never open larger than the work area. The monitor
        // frame then lays out its housing into whatever center it gets, so the
        // emulator lands at 100% when it fits and the largest size that fits
        // otherwise.
        windowW = std::min (windowW, (int) (work.right  - work.left));
        windowH = std::min (windowH, (int) (work.bottom - work.top));
        CenterInWorkArea (work, windowW, windowH, windowX, windowY);
    }

    hadSavedPlacement = m_windowManager.TryLoadSavedWindowPlacement (activeMon, windowX, windowY, windowW, windowH, m_startMaximized);

    // Clamp a restored placement to the work area as well: prefs written by
    // older builds could hold a full-monitor rect (a fullscreen transition
    // once saved its rect as the windowed placement), which would restore a
    // taskbar-covering "windowed" window. Pull it back onto the desktop.
    if (hadSavedPlacement && haveWork)
    {
        windowW = std::min (windowW, (int) (work.right  - work.left));
        windowH = std::min (windowH, (int) (work.bottom - work.top));
        windowX = std::clamp (windowX, work.left, std::max (work.left, work.right  - windowW));
        windowY = std::clamp (windowY, work.top,  std::max (work.top,  work.bottom - windowH));
    }

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

    // There is a window to parent a dialog to now, so anything reported
    // during startup can finally be shown.
    FlushPendingNotifications();

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
    m_host->Root().Adopt (m_sceneDriveLabel[0]);
    m_host->Root().Adopt (m_sceneDriveLabel[1]);
    m_host->Root().Adopt (m_captureBanner);
    m_host->Root().Adopt (m_sceneCompass);

    // The compass reports gestures; the shell owns what they mean. The signs
    // follow the drag's bargain -- the CONTENT goes where the arrow points --
    // so the right arrow and a rightward drag turn the scene the same way.
    m_sceneCompass.SetOnStep ([this] (DxuiOrbitControl::Part part)
    {
        switch (part)
        {
            case DxuiOrbitControl::Part::Left:   OrbitSceneBy ( kCompassStepYawRad,   0.0f); break;
            case DxuiOrbitControl::Part::Right:  OrbitSceneBy (-kCompassStepYawRad,   0.0f); break;
            case DxuiOrbitControl::Part::Up:     OrbitSceneBy (0.0f, -kCompassStepPitchRad); break;
            case DxuiOrbitControl::Part::Down:   OrbitSceneBy (0.0f,  kCompassStepPitchRad); break;
            default: break;
        }
    });

    m_sceneCompass.SetOnDrag ([this] (DxuiOrbitControl::Part part, float dxPx, float dyPx)
    {
        float  rate = OrbitRadPerPx();

        // Axis-locked to the arrow the drag started on: the arrow names an
        // axis, and a free two-axis tumble from a single arrow would make
        // the four of them meaningless.
        switch (part)
        {
            case DxuiOrbitControl::Part::Left:
            case DxuiOrbitControl::Part::Right:  OrbitSceneBy (-dxPx * rate, 0.0f); break;
            case DxuiOrbitControl::Part::Up:
            case DxuiOrbitControl::Part::Down:   OrbitSceneBy (0.0f,  dyPx * rate); break;
            default: break;
        }
    });

    m_sceneCompass.SetOnHome ([this] ()
    {
        m_sceneView.orbitYawRad   = 0.0f;
        m_sceneView.orbitPitchRad = 0.0f;
        InvalidateSceneComposition();
    });
    m_host->Root().Adopt (m_toolbar);
    m_host->Root().Adopt (m_joystickButton);
    m_host->Root().Adopt (m_switchBar);

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
    // Tick. SetTheme seeds the tooltip surface colors.
    m_joystickTooltip.SetPopupHost (m_host.get());
    m_joystickTooltip.SetTheme     (m_chromeTheme);
    m_toolbarTooltip.SetPopupHost  (m_host.get());
    m_toolbarTooltip.SetTheme      (m_chromeTheme);

    // The //c switch strip shares the same deferred-tooltip pattern.
    m_switchBarTooltip.SetPopupHost (m_host.get());
    m_switchBarTooltip.SetTheme     (m_chromeTheme);
    // The drive-widget write-protect tooltip shares the host popup pool.
    // It surfaces on a dwell over a write-protected drive and names the
    // protection source(s).
    m_driveTooltip.SetPopupHost (m_host.get());
    m_driveTooltip.SetTheme     (m_chromeTheme);

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
        RECT  rcActual  = {};
        UINT  windowDpi = 0;


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
        windowDpi = GetDpiForWindow (m_hwnd);
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

    // Command toolbar (DCR-2): commands route through the same HandleCommand
    // path as the menu; the volume group drives the master output gain and
    // persists in GlobalUserPrefs (saved with the rest on exit).
    m_toolbar.SetDispatch ([this] (WORD commandId) { HandleCommand (commandId); });

    // Input-mode segments route through the same toggle the band selector
    // used, so the leave-time neutralization of held arrow / X / Z inputs
    // runs identically.
    m_toolbar.SetInputSink ([this] (InputMappingMode mode) { ToggleInputMappingMode (mode); });
    m_toolbar.SetVolumeSink ([this] (float volume01, bool muted)
    {
        m_globalPrefs.masterVolume = volume01;
        m_globalPrefs.masterMuted  = muted;
        m_wasapiAudio.SetMasterGain (muted ? 0.0f : volume01);
    });
    m_toolbar.SetVolume (m_globalPrefs.masterVolume, m_globalPrefs.masterMuted);
    m_wasapiAudio.SetMasterGain (m_globalPrefs.masterMuted ? 0.0f : m_globalPrefs.masterVolume);
    m_mainMenu.SetCheckQuery ([this] (WORD commandId) -> bool
    {
        switch (commandId)
        {
            case IDM_MACHINE_ARROWS_JOYSTICK: return m_arrowsJoystick;
            case IDM_MACHINE_ARROWS_PADDLE:   return m_pointerMode == InputMappingMode::Paddle;

            default:                          return false;
        }
    });

    m_mainMenu.SetEnableQuery ([this] (WORD commandId) -> bool
    {
        switch (commandId)
        {
            case IDM_DISK_WP1:      return IsWriteProtectToggleOffered (0);
            case IDM_DISK_WP2:      return IsWriteProtectToggleOffered (1);
            case IDM_DISK_SALVAGE1: return IsSalvageOffered (0);
            case IDM_DISK_SALVAGE2: return IsSalvageOffered (1);
            default:           return true;
        }
    });

    m_mainMenu.SetLabelQuery ([this] (WORD commandId) -> std::wstring
    {
        switch (commandId)
        {
            case IDM_DISK_WP1:
            case IDM_DISK_WP2:
            {
                // Name the mounted image and state the ACTION the click will
                // take, flipping the verb with the image's own protection
                // (the file-borne flag or read-only attribute; the per-drive
                // USER write-protect pref is a different toggle). An empty
                // return keeps the static "Write-protect disk N" label for
                // an empty (disabled) drive.
                constexpr size_t  kMaxNameChars  = 20;
                constexpr size_t  kKeepHeadChars = 10;
                constexpr size_t  kKeepTailChars = 7;

                int           drive = (commandId == IDM_DISK_WP1) ? 0 : 1;
                DiskImage  *  image = m_diskStore.GetImage (6, drive);
                std::wstring  name;

                if (image == nullptr)
                {
                    return std::wstring();
                }

                name = std::filesystem::path (
                           m_diskStore.GetSourcePath (6, drive)).filename().wstring();

                if (name.empty())
                {
                    return std::wstring();
                }

                // Middle-truncate very long names so the row stays inside
                // the dropdown while keeping the extension visible.
                if (name.size() > kMaxNameChars)
                {
                    name = name.substr (0, kKeepHeadChars) + L"..."
                         + name.substr (name.size() - kKeepTailChars);
                }

                WriteProtectInfo  info = image->GetWriteProtectInfo();

                // Names the flag the command actually changes. The old wording
                // ("Allow writes to ...") described an outcome the command
                // cannot promise -- the host file's read-only attribute and the
                // drive preference protect the disk too, and neither is touched
                // here. It also flipped on readOnlyFile, so a writable image in
                // a read-only file offered to "allow writes" and then could not.
                return (info.imageFlag ? L"Clear \"" : L"Set \"")
                     + name + L"\" internal write-protect flag";
            }

            default:
                return std::wstring();
        }
    });

    // Load the app icon (IDI_CASSO) into a premultiplied BGRA8 pixel
    // buffer and hand it to the host caption (like WM_SETICON for the
    // window glyph). Loaded at 32x32 (high enough to look crisp at
    // typical caption sizes when D2D linearly downscales it); failure
    // is non-fatal -- the caption simply omits the icon if it misses.
    {
        std::vector<uint32_t>  iconPixels;
        int                    iconW      = 0;
        int                    iconH      = 0;
        HRESULT                hrIcon     = S_OK;

        hrIcon = LoadIconAsPremulBgra (hInstance, IDI_CASSO, 32, iconPixels, iconW, iconH);

        if (SUCCEEDED (hrIcon))
        {
            m_host->SetCaptionIcon (std::move (iconPixels), iconW, iconH);
        }
    }

    m_driveChrome[0].Initialize (6, 0, this);
    m_driveChrome[1].Initialize (6, 1, this);

    // Settle the desk-scene scale (monitor fit + band heights) BEFORE laying
    // the drive widgets, so they are born at the settled scale rather than
    // the 1.0 default.
    UpdateViewportLayout (clientW, clientH);

    {
        RECT  vr            = ComputeViewportRect (clientW, clientH);
        RECT  driveRect     = m_driveBand.Bounds();
        int   bottomInsetPx = clientH - driveRect.top;   // drive band height only

        (void) vr;                                        // dock side-effect: bands arranged

        if (DeskSceneActive())
        {
            SyncSceneDriveChrome();
        }
        else
        {
            LayoutDriveWidgetsInCommandBar (m_driveChrome, bottomInsetPx, clientW, clientH, dpi, m_chromeSceneScale);
        }

        {
            int  bandTop    = driveRect.top;
            int  bandHeight = MulDiv (s_kJoystickButtonBandDp, static_cast<int> (dpi), s_kBaseDpi);

            m_driveBandSurface.SetVisible (!DeskSceneActive());
            m_driveBandSurface.SetBounds (RECT{ 0, bandTop, clientW, clientH });
            LayoutJoystickButton (clientW, bandTop, bandHeight, dpi);
        }

        LayoutSwitchBar (dpi);
    }

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
    RECT     center       = {};
    RECT     viewportRect = {};



    BAIL_OUT_IF (m_viewport == nullptr, S_OK);

    // 3D desk scene (spec 018): the composition is computed for the center
    // rect (drives included -- they are scene objects now), the viewport
    // (the CRT target) becomes the projected glass rect, and the bottom band
    // collapses to the joystick row via SyncChromeBands' scene branch. The
    // settle loop is retained for the band's dock feedback.
    if (CrtMonitorActive() && m_d3dRenderer.IsFullscreen())
    {
        // Fullscreen presentation (FR-014): the glass fills the monitor with
        // a straight-on camera, every chrome band hidden -- the whole client
        // is the scene. The drive overlay strip presents the drives.
        HRESULT               hrLayout = S_OK;
        DeskSceneComposition  comp;
        RECT                  full     = { 0, 0, widthPx, heightPx };

        hrLayout = DeskSceneLayout::ComputeGlassFill (full, m_scaler.Dpi(),
                                                      m_deskScene.Metrics(), comp);
        BAIL_OUT_IF (hrLayout != S_OK, S_OK);

        m_deskScene.SetComposition (comp);
        m_chromeSceneScale = comp.sceneScale * s_kDeskDriveScale;
        viewportRect       = full;

        SyncSceneDriveChrome();
    }
    else if (CrtMonitorActive())
    {
        // The basename strip under the drive row is chrome, not scene, so the
        // composition is solved into a center rect short by its height and
        // the labels hang in what is left.
        int  labelStripPx = m_scaler.Px (s_kSceneDriveLabelStripDp + s_kSceneDriveLabelGapDp);

        for (int pass = 0; pass < s_kSceneScaleSettlePasses; pass++)
        {
            HRESULT               hrLayout = S_OK;
            DeskSceneComposition  comp;
            RECT                  sceneBox = {};

            center            = ComputeViewportRect (widthPx, heightPx);
            sceneBox          = center;
            sceneBox.bottom   = std::max (center.top, center.bottom - labelStripPx);

            hrLayout = DeskSceneLayout::Compute (sceneBox, m_scaler.Dpi(), DeskSceneDriveCount(),
                                                 m_deskScene.Metrics(), comp,
                                                 m_scaler.Px (s_kJoystickButtonBandDp + s_kStripEdgeZoneDp),
                                                 m_sceneView);
            BAIL_OUT_IF (hrLayout != S_OK, S_OK);

            m_deskScene.SetComposition (comp);
            m_chromeSceneScale = comp.sceneScale * s_kDeskDriveScale;

        }

        viewportRect = m_deskScene.Composition().glassRectPx;

        SyncSceneDriveChrome();

        // The input-mode buttons sit in the scene's gap between the monitor
        // and the drive row, where the 2D chrome kept them -- CENTERED in
        // the gap, never over the monitor's shell.
        {
            const DeskSceneComposition &  comp       = m_deskScene.Composition();
            int                           bandH      = m_scaler.Px (s_kJoystickButtonBandDp);
            int                           monBottom  = comp.monitorRectPx.bottom;
            int                           driveTop   = heightPx;
            int                           bandTop    = 0;

            for (int i = 0; i < comp.driveCount; i++)
            {
                driveTop = std::min (driveTop, (int) comp.driveRectPx[i].top);
            }

            // The input row lives on the command toolbar now -- there is no
            // gap between monitor and drives to float it in since the stack
            // became physical, and a row over the drive lids read as debris.
            m_joystickButton.Hide();

            (void) bandTop;
            (void) monBottom;
            (void) driveTop;
            (void) bandH;
        }
    }
    else if (DeskSceneActive() && m_d3dRenderer.IsFullscreen())
    {
        // Monitor opted out, fullscreen: still the immersive presentation --
        // every chrome band hidden and the picture filling the client (the
        // renderer letterboxes inside the target bounds), just without the
        // curved glass. The drives come from the overlay strip exactly as
        // they do with the monitor on, so the main composition holds nothing.
        m_chromeSceneScale = 1.0f;
        viewportRect       = { 0, 0, widthPx, heightPx };

        m_deskScene.SetComposition (DeskSceneComposition{});

        SyncSceneDriveChrome();
    }
    else if (DeskSceneActive())
    {
        // Monitor opted out: the picture goes back on a flat rect at classic
        // sizes, but the drives are NOT optional -- they compose as a 3D row
        // in the bottom band, through the same drives-only solve (its own
        // contained camera over the band, so FR-016 still holds within it)
        // the fullscreen overlay strip uses. The band keeps its classic
        // thickness, so the window geometry matches the flat chrome it
        // replaces; the input-mode buttons keep the band's top row.
        DeskSceneComposition  comp;
        RECT                  band     = {};
        RECT                  driveRow = {};
        bool                  composed = false;
        int                   joyH     = m_scaler.Px (s_kJoystickButtonBandDp);
        int                   pad      = m_scaler.Px (s_kSceneDriveRowPadDp);

        m_chromeSceneScale = 1.0f;
        center             = ComputeViewportRect (widthPx, heightPx);
        viewportRect       = center;

        band     = m_driveBand.Bounds();
        driveRow = { pad, band.top + joyH + pad / 2, widthPx - pad,
                     std::max (band.bottom - pad - m_scaler.Px (s_kSceneDriveLabelStripDp +
                                                                s_kSceneDriveLabelGapDp),
                               (LONG) (band.top + joyH)) };

        // A machine with no Disk ][ controller composes no row at all, and a
        // band too small to solve leaves the scene empty rather than stale.
        if (DeskSceneDriveCount() > 0)
        {
            composed = DeskSceneLayout::ComputeStrip (driveRow, m_scaler.Dpi(), DeskSceneDriveCount(),
                                                      m_deskScene.Metrics(), comp,
                                                      DeskSceneLayout::kDriveBandGazeDownRad) == S_OK;
        }

        m_deskScene.SetComposition (composed ? comp : DeskSceneComposition{});

        SyncSceneDriveChrome();
    }
    else
    {
        // No scene at all (compact theme, or the models never loaded): the
        // bare display fills the center at classic sizes over the 2D drive
        // band.
        m_chromeSceneScale = 1.0f;
        center             = ComputeViewportRect (widthPx, heightPx);
        viewportRect       = center;
    }

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

void EmulatorShell::SyncChromeBands()
{
    int  switchBandDp = 0;



    // When the machine has no Disk ][ controller, remove the drive-widget area
    // entirely (#84 Phase D): the drive band collapses to just the joystick-mode
    // button band (joystick input is independent of disk presence), reclaiming
    // the ~180 dp the drive widgets + their in-use indicators would occupy so
    // the emulator viewport grows into it. The widgets are already hidden and
    // un-hit-tested by the resize path when there is no controller.
    bool  hasDisk     = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
    int   driveBandDp = CrtMonitorActive() ? 0 : s_kJoystickButtonBandDp;

    if (hasDisk && !CrtMonitorActive())
    {
        // The joystick-selector portion is fixed UI; the drive-widget portion
        // zooms with the desk scene (m_chromeSceneScale) so the band hugs the
        // scaled widgets instead of leaving dead space around them. With the
        // 3D scene active there is NO bottom band at all: the drives are
        // scene objects and the input-mode buttons float in the scene's gap
        // between monitor and drive row, like the 2D chrome's arrangement.
        int  widgetPortionDp = m_driveBarThicknessDp - s_kJoystickButtonBandDp;

        driveBandDp = s_kJoystickButtonBandDp
                    + (int) lroundf ((float) widgetPortionDp * m_chromeSceneScale);
    }

    // The //c switch strip only exists on the //c; everywhere else the band
    // collapses to zero height so the dock leaves the viewport unchanged.
    switchBandDp = IsApple2c() ? s_kSwitchBandDp : 0;

    m_titleBand.SetBounds   (RECT{ 0, 0, 0, m_scaler.Px (s_kTitleBarBandDp) });
    m_navBand.SetBounds     (RECT{ 0, 0, 0, m_scaler.Px (s_kNavStripBandDp) });
    m_toolbarBand.SetBounds (RECT{ 0, 0, 0, m_scaler.Px (m_toolbar.BandDp()) });
    m_driveBand.SetBounds   (RECT{ 0, 0, 0, m_scaler.Px (driveBandDp) });
    m_switchBand.SetBounds  (RECT{ 0, 0, 0, m_scaler.Px (switchBandDp) });
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
    IDxuiControl *  kids[] = { &m_titleBand, &m_navBand, &m_toolbarBand, &m_driveBand, &m_switchBand, &m_centerBand };



    // The toolbar's band thickness depends on its responsive mode (icon+label
    // / ribbon / icon-only), which depends on the width -- plan it BEFORE the
    // bands dock so the strip gets the right height for this window size.
    m_toolbar.PlanForWidth (widthPx, m_scaler);

    SyncChromeBands();
    m_chromeDock.Arrange (RECT{ 0, 0, widthPx, heightPx }, m_scaler, kids);

    // The command toolbar rides its band: re-lay it every viewport pass so a
    // resize / DPI change reflows the buttons with the strip.
    //
    // EXCEPT IN FULLSCREEN, where the reveal overlay owns it. Both were
    // laying it out -- the dock into a band the fullscreen viewport does not
    // show, the overlay across the top -- so whichever ran last won, and the
    // buttons painted at one height while their hit rects sat at the other.
    // A single owner per presentation, or they disagree.
    if (!m_d3dRenderer.IsFullscreen())
    {
        m_toolbar.Layout (m_toolbarBand.Bounds(), m_scaler);
    }

    return m_centerBand.Bounds();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::EmulatorContentScreenRect
//
//  The emulator IMAGE rect in screen pixels, for the Settings live-preview
//  compositor's see-through reveal (#8). Answered from the renderer's cache
//  (recorded at the last CRT frame): that is the aspect-FITTED image rect, not
//  the whole center band, so the reveal hole hugs the picture instead of also
//  punching through over the letterbox. The cache is at most one frame stale
//  -- while the settings sheet is open TryPresentUiFrame force-presents every
//  UI frame -- and empty until the window + swap chain have produced a frame,
//  which callers read as "no reveal".
//
////////////////////////////////////////////////////////////////////////////////

RECT EmulatorShell::EmulatorContentScreenRect()
{
    return m_d3dRenderer.GetEmulatorContentScreenRect();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReflowChromeForMachineChange
//
//  A machine switch may add or remove the Disk ][ controller, which changes the
//  drive-band thickness (Phase D), the drive-widget visibility, and the hit-test
//  map. When disk presence changes, grow/shrink the WINDOW by the band delta so
//  the emulator viewport keeps its size and the top-left corner stays put -- NOT
//  hold the window size and re-center the viewport. The resulting WM_SIZE drives
//  OnSize, which re-lays the bands / widgets / hit rects. When presence is
//  unchanged (e.g. a swap between two controller-equipped machines) there is no
//  band delta, so just re-run OnSize at the current size to refresh the widgets.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReflowChromeForMachineChange()
{
    RECT  rcWindow      = {};
    bool  haveWindow    = false;
    bool  newHasDisk    = false;
    bool  newIsApple2c  = false;
    bool  layoutChanged = false;
    bool  didResize     = false;



    DXUI_ASSERT_UI_THREAD();   // chrome layout: never from the CPU thread

    haveWindow = m_hwnd != nullptr && GetWindowRect (m_hwnd, &rcWindow);

    if (haveWindow)
    {
        newHasDisk    = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
        newIsApple2c  = IsApple2c();
        layoutChanged = (newHasDisk != m_chromeSizedForHasDisk) ||
                        (newIsApple2c != m_chromeSizedForApple2c);
    }

    // The desk wears what the machine wore, so crossing the //c boundary
    // swaps both models. Reloading rebuilds every cached mesh, so the
    // scene's own state is pushed again right after.
    if (m_deskSceneReady && IsApple2c() != m_deskSceneMachineIsC)
    {
        HRESULT  hrModels = LoadDeskSceneModelsForMachine();

        if (SUCCEEDED (hrModels))
        {
            m_deskScene.SetPowerLampOn (true);
        }

        IGNORE_RETURN_VALUE (hrModels, S_OK);
    }

    // Resize the window by the total bottom-band delta -- the drive band
    // (disk-presence) plus the //c switch band -- but not for min/max/fullscreen
    // windows, where the user explicitly chose the size (mirrors
    // ApplyThemeToChrome). Those just relayout inside the fixed frame.
    if (haveWindow && layoutChanged &&
        !IsIconic (m_hwnd) && !IsZoomed (m_hwnd) && !m_d3dRenderer.IsFullscreen())
    {
        int  oldDriveDp  = m_chromeSizedForHasDisk ? m_driveBarThicknessDp : s_kJoystickButtonBandDp;
        int  newDriveDp  = newHasDisk              ? m_driveBarThicknessDp : s_kJoystickButtonBandDp;
        int  oldSwitchDp = m_chromeSizedForApple2c ? s_kSwitchBandDp : 0;
        int  newSwitchDp = newIsApple2c            ? s_kSwitchBandDp : 0;
        int  deltaPx     = (m_scaler.Px (newDriveDp)  - m_scaler.Px (oldDriveDp)) +
                           (m_scaler.Px (newSwitchDp) - m_scaler.Px (oldSwitchDp));

        m_chromeSizedForHasDisk = newHasDisk;
        m_chromeSizedForApple2c = newIsApple2c;

        // The bands are bottom-docked full-width, so only the height moves.
        // SWP_NOMOVE pins the top-left corner; the WM_SIZE it generates drives
        // OnSize to re-lay the bands, widgets, and hit-test map.
        SetWindowPos (m_hwnd, nullptr, 0, 0,
                      rcWindow.right  - rcWindow.left,
                      (rcWindow.bottom - rcWindow.top) + deltaPx,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // The WM_SIZE above already re-lays everything, so the relayout below
        // must not also run.
        didResize = true;
    }
    else if (haveWindow)
    {
        m_chromeSizedForHasDisk = newHasDisk;
        m_chromeSizedForApple2c = newIsApple2c;
    }

    // No band delta (unchanged presence, or a fixed-state window): relayout at
    // the current client size so widget visibility + hit rects still refresh.
    if (haveWindow && !didResize)
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
//  EmulatorShell::ShouldShowExternalDrive
//
//  The //c's second drive is an optional external unit that plugs into the
//  disk port, so it appears only when the user has marked it connected
//  (Hardware tab toggle -> $cassoUiPrefs.externalDriveConnected). The //c is
//  the only machine with a banked system ROM, so romBankSize is the
//  discriminator -- the same signal that gates the built-in IWM drive.
//
//  Everywhere else the second drive is whatever is attached to the Disk ][
//  card's second connector. That used to be unconditionally true, on the
//  reasoning that the card is two-drive hardware -- but the CARD having two
//  connectors was never the same claim as both of them having a drive on the
//  end, and this is the question the 2D widgets and the desk scene both ask,
//  so answering it from the config is what keeps them agreeing.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ShouldShowExternalDrive() const
{
    bool  externalIsOptional = (m_config.systemRom.romBankSize != 0);



    if (externalIsOptional)
    {
        return m_externalDriveConnected;
    }

    return m_config.AttachedDiskIiDriveCount() >= kDiskIiPortCount;
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
    IDxuiControl *  bands[] = { &m_titleBand, &m_navBand, &m_toolbarBand, &m_driveBand, &m_switchBand };



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
    SIZE  client         = {};
    int   framebufferWpx = m_scaler.Px (framebufferWidthDp);
    int   framebufferHpx = m_scaler.Px (framebufferHeightDp);



    // With the desk scene on, size the window so the monitor's screen RECESS
    // -- not the bare center -- equals the framebuffer, i.e. the emulator
    // image sits at 100% zoom inside the housing, with the bezel, desk margin
    // and chrome bands sized around it. This inverse defines the 100% scene,
    // where the drives sit at s_kDeskDriveScale, so the band math must run at
    // that scale regardless of the current window's. Scene off: the center is
    // the framebuffer directly at classic sizes.
    if (CrtMonitorActive())
    {
        SIZE   center     = DeskSceneLayout::CenterSizeForDisplayPx (framebufferWpx, framebufferHpx,
                                                                     m_scaler.Dpi(), DeskSceneDriveCount(),
                                                                     m_deskScene.Metrics(),
                                                                     m_scaler.Px (s_kJoystickButtonBandDp + s_kStripEdgeZoneDp));
        float  savedScale = m_chromeSceneScale;

        m_chromeSceneScale = s_kDeskDriveScale;
        client             = ClientSizeForCenterPx (center.cx, center.cy);
        m_chromeSceneScale = savedScale;
    }
    else
    {
        client = ClientSizeForCenterPx (framebufferWpx, framebufferHpx);
    }

    return client;
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
    HRESULT      hr             = S_OK;
    SIZE         desired        = {};
    RECT         rcActualClient = {};
    RECT         rcActualWindow = {};
    HMONITOR     hMon           = nullptr;
    MONITORINFO  mi             = { sizeof (mi) };
    int          ncOverheadW    = 0;
    int          ncOverheadH    = 0;
    int          desiredClientW = 0;
    int          desiredClientH = 0;
    int          fixedW         = 0;
    int          fixedH         = 0;
    bool         needsReconcile = !m_initialSizeReconciled && m_hwnd != nullptr;
    bool         haveRects      = false;
    bool         haveWork       = false;



    BAIL_OUT_IF (!needsReconcile, S_OK);

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

    haveRects = GetClientRect (m_hwnd, &rcActualClient) && GetWindowRect (m_hwnd, &rcActualWindow);

    BAIL_OUT_IF (!haveRects, S_OK);

    ncOverheadW = (rcActualWindow.right  - rcActualWindow.left)
                  - (rcActualClient.right  - rcActualClient.left);
    ncOverheadH = (rcActualWindow.bottom - rcActualWindow.top)
                  - (rcActualClient.bottom - rcActualClient.top);

    fixedW = desiredClientW + ncOverheadW;
    fixedH = desiredClientH + ncOverheadH;

    // The 100%-emulator + full monitor framing can want a window bigger than
    // the display; never size past the work area. When clamped, the monitor
    // frame re-fits its housing into the smaller client (emulator drops below
    // 100%), which beats a window whose menu/drives fall off-screen.
    hMon     = MonitorFromWindow (m_hwnd, MONITOR_DEFAULTTONEAREST);
    haveWork = (hMon != nullptr && GetMonitorInfo (hMon, &mi));

    if (haveWork)
    {
        fixedW = std::min (fixedW, (int) (mi.rcWork.right  - mi.rcWork.left));
        fixedH = std::min (fixedH, (int) (mi.rcWork.bottom - mi.rcWork.top));
    }

    if (fixedW != (rcActualWindow.right  - rcActualWindow.left) ||
        fixedH != (rcActualWindow.bottom - rcActualWindow.top))
    {
        // Recenter on the current monitor's work area using the final size. The
        // initial Create centered using a pre-reconcile estimate; without this
        // re-center the reconcile resize would grow the window from its
        // top-left and leave it off center vs the Ctrl+0 reset.
        int   x     = 0;
        int   y     = 0;
        UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE;

        if (haveWork)
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

Error:
    return;
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
//  The HRESULT it returns says only that the mount was queued, never that it
//  worked: the mount itself runs later, on the CPU thread. Recording the disk
//  here used to read that as success and put a file the loader would go on to
//  refuse into the picker's recent list. The recording moved to the mount's
//  own completion, which is the first place the answer is known.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::Mount (int slot, int drive, const std::wstring & path)
{
    HRESULT  hr = S_OK;



    hr = m_diskManager->Mount (slot, drive, path);
    CHR (hr);

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
//  The mount's own HRESULT goes to DiskMru rather than being tested here,
//  so the "only a mount that happened counts" rule lives with the list it
//  protects instead of with whoever remembered to check.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordRecentDisk (const std::wstring & path, HRESULT mountResult)
{
    HRESULT                    hr         = S_OK;
    DiskMru                    mru;
    std::filesystem::path      fsPath;
    std::vector<std::string>   serialized;
    std::vector<std::int64_t>  loadedAt;
    std::int64_t               nowUnix    = 0;



    BAIL_OUT_IF (path.empty(), S_OK);

    nowUnix = (std::int64_t) std::chrono::duration_cast<std::chrono::seconds> (
                  std::chrono::system_clock::now().time_since_epoch()).count();

    fsPath = std::filesystem::path (path);
    mru    = DiskMru::FromUtf8 (m_globalPrefs.recentDisks, m_globalPrefs.recentDiskLoadedAt);
    mru.RecordMountResult (mountResult, fsPath, nowUnix);
    mru.ToUtf8 (serialized, loadedAt);
    m_globalPrefs.recentDisks        = std::move (serialized);
    m_globalPrefs.recentDiskLoadedAt = std::move (loadedAt);

    SaveGlobalPrefs();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnMountCompleted
//
//  Every attempted mount ends here with its own HRESULT, and the only job
//  this half has is getting that onto the UI thread.
//
//  It posts rather than acting, in both directions. A mount the user started
//  ran on the CPU thread, and the reaction raises Dxui modals, which assert
//  UI-thread affinity. A command-line mount ran on the UI thread but did so
//  inside Initialize, before the message loop exists to service a modal, so
//  acting there would park startup behind a dialog with nothing running
//  behind it. Posting covers both, and the posted messages arrive in mount
//  order, which is what puts the boot disk at the top of the recent list.
//
//  With no window, or with the post refused, the fallback is to handle it
//  inline: ShowNotification queues rather than shows when there is nothing to
//  parent a dialog to, so the report survives either way.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnMountCompleted (int drive, const std::string & path, HRESULT mountResult,
                                      const MountDiagnosis & diagnosis)
{
    MountCompletion *  carried  = nullptr;
    MountCompletion    fallback;
    bool               isPosted = false;



    fallback.path      = path;
    fallback.diagnosis = diagnosis;
    fallback.result    = mountResult;
    fallback.drive     = drive;

    if (m_hwnd != nullptr)
    {
        carried = new (std::nothrow) MountCompletion (fallback);
    }

    if (carried != nullptr)
    {
        isPosted = (PostMessageW (m_hwnd, WM_APP_MOUNT_COMPLETED, 0,
                                  reinterpret_cast<LPARAM> (carried)) != FALSE);

        if (!isPosted)
        {
            delete carried;
        }
    }

    if (!isPosted)
    {
        HandleMountCompletion (fallback);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HandleMountCompletion
//
//  The UI-thread half. A mount that worked joins the recent-disks list and is
//  checked for a damaged image; a mount that did not is reported to the user
//  and joins nothing.
//
//  The failure report goes through EhmNotifyUser like every other user-facing
//  refusal in the tree, and for the same reason: an image the loader will not
//  take is bad input, not a bug in Casso, so it earns a sentence and not an
//  assert.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleMountCompletion (const MountCompletion & completion)
{
    std::wstring  message;



    RecordRecentDisk (fs::path (completion.path).wstring(), completion.result);

    if (SUCCEEDED (completion.result))
    {
        ReportDamagedMount (completion.drive);
        return;
    }

    message = DiskImageStore::FormatMountFailureMessage (completion.path, completion.diagnosis);

    EhmNotifyUser (message.c_str());
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
//  UI helper: start the drive-door-open animation and show the disk
//  picker IN PARALLEL. The picker's modal GetMessage loop owns the UI
//  thread, so RunMessageLoop stops driving frames; the host's modal
//  keep-alive tick (the same one that carries chrome through an OS
//  move / size loop) drives TryPresentUiFrame for the dialog's whole
//  lifetime, so the door visibly opens behind the picker instead of
//  stalling until it is dismissed. This replaces a blocking pre-dialog
//  wait that pumped the full animation before the picker appeared --
//  dead time at best, and a frozen door whenever the present gate
//  declined the frames.
//
//  Mount-on-success runs through DiskManager::Mount, which queues
//  to the CPU thread and posts a DoorClose sync event picked up
//  by UpdateDriveWidgets -- so we don't need to touch the door on
//  the success path here; BeginInsert closes it naturally. Cancel
//  restores the door to match the mount state: a mounted drive
//  closes back, an empty drive rests open (matches a real Disk II).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseForDisk (int drive)
{
    DriveWidgetState *  pSt          = nullptr;
    HRESULT             hrBrowse     = S_OK;
    bool                mountStarted = false;



    using std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto  nowMs = []() -> int64_t {
        return (int64_t) duration_cast<milliseconds> (steady_clock::now().time_since_epoch()).count();
    };



    if (drive < 0 || drive >= (int) m_driveWidgetState.size())
    {
        return;
    }

    pSt = &m_driveWidgetState[drive];

    // The time base MUST match DiskManager::NowMs (steady_clock ms):
    // TickDoorAnimation diffs the current frame time against
    // animationStartTimeMs. An empty drive rests with its door already
    // Open, so StartDoorTransition is a no-op there.
    pSt->StartDoorTransition (DriveWidgetState::Door::Opening, nowMs());
    m_d3dRenderer.MarkRedrawNeeded();

    // The keep-alive spans the whole modal picker (including its nested
    // IFileOpenDialog when the user clicks Browse...), animating the door
    // and keeping the printer preview live behind the dialog.
    m_host->BeginModalKeepAlive();

    hrBrowse = m_windowCommandManager->PromptInsertDiskMru (drive + 1, mountStarted);
    IGNORE_RETURN_VALUE (hrBrowse, S_OK);

    m_host->EndModalKeepAlive();

    // No-mount path (cancel or failure): the door follows the mount
    // state -- a mounted drive closes back, an empty drive rests open
    // (matches a real Disk II). mountStarted is the discriminator
    // because a cancel returns S_OK by design. When a mount DID start,
    // the door is left alone: the queued mount completes on the CPU
    // thread and BeginInsert runs the close choreography.
    if (!mountStarted && pSt->IsMounted())
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
    HRESULT      hr         = S_OK;
    HRESULT      hrActivate = S_OK;
    HRESULT      hrSave     = S_OK;
    std::string  resolved   = themeName;



    CBRA (m_themeManager);                       // null member = Casso bug
    BAIL_OUT_IF (themeName.empty(), S_OK);        // no theme requested -> no-op

    hrActivate = m_themeManager->Activate (themeName);
    if (FAILED (hrActivate))
    {
        resolved   = "Skeuomorphic";
        hrActivate = m_themeManager->Activate (resolved);
    }

    // Live guard now. Previously Activate reported "no such theme" as
    // S_FALSE, so CHR treated it as success and this function went on to
    // persist a theme name that never activated.
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



    CBRA (m_themeManager);                       // null member = Casso bug
    BAIL_OUT_IF (themeName.empty(), S_OK);        // no theme requested -> no-op

    hrActivate = m_themeManager->Activate (themeName);
    if (FAILED (hrActivate))
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





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::NotifyUser
//
//  The EHM notification sink. Forwards to the live shell; if there is none
//  (teardown), the message is dropped rather than crashing on a stale
//  pointer -- an error report is not worth taking the process down for.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::NotifyUser (const wchar_t * message)
{
    if (message == nullptr)
    {
        return;
    }

    // No shell yet (startup) or none left (teardown): hold the text rather
    // than drop it. Everything raised this early is replayed the moment there
    // is a window to show it in.
    if (s_pNotifyShell == nullptr)
    {
        QueueNotification (message);
        return;
    }

    s_pNotifyShell->ShowNotification (message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::QueueNotification
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::QueueNotification (const std::wstring & message)
{
    std::lock_guard<std::mutex>  guard (s_pendingNotifyMutex);



    s_pendingNotifications.push_back (message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowNotification
//
//  Shows one notification as a themed modal. Callable from any thread.
//
//  Three cases, in order. With no window yet the text is queued for
//  FlushPendingNotifications -- startup reports a bad prefs file before
//  there is anything to parent a dialog to. Off the UI thread it is posted,
//  because the dialog is Dxui and Dxui asserts UI-thread affinity; a flush
//  failing on the CPU thread takes that path. Otherwise it is shown here.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowNotification (const std::wstring & message)
{
    DialogDefinition  def;
    bool              isOffThread = false;



    if (m_hwnd == nullptr)
    {
        EmulatorShell::QueueNotification (message);
        return;
    }

    isOffThread = (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

    if (isOffThread)
    {
        wstring *  carried = new (std::nothrow) wstring (message);

        if (carried != nullptr && !PostMessageW (m_hwnd, WM_APP_NOTIFY_USER, 0,
                                                 reinterpret_cast<LPARAM> (carried)))
        {
            delete carried;
        }

        return;
    }

    def.title = L"Casso";
    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { message, false, wstring() });
    def.buttons.push_back (DialogButton { L"OK", 0, true, true, false });

    ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FlushPendingNotifications
//
//  Replays anything reported before the window existed. The queue is drained
//  under the lock and shown outside it, because showing is modal and holding
//  a lock across a nested message loop invites a deadlock with a CPU-thread
//  notification arriving mid-dialog.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::FlushPendingNotifications()
{
    std::vector<std::wstring>  pending;
    size_t                     i = 0;



    {
        std::lock_guard<std::mutex>  guard (s_pendingNotifyMutex);

        pending.swap (s_pendingNotifications);
    }

    for (i = 0; i < pending.size(); i++)
    {
        ShowNotification (pending[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowSalvageDialog
//
//  Shows a dialog whose body is a caller-built panel instead of wrapped text
//  runs. The salvage dialog needs a figures table and a warning banner, and
//  neither survives being expressed as a string: a table spaced with padding
//  characters comes apart at any font or DPI other than the author's.
//
//  Wider than the standard dialog because the table has three columns.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowSalvageDialog (const DialogDefinition             &  def,
                                      std::unique_ptr<SalvageDialogContent>  content)
{
    constexpr int  s_kDialogWidthDip  = 520;
    constexpr int  s_kChromeHeightDip = 108;   // caption + content pad*2 + button row
    constexpr int  s_kMinHeightDip    = 160;
    constexpr int  s_kMaxHeightDip    = 620;



    MessageDialog                       dlg;
    DxuiWindow::CreateParams            params;
    std::vector<MessageDialog::Button>  buttons;
    HRESULT                             hr        = S_OK;
    int                                 heightDip = 0;
    int                                 result    = -1;



    CBRAEx (content != nullptr, E_INVALIDARG);

    // The panel measures itself once laid out; until then its preferred
    // height is the estimate it reported for the width we are about to give
    // it, which is why the width is fixed above rather than derived.
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
//  EmulatorShell::IsSalvageOffered
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsSalvageOffered (int drive)
{
    // Reads the verdict reached at mount rather than re-deriving it. This runs
    // from the menu's enable query, so it runs on every draw of that menu:
    // assessing here cost 11 ms for an ordinary disk and 154 ms for a
    // copy-protected one, per drive, on the UI thread.
    return m_diskStore.IsSalvageOffered (6, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RunSalvageFlow
//
//  Assess, show the figures, write on confirmation, then offer to insert the
//  copy. The assessment is shown BEFORE anything is written: a lossy copy is
//  the user's decision to make with the numbers in front of them, not one to
//  learn about afterwards.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunSalvageFlow (int drive)
{
    SalvageAssessment                       assessment;
    DenibblizeReport                        report;
    DialogDefinition                        def;
    std::unique_ptr<SalvageDialogContent>   content;
    HRESULT                                 hr          = S_OK;
    int                                     choice      = 0;
    bool                                    isOffThread = false;
    std::wstring                            sourcePath;
    std::wstring                            destName;
    std::wstring                            summary;



    // The Disk menu dispatches on the CPU thread and this builds a Dxui modal.
    // Reached from the damage prompt it is already on the UI thread; reached
    // from the menu it was not, so the dialog never appeared and the command
    // looked like it did nothing at all.
    isOffThread = (m_hwnd != nullptr) &&
                  (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

    if (isOffThread)
    {
        PostMessageW (m_hwnd, WM_APP_RUN_SALVAGE, (WPARAM) drive, 0);
        return;
    }

    hr = m_diskStore.AssessSalvage (6, drive, assessment);
    if (FAILED (hr))
    {
        return;
    }

    if (!assessment.isOffered)
    {
        return;
    }

    sourcePath = fs::path (m_diskStore.GetSourcePath (6, drive)).wstring();
    destName   = fs::path (assessment.suggestedPath).filename().wstring();

    content = std::make_unique<SalvageDialogContent>();
    content->SetAssessment (sourcePath, destName, assessment);

    def.title = L"Salvage readable sectors";
    def.buttons.push_back (DialogButton { L"Salvage", 1, true,  false, false });
    def.buttons.push_back (DialogButton { L"Cancel",  0, false, true,  false });

    choice = ShowSalvageDialog (def, std::move (content));
    if (choice != 1)
    {
        return;
    }

    hr = m_diskStore.SalvageToFile (6, drive, assessment.suggestedPath, report);

    if (FAILED (hr))
    {
        DialogDefinition  failed;

        failed.title = L"Could not write the salvaged copy";
        failed.icon  = DialogIcon::Error;
        failed.body.push_back (DialogTextRun {
            fs::path (assessment.suggestedPath).wstring() + L"\n\n"
            L"The original disk image was not changed.\n\n" +
            WindowCommandManager::FormatSystemError (hr), false, std::wstring() });
        failed.buttons.push_back (DialogButton { L"OK", 0, true, true, false });

        ShowModalDialog (failed);
        return;
    }

    // Result first, then the question: the counts are what happened, not part
    // of the prompt.
    summary = L"Salvaged copy written to:\n\n" +
              fs::path (assessment.suggestedPath).wstring() + L"\n\n" +
              std::to_wstring (report.sectorsRecovered) + L" sectors recovered, " +
              std::to_wstring (report.sectorsLost) + L" lost. The original disk "
              L"image was not changed.\n\n"
              L"Insert the salvaged copy into drive " + std::to_wstring (drive + 1) + L"?";

    def = DialogDefinition();
    def.title = L"Salvage complete";
    def.body.push_back (DialogTextRun { summary, false, std::wstring() });
    def.buttons.push_back (DialogButton { L"Insert",  1, true,  false, false });
    def.buttons.push_back (DialogButton { L"Not now", 0, false, true,  false });

    choice = ShowModalDialog (def);

    if (choice == 1)
    {
        HRESULT  hrMount = m_diskManager->MountDiskInSlot6 (drive, assessment.suggestedPath);

        IGNORE_RETURN_VALUE (hrMount, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReportDamagedMount
//
//  The damage report, with salvage offered inline so the dialog is not a dead
//  end. Reached after every mount; silent unless the image failed its stored
//  checksum.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReportDamagedMount (int drive)
{
    DiskImage          * image       = m_diskStore.GetImage (6, drive);
    SalvageAssessment    assessment;
    DialogDefinition     def;
    HRESULT              hr          = S_OK;
    int                  choice      = 0;
    bool                 isOffThread = false;



    if (image == nullptr)
    {
        return;
    }

    // Mounts run on the CPU thread -- the picker and the menu both route
    // through it so a flush never races the drive engine -- and this raises a
    // modal. Bounce to the UI thread rather than building a dialog from there.
    isOffThread = (m_hwnd != nullptr) &&
                  (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

    if (isOffThread)
    {
        PostMessageW (m_hwnd, WM_APP_REPORT_DAMAGE, (WPARAM) drive, 0);
        return;
    }

    if (!image->HasSourceCrcMismatch())
    {
        return;
    }

    def.title = L"Disk image is damaged";
    def.icon  = DialogIcon::Warning;
    // The sentence leads and the path follows it: naming the file before
    // saying anything about it makes the reader hold a path in mind with no
    // reason to yet.
    def.body.push_back (DialogTextRun {
        L"This disk image's stored checksum does not match its contents. "
        L"The file is damaged or was written by a tool that miscomputed it.\n\n" +
        fs::path (m_diskStore.GetSourcePath (6, drive)).wstring() + L"\n\n"
        L"Casso has loaded it so you can read it, and has write-protected it "
        L"for this session. Rewriting the file would give it a newly computed "
        L"checksum, silently hiding the damaged sectors.",
        false, std::wstring() });

    hr = m_diskStore.AssessSalvage (6, drive, assessment);

    if (SUCCEEDED (hr) && assessment.isOffered)
    {
        def.buttons.push_back (DialogButton { L"Salvage readable sectors...", 1,
                                              false, false, false });
    }

    def.buttons.push_back (DialogButton { L"OK", 0, true, true, false });

    choice = ShowModalDialog (def);

    if (choice == 1)
    {
        RunSalvageFlow (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsWriteProtectToggleOffered
//
//  Whether the Disk menu should offer the write-protect toggle for a drive.
//  Reads the bay, then defers to the pure predicate so the rule itself stays
//  testable without a mounted store behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsWriteProtectToggleOffered (int drive)
{
    const DiskImage *  image   = m_diskStore.GetImage (6, drive);
    bool               mounted = m_diskStore.IsMounted (6, drive);



    if (image == nullptr)
    {
        return false;
    }

    return ShouldEnableWriteProtectMenuItem (mounted, image->GetWriteProtectInfo());
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowModalDialog
//
//  Every modal in Casso goes through the Dxui host path. Kept as its own
//  entry point so callers name the intent rather than the renderer.
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
    constexpr int       s_kDialogWidthDip   = 440;
    constexpr int       s_kChromeHeightDip  = 108;   // caption + content pad*2 + button row
    constexpr int       s_kMinHeightDip     = 120;
    constexpr int       s_kMaxHeightDip     = 620;
    constexpr int       s_kIconSrcPx        = 256;
    constexpr int       s_kDefaultIconDip   = 48;
    constexpr int       s_kGlyphSizeDip     = 32;
    constexpr wchar_t   s_kchGlyphInfo      = L'\uE946';   // MDL2 Info
    constexpr wchar_t   s_kchGlyphWarning   = L'\uE7BA';   // MDL2 Warning
    constexpr wchar_t   s_kchGlyphError     = L'\uEA39';   // MDL2 ErrorBadge
    constexpr uint32_t  s_kGlyphArgbInfo    = 0xFF4A9EDB;
    constexpr uint32_t  s_kGlyphArgbWarning = 0xFFF5A623;
    constexpr uint32_t  s_kGlyphArgbError   = 0xFFE5424D;



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
        HRESULT                hrIcon  = S_OK;


        hrIcon = LoadIconAsPremulBgra (m_hInstance, iconRes, s_kIconSrcPx, iconPixels, iconW, iconH);

        if (SUCCEEDED (hrIcon))
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
    // Bottom drive-bar thickness, full and compact. Layout: drive widget
    // (body + label strip + 2 dp bottom margin) bottom-anchored, with a
    // ~43 dp band above for the joystick-mode toggle button (8 dp gap +
    // ~27 dp button + 8 dp gap). Drive widget total height is body 160 +
    // label-strip gap 2 + label strip 18 = 180 dp (full) / 60 dp (compact).
    // With the desk scene on, SyncChromeBands scales the widget portion by
    // m_chromeSceneScale (s_kDeskDriveScale at 100%), so the band hugs the
    // scaled drives without a separate constant.
    constexpr int  s_kFullDriveBarDp    = 225;
    constexpr int  s_kCompactDriveBarDp = 105;



    int   desiredThicknessDp = theme.compactDrives ? s_kCompactDriveBarDp : s_kFullDriveBarDp;
    int   priorThicknessDp   = m_driveBarThicknessDp;
    RECT  rcClient           = {};
    RECT  rcWindow           = {};
    int   centerW            = 0;
    int   centerH            = 0;
    bool  canResize          = false;



    m_driveChrome[0].SetCompact (theme.compactDrives);
    m_driveChrome[1].SetCompact (theme.compactDrives);

    // The device selector's glyph style follows the drive style --
    // full skeuomorphic themes get the 3/4 perspective peripherals, compact
    // (DarkModern / retro) themes the top-down glyphs.
    m_joystickButton.SetSkeuoStyle   (!theme.compactDrives);
    m_toolbar.SetInputSkeuoStyle     (!theme.compactDrives);

    // Push the nav/dropdown palette onto the menu bar so both the
    // in-window strip and the popup-backed dropdown render with chrome
    // colors (the old per-frame apply path is dead post-T129).
    m_mainMenu.ApplyChromeColors (theme);

    // Every path applies the new thickness; only the window resize is
    // conditional. Min/max/fullscreen windows are skipped because the user
    // explicitly chose that state and should not see the window resize from
    // under them on a theme swap -- the thickness still lands, so the next
    // normal-state resize uses the right math. Short-circuit order matters:
    // the Is* / Get* calls must not run on a null HWND.
    canResize = m_hwnd != nullptr
                && desiredThicknessDp != priorThicknessDp
                && !IsIconic (m_hwnd)
                && !IsZoomed (m_hwnd)
                && !m_d3dRenderer.IsFullscreen()
                && GetClientRect (m_hwnd, &rcClient)
                && GetWindowRect (m_hwnd, &rcWindow);

    if (canResize)
    {
        // Capture the current center (emulator viewport) size BEFORE
        // mutating the drive-bar thickness -- ComputeViewportRect reads it.
        // The user may have resized the window manually since boot;
        // preserving "the emu viewport stays the same size, the drive bar
        // grows/shrinks around it" is the intuitive contract on a theme swap.
        RECT  before = ComputeViewportRect (rcClient.right  - rcClient.left,
                                            rcClient.bottom - rcClient.top);

        centerW = before.right  - before.left;
        centerH = before.bottom - before.top;
    }

    // Sits between the two blocks on purpose: the capture above needs the old
    // thickness, the sizing below needs the new one.
    m_driveBarThicknessDp = desiredThicknessDp;

    if (canResize)
    {
        SIZE  newClient   = ClientSizeForCenterPx (centerW, centerH);
        int   ncOverheadH = (rcWindow.bottom - rcWindow.top) - (rcClient.bottom - rcClient.top);
        int   ncOverheadW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
        int   newWindowW  = (int) newClient.cx + ncOverheadW;
        int   newWindowH  = (int) newClient.cy + ncOverheadH;

        SetWindowPos (m_hwnd, nullptr, 0, 0, newWindowW, newWindowH,
                      SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }

    // Re-run the authoritative layout unconditionally. The resize above only
    // produces a WM_SIZE when the window size actually CHANGES -- a swap
    // whose band delta nets out (compact bar vs the scene's joystick-only
    // bar), a maximized window, or fullscreen all skip it, and the 3D desk
    // scene depends on this pass: its composition only computes here, so
    // skipping it leaves a stale camera (drives off-screen) and the
    // outgoing theme's widgets still laid out. Idempotent when WM_SIZE
    // already ran it.
    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        (void) OnSize ((UINT) (rcClient.right - rcClient.left),
                       (UINT) (rcClient.bottom - rcClient.top));
        m_d3dRenderer.MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetCrtMonitorEnabled
//
//  Settings > Theme opt in/out for the CRT monitor -- the escape hatch back to
//  the flat picture at classic sizes (the 3D drives stay either way).
//  Persists the choice, then re-runs the authoritative OnSize layout at the
//  current client size so the monitor appears or disappears in place -- the
//  window itself does not resize; Ctrl+0 reaches the mode's 100% default.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetCrtMonitorEnabled (bool enabled)
{
    HRESULT  hr       = S_OK;
    RECT     rcClient = {};



    BAIL_OUT_IF (m_globalPrefs.crtMonitor == enabled, S_OK);

    m_globalPrefs.crtMonitor = enabled;

    if (m_userConfigStore != nullptr)
    {
        hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    }
    else
    {
        hr = m_globalPrefs.Save (m_assetBaseDir, m_uiFs);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        (void) OnSize ((UINT) (rcClient.right - rcClient.left),
                       (UINT) (rcClient.bottom - rcClient.top));
        m_d3dRenderer.MarkRedrawNeeded();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetSceneAntiAliasing
//
//  The scene's multisampling, in samples (1 / 2 / 4). Applies to the very next
//  frame -- the renderer drops its offscreen targets and rebuilds them at the
//  new count -- so the user sees the trade they just made without restarting.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetSceneAntiAliasing (int samples)
{
    HRESULT  hr     = S_OK;
    int      wanted = (samples >= 4) ? 4 : ((samples >= 2) ? 2 : 1);



    BAIL_OUT_IF (m_globalPrefs.sceneAntiAliasing == wanted, S_OK);

    m_globalPrefs.sceneAntiAliasing = wanted;

    ApplySceneAntiAliasing();

    if (m_userConfigStore != nullptr)
    {
        hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    }
    else
    {
        hr = m_globalPrefs.Save (m_assetBaseDir, m_uiFs);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    m_d3dRenderer.MarkRedrawNeeded();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplySceneAntiAliasing
//
//  Pushes the stored count at every renderer that draws the scene. Separate
//  from the setter because startup has to apply it too, without re-saving the
//  file it was just read from.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplySceneAntiAliasing()
{
    m_deskScene.SetSampleCount ((UINT) m_globalPrefs.sceneAntiAliasing);
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



    // With the desk scene active the selector's home is the command toolbar
    // and there is no band for it here. This is the one choke point every
    // caller funnels through -- init, OnSize, theme apply, the cached DPI
    // relayout -- so the gate lives HERE: gating the call sites one by one
    // is how the retired widget kept resurrecting at the window's bottom
    // edge whenever a path nobody remembered re-laid it.
    if (DeskSceneActive())
    {
        m_joystickButton.Hide();
        return;
    }



    m_joyBtnClientW    = clientW;
    m_joyBtnBandTop    = bandTopPx;
    m_joyBtnBandHeight = bandHeightPx;
    m_joyBtnDpi        = dpi;

    scaler.SetDpi (dpi);
    SyncSelectorState();
    m_joystickButton.Layout (anchor, scaler);
    // m_joystickTooltip is a deferred popup: it derives its DPI from its
    // popup host (set via SetPopupHost) at show time, so no SetDpi here.
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RelayoutJoystickButton
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RelayoutJoystickButton()
{
    DXUI_ASSERT_UI_THREAD();   // measures text through the Dxui renderer

    if (m_joyBtnClientW <= 0)
    {
        return;
    }

    LayoutJoystickButton (m_joyBtnClientW, m_joyBtnBandTop, m_joyBtnBandHeight, m_joyBtnDpi);
}




// (LayoutPrinterIndicator deleted: the standalone printer indicator is
// retired -- the toolbar's Printer button carries the status LED now.)





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowPrinterPanel
//
//  Lazily creates the printer panel / print preview window, wires its toolbar
//  callbacks to the existing delivery commands, pushes a fresh strip snapshot,
//  and brings it to the foreground. Mirrors ShowDisk2Debug's create pattern.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowPrinterPanel (bool activate)
{
    HRESULT     hr        = S_OK;
    HINSTANCE   hInstance = nullptr;



    DXUI_ASSERT_UI_THREAD();   // creates / shows a Dxui window


    if (m_printerPanel == nullptr || m_printerPanel->Hwnd() == nullptr)
    {
        hInstance      = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_printerPanel = std::make_unique<PrinterPanel> ();

        // No owner window: the preview is a peer of the main window, not an
        // owned popup. An owned window is permanently z-locked above its owner
        // (always-on-top of Casso); a peer can be sent behind Casso normally.
        hr = m_printerPanel->Create (hInstance,
                                     nullptr,
                                     m_d3dRenderer.GetDevice(),
                                     m_d3dRenderer.GetContext(),
                                     &m_chromeTheme);
        CHRF (hr, m_printerPanel.reset());

        ApplyAppIconToWindow (m_printerPanel->Hwnd());

        // Toolbar actions route through the existing command path (which
        // quiesces the worker, delivers/clears, and resumes), then re-snapshot.
        // Print / Save / Copy are non-destructive: they deliver the strip and
        // leave the paper in the printer, so one printout can be printed AND
        // saved AND copied. Discard is the one tear-off.
        m_printerPanel->SetOnPrint ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_PRINT);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnSaveAs ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_SAVEAS);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnCopy ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_COPY);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnDiscard ([this] ()
        {
            // The tear-off sound fires from the confirmed branch of the discard
            // handler (WindowCommandManager), NOT here -- so canceling the
            // confirmation dialog does not rip a page we are keeping.
            m_windowCommandManager->HandleCommand (IDM_PRINTER_DISCARD);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnFormFeed ([this] ()
        {
            int    rowsOnPage = 0;
            float  unused     = 0.0f;

            // The real ImageWriter's FORM FEED button, honored only while
            // the printer is idle -- pressing it mid-print would interleave
            // a page break into the guest's own stream, so it is ignored
            // (same idle signal that re-arms the auto-open logic).
            static constexpr int64_t   s_kFormFeedIdleMs = 1200;

            int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                  std::chrono::steady_clock::now().time_since_epoch()).count();

            if (nowMs - m_printerActiveLastMs < s_kFormFeedIdleMs)
            {
                return;   // an actual print is streaming: ignore the button
            }

            // Scale the form-feed sound by how much of the current page will
            // feed to the tear bar (less unused -> shorter feed -> shorter
            // grain). A page that just wrapped feeds a full sheet (unused ~1).
            rowsOnPage = m_printerWorker.RowsUsed() % PrinterGrid::kPageRows;
            unused = 1.0f - (float) rowsOnPage / (float) PrinterGrid::kPageRows;
            m_printerAudio.PlayFormFeed (unused);

            m_printerWorker.FormFeed();
        });

        // Dragging the preview's caption or edge enters the OS modal move/size
        // loop, which owns the UI thread and would otherwise freeze the print
        // mid-page (the carriage stops, the reveal stalls) until the drag ends.
        // Pump a full host frame per loop tick so the emulator keeps running
        // and the carriage keeps animating while the user repositions the
        // window -- the same keep-alive the main window uses for its caption.
        m_printerPanel->SetOnModalLoopTick ([this] ()
        {
            TryPresentUiFrame();
        });
    }

    SnapshotStripToPanel();

    // activate=false (auto-open path) shows the preview without pulling focus
    // off the guest, so a print popping up the window never eats keystrokes.
    m_printerPanel->Show (activate);

    // Auto-open sits the preview JUST BELOW the main window in the z-order, not
    // on top: a print arriving while the user is working must never pop a window
    // over what they are looking at (or steal focus from under them). The user
    // clicks / Alt-Tabs it forward whenever they want to watch it.
    if (!activate && m_printerPanel->Hwnd() != nullptr)
    {
        SetWindowPos (m_printerPanel->Hwnd(), m_hwnd, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PrinterDialogOwner
//
//  Owner HWND for the printer's confirmation / notice boxes. When the preview
//  panel is open the user is acting inside it (its Finish / Copy / Discard
//  buttons, or a menu command while watching it), so own the box by the panel
//  -- the modal box then centers on the panel and disables it while up. With
//  the panel closed the command came from the main menu, so own it by the main
//  window.
//
////////////////////////////////////////////////////////////////////////////////

HWND EmulatorShell::PrinterDialogOwner() const
{
    HWND  owner       = m_hwnd;
    bool  panelIsUp   = m_printerPanel != nullptr
                        && m_printerPanel->IsOpen()
                        && m_printerPanel->Hwnd() != nullptr
                        && IsWindowVisible (m_printerPanel->Hwnd());



    if (panelIsUp)
    {
        owner = m_printerPanel->Hwnd();
    }

    return owner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyAppIconToWindow
//
//  Give a child DxuiWindow the Casso icon so Alt-Tab / the taskbar show the
//  Casso motif rather than a generic window icon. The borderless Dxui panels do
//  not inherit the WNDCLASS icon and Alt-Tab reads the window's WM_GETICON, so
//  the big + small icons are attached explicitly. LR_SHARED handles are managed
//  by the system, so there is nothing to free.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyAppIconToWindow (HWND target)
{
    HINSTANCE   hInstance = nullptr;
    HICON       iconBig   = nullptr;
    HICON       iconSmall = nullptr;



    if (target == nullptr)
    {
        return;
    }

    hInstance = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));

    iconBig   = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO), IMAGE_ICON,
                                    GetSystemMetrics (SM_CXICON), GetSystemMetrics (SM_CYICON),
                                    LR_DEFAULTCOLOR | LR_SHARED);
    iconSmall = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO), IMAGE_ICON,
                                    GetSystemMetrics (SM_CXSMICON), GetSystemMetrics (SM_CYSMICON),
                                    LR_DEFAULTCOLOR | LR_SHARED);

    if (iconBig != nullptr)
    {
        SendMessageW (target, WM_SETICON, ICON_BIG, (LPARAM) iconBig);
    }

    if (iconSmall != nullptr)
    {
        SendMessageW (target, WM_SETICON, ICON_SMALL, (LPARAM) iconSmall);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SnapshotStripToPanel
//
//  Force-refreshes the panel from the drain worker WITHOUT stopping it: the
//  panel snapshots only its visible viewport span under the worker's raster
//  lock while the same interpreter keeps running. Fully non-destructive --
//  previewing (or refreshing) mid-print can never reset the guest's in-flight
//  state, so it cannot distort the output. (The original path stopped and
//  re-Start()ed the worker, which rebuilt the interpreter and reset its line
//  feed from Print Shop's ESC T back to the default, stretching everything
//  printed after a mid-print preview.)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SnapshotStripToPanel()
{
    int64_t   nowMs      = 0;
    bool      panelIsUp  = m_printerPanel != nullptr && m_printerPanel->IsOpen();
    bool      hasCard    = m_refs.printerCard != nullptr;



    if (panelIsUp && !hasCard)
    {
        PrintRaster   empty;

        m_printerPanel->SetStrip (empty);   // blank sheet
    }
    else if (panelIsUp)
    {
        nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                    std::chrono::steady_clock::now().time_since_epoch()).count();

        // Forced refresh through the panel's viewport: snapshots and renders
        // only the visible ~1-page span (never the whole strip), same as the
        // live path.
        m_printerPanel->RefreshLive (m_printerWorker, nowMs, true /* force */);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdatePrinterStatus
//
//  Samples the worker's thread-safe status signals, recomputes the LED state
//  through the pure PrinterStatusModel, feeds the toolbar's printer button,
//  and marks a redraw only on a change so a static screen still repaints the
//  LED on a transition.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePrinterStatus()
{
    int64_t        nowMs  = 0;
    PrinterStatus  status = PrinterStatus::Idle;



    if (m_refs.printerCard == nullptr)
    {
        m_toolbar.SetPrinterPresent (false);
        return;   // no card: the toolbar's printer button disables
    }

    m_toolbar.SetPrinterPresent (true);

    nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                std::chrono::steady_clock::now().time_since_epoch()).count();

    // A latched delivery error clears itself when the guest prints something
    // new -- red means "this page needs attention", and a fresh print means
    // the user has moved on (Error outranks Receiving in the model, so a
    // stale latch would otherwise mask the live print).
    if (m_printerDeliveryError &&
        m_printerWorker.ActivityCount() != m_printerErrorActivity)
    {
        m_printerDeliveryError = false;
    }

    m_printerStatus.Update (m_printerWorker.ActivityCount(),
                            (double) nowMs,
                            m_printerWorker.HasContent(),
                            m_printerDeliveryError);

    status = m_printerStatus.Status();

    // The toolbar's printer button carries the status light (DCR-2); repaint
    // only when the LED state actually changes.
    if (status != m_printerStatusShown)
    {
        m_printerStatusShown = status;
        m_toolbar.SetPrinterStatus (status);
        m_d3dRenderer.MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdatePrinterPreview
//
//  Per-frame: auto-open the preview the moment the guest starts printing, then
//  refresh the strip live as bytes flow. The read is non-destructive (see
//  SnapshotStripToPanel), so watching a print in progress never perturbs it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePrinterPreview()
{
    static constexpr int64_t   s_kAutoOpenIdleMs = 1200;   // activity gap that re-arms auto-open



    HRESULT    hr        = S_OK;
    uint64_t   activity  = 0;
    int64_t    nowMs     = 0;
    bool       previewUp = false;

    BAIL_OUT_IF (m_refs.printerCard == nullptr, S_OK);   // machine has no printer card

    activity = m_printerWorker.ActivityCount();
    nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                   std::chrono::steady_clock::now().time_since_epoch()).count();

    // Auto-open on a NEW print session: activity advancing after an idle gap.
    // Arm while idle, fire once (without stealing focus) when bytes resume, then
    // stay disarmed until idle again. This opens for a fresh print even when a
    // prior pending strip is still loaded (no HasContent edge to ride), yet
    // closing the window mid-print never fights a re-open -- activity keeps
    // advancing through the print, so the edge does not re-arm until it finishes.
    if (activity != m_printerAutoOpenActivity)
    {
        if (m_printerAutoOpenArmed)
        {
            ShowPrinterPanel (false /* activate */);
            m_printerAutoOpenArmed = false;
        }

        m_printerAutoOpenActivity = activity;
        m_printerActiveLastMs     = nowMs;
    }
    else if (!m_printerAutoOpenArmed && nowMs - m_printerActiveLastMs > s_kAutoOpenIdleMs)
    {
        m_printerAutoOpenArmed = true;   // print settled: re-arm for the next one
    }

    // Live refresh while the preview is genuinely visible. The panel's viewport
    // does its own change detection and renders at most the visible ~1-page span
    // (FR-033), so this per-frame call is flat-cost regardless of strip length.
    // A hidden panel bails: rendering off-screen buys nothing.
    previewUp = m_printerPanel != nullptr
                && m_printerPanel->IsOpen()
                && IsWindowVisible (m_printerPanel->Hwnd());

    BAIL_OUT_IF (!previewUp, S_OK);

    m_printerPanel->RefreshLive (m_printerWorker, nowMs);

    // Feed the paced carriage position to the printer audio so the mechanical
    // sound tracks the on-screen head (Option A), sharing the exact reveal the
    // panel just advanced. The audio thread gates its carriage loop + fires the
    // line-feed clacks off this; a closed / hidden preview stops publishing, so
    // the loop naturally goes quiet.
    {
        int64_t  progressDots   = 0;
        int      colDots        = 0;
        bool     inkActive      = false;
        int      sweepWidthDots = PrinterGrid::kDotsPerRow;
        m_printerPanel->GetPacedReveal (progressDots, colDots, inkActive, sweepWidthDots);
        m_printerAudio.PublishReveal (progressDots, colDots, inkActive, sweepWidthDots);
    }

    // Printer-sound volume + mute (Settings > Printing audio, FR-034). Read from
    // prefs each frame so an OK / Cancel in Settings binds on the next update
    // without any live-apply plumbing; the shared "Drive Audio" master still
    // gates the whole bus above this.
    m_printerAudio.SetVolume (m_globalPrefs.printerAudioVolume);
    m_printerAudio.SetMuted  (!m_globalPrefs.printerAudioEnabled);

    // Position the printer sound in the stereo field. Manual override (Settings >
    // Printing) pins a fixed pan; otherwise it auto-follows where the preview
    // window sits relative to the main Casso window -- center-to-center X offset,
    // normalized so the two windows just touching side by side is a hard pan and
    // a fully overlapping (co-centered) window is dead center.
    {
        float  pan  = 0.0f;
        float  panL = 0.0f;
        float  panR = 0.0f;

        if (m_globalPrefs.printerAudioPanOverride)
        {
            pan = std::clamp (m_globalPrefs.printerAudioPan, -1.0f, 1.0f);
        }
        else
        {
            RECT  mainR    = {};
            RECT  printerR = {};

            if (GetWindowRect (m_hwnd, &mainR) &&
                GetWindowRect (m_printerPanel->Hwnd(), &printerR))
            {
                float  mainCenter    = (float) (mainR.left    + mainR.right)    * 0.5f;
                float  printerCenter = (float) (printerR.left + printerR.right) * 0.5f;
                float  mainHalf      = (float) (mainR.right    - mainR.left)    * 0.5f;
                float  printerHalf   = (float) (printerR.right - printerR.left) * 0.5f;
                float  reference     = mainHalf + printerHalf;   // touching side by side

                if (reference > 1.0f)
                {
                    pan = std::clamp ((printerCenter - mainCenter) / reference, -1.0f, 1.0f);
                }
            }
        }

        DriveAudioMixer::PanToStereo (pan, panL, panR);
        m_printerAudio.SetPan (panL, panR);
    }

    // Hold a smooth present cadence while the carriage is sweeping or a pan/zoom
    // is easing. Without this the loop drops to Sleep(1) whenever the emulator
    // framebuffer is static (a guest that prints without touching the screen),
    // and that coarse, jittery tick makes the head step across the platen.
    if (m_printerPanel->NeedsAnimationFrame())
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LayoutSwitchBar
//
//  Positions the //c case-switch strip over its chrome band. On any other
//  machine the band is zero-height, so the strip is hidden (and un-hit-tested).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutSwitchBar (UINT dpi)
{
    DxuiDpiScaler  scaler;



    if (!IsApple2c())
    {
        m_switchBar.Hide();
        return;
    }

    scaler.SetDpi (dpi);
    SyncSwitchBarState();
    m_switchBar.Layout (m_switchBand.Bounds(), scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSwitchBarState
//
//  Pushes the live //c switch + indicator state onto the strip: the two
//  latching switches are read back from the keyboard device (single source of
//  truth), the disk-use LED tracks slot-6 drive activity, power is always lit
//  while the machine runs, and the reset button is "armed" only while Ctrl is
//  held (real Control-Reset).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSwitchBarState()
{
    Apple2eKeyboard *  iieKbd = m_refs.iieKeyboard;
    bool               diskOn = false;



    if (iieKbd != nullptr)
    {
        m_switchBar.SetEightyFortyIn (iieKbd->IsEightyColumnSwitchIn());
        m_switchBar.SetKeyboardIn    (iieKbd->IsKeyboardSwitchDvorak());
    }

    for (const DriveWidget & drive : m_driveChrome)
    {
        diskOn = diskOn || (drive.Led() == LedState::Active);
    }

    m_switchBar.SetDiskActive (diskOn);
    m_switchBar.SetPowerOn    (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::AuxRamBuffer
//
////////////////////////////////////////////////////////////////////////////////

const Byte * EmulatorShell::AuxRamBuffer() const
{
    return m_machineManager != nullptr ? m_machineManager->GetAuxRamBuffer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HandleSwitchBarClick
//
//  Actions a left-button release over one of the //c switch-strip parts. The
//  reset button is inert unless Ctrl is held (the real //c key does nothing on
//  its own); Open-Apple / Closed-Apple, mapped from the held Alt keys, ride the
//  reset into the firmware for a cold boot / diagnostics. The 80/40 and
//  keyboard switches latch: each click flips the switch state on the keyboard
//  device, which the strip re-reads on the next SyncSwitchBarState.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleSwitchBarClick (Apple2cSwitchBar::Part part)
{
    Apple2eKeyboard *  iieKbd = m_refs.iieKeyboard;



    switch (part)
    {
        case Apple2cSwitchBar::Part::Reset:
            // Only a modifier-qualified press resets, matching the case key.
            if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
            {
                m_refs.keyboard->SetKeyDown (false);
                PostCommand (IDM_MACHINE_RESET);
            }

            break;

        case Apple2cSwitchBar::Part::EightyForty:
            if (iieKbd != nullptr)
            {
                bool  newIn = !iieKbd->IsEightyColumnSwitchIn();

                iieKbd->SetEightyColumnSwitchIn (newIn);
                PersistSwitchState ("eightyColumnSwitch", newIn);
            }

            break;

        case Apple2cSwitchBar::Part::Keyboard:
            if (iieKbd != nullptr)
            {
                bool  newDvorak = !iieKbd->IsKeyboardSwitchDvorak();

                iieKbd->SetKeyboardSwitchDvorak (newDvorak);
                PersistSwitchState ("keyboardDvorak", newDvorak);
            }

            break;

        default:
            break;
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
        m_mainMenu.ClearFocus();
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
    bool  shift       = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    int   dir         = shift ? -1 : 1;
    int   index       = m_chromeFocusIndex;
    bool  exitVk      = (vk == VK_ESCAPE || vk == VK_F10);
    bool  menuIsOpen  = m_mainMenu.IsOpen();
    bool  onMenuTitle = index >= s_kChromeFocusMenuFirst && index <= s_kChromeFocusMenuLast;



    // An open dropdown owns navigation; delegate and reconcile the ring.
    if (menuIsOpen)
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
    }
    else if (exitVk)
    {
        SetChromeFocusIndex (s_kChromeFocusNone);
    }
    else if (vk == VK_TAB)
    {
        SetChromeFocusIndex ((index + dir + s_kChromeFocusCount) % s_kChromeFocusCount);
    }

    // A menu title is focused with its dropdown closed. Left/Right wrap within
    // the titles here rather than walking the whole ring.
    else if (onMenuTitle && vk == VK_LEFT)
    {
        SetChromeFocusIndex ((index == s_kChromeFocusMenuFirst) ? s_kChromeFocusMenuLast : index - 1);
    }
    else if (onMenuTitle && vk == VK_RIGHT)
    {
        SetChromeFocusIndex ((index == s_kChromeFocusMenuLast) ? s_kChromeFocusMenuFirst : index + 1);
    }
    else if (onMenuTitle && (vk == VK_DOWN || vk == VK_RETURN || vk == VK_SPACE))
    {
        m_mainMenu.Open ((MainMenuId) index, true);
    }

    // The joystick-mode button or a drive widget is focused. Left/Right walk
    // the whole ring so horizontal arrows feel natural along the bottom bar.
    else if (vk == VK_LEFT)
    {
        SetChromeFocusIndex ((index - 1 + s_kChromeFocusCount) % s_kChromeFocusCount);
    }
    else if (vk == VK_RIGHT)
    {
        SetChromeFocusIndex ((index + 1) % s_kChromeFocusCount);
    }
    else if (vk == VK_RETURN || vk == VK_SPACE)
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

    // The ring owns every keydown it sees -- an unrecognized key is swallowed
    // rather than leaking through to the guest. See the banner.
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
//  SetDriveUserWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveUserWriteProtect (int drive, bool wp)
{
    DiskImage *  image = nullptr;



    if (drive < 0 || drive >= (int) m_userWriteProtect.size())
    {
        return;
    }

    m_userWriteProtect[(size_t) drive] = wp;

    // Apply to whatever is mounted right now so the toggle takes effect
    // without a remount; a later mount re-applies the standing preference
    // via DiskManager::MountDiskInSlot6.
    image = m_diskStore.GetImage (6, drive);

    if (image != nullptr)
    {
        image->SetUserWriteProtected (wp);
    }
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
//  The UI thread's whole life: drain messages, render one frame, park. The
//  CPU runs on its own thread, so this loop is not clocked by the emulator --
//  it is clocked by vsync and by the frame-ready event the CPU thread raises.
//
//  The event is created BEFORE the CPU thread starts, because the thread
//  begins publishing frames the instant it starts and signaling a null handle
//  would drop the very first one -- which is the one that gets the window
//  painted.
//
//  Loop ordering is deliberate. The settings sheet is destroyed at the TOP of
//  an iteration, not from its own EndDialog callback: that callback runs deep
//  inside DispatchMessage, so resetting the pointer there tears a window down
//  from inside its own message handler.
//
//  Messages are drained to empty before rendering, so a burst of input is
//  absorbed by one frame instead of one frame per message. WM_QUIT breaks out
//  carrying its exit code rather than cleaning up in place -- Stop plus
//  DestroyFrameReadyEvent is the single cleanup path, and duplicating it is
//  how one of the two gets missed.
//
//  The drain is TIME-BOUNDED. "Drain to empty" is unbounded under a sustained
//  input + repaint flood: a Display-slider drag invalidates the settings sheet
//  on every mouse move, the sheet's WM_PAINT is a generated message delivered
//  INSIDE this drain the moment the queue goes quiet, and each such paint runs
//  long enough for the next mouse move to arrive -- so the drain never exits
//  and the emulator present below never runs until the drag pauses. The
//  deadline forces a present at least every s_kMaxDrainMs; leftover messages
//  are simply picked up by the next iteration's drain.
//
//  When TryPresentUiFrame reports nothing was presented, the thread blocks in
//  WaitForFrameOrMessage instead of spinning: an idle BASIC prompt produces
//  no framebuffer changes, and polling it would burn a core for nothing.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::RunMessageLoop()
{
    // Longest one drain pass may run before the loop breaks out to present.
    // See the banner: without this, an input + repaint flood starves the
    // present for as long as the input keeps arriving. 8ms leaves room for a
    // vsynced present in the same ~16ms frame; an uncontended drain still
    // exits on empty-queue long before the deadline.
    constexpr int64_t  s_kMaxDrainMs = 8;



    MSG      msg             = {};
    HRESULT  hr              = S_OK;
    int      exitCode        = 0;
    int64_t  drainDeadlineMs = 0;
    bool     quitting        = false;

    auto  nowMs = []() -> int64_t {
        return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };



    // Auto-reset wake signal the CPU thread raises after each published
    // frame, so the idle UI loop can block on it instead of spin-polling.
    // Must exist before the CPU thread starts publishing.
    m_frameReadyEvent = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    CWRA (m_frameReadyEvent);

    hr = m_cpuManager.Start (
        [this] { OnCpuThreadStart(); },
        [this] (const EmulatorCommand & cmd) { DispatchCpuCommand (cmd); },
        [this] { RunCpuThreadFrame(); },
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

        // Process pending messages, bounded by the drain deadline (see banner):
        // the deadline check gates the NEXT retrieval, so a message already
        // removed from the queue is always dispatched, never dropped.
        drainDeadlineMs = nowMs() + s_kMaxDrainMs;

        while (nowMs() < drainDeadlineMs &&
               PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                // Carry the exit code out rather than tearing down here: the
                // Stop / DestroyFrameReadyEvent pair below is the only cleanup
                // path, and duplicating it is how one of them gets missed.
                exitCode = static_cast<int> (msg.wParam);
                quitting = true;
                break;
            }

            // Title refresh marshaled from a non-UI thread (SwitchMachine
            // runs on the CPU thread; DxuiHwndSource::SetTitle is UI-only).
            // This message is posted only by a completed machine switch, so
            // it doubles as the signal to reflow the chrome for a possible
            // Disk ][ controller add/remove (the window size is unchanged, so
            // no WM_SIZE / OnSize would otherwise re-evaluate it).
            // A notification raised off the UI thread. lParam owns a
            // heap-allocated copy of the text, handed over by ShowNotification.
            // A mount that ran on the CPU thread wants its damage report raised
            // here, where a modal can be built.
            if (msg.message == WM_APP_REPORT_DAMAGE)
            {
                ReportDamagedMount ((int) msg.wParam);
                continue;
            }

            // Likewise the salvage flow: the Disk menu dispatches it from the
            // CPU thread, and it builds a modal.
            if (msg.message == WM_APP_RUN_SALVAGE)
            {
                RunSalvageFlow ((int) msg.wParam);
                continue;
            }

            // One mount's outcome, from whichever thread ran it. lParam owns a
            // heap-allocated copy, handed over by OnMountCompleted. Startup
            // mounts land here too, which is what keeps a bad --disk1 from
            // raising a dialog before this loop existed to run it.
            if (msg.message == WM_APP_MOUNT_COMPLETED)
            {
                MountCompletion *  carried = reinterpret_cast<MountCompletion *> (msg.lParam);

                if (carried != nullptr)
                {
                    HandleMountCompletion (*carried);
                    delete carried;
                }

                continue;
            }

            if (msg.message == WM_APP_NOTIFY_USER)
            {
                wstring *  carried = reinterpret_cast<wstring *> (msg.lParam);

                if (carried != nullptr)
                {
                    ShowNotification (*carried);
                    delete carried;
                }

                continue;
            }

            if (msg.message == WM_APP_DXUI_UPDATE_TITLE)
            {
                UpdateWindowTitle();
                ReflowChromeForMachineChange();

                // A switch may have changed the default pointer mode on the CPU
                // thread (ApplyDefaultPointerForMachine defers its UI reflection
                // here to stay off the CPU thread). Sync the selector state and
                // relayout the joystick button on the UI thread; both are
                // idempotent when nothing changed.
                SyncSelectorState();
                RelayoutJoystickButton();
                continue;
            }

            // Modeless Settings: let the sheet claim its dialog-navigation keys
            // (Tab / Enter / Escape) first (Dxui's IsDialogMessage equivalent).
            if (m_settingsSheet != nullptr && m_settingsSheet->ProcessDialogMessage (msg))
            {
                continue;
            }

            // Suppress the emulator's accelerators while the settings sheet is
            // the active window, so keystrokes meant for it (the color-picker
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

        if (quitting)
        {
            break;
        }

        // One UI render cycle (framebuffer latch + chrome + printer preview /
        // audio + present). TryPresentUiFrame is ALSO driven off a WM_TIMER during an
        // OS modal move / size loop (OnModalLoopTick) so the preview + printer
        // sound keep running while the user holds the title bar. When nothing
        // needs presenting, WaitForFrameOrMessage parks the thread until a frame
        // event or a message arrives instead of spin-sleeping.
        if (!TryPresentUiFrame())
        {
            WaitForFrameOrMessage();
        }
    }

    m_cpuManager.Stop();

Error:
    DestroyFrameReadyEvent();
    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPresentUiFrame
//
//  One UI render cycle, and the answer to "was anything actually presented?"
//  -- which is what lets the caller park the thread instead of spinning.
//
//  Presenting is NOT unconditional. The 9-pass CRT post-process is expensive
//  enough (~20%% of GPU at a static BASIC prompt) that the frame is skipped
//  when neither the emulator framebuffer nor any CRT parameter changed and
//  the persistence trail has finished decaying. Everything in the middle of
//  this function exists to answer that question honestly: each subsystem that
//  is mid-animation calls MarkRedrawNeeded so its frames are not dropped.
//
//  The ones that must vote:
//
//    drive widgets   a door mid-open / mid-close, plus one frame after the
//                    last drive goes idle so the activity LED actually clears
//    open menus      a paused machine produces no framebuffer changes, so
//                    without this a menu opens in state only and looks dead
//    settings sheet  live Display edits must land on the very next present;
//                    otherwise a brightness drag waits for a cursor blink
//
//  CRT parameters are pushed every frame rather than on change, so a slider
//  edit is visible on the next present with no change-tracking to get wrong.
//
//  This also runs off a WM_TIMER during an OS modal move / size loop, which
//  is why the preview and printer audio keep running while the user holds the
//  title bar -- see OnModalLoopTick.
//
//  Presenting goes through InvalidateRect / UpdateWindow rather than a direct
//  Present: the host owns the paint pump, so the framebuffer is staged and a
//  synchronous WM_PAINT drives clear -> composite -> chrome -> present in the
//  one order that puts the chrome on top.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryPresentUiFrame()
{
    HRESULT  hr                        = S_OK;
    bool     didPresent                = false;
    bool     anyDriveLive              = false;
    bool     framebufferDirtyThisFrame = false;
    uint32_t driveSig                  = 0;



    // Copy latest framebuffer under lock, then present with vsync
    {
        lock_guard<mutex> lock (m_framebufferMutex);

        if (m_framebufferReady)
        {
            m_framebufferReady                 = false;
            framebufferDirtyThisFrame = true;
        }
    }

    // / FR-038. Push the latest CRT params (brightness slider,
    // scanlines/bloom/color-bleed toggles + magnitudes) to the
    // renderer every UI frame so user edits land on the very next
    // present. The active theme's `crtDefaults` only apply when the
    // user hasn't customized anything yet (see MakeCrtParams), and they
    // come RESOLVED -- reading the base theme here dropped the machine
    // overrides, so the picture changed brightness whenever a resize let
    // the other caller set the parameters instead.
    {
        const ThemeCrtDefaults *  themeDefaults = nullptr;
        if (m_themeManager != nullptr && m_themeManager->GetActiveTheme() != nullptr)
        {
            themeDefaults = &m_themeManager->ActiveCrtDefaults();
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

    // The capture banner and the fullscreen toolbar reveal, both per-frame
    // because both answer where the pointer is right now.
    SyncCaptureBanner();
    TickFullscreenToolbar();


    for (const DriveWidgetState & st : m_driveWidgetState)
    {
        bool  doorMoving = (st.doorState == DriveWidgetState::Door::Opening ||
                            st.doorState == DriveWidgetState::Door::Closing);
        bool  motorOn    = st.motorOn.load    (memory_order_relaxed);
        bool  diskActive = st.diskActive.load (memory_order_relaxed);

        anyDriveLive = anyDriveLive || motorOn || diskActive;

        // Everything about a drive that is VISIBLE, folded into one word so
        // the present vote below can ask whether it moved rather than whether
        // it is busy.
        driveSig = (driveSig << 3) | (motorOn ? 1u : 0u)
                                   | (diskActive ? 2u : 0u)
                                   | (doorMoving ? 4u : 0u);

        if (doorMoving)
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
    }

    // 3D scene drive visuals: activity lamp, door swing, and the padlock,
    // pushed from the same per-drive state the 2D widgets mirror. The scene
    // only rebuilds geometry when a value actually moved.
    if (DeskSceneActive())
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        for (int i = 0; i < 2; i++)
        {
            const DriveWidgetState &  st       = m_driveWidgetState[i];
            float                     t        = std::clamp ((float) (nowMs - st.animationStartTimeMs) /
                                                             (float) DriveWidgetState::kDoorAnimationMs, 0.0f, 1.0f);
            float                     progress = 0.0f;
            bool                      lampOn   = st.motorOn.load    (memory_order_relaxed) ||
                                                 st.diskActive.load (memory_order_relaxed);

            switch (st.doorState)
            {
                case DriveWidgetState::Door::Open:     progress = 1.0f;     break;
                case DriveWidgetState::Door::Opening:  progress = t;        break;
                case DriveWidgetState::Door::Closing:  progress = 1.0f - t; break;
                case DriveWidgetState::Door::Closed:   progress = 0.0f;     break;
            }

            m_deskScene.SetDriveVisuals (i, lampOn, progress, st.writeProtect.Any());
        }

        // A mount or eject changes the basename strip under the drive, and
        // neither runs a layout pass -- so watch the source paths here and
        // re-hang the labels (with their text measurement) only on a change.
        {
            bool  labelsMoved = false;

            for (int i = 0; i < (int) m_sceneLabelPath.size(); i++)
            {
                std::string  source = m_diskStore.GetSourcePath (6, i);

                if (source != m_sceneLabelPath[i])
                {
                    m_sceneLabelPath[i] = source;
                    labelsMoved         = true;
                }
            }

            if (labelsMoved)
            {
                SyncSceneDriveLabels();
                m_d3dRenderer.MarkRedrawNeeded();
            }
        }
    }

    // Fullscreen drive overlay strip (FR-015): tick the FSM from this
    // frame's observations, apply its capture effects, and compose the slid
    // band the hook will render.
    if (DeskSceneActive() && m_d3dRenderer.IsFullscreen() && DeskSceneDriveCount() > 0)
    {
        StripInputs   inputs;
        StripEffects  effects;
        POINT         cursor  = {};
        RECT          client  = {};
        int64_t       stripNowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                       std::chrono::steady_clock::now().time_since_epoch()).count();

        GetClientRect (m_hwnd, &client);

        inputs.nowMs = stripNowMs;

        if (GetCursorPos (&cursor) && ScreenToClient (m_hwnd, &cursor) && PtInRect (&client, cursor))
        {
            inputs.pointerAtBottomEdge = cursor.y >= client.bottom - m_scaler.Px (s_kStripEdgeZoneDp);
            inputs.pointerOverStrip    = m_stripState.Mode() != StripMode::Hidden &&
                                         PtInRect (&m_stripRectPx, cursor);
        }

        inputs.hotkey        = m_stripHotkeyPending;
        m_stripHotkeyPending = false;

        inputs.pinned        = m_stripBrowseOpen || m_driveTooltip.IsVisible();

        // LIVE, not Active: Mouse mode being CONFIGURED is not the guest
        // owning the pointer. At a BASIC prompt in Mouse mode the host
        // cursor is the only pointer there is, and the bottom edge must
        // summon the strip -- Active gated the reveal off for the whole
        // session on a machine whose mouse is built in.
        inputs.guestPointer  = m_paddleCaptured    ? GuestPointerMode::Paddle
                             : GuestMouseLive()    ? GuestPointerMode::Mouse
                             :                       GuestPointerMode::None;
        inputs.anyDriveActive = anyDriveLive;

        effects = m_stripState.Tick (inputs);

        if (effects.releaseCapture)
        {
            if (m_paddleCaptured)
            {
                StopPaddleCapture();
            }
            else
            {
                m_stripSuppressGuestMouse = true;
            }
        }

        if (effects.restoreCapture == GuestPointerMode::Paddle)
        {
            StartPaddleCapture();
        }
        else if (effects.restoreCapture == GuestPointerMode::Mouse)
        {
            m_stripSuppressGuestMouse = false;
        }

        // The band slides up from the bottom edge: only the top
        // `progress * height` sliver is on-screen mid-animation.
        {
            float  progress = m_stripState.SlideProgress (stripNowMs);
            int    bandH    = m_scaler.Px (s_kStripBandDp);

            if (progress > 0.0f)
            {
                HRESULT  hrStrip = S_OK;

                RECT  driveRow = {};

                m_stripRectPx = { 0, client.bottom - (int) (progress * (float) bandH),
                                  client.right, client.bottom - (int) (progress * (float) bandH) + bandH };

                // The drives get the band LESS the name strip, the way the
                // windowed drive band reserves it: the disk's name and its
                // padlock belong under the drive here too, and a row composed
                // into the whole band would put them off the screen's edge.
                driveRow         = m_stripRectPx;
                driveRow.bottom -= m_scaler.Px (s_kSceneDriveLabelStripDp + s_kSceneDriveLabelGapDp);

                // The drive band's calibrated look-down, not the desk's
                // near-level default: the band angle is what shows the
                // drives' tops, and the fullscreen strip is the same
                // drives-only row the windowed band composes.
                hrStrip = DeskSceneLayout::ComputeStrip (driveRow, m_scaler.Dpi(),
                                                         DeskSceneDriveCount(),
                                                         m_deskScene.Metrics(), m_stripComp,
                                                         DeskSceneLayout::kDriveBandGazeDownRad);
                IGNORE_RETURN_VALUE (hrStrip, S_OK);
            }
            else
            {
                m_stripRectPx = {};
                m_stripComp   = {};
            }
        }

        // The strip's names ride its slide: re-hung every pass so they track
        // the band on its way in and out, and retire with it.
        SyncSceneDriveLabels();

        if (m_stripState.Mode() != StripMode::Hidden || m_stripState.ActivityIndicator())
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
    }
    else
    {
        // Not presenting the strip (windowed, or fullscreen left): never
        // strand a suppressed guest mouse.
        m_stripSuppressGuestMouse = false;
        m_stripRectPx             = {};
    }

    // Keep presenting while the drives' visible state is CHANGING, plus the
    // one frame after it settles so the last change actually reaches the
    // screen. Doors mid-swing vote separately above; this covers the activity
    // lamps.
    //
    // CHANGING, not merely LIVE. The vote used to fire for as long as a motor
    // was energized, and a Disk II motor stays energized until the guest
    // writes $C0E8 -- which plenty of software simply never does once it has
    // loaded. A demo that leaves the drive spinning is faithful hardware
    // behavior, and it pinned Casso at a full 60 fps of nine-pass CRT
    // post-processing forever, over a picture that had not changed in
    // minutes. An LED that is steadily lit is not an animation.
    if (driveSig != m_lastDriveSig || m_driveSigSettling)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    m_driveSigSettling = (driveSig != m_lastDriveSig);
    m_lastDriveSig     = driveSig;

    // //c switch strip: refresh the disk-use LED (drive activity) and the
    // Ctrl-armed reset cue every UI frame so they track live state.
    if (IsApple2c())
    {
        SyncSwitchBarState();
    }

    if (m_disk2DebugPanel != nullptr)
    {
        hr = m_disk2DebugPanel->RenderFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_inputDebugPanel != nullptr)
    {
        hr = m_inputDebugPanel->RenderFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_printerPanel != nullptr)
    {
        hr = m_printerPanel->RenderFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
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

    // Drive the chrome tooltip dwell timers (joystick button, toolbar,
    // //c switch strip, drive widgets); each shows / hides its popup once
    // the open / close delay elapses after a hover.
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_joystickTooltip.Tick  (nowMs);
        m_toolbarTooltip.Tick   (nowMs);
        m_switchBarTooltip.Tick (nowMs);
        m_driveTooltip.Tick     (nowMs);

        // A HELD COMPASS ARROW REPEATS, and a held arrow produces no messages
        // to wake this loop -- the pointer is not moving, which is the very
        // condition the repeat exists for. So it votes for a present the
        // whole time it is held, not only on the frames it fires: without
        // that the loop parks and the repeat stops between steps.
        if (m_sceneCompass.WantsTick())
        {
            m_sceneCompass.Tick (nowMs);

            m_d3dRenderer.MarkRedrawNeeded();
        }
    }

    // Refresh the printer status LED; marks a redraw itself on a change so
    // a static screen (e.g. a pending page at the BASIC prompt) repaints.
    UpdatePrinterStatus();

    // Auto-open the print preview when a print begins and stream the strip
    // into it live as the guest prints (non-destructive snapshot).
    UpdatePrinterPreview();

    didPresent = m_d3dRenderer.NeedsPresent (framebufferDirtyThisFrame);


    if (didPresent)
    {
        // Drive the host paint pump for this frame. Stage the emulator
        // framebuffer for the before-present hook, then request a
        // synchronous WM_PAINT: the host clears, the hook composites the
        // framebuffer, the chrome paints on top, and the host presents.
        m_pendingFramebuffer = framebufferDirtyThisFrame ? m_uiFramebuffer.data() : nullptr;
        InvalidateRect (m_hwnd, nullptr, FALSE);
        UpdateWindow   (m_hwnd);
    }

    return didPresent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnModalLoopTick
//
//  Called by the host on its keep-alive timer WHILE a modal loop owns the UI
//  thread: the OS move / size loop (a title-bar hold or resize-edge drag), or
//  a modal dialog armed through BeginModalKeepAlive (the disk picker in
//  BrowseForDisk). RunMessageLoop is not iterating during those loops, so
//  without this the drive-door animation, the live printer preview, and its
//  paced audio all freeze until the loop ends -- then jump. The host owns the
//  timer; we just render one frame.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnModalLoopTick()
{
    TryPresentUiFrame();
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
        fs::path  baseDir;
        wstring   devicesDir;
        HRESULT   hrLoad     = S_OK;

        // Use the same user-writable asset root that Main.cpp /
        // AssetBootstrap used when writing the WAVs so the read
        // path agrees with the write path.
        baseDir     = AssetBootstrap::GetAssetBaseDirectory();
        devicesDir  = (baseDir / L"Devices" / L"DiskII").wstring();

        m_driveAudioMixer.SetSampleLoadContext (devicesDir, m_wasapiAudio.GetSampleRate());

        hrLoad = m_driveAudioMixer.SetMechanism (m_driveAudioMixer.GetMechanism());
        IGNORE_RETURN_VALUE (hrLoad, S_OK);
    }

    // Load the ImageWriter mechanical sound set (embedded CC BY 4.0 grains that
    // EnsureImageWriterSounds extracted to the asset base). Decodes MP3 via the
    // same Media Foundation path as the Disk II WAVs; a missing grain is silent.
    if (m_wasapiAudio.IsInitialized())
    {
        fs::path  soundsDir = AssetBootstrap::GetAssetBaseDirectory() / L"ImageWriter II Sounds";
        HRESULT   hrSnd     = m_printerAudio.LoadSounds (soundsDir.wstring().c_str(),
                                                         m_wasapiAudio.GetSampleRate());
        IGNORE_RETURN_VALUE (hrSnd, S_OK);
    }

    // The initial machine is built before WASAPI comes up, so its PSGs
    // do not yet know the host sample rate. Seed it now (machine switches
    // after this point pick it up at build time in MachineManager).
    if (m_wasapiAudio.IsInitialized() && m_refs.mockingboard != nullptr)
    {
        m_refs.mockingboard->SetSampleRate (m_wasapiAudio.GetSampleRate());
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
            HRESULT  hrSwitch = S_OK;

            wstring wideName (cmd.payload.begin(), cmd.payload.end());
            hrSwitch = SwitchMachine (wideName);

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
                // StepOne dispatches a pending interrupt vector itself (see
                // the slice loop) -- no separate TryStepInterrupt poll needed.
                m_cpu->StepOne();

                if (m_refs.diskController != nullptr)
                {
                    m_refs.diskController->Tick (m_cpu->GetLastInstructionCycles());
                }

                if (m_refs.mockingboard != nullptr)
                {
                    m_refs.mockingboard->Tick (m_cpu->GetLastInstructionCycles());
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

        case IDM_DISK_WRITEPROTECT1:
        case IDM_DISK_WRITEPROTECT2:
        {
            int   drive = (cmd.id == IDM_DISK_WRITEPROTECT1) ? 0 : 1;
            bool  wp    = (!cmd.payload.empty() && cmd.payload[0] == '1');

            SetDriveUserWriteProtect (drive, wp);
            break;
        }

        case IDM_DISK_WP1:
        case IDM_DISK_WP2:
        {
            int      drive    = (cmd.id == IDM_DISK_WP1) ? 0 : 1;
            HRESULT  hrToggle = S_OK;

            // Runs on the CPU thread like mount / eject so the flush never
            // races the drive engine; failures already reported inside.
            hrToggle = m_diskManager->ToggleImageWriteProtect (drive);
            IGNORE_RETURN_VALUE (hrToggle, S_OK);
            break;
        }

        case IDM_DISK_SALVAGE1:
        case IDM_DISK_SALVAGE2:
        {
            RunSalvageFlow ((cmd.id == IDM_DISK_SALVAGE1) ? 0 : 1);
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
            HRESULT  hrMech = S_OK;

            // Payload is "shugart" or "alps" (canonical lower-case from
            // SettingsPanelState). DriveAudioMixer matches case-insensitively
            // and canonicalizes internally, so hand the token over as-is.
            std::wstring  mechWide (cmd.payload.begin(), cmd.payload.end());
            hrMech = m_driveAudioMixer.SetMechanism (mechWide);

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
//  EmulatorShell::PersistSwitchState
//
//  Writes one //c case-switch latch into the current machine's per-machine
//  $cassoUiPrefs block so the position survives across runs. Best-effort: a
//  missing store / machine name, or a write failure, just leaves the on-disk
//  state as it was.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PersistSwitchState (const char * key, bool value)
{
    HRESULT  hr = S_OK;



    if (m_userConfigStore == nullptr || m_currentMachineName.empty())
    {
        return;
    }

    hr = DiskSettings::WriteSavedUiPrefBool (*m_userConfigStore, m_uiFs, key,
                                             m_currentMachineName, value);
    IGNORE_RETURN_VALUE (hr, S_OK);
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
//  loop sees the framebuffer-dirty flag next iteration and presents.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StepInstructionWhilePaused()
{
    if (m_cpu == nullptr)
    {
        return;
    }

    // StepOne dispatches a pending interrupt vector itself (see the slice
    // loop) -- no separate TryStepInterrupt poll needed.
    m_cpu->StepOne();

    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->Tick (m_cpu->GetLastInstructionCycles());
    }

    if (m_refs.mockingboard != nullptr)
    {
        m_refs.mockingboard->Tick (m_cpu->GetLastInstructionCycles());
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
//  framebuffer under m_framebufferMutex and wakes the UI thread. Skips the whole
//  handoff when the frame is byte-identical to the one last published, so a
//  static screen stops driving the CRT post-process + Present at 60 Hz.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PublishFramebuffer()
{
    HRESULT  hr = S_OK;
    BOOL     ok = FALSE;



    // THE UPSTREAM GATE ANSWERS A DIFFERENT QUESTION. RunCpuThreadFrame asks
    // whether the picture COULD have changed -- a write landed in a display
    // page, the mode moved, the flash phase flipped -- and it is deliberately
    // conservative, because guessing wrong the other way drops a frame the
    // user was waiting on.
    //
    // The flash phase is the one that matters here. It flips about four times
    // a second whatever is on screen, so a full-screen hi-res picture with no
    // text on it at all re-rasterized and republished at 3.7 Hz forever --
    // byte for byte the same image every time. Downstream that was enough to
    // keep resetting the persistence settle counter, and Casso ran the
    // nine-pass CRT chain at a full 60 fps over a still image indefinitely.
    //
    // So the handoff is gated on the RESULT rather than the prediction: a
    // frame identical to the one already published is not published again.
    // The compare is one pass over ~840 KB, and it only runs when the cheap
    // gate upstream already thought something moved.
    {
        lock_guard<mutex>  lock (m_framebufferMutex);

        bool  same = m_uiFramebuffer.size() == m_cpuFramebuffer.size()
                     && memcmp (m_uiFramebuffer.data(), m_cpuFramebuffer.data(),
                                m_cpuFramebuffer.size() * sizeof (uint32_t)) == 0;

        BAIL_OUT_IF (same, S_OK);

        m_uiFramebuffer    = m_cpuFramebuffer;
        m_framebufferReady = true;
    }

    if (m_frameReadyEvent != nullptr)
    {
        ok = SetEvent (m_frameReadyEvent);
        CWRA (ok);
    }

Error:
    return;
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
//  RunCpuThreadFrame
//
//  The CPU thread's per-frame callback. Always advances the emulation
//  (ExecuteCpuSlices). Then, at most ~60 Hz (ShouldPublishFrame throttles
//  Maximum speed), it renders + publishes ONLY when the picture can have
//  changed: the bus raised video-dirty (a write into a display page or a
//  banking change), or the video mode / flash phase / color signature moved.
//  A steady screen re-rasterizes nothing -- just a few cheap comparisons.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunCpuThreadFrame()
{
    uint32_t  modeSig     = 0;
    bool      flashOn     = false;
    uint64_t  colorSig    = 0;
    bool      needsRender = false;



    // Emulation always advances; only the publish is throttled and gated.
    ExecuteCpuSlices();

    if (ShouldPublishFrame())
    {
        modeSig     = ComputeVideoModeSig();
        flashOn     = ComputeFlashOn();
        colorSig    = ComputeColorSig();
        needsRender = m_memoryBus.VideoDirty()
                      || modeSig  != m_lastRenderModeSig
                      || flashOn  != m_lastRenderFlashOn
                      || colorSig != m_lastRenderColorSig;
    }

    if (needsRender)
    {
        RenderFramebuffer();
        PublishFramebuffer();

        m_memoryBus.ClearVideoDirty();
        m_lastRenderModeSig  = modeSig;
        m_lastRenderFlashOn  = flashOn;
        m_lastRenderColorSig = colorSig;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldPublishFrame
//
//  Presentation-side pacing. Below Maximum speed the CPU thread is already
//  paced to one frame per vsync by its waitable timer, so every frame is
//  published. At Maximum speed emulation is unthrottled, so gate the
//  rasterize/publish to a ~60 Hz wall-clock cadence.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ShouldPublishFrame()
{
    SpeedMode                         speed     = m_cpuManager.GetSpeedMode();
    chrono::steady_clock::time_point  now       = chrono::steady_clock::now();
    bool                              shouldPub = false;



    shouldPub = speed != SpeedMode::Maximum ||
                now - m_lastPublishSteady >= chrono::microseconds (s_kMaxSpeedPublishIntervalUs);

    if (shouldPub)
    {
        m_lastPublishSteady = now;
    }

    return shouldPub;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeVideoModeSig
//
//  Packs every soft-switch that changes what the renderer produces into a
//  small integer: the base graphics/mixed/page2/hi-res selects plus the //e
//  80STORE / 80COL / ALTCHARSET / double-hi-res bits. Any change re-renders.
//  Mirrors exactly the inputs MachineManager::SelectVideoMode reads.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::ComputeVideoModeSig()
{
    uint32_t                  sig = 0;
    Apple2eSoftSwitchBank *   iie = m_refs.iieSoftSwitches;



    if (m_refs.softSwitches != nullptr)
    {
        sig |= m_refs.softSwitches->IsGraphicsMode() ? 0x01u : 0u;
        sig |= m_refs.softSwitches->IsMixedMode()    ? 0x02u : 0u;
        sig |= m_refs.softSwitches->IsPage2()        ? 0x04u : 0u;
        sig |= m_refs.softSwitches->IsHiresMode()    ? 0x08u : 0u;

        if (iie != nullptr)
        {
            sig |= iie->Is80Store()     ? 0x10u : 0u;
            sig |= iie->Is80ColMode()   ? 0x20u : 0u;
            sig |= iie->IsAltCharSet()  ? 0x40u : 0u;
            sig |= iie->IsDoubleHiRes() ? 0x80u : 0u;
        }
    }

    return sig;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeFlashOn
//
//  Derives the text flash/cursor-blink phase from emulated time (total CPU
//  cycles), toggling every 16 emulated frames as the real VBL-driven blink
//  does. Computed independently of rendering so the gate can re-render on a
//  toggle without the flash freezing when frames are skipped.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ComputeFlashOn()
{
    uint64_t  cyclesPerToggle = 16ull * m_cyclesPerFrame;
    bool      flashOn         = true;   // no clock yet: show the glyph



    if (m_cpu != nullptr && cyclesPerToggle != 0)
    {
        flashOn = ((m_cpu->GetTotalCycles() / cyclesPerToggle) & 1ull) == 0;
    }

    return flashOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeColorSig
//
//  Folds the monitor color mode and the color-monitor text color into one
//  value so a live change (View menu / Settings) re-renders the frame.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t EmulatorShell::ComputeColorSig()
{
    uint64_t  mode = (uint64_t) m_colorMode.load (memory_order_acquire);
    uint64_t  argb = (uint64_t) m_colorMonitorTextArgb.load (memory_order_acquire);



    return (mode << 32) | argb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WaitForFrameOrMessage
//
//  Idle UI-loop block. Sleeps until the CPU thread signals a new frame OR a
//  Windows message arrives, replacing the old Sleep(1) spin. Caps the wait at
//  a bounded upkeep interval so drive-activity sampling stays live behind a
//  static screen, and drops to a faster tick while a tooltip dwell is pending.
//  Every other animated surface (persistence trail, drive doors, open menus,
//  live Settings edits) forces a present through NeedsPresent and so never
//  reaches this path.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::WaitForFrameOrMessage()
{
    DWORD  timeout = s_kIdleUpkeepMs;
    DWORD  waited  = 0;



    if (m_joystickTooltip.WantsTick()  ||
        m_switchBarTooltip.WantsTick() ||
        m_driveTooltip.WantsTick()     ||
        m_sceneCompass.WantsTick())
    {
        timeout = s_kIdleAnimationTickMs;
    }

    waited = MsgWaitForMultipleObjectsEx (1, &m_frameReadyEvent, timeout,
                                          QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    IGNORE_RETURN_VALUE (waited, 0u);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DestroyFrameReadyEvent
//
//  Closes the frame-ready wake event. Idempotent; the caller must have
//  stopped (joined) the CPU thread first so no SetEvent can race the close.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DestroyFrameReadyEvent()
{
    if (m_frameReadyEvent != nullptr)
    {
        CloseHandle (m_frameReadyEvent);
        m_frameReadyEvent = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteCpuSlices
//
//  One emulated frame's worth of CPU time, cut into ~1023-cycle slices.
//
//  The slice exists for audio, not for the CPU. Samples are generated per
//  slice, so the slice length sets how finely speaker toggles are resolved:
//  one slice per frame would quantize the speaker to 60 Hz and turn Apple II
//  square-wave tones into buzzing. A prime-ish length also keeps the slice
//  boundary from landing on the same instruction every frame.
//
//  Device ticking is deliberately NOT per slice. The Disk II engine is
//  advanced by EACH instruction's cycle count, because the boot ROM sits in a
//  tight LDA $C0EC / BPL loop reading the data latch: at slice granularity it
//  would see roughly one valid nibble per thousand cycles instead of one per
//  thirty-two, never accumulate enough sync bytes to match a sector header,
//  and hang forever on a disk that reads fine.
//
//  StepOne is the whole step -- it polls the interrupt lines itself and
//  substitutes an NMI / IRQ vector for the opcode fetch when one is pending,
//  reporting the cost through GetLastInstructionCycles either way. An outer
//  interrupt poll used to wrap this and was simply a second, redundant poll
//  on every instruction.
//
//  The fractional sample carried in m_sampleRemainder is what keeps audio
//  from drifting: cycles per sample is rarely integral, and truncating it
//  every slice would lose a sample every few frames and slowly desync.
//
//  Double speed doubles the cycle target rather than shortening the frame, so
//  the audio and video cadence stay at their real rates and only the guest
//  runs faster.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ExecuteCpuSlices()
{
    static constexpr uint32_t kSliceCycles = 1023;



    HRESULT   hr              = S_OK;
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
        sliceTarget = targetCycles - executed;

        if (sliceTarget > kSliceCycles)
        {
            sliceTarget = kSliceCycles;
        }

        // Feed the next paste character if available; the slice budget is
        // the guest-time currency the settle pacing is measured in.
        m_clipboardManager->DrainPasteBuffer (sliceTarget);

        sliceActual = 0;

        while (sliceActual < sliceTarget)
        {
            // StepOne polls the interrupt lines itself and dispatches a
            // pending NMI/IRQ vector in place of the opcode fetch (see
            // EmuCpu::StepOne), reporting the cost through
            // GetLastInstructionCycles either way -- so a bare StepOne is the
            // whole step. The former outer TryStepInterrupt was a second,
            // redundant interrupt poll on every instruction.
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

            if (m_refs.mockingboard != nullptr)
            {
                m_refs.mockingboard->Tick (cycles);
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

            hr = m_wasapiAudio.SubmitFrame (m_refs.speaker->GetToggleTimestamps(),
                                            sliceActual,
                                            m_refs.speaker->GetFrameInitialState(),
                                            numSamples,
                                            &m_driveAudioMixer,
                                            m_cpu->GetTotalCycles(),
                                            &m_mockingboardAudioMixer);
            IGNORE_RETURN_VALUE (hr, S_OK);

            m_refs.speaker->ClearTimestamps();
            m_refs.speaker->BeginFrame();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFramebuffer
//
//  Rasterize one emulated video frame into the CPU-side framebuffer: pick the
//  active mode, render it, overlay mixed-mode text, then tint.
//
//  Text color is pushed in rather than baked into the renderer because the
//  same glyph raster serves every monitor type. Color monitors get white text
//  directly; the monochrome modes leave the renderer's green in place and let
//  the tint pass below recolor the entire frame to the chosen phosphor, so
//  green / amber / white monitors need no separate glyph path.
//
//  Flash state is likewise pushed in from emulated time instead of being
//  self-advanced by Render, so a blinking cursor keeps its phase across the
//  frames the render-skip gate drops.
//
//  The dirty-row text cache is force-invalidated in exactly two cases, and
//  both are correctness, not tuning:
//
//    monochrome mode  the tint below is not idempotent, so a row that was
//                     skipped keeps its previous tinted value and darkens a
//                     little more every frame
//    mode change      the buffer last held graphics or another text width, so
//                     no cached row describes what is on screen
//
//  Steady color text hits neither, which is the case that matters -- it lets
//  AppleTextMode redraw only the rows that changed.
//
//  Render is handed a null videoRam so it reads through MemoryBus rather than
//  the CPU's memory array. Only the bus page table reflects live MMU banking
//  ($0400-$07FF and $2000-$3FFF switching between main and aux under 80STORE
//  with PAGE2 / HIRES); the //e MMU re-points those pages at buffers the
//  RamDevice owns, so reading the CPU array directly would show main memory
//  while the guest is displaying aux.
//
//  Mixed mode overlays rows 20-23 through the same RenderRowRange entry point
//  on both the 40- and 80-column renderers, so the split screen is one code
//  path with a width choice rather than two duplicated ones (FR-017a/FR-020).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RenderFramebuffer()
{
    ColorMode  color   = m_colorMode.load (memory_order_acquire);
    bool       flashOn = ComputeFlashOn();



    // Nothing to render before a machine is built, or after one is torn
    // down. The modes are created and cleared together, so text40 answers
    // for all of them and every use below can go straight to m_refs.
    if (m_refs.text40 == nullptr)
    {
        return;
    }

    // A color monitor renders text white; the monochrome monitors keep the
    // text renderer's green here and the post-render tint below recolors the
    // whole frame to the selected phosphor. Flash state is pushed in from
    // emulated time (Render no longer self-advances it) so the blink
    // survives the render-skip gate.
    {
        uint32_t textOnColor = (color == ColorMode::Color)
                                   ? m_colorMonitorTextArgb.load (memory_order_acquire)
                                   : s_kMonoSourceTextBgra;

        m_refs.text40->SetOnColor    (textOnColor);
        m_refs.text40->SetFlashState (flashOn);

        m_refs.text80->SetOnColor    (textOnColor);
        m_refs.text80->SetFlashState (flashOn);

        // Both graphics modes decode from the dots differently per monitor,
        // so they need the monitor type rather than a tint of one decode.
        // In both cases the color decode has already discarded what a
        // monochrome monitor would show -- DHR collapses each 4-dot cell to
        // one palette entry, and hi-res folds the half-dot shift into a
        // color pair -- so no amount of post-tinting brings it back.
        bool monoMonitor = (color != ColorMode::Color);

        m_refs.hiRes->SetMonochrome       (monoMonitor);
        m_refs.doubleHiRes->SetMonochrome (monoMonitor);
    }

    m_machineManager->SelectVideoMode();

    // Dirty-row text cache: force a full re-raster when reusing last frame's
    // rows would be unsafe. (1) A monochrome color mode applies a
    // non-idempotent tint over the whole framebuffer below, so a row we skipped
    // would darken every frame. (2) A change of active mode means the buffer
    // last held graphics / another mode, so no text row can be trusted. Steady
    // color text hits neither and lets AppleTextMode redraw only changed rows.
    {
        bool forceFullText = (color != ColorMode::Color)
                          || (m_refs.activeVideoMode != m_prevActiveVideoMode);

        if (forceFullText)
        {
            m_refs.text40->InvalidateCache();
            m_refs.text80->InvalidateCache();
        }
    }

    m_prevActiveVideoMode = m_refs.activeVideoMode;

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
    if (m_mixedMode && m_graphicsMode)
    {
        static constexpr int kMixedFirstRow = 20;
        static constexpr int kMixedLastRow  = 24;

        bool  use80Col = m_refs.iieSoftSwitches != nullptr
                      && m_refs.iieSoftSwitches->Is80ColMode();

        if (use80Col)
        {
            m_refs.text80->SetPage2 (false);
            m_refs.text80->RenderRowRange (kMixedFirstRow, kMixedLastRow,
                                           nullptr,
                                           m_cpuFramebuffer.data(),
                                           kFramebufferWidth,
                                           kFramebufferHeight);
        }
        else
        {
            m_refs.text40->SetPage2 (m_page2);
            m_refs.text40->RenderRowRange (kMixedFirstRow, kMixedLastRow,
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
//  Shutdown, in the one order that works. Each step here is placed against a
//  lifetime it would otherwise outlive:
//
//    window placement   saved first, while the HWND still has a valid rect
//    drag / drop        RevokeDragDrop needs a live window handle, so the
//                       target is revoked before the HWND goes away (P6)
//    printer worker     joined before teardown frees the card out from under
//                       the drain thread
//    pending strip      persisted while the job object is still alive
//    CPU thread         stopped last, after everything it can touch is quiet
//
//  An empty or content-free job CLEARS the sidecar rather than leaving it, so
//  a print that was discarded does not reappear on next launch. Losing the
//  strip on an abnormal termination is accepted by the spec (FR-026); this
//  path only owes correctness on a clean exit.
//
//  PostQuitMessage is called here because IDxuiHostClient::OnDestroy is
//  notification-only -- the host deliberately does not post it, since not
//  every host window is an application's main window. This one is, so it owns
//  that call; without it the message loop never ends.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnDestroy()
{
    m_windowManager.SaveWindowPlacement (m_hwnd, m_d3dRenderer.IsFullscreen());

    // P6 -- revoke the IDropTarget before the HWND is destroyed.
    // RevokeDragDrop requires a valid window handle.
    m_dragDropTarget.Shutdown();

    // Join the printer drain thread before teardown frees the card.
    m_printerWorker.Stop();

    // Persist the pending strip on clean exit (FR-026); empty clears any stale
    // sidecar. Loss on abnormal termination is acceptable per the spec.
    if (!m_currentMachineName.empty())
    {
        PrinterJob *   printJob = m_printerWorker.Job();

        if (printJob != nullptr && printJob->HasContent())
        {
            HRESULT   hrSave = PrintJobStore::Save (PendingPrintDir(), printJob->Raster());
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }
        else
        {
            PrintJobStore::Clear (PendingPrintDir());
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
//  Routes one pointer move through every chrome consumer, in priority order.
//
//  The two guest-input modes sit at the top and behave OPPOSITELY, which is
//  the thing to know before editing this:
//
//    paddle mode  captures. It consumes the move outright -- relative motion
//                 drives the held axes and the cursor is snapped back to
//                 center -- so the chrome must never see it, and the function
//                 bails immediately.
//    mouse mode   does NOT capture. It maps the position to the guest and
//                 then deliberately falls through to normal routing, because
//                 the viewport carries no chrome and moves over the menu bar
//                 or drive band must keep working exactly as before.
//
//  Below that, UiShell gets first refusal (it owns the caption bar and nav
//  strip); anything it claims ends the walk. What remains is the shell's own
//  chrome: joystick button, command toolbar, //c switch strip, and the drive
//  widgets, each pairing a hover update with a tooltip show / hide request.
//
//  Tooltips are REQUESTS, not shows -- the dwell timers in TryPresentUiFrame decide
//  when a popup actually appears, so a fast pass over three widgets does not
//  flash three tooltips.
//
//  The drive walk runs before the shell dispatch because it has to visit
//  every widget regardless of hit: leaving a widget is what re-arms its
//  basename marquee, so an early exit on the first hit would strand the
//  previously hovered drive mid-scroll.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    HRESULT            hr           = S_OK;
    DxuiMessageResult  result       = DxuiMessageResult::NotHandled;
    int                x            = ((int) (short) LOWORD (lParam));
    int                y            = ((int) (short) HIWORD (lParam));
    bool               leftDown     = (wParam & MK_LBUTTON) != 0;
    bool               overBtn      = false;
    bool               shellHandled = false;
    DriveWidget *      wpDrive      = nullptr;
    int64_t            nowMs        = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                          std::chrono::steady_clock::now().time_since_epoch()).count();



    // The compass sees every move: armed, it owns the gesture; idle, the
    // call is what keeps its hover highlight honest. Ahead of the drags
    // below because a press the compass took must never feed the orbit's
    // own anchor math as well.
    if (!m_paddleCaptured && m_sceneCompass.OnPointerMove (x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // A pan in flight owns the move outright. Measured from the ANCHOR the
    // press recorded rather than accumulated frame to frame, so the scene
    // tracks the cursor exactly however far or slowly it travels and a long
    // drag cannot creep away from it.
    //
    // The paddle check below cannot be reached while this claims the move, so
    // the capture is tested HERE too. A pan cannot start under paddle capture
    // today -- the press handler bails before arming one -- but that is one
    // early-out in another function away from being untrue, and the failure
    // it would cause is a game whose paddles stop responding.
    // An orbit in flight owns the move the same way a pan does, whichever
    // button is driving it.
    if (m_sceneOrbiting && !m_paddleCaptured &&
        ((m_sceneOrbitLeftBtn && leftDown) ||
         (!m_sceneOrbitLeftBtn && (wParam & MK_RBUTTON) != 0)))
    {
        // Under the slop this is still a click in the making, so the scene
        // must not stir: a picture that shifts a pixel under a press and
        // shifts back is worse than one that does not move at all. The
        // right-button orbit has no click to protect and turns at once.
        if (!m_sceneOrbitMoved &&
            m_sceneOrbitLeftBtn &&
            std::abs (x - m_sceneOrbitStartPx.x) <= s_kSceneOrbitSlopPx &&
            std::abs (y - m_sceneOrbitStartPx.y) <= s_kSceneOrbitSlopPx)
        {
            return DxuiMessageResult::Handled;
        }

        m_sceneOrbitMoved = true;

        UpdateSceneOrbit (x, y);
        return DxuiMessageResult::Handled;
    }

    // THE TILT FOLLOWS THE POINTER, not the mark. Dragging up tips the face
    // up and dragging down tips it down, whichever mark the gesture started
    // on -- the marks say which way the control goes, they are not two
    // separate handles that move in opposite senses. Screen y grows downward,
    // so the travel is negated to get "up is up".
    if (m_bezelTilting && leftDown && !m_paddleCaptured)
    {
        m_deskScene.SetBezelTilt (m_bezelTiltStartRad
                                  + ((float) (m_bezelTiltStartPx.y - y)) * kBezelTiltRadPerPx);
        InvalidateSceneComposition();

        return DxuiMessageResult::Handled;
    }

    if (m_scenePanning && leftDown && !m_paddleCaptured)
    {
        RECT   box    = m_deskScene.Composition().viewportPx;
        float  width  = (float) (box.right - box.left);
        float  height = (float) (box.bottom - box.top);

        if (width > 0.0f && height > 0.0f)
        {
            // A pixel of cursor travel is two NDC units across the whole
            // viewport, and NDC y runs opposite client y.
            m_sceneView.panX = m_scenePanStartX
                             + ((float) (x - m_scenePanStartPx.x) / width)  * 2.0f;
            m_sceneView.panY = m_scenePanStartY
                             - ((float) (y - m_scenePanStartPx.y) / height) * 2.0f;

            ClampSceneView();
            InvalidateSceneComposition();
        }

        return DxuiMessageResult::Handled;
    }

    // Paddle mode owns the pointer while captured: relative motion drives
    // the held paddle axes and the cursor is snapped back to center, so the
    // chrome never sees the move.
    if (m_paddleCaptured)
    {
        UpdatePaddleFromMouse (x, y);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_paddleCaptured, S_OK);

    // //c Mouse mode (non-capturing): a move over the emulator viewport
    // drives the guest mouse via absolute mapping. Deliberately falls
    // through to normal routing — the viewport has no chrome, and moves
    // outside it (menu bar, drive band) behave exactly as before.
    if (GuestMouseActive())
    {
        UpdateGuestMouseFromHost (x, y);
    }

    // A fresh hover over a drive widget replays its basename marquee, so
    // the full filename can be re-read on demand. The same pass notes a
    // write-protected drive under the pointer so the WP tooltip can show.
    for (DriveWidget & drive : m_driveChrome)
    {
        RECT  outer  = drive.OuterRect();
        bool  inside = x >= outer.left && x < outer.right &&
                       y >= outer.top  && y < outer.bottom;

        drive.UpdateMarqueeHover (inside, nowMs);

        if (inside && drive.IsWriteProtected())
        {
            wpDrive = &drive;
        }
    }

    shellHandled = m_uiShell.OnMouseMove (x, y, leftDown);

    if (shellHandled)
    {
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (shellHandled, S_OK);

    overBtn = m_joystickButton.HitTest (x, y);
    m_joystickButton.SetHovered (overBtn);
    m_joystickButton.SetHoverPoint (x, y);

    if (overBtn)
    {
        // Per-segment tooltip: each device explains its own mapping.
        m_joystickTooltip.RequestShow (m_joystickButton.Bounds(),
                                       m_joystickButton.TooltipTextAt (x, y), nowMs);
    }
    else
    {
        m_joystickTooltip.RequestHide (nowMs);
    }

    // Command toolbar hover / slider drag (DCR-2). In icon-only mode the
    // hovered button's label surfaces as a tooltip (no labels on the strip).
    if (m_toolbar.OnToolbarMouseMove (x, y, leftDown))
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    {
        RECT             anchor = {};
        const wchar_t *  tip    = m_toolbar.TooltipAt (x, y, anchor);

        if (tip != nullptr)
        {
            m_toolbarTooltip.RequestShow (anchor, tip, nowMs);
        }
        else
        {
            m_toolbarTooltip.RequestHide (nowMs);
        }
    }

    // //c switch strip: hover state and a per-part tooltip (reset / 80/40 /
    // keyboard). Inert on non-//c machines (hidden).
    if (IsApple2c())
    {
        const wchar_t * tip = m_switchBar.TooltipTextAt (x, y);

        m_switchBar.SetHovered    (m_switchBar.HitTest (x, y));
        m_switchBar.SetHoverPoint (x, y);

        if (tip != nullptr)
        {
            m_switchBarTooltip.RequestShow (m_switchBar.Bounds(), tip, nowMs);
        }
        else
        {
            m_switchBarTooltip.RequestHide (nowMs);
        }
    }

    // Drive hover tooltip, suppressed while the joystick button owns the
    // hover (mutually exclusive bands, but be explicit). Windowed, the 3D
    // scene's name strips already show every basename and padlock, so the
    // only tooltip left is the padlock's WHY -- the write-protect
    // composition, anchored to the label it explains. Dwelling on the case
    // itself volunteers nothing the strip is not already saying. Fullscreen
    // shows no labels, so the overlay strip's drives keep the name tooltip,
    // joined by the write-protect composition when the disk is protected --
    // there is no padlock anywhere else to ask. The 2D path keeps its dwell
    // tooltip for protected drives only (the basename lives on the widget's
    // marquee label there).
    {
        std::wstring  tip;
        RECT          anchor = {};

        if (DeskSceneActive())
        {
            // The name strip answers for the padlock in BOTH presentations:
            // the strip carries names and locks in fullscreen now, so the
            // lock explains itself there the same way it does on the desk.
            for (int i = 0; i < (int) m_sceneDriveLabelRect.size(); i++)
            {
                POINT  lp = { x, y };

                if (m_sceneDriveLabelRect[i].right > m_sceneDriveLabelRect[i].left &&
                    PtInRect (&m_sceneDriveLabelRect[i], lp) &&
                    m_driveWidgetState[i].writeProtect.Any())
                {
                    anchor = m_sceneDriveLabelRect[i];
                    tip    = ComposeWriteProtectTooltip (
                                 i + 1,
                                 std::filesystem::path (m_diskStore.GetSourcePath (6, i))
                                     .filename().wstring(),
                                 m_driveWidgetState[i].writeProtect);
                    break;
                }
            }
        }

        if (tip.empty() && DeskSceneActive() && m_d3dRenderer.IsFullscreen())
        {
            POINT  pt = { x, y };

            if (m_stripRectPx.bottom > m_stripRectPx.top &&
                PtInRect (&m_stripRectPx, pt) && !overBtn)
            {
                SceneHitResult  sceneHit = StripHit (x, y);

                if (sceneHit.target == SceneHitResult::Target::Drive)
                {
                    std::wstring  imageName = std::filesystem::path (
                        m_diskStore.GetSourcePath (6, sceneHit.driveIndex)).filename().wstring();

                    anchor = m_stripComp.driveRectPx[sceneHit.driveIndex];
                    tip    = ComposeWriteProtectTooltip (
                                 sceneHit.driveIndex + 1, imageName,
                                 m_driveWidgetState[sceneHit.driveIndex].writeProtect);

                    if (tip.empty())
                    {
                        tip = imageName;
                    }
                }
            }
        }

        if (tip.empty() && !DeskSceneActive() && wpDrive != nullptr && !overBtn)
        {
            std::wstring  imageName = std::filesystem::path (
                m_diskStore.GetSourcePath (6, wpDrive->Drive())).filename().wstring();

            anchor = wpDrive->OuterRect();
            tip    = ComposeWriteProtectTooltip (wpDrive->Drive() + 1, imageName, wpDrive->WriteProtect());
        }

        if (!tip.empty())
        {
            m_driveTooltip.RequestShow (anchor, tip, nowMs);
        }
        else
        {
            m_driveTooltip.RequestHide (nowMs);
        }
    }

Error:
    return result;
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

DxuiMessageResult EmulatorShell::OnMouseLeave()
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
    m_toolbar.OnToolbarMouseLeave();
    m_toolbarTooltip.RequestHide (nowMs);
    m_driveTooltip.RequestHide (nowMs);

    m_switchBar.SetHovered     (false);
    m_switchBar.SetPressedPart (Apple2cSwitchBar::Part::None);
    m_switchBarTooltip.RequestHide (nowMs);

    // //c Mouse mode: the cursor left the window entirely — release the
    // guest mouse target (non-capturing contract).
    if (m_mouse != nullptr)
    {
        m_mouse->ClearHostTarget();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestMouseActive
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::GuestMouseActive() const
{
    // The fullscreen drive strip's hotkey summon "releases" the guest mouse
    // for the interaction; the FSM restores it when the strip hides.
    return m_pointerMode == InputMappingMode::Mouse && m_mouse != nullptr
        && m_mouseConnected && !m_stripSuppressGuestMouse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestMouseLive
//
//  "Mouse-aware software is actually running", so Mouse mode stays invisible
//  at a BASIC prompt: hardware truth (the IOU's own interrupt enables, which
//  only the $C079 / $C05x programming sequence can set) rather than anything
//  garbage RAM could fake.
//
//  EITHER enable counts. The gate first read X/Y alone, on the assumption
//  that SETMOUSE programs ENBXY for every active mode -- it does not.
//  MousePaint's main app asks for mode $09 (mouse on + VBL interrupt, no
//  movement interrupt), so the firmware issues DISXY / ENVBL and X/Y stays
//  masked for as long as the app runs. That read as a dead mouse: the pointer
//  froze wherever the firmware left it and clicks went nowhere, while the
//  startup screen and tutorial -- different modes -- worked.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::GuestMouseLive() const
{
    return GuestMouseActive() &&
           (m_mouse->XyInterruptsEnabled() || m_mouse->VblInterruptsEnabled());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateGuestMouseFromHost
//
//  Absolute host->guest mapping for //c Mouse mode. The mouse firmware owns
//  position and clamping, publishing both through the slot-7 screen holes:
//
//      position   $047F/$057F (X lo/hi)   $04FF/$05FF (Y lo/hi)
//      clamp min  $047D/$057D (X)         $04FD/$05FD (Y)
//      clamp max  $067D/$077D (X)         $06FD/$07FD (Y)
//
//  The host position's fraction across the viewport maps into the live
//  clamp window, and the delta from the firmware's current position is
//  queued as movement units. Self-correcting: any units the firmware
//  clamps away are re-derived from the holes on the next move. Sanity
//  checks make this a no-op until the guest app has initialized the mouse
//  firmware (pre-INITMOUSE holes are garbage). PeekByte reads the CPU's
//  memory array directly (same cross-thread pattern as screen scraping).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateGuestMouseFromHost (int xPx, int yPx)
{
    const RECT & vp        = m_viewportBoundsPx;
    int          vpW       = vp.right  - vp.left;
    int          vpH       = vp.bottom - vp.top;
    bool         isLive    = GuestMouseLive() && vpW > 1 && vpH > 1;
    uint16_t     fx        = 0;
    uint16_t     fy        = 0;
    bool         isInside  = xPx >= vp.left && xPx < vp.right &&
                             yPx >= vp.top  && yPx < vp.bottom;



    if (isLive && CrtMonitorActive())
    {
        // Curvature-correct mapping (spec 018): the pixel comes from the
        // inverse projection through the glass, so only the picture counts
        // -- pointer positions off the glass (including what used to be
        // letterbox bars) release the guest mouse.
        SceneHitResult  hit = DeskSceneHit (xPx, yPx);

        if (hit.target == SceneHitResult::Target::Glass)
        {
            fx = static_cast<uint16_t> (MulDiv (hit.emulatedPixel.x, 65535, kFramebufferWidth - 1));
            fy = static_cast<uint16_t> (MulDiv (hit.emulatedPixel.y, 65535, kFramebufferHeight - 1));

            m_mouse->SetHostTargetFraction (fx, fy);
        }
        else
        {
            m_mouse->ClearHostTarget();
        }
    }
    else if (isLive && !isInside)
    {
        // Leaving the viewport releases the guest mouse to wherever the
        // firmware last put it (non-capturing contract).
        m_mouse->ClearHostTarget();
    }
    else if (isLive)
    {
        // Publish the viewport fraction only. The DEVICE projects it into the
        // firmware's live clamp window on the CPU thread (AppleMouse::Tick ->
        // RetargetFromHoles): guest memory must not be read from the UI thread
        // -- the CPU's debug array is not the live MMU-mapped RAM, and bus
        // reads here would race the CPU thread. (The original PeekByte-based
        // mapping read stale bytes and silently no-oped in production.)
        fx = static_cast<uint16_t> (MulDiv (xPx - vp.left, 65535, vpW - 1));
        fy = static_cast<uint16_t> (MulDiv (yPx - vp.top,  65535, vpH - 1));

        m_mouse->SetHostTargetFraction (fx, fy);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSetCursor
//
//  //c Mouse mode is non-capturing but hides the host cursor while it is
//  over the emulator viewport (the guest draws its own pointer); leaving
//  the viewport -- or the client area -- restores the normal arrow.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnSetCursor (WORD hitTest)
{
    DxuiMessageResult  result     = DxuiMessageResult::NotHandled;
    POINT              pt         = {};
    bool               overGuest  = false;



    // Only hide the cursor once guest software has turned the mouse on
    // (GuestMouseLive) -- over a BASIC prompt or a non-mouse game the guest
    // draws no pointer, so hiding the host cursor would just look broken.
    // Short-circuit order matters: the cursor is only read once that holds.
    overGuest = hitTest == HTCLIENT
                && GuestMouseLive()
                && GetCursorPos (&pt)
                && ScreenToClient (m_hwnd, &pt);

    if (overGuest && CrtMonitorActive())
    {
        // With the desk scene, "over the display" means over the curved
        // glass itself, not the bounding rect around it.
        SceneHitResult  hit = DeskSceneHit (pt.x, pt.y);

        overGuest = hit.target == SceneHitResult::Target::Glass;
    }
    else if (overGuest)
    {
        overGuest = pt.x >= m_viewportBoundsPx.left && pt.x < m_viewportBoundsPx.right
                 && pt.y >= m_viewportBoundsPx.top  && pt.y < m_viewportBoundsPx.bottom;
    }

    if (overGuest)
    {
        SetCursor (nullptr);
        result = DxuiMessageResult::Handled;
    }
    else if (hitTest == HTCLIENT && DeskSceneActive() && !m_d3dRenderer.IsFullscreen()
             && m_deskScene.MaxBezelTiltRad() > 0.0f
             && GetCursorPos (&pt) && ScreenToClient (m_hwnd, &pt))
    {
        // A HAND OVER THE TILT MARKS, because they are the one thing on the
        // monitor you can take hold of. Resolved through the same hit test
        // the press uses, so the cursor changes exactly where the drag would
        // actually start -- a hand offered anywhere else would be a promise
        // the press does not keep.
        SceneHitResult  hit = DeskSceneHit (pt.x, pt.y);

        if (hit.target == SceneHitResult::Target::BezelTilt || m_bezelTilting)
        {
            SetCursor (LoadCursorW (nullptr, IDC_HAND));
            result = DxuiMessageResult::Handled;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnMouseWheel
//
//  The wheel frames the desk scene, and WHICH WAY it frames it depends on
//  what sent it. Windows gives a touchpad and a mouse the same message, so
//  the message alone cannot say -- but the DELTA can. A wheel is detented and
//  reports whole WHEEL_DELTA notches; a precision touchpad reports the finger,
//  in fractions of one. So:
//
//    - a touchpad PINCH arrives as Ctrl+wheel, and zooms.
//    - a whole notch is a mouse wheel, and zooms -- a mouse has no pinch, and
//      taking its zoom away to gain a pan would be a poor trade.
//    - anything else is a two-finger slide, and PANS.
//
//  Which makes the touchpad behave like every map and every drawing program:
//  drag to move, pinch to scale. Horizontal wheel joins in for the same
//  reason it used to be ignored -- panning on one axis with no way to reach
//  the other is worse than not panning, and a precision touchpad sends both
//  axes, so the pair of them is a real pan and either alone is not.
//
//  The test is deliberately one-sided: only a delta that is NOT a whole notch
//  is treated as a touchpad. A touchpad whose driver rounds to 120 keeps
//  zooming, which is what it did before this -- no worse, just not better.
//
//  Touchscreen gestures do not come through here at all. They arrive as
//  WM_GESTURE and are handled in OnGesture, whose pinch and one-finger drag
//  are untouched by any of this.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    int     delta   = GET_WHEEL_DELTA_WPARAM (wParam);
    POINT   pt      = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    bool    pinch   = (GET_KEYSTATE_WPARAM (wParam) & MK_CONTROL) != 0;
    bool    detent  = (delta % WHEEL_DELTA) == 0;
    float   notch   = 0.0f;
    float   factor  = 1.0f;



    // Paddle capture owns the pointer: it is hidden and confined to the
    // window because someone is playing a game with it. Nothing about a wheel
    // notch there is a request to reframe the scene, and having the desk zoom
    // out from under a game is the kind of thing that reads as a glitch.
    //
    // Mouse mode is NOT excluded. The guest mouse has no wheel to steal the
    // notch from, so zooming stays available while pointing.
    //
    // Fullscreen is: the picture owns the client and the desk is not on
    // screen, so there is no camera to move -- only hidden state to
    // scramble for the return to windowed.
    if (delta == 0 || !DeskSceneActive() || m_paddleCaptured ||
        m_d3dRenderer.IsFullscreen())
    {
        return DxuiMessageResult::NotHandled;
    }

    // Screen -> client: WM_MOUSEWHEEL packs the point in SCREEN coordinates,
    // unlike every button message, which is a reliable way to zoom toward the
    // wrong place on a window that is not at the origin.
    if (m_hwnd == nullptr || !ScreenToClient (m_hwnd, &pt))
    {
        return DxuiMessageResult::NotHandled;
    }

    // Fractional notches matter: a precision touchpad sends many small
    // deltas rather than one WHEEL_DELTA, and rounding them to whole notches
    // turns a smooth slide into a series of jumps.
    notch = (float) delta / (float) WHEEL_DELTA;

    // Shift turns the slide into an orbit -- the touchpad's spin-the-scene,
    // matching Shift+drag on the buttons. Content follows the fingers, and
    // Windows reports a downward slide negative and a rightward one positive
    // (see PanSceneByNotch), so both axes take the negative.
    //
    // THE KEYBOARD IS ASKED DIRECTLY, not the message. Shift+slide is the
    // gesture Windows itself repurposes into horizontal scrolling, and the
    // precision-touchpad path synthesizes those wheel messages WITHOUT
    // MK_SHIFT in their keystate -- so the one gesture this branch exists
    // for arrived flagless, fell through, and panned.
    if (((GET_KEYSTATE_WPARAM (wParam) & MK_SHIFT) != 0 ||
         (GetKeyState (VK_SHIFT) & 0x8000) != 0) && !pinch)
    {
        // BOTH SIGNS ARE DERIVED FROM THE PAN, not measured one gesture at
        // a time. The pan is the one slide mapping the user has validated:
        // panX -= notch reads as content-follows-fingers, which pins what
        // this hardware reports -- a rightward or upward slide arrives
        // NEGATIVE. The drag's bargain then fixes the orbit: drag right is
        // yaw negative and drag up is pitch negative, so a slide, carrying
        // a negative notch for the same motion, multiplies by POSITIVE
        // rates on both axes. The first flip fixed the vertical axis alone
        // and left horizontal inverted, which read as "backwards" the
        // moment the scene was spun side to side.
        if (horizontal)
        {
            OrbitSceneBy (notch * s_kOrbitRadPerNotch, 0.0f);
        }
        else
        {
            OrbitSceneBy (0.0f, notch * s_kOrbitRadPerNotch);
        }

        return DxuiMessageResult::Handled;
    }

    if (!pinch && !detent)
    {
        return PanSceneByNotch (notch, horizontal);
    }

    // A horizontal wheel that got this far is a tilt wheel, which has no
    // second axis to pair with and so still means nothing here.
    if (horizontal)
    {
        return DxuiMessageResult::NotHandled;
    }

    factor = std::pow (s_kSceneZoomStep, notch);

    ZoomSceneAt (pt, factor);

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PanSceneByNotch
//
//  A touchpad slide, in wheel notches, moved into the scene's pan.
//
//  DECLINED AT 1x, exactly as the touch pan is: with the scene framed to fit,
//  there is nowhere to pan to, and claiming the message would only take the
//  slide away from whatever else might want it. Returning NotHandled leaves
//  it to be scrolled by something that can.
//
//  THE SIGNS MATCH THE TOUCH DRAG, not the scrollbar. Windows' own convention
//  for a wheel is that the viewport follows the fingers, so content appears to
//  go the other way; a DRAG is the opposite bargain -- the content is what you
//  have hold of, and it goes where your fingers go, the way it does on a map.
//  Since this gesture exists to be the touchpad's version of the one-finger
//  touch pan, it copies that one's relationship to the hand.
//
//  Which took two goes, because the reasoning above does not settle the sign
//  on its own: it says the scene follows the fingers, and then you still have
//  to know which way Windows reports fingers going. It reports a downward
//  slide as a NEGATIVE vertical delta and a rightward slide as a POSITIVE
//  horizontal one -- opposite senses, on the same hand movement -- so one of
//  the two axes was always going to come out backward from a single rule.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::PanSceneByNotch (float notch, bool horizontal)
{
    if (m_sceneView.zoom <= 1.0f)
    {
        return DxuiMessageResult::NotHandled;
    }

    if (horizontal)
    {
        m_sceneView.panX -= notch * s_kScenePanStep;
    }
    else
    {
        m_sceneView.panY -= notch * s_kScenePanStep;
    }

    ClampSceneView();
    InvalidateSceneComposition();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OrbitSceneBy / BeginSceneOrbit / UpdateSceneOrbit
//
//  The inspection orbit: the camera swings about its gaze target so every
//  side of the devices can be looked at. The signs follow the touch drag's
//  bargain, the same one the pan keeps -- the CONTENT goes where the fingers
//  go. Dragging right pushes the stack's front to the right, which shows its
//  left flank, which is the eye swinging the OTHER way; dragging down tips
//  the top toward the viewer, which is the eye rising. Hence yaw takes the
//  negative of the drag and pitch the positive.
//
//  Yaw wraps rather than clamps -- spinning past the back and around is the
//  point. Pitch is bounded here only loosely, against unbounded wind-up while
//  pinned; the REAL elevation clamp lives in the layout, on the total, where
//  the seat's own baseline is known.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OrbitSceneBy (float yawRad, float pitchRad)
{
    constexpr float  kTwoPi = 6.2831853f;



    m_sceneView.orbitYawRad += yawRad;

    if (m_sceneView.orbitYawRad > 3.1415927f)
    {
        m_sceneView.orbitYawRad -= kTwoPi;
    }
    else if (m_sceneView.orbitYawRad < -3.1415927f)
    {
        m_sceneView.orbitYawRad += kTwoPi;
    }

    m_sceneView.orbitPitchRad = std::clamp (m_sceneView.orbitPitchRad + pitchRad,
                                            -s_kOrbitPitchLimit, s_kOrbitPitchLimit);

    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::BeginSceneOrbit
//
//  Arms an orbit drag at the press: everything after is absolute from this
//  anchor, so the drag tracks the pointer exactly and cannot creep.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BeginSceneOrbit (int x, int y)
{
    m_sceneOrbiting      = true;
    m_sceneOrbitMoved    = false;
    m_sceneOrbitStartPx  = POINT { x, y };
    m_sceneOrbitStartYaw = m_sceneView.orbitYawRad;
    m_sceneOrbitStartPit = m_sceneView.orbitPitchRad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OrbitRadPerPx
//
//  Radians per pixel of drag, FROM THE VIEWPORT, not a constant. The
//  coordinates the handlers see are DPI-scaled, so a fixed radians-per-pixel
//  was twice as touchy at 200% as at 100% -- a forty-degree drag came out
//  eighty, and the first captures of this feature were of poses nobody had
//  asked for. Tying the sweep to the viewport's width makes the same hand
//  motion the same turn on every monitor: a drag across the window is
//  s_kOrbitDragSweepRad, wherever it happens.
//
////////////////////////////////////////////////////////////////////////////////

float EmulatorShell::OrbitRadPerPx() const
{
    RECT   box = m_deskScene.Composition().viewportPx;
    float  w   = (float) (box.right - box.left);



    return s_kOrbitDragSweepRad / (std::max) (w, 200.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdateSceneOrbit
//
//  Absolute from the press's anchor, like the pan: a long drag tracks the
//  cursor exactly and cannot creep.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateSceneOrbit (int x, int y)
{
    float  radPerPx = OrbitRadPerPx();



    m_sceneView.orbitYawRad   = m_sceneOrbitStartYaw
                              - (float) (x - m_sceneOrbitStartPx.x) * radPerPx;
    m_sceneView.orbitPitchRad = std::clamp (
        m_sceneOrbitStartPit + (float) (y - m_sceneOrbitStartPx.y) * radPerPx,
        -s_kOrbitPitchLimit, s_kOrbitPitchLimit);

    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnGesture
//
//  Touchscreen pinch and drag, framing the desk scene exactly as the wheel
//  and a mouse drag do.
//
//  Windows reports both gestures ABSOLUTELY -- a pinch as the current
//  separation between the fingers, a pan as the current point -- so each step
//  is the ratio or difference against the previous report, and GF_BEGIN
//  reseeds rather than measuring the first step of a new gesture against
//  wherever the last one ended.
//
//  A pinch is a RATIO, not a difference: fingers moving 20 px apart means
//  something quite different starting from 40 px apart than from 400, and
//  only the ratio matches what the hand is doing.
//
//  ptsLocation is in SCREEN coordinates like the wheel's point, not client
//  like the button messages -- the same trap, in a second place.
//
//  A single-finger drag pans without the zoom gate the mouse path applies.
//  On a mouse the press has to be shared with clicking, so panning waits
//  until there is something to pan to; a touch drag on the backdrop has no
//  competing meaning, and refusing to move would just read as broken.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnGesture (WPARAM wParam, LPARAM lParam)
{
    GESTUREINFO  info    = {};
    POINT        pt      = {};
    bool         handled = false;



    info.cbSize = sizeof (info);

    // Fullscreen shows the picture, not the desk: no gesture moves a camera
    // that is not on screen.
    if (!DeskSceneActive() || m_d3dRenderer.IsFullscreen() ||
        !GetGestureInfo (reinterpret_cast<HGESTUREINFO> (lParam), &info))
    {
        return DxuiMessageResult::NotHandled;
    }

    pt.x = info.ptsLocation.x;
    pt.y = info.ptsLocation.y;

    if (m_hwnd == nullptr || !ScreenToClient (m_hwnd, &pt))
    {
        return DxuiMessageResult::NotHandled;
    }

    switch (wParam)
    {
        case GID_ZOOM:
        {
            if ((info.dwFlags & GF_BEGIN) != 0 || m_gestureZoomLast == 0)
            {
                m_gestureZoomLast = info.ullArguments;
                handled           = true;
                break;
            }

            if (info.ullArguments > 0)
            {
                ZoomSceneAt (pt, (float) info.ullArguments / (float) m_gestureZoomLast);
                m_gestureZoomLast = info.ullArguments;
            }

            handled = true;
            break;
        }

        case GID_PAN:
        {
            RECT   box    = m_deskScene.Composition().viewportPx;
            float  width  = (float) (box.right - box.left);
            float  height = (float) (box.bottom - box.top);

            // TWO fingers dragging together orbit; one finger pans. Windows
            // reports the finger separation in ullArguments for a pan, and a
            // single finger reports zero -- which is the whole discriminator.
            // The two-finger form is claimed unconditionally: it has no
            // widget meaning to preserve and orbit works at any zoom.
            if (info.ullArguments > 0)
            {
                if ((info.dwFlags & GF_BEGIN) != 0)
                {
                    m_gesturePanLastPx = pt;
                    handled            = true;
                    break;
                }

                OrbitSceneBy (-(float) (pt.x - m_gesturePanLastPx.x) * OrbitRadPerPx(),
                              (float) (pt.y - m_gesturePanLastPx.y) * OrbitRadPerPx());

                m_gesturePanLastPx = pt;
                handled            = true;
                break;
            }

            // NOT CLAIMED when there is nothing to pan to. Windows promotes an
            // unhandled gesture to mouse input, so claiming a one-finger drag
            // at 1x would swallow it and leave touch unable to work the drive
            // widgets at all -- the scene would gain a pan it cannot use and
            // lose every touch drag that meant something else.
            //
            // Declined for the same reason while the guest mouse is live: a
            // one-finger drag then is someone pointing, and the promotion to
            // mouse input is exactly what has to keep happening.
            if (m_sceneView.zoom <= 1.0f || width <= 0.0f || height <= 0.0f ||
                GuestMouseLive())
            {
                break;
            }

            if ((info.dwFlags & GF_BEGIN) != 0)
            {
                m_gesturePanLastPx = pt;
                handled            = true;
                break;
            }

            m_sceneView.panX += ((float) (pt.x - m_gesturePanLastPx.x) / width)  * 2.0f;
            m_sceneView.panY -= ((float) (pt.y - m_gesturePanLastPx.y) / height) * 2.0f;

            ClampSceneView();
            InvalidateSceneComposition();

            m_gesturePanLastPx = pt;
            handled            = true;
            break;
        }

        default:
            break;
    }

    if ((info.dwFlags & GF_END) != 0)
    {
        m_gestureZoomLast = 0;
    }

    return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
//  Press half of the click pair. Unlike the release, this handler mostly
//  ARMS things -- pressed visuals, capture, dismissals -- and leaves the
//  acting to OnLButtonUp, which is what makes a press-then-drag-off cancel
//  the way a Windows button should.
//
//  Paddle capture short-circuits everything: the pointer is hidden and
//  confined, so the press is fire button 0 and no chrome may see it.
//
//  Otherwise the press is broadcast rather than routed. Every widget gets its
//  pressed state set, because they are hit-tested independently and only one
//  can match; there is no consumption chain to respect on the way down.
//
//  Two dismissal behaviors ride along:
//
//    focus ring   a click anywhere hands focus back to the pointer, so the
//                 painted keyboard-focus visual is dropped rather than left
//                 stranded on whatever the keyboard last selected
//    open menu    a press OUTSIDE the menu strip closes the menu. The popup
//                 takes no capture, so the owning window is the only thing
//                 positioned to notice a click-away
//
//  The UI shell's return is explicitly ignored: nothing later in this handler
//  varies on it, and the message is always reported as not fully handled so
//  the default processing still runs.
//
//  The guest mouse button is gated on GuestMouseLive rather than merely
//  active -- guest software must have turned the mouse ON -- so a click at a
//  BASIC prompt is not silently swallowed by a device nobody is reading.
//
//  A press on empty scene BACKGROUND arms a pan instead. It is armed last,
//  after every widget has had its say, so dragging can never steal a click
//  from something that wanted it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    HRESULT            hr          = S_OK;
    DxuiMessageResult  result      = DxuiMessageResult::NotHandled;
    int                x           = ((int) (short) LOWORD (lParam));
    int                y           = ((int) (short) HIWORD (lParam));
    bool               consumed    = false;
    bool               toolbarTook = false;
    bool               chromeTook  = false;



    UNREFERENCED_PARAMETER (wParam);

    // While paddle-captured, the left button is fire button 0 and the
    // pointer is hidden/confined, so nothing else acts on the press.
    if (m_paddleCaptured)
    {
        PushPaddleButton (0, true);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_paddleCaptured, S_OK);

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

    chromeTook = m_joystickButton.HitTest (x, y);

    m_joystickButton.SetPressed (chromeTook);

    // Command toolbar press (button press states + slider drag start).
    toolbarTook = m_toolbar.OnToolbarLButtonDown (x, y);

    if (toolbarTook)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    chromeTook = chromeTook || toolbarTook;

    if (IsApple2c())
    {
        Apple2cSwitchBar::Part  part = m_switchBar.PartAt (x, y);

        m_switchBar.SetPressedPart (part);

        chromeTook = chromeTook || part != Apple2cSwitchBar::Part::None;
    }

    // The UI shell (debug panels, on-screen buttons) gets first crack at
    // the press. We still report the message as not fully handled, but its
    // verdict is not moot: a widget that took the press owns the release
    // too, and the scene gestures below must not arm over it.
    consumed   = m_uiShell.OnLButtonDown (x, y);
    chromeTook = chromeTook || consumed;

    // //c Mouse mode (non-capturing): a press over the emulator display is
    // the guest mouse button -- but only once guest software has turned the
    // mouse on, so clicks aren't silently swallowed at a BASIC prompt.
    // Chrome outside the display already had its chance above. With the
    // desk scene, "over the display" means over the curved glass itself
    // (release stays deliberately ungated, matching the 2D contract).
    if (GuestMouseLive())
    {
        bool  overDisplay = false;

        if (CrtMonitorActive())
        {
            SceneHitResult  hit = DeskSceneHit (x, y);

            overDisplay = hit.target == SceneHitResult::Target::Glass;
        }
        else
        {
            overDisplay = x >= m_viewportBoundsPx.left && x < m_viewportBoundsPx.right
                       && y >= m_viewportBoundsPx.top  && y < m_viewportBoundsPx.bottom;
        }

        if (overDisplay)
        {
            m_mouse->SetButton (true);
        }
    }

    // Pan arms LAST, and only on empty scene background: anything the user
    // could have meant to click has already claimed the press by here, so a
    // drag can never steal one. Zoomed all the way out there is nothing to
    // pan to, so it stays disarmed and an idle drag on the backdrop does
    // nothing rather than wobbling a scene that already fits.
    //
    // NOT WHILE THE GUEST MOUSE IS LIVE. //c Mouse mode maps the host pointer
    // absolutely, and it keeps doing so over the BACKGROUND -- so a drag out
    // there is very likely someone steering the guest cursor toward an edge,
    // and panning would both hijack that drag and freeze the guest pointer
    // for its duration. GuestMouseLive rather than Active on purpose: at a
    // BASIC prompt nothing is reading the mouse, so panning stays available.
    // Shift turns the press into an orbit -- the touchpad's road to it, where
    // a right-drag is awkward. Ahead of the pan arm, and regardless of zoom.
    // Never in fullscreen, where the desk is not on screen.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() &&
        (wParam & MK_SHIFT) != 0 && !m_mainMenu.IsOpen() &&
        PointInSceneRect (x, y) && !chromeTook)
    {
        BeginSceneOrbit (x, y);
        m_sceneOrbitLeftBtn = true;
        result = DxuiMessageResult::Handled;
        BAIL_OUT_IF (true, S_OK);
    }

    // The compass outranks everything on the scene: it is drawn on top,
    // so a press where it sits belongs to it.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() && !m_mainMenu.IsOpen() &&
        m_sceneCompass.OnPointerDown (x, y))
    {
        result = DxuiMessageResult::Handled;
        BAIL_OUT_IF (true, S_OK);
    }

    // Grabbing a tilt mark starts the bezel drag. Before the orbit, which is
    // the only other thing a press on the scene begins, and which would
    // otherwise swallow the gesture.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() && !m_mainMenu.IsOpen()
        && !GuestMouseLive() && m_deskScene.MaxBezelTiltRad() > 0.0f)
    {
        SceneHitResult  hit = DeskSceneHit (x, y);

        if (hit.target == SceneHitResult::Target::BezelTilt)
        {
            m_bezelTilting      = true;
            m_bezelTiltStartPx  = POINT { x, y };
            m_bezelTiltStartRad = m_deskScene.BezelTiltRad();

            result = DxuiMessageResult::Handled;
            BAIL_OUT_IF (true, S_OK);
        }
    }

    // A PLAIN DRAG ON THE SCENE TURNS IT, AND THE SCENE INCLUDES THE MACHINE.
    // It used to pan, and only when zoomed -- so at rest the most natural
    // gesture in the window did nothing at all. Turning is what people expect
    // of a 3D thing under the mouse; the pan still lives on the touchpad's
    // two-finger slide, beside the zoom on its pinch.
    //
    // ARMED OVER ANYTHING THE SCENE SHOWS, not only over the empty backdrop.
    // Requiring a target miss meant the picture and the drives' faces -- the
    // largest and most obvious surfaces in the window, the ones a hand
    // reaches for first -- were dead to the gesture, and a drag begun on the
    // machine did nothing while the same drag an inch to the left turned it.
    // A press there still MEANS what it meant; it is the release that decides
    // which, exactly as it does for a button or for the compass:
    //
    //     travel past the slop  ->  a turn, and the release ends it
    //     released inside it    ->  a click, and the chain below runs
    //
    // The two grab targets are excluded because a press on them already
    // begins a different drag or a command: the bezel's tilt marks (armed
    // above, which bails before reaching here) and a drive's door.
    //
    // ONLY ON THE SCENE'S OWN RECT, and only where no chrome took the press.
    // A scene hit of None is true of every pixel of the toolbar and the
    // status bar as well -- there is no machine out there to hit -- so arming
    // on that alone armed a turn under the command buttons, and the release
    // that would have fired them ended the turn instead.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() &&
        !m_mainMenu.IsOpen() && !GuestMouseLive() &&
        PointInSceneRect (x, y) && !chromeTook)
    {
        SceneHitResult  hit    = DeskSceneHit (x, y);
        bool            onDoor = hit.target == SceneHitResult::Target::Drive
                                 && hit.region == DriveWidgetRegion::Eject;

        if (!onDoor && hit.target != SceneHitResult::Target::BezelTilt)
        {
            BeginSceneOrbit (x, y);
            m_sceneOrbitLeftBtn = true;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
//  Where clicks actually DO things. This is a strict priority chain -- the
//  first consumer to claim the release ends the walk -- and the order is the
//  contract:
//
//    paddle capture   fire button 0, capture retained
//    toolbar          button dispatch / mute / slider drop
//    //c switch strip reset, 80/40, keyboard
//    UI shell         debug panels and on-screen buttons
//    input-mode button
//    suppressed drop  (see below)
//    drive widgets    body browses, eject ejects then browses
//    viewport         paddle re-grab
//
//  Three of these are subtle enough to be worth stating outright.
//
//  The input-mode button routes through ToggleInputMappingMode -- the same
//  entry point as the Machine menu command -- rather than assigning the mode
//  directly, so leaving a mode still neutralizes its held arrow / X / Z
//  inputs. Setting the field would strand whatever was down at the time.
//
//  The suppressed-click check exists because a completed OLE drop onto a
//  drive widget is followed by a WM_LBUTTONUP from the OS that lands on that
//  same drive. Without swallowing it, dropping a disk image mounts it and
//  then immediately opens the file-open dialog on top of it.
//
//  The paddle re-grab predicate is captured BEFORE StartPaddleCapture runs,
//  because that call sets m_paddleCaptured -- re-testing afterwards reads
//  false and the bail below would not fire.
//
//  The guest mouse button release at the end is deliberately NOT viewport-
//  gated, unlike the press. A press inside the viewport released outside it
//  must still clear the button, or the guest is left with it stuck down.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    HRESULT                 hr            = S_OK;
    DxuiMessageResult       result        = DxuiMessageResult::NotHandled;
    int                     x             = ((int) (short) LOWORD (lParam));
    int                     y             = ((int) (short) HIWORD (lParam));
    DriveWidgetRegion       region        = DriveWidgetRegion::None;
    Apple2cSwitchBar::Part  switchPart    = Apple2cSwitchBar::Part::None;
    bool                    toolbarTook   = false;
    bool                    shellTook     = false;
    bool                    onSwitchPart  = false;
    bool                    onModeButton  = false;
    bool                    wasSuppressed = false;
    bool                    driveTook     = false;
    bool                    canGrabPaddle = false;



    UNREFERENCED_PARAMETER (wParam);

    // Ending a pan consumes the release. The press it began with never
    // reached a widget, so letting the release run the click chain would fire
    // whatever the cursor happened to land on after the drag.
    if (m_scenePanning)
    {
        m_scenePanning = false;
        ReleaseCapture();
        return DxuiMessageResult::Handled;
    }

    // The compass's release fires its click or ends its drag, and either
    // way the press never reached a widget, so the click chain stays out
    // of it.
    if (m_sceneCompass.OnPointerUp (x, y))
    {
        ReleaseCapture();
        return DxuiMessageResult::Handled;
    }

    // Likewise a drag orbit's release -- BUT ONLY IF IT TURNED. A press that
    // armed one and never travelled is a click, and swallowing its release
    // would make every press on the machine do nothing at all. Fall through
    // and let the chain below read it as the click it was.
    if (m_sceneOrbiting && m_sceneOrbitLeftBtn)
    {
        bool  turned = m_sceneOrbitMoved;

        m_sceneOrbiting   = false;
        m_sceneOrbitMoved = false;

        if (turned)
        {
            ReleaseCapture();
            return DxuiMessageResult::Handled;
        }
    }

    // ...and a bezel tilt's, which also writes where it came to rest. Saved
    // on release rather than on every step of the drag: the tilt is a
    // preference, not an animation, and a file rewritten per mouse-move is a
    // file rewritten a hundred times a second.
    if (m_bezelTilting)
    {
        m_bezelTilting = false;
        ReleaseCapture();
        PersistBezelTilt();
        return DxuiMessageResult::Handled;
    }

    // While paddle-captured, the left button is fire button 0; release it
    // and keep the capture (the transient click-capture path is bypassed).
    if (m_paddleCaptured)
    {
        PushPaddleButton (0, false);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_paddleCaptured, S_OK);

    ReleaseCapture();
    m_joystickButton.SetPressed (false);

    // Command toolbar release: click dispatch / mute toggle / slider drop.
    toolbarTook = m_toolbar.OnToolbarLButtonUp (x, y);

    if (toolbarTook)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    BAIL_OUT_IF (toolbarTook, S_OK);

    // //c switch strip: latch the switch / fire the (Ctrl-gated) reset on
    // release over a part. Captured before the pressed-part is cleared.
    if (IsApple2c())
    {
        switchPart = m_switchBar.PartAt (x, y);
        m_switchBar.SetPressedPart (Apple2cSwitchBar::Part::None);
    }

    shellTook = m_uiShell.OnLButtonUp (x, y);

    BAIL_OUT_IF (shellTook, S_OK);

    onSwitchPart = switchPart != Apple2cSwitchBar::Part::None;

    if (onSwitchPart)
    {
        HandleSwitchBarClick (switchPart);
    }

    BAIL_OUT_IF (onSwitchPart, S_OK);

    // Cycling the input-mode button routes through the same path as the
    // Machine menu command so the leave-time neutralization of held
    // arrow / X / Z inputs runs.
    onModeButton = m_joystickButton.HitTest (x, y);

    if (onModeButton)
    {
        switch (m_joystickButton.SegmentAt (x, y))
        {
            case InputDeviceSelector::Segment::Joystick:
                ToggleInputMappingMode (InputMappingMode::Joystick);
                break;
            case InputDeviceSelector::Segment::Paddle:
                ToggleInputMappingMode (InputMappingMode::Paddle);
                break;
            case InputDeviceSelector::Segment::Mouse:
                ToggleInputMappingMode (InputMappingMode::Mouse);
                break;
            default:
                break;
        }
    }

    BAIL_OUT_IF (onModeButton, S_OK);

    // If we just finished an OLE drop on a drive widget, the OS posts
    // a WM_LBUTTONUP that lands here on top of the drive. Swallow it
    // so the user doesn't see the file-open dialog pop up immediately
    // after the dropped image mounts.
    wasSuppressed = m_dragDropTarget.ConsumeSuppressedClick();

    BAIL_OUT_IF (wasSuppressed, S_OK);

    // Drive clicks. Scene active: the 3D drives resolve through the hit
    // tester with the same region semantics (slot = eject + browse, body =
    // browse); otherwise the 2D widget walk. Either way the actions route
    // through the identical handlers -- the scene only changes how hits are
    // found, never what they do.
    if (DeskSceneActive())
    {
        POINT           pt      = { x, y };
        bool            inStrip = m_d3dRenderer.IsFullscreen() &&
                                  m_stripRectPx.bottom > m_stripRectPx.top &&
                                  PtInRect (&m_stripRectPx, pt);
        SceneHitResult  sceneHit = inStrip ? StripHit (x, y) : DeskSceneHit (x, y);

        // ONLY THE DOOR ACTS. The body region stays for hover -- the
        // tooltip that names the disk -- but a click there does nothing:
        // opening a drive is done by its door, and a whole case that
        // browses on any touch turned every stray click into a dialog.
        if (sceneHit.target == SceneHitResult::Target::Drive &&
            sceneHit.region == DriveWidgetRegion::Eject)
        {
            Eject (6, sceneHit.driveIndex);

            // A browse opened from the strip pins it (the FSM must not
            // auto-hide under the dialog).
            m_stripBrowseOpen = inStrip;
            BrowseForDisk (sceneHit.driveIndex);
            m_stripBrowseOpen = false;

            driveTook = true;
        }
    }
    else
    {
        for (DriveWidget & drive : m_driveChrome)
        {
            region = drive.HitTest (x, y);

            if (region == DriveWidgetRegion::Body)
            {
                BrowseForDisk (drive.Drive());
                driveTook = true;
                break;
            }

            if (region == DriveWidgetRegion::Eject)
            {
                Eject (6, drive.Drive());
                BrowseForDisk (drive.Drive());
                driveTook = true;
                break;
            }
        }
    }

    BAIL_OUT_IF (driveTook, S_OK);

    // (The standalone printer indicator's click-to-open is retired: the
    // toolbar's Printer button dispatches IDM_PRINTER_PREVIEW instead, DCR-2.)

    // A bare left-click on the emulator screen (no chrome / widget / drive
    // hit) in Paddle mode re-grabs the pointer after an Esc release. The
    // predicate is captured BEFORE the call, because StartPaddleCapture sets
    // m_paddleCaptured and re-testing it afterwards would read false.
    canGrabPaddle = m_pointerMode == InputMappingMode::Paddle && !m_paddleCaptured;

    if (canGrabPaddle)
    {
        StartPaddleCapture();
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (canGrabPaddle, S_OK);

    // //c Mouse mode: any left-release drops the guest mouse button --
    // unconditionally (not viewport-gated), so a press inside the viewport
    // released outside it can never leave the guest button stuck.
    if (GuestMouseActive())
    {
        m_mouse->SetButton (false);
    }

Error:
    return result;
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

    if (m_paddleCaptured)
    {
        PushPaddleButton (1, true);
        result = DxuiMessageResult::Handled;
    }
    else if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen())
    {
        // Right-drag orbits the scene. Unconditionally on the scene -- unlike
        // the pan there is no widget interaction to share the button with,
        // and orbit is useful at any zoom. Not in fullscreen, where the desk
        // is not on screen and there is no camera to swing.
        SetCapture (m_hwnd);
        BeginSceneOrbit ((int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam));
        m_sceneOrbitLeftBtn = false;
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnRButtonUp (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    UNREFERENCED_PARAMETER (wParam);

    if (m_sceneOrbiting && !m_sceneOrbitLeftBtn)
    {
        int      x     = (int) (short) LOWORD (lParam);
        int      y     = (int) (short) HIWORD (lParam);
        bool     still = std::abs (x - m_sceneOrbitStartPx.x) <= 3 &&
                         std::abs (y - m_sceneOrbitStartPx.y) <= 3;
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_sceneOrbiting = false;
        ReleaseCapture();

        // Two motionless right-clicks in double-click time reset the orbit
        // -- the pose home button, without stealing a key.
        if (still)
        {
            if (nowMs - m_sceneOrbitTapMs <= (int64_t) GetDoubleClickTime())
            {
                m_sceneView.orbitYawRad   = 0.0f;
                m_sceneView.orbitPitchRad = 0.0f;
                m_sceneOrbitTapMs         = 0;
                InvalidateSceneComposition();
            }
            else
            {
                m_sceneOrbitTapMs = nowMs;
            }
        }

        return DxuiMessageResult::Handled;
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  OnKillFocus
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnKillFocus()
{
    StopPaddleCapture();

    // Losing keyboard focus means the matching WM_KEYUPs will never arrive
    // here -- whatever window took focus gets them. Release the guest keyboard
    // latch, its armed auto-repeat, and the modifier states, so a key held
    // across a focus change (Enter while a window pops up, Alt-Tab mid-key)
    // can never leave the emulated key repeating forever.
    ReleaseGuestKeys();
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGuestKeys
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReleaseGuestKeys()
{
    auto *  iieKbd = m_refs.iieKeyboard;



    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetKeyDown (false);
        m_refs.keyboard->BeginKeyRepeat (0);
    }

    if (iieKbd != nullptr)
    {
        iieKbd->SetOpenApple   (false);
        iieKbd->SetClosedApple (false);
        iieKbd->SetShift       (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCancelMode
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnCancelMode()
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

void EmulatorShell::OpenSettings()
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
    bool  claimed = true;



    // The mnemonic arm both TESTS and ACTS -- HandleAltKey opens the menu --
    // so it has to lead the ladder rather than fold into a predicate.
    if (altHeld && vk >= 0x20 && vk <= 0x7E && m_mainMenu.HandleAltKey ((wchar_t) vk))
    {
        // Claimed by the menu bar.
    }
    else if (vk == VK_F10 && !ctrlHeld && !altHeld)
    {
        // F10 enters the chrome keyboard-focus ring at the first menu title
        // (dropdown closed). Exiting the ring is handled inside
        // HandleChromeFocusKey, which intercepts F10 once the ring is active.
        SetChromeFocusIndex (s_kChromeFocusMenuFirst);
    }
    else if (vk == 'V' && ctrlHeld && !altHeld)
    {
        // Windows has already synthesized this combo's WM_CHAR (^V, 0x16);
        // without the swallow it lands in the guest keyboard latch AHEAD of
        // the pasted text, planting an invisible control byte in the input
        // line (the classic paste-then-SYNTAX-ERROR).
        m_swallowMetaChar = true;
        m_clipboardManager->PasteFromClipboard (m_hwnd);
    }
    else if (vk == 'R' && ctrlHeld && !(GetKeyState (VK_SHIFT) & 0x8000))
    {
        m_swallowMetaChar = true;
        PostCommand (IDM_MACHINE_RESET);
    }
    else
    {
        claimed = false;
    }

    return claimed;
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
    auto    * iieKbd = m_refs.iieKeyboard;
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
//  Skims off every keystroke the SHELL owns, then hands the rest to the guest.
//
//  The pre-checks run in a fixed order, each an escape hatch that must not be
//  reachable from the guest's side:
//
//    Esc in paddle mode  releases the pointer capture and returns the mapping
//                        to Off. This is first because a captured pointer has
//                        hidden the cursor, and Esc is the only way out
//    chrome focus ring   while a menu title / button / drive holds keyboard
//                        focus (or any dropdown is open), the ring owns every
//                        keydown, so typed letters cannot leak into the //e
//                        while the user is arrowing through a menu
//    meta shortcuts      host-level chords
//
//  Whatever survives is by definition the guest's, and is delivered through
//  the VIEWPORT rather than straight to m_refs.keyboard. That indirection is
//  the point: the viewport is configured with SetConsumesInput and
//  SetWantsAllKeys, so it forwards everything -- Esc, Tab, arrows included --
//  back to OnViewportKey, and guest input stays on the single Dxui input path
//  (FR-034) instead of the shell reaching around the framework.
//
//  Always reports Handled, including on the bail paths: once a keystroke has
//  been classified as shell-owned it must not also reach default processing.
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
    if (m_pointerMode == InputMappingMode::Paddle && vk == VK_ESCAPE)
    {
        SetPointerMapping (InputMappingMode::Off);
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





////////////////////////////////////////////////////////////////////////////////
//
//  HostKeyboardLayoutIsDvorak
//
//  True when the host's active keyboard layout is a Dvorak variant. Probed
//  behaviorally rather than by KLID string, so it catches every Dvorak layout
//  (US, left/right-hand, third-party) without a hard-coded list: VkKeyScanEx
//  reports which physical key (VK code) produces 'o'. On QWERTY that is VK 'O';
//  on Dvorak 'o' lives on the physical 'S' key, so it reports VK 'S'. Unlike
//  ToUnicode this leaves no dead-key state behind. The //c keyboard switch only
//  needs to remap when the host is QWERTY -- see Apple2eKeyboard::MapTypedChar.
//
////////////////////////////////////////////////////////////////////////////////

static bool HostKeyboardLayoutIsDvorak()
{
    HKL    hkl = GetKeyboardLayout (0);
    SHORT  vk  = VkKeyScanExW (L'o', hkl);



    // vk == -1 means 'o' is unreachable on this layout -- assume QWERTY.
    return vk != -1 && LOBYTE (vk) == 'S';
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportKey
//
//  Applies one guest keystroke to the Apple ][ keyboard latch and game port.
//  Everything arriving here has already been classified as the guest's.
//
//  Down, Up, and Char are three different jobs, not three cases of one:
//  Down handles control codes and joystick axes, Up disarms, and Char is the
//  only path that types a printable character.
//
//  Auto-repeat is the subtlest part. Control-code presses are gated on the
//  repeat bit so the HOST's repeat never reaches the latch -- a fresh press
//  arms the $C000 strobe once and registers the key with BeginKeyRepeat, and
//  the emulated //e then generates its own authentic repeat cadence in Tick.
//  Letting both repeats run would double the rate and sound wrong.
//
//  A key-up always calls BeginKeyRepeat(0). The //e latch holds exactly one
//  key, so a release necessarily ends the current repeat; clearing it also
//  stops a later non-character press (a bare modifier, say) from resurrecting
//  the previous character's repeat.
//
//  Joystick emulation overlays the arrows and X / Z, and only when the mode
//  is on AND a game-port paddle bank exists. Three details matter:
//
//    - driveJoystick is recomputed per event, so a mode change between a
//      press and its release is honored and nothing is left held
//    - arrows are WITHHELD from the keyboard latch in this mode; a held
//      direction would otherwise flood $C000 and starve a game's reads
//    - the fire buttons are re-resolved on EVERY key event, not just X / Z,
//      so an Alt press re-applies its Open / Closed-Apple mapping without
//      clobbering a still-held X, and a released X cannot leave a button
//      stuck down while Alt is still held
//
//  Opposing arrows resolve last-pressed-wins via the per-axis memory, which
//  is what makes a quick left-right reversal read as a reversal rather than
//  as centered.
//
//  The Char path feeds MapTypedChar so the //c keyboard switch can remap to
//  Dvorak. The host's own layout is probed live and pushed in, so a host that
//  is ALREADY Dvorak skips the remap instead of translating twice. Clipboard
//  paste bypasses this path entirely and calls KeyPress directly -- pasted
//  text is never remapped, matching the hardware encoder.
//
//  Always returns true: nothing here bubbles back to the framework, since the
//  shell's escape routes live in the OnKeyDown pre-checks, not in the sink.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportKey (const DxuiKeyEvent & ev)
{
    bool  hasKeyboard = false;



    // Arrow keys double as the emulated joystick axes / the X / Z keys as
    // fire buttons when "Map Arrows to Joystick" is on AND a game-port
    // paddle bank is present. Recomputed per event so a mode change between
    // press and release is always honored.
    bool  driveJoystick = m_arrowsJoystick &&
                          (m_refs.iieSoftSwitches != nullptr ||
                           m_refs.gamePort != nullptr);
    // The guest owns every key that reaches here either way; with no keyboard
    // device there is simply nothing to deliver it to.
    hasKeyboard = m_refs.keyboard != nullptr;

    if (hasKeyboard && ev.kind == DxuiKeyEventKind::Down)
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
    else if (hasKeyboard && ev.kind == DxuiKeyEventKind::Up)
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

        if (m_arrowsJoystick && IsArrowVk (vk))
        {
            UpdateJoystickAxesFromKeys();
        }

        if (m_arrowsJoystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
    else if (hasKeyboard)   // DxuiKeyEventKind::Char
    {
        WPARAM  ch = ev.vk;

        if (ch >= 1 && ch <= 127)
        {
            // //c keyboard switch: remap physical keystrokes to Dvorak when the
            // switch is engaged. A no-op on the //e, when the switch is out, and
            // when the HOST layout is already Dvorak (the shell feeds that live
            // so MapTypedChar can skip the remap and avoid double-translating).
            // Clipboard paste feeds KeyPress directly (not this path), so pasted
            // text is never remapped -- matching the hardware encoder.
            Byte  code = static_cast<Byte> (ch);

            if (m_refs.iieKeyboard != nullptr)
            {
                m_refs.iieKeyboard->SetHostKeyboardDvorak (HostKeyboardLayoutIsDvorak());

                code = m_refs.iieKeyboard->MapTypedChar (code);
            }

            m_refs.keyboard->KeyPress (code);
            m_refs.keyboard->BeginKeyRepeat (code);
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
//  center), and the joystick maps to arrow keys -- neither fits the
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
//  canceling to center.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateJoystickAxesFromKeys()
{
    HRESULT  hr       = S_OK;
    auto   * iieSw    = m_refs.iieSoftSwitches;
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
    auto   * iieKbd   = m_refs.iieKeyboard;
    auto   * gamePort = m_refs.gamePort;
    bool     button0  = false;
    bool     button1  = false;



    BAIL_OUT_IF (iieKbd == nullptr && gamePort == nullptr, S_OK);

    // Only read the physical keys while WE are the foreground app. The async
    // key state is global, so a held Alt during the Alt-Tab switcher (or any
    // time another app is active) would otherwise keep re-pressing Open-Apple /
    // button 0 in the guest every frame -- e.g. re-triggering a Print Shop
    // print on the way out. Foreground reads normally (no added latency); not
    // foreground leaves the buttons released. Matches the input gate used
    // elsewhere (GetForegroundWindow() != m_hwnd).
    if (GetForegroundWindow() == m_hwnd)
    {
        button0 = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton0Vk)) & 0x8000) != 0 ||
                  (GetKeyState      (VK_LMENU)                                & 0x8000) != 0;
        button1 = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton1Vk)) & 0x8000) != 0 ||
                  (GetKeyState      (VK_RMENU)                                & 0x8000) != 0;
    }

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
    // Combined PRESET setter (button cycle + legacy callers): selects BOTH
    // axes of the split model. The Machine-menu items toggle the
    // axes independently via SetArrowsJoystick / SetPointerMapping, so
    // e.g. Joystick keys + Mouse pointer can coexist (disjoint game-port
    // lines); presets deliberately reset the other axis.
    switch (mode)
    {
        case InputMappingMode::Joystick:
            SetPointerMapping (InputMappingMode::Off);
            SetArrowsJoystick (true);
            break;

        case InputMappingMode::Paddle:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Paddle);
            break;

        case InputMappingMode::Mouse:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Mouse);
            break;

        case InputMappingMode::Off:
        default:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Off);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetArrowsJoystick
//
//  Keys axis of the split input model: maps arrows + X/Z onto the joystick
//  axes / fire buttons. Independent of the Mouse pointer (disjoint hardware),
//  but mutually exclusive with Paddle -- both drive the game-port paddle
//  lines -- so enabling it drops an active Paddle. Turning it off neutralizes
//  the key-derived stick and buttons so a held key can't stay stuck (axes
//  left alone while Paddle owns them).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetArrowsJoystick (bool on)
{
    auto * iieSw    = m_refs.iieSoftSwitches;
    auto * iieKbd   = m_refs.iieKeyboard;
    auto * gamePort = m_refs.gamePort;



    // Mirror of the rule in SetPointerMapping: the Keys axis drives PDL0/1,
    // so enabling it must drop an active Paddle (they fight over the same
    // game-port lines). Mouse uses a separate slot card and may coexist.
    if (on && m_pointerMode == InputMappingMode::Paddle)
    {
        SetPointerMapping (InputMappingMode::Off);
    }

    m_arrowsJoystick               = on;
    m_globalPrefs.arrowsToJoystick = on;
    SyncInputModeUi();

    if (on)
    {
        UpdateJoystickAxesFromKeys();
        UpdateJoystickButtonsFromKeys();
        return;
    }

    if (m_pointerMode != InputMappingMode::Paddle)
    {
        if (iieSw != nullptr)
        {
            iieSw->SetPaddle (0, Apple2eSoftSwitchBank::s_knPaddleCenter);
            iieSw->SetPaddle (1, Apple2eSoftSwitchBank::s_knPaddleCenter);
        }

        if (gamePort != nullptr)
        {
            gamePort->SetPaddle (0, AppleGamePort::s_knPaddleCenter);
            gamePort->SetPaddle (1, AppleGamePort::s_knPaddleCenter);
        }
    }

    if (gamePort != nullptr)
    {
        gamePort->SetButton (0, false);
        gamePort->SetButton (1, false);
    }

    if (iieKbd != nullptr)
    {
        bool  fg = (GetForegroundWindow() == m_hwnd);   // never latch Alt-as-Open-Apple while backgrounded

        iieKbd->SetOpenApple   (fg && (GetKeyState (VK_LMENU) & 0x8000) != 0);
        iieKbd->SetClosedApple (fg && (GetKeyState (VK_RMENU) & 0x8000) != 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPointerMapping
//
//  Pointer axis of the split input model: Off / Paddle (capturing) /
//  Mouse (non-capturing //c IOU mouse). Paddle and Mouse are mutually
//  exclusive by construction -- both claim the host pointer.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetPointerMapping (InputMappingMode pointer)
{
    auto             * iieSw = m_refs.iieSoftSwitches;
    InputMappingMode   prev  = m_pointerMode;



    if (pointer == InputMappingMode::Joystick)   // not a pointer mode
    {
        pointer = InputMappingMode::Off;
    }

    // Paddle owns the game-port paddle axes (PDL0/1) -- the same lines the
    // arrows->joystick remap drives -- so entering Paddle must drop the Keys
    // axis. Mouse is a separate slot card (disjoint lines) and may coexist
    // with Joystick, so only Paddle clears it. Enforced here (not just in the
    // SetInputMappingMode presets) so the per-segment / menu toggle paths
    // honor the same paddle-vs-joystick exclusivity.
    if (pointer == InputMappingMode::Paddle && m_arrowsJoystick)
    {
        SetArrowsJoystick (false);
    }

    if (prev == InputMappingMode::Paddle && pointer != InputMappingMode::Paddle)
    {
        StopPaddleCapture();
    }

    // Leaving Mouse: release a held guest button so it can't stick, and
    // drop the absolute target so the guest mouse stops tracking.
    if (prev == InputMappingMode::Mouse && pointer != InputMappingMode::Mouse
        && m_mouse != nullptr)
    {
        m_mouse->SetButton (false);
        m_mouse->ClearHostTarget();
    }

    m_pointerMode                = pointer;
    m_globalPrefs.pointerMapping = pointer;
    SyncInputModeUi();

    if (pointer == InputMappingMode::Paddle)
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        if (iieSw != nullptr)
        {
            iieSw->SetPaddle (0, Apple2eSoftSwitchBank::s_knPaddleCenter);
            iieSw->SetPaddle (1, Apple2eSoftSwitchBank::s_knPaddleCenter);
        }

        m_paddleAxisX = (float) s_kPaddleCenterByte;
        m_paddleAxisY = (float) s_kPaddleCenterByte;

        // THE HUD NOTICE SAYS THIS NOW, in both presentations. Entering
        // paddle mode used to force a tooltip up for eight seconds, because
        // the capture means the hover that would normally dismiss one never
        // fires -- so it had to time out instead. That put a panel over the
        // chrome it was anchored to, on top of whatever tooltip the pointer
        // had already summoned, and it said what the notice over the picture
        // now says for exactly as long as the capture lasts.
        //
        // AND WHATEVER IS ALREADY UP GOES NOW. The click that turns paddle
        // mode on is a click ON a control, so its tooltip is showing -- and
        // the capture that follows takes the pointer, so the move that would
        // dismiss it never comes. It would sit there until its lifetime ran
        // out, which is a long time to leave a balloon over a game.
        m_joystickTooltip.HideImmediate();
        m_toolbarTooltip.HideImmediate();
        m_driveTooltip.HideImmediate();
        m_switchBarTooltip.HideImmediate();

        StartPaddleCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncInputModeUi
//
//  Common tail for the axis setters: refresh the toggle button's displayed
//  mode, keep the legacy combined pref in sync (downgrade compat), persist.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncInputModeUi()
{
    m_globalPrefs.inputMappingMode = DisplayInputMode();
    SyncSelectorState();
    RelayoutJoystickButton();
    SaveGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncSelectorState
//
//  Pushes the split-model state (Keys, Pointer, mouse availability) into
//  the device selector.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSelectorState()
{
    m_joystickButton.SetState (m_arrowsJoystick, m_pointerMode,
                               m_mouse != nullptr && m_mouseConnected);
    m_toolbar.SetInputState   (m_arrowsJoystick, m_pointerMode,
                               m_mouse != nullptr && m_mouseConnected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDefaultPointerForMachine
//
//  A //c with its mouse connected and no pointer mapping chosen
//  defaults Pointer to Mouse -- a runtime nudge, not persisted, and
//  invisible until guest mouse software runs (firmware-live gate). Called
//  after the per-machine connected states are seeded at launch and on
//  machine switch.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyDefaultPointerForMachine()
{
    if (m_mouse != nullptr && m_mouseConnected
        && m_pointerMode == InputMappingMode::Off)
    {
        // State only -- NO chrome work here. This runs on the CPU thread
        // during SwitchMachine, and the selector sync / joystick-button
        // relayout measure text through the Dxui renderer, which is
        // UI-thread-only (DxuiAssertUiThread fired on a //c -> //e switch).
        // Both paths already relayout on the UI thread afterwards: a machine
        // switch posts WM_APP_DXUI_UPDATE_TITLE, whose handler runs
        // ReflowChromeForMachineChange -> OnSize -> LayoutJoystickButton
        // (which SyncSelectorState()s itself), and the launch path lays out
        // the chrome later in Initialize.
        m_pointerMode = InputMappingMode::Mouse;

        // SyncSelectorState / RelayoutJoystickButton touch Dxui (text
        // measurement) and assert the UI thread. On a machine switch this runs
        // on the CPU thread, so defer the chrome reflection to the post-switch
        // handler on the UI thread (WM_APP_DXUI_UPDATE_TITLE, posted by the
        // UpdateWindowTitle at the end of SwitchMachine). On the UI thread
        // (launch, or before the window exists) reflect it immediately.
        bool  offUiThread = (m_hwnd != nullptr) &&
                            (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

        if (offUiThread)
        {
            return;
        }

        SyncSelectorState();

        if (m_hwnd != nullptr)
        {
            RelayoutJoystickButton();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleInputMappingMode
//
//  Advances the input mapping mode Off -> Joystick -> Paddle -> Off.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CycleInputMappingMode()
{
    InputMappingMode  next    = InputMappingMode::Off;
    InputMappingMode  current = DisplayInputMode();



    // Mouse (mouse-capable machines only) deliberately precedes Paddle:
    // entering Paddle CAPTURES the pointer (clicks become fire buttons),
    // so any mode placed after Paddle would be unreachable by clicking
    // the toggle. Mouse mode is non-capturing, so the toggle stays
    // clickable and Paddle remains reachable from it.
    switch (current)
    {
        case InputMappingMode::Off:
            next = InputMappingMode::Joystick;
            break;

        case InputMappingMode::Joystick:
            next = (m_mouse != nullptr && m_mouseConnected)
                       ? InputMappingMode::Mouse
                       : InputMappingMode::Paddle;
            break;

        case InputMappingMode::Mouse:
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
    switch (target)
    {
        case InputMappingMode::Joystick:
            SetArrowsJoystick (!m_arrowsJoystick);
            break;

        case InputMappingMode::Paddle:
            SetPointerMapping (m_pointerMode == InputMappingMode::Paddle
                                   ? InputMappingMode::Off : InputMappingMode::Paddle);
            break;

        case InputMappingMode::Mouse:
            SetPointerMapping (m_pointerMode == InputMappingMode::Mouse
                                   ? InputMappingMode::Off : InputMappingMode::Mouse);
            break;

        default:
            SetInputMappingMode (InputMappingMode::Off);
            break;
    }
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

void EmulatorShell::StartPaddleCapture()
{
    HRESULT  hr      = S_OK;
    RECT     client  = {};
    POINT    topLeft = {};
    POINT    botRt   = {};
    RECT     clip    = {};
    POINT    center  = {};



    BAIL_OUT_IF (m_pointerMode != InputMappingMode::Paddle, S_OK);
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

void EmulatorShell::StopPaddleCapture()
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

void EmulatorShell::PushPaddlePosition()
{
    auto * iieSw    = m_refs.iieSoftSwitches;
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
    auto * iieKbd   = m_refs.iieKeyboard;
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
//  Decides whether a synthesized WM_CHAR belongs to the guest, and drops it
//  otherwise. This handler is almost entirely suppression: Windows manufactures
//  a WM_CHAR from a WM_KEYDOWN whether or not anything consumed the keydown,
//  so every case OnKeyDown claimed has to be claimed again here.
//
//  Three things are swallowed:
//
//    overlay input   a letter typed while the settings panel, an open menu, or
//                    the chrome focus ring owns the keyboard would otherwise
//                    ALSO drop into the //e latch
//    fire keys       X / Z are joystick buttons in joystick mode and were
//                    already handled as key transitions, so their characters
//                    must not type as well -- mirroring how the arrows are
//                    withheld from the latch
//    OS auto-repeat  the host repeat rate would flood $C000 and confuse games
//                    that poll it; the emulated //e generates its own repeat
//                    in CPU time (AppleKeyboard::Tick) from the single latch
//
//  What survives goes through the viewport, not straight to the keyboard, so
//  characters travel the same Dxui path as the key transitions (FR-034).
//
//  Handled is returned either way. A character the shell deliberately
//  suppressed must not reach DefWindowProc any more than one the guest took.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnChar (WPARAM ch, LPARAM lParam)
{
    bool  isRepeat = (lParam & s_kPreviousKeyDownLParamBit) != 0;



    // A host-meta shortcut (Ctrl+V paste, Ctrl+R reset) claimed the keydown,
    // but Windows synthesized its control character anyway; swallow exactly
    // that one char so it never types into the guest.
    if (m_swallowMetaChar)
    {
        m_swallowMetaChar = false;
        return DxuiMessageResult::Handled;
    }



    // Suppress the WM_CHAR that Windows synthesizes from a WM_KEYDOWN
    // already consumed by overlay UI (settings panel / open menu) or by the
    // chrome keyboard-focus ring. Without this, a letter typed while a menu
    // title / button / drive is focused would also drop into the //e latch.
    bool  overlayOwnsIt = m_uiShell.IsCapturingInput() ||
                          m_chromeFocusIndex != s_kChromeFocusNone;

    // In joystick mode the X / Z keys are fire buttons (handled in OnKeyDown
    // / OnKeyUp), so swallow their WM_CHAR to keep the letters from also
    // typing into the //e keyboard latch -- mirroring how arrow keys are
    // withheld from the latch.
    bool  isFireKey = m_arrowsJoystick &&
                      (m_refs.iieSoftSwitches != nullptr ||
                       m_refs.gamePort != nullptr) &&
                      (ch == L'x' || ch == L'X' || ch == L'z' || ch == L'Z');

    // What survives all of the above is the guest's. `isRepeat` drops Windows
    // OS auto-repeat: the host repeat rate would flood $C000 and confuse
    // real-time games that poll it. A fresh press is latched once and
    // registered for the emulator's own authentic //e auto-repeat cadence
    // (driven in CPU time by AppleKeyboard::Tick).
    bool  isGuestChar = m_refs.keyboard != nullptr &&
                        !overlayOwnsIt &&
                        !isRepeat &&
                        !isFireKey;



    // Route through the viewport so the //e keyboard latch is fed on the same
    // Dxui path as the key transitions (FR-034).
    if (isGuestChar && m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Char;
        ev.vk   = ch;

        (void) m_viewport->OnKey (ev);
    }

    // Consumed either way: a character the shell suppressed must not fall
    // through to DefWindowProc any more than one the guest took.
    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
//  The authoritative chrome layout pass. Everything positioned in the window
//  is settled here, in a dependency order that is not interchangeable:
//
//    1. back-buffer size    the CRT post-process needs the new dimensions
//    2. UiShell + menu bar  top-anchored chrome
//    3. viewport layout     settles the desk-scene scale and band heights
//    4. drive widgets       positioned INSIDE the band step 3 just sized
//    5. joystick / switch   the remaining band occupants
//    6. hit-test rects      re-registered from the final geometry
//
//  Step 3 before step 4 is the fix for widgets that lagged one resize behind:
//  laying the widgets against the previous scale left them disagreeing with
//  the band they sit in until the next size event.
//
//  The swap chain is NOT resized here. DxuiHwndSource::HandleSize already did
//  it, along with recreating the back-buffer RTV and D2D target, before this
//  ran -- the renderer no longer owns the swap chain and only needs to be
//  told the new size.
//
//  Machines with no Disk II controller collapse both drive widgets to empty
//  rects rather than skipping the layout, because an empty rect is also what
//  makes the drag-drop overlay treat the whole window as a drop target. The
//  joystick button still lays out: joystick input does not depend on disks.
//  On a //c, the external drive is re-hidden after layout because the shared
//  layout helper un-hides both.
//
//  The disk-presence and //c-ness this size accounts for are recorded here
//  precisely because OnSize is authoritative -- it fires on real WM_SIZE only,
//  never per frame -- so ReflowChromeForMachineChange can read the pre-switch
//  values and grow or shrink the window by the band delta after a machine
//  change that produces no WM_SIZE of its own.
//
//  The synchronous repaint at the end is what keeps an interactive drag-resize
//  from showing stale or black frames: RunMessageLoop is blocked inside the OS
//  modal resize loop and cannot present, so this drives the paint directly.
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
        UINT  dpi           = GetDpiForWindow (m_hwnd);
        RECT  menuBarBounds = {};
        HRESULT  hrUiR           = m_uiShell.OnResize (m_d3dRenderer.GetBackBufferWidth(),
                                                       m_d3dRenderer.GetBackBufferHeight(),
                                                       dpi);

        IGNORE_RETURN_VALUE (hrUiR, S_OK);

        // Fullscreen presentation (FR-014): the scene owns the whole client
        // and every chrome element collapses. The windowed path below is the
        // one that restores everything -- including the host caption -- when
        // fullscreen exits, because this OnSize runs on both transitions.
        if (DeskSceneActive() && m_d3dRenderer.IsFullscreen())
        {
            SetChromeHiddenForFullscreenScene (true);
            UpdateViewportLayout (static_cast<int> (width), renderH);
            m_chromeSizedForHasDisk = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
            m_chromeSizedForApple2c = IsApple2c();
        }
        else
        {

        // Chrome visibility FIRST: the caption height feeds the menu bar's
        // anchor, and a hidden caption reports zero.
        SetChromeHiddenForFullscreenScene (false);

        menuBarBounds = { 0, m_host->CaptionHeightPx(), static_cast<int> (width), m_host->CaptionHeightPx() };
        m_mainMenu.Layout (menuBarBounds, m_scaler);

        // Settle the desk-scene scale (monitor fit + scaled band heights) for
        // THIS size before laying the drive widgets, so widgets and band agree
        // instead of the widgets lagging one resize behind.
        UpdateViewportLayout (static_cast<int> (width), renderH);

        {
            RECT  vr            = ComputeViewportRect (static_cast<int> (width), renderH);
            RECT  driveRect     = m_driveBand.Bounds();
            int   bottomInsetPx = renderH - driveRect.top;   // drive band height only
            bool  fHasDisk      = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();

            (void) vr;                                        // dock side-effect: bands arranged

            if (DeskSceneActive())
            {
                // The 3D scene owns the drives; nothing to lay out. The hit
                // registry is refreshed below with the rest of the chrome.
            }
            else if (fHasDisk)
            {
                LayoutDriveWidgetsInCommandBar (m_driveChrome, bottomInsetPx, static_cast<int> (width), renderH, dpi, m_chromeSceneScale);

                // LayoutDriveWidgetsInCommandBar lays out (and un-hides) BOTH
                // widgets. Re-collapse the external one when it is an optional
                // //c drive the user has not connected, so only the internal
                // drive shows.
                if (!ShouldShowExternalDrive())
                {
                    m_driveChrome[1].Hide();
                }
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

            // OnSize is the authoritative layout (only fires on a real WM_SIZE,
            // never per-frame), so record the disk-presence + //c-ness this
            // window size now accounts for. ReflowChromeForMachineChange reads
            // these pre-switch values to grow/shrink the window by the band delta.
            m_chromeSizedForHasDisk = fHasDisk;
            m_chromeSizedForApple2c = IsApple2c();

            {
                int  bandTop    = driveRect.top;
                int  bandHeight = MulDiv (s_kJoystickButtonBandDp, static_cast<int> (dpi), s_kBaseDpi);

                m_driveBandSurface.SetVisible (!DeskSceneActive());
                m_driveBandSurface.SetBounds (RECT{ 0, bandTop, static_cast<int> (width), renderH });
                LayoutJoystickButton (static_cast<int> (width), bandTop, bandHeight, dpi);
            }

            LayoutSwitchBar (dpi);

            if (DeskSceneActive())
            {
                SyncSceneDriveChrome();
            }
            else
            {
                m_uiShell.HitTest().Clear();
                if (fHasDisk)
                {
                    m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[0].BodyRect(), DxuiHitSlot::Custom, 0 });
                    if (ShouldShowExternalDrive())
                    {
                        m_uiShell.HitTest().Register (DxuiHitRect { m_driveChrome[1].BodyRect(), DxuiHitSlot::Custom, 1 });
                    }
                }
            }
        }

        }   // windowed chrome path
    }

    // (Viewport layout already settled above, before the drive widgets.)

    {
        lock_guard<mutex> lock (m_framebufferMutex);

        if (!m_uiFramebuffer.empty())
        {
            const ThemeCrtDefaults  * themeDefaults = nullptr;
            CrtParams                 params        = {};

            if (m_themeManager != nullptr && m_themeManager->GetActiveTheme() != nullptr)
            {
                themeDefaults = &m_themeManager->ActiveCrtDefaults();
            }

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
//  Composes the caption, and marshals itself to the UI thread when needed.
//
//  The thread check is not defensive coding -- SwitchMachine legitimately
//  calls this from the CPU thread, while DxuiHwndSource::SetTitle mutates the
//  caption bar and asserts the UI thread. An off-thread call therefore posts
//  WM_APP_DXUI_UPDATE_TITLE and returns; the message loop calls back here on
//  the right thread. (That same message doubles as the machine-switch signal
//  to reflow the chrome -- see RunMessageLoop.)
//
//  The caption is deliberately quiet. Running is the expected state and gets
//  no tag at all, so a healthy window reads simply "Casso - <machine>" and
//  only speaks up when something is off: Paused and Stopped are tagged in
//  every build, because those states leave the window looking identical to a
//  running one.
//
//  Debug builds append the full binary identity (version, architecture,
//  compile timestamp) so a window can never be mistaken for a stale rebuild
//  still sitting on screen. It uses the same " - " separator as the machine
//  name so the whole caption reads as one list rather than two grammars.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateWindowTitle()
{
    HRESULT  hr          = S_OK;
    wstring  title;
    wstring  wideName;
    bool     isOffThread = false;



    BAIL_OUT_IF (m_hwnd == nullptr, S_OK);

    // SwitchMachine calls this on the CPU thread; DxuiHwndSource::SetTitle
    // mutates the caption bar and asserts the UI thread. Bounce off-thread
    // callers through the message loop (WM_APP_DXUI_UPDATE_TITLE handler above).
    isOffThread = GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId();

    if (isOffThread)
    {
        PostMessageW (m_hwnd, WM_APP_DXUI_UPDATE_TITLE, 0, 0);
    }

    BAIL_OUT_IF (isOffThread, S_OK);

    title = L"Casso";

    if (!m_config.name.empty())
    {
        wideName = fs::path (m_config.name).wstring();
        title += L" - ";
        title += wideName;
    }

#if defined (_DEBUG)
    // Say it outright. The build-identity stamp below appears on debug builds
    // ONLY, so its presence was already the signal -- but that is a fact about
    // the code, not something a caption reading "v1.17.0 x64 (...)" conveys to
    // anyone looking at it. A debug build is ~6x the CPU of a release one for
    // identical work, so mistaking one for the other sends you measuring the
    // wrong binary.
    title += L" [Debug]";
#endif

    // Flag a paused / stopped emulator in every build -- those states are worth
    // surfacing because the window looks the same either way. Running is the
    // expected state and gets no tag at all, so the caption stays a clean
    // "Casso - <machine>" and only says something when something is off.
    if (m_cpuManager.IsPaused())
    {
        title += L" [Paused]";
    }
    else if (!m_cpuManager.IsRunning())
    {
        title += L" [Stopped]";
    }

#if defined (_DEBUG)
    // Dev builds stamp the exact binary identity (version, arch, compile
    // timestamp) so a window is never mistaken for a stale rebuild. Same " - "
    // separator the machine name uses, so the caption reads as one list.
    title += L" - ";
    title += CassoBuildInfo();
#endif

    m_host->SetTitle (title);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterBannerMessage
//
//  One-line printer summary for the Settings > Printing info banner. The
//  machine-can-print fact comes from the config's enabled slots (core, tested);
//  the wording is host UI copy. //c is slotless, so it reads as no printer.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::PrinterBannerMessage() const
{
    std::wstring  message;



    if (m_config.HasEnabledSlotDevice ("parallel-printer"))
    {
        message = L"Emulating an Apple ImageWriter II connected via parallel interface.";
    }
    else
    {
        message = L"No printer is connected to this " + fs::path (m_config.name).wstring() + L".";
    }

    return message;
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
    HRESULT Create (const std::wstring & reason, const std::wstring & path, uint64_t total)
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
        // and recenter. Sizing the *client* rect (via AdjustWindowRect)
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
        return hr;
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
        LRESULT                result = 0;



        TraceProgressWindow *  self   = reinterpret_cast<TraceProgressWindow *> (
            GetWindowLongPtrW (hwnd, GWLP_USERDATA));

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
        PAINTSTRUCT  ps         = {};
        HDC          hdc        = BeginPaint (hwnd, &ps);
        RECT         rc         = {};
        RECT         bar        = {};
        int          pad        = Scaled (s_kPadPx);
        int          line       = Scaled (s_kLinePx);
        int          barH       = Scaled (s_kBarPx);
        int          gap        = Scaled (s_kGapSmallPx);
        int          pct        = (m_total > 0) ? (int) ((m_done * 100) / m_total) : 0;
        wchar_t      line1[128] = {};
        wchar_t      line3[160] = {};
        HBRUSH       fill       = CreateSolidBrush (RGB (0x2D, 0x7D, 0x46));
        RECT         r1         = {};
        RECT         r2         = {};
        RECT         r3         = {};
        RECT         filled     = {};
        HFONT        oldFont    = nullptr;

        GetClientRect (hwnd, &rc);

        oldFont = (m_font != nullptr) ? (HFONT) SelectObject (hdc, m_font) : nullptr;

        // SetProgress invalidates without erasing (bErase = FALSE) and the
        // text is drawn transparently, so wipe the client area first --
        // otherwise each new progress value piles up on the previous one.
        FillRect (hdc, &rc, (HBRUSH) (COLOR_WINDOW + 1));

        swprintf_s (line1, L"Writing execution trace (%s)\x2026", m_reason.c_str());
        swprintf_s (line3, L"%llu of %llu instructions  (%d%%)",
                    (unsigned long long) m_done, (unsigned long long) m_total, pct);

        SetBkMode   (hdc, TRANSPARENT);

        r1 = { pad, pad, rc.right - pad, pad + line };
        DrawTextW (hdc, line1, -1, &r1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        r2 = { pad, r1.bottom + gap, rc.right - pad, r1.bottom + gap + line };
        DrawTextW (hdc, m_path.c_str(), -1, &r2, DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS);

        r3 = { pad, r2.bottom + gap, rc.right - pad, r2.bottom + gap + line };
        DrawTextW (hdc, line3, -1, &r3, DT_LEFT | DT_SINGLELINE);

        // Anchor the bar to the bottom with a margin equal to the top pad,
        // so the bottom whitespace mirrors the caption-to-text gap and the
        // bar can't be clipped by the window edge at any DPI.
        bar.left   = pad;
        bar.right  = rc.right - pad;
        bar.bottom = rc.bottom - pad;
        bar.top    = bar.bottom - barH;
        FrameRect (hdc, &bar, (HBRUSH) GetStockObject (GRAY_BRUSH));

        filled = bar;
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
    HRESULT       hr             = S_OK;
    bool          expected       = false;
    SYSTEMTIME    st             = {};
    wchar_t       name[64]       = {};
    wchar_t       cwd[MAX_PATH]  = {};
    std::wstring  path;
    uint64_t      total          = 0;
    bool          wonTheRace     = false;
    bool          hasTrace       = false;



    // One-shot: the graceful-exit path and the crash handler both call this,
    // and only the first through gets to write.
    wonTheRace = m_traceDumped.compare_exchange_strong (expected, true);

    BAIL_OUT_IF (!wonTheRace, S_OK);

    hasTrace = m_traceCapacity != 0 && m_cpu != nullptr && m_cpu->IsTraceEnabled();

    BAIL_OUT_IF (!hasTrace, S_OK);

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

    // Scoped so the bails above never jump across the window's construction.
    {
        TraceProgressWindow  win;

        win.Create (reason, path, total);

        hr = m_cpu->DumpTraceToFile (path, [&win] (uint64_t done, uint64_t tot)
        {
            win.SetProgress (done, tot);
        });

        win.Destroy();

        // Tear the progress window down FIRST, then report: the notify is modal,
        // and leaving a progress dialog stranded behind it looks like a hang.
        // This path also runs from the crash handler, where the trace is the only
        // artifact -- silently losing it is the worst possible outcome.
        CHRN (hr, L"Could not write the CPU trace file");
    }

Error:
    return;
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
    HRESULT            hr         = S_OK;
    Disk2Controller  * controller = nullptr;
    int                Disk2Count = 0;
    HINSTANCE          hInstance  = nullptr;
    size_t             i          = 0;



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

        ApplyAppIconToWindow (m_disk2DebugPanel->Hwnd());

        m_disk2DebugPanel->SetUptimeAnchor (m_uptimeAnchor);
        m_disk2DebugPanel->SetMultiControllerHint (Disk2Count > 1);

        if (m_cpu != nullptr)
        {
            m_disk2DebugPanel->SetCycleCounter (m_cpu->GetCycleCounterPtr());
        }

        controller->SetEventSink (m_disk2DebugPanel.get());

        for (auto & diskAudioSource : m_diskAudioSources)
        {
            if (diskAudioSource != nullptr)
            {
                diskAudioSource->SetAudioEventSink (m_disk2DebugPanel.get());
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
//  Shows the input debug panel, creating it on first use.
//
//  The panel is created lazily and then kept: it is a diagnostic window most
//  sessions never open, but one that is toggled repeatedly when it is in use.
//  The re-create test covers a null panel AND a live panel whose HWND has
//  already been destroyed, since closing the window leaves the object behind.
//
//  Creation wires the panel in as an input event SINK on every device that
//  produces input -- keyboard, //e soft switches, game port -- so it observes
//  the real event stream rather than polling state and inventing its own
//  version of what happened.
//
//  A failed Create resets the pointer, so a subsequent open retries cleanly
//  instead of finding a half-built panel and short-circuiting the branch.
//
//  The uptime anchor and cycle counter are handed over so the panel timestamps
//  events in the emulator's own time base, not the host's.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenInputDebugDialog()
{
    HRESULT    hr        = S_OK;
    HINSTANCE  hInstance = nullptr;



    CBR (m_refs.keyboard != nullptr);

    if (m_inputDebugPanel == nullptr || m_inputDebugPanel->Hwnd() == nullptr)
    {
        Apple2eSoftSwitchBank * iieSwitches = nullptr;

        hInstance         = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_inputDebugPanel = std::make_unique<InputDebugPanel>();

        hr = m_inputDebugPanel->Create (hInstance,
                                         m_hwnd,
                                         m_d3dRenderer.GetDevice(),
                                         m_d3dRenderer.GetContext(),
                                         &m_chromeTheme);
        CHRF (hr, m_inputDebugPanel.reset());

        ApplyAppIconToWindow (m_inputDebugPanel->Hwnd());

        m_inputDebugPanel->SetUptimeAnchor (m_uptimeAnchor);

        // $C063 is the //c's active-low mouse button whenever a mouse device
        // is wired up (matching Apple2eKeyboard's own read), and the //e's
        // shift-key mod otherwise.
        m_inputDebugPanel->SetMouseButtonAtC063 (m_mouse != nullptr);

        if (m_cpu != nullptr)
        {
            m_inputDebugPanel->SetCycleCounter (m_cpu->GetCycleCounterPtr());
        }

        m_refs.keyboard->SetInputEventSink (m_inputDebugPanel.get());

        iieSwitches = m_refs.iieSoftSwitches;
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

    for (auto & diskAudioSource : m_diskAudioSources)
    {
        if (diskAudioSource != nullptr)
        {
            diskAudioSource->SetAudioEventSink (m_disk2DebugPanel.get());
        }
    }

    if (m_inputDebugPanel != nullptr && m_refs.keyboard != nullptr)
    {
        Apple2eSoftSwitchBank * iieSwitches = nullptr;

        m_refs.keyboard->SetInputEventSink (m_inputDebugPanel.get());

        iieSwitches = m_refs.iieSoftSwitches;
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


