#pragma once

#include "Pch.h"

#include "Ui/Chrome/ChromeMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShellInternal
//
//  What the shell's translation units share and nothing else needs: the
//  private window messages, the tuning constants, and the one clock helper
//  the banners are timed by. The shell is one class in twelve files, split
//  by concern; this is the file-scope state those files had when it was one.
//
////////////////////////////////////////////////////////////////////////////////

// Private window messages, posted between threads and handled by the window.
#define WM_APP_NOTIFY_USER     (WM_APP + 0x22)
#define WM_APP_REPORT_DAMAGE   (WM_APP + 0x23)
#define WM_APP_MOUNT_COMPLETED (WM_APP + 0x25)
#define WM_APP_CHANGE_REPORT   (WM_APP + 0x26)
#define WM_APP_CHANGE_ASK      (WM_APP + 0x27)
#define WM_APP_SHOW_NOTICE     (WM_APP + 0x28)





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
// Gap between the two drives. The compact presentation needs a wide one,
// because it puts each drive's caption on the same line as the other's rail
// and a narrow gap left "DRIVE 2" reading as a label on drive 1's bar. The
// modeled drives have no caption beside them and keep the close spacing:
// standing them 44 dp apart would push the pair out to the window's edges.
static constexpr int     s_kDriveWidgetGapDp        = 16;
static constexpr int     s_kCompactDriveWidgetGapDp = 44;

//  How long the change band stands before closing itself. Long enough to read
//  twice without hurrying, short enough that a build loop does not leave a
//  strip on the screen all afternoon.
static constexpr int64_t s_kChangeBannerHoldMs  = 30000;





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeBannerNowMs
//
//  Milliseconds off the monotonic clock.
//
//  MONOTONIC, NOT WALL CLOCK. The band's countdown must not lurch when the
//  system clock is corrected under it.
//
////////////////////////////////////////////////////////////////////////////////

inline int64_t ChangeBannerNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                         std::chrono::steady_clock::now().time_since_epoch()).count();
}



static constexpr int     s_kLabelBottomGapDp    = 2;

// Vertical clearance (dp) the desk scene keeps between the monitor and the
// drive row. It began as the band that held the input-mode button under the
// 2D chrome. The button moved to the command toolbar and the band went, but
// the scene was composed against this gap, so the value stays.
static constexpr int     s_kSceneDriveGapDp = 43;

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

// The pointer-capture notice: how to get the mouse back, said for as long as
// it is held. The bar sizes itself to this text; nothing here places it.
static const wchar_t * const  s_kpszCaptureNotice =
    L"Press Esc to release the mouse and exit paddle mode";

// The readout sits in the bottom-left corner, inset far enough that its
// shadow clears the edges.
//
static constexpr int     s_kFrameRateInsetDp        = 12;
static constexpr int     s_kFrameRateWidthDp        = 120;
static constexpr int     s_kFrameRateHeightDp       = 28;

// The scene-pose readout. Wider than the frame rate because it carries five
// numbers, and centered on the glass rather than hung off a corner: the
// picture is the one place a screenshot of the scene always includes.
static constexpr int     s_kScenePoseWidthDp        = 320;
static constexpr int     s_kScenePoseHeightDp       = 24;

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
// -1 = guest (//e has focus); 0..6 = the seven menu titles File..Help; 7..16 =
// the ten toolbar entries in strip order; 17/18 = drive widgets 1/2. The ring
// wraps modulo s_kChromeFocusCount when traversed with Tab.
static constexpr int     s_kChromeFocusNone          = -1;
static constexpr int     s_kChromeFocusMenuFirst     = 0;
static constexpr int     s_kChromeFocusMenuLast      = 6;
static constexpr int     s_kChromeFocusToolbarFirst  = 7;
static constexpr int     s_kChromeFocusToolbarLast   = 16;
static constexpr int     s_kChromeFocusDrive0        = 17;
static constexpr int     s_kChromeFocusDrive1        = 18;
static constexpr int     s_kChromeFocusCount         = 19;





////////////////////////////////////////////////////////////////////////////////
//
// Posted (not sent) to the shell HWND to marshal a window-title refresh
// onto the UI thread when UpdateWindowTitle is called from the CPU thread
// (SwitchMachine). Drained by RunMessageLoop before DispatchMessage.
//
////////////////////////////////////////////////////////////////////////////////

#define WM_APP_DXUI_UPDATE_TITLE (WM_APP + 0x21)
