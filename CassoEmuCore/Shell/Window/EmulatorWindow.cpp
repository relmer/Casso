#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/WindowCommandManager.h"
#include "Shell/Components/ShellWindow.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/WindowManager.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/DiskManager.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/Components/ShellPrinter.h"
#include "Shell/Components/ShellUpdater.h"
#include "Update/UpdateResult.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Devices/Disk/WriteProtectChange.h"





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
    if (m_window->TryGetCursorMonitorWorkArea (work, activeMon))
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
        SIZE  client = m_window->GetClientSizeForFramebufferPx (kFramebufferWidth, kFramebufferHeight);

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

    haveWork = m_window->TryGetCursorMonitorWorkArea (work, activeMon);

    if (haveWork)
    {
        // The 100%-emulator + full monitor framing can want a window taller
        // than the display; never open larger than the work area. The monitor
        // frame then lays out its housing into whatever center it gets, so the
        // emulator lands at 100% when it fits and the largest size that fits
        // otherwise.
        windowW = std::min (windowW, (int) (work.right  - work.left));
        windowH = std::min (windowH, (int) (work.bottom - work.top));
        m_window->CenterInWorkArea (work, windowW, windowH, windowX, windowY);
    }

    hadSavedPlacement = m_window->m_windowManager->TryLoadSavedWindowPlacement (activeMon, windowX, windowY, windowW, windowH, m_window->m_startMaximized);

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

    m_hwnd = m_host->GetHwnd();
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
    m_host->GetRoot().SetLayout (std::make_unique<DxuiAbsoluteLayout>());
    m_viewport = &m_host->GetRoot().Add<DxuiViewport>();
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
    m_host->GetRoot().Adopt (m_chrome->m_mainMenu);
    m_host->GetRoot().Adopt (m_chrome->m_driveBandSurface);
    m_host->GetRoot().Adopt (m_disks->GetDriveChrome()[0]);
    m_host->GetRoot().Adopt (m_disks->GetDriveChrome()[1]);
    m_host->GetRoot().Adopt (m_tapeDeck->GetWidget());
    m_host->GetRoot().Adopt (m_chrome->m_fpsReadout);
    m_host->GetRoot().Adopt (m_scene->m_sceneViewReadout);
    m_host->GetRoot().Adopt (m_scene->m_sceneDriveLabel[0]);
    m_host->GetRoot().Adopt (m_scene->m_sceneDriveLabel[1]);
    m_host->GetRoot().Adopt (m_scene->m_sceneDriveInfoIcon[0]);
    m_host->GetRoot().Adopt (m_scene->m_sceneDriveInfoIcon[1]);
    m_host->GetRoot().Adopt (m_scene->m_stripTapeLabel[0]);
    m_host->GetRoot().Adopt (m_scene->m_stripTapeLabel[1]);
    m_host->GetRoot().Adopt (m_scene->m_stripTapeLabel[2]);
    m_host->GetRoot().Adopt (m_scene->m_sceneCompass);
    m_host->GetRoot().Adopt (m_scene->m_compassHint);

    // The compass reports gestures; the shell owns what they mean. The signs
    // follow the drag's bargain -- the CONTENT goes where the arrow points --
    // so the right arrow and a rightward drag turn the scene the same way.
    // With Ctrl held the compass pans instead, as Ctrl does to a drag on the
    // scene itself: an arrow click moves the scene a pan step its way, and a
    // drag moves it with the pointer.
    m_scene->m_sceneCompass.SetOnStep ([this] (DxuiOrbitControl::Part part)
    {
        float  step = ShellDeskScene::s_kScenePanStep;

        if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
        {
            switch (part)
            {
                case DxuiOrbitControl::Part::Left:   m_scene->PanSceneByCompass (-step, 0.0f); break;
                case DxuiOrbitControl::Part::Right:  m_scene->PanSceneByCompass ( step, 0.0f); break;
                case DxuiOrbitControl::Part::Up:     m_scene->PanSceneByCompass (0.0f, -step); break;
                case DxuiOrbitControl::Part::Down:   m_scene->PanSceneByCompass (0.0f,  step); break;
                default: break;
            }

            return;
        }

        switch (part)
        {
            case DxuiOrbitControl::Part::Left:   m_scene->OrbitSceneBy ( ShellDeskScene::kCompassStepYawRad,   0.0f); break;
            case DxuiOrbitControl::Part::Right:  m_scene->OrbitSceneBy (-ShellDeskScene::kCompassStepYawRad,   0.0f); break;
            case DxuiOrbitControl::Part::Up:     m_scene->OrbitSceneBy (0.0f, -ShellDeskScene::kCompassStepPitchRad); break;
            case DxuiOrbitControl::Part::Down:   m_scene->OrbitSceneBy (0.0f,  ShellDeskScene::kCompassStepPitchRad); break;
            default: break;
        }
    });

    m_scene->m_sceneCompass.SetOnDrag ([this] (DxuiOrbitControl::Part part, float dxPx, float dyPx)
    {
        float  rate = m_scene->OrbitRadPerPx();

        if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
        {
            const RECT &  vp = m_scene->m_deskScene.Composition().viewportPx;

            if (vp.right > vp.left && vp.bottom > vp.top)
            {
                m_scene->PanSceneByCompass (dxPx / (float) (vp.right - vp.left) * 2.0f,
                                            dyPx / (float) (vp.bottom - vp.top) * 2.0f);
            }

            return;
        }

        // Axis-locked to the arrow the drag started on: the arrow names an
        // axis, and a free two-axis tumble from a single arrow would make
        // the four of them meaningless.
        switch (part)
        {
            case DxuiOrbitControl::Part::Left:
            case DxuiOrbitControl::Part::Right:  m_scene->OrbitSceneBy (-dxPx * rate, 0.0f); break;
            case DxuiOrbitControl::Part::Up:
            case DxuiOrbitControl::Part::Down:   m_scene->OrbitSceneBy (0.0f,  dyPx * rate); break;
            default: break;
        }
    });

    // Ctrl+click on the center is Ctrl+0: the whole view reset, zoom and
    // window size included, with the window's placement saved.
    m_scene->m_sceneCompass.SetOnHome ([this] ()
    {
        if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
        {
            PostMessage (m_hwnd, WM_COMMAND, IDM_VIEW_RESET_SIZE, 0);
            return;
        }

        m_scene->m_sceneView.orbitYawRad   = 0.0f;
        m_scene->m_sceneView.orbitPitchRad = 0.0f;
        m_scene->InvalidateSceneComposition();
    });
    m_host->GetRoot().Adopt (m_chrome->m_toolbar);
    m_host->GetRoot().Adopt (m_chrome->m_switchBar);
    m_host->GetRoot().Adopt (m_disks->GetChangeBanner());
    //  The backing goes in FIRST: the root paints its children in the order
    //  they were adopted, so the panel lands under the words rather than over
    //  them.
    m_chrome->m_standInBarSurface.SetToken (DxuiSurface::Token::Background);
    m_host->GetRoot().Adopt (m_chrome->m_standInBarSurface);
    m_host->GetRoot().Adopt (m_chrome->m_standInBar);

    //  FIXED WORDS, SET ONCE. The bar says the same thing every time it is up,
    //  and its band is measured from that text before the bar has ever been
    //  shown -- so the text cannot wait until the first capture to exist.
    m_chrome->m_standInBar.SetSeverity (DxuiInfoBanner::Severity::Info);

    //  CENTERED, because this bar spans the window rather than sitting in a
    //  dialog: one short line held against the leading edge of a wide strip
    //  reads as something that failed to lay out.
    m_chrome->m_standInBar.SetCentered (true);
    m_chrome->m_standInBar.SetVisible  (false);

    //  THE NOTICES, ADOPTED AFTER THE CAPTURE BAR so that when both are up
    //  the notices are on top -- they are the newer, and the older one is
    //  still readable in the strip above them.
    m_host->GetRoot().Adopt (m_chrome->m_notices);

    // Give the host the chrome theme so its paint pump renders the
    // adopted chrome -- PaintPump no-ops when no theme is set.
    // m_chromeTheme is reassigned in place on theme switches, so this
    // pointer stays valid and the host reads the updated palette on the
    // next paint.
    m_host->SetTheme (&m_chrome->m_chromeTheme);

    // Route the menu bar's open submenu through the host popup pool so
    // the dropdown renders as a real top-level window (escapes the
    // client area + occludes). The strip stays in-window. The
    // full-ownership host owns the device, so its pool makes real popups.
    m_chrome->m_mainMenu.SetPopupHost (m_host.get());

    // The hover tooltips render through the host popup pool too; their
    // dwell timers are driven from the main frame loop's Tick. SetTheme
    // seeds the tooltip surface colors.
    m_chrome->m_toolbarTooltip.SetPopupHost  (m_host.get());
    m_chrome->m_toolbarTooltip.SetTheme      (m_chrome->m_chromeTheme);

    // The //c switch strip shares the same deferred-tooltip pattern.
    m_chrome->m_switchBarTooltip.SetPopupHost (m_host.get());
    m_chrome->m_switchBarTooltip.SetTheme     (m_chrome->m_chromeTheme);
    // The drive-widget write-protect tooltip shares the host popup pool.
    // It surfaces on a dwell over a write-protected drive and names the
    // protection source(s).
    m_chrome->m_driveTooltip.SetPopupHost (m_host.get());
    m_chrome->m_driveTooltip.SetTheme     (m_chrome->m_chromeTheme);

    // The caption buttons' tooltip, in place of the stock system one.
    m_chrome->m_captionTooltip.SetPopupHost (m_host.get());
    m_chrome->m_captionTooltip.SetTheme     (m_chrome->m_chromeTheme);

    // Defer the size reconcile until after ShowWindow. The NC frame
    // (border carve-out from DefWindowProc + DWM rounded corners +
    // thick frame) doesn't materialize until the window is shown,
    // so measuring NC overhead now returns 0 and the reconcile would
    // shrink the window to match the (wrong) measurement. The flag
    // tells ReconcileInitialClientSize whether to run; saved
    // placement deliberately bypasses the reset-to-default sizing.
    m_window->m_initialSizeReconciled = hadSavedPlacement;

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
        RECT  menuBarBounds = { 0, m_host->GetCaptionHeightPx(), clientW, m_host->GetCaptionHeightPx() };

        m_chrome->m_mainMenu.Layout (menuBarBounds, m_scaler);
    }

    m_chrome->m_mainMenu.SetDispatch ([this] (WORD commandId) { HandleCommand (commandId); });

    // Command toolbar: its entries are the same command table the menu
    // reads, so a click dispatches through HandleCommand like a menu row;
    // the volume group drives the master output gain and persists in
    // GlobalUserPrefs through the coalescing save below.
    m_chrome->m_mainMenu.GetCommands().BuildToolbar (m_chrome->m_toolbar, m_printer->GetLed(), m_chrome->m_volumeFlyout);

    // Mouse mode routes through the same toggle the band selector used, so
    // the leave-time release of a held guest button runs identically. It is
    // offered only where there is a mouse to drive: the //c.
    m_chrome->m_mainMenu.GetCommands().SetMouseModeFns (
        [this] () { return m_pointerMode == InputMappingMode::Mouse; },
        [this] () { return m_machine.GetMouse() != nullptr && m_mouseConnected; },
        [this] () { ToggleInputMappingMode (InputMappingMode::Mouse); });

    m_chrome->m_volumeFlyout.SetSink ([this] (float volume01, bool muted)
    {
        m_settings->GetPrefs().masterVolume = volume01;
        m_settings->GetPrefs().masterMuted  = muted;
        m_audio->GetOutput().SetMasterGain (muted ? 0.0f : volume01);
        m_chrome->m_mainMenu.GetCommands().SetMuted (muted);

        // Deferred, not immediate: the slider reports every intermediate
        // value, so a save here would rewrite the prefs file on each tick of
        // a drag.
        m_settings->SaveGlobalPrefsDeferred();
    });
    m_chrome->m_volumeFlyout.SetVolume (m_settings->GetPrefs().masterVolume, m_settings->GetPrefs().masterMuted);
    m_chrome->m_mainMenu.GetCommands().SetMuted (m_settings->GetPrefs().masterMuted);
    m_audio->GetOutput().SetMasterGain (m_settings->GetPrefs().masterMuted ? 0.0f : m_settings->GetPrefs().masterVolume);

    // The theme + monitor-color pickers, and the catalog behind the first of
    // them. Both option lists render through the host popup pool for the same
    // reason the menu bar's does: they hang off the strip over the viewport.
    WireToolbarPickers();
    m_chrome->RefreshToolbarThemeList();
    m_chrome->SyncToolbarState();
    m_chrome->m_mainMenu.SetCheckQuery ([this] (WORD commandId) -> bool
    {
        switch (commandId)
        {
            case IDM_MACHINE_ARROWS_JOYSTICK: return m_arrowsJoystick;
            case IDM_MACHINE_ARROWS_PADDLE:   return m_pointerMode == InputMappingMode::Paddle;
            case IDM_VIEW_FRAME_RATE:         return m_settings->GetPrefs().showFrameRate;
            case IDM_VIEW_SCENE_VIEW:         return m_settings->GetPrefs().showSceneView;

            default:                          return false;
        }
    });

    m_chrome->m_mainMenu.GetCommands().SetPlayerPickedFn (
        [this] (size_t player, const PlayerEntry & entry)
        {
            PickPlayer (player, entry);
        });

    m_chrome->m_mainMenu.GetCommands().SetPlayerModeFn (
        [this] (size_t player, PlayerMode mode)
        {
            SetPlayerMode (player, mode);
        });

    m_chrome->m_mainMenu.GetCommands().SetProfilePickedFn (
        [this] (const ControllerUnitKey & unit, const std::string & profileName)
        {
            PickControllerProfile (unit, profileName);
        });

    m_chrome->m_mainMenu.GetCommands().SetNewProfileFn ([this] (const ControllerUnitKey & unit)
    {
        StartNewControllerProfile (unit);
    });

    m_chrome->m_mainMenu.SetEnableQuery ([this] (WORD commandId) -> bool
    {
        switch (commandId)
        {
            case IDM_DISK_WP1:      return m_disks->IsWriteProtectToggleOffered (0);
            case IDM_DISK_WP2:      return m_disks->IsWriteProtectToggleOffered (1);
            case IDM_DISK_SALVAGE1: return m_disks->IsSalvageOffered (0);
            case IDM_DISK_SALVAGE2: return m_disks->IsSalvageOffered (1);
            case IDM_STORAGE_DRIVE2:   return m_disks->IsSecondDriveOffered();
            case IDM_STORAGE_RECORDER: return m_tapeDeck->MachineHasCassettePort();
            case IDM_DISK_INSERT2:     return m_disks->ShouldShowExternalDrive();
            case IDM_DISK_EJECT2:      return m_disks->ShouldShowExternalDrive();
            case IDM_TAPE_INSERT:      return m_tapeDeck->IsTapeRecorderShown();
            case IDM_TAPE_NEW:         return m_tapeDeck->IsTapeRecorderShown();
            case IDM_TAPE_PLAY:        return m_tapeDeck->IsTapeRecorderShown() && TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,        m_tapeDeck->GetTapeView());
            case IDM_TAPE_STOP:        return m_tapeDeck->IsTapeRecorderShown() && TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Stop,        m_tapeDeck->GetTapeView());
            case IDM_TAPE_REWIND:      return m_tapeDeck->IsTapeRecorderShown() && TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Rewind,      m_tapeDeck->GetTapeView());
            case IDM_TAPE_FASTFORWARD: return m_tapeDeck->IsTapeRecorderShown() && TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::FastForward, m_tapeDeck->GetTapeView());
            case IDM_TAPE_EJECT:       return m_tapeDeck->IsTapeRecorderShown() && TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Eject,       m_tapeDeck->GetTapeView());
            default:           return true;
        }
    });

    m_chrome->m_mainMenu.SetLabelQuery ([this] (WORD commandId) -> std::wstring
    {
        switch (commandId)
        {
            // The action the click takes: attach what is not there, detach
            // what is. The //c's second drive is an external unit on its
            // disk port, and its menu item shows that.
            case IDM_STORAGE_DRIVE2:
            {
                bool  isC = m_machine.GetConfig().systemRom.romBankSize != 0;

                if (m_disks->ShouldShowExternalDrive())
                {
                    return isC ? std::wstring (L"Detach &external drive") : std::wstring (L"Detach &drive 2");
                }

                return isC ? std::wstring (L"Attach &external drive") : std::wstring (L"Attach &drive 2");
            }

            case IDM_STORAGE_RECORDER:
            {
                return m_tapeDeck->IsTapeRecorderShown() ? std::wstring (L"Detach ca&ssette recorder")
                                                         : std::wstring (L"Attach ca&ssette recorder");
            }

            case IDM_DISK_WP1:
            case IDM_DISK_WP2:
            {
                // Give the mounted image and the ACTION the click will take:
                // write-enable when the WOZ flag or the read-only attribute
                // protects it, write-protect otherwise. The per-drive user
                // preference is a different toggle and does not flip it. An
                // empty return keeps the static "Write-protect disk N" label
                // for an empty (disabled) drive.
                int           drive = (commandId == IDM_DISK_WP1) ? 0 : 1;
                DiskImage  *  image = m_machine.GetDiskStore().GetImage (6, drive);
                std::wstring  name;

                if (image == nullptr)
                {
                    return std::wstring();
                }

                name = std::filesystem::path (
                           m_machine.GetDiskStore().GetSourcePath (6, drive)).filename().wstring();

                if (name.empty())
                {
                    return std::wstring();
                }

                return WriteProtectChange::GetMenuLabel (
                           WriteProtectChange::IsImageProtected (image->GetWriteProtectInfo()), name);
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

        hrIcon = m_window->LoadIconAsPremulBgra (hInstance, IDI_CASSO, 32, iconPixels, iconW, iconH);

        if (SUCCEEDED (hrIcon))
        {
            m_host->SetCaptionIcon (std::move (iconPixels), iconW, iconH);
        }
    }

    m_disks->GetDriveChrome()[0].Initialize (6, 0, m_disks.get());
    m_disks->GetDriveChrome()[1].Initialize (6, 1, m_disks.get());

    // Settle the desk-scene scale (monitor fit + band heights) BEFORE laying
    // the drive widgets, so they are born at the settled scale rather than
    // the 1.0 default.
    m_chrome->UpdateViewportLayout (clientW, clientH);

    {
        RECT  vr            = m_chrome->ComputeViewportRect (clientW, clientH);
        RECT  driveRect     = m_chrome->m_driveBand.GetBounds();
        int   bottomInsetPx = clientH - driveRect.top;   // drive band height only

        (void) vr;                                        // dock side-effect: bands arranged

        if (DeskSceneActive())
        {
            m_scene->SyncSceneDriveChrome();
        }
        else
        {
            m_chrome->LayoutDriveWidgetsInCommandBar (m_disks->GetDriveChrome(), bottomInsetPx, clientW, clientH, dpi, m_chrome->m_chromeSceneScale,
                                                      m_disks->ShouldShowExternalDrive() ? 2 : 1);
        }

        m_chrome->m_driveBandSurface.SetVisible (!DeskSceneActive());
        m_chrome->m_driveBandSurface.SetBounds (RECT{ 0, driveRect.top, clientW, clientH });

        m_chrome->LayoutSwitchBar (dpi);
    }

    // Load accelerator table
    m_window->m_accelTable = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR));
    CWRA (m_window->m_accelTable);

Error:
    return hr;
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
    m_renderer->m_d3dRenderer.SetTargetBounds (boundsPx);
    m_renderer->m_d3dRenderer.MarkRedrawNeeded();

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

    if (m_chrome->m_mainMenu.IsOpen())
    {
        m_chrome->m_mainMenu.Hide();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnExitSizeMove
//
//  WHERE THE WINDOW'S PLACEMENT IS PERSISTED, and the only place a move or a
//  drag-resize is.
//
//  It used to be saved from OnMove and OnSize, which fire for a PROGRAMMATIC
//  SetWindowPos exactly as they do for the user: a script that positioned the
//  window to photograph it, or any tool that nudged it, silently overwrote the
//  size and place the user had chosen. The OS drag loop runs only for a real
//  drag of the caption or a border, so its end is the moment that means "the
//  user put it here".
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnExitSizeMove()
{
    m_window->m_windowManager->SaveWindowPlacement (m_hwnd, m_renderer->m_d3dRenderer.IsFullscreen());
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnUserWindowStateCommand
//
//  Notes that the maximize or restore about to happen is the USER'S. The
//  resize has not run yet, so the placement is not readable here; OnSize
//  spends the flag once the window has actually changed.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnUserWindowStateCommand()
{
    m_window->m_userStateChange = true;
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
    m_renderer->m_frameReadyEvent = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    CWRA (m_renderer->m_frameReadyEvent);

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
    m_disks->GetManager()->SetColdBootMountWindow (false);

    // UI thread loop: process messages, present latest framebuffer with vsync
    while (m_cpuManager.IsRunning())
    {
        // Destroy a closed modeless settings sheet at a safe point: its
        // EndDialog callback ran deep inside DispatchMessage, so deferring the
        // reset here avoids tearing the window down from its own message handler.
        if (m_settings->TryDestroyClosedSheet())
        {
            // The sheet may have created, renamed or deleted profiles, and
            // the command bar's profile list is built from them.
            SyncPaddleSourceList();
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
            // Modeless Settings: let the sheet claim its dialog-navigation keys
            // (Tab / Enter / Escape) first (Dxui's IsDialogMessage equivalent).
            if (m_settings->GetSheet() != nullptr && m_settings->GetSheet()->ProcessDialogMessage (msg))
            {
                continue;
            }

            // Suppress the emulator's accelerators while the settings sheet is
            // the active window, so keystrokes meant for it (the color-picker
            // hex field, Ctrl chords) never leak into emulator menu commands.
            bool  settingsActive = (m_settings->GetSheet() != nullptr &&
                                    m_settings->GetSheet()->GetHwnd() == GetActiveWindow());

            if (settingsActive ||
                m_window->m_accelTable == nullptr ||
                !TranslateAccelerator (m_hwnd, m_window->m_accelTable, &msg))
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
        if (!m_renderer->TryPresentUiFrame())
        {
            WaitForFrameOrMessage();
        }
        else if (!m_updater->HasCheckStarted())
        {
            // After the first frame is on screen, so the update check never
            // stands between a launch and a picture.
            m_updater->StartAutomaticUpdateCheck();
        }
    }

    m_cpuManager.Stop();

Error:
    DestroyFrameReadyEvent();
    return exitCode;
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
    m_renderer->TryPresentUiFrame();
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
//  live Settings edits, sliding notices) forces a present through
//  NeedsPresent and so never reaches this path.
//
//  A notice's expiry is a change due at a known time, so the wait ends no
//  later than that, and the notice leaves when its time is up rather than on
//  the next upkeep tick after it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::WaitForFrameOrMessage()
{
    DWORD                   timeout      = s_kIdleUpkeepMs;
    DWORD                   waited       = 0;
    std::optional<int64_t>  nextChangeMs = m_chrome->m_notices.GetNextChangeMs();
    std::optional<int64_t>  shimmerMs    = m_updater->GetMsUntilShimmer ((int64_t) GetTickCount64());
    int64_t                 untilMs      = 0;
    int64_t                 nowMs        = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                               std::chrono::steady_clock::now().time_since_epoch()).count();



    if (m_chrome->m_switchBarTooltip.WantsTick() ||
        m_chrome->m_driveTooltip.WantsTick()     ||
        m_chrome->m_captionTooltip.WantsTick()   ||
        m_scene->m_sceneCompass.WantsTick()     ||
        m_scene->m_compassHintOpacity != (m_scene->m_sceneCompass.IsHovered() ? 1.0f : 0.0f) ||
        m_chrome->m_mainMenu.WantsTick()         ||
        (m_host != nullptr && m_host->GetContextMenu().WantsTick()) ||
        m_chrome->m_toolbar.WantsTick())
    {
        timeout = s_kIdleAnimationTickMs;
    }

    if (nextChangeMs.has_value())
    {
        untilMs = std::clamp (*nextChangeMs - nowMs, (int64_t) 0, (int64_t) timeout);
        timeout = (DWORD) untilMs;
    }

    // Wake for the update indicator's next shimmer sweep, no sooner.
    if (shimmerMs.has_value())
    {
        timeout = (DWORD) std::clamp (*shimmerMs, (int64_t) 0, (int64_t) timeout);
    }

    waited = MsgWaitForMultipleObjectsEx (1, &m_renderer->m_frameReadyEvent, timeout,
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
    if (m_renderer->m_frameReadyEvent != nullptr)
    {
        CloseHandle (m_renderer->m_frameReadyEvent);
        m_renderer->m_frameReadyEvent = nullptr;
    }
}





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
    // NOT SAVED HERE. Exit is not a placement the user chose: whatever the
    // window happened to be doing when it closed would overwrite what they
    // last put it at deliberately. The two paths above have already stored
    // every change that was theirs.

    // P6 -- revoke the IDropTarget before the HWND is destroyed.
    // RevokeDragDrop requires a valid window handle.
    m_window->m_dragDropTarget.Shutdown();

    // Join the printer drain thread before teardown frees the card, and
    // persist the pending strip on clean exit (FR-026); empty clears any stale
    // sidecar. Loss on abnormal termination is acceptable per the spec.
    m_printer->StopAndSavePendingStrip();

    m_cpuManager.Stop();

    // IDxuiHostClient::OnDestroy is notification-only — the host
    // does NOT call PostQuitMessage. EmulatorShell is the
    // application's main window, so it owns that call.
    PostQuitMessage (0);
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

    // Controller input follows the application, not one window: the Settings
    // sheet is Casso too, and XInput was measured still delivering while
    // another application was in front, so the gate has to be ours.
    if (m_controllerService != nullptr)
    {
        m_controllerService->SetActive (active);
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
//  OnCancelMode
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnCancelMode()
{

    //  THE ECHO OF OUR OWN LAYOUT IS NOT A TAKEOVER. Docking the capture bar
    //  resizes the picture, and Windows answers a resize by cancelling the
    //  mode of whoever holds the pointer -- so the pass that makes room for
    //  the notice saying the mouse is held would be the thing that lets it go.
    //
    //  Exactly ONE cancel is swallowed, only just after that dock, and only
    //  while this window is still the foreground one with no menu open. Every
    //  other cancel -- a modal, a secure desktop, a real takeover -- releases
    //  the pointer as it always did, which is what keeps a hidden cursor from
    //  being stranded behind someone else's window.
    if (m_paddleCaptured && m_chrome->m_captureReflowMs != 0
        && ChangeBannerNowMs() - m_chrome->m_captureReflowMs <= ShellChrome::s_kCaptureReflowEchoMs
        && GetForegroundWindow() == m_hwnd
        && !m_chrome->m_mainMenu.IsOpen())
    {
        m_chrome->m_captureReflowMs = 0;
        SetCapture (m_hwnd);
        ClipPaddleCursorToClient();

        return DxuiMessageResult::Handled;
    }

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
    minClient = m_chrome->GetClientSizeForCenterPx (m_scaler.ToPx (s_kMinCenterWidthDp),
                                                    m_scaler.ToPx (s_kMinCenterHeightDp));

    // Never narrower than the menu strip's content so every title stays
    // on-strip. The width is physical client px, the same space as minClient.
    menuWidthPx = m_chrome->m_mainMenu.GetMenuStripContentWidthPx() + m_scaler.ToPx (s_kMenuRightPadDp);

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

    m_chrome->m_inChromeLayout = true;

    // A resize restretches the window; drop any open menu so its
    // window-anchored popup is not left stranded.
    if (m_chrome->m_mainMenu.IsOpen())
    {
        m_chrome->m_mainMenu.Hide();
    }

    // The host (DxuiHwndSource::HandleSize) already resized its swap
    // chain and recreated the back-buffer RTV + D2D target before this
    // OnSize fired. The renderer no longer owns the swap chain; it just
    // needs the new back-buffer dimensions for the CRT post-process.
    m_renderer->m_d3dRenderer.SetBackBufferSize (static_cast<int> (width), renderH);

    {
        UINT  dpi           = GetDpiForWindow (m_hwnd);
        RECT  menuBarBounds = {};
        HRESULT  hrUiR           = m_uiShell.OnResize (m_renderer->m_d3dRenderer.GetBackBufferWidth(),
                                                       m_renderer->m_d3dRenderer.GetBackBufferHeight(),
                                                       dpi);

        IGNORE_RETURN_VALUE (hrUiR, S_OK);

        // THE SCALER TAKES THE WINDOW'S DPI BEFORE ANYTHING IS LAID OUT. A
        // move to a monitor of another DPI resizes the window from inside the
        // host's WM_DPICHANGED, so this WM_SIZE arrives before OnDpiChanged
        // has updated the scaler. Laying out at the old DPI against a caption
        // already at the new one overlapped the menu strip and the command
        // bar, and nothing laid them out again until the next resize.
        if (dpi != 0)
        {
            m_scaler.SetDpi (dpi);
        }

        // Fullscreen presentation (FR-014): the picture owns the whole client
        // and every chrome element collapses, whichever theme is on. The
        // windowed path below is the one that restores everything --
        // including the host caption -- when fullscreen exits, because this
        // OnSize runs on both transitions.
        if (m_renderer->m_d3dRenderer.IsFullscreen())
        {
            m_chrome->SetChromeHiddenForFullscreenScene (true);
            m_chrome->UpdateViewportLayout (static_cast<int> (width), renderH);
            m_chrome->m_chromeSizedForHasDisk = (m_disks->GetManager() != nullptr) && m_disks->GetManager()->HasSlot6Controller();
            m_chrome->m_chromeSizedForApple2c = MachineHasCaseSwitches();
        }
        else
        {

        // Chrome visibility FIRST: the caption height feeds the menu bar's
        // anchor, and a hidden caption reports zero.
        m_chrome->SetChromeHiddenForFullscreenScene (false);

        menuBarBounds = { 0, m_host->GetCaptionHeightPx(), static_cast<int> (width), m_host->GetCaptionHeightPx() };
        m_chrome->m_mainMenu.Layout (menuBarBounds, m_scaler);

        // Settle the desk-scene scale (monitor fit + scaled band heights) for
        // THIS size before laying the drive widgets, so widgets and band agree
        // instead of the widgets lagging one resize behind.
        m_chrome->UpdateViewportLayout (static_cast<int> (width), renderH);

        {
            RECT  vr            = m_chrome->ComputeViewportRect (static_cast<int> (width), renderH);
            RECT  driveRect     = m_chrome->m_driveBand.GetBounds();
            int   bottomInsetPx = renderH - driveRect.top;   // drive band height only
            bool  fHasDisk      = (m_disks->GetManager() != nullptr) && m_disks->GetManager()->HasSlot6Controller();

            (void) vr;                                        // dock side-effect: bands arranged

            if (DeskSceneActive())
            {
                // The 3D scene owns the drives; nothing to lay out. The hit
                // registry is refreshed below with the rest of the chrome.
            }
            else if (fHasDisk)
            {
                m_chrome->LayoutDriveWidgetsInCommandBar (m_disks->GetDriveChrome(), bottomInsetPx, static_cast<int> (width), renderH, dpi,
                                                          m_chrome->m_chromeSceneScale, m_disks->ShouldShowExternalDrive() ? 2 : 1);

                // LayoutDriveWidgetsInCommandBar lays out (and un-hides) BOTH
                // widgets. Re-collapse the external one when it is an optional
                // //c drive the user has not connected, so only the internal
                // drive shows.
                if (!m_disks->ShouldShowExternalDrive())
                {
                    m_disks->GetDriveChrome()[1].Hide();
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
                m_disks->GetDriveChrome()[0].Hide();
                m_disks->GetDriveChrome()[1].Hide();
            }

            // OnSize is the authoritative layout (only fires on a real WM_SIZE,
            // never per-frame), so record the disk-presence + //c-ness this
            // window size now accounts for. ReflowChromeForMachineChange reads
            // these pre-switch values to grow/shrink the window by the band delta.
            m_chrome->m_chromeSizedForHasDisk = fHasDisk;
            m_chrome->m_chromeSizedForApple2c = MachineHasCaseSwitches();

            m_chrome->m_driveBandSurface.SetVisible (!DeskSceneActive());
            m_chrome->m_driveBandSurface.SetBounds (RECT{ 0, driveRect.top, static_cast<int> (width), renderH });

            m_chrome->LayoutSwitchBar (dpi);

            if (DeskSceneActive())
            {
                m_scene->SyncSceneDriveChrome();
            }
            else
            {
                m_uiShell.GetHitTester().Clear();
                if (fHasDisk)
                {
                    m_uiShell.GetHitTester().Register (DxuiHitRect { m_disks->GetDriveChrome()[0].GetBodyRect(), DxuiHitSlot::Custom, 0 });
                    if (m_disks->ShouldShowExternalDrive())
                    {
                        m_uiShell.GetHitTester().Register (DxuiHitRect { m_disks->GetDriveChrome()[1].GetBodyRect(), DxuiHitSlot::Custom, 1 });
                    }
                }

                m_tapeDeck->RegisterTapeDropTarget();
            }
        }

        }   // windowed chrome path
    }

    // (Viewport layout already settled above, before the drive widgets.)

    {
        lock_guard<mutex> lock (m_renderer->m_framebufferMutex);

        if (!m_renderer->m_uiFramebuffer.empty())
        {
            CrtParams  params = {};

            params = MakeCrtParams (m_renderer->ResolveCrtForCurrentMode(),
                                    (float) m_renderer->m_d3dRenderer.GetBackBufferWidth(),
                                    (float) m_renderer->m_d3dRenderer.GetBackBufferHeight());
            m_renderer->m_d3dRenderer.SetCrtParams (params);

            m_renderer->m_pendingFramebuffer = m_renderer->m_uiFramebuffer.data();
        }
    }

    // Repaint immediately at the new size through the host pump. The
    // host already resized its swap chain in HandleSize; driving a
    // synchronous WM_PAINT here avoids a stale / black frame during an
    // interactive drag-resize (when RunMessageLoop is blocked in the OS
    // modal resize loop).
    InvalidateRect (m_hwnd, nullptr, FALSE);
    UpdateWindow   (m_hwnd);

    // A MAXIMIZE OR RESTORE THE USER ASKED FOR, carried out. Those never
    // enter the OS drag loop, so OnExitSizeMove cannot see them; the flag
    // is what says this one was theirs rather than a programmatic
    // ShowWindow, which produces an identical WM_SIZE.
    if (m_window->m_userStateChange)
    {
        m_window->m_userStateChange = false;
        m_window->m_windowManager->SaveWindowPlacement (m_hwnd, m_renderer->m_d3dRenderer.IsFullscreen());
    }

    m_chrome->m_inChromeLayout = false;

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
//  The coalescing global-prefs write. It is a one-shot: the timer is armed by
//  SaveGlobalPrefsDeferred, re-armed by each further change, and killed here
//  once the changes have stopped long enough for it to fire. Also the poll
//  for another instance's update check record.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnTimer (UINT_PTR timerId)
{
    HRESULT  hr = S_OK;



    if (timerId == ShellUpdater::kSharedCheckTimerId)
    {
        m_updater->PollSharedCheckRecord();
        return DxuiMessageResult::Handled;
    }

    if (timerId != ShellSettings::kPrefsSaveTimerId)
    {
        return DxuiMessageResult::NotHandled;
    }

    if (m_hwnd != nullptr && m_host != nullptr)
    {
        hr = m_host->KillTimer (ShellSettings::kPrefsSaveTimerId);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_settings->FlushDeferredGlobalPrefs();

    return DxuiMessageResult::Handled;
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
    int64_t          nowMs  = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                  std::chrono::steady_clock::now().time_since_epoch()).count();
    const wchar_t  * tip    = nullptr;
    RECT             anchor = {};



    // The caption buttons' tooltip. The host suppresses the stock system
    // tooltip for these, so a hover over one raises ours; anything else in
    // the non-client area takes it down.
    switch (hitTest)
    {
        case HTMINBUTTON: tip = L"Minimize";                                     break;
        case HTMAXBUTTON: tip = (IsZoomed (m_hwnd) != FALSE) ? L"Restore" : L"Maximize"; break;
        case HTCLOSE:     tip = L"Close";                                        break;
        default:          break;
    }

    if (tip != nullptr && m_host != nullptr &&
        m_host->GetNcSystemButtonRectPx (POINT { xScreen, yScreen }, anchor))
    {
        m_chrome->m_captionTooltip.RequestShow (anchor, tip, nowMs);
    }
    else
    {
        m_chrome->m_captionTooltip.RequestHide (nowMs);
    }

    // The host owns caption / system-button hover now. Our only stake in
    // a non-client move is dropping a latched menu hover: when the
    // pointer leaves the menu upward into the caption the client
    // mouse-move stream stops, so this is the one signal that clears it.
    m_chrome->m_mainMenu.ClearHover();
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
    int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                         std::chrono::steady_clock::now().time_since_epoch()).count();



    // Caption-button hover teardown is the host's job. The tooltip is ours.
    m_chrome->m_captionTooltip.RequestHide (nowMs);

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
    if (m_chrome->m_mainMenu.IsOpen())
    {
        m_chrome->m_mainMenu.Hide();
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





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnAppMessage
//
//  The messages this shell posts to its own window.
//
//  HERE RATHER THAN IN RunMessageLoop, WHICH IS THE WHOLE POINT. That loop
//  picked every one of these off before DispatchMessage, so they existed only
//  while it was the pump that was running -- and any modal dialog runs a pump
//  of its own. A question about a changed disk arriving while a picker or the
//  About box was open reached DefWindowProc, its heap payload leaked, and the
//  store had already recorded that a question was outstanding, so it never
//  asked again: that bay stayed stuck with a pending change until it was
//  ejected. A mount completing under the same dialog went the same way, and
//  the user was never told it had failed. Reached from the window procedure,
//  every pump delivers them.
//
//  A QUESTION ARRIVING UNDER A MODAL THEREFORE OPENS AS A NESTED MODAL, which
//  is ordinary Win32 -- a disabled owner still receives posted messages -- and
//  better than the alternative of not being told at all.
//
//  EACH lParam OWNS A HEAP PAYLOAD handed over by whoever posted it, and this
//  is where it is freed.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    // A disk changed outside Casso. The store decided on the thread that owns
    // disk writes; both of these build UI, so they land here.
    if (msg == WM_APP_CHANGE_REPORT)
    {
        ShellDisks::ChangeNotice *  carried = reinterpret_cast<ShellDisks::ChangeNotice *> (lParam);

        if (carried != nullptr)
        {
            m_disks->ShowChangeBanner (*carried);
            delete carried;
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == WM_APP_CHANGE_ASK)
    {
        ShellDisks::ChangeNotice *  carried = reinterpret_cast<ShellDisks::ChangeNotice *> (lParam);

        if (carried != nullptr)
        {
            m_disks->AskAboutChange (*carried);
            delete carried;
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == WM_APP_NOTIFY_USER)
    {
        wstring *  carried = reinterpret_cast<wstring *> (lParam);

        if (carried != nullptr)
        {
            ShowNotification (*carried);
            delete carried;
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == WM_APP_SHOW_NOTICE)
    {
        wstring *  carried = reinterpret_cast<wstring *> (lParam);

        if (carried != nullptr)
        {
            m_chrome->ShowNotice (*carried);
            delete carried;
        }

        return DxuiMessageResult::Handled;
    }

    // Game-port input submitted off the UI thread (the controller thread, or a
    // machine rebuild) waits here to be written: the device setters report
    // host input to their input event sink, which is UI-thread only.
    if (msg == WM_APP_GAMEPORT_FLUSH)
    {
        m_gamePortMixer.FlushPending();

        return DxuiMessageResult::Handled;
    }

    // The players' slots changed, or a controller came or went. Saying so and
    // writing it to the prefs both belong here, not on the controller thread.
    if (msg == WM_APP_CONTROLLER_PICK)
    {
        std::vector<std::wstring>  notices;
        bool                       haveEntriesChanged     = false;
        bool                       haveLastHoldersChanged = false;

        {
            std::lock_guard<std::mutex>  lock (m_controllerPickMutex);

            notices.swap (m_controllerPickNotices);
            haveEntriesChanged         = m_controllerPickHasEntries;
            haveLastHoldersChanged     = m_controllerPickHasHolders;
            m_controllerPickHasEntries = false;
            m_controllerPickHasHolders = false;
        }

        // A device arriving or leaving changes the rows even when it changes
        // nothing else, so the list is rebuilt on every one of these.
        ApplyControllerSlotsChange (notices, haveEntriesChanged, haveLastHoldersChanged);
        SyncPaddleSourceList();

        return DxuiMessageResult::Handled;
    }

    // A mount that ran on the CPU thread wants its damage report raised here,
    // where a modal can be built.
    if (msg == WM_APP_REPORT_DAMAGE)
    {
        m_disks->ReportDamagedMount ((int) wParam);

        return DxuiMessageResult::Handled;
    }

    // One mount's outcome, from whichever thread ran it. Startup mounts land
    // here too, which is what keeps a bad --disk1 from raising a dialog before
    // there was a pump to run it.
    if (msg == WM_APP_MOUNT_COMPLETED)
    {
        ShellDisks::MountCompletion *  carried = reinterpret_cast<ShellDisks::MountCompletion *> (lParam);

        if (carried != nullptr)
        {
            m_disks->HandleMountCompletion (*carried);
            delete carried;
        }

        return DxuiMessageResult::Handled;
    }

    // A piece of update work finished on the update service's thread.
    if (msg == WM_APP_UPDATE_RESULT)
    {
        UpdateResult *  carried = reinterpret_cast<UpdateResult *> (lParam);

        if (carried != nullptr)
        {
            m_updater->HandleUpdateResult (*carried);
            delete carried;
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == WM_APP_DXUI_UPDATE_TITLE)
    {
        m_window->UpdateWindowTitle();
        m_chrome->ReflowChromeForMachineChange();

        // The machine may now sit in front of a different monitor, which
        // changes every override key. This is the UI-thread side of the
        // switch; SwitchMachine runs on the CPU thread and must not do file
        // work or race the render path.
        m_renderer->RefreshCrtOverrideKeys();

        // A switch adopts the machine's own input mapping and may change the
        // default pointer mode, both on the CPU thread, which defers their UI
        // reflection here. Sync the selector state on the UI thread; it is
        // idempotent when nothing changed.
        SyncSelectorState();

        return DxuiMessageResult::Handled;
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnCopyData
//
//  A writing tool saying what its change to a mounted image meant.
//
//  THE SHELL DOES NO MATCHING. Which bay the path belongs to, whether the file
//  actually changed, and what to do about it are all decided in core; this
//  reads the bytes, hands them over, and returns.
//
//  IT RETURNS IMMEDIATELY, and it must: this runs inside the SENDER's blocking
//  SendMessage, so anything done here is time a build spends waiting. Recording
//  a pending change is all that happens; acting on it belongs to the thread that
//  owns disk writes, at a moment with nothing in flight.
//
//  A MESSAGE FROM ANYTHING ELSE IS NOT OURS. Any process on the desktop can
//  address a WM_COPYDATA at this window, so the registered id is checked before
//  a single byte is read.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnCopyData (WPARAM sender, LPARAM data)
{
    const COPYDATASTRUCT *       carried    = reinterpret_cast<const COPYDATASTRUCT *> (data);
    bool                         wellFormed = false;
    Win32IntentChannel::Payload  payload;



    UNREFERENCED_PARAMETER (sender);

    if (carried == nullptr || carried->dwData != Win32IntentChannel::GetMessageId())
    {
        return DxuiMessageResult::NotHandled;
    }

    wellFormed = Win32IntentChannel::Decode (reinterpret_cast<const Byte *> (carried->lpData),
                                             (size_t) carried->cbData, payload);

    //  A malformed payload is claimed rather than passed on: it carried our own
    //  id, so it was meant for us and simply was not readable.
    if (wellFormed)
    {
        m_machine.GetDiskStore().NoteExternalChange (payload.imagePath, payload.intent);
    }

    return DxuiMessageResult::Handled;
}
