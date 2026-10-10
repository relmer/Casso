#include "Pch.h"

#include "Shell/Components/ShellWindow.h"
#include "Shell/EmulatorShell.h"
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
#include "Devices/Tape/TapeImageLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow
//
////////////////////////////////////////////////////////////////////////////////

ShellWindow::ShellWindow (EmulatorShell & shell)
    : m_shell (shell)
{
    m_windowManager = std::make_unique<WindowManager> (m_shell.m_settings->GetPrefs(), [this] { m_shell.m_settings->SaveGlobalPrefs(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellWindow
//
////////////////////////////////////////////////////////////////////////////////

ShellWindow::~ShellWindow() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetCursorMonitorWorkArea
//
////////////////////////////////////////////////////////////////////////////////

bool ShellWindow::TryGetCursorMonitorWorkArea (RECT & outWork, HMONITOR & outMonitor)
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
//  ShellWindow::CenterInWorkArea
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::CenterInWorkArea (
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
//  ShellWindow::LoadIconAsPremulBgra
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

HRESULT ShellWindow::LoadIconAsPremulBgra (
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
//  ShellWindow::GetEmulatorContentScreenRect
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

RECT ShellWindow::GetEmulatorContentScreenRect()
{
    return m_shell.m_renderer->GetD3D().GetEmulatorContentScreenRect();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::GetClientSizeForFramebufferPx
//
//  Framebuffer scale policy: linear DPI scaling. The Apple ][ pixel grid
//  (given in DIPs) scales at the same rate as the chrome dp, so the
//  framebuffer and chrome insets stay in proportion at every DPI. Both
//  the initial window size and Ctrl+0 reset go through here.
//
////////////////////////////////////////////////////////////////////////////////

SIZE ShellWindow::GetClientSizeForFramebufferPx (int framebufferWidthDp, int framebufferHeightDp)
{
    SIZE  client         = {};
    int   framebufferWpx = m_shell.m_scaler.ToPx (framebufferWidthDp);
    int   framebufferHpx = m_shell.m_scaler.ToPx (framebufferHeightDp);



    // With the desk scene on, size the window so the monitor's screen RECESS
    // -- not the bare center -- equals the framebuffer, i.e. the emulator
    // image sits at 100% zoom inside the housing, with the bezel, desk margin
    // and chrome bands sized around it. This inverse defines the 100% scene,
    // where the drives sit at s_kDeskDriveScale, so the band math must run at
    // that scale regardless of the current window's. Scene off: the center is
    // the framebuffer directly at classic sizes.
    if (m_shell.CrtMonitorActive())
    {
        SIZE   center     = DeskSceneLayout::CenterSizeForDisplayPx (framebufferWpx, framebufferHpx,
                                                                     m_shell.m_scaler.GetDpi(), m_shell.m_scene->DeskSceneDriveCount(),
                                                                     m_shell.m_scene->GetScene().Metrics(),
                                                                     m_shell.m_scaler.ToPx (s_kSceneDriveGapDp + s_kStripEdgeZoneDp));
        float  savedScale = m_shell.m_chrome->GetSceneScale();

        m_shell.m_chrome->SetSceneScale (s_kDeskDriveScale);
        client = m_shell.m_chrome->GetClientSizeForCenterPx (center.cx, center.cy);
        m_shell.m_chrome->SetSceneScale (savedScale);
    }
    else
    {
        client = m_shell.m_chrome->GetClientSizeForCenterPx (framebufferWpx, framebufferHpx);

        // Never narrower than the drive row. With the recorder beside the
        // drives the row outgrows a 100% screen, and a window wrapped tightly
        // around the screen cut the recorder off at its right edge.
        client.cx = max (client.cx, (LONG) GetDriveRowWidthPx());
    }

    return client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::GetDriveRowWidthPx
//
//  How wide the flat drive row is -- the drives that show, the recorder when
//  the machine has one, the gaps between them and a gap at either end --
//  measured on throwaway widgets so the live ones keep their layout.
//
////////////////////////////////////////////////////////////////////////////////

int ShellWindow::GetDriveRowWidthPx()
{
    UINT            dpi    = m_shell.m_scaler.GetDpi();
    int             gap    = MulDiv (s_kCompactDriveWidgetGapDp, (int) dpi, s_kBaseDpi);
    int             count  = m_shell.m_disks->ShouldShowExternalDrive() ? 2 : 1;
    DxuiDpiScaler   scaler;
    DriveWidget     drive;
    TapeDeckWidget  tape;
    RECT            outer  = {};
    int             width  = 0;



    scaler.SetDpi (dpi);

    drive.Layout (RECT {}, scaler);
    outer = drive.GetOuterRect();
    width = count * (outer.right - outer.left) + (count + 1) * gap;

    if (m_shell.m_tapeDeck->IsTapeRecorderShown())
    {
        tape.Layout (RECT {}, scaler);
        outer  = tape.GetOuterRect();
        width += (outer.right - outer.left) + gap;
    }

    return width;
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

void ShellWindow::ReconcileInitialClientSize()
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
    bool         needsReconcile = !m_initialSizeReconciled && m_shell.m_hwnd != nullptr;
    bool         haveRects      = false;
    bool         haveWork       = false;



    BAIL_OUT_IF (!needsReconcile, S_OK);

    m_initialSizeReconciled = true;

    desired         = GetClientSizeForFramebufferPx (kFramebufferWidth, kFramebufferHeight);
    desiredClientW  = (int) desired.cx;
    desiredClientH  = (int) desired.cy;

    // Force a fresh WM_NCCALCSIZE so DefWindowProc carves the actual
    // thick-frame borders into the client rect. Without this, the
    // post-ShowWindow GetClientRect returns the full window rect
    // (NC overhead = 0) and the reconcile math thinks no resize is
    // needed -- leaving the emulator pixel grid undersized by the
    // border width on the eventual first NCCALCSIZE.
    SetWindowPos (m_shell.m_hwnd, nullptr, 0, 0, 0, 0,
                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    haveRects = GetClientRect (m_shell.m_hwnd, &rcActualClient) && GetWindowRect (m_shell.m_hwnd, &rcActualWindow);

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
    hMon     = MonitorFromWindow (m_shell.m_hwnd, MONITOR_DEFAULTTONEAREST);
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

        SetWindowPos (m_shell.m_hwnd, nullptr, x, y, fixedW, fixedH, flags);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::ApplyAppIconToWindow
//
//  Give a child DxuiWindow the Casso icon so Alt-Tab / the taskbar show the
//  Casso motif rather than a generic window icon. The borderless Dxui panels do
//  not inherit the WNDCLASS icon and Alt-Tab reads the window's WM_GETICON, so
//  the big + small icons are attached explicitly. LR_SHARED handles are managed
//  by the system, so there is nothing to free.
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::ApplyAppIconToWindow (HWND target)
{
    HINSTANCE   hInstance = nullptr;
    HICON       iconBig   = nullptr;
    HICON       iconSmall = nullptr;



    if (target == nullptr)
    {
        return;
    }

    hInstance = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_shell.m_hwnd, GWLP_HINSTANCE));

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
//  An undocumented --title puts a launcher's own label in front of all of it,
//  which is what lets several windows running the same machine be told apart.
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::UpdateWindowTitle()
{
    HRESULT  hr          = S_OK;
    wstring  title;
    wstring  wideName;
    bool     isOffThread = false;



    BAIL_OUT_IF (m_shell.m_hwnd == nullptr, S_OK);

    // SwitchMachine calls this on the CPU thread; DxuiHwndSource::SetTitle
    // mutates the caption bar and asserts the UI thread. Bounce off-thread
    // callers through the message loop (WM_APP_DXUI_UPDATE_TITLE handler above).
    isOffThread = GetWindowThreadProcessId (m_shell.m_hwnd, nullptr) != GetCurrentThreadId();

    if (isOffThread)
    {
        PostMessageW (m_shell.m_hwnd, WM_APP_DXUI_UPDATE_TITLE, 0, 0);
    }

    BAIL_OUT_IF (isOffThread, S_OK);

    //  The launcher's label, ahead of everything the emulator has to say about
    //  itself. FIRST because that is the half of a caption a taskbar button or
    //  an Alt+Tab thumbnail still has room for once it truncates, and the whole
    //  reason the label was passed in is to tell one window from several
    //  identical ones.
    if (!m_titlePrefix.empty())
    {
        title += m_titlePrefix;
        title += L" - ";
    }

    title += L"Casso";

    if (!m_shell.m_machine.GetConfig().name.empty())
    {
        wideName = fs::path (m_shell.m_machine.GetConfig().name).wstring();
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
    if (m_shell.m_cpuManager.IsPaused())
    {
        title += L" [Paused]";
    }
    else if (!m_shell.m_cpuManager.IsRunning())
    {
        title += L" [Stopped]";
    }

#if defined (_DEBUG)
    // Dev builds stamp the exact binary identity (version, arch, compile
    // timestamp) so a window is never mistaken for a stale rebuild. Same " - "
    // separator the machine name uses, so the caption reads as one list.
    title += L" - ";
    title += GetCassoBuildInfo();
#endif

    m_shell.m_host->SetTitle (title);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::InstallIntentMessageFilter
//
//  Lets a stated intent cross an integrity boundary.
//
//  THE RECEIVER'S JOB, NOT THE SENDER'S. The filter takes the receiving window,
//  and the sender runs inside CassoCli.exe with no window at all -- so there is
//  nowhere else this could live.
//
//  THE FILTER TAKES A WINDOW MESSAGE, so it is installed for WM_COPYDATA as a
//  whole. The registered id that distinguishes this project's messages lives in
//  `dwData`, which the filter cannot see; it is checked in the handler instead.
//
//  BEST EFFORT. Where the call fails there is nothing useful to do: an intent
//  that does not arrive falls back to asking, which is correct behavior.
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::InstallIntentMessageFilter()
{
    BOOL  allowed = FALSE;



    if (m_shell.m_hwnd == nullptr)
    {
        return;
    }

    allowed = ChangeWindowMessageFilterEx (m_shell.m_hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);

    IGNORE_RETURN_VALUE (allowed, TRUE);
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

void ShellWindow::InstallDragDropTarget()
{
    HRESULT     hrDrop            = S_OK;
    const UINT  kWmCopyGlobalData = 0x0049;   // undocumented but real



    // Drag-drop is an optional convenience -- File > Open and the drive
    // widgets' click-to-browse cover the same mounts -- so a failed
    // registration disables drop but must not prevent launch.
    // Disks and tapes both; OnFileDropped sends each only to what can take it.
    hrDrop = m_dragDropTarget.Initialize (m_shell.m_hwnd,
                                          &m_shell.m_uiShell.GetHitTester(),
                                          [this] (int tag, const std::wstring & path) { m_shell.OnFileDropped (tag, path); },
                                          [] (const std::wstring & path)
                                          {
                                              return IsSupportedDiskImageExtension (path) ||
                                                     TapeImageLoader::IsTapeFileExtension (path);
                                          });
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
    (void) ChangeWindowMessageFilterEx (m_shell.m_hwnd, WM_DROPFILES,      MSGFLT_ALLOW, nullptr);
    (void) ChangeWindowMessageFilterEx (m_shell.m_hwnd, WM_COPYDATA,       MSGFLT_ALLOW, nullptr);
    (void) ChangeWindowMessageFilterEx (m_shell.m_hwnd, kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
}
