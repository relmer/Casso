#include "Pch.h"
#include "Shell/ShellDebugger.h"
#include "Shell/Components/ShellHistory.h"

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
//  Loads an HICON resource into a CPU-side premultiplied BGRA8 pixel
//  buffer for DxuiTextRenderer::DrawIconBitmap, through
//  DxuiIconImage::FromHicon. It used to multiply by alpha a second time
//  after DrawIconEx, which already writes premultiplied pixels, and so
//  darkened every anti-aliased edge of the caption icon.
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
    HRESULT        hr    = S_OK;
    HICON          hIcon = nullptr;
    HRESULT        hrGle = E_FAIL;
    DxuiIconImage  image;



    // A missing resource id reads differently from an exhausted GDI heap, so
    // the OS code is carried out rather than flattened to "no icon".
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

    hr = DxuiIconImage::FromHicon (hIcon, sizePx, image);
    CHR (hr);

    outPixels = std::move (image.bgraPremul);
    outW      = image.width;
    outH      = image.height;

Error:
    if (hIcon != nullptr)
    {
        DestroyIcon (hIcon);
    }

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

    // Behind live, where the replay stands.
    title += m_shell.m_history->GetReplayCaption();

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





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::InitializeOle
//
//  OleInitialize implies CoInitializeEx (STA). S_FALSE (already initialized)
//  also adds a reference, which OleUninitialize releases at shutdown. A hard
//  failure means the UI thread's apartment was initialized with a
//  conflicting model, a startup-ordering bug, so the call asserts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellWindow::InitializeOle()
{
    HRESULT  hr = S_OK;



    hr = OleInitialize (nullptr);
    CHRA (hr);

    m_fOleInitialized = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::UninitializeOle
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::UninitializeOle()
{
    if (m_fOleInitialized)
    {
        OleUninitialize();
        m_fOleInitialized = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::ShutdownDragDrop
//
//  Revoked while the window handle is valid, as RevokeDragDrop requires.
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::ShutdownDragDrop()
{
    m_dragDropTarget.Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::ConsumeSuppressedDropClick
//
////////////////////////////////////////////////////////////////////////////////

bool ShellWindow::ConsumeSuppressedDropClick()
{
    return m_dragDropTarget.ConsumeSuppressedClick();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::GetStartupShowState
//
//  Windows passes a requested window state through CreateProcess to
//  wCassoMain. Start-Process -WindowStyle Minimized and scripted launches use
//  it to start Casso without taking focus from the user's work.
//
//  Only a PARTICULAR request from the launcher is used. SW_SHOWDEFAULT,
//  SW_SHOW and normal come from an ordinary double-click and mean nothing in
//  particular, so the saved placement's state is used for them.
//
////////////////////////////////////////////////////////////////////////////////

int ShellWindow::GetStartupShowState() const
{
    int  show = m_startMaximized ? SW_SHOWMAXIMIZED : SW_SHOW;



    switch (m_startShowCmd)
    {
        case SW_HIDE:
        case SW_MINIMIZE:
        case SW_SHOWMINIMIZED:
        case SW_SHOWMINNOACTIVE:
        case SW_SHOWNOACTIVATE:
        case SW_SHOWNA:
            show = m_startShowCmd;
            break;

        default:
            break;
    }

    return show;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::TryLoadSavedPlacement
//
////////////////////////////////////////////////////////////////////////////////

bool ShellWindow::TryLoadSavedPlacement (LONG & outX, LONG & outY, int & outW, int & outH)
{
    return m_windowManager->TryLoadSavedWindowPlacement (outX, outY, outW, outH, m_startMaximized);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::SavePlacement
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::SavePlacement (bool isFullscreen)
{
    m_windowManager->SaveWindowPlacement (m_shell.m_hwnd, isFullscreen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::TakeUserStateChange
//
////////////////////////////////////////////////////////////////////////////////

bool ShellWindow::TakeUserStateChange()
{
    bool  isChanged = m_userStateChange;



    m_userStateChange = false;

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::LoadAcceleratorTable
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellWindow::LoadAcceleratorTable (HINSTANCE hInstance)
{
    HRESULT  hr = S_OK;



    m_accelTable = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR));
    CWRA (m_accelTable);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::TranslateAcceleratorMessage
//
////////////////////////////////////////////////////////////////////////////////

bool ShellWindow::TranslateAcceleratorMessage (MSG & msg)
{
    return m_accelTable != nullptr && TranslateAccelerator (m_shell.m_hwnd, m_accelTable, &msg) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::OnMoved
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::OnMoved()
{
    //  A snap layout, Win+arrow or a monitor going away places the window
    //  without the OS drag loop, so this is the only report of it. Inside the
    //  loop the rect changes once per pixel and the end of it is the
    //  placement. SaveWindowPlacement refuses anything that is not a settled
    //  windowed rect, which is what kept the transitions out before.
    if (!m_inSizeMove)
    {
        WindowTrace::LogWindow ("place.change", "main", m_shell.m_hwnd, "outside the OS drag loop");
        SavePlacement (m_shell.m_renderer->GetD3D().IsFullscreen());
    }

    if (m_shell.m_chrome->GetMainMenu().IsOpen())
    {
        m_shell.m_chrome->GetMainMenu().Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::OnEnterSizeMove
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::OnEnterSizeMove()
{
    m_inSizeMove = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::OnExitSizeMove
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
//
////////////////////////////////////////////////////////////////////////////////

void ShellWindow::OnExitSizeMove()
{
    m_inSizeMove = false;
    WindowTrace::LogWindow ("move.end", "main", m_shell.m_hwnd, "the OS drag loop ended");

    SavePlacement (m_shell.m_renderer->GetD3D().IsFullscreen());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::ClampMinMax
//
//  Clamps the window's minimum track size so the bottom drive bar can
//  never be dragged up into the menu strip / NC area. The floor is the
//  client size for a minimum emulator viewport (the chrome-band dock
//  adds the live title / nav / drive-bar insets), widened so no menu
//  title clips, then translated to a window size by the live NC overhead.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult ShellWindow::ClampMinMax (MINMAXINFO * info)
{
    HRESULT            hr          = S_OK;
    DxuiMessageResult  result      = DxuiMessageResult::NotHandled;
    RECT               rcClient    = {};
    RECT               rcWindow    = {};
    SIZE               minClient   = {};
    int                menuWidthPx = 0;
    int                ncOverheadW = 0;
    int                ncOverheadH = 0;



    BAIL_OUT_IF (info == nullptr || m_shell.m_hwnd == nullptr, S_OK);

    // Client size for the minimum center: the chrome-band dock adds the
    // live title / nav / drive-bar insets around the requested viewport.
    minClient = m_shell.m_chrome->GetClientSizeForCenterPx (m_shell.m_scaler.ToPx (s_kMinCenterWidthDp),
                                                    m_shell.m_scaler.ToPx (s_kMinCenterHeightDp));

    // Never narrower than the menu strip's content so every title stays
    // on-strip. The width is physical client px, the same space as minClient.
    menuWidthPx = m_shell.m_chrome->GetMainMenu().GetMenuStripContentWidthPx() + m_shell.m_scaler.ToPx (s_kMenuRightPadDp);

    if (minClient.cx < menuWidthPx)
    {
        minClient.cx = menuWidthPx;
    }

    // Translate the client floor to a window floor via the live NC overhead
    // (the custom chrome keeps this small -- just the resize borders -- but
    // it is non-zero).
    if (GetClientRect (m_shell.m_hwnd, &rcClient) && GetWindowRect (m_shell.m_hwnd, &rcWindow))
    {
        ncOverheadW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
        ncOverheadH = (rcWindow.bottom - rcWindow.top)  - (rcClient.bottom - rcClient.top);
    }

    info->ptMinTrackSize.x = minClient.cx + ncOverheadW;
    info->ptMinTrackSize.y = minClient.cy + ncOverheadH;

    //  Once per change: the OS asks this on every pointer tick of a drag.
    if (info->ptMaxSize.x != m_lastMaxSize.x || info->ptMaxSize.y != m_lastMaxSize.y)
    {
        m_lastMaxSize = info->ptMaxSize;
        WindowTrace::LogRect ("minmax", "main", info->ptMaxPosition.x, info->ptMaxPosition.y,
                              info->ptMaxSize.x, info->ptMaxSize.y,
                              "the maximized rect the OS offers");
    }

    result = DxuiMessageResult::Handled;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow::LayOutForSize
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

void ShellWindow::LayOutForSize (UINT widthPx, UINT heightPx)
{
    int       width     = static_cast<int> (widthPx);
    int       renderH   = static_cast<int> (heightPx);



    UNREFERENCED_PARAMETER (widthPx);

    m_shell.m_chrome->SetInLayout (true);

    // A resize restretches the window; drop any open menu so its
    // window-anchored popup is not left stranded.
    if (m_shell.m_chrome->GetMainMenu().IsOpen())
    {
        m_shell.m_chrome->GetMainMenu().Hide();
    }

    // The host (DxuiHwndSource::HandleSize) already resized its swap
    // chain and recreated the back-buffer RTV + D2D target before this
    // OnSize fired. The renderer no longer owns the swap chain; it just
    // needs the new back-buffer dimensions for the CRT post-process.
    m_shell.m_renderer->GetD3D().SetBackBufferSize (static_cast<int> (width), renderH);

    {
        UINT  dpi           = GetDpiForWindow (m_shell.m_hwnd);
        RECT  menuBarBounds = {};
        HRESULT  hrUiR           = m_shell.m_uiShell.OnResize (m_shell.m_renderer->GetD3D().GetBackBufferWidth(),
                                                       m_shell.m_renderer->GetD3D().GetBackBufferHeight(),
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
            m_shell.m_scaler.SetDpi (dpi);
        }

        // Fullscreen presentation (FR-014): the picture owns the whole client
        // and every chrome element collapses, whichever theme is on. The
        // windowed path below is the one that restores everything --
        // including the host caption -- when fullscreen exits, because this
        // OnSize runs on both transitions.
        if (m_shell.m_renderer->GetD3D().IsFullscreen())
        {
            m_shell.m_chrome->SetChromeHiddenForFullscreenScene (true);
            m_shell.m_chrome->UpdateViewportLayout (static_cast<int> (width), renderH);
            m_shell.m_chrome->RecordSizedFor ((m_shell.m_disks->GetManager() != nullptr) && m_shell.m_disks->GetManager()->HasSlot6Controller(),
                                      m_shell.MachineHasCaseSwitches());
        }
        else
        {

        // Chrome visibility FIRST: the caption height feeds the menu bar's
        // anchor, and a hidden caption reports zero.
        m_shell.m_chrome->SetChromeHiddenForFullscreenScene (false);

        menuBarBounds = { 0, m_shell.m_host->GetCaptionHeightPx(), static_cast<int> (width), m_shell.m_host->GetCaptionHeightPx() };
        m_shell.m_chrome->GetMainMenu().Layout (menuBarBounds, m_shell.m_scaler);

        // Settle the desk-scene scale (monitor fit + scaled band heights) for
        // THIS size before laying the drive widgets, so widgets and band agree
        // instead of the widgets lagging one resize behind.
        m_shell.m_chrome->UpdateViewportLayout (static_cast<int> (width), renderH);

        {
            RECT  vr            = m_shell.m_chrome->ComputeViewportRect (static_cast<int> (width), renderH);
            RECT  driveRect     = m_shell.m_chrome->GetDriveBandBounds();
            int   bottomInsetPx = renderH - driveRect.top;   // drive band height only
            bool  fHasDisk      = (m_shell.m_disks->GetManager() != nullptr) && m_shell.m_disks->GetManager()->HasSlot6Controller();

            (void) vr;                                        // dock side-effect: bands arranged

            if (m_shell.DeskSceneActive())
            {
                // The 3D scene owns the drives; nothing to lay out. The hit
                // registry is refreshed below with the rest of the chrome.
            }
            else if (fHasDisk)
            {
                m_shell.m_chrome->LayoutDriveWidgetsInCommandBar (m_shell.m_disks->GetDriveChrome(), bottomInsetPx, static_cast<int> (width), renderH, dpi,
                                                          m_shell.m_chrome->GetSceneScale(), m_shell.m_disks->ShouldShowExternalDrive() ? 2 : 1);

                // LayoutDriveWidgetsInCommandBar lays out (and un-hides) BOTH
                // widgets. Re-collapse the external one when it is an optional
                // //c drive the user has not connected, so only the internal
                // drive shows.
                if (!m_shell.m_disks->ShouldShowExternalDrive())
                {
                    m_shell.m_disks->GetDriveChrome()[1].Hide();
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
                m_shell.m_disks->GetDriveChrome()[0].Hide();
                m_shell.m_disks->GetDriveChrome()[1].Hide();
            }

            // OnSize is the authoritative layout (only fires on a real WM_SIZE,
            // never per-frame), so record the disk-presence + //c-ness this
            // window size now accounts for. ReflowChromeForMachineChange reads
            // these pre-switch values to grow/shrink the window by the band delta.
            m_shell.m_chrome->RecordSizedFor (fHasDisk, m_shell.MachineHasCaseSwitches());
            m_shell.m_chrome->LayoutDriveBandSurface (!m_shell.DeskSceneActive(), RECT{ 0, driveRect.top, static_cast<int> (width), renderH });

            m_shell.m_chrome->LayoutSwitchBar (dpi);

            if (m_shell.DeskSceneActive())
            {
                m_shell.m_scene->SyncSceneDriveChrome();
            }
            else
            {
                m_shell.m_uiShell.GetHitTester().Clear();
                if (fHasDisk)
                {
                    m_shell.m_uiShell.GetHitTester().Register (DxuiHitRect { m_shell.m_disks->GetDriveChrome()[0].GetBodyRect(), DxuiHitSlot::Custom, 0 });
                    if (m_shell.m_disks->ShouldShowExternalDrive())
                    {
                        m_shell.m_uiShell.GetHitTester().Register (DxuiHitRect { m_shell.m_disks->GetDriveChrome()[1].GetBodyRect(), DxuiHitSlot::Custom, 1 });
                    }
                }

                m_shell.m_tapeDeck->RegisterTapeDropTarget();
            }
        }

        }   // windowed chrome path
    }

    // (Viewport layout already settled above, before the drive widgets.)

    m_shell.m_renderer->StageFramebufferForResize();

    // Repaint immediately at the new size through the host pump. The
    // host already resized its swap chain in HandleSize; driving a
    // synchronous WM_PAINT here avoids a stale / black frame during an
    // interactive drag-resize (when RunMessageLoop is blocked in the OS
    // modal resize loop).
    InvalidateRect (m_shell.m_hwnd, nullptr, FALSE);
    UpdateWindow   (m_shell.m_hwnd);

    // A MAXIMIZE OR RESTORE THE USER ASKED FOR, carried out. Those never
    // enter the OS drag loop, so OnExitSizeMove cannot see them; the flag
    // is what says this one was theirs rather than a programmatic
    // ShowWindow, which produces an identical WM_SIZE.
    if (TakeUserStateChange())
    {
        WindowTrace::LogWindow ("state.change", "main", m_shell.m_hwnd, "a maximize or restore the user asked for");
        SavePlacement (m_shell.m_renderer->GetD3D().IsFullscreen());
    }

    m_shell.m_chrome->SetInLayout (false);
}
