#include "Pch.h"

#include "DxuiHwndSource.h"
#include "DxuiCaptionBar.h"
#include "DxuiPopupHost.h"
#include "DxuiSystemButton.h"
#include "IDxuiHostClient.h"
#include "Theme/DxuiDwm.h"
#include "Theme/IDxuiTheme.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#if defined(_DEBUG)
// DXGI_DEBUG_ALL is an extern GUID exported by dxguid.lib; only the
// _DEBUG InfoQueue break-on-severity wiring in CreateDeviceAndSwapChain
// references it.
#pragma comment(lib, "dxguid.lib")
#endif


// Host-owned WM_TIMER id armed for the duration of an OS modal move / size loop
// (WM_ENTERSIZEMOVE..WM_EXITSIZEMOVE) to drive IDxuiHostClient::OnModalLoopTick.
// Distinct from any client SetTimer id, which route to OnTimer instead.
static constexpr UINT_PTR  s_kModalLoopTimerId = 0xDCE1;





namespace
{
    //
    //  Nudge a freshly-created, still-hidden CW_USEDEFAULT window the
    //  minimum needed so its whole frame sits within its monitor's work
    //  area — fixing a cascade that would open the bottom edge (and its
    //  command-button row) beneath the taskbar — without otherwise moving
    //  it (position only, no re-centering over the owner). rcWork already
    //  excludes the taskbar.
    //
    void  NudgeWindowOnScreen (HWND hwnd)
    {
        RECT         rect    = {};
        HMONITOR     monitor = nullptr;
        MONITORINFO  info    = { sizeof (info) };
        RECT         work    = { 0, 0, 1920, 1080 };
        POINT        placed  = {};



        if (hwnd != nullptr && GetWindowRect (hwnd, &rect) != FALSE)
        {
            monitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
            if (monitor != nullptr && GetMonitorInfoW (monitor, &info) != FALSE)
            {
                work = info.rcWork;
            }
            else if (SystemParametersInfoW (SPI_GETWORKAREA, 0, &work, 0) == FALSE)
            {
                work = { 0, 0, 1920, 1080 };
            }

            placed = DxuiHwndSource::ClampToWorkArea (rect, work);

            if (placed.x != rect.left || placed.y != rect.top)
            {
                SetWindowPos (hwnd, nullptr, placed.x, placed.y, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSource::NotifySystemButtonsMaximizedInTree
//
//  Recursively walks the control subtree and pushes the maximized
//  state onto every DxuiSystemButton of kind Max.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::NotifySystemButtonsMaximizedInTree (IDxuiControl * control, bool maximized)
{
    HRESULT             hr     = S_OK;
    size_t              n      = 0;
    size_t              i      = 0;
    IDxuiControl      * child  = nullptr;
    DxuiSystemButton  * button = nullptr;



    BAIL_OUT_IF (control == nullptr, S_OK);

    button = dynamic_cast<DxuiSystemButton *> (control);
    if (button != nullptr && button->Kind() == DxuiSystemButtonKind::Max)
    {
        button->SetMaximized (maximized);
    }

    n = control->ChildCount();
    for (i = 0; i < n; ++i)
    {
        child = control->Child (i);
        NotifySystemButtonsMaximizedInTree (child, maximized);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSource::FindNcSystemControlInTree
//
//  Depth-first, top-most-child-first search for the deepest NC system
//  button (min / max / close) whose bounds contain the point. Returns
//  nullptr when the point hits no system button.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiHwndSource::FindNcSystemControlInTree (IDxuiControl * control, POINT clientDip)
{
    HRESULT          hr    = S_OK;
    size_t           n     = 0;
    size_t           i     = 0;
    IDxuiControl   * child = nullptr;
    IDxuiControl   * found = nullptr;
    RECT             rc    = {};
    DxuiHitTestKind  kind  = DxuiHitTestKind::None;



    BAIL_OUT_IF (control == nullptr, S_OK);

    n = control->ChildCount();
    for (i = n; i > 0 && found == nullptr; --i)
    {
        child = control->Child (i - 1);
        if (child == nullptr || !child->Visible())
        {
            continue;
        }

        rc = child->Bounds();
        if (clientDip.x < rc.left || clientDip.x >= rc.right ||
            clientDip.y < rc.top  || clientDip.y >= rc.bottom)
        {
            continue;
        }

        found = FindNcSystemControlInTree (child, clientDip);
        if (found != nullptr)
        {
            break;
        }

        kind = child->ClassifyHit (clientDip);
        if (kind == DxuiHitTestKind::MinButton ||
            kind == DxuiHitTestKind::MaxButton ||
            kind == DxuiHitTestKind::CloseButton)
        {
            found = child;
            break;
        }
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSource
//
//  Default constructor — full-ownership mode. Caller must drive
//  Create() before the host is usable.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHwndSource::DxuiHwndSource()
{
    m_root = std::make_unique<DxuiPanel>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSource (synthetic-root ctor)
//
//  Test-only mode. No HWND, no device, no swap chain. The caller
//  supplies a pre-built root panel; tests then drive
//  ClassifyHitForTest() directly.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHwndSource::DxuiHwndSource (RECT                       clientBoundsDip,
                                float                      resizeBorderDip,
                                std::unique_ptr<DxuiPanel> root)
{
    m_root                       = std::move (root);
    m_params.resizeBorderDip     = resizeBorderDip;
    m_params.initialSizeDip.cx   = clientBoundsDip.right  - clientBoundsDip.left;
    m_params.initialSizeDip.cy   = clientBoundsDip.bottom - clientBoundsDip.top;
    m_synthetic                  = true;

    if (m_root != nullptr)
    {
        m_root->SetBounds (clientBoundsDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiHwndSource
//
//  Idempotent teardown. Safe to call when the host was never Created.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHwndSource::~DxuiHwndSource()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DefaultAppIcon
//
//  The host executable's first icon group -- the same icon Explorer shows
//  for the exe -- loaded once per size and cached for the process lifetime.
//  Windows secondary to the main frame default to it so pickers, panels,
//  and dialogs carry the app's identity instead of the generic Windows
//  icon. Returns nullptr when the exe has no icon resources; callers then
//  leave the system default in place.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    struct FirstIconGroup
    {
        bool      found     = false;
        wchar_t   nameBuf[256] = {};
        LPCWSTR   id        = nullptr;
    };
}

static BOOL CALLBACK FirstIconGroupProc (HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param)
{
    FirstIconGroup * out = (FirstIconGroup *) param;

    if (IS_INTRESOURCE (name))
    {
        out->id = name;
    }
    else
    {
        (void) wcsncpy_s (out->nameBuf, name, _TRUNCATE);
        out->id = out->nameBuf;
    }
    out->found = true;

    return FALSE;   // first group only
}

static HICON DefaultAppIcon (bool big)
{
    static HICON   s_big    = nullptr;
    static HICON   s_small  = nullptr;
    static bool    s_loaded = false;

    if (!s_loaded)
    {
        FirstIconGroup   group;

        s_loaded = true;
        (void) EnumResourceNamesW (nullptr, RT_GROUP_ICON, FirstIconGroupProc, (LONG_PTR) &group);

        if (group.found)
        {
            HMODULE   exe = GetModuleHandleW (nullptr);

            s_big   = (HICON) LoadImageW (exe, group.id, IMAGE_ICON,
                                          GetSystemMetrics (SM_CXICON),
                                          GetSystemMetrics (SM_CYICON), LR_DEFAULTCOLOR);
            s_small = (HICON) LoadImageW (exe, group.id, IMAGE_ICON,
                                          GetSystemMetrics (SM_CXSMICON),
                                          GetSystemMetrics (SM_CYSMICON), LR_DEFAULTCOLOR);
        }
    }

    return big ? s_big : s_small;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Registers a per-instance window class, calls CreateWindowEx with
//  WS_OVERLAPPEDWINDOW (so the OS still gives us proper resize-frame
//  math + Aero Snap + caption-double-click-to-maximize) even though
//  we collapse the NC area into the client rect in WM_NCCALCSIZE.
//  Then builds the D3D11 device + DXGI swap chain, initialises the
//  painter / text renderer, and applies the requested DwM bits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiHwndSource::Create (const CreateParams & params)
{
    HRESULT      hr             = S_OK;
    WNDCLASSEXW  wc             = { sizeof (wc) };
    DWORD        style          = 0;
    DWORD        exStyle        = 0;
    HINSTANCE    hInstance      = nullptr;
    wchar_t      classNameBuf[64] = {};
    uint32_t     serial         = 0;
    UINT         dpiAtCreate    = 0;
    int          windowX        = 0;
    int          windowY        = 0;
    int          widthPx        = 0;
    int          heightPx       = 0;



    DXUI_ASSERT_UI_THREAD();

    // Reject double-create.
    CBRA (m_hwnd == nullptr);
    CBRA (!m_synthetic);

    m_params    = params;
    hInstance   = (params.hInstance != nullptr) ? params.hInstance
                                                : GetModuleHandleW (nullptr);
    m_hInstance = hInstance;

    if (params.classNameOverride != nullptr)
    {
        // Caller-supplied stable class name. Skip the per-instance
        // serial so consumers like Casso can keep their well-known
        // window-class identifier (e.g. for tooling / Spy++).
        m_className = params.classNameOverride;
    }
    else
    {
        serial = s_classSerial.fetch_add (1);
        (void) swprintf_s (classNameBuf, L"DxuiHwndSource_%u_%p", serial, (void *) this);
        m_className = classNameBuf;
    }

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &DxuiHwndSource::s_WndProcThunk;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor (nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = m_className.c_str();

    CWRA (RegisterClassExW (&wc));
    m_classRegistered = true;

    style   = params.borderless ? (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN)
                                : WS_OVERLAPPEDWINDOW;
    if (!params.resizable)
    {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    exStyle = WS_EX_APPWINDOW;
    if (params.createNoActivate)
    {
        exStyle |= WS_EX_NOACTIVATE;   // stripped right after creation (see CreateParams)
    }

    // Composited-transparent mode: the window blends per-pixel over
    // whatever is behind it via the desktop compositor, so it must opt
    // out of the redirection bitmap (there is no opaque surface to
    // redirect) and drive its swap chain through DirectComposition.
    if (params.composited)
    {
        exStyle |= WS_EX_NOREDIRECTIONBITMAP;
    }

    // Initial DPI seed. WM_DPICHANGED will rescale later if the
    // window straddles or moves between monitors.
    dpiAtCreate = GetDpiForSystem();
    if (dpiAtCreate == 0)
    {
        dpiAtCreate = s_kDefaultDpi;
    }
    m_scaler.SetDpi (dpiAtCreate);

    if (params.useInitialWindowRectPx)
    {
        // Caller pre-computed a window-pixel placement (e.g. restored
        // from a saved RECT). Pass it through verbatim and skip the
        // DIP → pixel conversion path.
        windowX  = params.initialWindowRectPx.left;
        windowY  = params.initialWindowRectPx.top;
        widthPx  = params.initialWindowRectPx.right  - params.initialWindowRectPx.left;
        heightPx = params.initialWindowRectPx.bottom - params.initialWindowRectPx.top;
    }
    else
    {
        // Let the OS pick a cascade position (the window keeps its natural
        // placement, not re-centered over the owner); NudgeWindowOnScreen
        // below only corrects it if it lands partly off-screen.
        windowX  = CW_USEDEFAULT;
        windowY  = CW_USEDEFAULT;
        widthPx  = MulDiv (params.initialSizeDip.cx, (int) dpiAtCreate, (int) s_kDefaultDpi);
        heightPx = MulDiv (params.initialSizeDip.cy, (int) dpiAtCreate, (int) s_kDefaultDpi);
    }

    m_hwnd = CreateWindowExW (exStyle,
                              m_className.c_str(),
                              params.title.c_str(),
                              style,
                              windowX,
                              windowY,
                              widthPx,
                              heightPx,
                              params.ownerHwnd,
                              nullptr,
                              hInstance,
                              this);
    CWRA (m_hwnd);
    m_ownsHwnd = true;

    // The NOACTIVATE bit exists only to keep CreateWindowEx from activating
    // the newborn window (stealing focus mid-keystroke); strip it now so the
    // window activates normally from here on (clicks, an activating Show()).
    if (params.createNoActivate)
    {
        SetWindowLongPtrW (m_hwnd, GWL_EXSTYLE,
                           GetWindowLongPtrW (m_hwnd, GWL_EXSTYLE) & ~(LONG_PTR) WS_EX_NOACTIVATE);
    }

    // Re-seed scaler from the per-window DPI now that the HWND knows
    // which monitor it landed on.
    m_scaler.SetDpi (GetDpiForWindow (m_hwnd));

    // A CW_USEDEFAULT dialog can cascade with its lower edge — and button
    // row — beneath the taskbar. Nudge the still-hidden window the minimum
    // needed to sit fully within its monitor's work area (position only, no
    // re-centering). The saved-RECT path places itself and is left as-is.
    if (!params.useInitialWindowRectPx)
    {
        NudgeWindowOnScreen (m_hwnd);
    }

    // Apply app icons. Win32 MessageBox dialogs + the taskbar pick the icon
    // up via WM_GETICON, NOT WNDCLASS::hIcon, so the explicit WM_SETICON
    // pair is required even when the class was registered with icons. When
    // the caller supplies none, default to the host executable's first icon
    // group so secondary windows (pickers, panels, dialogs) carry the app's
    // identity instead of the generic Windows icon.
    {
        HICON   iconBig   = params.appIconBig;
        HICON   iconSmall = params.appIconSmall;

        if (iconBig == nullptr && iconSmall == nullptr)
        {
            iconBig   = DefaultAppIcon (true);
            iconSmall = DefaultAppIcon (false);
        }

        if (iconBig != nullptr)
        {
            SendMessageW (m_hwnd, WM_SETICON, ICON_BIG, (LPARAM) iconBig);
        }
        if (iconSmall != nullptr)
        {
            SendMessageW (m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM) iconSmall);
        }
    }

    if (params.createSwapChain)
    {
        hr = CreateDeviceAndSwapChain();
        CHRA (hr);

        hr = CreateRenderResources();
        CHRA (hr);

        m_ownsPaintPump = true;
    }

    ApplyDwmConfiguration();

    m_focusManager.Attach (RootPanel());
    NotifySystemButtonsMaximized (IsZoomed (m_hwnd) != FALSE);

    // Host-owned caption (SetWindowText model). Built last so the HWND,
    // DPI scaler, and (optional) app icon are all live.
    if (m_params.captionStyle != DxuiCaptionStyle::None)
    {
        BuildCaption();
    }

Error:

    if (FAILED (hr))
    {
        Destroy();
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSource::ClampToWorkArea
//
//  Pure placement geometry (declared in the header). Shifts `windowRect`
//  the minimum needed so the whole frame lies within `work`, returning the
//  new top-left. The bottom / right are corrected first, so a window larger
//  than the work area on an axis then pins to work's top / left there —
//  keeping the caption on-screen rather than the bottom button row off it.
//  A window that already fits is returned at its current position.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiHwndSource::ClampToWorkArea (const RECT & windowRect, const RECT & work)
{
    LONG   width  = windowRect.right  - windowRect.left;
    LONG   height = windowRect.bottom - windowRect.top;
    POINT  result = { windowRect.left, windowRect.top };



    if (result.x + width  > work.right)  { result.x = work.right  - width;  }
    if (result.y + height > work.bottom) { result.y = work.bottom - height; }
    if (result.x < work.left)            { result.x = work.left;            }
    if (result.y < work.top)             { result.y = work.top;             }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
//  Idempotent. Releases render resources first (they reference the
//  swap chain), then drops the device / swap chain, destroys the
//  HWND if owned, and unregisters the per-instance class.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::Destroy ()
{
    DXUI_ASSERT_UI_THREAD();

    ReleaseRenderResources();

    m_rtv.Reset();
    m_swapChain.Reset();
    m_compVisual.Reset();
    m_compTarget.Reset();
    m_compDevice.Reset();
    m_context.Reset();
    m_device.Reset();

    // Tear down the popup pool BEFORE destroying the HWND (popup
    // hosts hold non-owning device pointers and may have HWNDs
    // parented to ours).
    m_popupActive.clear();
    m_popupPool.clear();

    if (m_hwnd != nullptr && m_ownsHwnd)
    {
        DestroyWindow (m_hwnd);
    }
    m_hwnd       = nullptr;
    m_ownsHwnd   = false;
    m_adoptMode  = false;

    if (m_classRegistered && m_hInstance != nullptr)
    {
        UnregisterClassW (m_className.c_str(), m_hInstance);
        m_classRegistered = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateInAdoptMode
//
//  Wraps an existing HWND whose lifecycle, swap chain, and D3D
//  device the caller continues to own. No CreateWindow, no
//  DestroyWindow, no class registration, no render resources. The
//  caller's WndProc forwards messages via HandleMessage(); the host
//  classifies NC hits and tracks DPI / theme for its internal
//  panel tree. Tests may pass nullptr for existingHwnd.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiHwndSource::CreateInAdoptMode (
    HWND                              existingHwnd,
    const CreateParams              & params,
    std::unique_ptr<DxuiHwndSource> & outHost)
{
    HRESULT                          hr   = S_OK;
    std::unique_ptr<DxuiHwndSource>  host;
    UINT                             dpi  = 0;



    DXUI_ASSERT_UI_THREAD();

    host = std::unique_ptr<DxuiHwndSource> (new DxuiHwndSource());
    CPRA (host.get());

    host->m_params      = params;
    host->m_hwnd        = existingHwnd;
    host->m_hInstance   = params.hInstance;
    host->m_ownsHwnd    = false;
    host->m_synthetic   = false;
    host->m_adoptMode   = true;

    // Seed the DPI scaler from the adopted HWND so the NC hit-test
    // pixel-to-DIP math is coherent before the first WM_DPICHANGED.
    if (existingHwnd != nullptr)
    {
        dpi = GetDpiForWindow (existingHwnd);
    }
    if (dpi == 0)
    {
        dpi = s_kDefaultDpi;
    }
    host->m_scaler.SetDpi (dpi);

    // Host-owned caption in adopt mode: build it now (HWND + DPI are
    // live). The consumer drives RenderCaption / LayoutCaptionForClient
    // from its own pump; NC-mouse routing runs inside HandleMessage.
    if (params.captionStyle != DxuiCaptionStyle::None)
    {
        host->BuildCaption();
    }

    outHost = std::move (host);

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHitTestDelegate
//
//  Plugs in an adopt-mode classifier that wins over the framework
//  resize-edge / panel-tree walk when it returns anything other
//  than HTNOWHERE. Lets a consumer keep its bespoke caption and
//  system-button hit-testing without first reshaping its chrome
//  onto DxuiCaptionBar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetHitTestDelegate (std::function<LRESULT (POINT)> delegate)
{
    DXUI_ASSERT_UI_THREAD();

    m_hitTestDelegate = std::move (delegate);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetBeforePresentHook
//
//  Stores a callback that the host's WM_PAINT pump invokes once per
//  frame after the panel-tree Paint walk and before swap-chain
//  Present. Lets an external renderer (e.g. the Apple ][ framebuffer
//  D3D pass) composite into the host's back buffer without owning
//  the swap chain itself. Passing a null function clears any
//  previously-installed hook.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetBeforePresentHook (std::function<void()> hook)
{
    DXUI_ASSERT_UI_THREAD();

    m_beforePresentHook = std::move (hook);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAfterPaintHook
//
//  Stores a callback that the host's WM_PAINT pump invokes once per
//  frame after the panel-tree Paint walk and before swap-chain
//  Present, passing the back-buffer RTV and its pixel dimensions. Lets
//  a consumer run full-screen shader passes on the painted back buffer
//  (e.g. a settings live-preview blur / compose). Passing a null
//  function clears any previously-installed hook.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetAfterPaintHook (std::function<void(ID3D11RenderTargetView *, int, int)> hook)
{
    DXUI_ASSERT_UI_THREAD();

    m_afterPaintHook = std::move (hook);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetOverlayHooks
//
//  Stores the predicate + paint callback the pump uses to composite a client
//  modal overlay as its own top layer after the page has flushed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetOverlayHooks (std::function<bool()> isActive,
                                      std::function<void(IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &)> paint)
{
    DXUI_ASSERT_UI_THREAD();

    m_overlayActiveHook = std::move (isActive);
    m_overlayPaintHook  = std::move (paint);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetComposedOpacity
//
//  Fades the whole composited visual via IDCompositionVisual3::SetOpacity so
//  what is behind the window on the desktop (e.g. the live emulator) shows
//  through. No-op for a non-composited window. Committed immediately so the
//  change lands even between paint-pump frames.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetComposedOpacity (float opacity)
{
    ComPtr<IDCompositionVisual3>  visual3;


    DXUI_ASSERT_UI_THREAD();

    if (!m_compVisual || !m_compDevice)
    {
        return;
    }

    // The visual is created from a v2 (desktop) device, so it exposes
    // IDCompositionVisual3::SetOpacity -- the canonical per-visual opacity that
    // blends the whole composited window over whatever is behind it.
    if (SUCCEEDED (m_compVisual.As (&visual3)))
    {
        (void) visual3->SetOpacity (opacity);
    }
    (void) m_compDevice->Commit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleMessage
//
//  Public WndProc forwarder. Returns true when Dxui owns the
//  message end-to-end (caller returns outResult immediately);
//  returns false to let the caller's WndProc keep handling it.
//  In adopt mode Dxui owns only the STRUCTURAL non-client messages
//  (WM_NCCALCSIZE / WM_NCHITTEST) so the borderless frame + button
//  hit-testing are consistent; NC *mouse* messages (button hover /
//  press) are left to the consumer's WndProc, which owns the legacy
//  caption chrome and its SC_* dispatch. DPI and theme messages do
//  their tree-side propagation without claiming the message so the
//  caller can keep doing its own work (e.g. SetWindowPos in
//  Window::HandleDpiChanged).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHwndSource::HandleMessage (UINT msg, WPARAM wp, LPARAM lp, LRESULT & outResult)
{
    DXUI_ASSERT_UI_THREAD();

    outResult = 0;

    switch (msg)
    {
        case WM_NCCALCSIZE:
            outResult = HandleNcCalcSize (wp, lp);
            return true;

        case WM_NCHITTEST:
            outResult = HandleNcHitTest (lp);
            return true;

        case WM_NCMOUSEMOVE:
        case WM_NCMOUSELEAVE:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
            // Route caption system-button hover / press / dispatch to the
            // host-owned caption. Returns true (consumed) only when the
            // event lands on a caption button; otherwise the consumer's
            // WndProc keeps the message (caption drag, menu dismiss, ...).
            if (m_caption && RouteCaptionNcMouse (msg, wp, lp))
            {
                outResult = 0;
                return true;
            }
            return false;

        case WM_DPICHANGED:
            HandleDpiChanged (wp, lp);
            return false;

        case WM_SIZE:
            HandleSize (wp, lp);
            return false;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            HandleThemeChange();
            return false;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetTheme (const IDxuiTheme * theme)
{
    DXUI_ASSERT_UI_THREAD();

    m_theme = theme;
    m_focusManager.SetTheme (theme);
    if (RootPanel() != nullptr)
    {
        RootPanel()->OnThemeChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetClient
//
//  Install (or clear) the optional IDxuiHostClient hook so a
//  consumer can receive the Win32 messages the host does not own
//  end-to-end (WM_COMMAND, WM_KEYDOWN, WM_TIMER, ...). The host
//  stores a non-owning pointer; the client must outlive the host
//  or call SetClient(nullptr) before destruction.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetClient (IDxuiHostClient * client)
{
    DXUI_ASSERT_UI_THREAD();

    m_client = client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDefaultProcForTest
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetDefaultProcForTest (std::function<LRESULT (HWND, UINT, WPARAM, LPARAM)> defaultProc)
{
    DXUI_ASSERT_UI_THREAD();

    m_defaultProcForTest = std::move (defaultProc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTrackMouseEventForTest
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetTrackMouseEventForTest (std::function<BOOL (TRACKMOUSEEVENT *)> trackMouseEvent)
{
    DXUI_ASSERT_UI_THREAD();

    m_trackMouseEventForTest = std::move (trackMouseEvent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetContentPanel
//
//  Replace the root panel with a caller-supplied tree. Lets a
//  consumer install a fully-assembled content panel (a SettingsWindow
//  content tree, ...) as the host's paint / hit-test /
//  focus / accessibility root in one shot. The previous root is
//  destroyed. When the host already owns a real HWND, the new
//  panel's bounds are set from the current client rect so it lays
//  out immediately; in synthetic mode the panel inherits the
//  previous root's bounds.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetContentPanel (std::unique_ptr<DxuiPanel> panel)
{
    HRESULT  hr         = S_OK;
    RECT     bounds     = {};
    bool     liveLayout = false;


    DXUI_ASSERT_UI_THREAD();
    CBRAEx (panel != nullptr, E_INVALIDARG);

    //
    //  Capture the prior root's bounds (synthetic mode) or compute
    //  them from the live client rect (full-ownership mode) before
    //  swapping. Adopt mode without an HWND leaves bounds at zero;
    //  WM_SIZE on the caller's HWND will drive layout later.
    //
    if (m_hwnd != nullptr)
    {
        RECT  clientRectPx = {};

        if (GetClientRect (m_hwnd, &clientRectPx))
        {
            // Lay the root out in PHYSICAL PIXELS (matches widget paint).
            bounds     = clientRectPx;
            liveLayout = true;
        }
    }
    else if (m_root != nullptr)
    {
        bounds = m_root->Bounds();
    }

    m_root = std::move (panel);

    if (m_root != nullptr)
    {
        if (liveLayout)
        {
            RECT  rootBounds = bounds;

            if (m_params.insetRootBelowCaption && m_caption)
            {
                rootBounds.top += m_caption->PreferredHeightPx (m_scaler);
            }

            m_root->Layout (rootBounds, m_scaler);
        }
        else
        {
            m_root->SetBounds (bounds);
        }

        if (m_hwnd != nullptr)
        {
            NotifySystemButtonsMaximized (IsZoomed (m_hwnd) != FALSE);
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetContentRootRef
//
//  Point the host's paint / layout / focus / hit-test pump at an
//  externally-owned root panel (DxuiWindow, which IS its own content
//  root). The owned m_root is left intact but shadowed. When an HWND
//  exists the ref is laid out from the current client rect and the
//  focus manager is re-attached to it so keyboard focus + rendering
//  target the live content immediately.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetContentRootRef (DxuiPanel * root)
{
    RECT  clientRectPx = {};



    DXUI_ASSERT_UI_THREAD();

    m_rootRef = root;
    m_focusManager.Attach (RootPanel());

    if (m_rootRef != nullptr && m_hwnd != nullptr && GetClientRect (m_hwnd, &clientRectPx))
    {
        RECT  rootBounds = clientRectPx;

        if (m_params.insetRootBelowCaption && m_caption)
        {
            rootBounds.top += m_caption->PreferredHeightPx (m_scaler);
        }

        m_rootRef->Layout (rootBounds, m_scaler);
        NotifySystemButtonsMaximized (IsZoomed (m_hwnd) != FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTimer
//
//  Thin convenience wrapper around `::SetTimer` so consumers don't
//  have to reach for the global symbol. WM_TIMER dispatches to
//  `IDxuiHostClient::OnTimer` (DxuiHwndSource's WndProc already
//  forwards the message). Returns true iff the timer was scheduled;
//  no-ops in release when the host has no HWND.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHwndSource::SetTimer (UINT_PTR timerId, UINT intervalMs)
{
    HRESULT   hr     = S_OK;
    UINT_PTR  result = 0;


    DXUI_ASSERT_UI_THREAD();
    CBRA (m_hwnd != nullptr);

    result = ::SetTimer (m_hwnd, timerId, intervalMs, nullptr);

Error:
    return (result != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KillTimer
//
//  Thin convenience wrapper around `::KillTimer`. Returns true iff
//  the timer was found and cancelled; no-ops in release when the
//  host has no HWND.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHwndSource::KillTimer (UINT_PTR timerId)
{
    HRESULT  hr     = S_OK;
    BOOL     result = FALSE;


    DXUI_ASSERT_UI_THREAD();
    CBRA (m_hwnd != nullptr);

    result = ::KillTimer (m_hwnd, timerId);

Error:
    return (result != FALSE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDeviceAndSwapChain
//
//  D3D11 device with BGRA support (R2 — Direct2D interop requirement);
//  DXGI 1.2 flip-discard swap chain bound to the host HWND.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiHwndSource::CreateDeviceAndSwapChain ()
{
    HRESULT                hr            = S_OK;
    UINT                   createFlags   = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL      featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
    };
    D3D_FEATURE_LEVEL      obtainedLevel = D3D_FEATURE_LEVEL_11_0;
    ComPtr<IDXGIDevice>    dxgiDevice;
    ComPtr<IDXGIAdapter>   dxgiAdapter;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1  scd           = {};
    RECT                   clientRect    = {};



#if defined(_DEBUG)
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice (nullptr,
                            D3D_DRIVER_TYPE_HARDWARE,
                            nullptr,
                            createFlags,
                            featureLevels,
                            (UINT) _countof (featureLevels),
                            D3D11_SDK_VERSION,
                            m_device.GetAddressOf(),
                            &obtainedLevel,
                            m_context.GetAddressOf());

#if defined(_DEBUG)
    // If the debug runtime is missing (clean Windows install without the
    // Graphics Tools optional feature), retry without the debug flag.
    if (FAILED (hr))
    {
        createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice (nullptr,
                                D3D_DRIVER_TYPE_HARDWARE,
                                nullptr,
                                createFlags,
                                featureLevels,
                                (UINT) _countof (featureLevels),
                                D3D11_SDK_VERSION,
                                m_device.GetAddressOf(),
                                &obtainedLevel,
                                m_context.GetAddressOf());
    }
#endif

    CHRA (hr);

#if defined(_DEBUG)
    // Wire the D3D11 + DXGI InfoQueues so the debug layer DebugBreak()s
    // on the exact call that violates a rule rather than letting the
    // violation propagate into a later AV / DEVICE_REMOVED. We break on
    // WARNING too: the DEVICE_REMOVAL_PROCESS_POSSIBLY_AT_FAULT
    // diagnostic (and the upstream violation that triggers it) is often
    // emitted at WARNING severity.
    {
        ComPtr<ID3D11InfoQueue>  d3dInfoQueue;
        HRESULT                  hrD3dInfo = m_device.As (&d3dInfoQueue);


        if (SUCCEEDED (hrD3dInfo) && d3dInfoQueue)
        {
            d3dInfoQueue->SetBreakOnSeverity (D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            d3dInfoQueue->SetBreakOnSeverity (D3D11_MESSAGE_SEVERITY_ERROR,      TRUE);
            d3dInfoQueue->SetBreakOnSeverity (D3D11_MESSAGE_SEVERITY_WARNING,    TRUE);
        }
    }

    // DXGI keeps its own InfoQueue covering swap-chain / Present /
    // ResizeBuffers diagnostics that don't surface through D3D11's
    // queue. Resolve via the optional dxgidebug.dll entry point so we
    // degrade gracefully on SKUs without the Graphics Tools feature.
    {
        HMODULE  dxgiDebug = LoadLibraryW (L"dxgidebug.dll");


        if (dxgiDebug != nullptr)
        {
            using PFN_DXGIGetDebugInterface = HRESULT (WINAPI *) (REFIID, void **);
            auto  pfnGet = reinterpret_cast<PFN_DXGIGetDebugInterface> (GetProcAddress (dxgiDebug, "DXGIGetDebugInterface"));

            if (pfnGet != nullptr)
            {
                ComPtr<IDXGIInfoQueue>  dxgiInfoQueue;
                HRESULT                 hrDxgiInfo = pfnGet (IID_PPV_ARGS (dxgiInfoQueue.GetAddressOf()));

                if (SUCCEEDED (hrDxgiInfo) && dxgiInfoQueue)
                {
                    dxgiInfoQueue->SetBreakOnSeverity (DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                    dxgiInfoQueue->SetBreakOnSeverity (DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,      TRUE);
                    dxgiInfoQueue->SetBreakOnSeverity (DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING,    TRUE);
                }
            }
        }
    }
#endif

    hr = m_device.As (&dxgiDevice);
    CHRA (hr);

    hr = dxgiDevice->GetAdapter (dxgiAdapter.GetAddressOf());
    CHRA (hr);

    hr = dxgiAdapter->GetParent (IID_PPV_ARGS (dxgiFactory.GetAddressOf()));
    CHRA (hr);

    scd.Width            = 0;
    scd.Height           = 0;
    scd.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount      = 2;
    scd.Scaling          = DXGI_SCALING_STRETCH;
    scd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    if (m_params.composited)
    {
        // Composition swap chains require explicit non-zero dimensions
        // (unlike CreateSwapChainForHwnd, which auto-sizes from the HWND
        // when passed 0). Seed from the current client area; HandleSize
        // resizes it thereafter. Premultiplied alpha lets the desktop
        // compositor blend the frame over the windows behind it.
        GetClientRect (m_hwnd, &clientRect);
        scd.Width      = (UINT) std::max<LONG> (1, clientRect.right  - clientRect.left);
        scd.Height     = (UINT) std::max<LONG> (1, clientRect.bottom - clientRect.top);
        scd.Scaling    = DXGI_SCALING_STRETCH;
        scd.AlphaMode  = DXGI_ALPHA_MODE_PREMULTIPLIED;

        hr = dxgiFactory->CreateSwapChainForComposition (m_device.Get(),
                                                         &scd,
                                                         nullptr,
                                                         m_swapChain.GetAddressOf());
        CHRA (hr);

        // Create a v2 (desktop) DirectComposition device rather than the v1
        // DCompositionCreateDevice: only a v2+ device produces IDCompositionVisual2
        // visuals, and SetComposedOpacity's per-visual opacity effect (an
        // IDCompositionEffectGroup fed through IDCompositionVisual2::SetEffect)
        // needs one. A v1 visual has no SetEffect, so the live-preview fade would
        // silently no-op. The concrete object still exposes the v1 IDCompositionDevice
        // interface (Commit + CreateTargetForHwnd) and QIs to IDCompositionDevice3.
        ComPtr<IDCompositionDesktopDevice>  desktopDevice;
        ComPtr<IDCompositionVisual2>        compVisual2;

        hr = DCompositionCreateDevice2 (dxgiDevice.Get(),
                                        IID_PPV_ARGS (desktopDevice.GetAddressOf()));
        CHRA (hr);

        hr = desktopDevice.As (&m_compDevice);
        CHRA (hr);

        hr = desktopDevice->CreateTargetForHwnd (m_hwnd, TRUE, m_compTarget.GetAddressOf());
        CHRA (hr);

        hr = desktopDevice->CreateVisual (compVisual2.GetAddressOf());
        CHRA (hr);

        hr = compVisual2.As (&m_compVisual);
        CHRA (hr);

        hr = m_compVisual->SetContent (m_swapChain.Get());
        CHRA (hr);

        hr = m_compTarget->SetRoot (m_compVisual.Get());
        CHRA (hr);

        hr = m_compDevice->Commit();
        CHRA (hr);
    }
    else
    {
        scd.AlphaMode  = DXGI_ALPHA_MODE_IGNORE;

        // DXGI_SCALING_NONE (top-left anchored, no stretch) rather than the
        // STRETCH default: with the flip model, STRETCH makes DXGI scale the
        // one-frame-stale back buffer to the new window size on every WM_SIZE
        // step of a live edge-drag, which reads as the client area jittering.
        // NONE keeps the content pinned to the origin; paired with the
        // synchronous redraw in HandleSize the freshly exposed edge is filled
        // the same frame, so the resize stays steady. (Composition swap chains
        // only accept STRETCH, so this is the HWND path only.)
        scd.Scaling    = DXGI_SCALING_NONE;

        hr = dxgiFactory->CreateSwapChainForHwnd (m_device.Get(),
                                                  m_hwnd,
                                                  &scd,
                                                  nullptr,
                                                  nullptr,
                                                  m_swapChain.GetAddressOf());
        CHRA (hr);
    }

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateRenderResources
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiHwndSource::CreateRenderResources ()
{
    HRESULT  hr  = S_OK;



    m_painter      = std::make_unique<DxuiPainter>();
    m_textRenderer = std::make_unique<DxuiTextRenderer>();

    hr = m_painter->Initialize (m_device.Get(), m_context.Get());
    CHRA (hr);

    hr = m_textRenderer->Initialize (m_device.Get());
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseRenderResources
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::ReleaseRenderResources ()
{
    ReleaseBackBufferRtv();

    if (m_textRenderer != nullptr)
    {
        m_textRenderer->Shutdown();
        m_textRenderer.reset();
    }
    if (m_painter != nullptr)
    {
        m_painter->Shutdown();
        m_painter.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferRtv
//
//  Acquires buffer 0 from the swap chain, creates a render-target
//  view on it, and sets a viewport covering the full back buffer.
//  Called from CreateRenderResources after the swap chain is built
//  and from HandleSize after every ResizeBuffers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiHwndSource::CreateBackBufferRtv ()
{
    HRESULT                   hr           = S_OK;
    DXGI_SWAP_CHAIN_DESC1     scd          = {};
    D3D11_VIEWPORT            vp           = {};
    ComPtr<ID3D11Texture2D>   backBuffer;



    CBRA (m_swapChain);
    CBRA (m_device);
    CBRA (m_context);
    CBRA (m_rtv == nullptr);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (backBuffer.GetAddressOf()));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
    CHRA (hr);

    hr = m_swapChain->GetDesc1 (&scd);
    CHRA (hr);

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (float) scd.Width;
    vp.Height   = (float) scd.Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    // Rebind the Direct2D target bitmap (used by DxuiTextRenderer) to whichever
    // surface is live: the offscreen content texture when this window is on the
    // compose path, or the new back-buffer surface otherwise. Routed through the
    // base's BindTextTarget so the text-target binding state stays tracked
    // across resizes. No-op if the text renderer hasn't been Initialized yet
    // (e.g. mid-tear-down).
    hr = BindTextTarget (HasComposeHook());
    CHRA (hr);

Error:

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  BackBufferSurface
//
//  DxuiRenderTarget surface contract: the swap-chain back buffer as an
//  IDXGISurface, so the base can bind the Direct2D text target to it. Returns
//  null before the swap chain exists.
//
////////////////////////////////////////////////////////////////////////////////

ComPtr<IDXGISurface> DxuiHwndSource::BackBufferSurface () const
{
    ComPtr<ID3D11Texture2D>  backBuffer;
    ComPtr<IDXGISurface>     surface;


    if (m_swapChain == nullptr)
    {
        return nullptr;
    }
    if (FAILED (m_swapChain->GetBuffer (0, IID_PPV_ARGS (backBuffer.GetAddressOf()))))
    {
        return nullptr;
    }
    if (FAILED (backBuffer.As (&surface)))
    {
        return nullptr;
    }
    return surface;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseBackBufferRtv
//
//  Drops the back-buffer RTV. Must be called before ResizeBuffers so
//  the back-buffer reference count drops to zero and the resize can
//  succeed. Idempotent.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::ReleaseBackBufferRtv ()
{
    if (m_textRenderer != nullptr)
    {
        m_textRenderer->UnbindBackBuffer();
    }
    if (m_context && m_rtv)
    {
        ID3D11RenderTargetView *  nullRtv[1] = { nullptr };

        m_context->OMSetRenderTargets (1, nullRtv, nullptr);
    }
    m_rtv.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BackBufferSizePx
//
//  DxuiRenderTarget surface contract: the back buffer's pixel size, read
//  from the swap-chain description. Returns {0,0} before the swap chain
//  exists so RenderFrame no-ops.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiHwndSource::BackBufferSizePx () const
{
    DXGI_SWAP_CHAIN_DESC1  scd = {};


    if (m_swapChain == nullptr || FAILED (m_swapChain->GetDesc1 (&scd)))
    {
        return SIZE{ 0, 0 };
    }
    return SIZE{ (LONG) scd.Width, (LONG) scd.Height };
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintContent
//
//  DxuiRenderTarget surface contract: walk the root panel tree, the
//  host-owned caption, and any client modal overlay onto `target`. The base
//  RenderFrame has already cleared the target and run the before-present hook
//  (e.g. the Apple ][ framebuffer via D3DRenderer::UploadAndComposite); it
//  runs the after-paint hook and Presents afterwards. Split out of the old
//  PaintPump so the clear/present orchestration lives in DxuiRenderTarget.
//
//  Bails cleanly if the painter / text renderer / root are missing, and never
//  leaves the painter / text renderer mid-frame after an early CHRA bail.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::PaintContent (ID3D11RenderTargetView * target, int widthPx, int heightPx, const IDxuiTheme & theme)
{
    HRESULT  hr           = S_OK;
    bool     painterBegun = false;
    bool     textBegun    = false;


    DXUI_ASSERT_UI_THREAD();

    if (RootPanel() == nullptr || m_painter == nullptr || m_textRenderer == nullptr)
    {
        return;
    }

    // Walk the panel tree. Painter buffers geometry between Begin / End; the
    // text renderer composites Direct2D over the same target between
    // BeginDraw / EndDraw. The D2D bitmap is bound once per back-buffer
    // lifetime by CreateBackBufferRtv.
    hr = m_painter->Begin (widthPx, heightPx);
    CHRA (hr);
    painterBegun = true;

    hr = m_textRenderer->BeginDraw();
    CHRA (hr);
    textBegun = true;

    RootPanel()->Paint (*m_painter, *m_textRenderer, theme);

    // Host-owned caption paints last so it overlays the top strip (the
    // before-present hook fills the whole back buffer, the chrome bands paint
    // over it, and the caption sits on top).
    if (m_caption)
    {
        m_caption->Paint (*m_painter, *m_textRenderer, theme);
    }

    // Flush the page: painter (D3D control fills) FIRST, then the text (D2D
    // glyphs / labels) so the foreground composites on top of the fills --
    // matching the proven UiShell::Render order.
    hr = m_painter->End (target);
    painterBegun = false;
    CHRA (hr);

    hr = m_textRenderer->EndDraw();
    textBegun = false;
    CHRA (hr);

    // A client modal overlay (e.g. the Settings color picker) paints as a
    // SEPARATE top layer with its OWN fill+text flush, so its dialog fill
    // covers the page's already-committed text and its own labels sit on top
    // of that. (Painting it into the page's batch would leave page text --
    // flushed last of all -- bleeding through the dialog.)
    if (m_overlayActiveHook && m_overlayActiveHook() && m_overlayPaintHook)
    {
        hr = m_painter->Begin (widthPx, heightPx);
        CHRA (hr);
        painterBegun = true;

        hr = m_textRenderer->BeginDraw();
        CHRA (hr);
        textBegun = true;

        m_overlayPaintHook (*m_painter, *m_textRenderer, theme);

        hr = m_painter->End (target);
        painterBegun = false;
        CHRA (hr);

        hr = m_textRenderer->EndDraw();
        textBegun = false;
        CHRA (hr);
    }

Error:
    // Make sure the painter / text renderer don't stay mid-frame after an
    // early CHRA bail-out; the next paint pass would otherwise see them in an
    // inconsistent state.
    if (textBegun)
    {
        (void) m_textRenderer->EndDraw();
    }
    if (painterBegun)
    {
        (void) m_painter->End (target);
    }
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PresentFrame
//
//  DxuiRenderTarget surface contract: present the swap chain, then commit the
//  DirectComposition device (composited-transparent mode drives the swap chain
//  through a DComp visual, so the desktop compositor only picks up the new
//  frame after a Commit).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::PresentFrame ()
{
    HRESULT  hr = S_OK;


    if (m_swapChain == nullptr)
    {
        return;
    }

    hr = m_swapChain->Present (m_params.presentSyncInterval, 0);
    CHRA (hr);

    if (m_compDevice)
    {
        hr = m_compDevice->Commit();
        CHRA (hr);
    }

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintPump
//
//  WM_PAINT body for full-ownership mode. Delegates the frame orchestration to
//  DxuiRenderTarget::RenderFrame (clear -> before-present hook -> PaintContent
//  -> after-paint / compose -> PresentFrame), which bails cleanly if the
//  painter / text renderer / RTV / swap chain are missing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::PaintPump ()
{
    DXUI_ASSERT_UI_THREAD();
    RenderFrame (m_theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDwmConfiguration
//
//  Drives the four DxuiDwm helpers off the CreateParams. Called once
//  by Create(), and again by HandleThemeChange() when Windows fires
//  WM_SETTINGCHANGE / WM_THEMECHANGED / WM_DWMCOLORIZATIONCOLORCHANGED.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::ApplyDwmConfiguration ()
{
    bool  wantMica = false;



    if (m_hwnd == nullptr)
    {
        return;
    }

    wantMica = (m_params.backdrop == DxuiHwndSourceBackdrop::Mica);

    // Extend the frame first — Mica is invisible without it, and the
    // OS drop-shadow also depends on this even when backdrop is None.
    DxuiDwm::ExtendFrameIntoClientArea (m_hwnd, s_kExtendFrameInsetPx);
    DxuiDwm::ApplyRoundedCorners       (m_hwnd, m_params.roundedCorners);
    DxuiDwm::ApplyMicaBackdrop         (m_hwnd, wantMica);
    DxuiDwm::ApplyImmersiveDarkMode    (m_hwnd, m_params.darkMode);

#ifdef _DEBUG
    m_dwmSeam.extendFrameApplied    = true;
    m_dwmSeam.roundedCornersApplied = m_params.roundedCorners;
    m_dwmSeam.micaBackdropApplied   = wantMica;
    m_dwmSeam.darkModeApplied       = m_params.darkMode;
    m_dwmSeam.backdropRequested     = m_params.backdrop;
    m_dwmSeam.roundedRequested      = m_params.roundedCorners;
    m_dwmSeam.darkRequested         = m_params.darkMode;
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProcThunk
//
//  Forwards Win32 messages to the per-instance WndProc. Stashes
//  `this` in GWLP_USERDATA on WM_NCCREATE — same pattern as Casso's
//  legacy Window class.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DxuiHwndSource::s_WndProcThunk (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DxuiHwndSource *  pThis = nullptr;
    CREATESTRUCTW  *  pcs   = nullptr;



    if (msg == WM_NCCREATE)
    {
        pcs   = reinterpret_cast<CREATESTRUCTW *> (lp);
        pThis = static_cast<DxuiHwndSource *> (pcs->lpCreateParams);
        SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (pThis));
        if (pThis != nullptr)
        {
            pThis->m_hwnd = hwnd;
        }
    }
    else
    {
        pThis = reinterpret_cast<DxuiHwndSource *> (GetWindowLongPtr (hwnd, GWLP_USERDATA));
    }

    if (pThis == nullptr)
    {
        return DefWindowProc (hwnd, msg, wp, lp);
    }

    return pThis->WndProc (msg, wp, lp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WndProc
//
//  Per-instance message router. Handles the NC family in full and
//  forwards everything else to DefWindowProc. Painting and input
//  routing wire up in later phases once the consumer ships its
//  paint pump on top of this primitive.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHwndSource::WndProc (UINT msg, WPARAM wp, LPARAM lp)
{
    DXUI_ASSERT_UI_THREAD();

    switch (msg)
    {
        case WM_NCCALCSIZE:
            return HandleNcCalcSize (wp, lp);

        case WM_NCHITTEST:
        {
            // Framework classification (resize edges + delegate +
            // panel-tree walk) wins for chrome hits; HTCLIENT /
            // HTNOWHERE outcomes fall through to the client hook so
            // a consumer can reclassify what the framework treats
            // as plain client area.
            LRESULT  hostHt = HandleNcHitTest (lp);

            if (hostHt != HTCLIENT && hostHt != HTNOWHERE)
            {
                return hostHt;
            }
            if (m_client != nullptr)
            {
                return m_client->OnNcHitTest (m_hwnd, msg, wp, lp);
            }
            return hostHt;
        }

        case WM_NCMOUSEMOVE:
        {
            POINT  ptScreen = { GET_X_LPARAM (lp), GET_Y_LPARAM (lp) };

            if (m_client != nullptr &&
                m_client->OnNcMouseMove ((LRESULT) wp, ptScreen.x, ptScreen.y) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            return HandleNcMouse (msg, wp, lp);
        }

        case WM_NCMOUSELEAVE:
            if (m_client != nullptr && m_client->OnNcMouseLeave() == DxuiMessageResult::Handled)
            {
                return 0;
            }
            return HandleNcMouse (msg, wp, lp);

        case WM_NCLBUTTONDOWN:
        {
            POINT  ptScreen = { GET_X_LPARAM (lp), GET_Y_LPARAM (lp) };

            if (m_client != nullptr &&
                m_client->OnNcLButtonDown ((LRESULT) wp, ptScreen.x, ptScreen.y) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            return HandleNcMouse (msg, wp, lp);
        }

        case WM_NCLBUTTONUP:
        {
            POINT  ptScreen = { GET_X_LPARAM (lp), GET_Y_LPARAM (lp) };

            if (m_client != nullptr &&
                m_client->OnNcLButtonUp ((LRESULT) wp, ptScreen.x, ptScreen.y) == DxuiMessageResult::Handled)
            {
                DispatchNcUpToTrackedButton (lp);
                return 0;
            }
            return HandleNcMouse (msg, wp, lp);
        }

        case WM_DPICHANGED:
            HandleDpiChanged (wp, lp);
            if (m_client != nullptr)
            {
                m_client->OnDpiChanged (m_scaler.Dpi());
            }
            return 0;

        case WM_DPICHANGED_BEFOREPARENT:
            // Forward to every active pooled DxuiPopupHost so popups
            // straddling a monitor boundary re-DPI before the owner
            // repaints.
            for (DxuiPopupHost * popup : m_popupActive)
            {
                if (popup != nullptr)
                {
                    popup->HandleDpiChanged ((UINT) HIWORD (wp));
                }
            }
            return 0;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            HandleThemeChange();
            break;

        case WM_SIZE:
            HandleSize (wp, lp);
            if (m_client != nullptr)
            {
                (void) m_client->OnSize (LOWORD (lp), HIWORD (lp));
            }
            break;

        case WM_CREATE:
            if (m_client != nullptr)
            {
                return m_client->OnCreate (m_hwnd, msg, wp, lp);
            }
            break;

        case WM_DESTROY:
            // Bookkeeping only; the actual teardown happens in
            // Destroy()/~DxuiHwndSource(). Notify the client so it
            // can persist state (window placement, etc.) before
            // the HWND goes away.
            if (m_client != nullptr)
            {
                m_client->OnDestroy();
            }
            break;

        case WM_CLOSE:
            if (m_client != nullptr && m_client->OnClose() == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_CHAR:
            if (m_client != nullptr && m_client->OnChar (wp, lp) == DxuiMessageResult::Handled)
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_COMMAND:
            if (m_client != nullptr &&
                m_client->OnCommandEx (LOWORD (wp),
                                       HIWORD (wp),
                                       reinterpret_cast<HWND> (lp)) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (m_client != nullptr && m_client->OnKeyDown (wp, lp) == DxuiMessageResult::Handled)
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (m_client != nullptr && m_client->OnKeyUp (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_SETCURSOR:
            if (m_client != nullptr && m_client->OnSetCursor (LOWORD (lp)) == DxuiMessageResult::Handled)
            {
                return TRUE;
            }
            break;

        case WM_MOUSEMOVE:
            TrackClientMouseLeave();
            if (m_client != nullptr && m_client->OnMouseMove (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_MOUSELEAVE:
            // The cursor left the client area -- into an NC region of
            // this window (caption / system button / resize edge) or
            // off-window entirely. Clear client hover and stop tracking;
            // the next WM_MOUSEMOVE re-arms. NC hover is handled
            // independently via WM_NCMOUSEMOVE / WM_NCMOUSELEAVE.
            //
            // (Historically this re-armed tracking and ignored the leave
            // whenever WindowFromPoint still resolved to this window, to
            // absorb the continuous TME_LEAVE storm caused by a child
            // render surface that covered the client area. That child is
            // gone -- a single top-level window owns everything now -- so
            // WindowFromPoint resolves to this HWND even for its own NC
            // edges, which made that guard re-arm in a tight loop and
            // wedge the cursor / hover. A leave is now always real.)
            m_clientMouseLeaveTracking = false;
            if (m_client != nullptr && m_client->OnMouseLeave() == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_GETMINMAXINFO:
            if (m_client != nullptr && m_client->OnGetMinMax (reinterpret_cast<MINMAXINFO *> (lp)) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
            if (m_client != nullptr && m_client->OnLButtonDown (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (m_client != nullptr && m_client->OnLButtonUp (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_RBUTTONDOWN:
            if (m_client != nullptr && m_client->OnRButtonDown (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_RBUTTONUP:
            if (m_client != nullptr && m_client->OnRButtonUp (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_MOUSEWHEEL:
            if (m_client != nullptr && m_client->OnMouseWheel (wp, lp, false) == DxuiMessageResult::Handled)
            {
                if (!m_suppressInputInvalidate) { InvalidateRect (m_hwnd, nullptr, FALSE); }
                return 0;
            }
            break;

        case WM_MOUSEHWHEEL:
            if (m_client != nullptr && m_client->OnMouseWheel (wp, lp, true) == DxuiMessageResult::Handled)
            {
                if (!m_suppressInputInvalidate) { InvalidateRect (m_hwnd, nullptr, FALSE); }
                return 0;
            }
            break;

        case WM_ACTIVATEAPP:
            if (m_client != nullptr && m_client->OnActivateApp (wp != 0) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (m_client != nullptr && m_client->OnKillFocus() == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_CANCELMODE:
            if (m_client != nullptr && m_client->OnCancelMode() == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_MOVE:
            if (m_client != nullptr &&
                m_client->OnMove ((int) (short) LOWORD (lp),
                                  (int) (short) HIWORD (lp)) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_ENTERSIZEMOVE:
            // The OS is about to run its own modal move / size loop, during which
            // our outer pump stops. Run a short internal timer so the client can
            // keep painting; fall through so the OS loop still starts normally.
            // Also tick once right now so animation continues from the first
            // frame of the drag instead of stalling until the first timer fires.
            ::SetTimer (m_hwnd, s_kModalLoopTimerId, USER_TIMER_MINIMUM, nullptr);
            if (m_client != nullptr) { m_client->OnModalLoopTick(); }
            break;

        case WM_EXITSIZEMOVE:
            ::KillTimer (m_hwnd, s_kModalLoopTimerId);
            break;

        case WM_TIMER:
            // The host-owned keep-alive tick (armed by WM_ENTERSIZEMOVE) is not a
            // client SetTimer id; route it to OnModalLoopTick, not OnTimer.
            if (wp == s_kModalLoopTimerId)
            {
                if (m_client != nullptr) { m_client->OnModalLoopTick(); }
                return 0;
            }
            if (m_client != nullptr && m_client->OnTimer (static_cast<UINT_PTR> (wp)) == DxuiMessageResult::Handled)
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_NOTIFY:
            if (m_client != nullptr && m_client->OnNotify (wp, lp) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_DRAWITEM:
            if (m_client != nullptr)
            {
                return m_client->OnDrawItem (m_hwnd, msg, wp, lp);
            }
            break;

        case WM_CTLCOLORSTATIC:
            if (m_client != nullptr)
            {
                LRESULT  brush = m_client->OnCtlColorStatic (reinterpret_cast<HDC> (wp),
                                                             reinterpret_cast<HWND> (lp));

                if (brush != 0)
                {
                    return brush;
                }
            }
            break;

        case WM_INITMENUPOPUP:
            if (m_client != nullptr &&
                m_client->OnInitMenuPopup (reinterpret_cast<HMENU> (wp),
                                           LOWORD (lp),
                                           HIWORD (lp) != 0) == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;

        case WM_PAINT:
        {
            // Full-ownership mode runs the host's panel-tree paint
            // pump (clear + walk children + before-present hook +
            // Present). Adopt / synthetic / opt-out-swap-chain modes
            // have no host swap chain to drive, so fall through to
            // the client's OnPaint() for legacy chrome painters.
            if (m_swapChain && m_rtv)
            {
                PAINTSTRUCT  ps = {};

                BeginPaint (m_hwnd, &ps);
                PaintPump();
                EndPaint   (m_hwnd, &ps);
                return 0;
            }

            if (m_client != nullptr && m_client->OnPaint() == DxuiMessageResult::Handled)
            {
                return 0;
            }
            break;
        }
    }

    return DefaultProc (msg, wp, lp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DefaultProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHwndSource::DefaultProc (UINT msg, WPARAM wp, LPARAM lp)
{
    if (m_defaultProcForTest)
    {
        return m_defaultProcForTest (m_hwnd, msg, wp, lp);
    }

    return DefWindowProc (m_hwnd, msg, wp, lp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMouseEventHost
//
////////////////////////////////////////////////////////////////////////////////

BOOL DxuiHwndSource::TrackMouseEventHost (TRACKMOUSEEVENT * pEvent)
{
    if (m_trackMouseEventForTest)
    {
        return m_trackMouseEventForTest (pEvent);
    }

    return TrackMouseEvent (pEvent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackClientMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::TrackClientMouseLeave()
{
    TRACKMOUSEEVENT  tme = { sizeof (tme) };



    if (m_clientMouseLeaveTracking)
    {
        return;
    }

    tme.dwFlags   = TME_LEAVE;
    tme.hwndTrack = m_hwnd;
    if (TrackMouseEventHost (&tme))
    {
        m_clientMouseLeaveTracking = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcCalcSize
//
//  Mirrors the microsoft/terminal NonClientIslandWindow trick: let
//  DefWindowProc compute the default frame (so Windows still does the
//  resize-border math + Aero Snap awareness), then re-apply the
//  original top so the title-bar area collapses into our client rect
//  for custom-painted chrome.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHwndSource::HandleNcCalcSize (WPARAM wp, LPARAM lp)
{
    NCCALCSIZE_PARAMS *  pParams      = nullptr;
    LRESULT              defResult    = 0;
    LONG                 originalTop  = 0;



    if (!m_params.borderless)
    {
        return DefaultProc (WM_NCCALCSIZE, wp, lp);
    }

    if (wp == FALSE)
    {
        return 0;
    }

    pParams = reinterpret_cast<NCCALCSIZE_PARAMS *> (lp);
    if (pParams == nullptr)
    {
        return 0;
    }

    originalTop = pParams->rgrc[0].top;
    defResult   = DefaultProc (WM_NCCALCSIZE, wp, lp);
    if (defResult != 0)
    {
        return defResult;
    }

    pParams->rgrc[0].top = originalTop;
    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcHitTest
//
//  Resolves a screen-space mouse position to a Win32 HT* code by
//  delegating to the framework classifier (resize edges + tree walk
//  + DxuiHitTestKind → HT* mapping).
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHwndSource::HandleNcHitTest (LPARAM lp)
{
    POINT            ptScreen     = {};
    POINT            ptClient     = {};
    RECT             rcClient     = {};
    DxuiHitTestKind  kind         = DxuiHitTestKind::Client;
    LRESULT          delegateHt   = HTNOWHERE;



    ptScreen.x = GET_X_LPARAM (lp);
    ptScreen.y = GET_Y_LPARAM (lp);

    // Adopt-mode plug-in: the consumer's existing caption / system-
    // button classifier wins when it produces anything other than
    // HTNOWHERE. Falls through to the framework classifier when the
    // delegate abstains or is not set.
    if (m_hitTestDelegate)
    {
        delegateHt = m_hitTestDelegate (ptScreen);
        if (delegateHt != HTNOWHERE)
        {
            return delegateHt;
        }
    }

    if (m_hwnd == nullptr)
    {
        return HTNOWHERE;
    }

    ptClient = ptScreen;
    if (!ScreenToClient (m_hwnd, &ptClient))
    {
        return HTNOWHERE;
    }

    if (!GetClientRect (m_hwnd, &rcClient))
    {
        return HTNOWHERE;
    }

    // Convert client-pixel point to client-DIP before running the
    // classifier — controls store bounds in DIPs.
    ptClient.x = MulDiv (ptClient.x, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    ptClient.y = MulDiv (ptClient.y, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    rcClient.right  = MulDiv (rcClient.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    rcClient.bottom = MulDiv (rcClient.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());

    kind = ClassifyHitInternal (ptClient, rcClient);
    return KindToHt (kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcMouse
//
//  Routes NC mouse state to custom system-button controls and lets
//  DefWindowProc keep the standard caption / resize behaviours for
//  everything else.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHwndSource::HandleNcMouse (UINT msg, WPARAM wp, LPARAM lp)
{
    POINT                ptScreen = {};
    POINT                ptClient = {};
    IDxuiControl       * control  = nullptr;
    DxuiMouseEvent       ev       = {};
    TRACKMOUSEEVENT      tme      = { sizeof (tme) };



    if (msg == WM_NCMOUSELEAVE)
    {
        if (m_lastHoveredNcControl != nullptr)
        {
            ev.kind = DxuiMouseEventKind::Leave;
            m_lastHoveredNcControl->OnMouse (ev);
            m_lastHoveredNcControl = nullptr;
            if (m_hwnd != nullptr)
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);
            }
        }

        // Mirror the move path: forward the leave to DefWindowProc so the
        // DWM's caption-button hover bookkeeping tears down cleanly.
        return DefaultProc (msg, wp, lp);
    }

    if (m_hwnd == nullptr)
    {
        return DefaultProc (msg, wp, lp);
    }

    ptScreen.x = GET_X_LPARAM (lp);
    ptScreen.y = GET_Y_LPARAM (lp);
    ptClient   = ptScreen;
    if (!ScreenToClient (m_hwnd, &ptClient))
    {
        return DefaultProc (msg, wp, lp);
    }

    ptClient.x = MulDiv (ptClient.x, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    ptClient.y = MulDiv (ptClient.y, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    control    = FindNcSystemControlAt (ptClient);
    if (control == nullptr)
    {
        if (m_lastHoveredNcControl != nullptr)
        {
            ev.kind       = (msg == WM_NCLBUTTONUP) ? DxuiMouseEventKind::Up : DxuiMouseEventKind::Leave;
            ev.button     = (msg == WM_NCLBUTTONUP) ? DxuiMouseButton::Left  : DxuiMouseButton::None;
            ev.positionDip = ptClient;
            m_lastHoveredNcControl->OnMouse (ev);
            m_lastHoveredNcControl = nullptr;
            InvalidateRect (m_hwnd, nullptr, FALSE);
        }
        return DefaultProc (msg, wp, lp);
    }

    if (msg == WM_NCMOUSEMOVE)
    {
        if (m_lastHoveredNcControl != nullptr && m_lastHoveredNcControl != control)
        {
            ev.kind = DxuiMouseEventKind::Leave;
            m_lastHoveredNcControl->OnMouse (ev);
        }

        tme.dwFlags = TME_NONCLIENT | TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEventHost (&tme);

        ev.kind = DxuiMouseEventKind::Move;
        m_lastHoveredNcControl = control;
    }
    else if (msg == WM_NCLBUTTONDOWN)
    {
        ev.kind   = DxuiMouseEventKind::Down;
        ev.button = DxuiMouseButton::Left;
    }
    else if (msg == WM_NCLBUTTONUP)
    {
        ev.kind   = DxuiMouseEventKind::Up;
        ev.button = DxuiMouseButton::Left;
    }
    else
    {
        return DefaultProc (msg, wp, lp);
    }

    ev.positionDip = ptClient;
    control->OnMouse (ev);
    InvalidateRect (m_hwnd, nullptr, FALSE);

    (void) wp;

    // Forward NC mouse-move to DefWindowProc after painting our own button
    // hover. Eating it (returning 0) is non-conformant for a custom frame;
    // DefWindowProc draws nothing of its own here (WM_NCCALCSIZE removed the
    // standard non-client area) but keeps the DWM's caption-button hover
    // bookkeeping alive. Button presses (WM_NCLBUTTONDOWN / UP) stay fully
    // owned so we drive min / max / close ourselves.
    //
    // NOTE: this is a prerequisite for the Win11 snap-layouts flyout but is
    // not sufficient on its own here -- the host's flip swap chain covers
    // the entire window (caption included), so the DWM has no non-client
    // redirection surface in the maximize-button region to host the flyout.
    if (msg == WM_NCMOUSEMOVE)
    {
        return DefaultProc (msg, wp, lp);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchNcUpToTrackedButton
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::DispatchNcUpToTrackedButton (LPARAM lp)
{
    POINT           ptScreen = {};
    POINT           ptClient = {};
    DxuiMouseEvent  ev       = {};



    if (m_lastHoveredNcControl == nullptr)
    {
        return;
    }

    ptScreen.x = GET_X_LPARAM (lp);
    ptScreen.y = GET_Y_LPARAM (lp);
    ptClient   = ptScreen;
    if (m_hwnd != nullptr && ScreenToClient (m_hwnd, &ptClient))
    {
        ptClient.x = MulDiv (ptClient.x, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
        ptClient.y = MulDiv (ptClient.y, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    }

    ev.kind        = DxuiMouseEventKind::Up;
    ev.button      = DxuiMouseButton::Left;
    ev.positionDip = ptClient;
    m_lastHoveredNcControl->OnMouse (ev);
    m_lastHoveredNcControl = nullptr;

    if (m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDpiChanged
//
//  Updates the scaler, re-positions / -sizes the window per the
//  suggested rect, and triggers a relayout pass on the root panel.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::HandleDpiChanged (WPARAM wp, LPARAM lp)
{
    UINT          newDpi      = HIWORD (wp);
    const RECT *  suggested   = reinterpret_cast<const RECT *> (lp);
    RECT          rcClient    = {};



    if (newDpi == 0)
    {
        newDpi = s_kDefaultDpi;
    }
    m_scaler.SetDpi (newDpi);

    if (suggested != nullptr && m_hwnd != nullptr)
    {
        SetWindowPos (m_hwnd,
                      nullptr,
                      suggested->left,
                      suggested->top,
                      suggested->right  - suggested->left,
                      suggested->bottom - suggested->top,
                      SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (m_hwnd != nullptr && m_root != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        MaybeRelayoutRoot (rcClient);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSize
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::HandleSize (WPARAM wp, LPARAM lp)
{
    HRESULT  hr        = S_OK;
    RECT     rcClient  = {};
    UINT     widthPx   = LOWORD (lp);
    UINT     heightPx  = HIWORD (lp);



    if (wp == SIZE_MAXIMIZED)
    {
        NotifySystemButtonsMaximized (true);
    }
    else if (wp == SIZE_RESTORED)
    {
        NotifySystemButtonsMaximized (false);
    }

    // Resize the swap chain to the new client extent. Composition swap chains
    // (full-ownership DxuiWindows such as the Settings sheet) have no window to
    // inherit dimensions from, so ResizeBuffers(0, 0, ...) fails with "a
    // non-zero Width and Height must be specified for Composition SwapChains";
    // pass the explicit WM_SIZE client size instead -- which also works for the
    // hwnd swap chain. A zero extent (e.g. SIZE_MINIMIZED) is skipped so the
    // buffers keep their last valid size.
    if (m_swapChain && widthPx > 0 && heightPx > 0)
    {
        ReleaseBackBufferRtv();
        (void) m_swapChain->ResizeBuffers (0, widthPx, heightPx, DXGI_FORMAT_UNKNOWN, 0);

        hr = CreateBackBufferRtv();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_root != nullptr && m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        MaybeRelayoutRoot (rcClient);
    }

    // Re-render synchronously at the new size. ResizeBuffers discarded the back
    // buffer, so without an immediate frame DWM composites the blank / stale
    // swap chain during a live edge-drag -- which reads as the content subtly
    // shifting as the window resizes. Full-ownership mode only; adopt mode's
    // client repaints through its own OnSize.
    if (m_ownsPaintPump)
    {
        RenderFrame (m_theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleThemeChange
//
//  Re-broadcast theme-changed through the tree and re-apply the
//  DwM bits (rounded corners / Mica / immersive-dark-mode may need
//  re-asserting after a system theme switch).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::HandleThemeChange ()
{
    ApplyDwmConfiguration();

    if (RootPanel() != nullptr)
    {
        RootPanel()->OnThemeChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MaybeRelayoutRoot
//
//  Drives a root-panel layout pass for a new client-pixel rect, but
//  ONLY when the host owns its paint pump (full-ownership mode with a
//  live swap chain). In adopt mode -- or full-ownership mode created
//  with createSwapChain = false, where the consumer paints + lays out
//  the chrome itself -- the host must not run a second, competing
//  layout pass. Doing so previously double-laid-out adopted chrome
//  (e.g. the menu bar) against a different DxuiDpiScaler than the
//  consumer's, collapsing measured item spacing on resize.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::MaybeRelayoutRoot (const RECT & clientPx)
{
    RECT  boundsDip = clientPx;



    if (!m_ownsPaintPump || RootPanel() == nullptr)
    {
        return;
    }

    //
    //  The root lays out in PHYSICAL PIXELS -- matching how Dxui leaf
    //  widgets paint their bounds and the adopt-mode convention. (The
    //  caption is the exception: it self-scales from DIP bounds, so it is
    //  laid out separately, below, from a DIP rect.)
    //
    {
        RECT  rootPx = clientPx;

        if (m_params.insetRootBelowCaption && m_caption)
        {
            rootPx.top += m_caption->PreferredHeightPx (m_scaler);
        }

        RootPanel()->Layout (rootPx, m_scaler);
    }

    boundsDip.right  = MulDiv (clientPx.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    boundsDip.bottom = MulDiv (clientPx.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());

    LayoutCaption (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildCaption
//
//  Constructs the host-owned DxuiCaptionBar for the configured caption
//  style, hands it the window HWND so its system buttons can dispatch,
//  seeds the title from the window text, and lays it out against the
//  current client rect.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::BuildCaption ()
{
    RECT  clientPx  = {};
    RECT  clientDip = {};



    DXUI_ASSERT_UI_THREAD();

    m_caption = std::make_unique<DxuiCaptionBar>();
    m_caption->ConfigureButtons (m_params.captionStyle == DxuiCaptionStyle::Standard
                                     ? DxuiCaptionBar::Buttons::MinMaxClose
                                     : DxuiCaptionBar::Buttons::CloseOnly);
    m_caption->SetSystemHwnd (m_hwnd);
    m_caption->SetTitle      (m_params.title);
    m_caption->SetMaximized  (IsZoomed (m_hwnd) != FALSE);

    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &clientPx))
    {
        clientDip        = clientPx;
        clientDip.right  = MulDiv (clientPx.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
        clientDip.bottom = MulDiv (clientPx.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
        LayoutCaption (clientDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutCaption
//
//  Positions the host-owned caption as a full-width strip at the top of
//  the client area, one PreferredHeightDip tall, and refreshes its
//  maximize-button glyph from the live zoom state.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::LayoutCaption (const RECT & clientDip)
{
    RECT  rc = {};



    DXUI_ASSERT_UI_THREAD();

    if (!m_caption)
    {
        return;
    }

    rc.left   = 0;
    rc.top    = 0;
    rc.right  = clientDip.right;
    rc.bottom = m_caption->PreferredHeightDip();

    m_caption->SetMaximized (IsZoomed (m_hwnd) != FALSE);
    m_caption->Layout (rc, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTitle
//
//  SetWindowText-style title update: drives the Win32 window text (so
//  Alt-Tab / taskbar stay correct) and the host-owned caption glyph.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetTitle (const std::wstring & title)
{
    DXUI_ASSERT_UI_THREAD();

    m_params.title = title;

    if (m_hwnd != nullptr)
    {
        SetWindowTextW (m_hwnd, title.c_str());
    }
    if (m_caption)
    {
        m_caption->SetTitle (title);
        if (m_hwnd != nullptr)
        {
            InvalidateRect (m_hwnd, nullptr, FALSE);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCaptionIcon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetCaptionIcon (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_caption)
    {
        m_caption->SetAppIcon (std::move (bgraPremul), widthPx, heightPx);
        if (m_hwnd != nullptr)
        {
            InvalidateRect (m_hwnd, nullptr, FALSE);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptionHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHwndSource::CaptionHeightPx () const
{
    if (!m_caption)
    {
        return 0;
    }
    return m_caption->PreferredHeightPx (m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderCaption
//
//  Adopt-mode paint hook: the consumer calls this from its own paint
//  pump (the host owns no swap chain in adopt mode) so the caption
//  composites on top of the consumer's content.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::RenderCaption (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_caption)
    {
        m_caption->Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutCaptionForClient
//
//  Adopt-mode layout hook: the consumer calls this on WM_SIZE with the
//  client rect in physical pixels.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::LayoutCaptionForClient (const RECT & clientPx)
{
    RECT  clientDip = clientPx;



    DXUI_ASSERT_UI_THREAD();

    if (!m_caption)
    {
        return;
    }

    clientDip.right  = MulDiv (clientPx.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    clientDip.bottom = MulDiv (clientPx.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    LayoutCaption (clientDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteCaptionNcMouse
//
//  Drives caption system-button hover / press / release from the raw NC
//  mouse messages (adopt mode). Returns true only when the event lands
//  on a caption button -- the consumer keeps everything else. Mirrors
//  the full-ownership HandleNcMouse button path without touching
//  DefWindowProc (the consumer owns the default handling).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHwndSource::RouteCaptionNcMouse (UINT msg, WPARAM wp, LPARAM lp)
{
    POINT           ptScreen = {};
    POINT           ptClient = {};
    IDxuiControl  * control  = nullptr;
    DxuiMouseEvent  ev       = {};



    (void) wp;

    if (m_caption == nullptr || m_hwnd == nullptr)
    {
        return false;
    }

    if (msg == WM_NCMOUSELEAVE)
    {
        if (m_lastHoveredNcControl != nullptr)
        {
            ev.kind = DxuiMouseEventKind::Leave;
            m_lastHoveredNcControl->OnMouse (ev);
            m_lastHoveredNcControl = nullptr;
            InvalidateRect (m_hwnd, nullptr, FALSE);
            return true;
        }
        return false;
    }

    ptScreen.x = GET_X_LPARAM (lp);
    ptScreen.y = GET_Y_LPARAM (lp);
    ptClient   = ptScreen;
    if (!ScreenToClient (m_hwnd, &ptClient))
    {
        return false;
    }

    ptClient.x = MulDiv (ptClient.x, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    ptClient.y = MulDiv (ptClient.y, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    control    = FindNcSystemControlAt (ptClient);

    if (control == nullptr)
    {
        // Left the buttons: drop any latched hover, but don't consume --
        // the consumer still needs caption drag / menu dismiss.
        if (m_lastHoveredNcControl != nullptr && msg == WM_NCMOUSEMOVE)
        {
            ev.kind = DxuiMouseEventKind::Leave;
            m_lastHoveredNcControl->OnMouse (ev);
            m_lastHoveredNcControl = nullptr;
            InvalidateRect (m_hwnd, nullptr, FALSE);
        }
        return false;
    }

    switch (msg)
    {
        case WM_NCMOUSEMOVE:
            if (m_lastHoveredNcControl != nullptr && m_lastHoveredNcControl != control)
            {
                ev.kind = DxuiMouseEventKind::Leave;
                m_lastHoveredNcControl->OnMouse (ev);
            }
            ev.kind                = DxuiMouseEventKind::Move;
            m_lastHoveredNcControl = control;
            break;

        case WM_NCLBUTTONDOWN:
            ev.kind   = DxuiMouseEventKind::Down;
            ev.button = DxuiMouseButton::Left;
            break;

        case WM_NCLBUTTONUP:
            ev.kind   = DxuiMouseEventKind::Up;
            ev.button = DxuiMouseButton::Left;
            break;

        default:
            return false;
    }

    ev.positionDip = ptClient;
    control->OnMouse (ev);
    InvalidateRect (m_hwnd, nullptr, FALSE);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHitForTest
//
//  Public test seam — runs the same classifier the WM_NCHITTEST
//  path uses, against the cached client bounds (test mode) or the
//  live client rect (full mode).
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiHwndSource::ClassifyHitForTest (POINT clientDip) const
{
    RECT  rcClient  = {};



    DXUI_ASSERT_UI_THREAD();

    if (m_synthetic)
    {
        rcClient.right  = m_params.initialSizeDip.cx;
        rcClient.bottom = m_params.initialSizeDip.cy;
        return ClassifyHitInternal (clientDip, rcClient);
    }

    if (m_hwnd == nullptr || !GetClientRect (m_hwnd, &rcClient))
    {
        return DxuiHitTestKind::None;
    }

    rcClient.right  = MulDiv (rcClient.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    rcClient.bottom = MulDiv (rcClient.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    return ClassifyHitInternal (clientDip, rcClient);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindNcSystemControlAt
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiHwndSource::FindNcSystemControlAt (POINT clientDip) const
{
    size_t           n       = 0;
    size_t           i       = 0;
    IDxuiControl   * child   = nullptr;
    IDxuiControl   * found   = nullptr;
    RECT             rc      = {};
    DxuiHitTestKind  kind    = DxuiHitTestKind::None;



    // Host-owned caption first — its system buttons are the topmost NC
    // controls and are not part of the consumer's root tree.
    if (m_caption != nullptr)
    {
        rc = m_caption->Bounds();
        if (clientDip.x >= rc.left && clientDip.x < rc.right &&
            clientDip.y >= rc.top  && clientDip.y < rc.bottom)
        {
            found = FindNcSystemControlInTree (m_caption.get(), clientDip);
            if (found != nullptr)
            {
                return found;
            }
        }
    }

    if (RootPanel() == nullptr)
    {
        return nullptr;
    }

    n = RootPanel()->ChildCount();
    for (i = n; i > 0; --i)
    {
        child = RootPanel()->Child (i - 1);
        if (child == nullptr || !child->Visible())
        {
            continue;
        }

        rc = child->Bounds();
        if (clientDip.x < rc.left || clientDip.x >= rc.right ||
            clientDip.y < rc.top  || clientDip.y >= rc.bottom)
        {
            continue;
        }

        found = FindNcSystemControlInTree (child, clientDip);
        if (found != nullptr)
        {
            return found;
        }

        kind = child->ClassifyHit (clientDip);
        if (kind == DxuiHitTestKind::MinButton ||
            kind == DxuiHitTestKind::MaxButton ||
            kind == DxuiHitTestKind::CloseButton)
        {
            return child;
        }
    }

    return nullptr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  NotifySystemButtonsMaximized
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::NotifySystemButtonsMaximized (bool maximized)
{
    NotifySystemButtonsMaximizedInTree (RootPanel(), maximized);
    if (m_caption)
    {
        m_caption->SetMaximized (maximized);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHitInternal
//
//  Shared classification path. Resize edges win first, then the tree
//  walk hands off to whichever control owns the point.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiHwndSource::ClassifyHitInternal (POINT clientDip, RECT clientBoundsDip) const
{
    DxuiHitTestKind  edge       = DxuiHitTestKind::None;
    int              borderPx   = 0;
    size_t           n          = 0;
    size_t           i          = 0;
    IDxuiControl *   child      = nullptr;
    RECT             rc         = {};
    DxuiHitTestKind  kind       = DxuiHitTestKind::None;



    borderPx = (int) m_params.resizeBorderDip;
    if (borderPx < s_kMinResizeBorderPx)
    {
        borderPx = s_kMinResizeBorderPx;
    }

    if (m_params.resizable)
    {
        edge = ClassifyResizeEdge (clientDip, clientBoundsDip, borderPx);
        if (edge != DxuiHitTestKind::None)
        {
            return edge;
        }
    }

    // Host-owned caption wins over the consumer's root content (it is
    // drawn on top of the top strip). Buttons / caption / nothing.
    if (m_caption != nullptr)
    {
        RECT  crc = m_caption->Bounds();

        if (clientDip.x >= crc.left && clientDip.x < crc.right &&
            clientDip.y >= crc.top  && clientDip.y < crc.bottom)
        {
            DxuiHitTestKind  ck = m_caption->ClassifyHit (clientDip);

            if (ck != DxuiHitTestKind::None && ck != DxuiHitTestKind::Client)
            {
                return ck;
            }
        }
    }

    if (RootPanel() == nullptr)
    {
        return DxuiHitTestKind::Client;
    }

    // Reverse z-order so visually-topmost children win.
    n = RootPanel()->ChildCount();
    for (i = n; i > 0; --i)
    {
        child = RootPanel()->Child (i - 1);
        if (child == nullptr || !child->Visible())
        {
            continue;
        }

        rc = child->Bounds();
        if (clientDip.x < rc.left || clientDip.x >= rc.right ||
            clientDip.y < rc.top  || clientDip.y >= rc.bottom)
        {
            continue;
        }

        kind = child->ClassifyHit (clientDip);
        if (kind != DxuiHitTestKind::None && kind != DxuiHitTestKind::Client)
        {
            return kind;
        }
    }

    return DxuiHitTestKind::Client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyResizeEdge
//
//  Eight-edge resize hit test inside the client rect. Corner inset
//  takes priority over edge inset so the diagonal cursors fire in
//  the right places.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiHwndSource::ClassifyResizeEdge (POINT clientDip,
                                                    RECT  clientBoundsDip,
                                                    int   resizeBorderPx)
{
    bool  left   = (clientDip.x >= clientBoundsDip.left   && clientDip.x <  clientBoundsDip.left   + resizeBorderPx);
    bool  right  = (clientDip.x <  clientBoundsDip.right  && clientDip.x >= clientBoundsDip.right  - resizeBorderPx);
    bool  top    = (clientDip.y >= clientBoundsDip.top    && clientDip.y <  clientBoundsDip.top    + resizeBorderPx);
    bool  bottom = (clientDip.y <  clientBoundsDip.bottom && clientDip.y >= clientBoundsDip.bottom - resizeBorderPx);



    if (top    && left)  { return DxuiHitTestKind::ResizeCornerTL; }
    if (top    && right) { return DxuiHitTestKind::ResizeCornerTR; }
    if (bottom && left)  { return DxuiHitTestKind::ResizeCornerBL; }
    if (bottom && right) { return DxuiHitTestKind::ResizeCornerBR; }
    if (top)             { return DxuiHitTestKind::ResizeEdgeTop;    }
    if (bottom)          { return DxuiHitTestKind::ResizeEdgeBottom; }
    if (left)            { return DxuiHitTestKind::ResizeEdgeLeft;   }
    if (right)           { return DxuiHitTestKind::ResizeEdgeRight;  }

    return DxuiHitTestKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KindToHt
//
//  Maps the abstract DxuiHitTestKind to the matching Win32 HT* code.
//  MaxButton → HTMAXBUTTON is what unlocks the Win11 snap-layouts
//  hover popover.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHwndSource::KindToHt (DxuiHitTestKind kind)
{
    switch (kind)
    {
        case DxuiHitTestKind::None:             return HTNOWHERE;
        case DxuiHitTestKind::Client:           return HTCLIENT;
        case DxuiHitTestKind::Caption:          return HTCAPTION;
        case DxuiHitTestKind::MinButton:        return HTMINBUTTON;
        case DxuiHitTestKind::MaxButton:        return HTMAXBUTTON;
        case DxuiHitTestKind::CloseButton:      return HTCLOSE;
        case DxuiHitTestKind::ResizeEdgeLeft:   return HTLEFT;
        case DxuiHitTestKind::ResizeEdgeRight:  return HTRIGHT;
        case DxuiHitTestKind::ResizeEdgeTop:    return HTTOP;
        case DxuiHitTestKind::ResizeEdgeBottom: return HTBOTTOM;
        case DxuiHitTestKind::ResizeCornerTL:   return HTTOPLEFT;
        case DxuiHitTestKind::ResizeCornerTR:   return HTTOPRIGHT;
        case DxuiHitTestKind::ResizeCornerBL:   return HTBOTTOMLEFT;
        case DxuiHitTestKind::ResizeCornerBR:   return HTBOTTOMRIGHT;
    }
    return HTCLIENT;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AcquirePopup
//
//  LIFO pool reuse. The first AcquirePopup() call seeds the pool to
//  its initial size (3). When every popup is currently checked out
//  a new DxuiPopupHost is created on demand; otherwise the last
//  idle instance is returned. Production-mode pool entries share
//  the host's D3D device; if the host has no device (synthetic /
//  test mode) the popup is wired up via InitializeForTest() so the
//  dismiss / chain / placement state machinery still works
//  headlessly.
//
//  Ownership model: m_popupPool owns every DxuiPopupHost the host
//  has ever created (idle + active). m_popupActive is a parallel
//  vector of raw pointers naming the currently checked-out ones.
//  Pool seeding sweeps debug counters (hits / misses) per FR-055.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupHost * DxuiHwndSource::AcquirePopup ()
{
    DxuiPopupHost  *  popup  = nullptr;


    DXUI_ASSERT_UI_THREAD();

    // Seed the pool on the very first acquire.
    if (m_popupPool.empty())
    {
        for (size_t i = 0; i < s_kPopupPoolInitialSize; ++i)
        {
            std::unique_ptr<DxuiPopupHost>  fresh = std::make_unique<DxuiPopupHost>();

            InitializePooledPopup (fresh.get());
            m_popupPool.push_back (std::move (fresh));
        }
    }

    // Walk the pool in LIFO order looking for an idle instance.
    for (size_t i = m_popupPool.size(); i-- > 0; )
    {
        bool  active = false;

        for (DxuiPopupHost * a : m_popupActive)
        {
            if (a == m_popupPool[i].get()) { active = true; break; }
        }

        if (!active)
        {
            popup = m_popupPool[i].get();
            m_popupActive.push_back (popup);
#ifdef _DEBUG
            m_popupHits++;
#endif
            return popup;
        }
    }

    // Every pool entry is in use — grow on demand.
    {
        std::unique_ptr<DxuiPopupHost>  fresh = std::make_unique<DxuiPopupHost>();

        InitializePooledPopup (fresh.get());
        popup = fresh.get();
        m_popupPool.push_back (std::move (fresh));
        m_popupActive.push_back (popup);
#ifdef _DEBUG
        m_popupMisses++;
#endif
    }

    return popup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleasePopup
//
//  Returns a previously-acquired popup to the idle set. The popup
//  remains owned by m_popupPool and is reused on the next Acquire.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::ReleasePopup (DxuiPopupHost * popup)
{
    DXUI_ASSERT_UI_THREAD();

    if (popup == nullptr)
    {
        return;
    }

    if (popup->IsOpen())
    {
        popup->Close (0);
    }

    for (size_t i = 0; i < m_popupActive.size(); ++i)
    {
        if (m_popupActive[i] == popup)
        {
            m_popupActive.erase (m_popupActive.begin() + (ptrdiff_t) i);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPopupRenderDevice
//
//  Supplies the device/context an adopt-mode host's popup pool should
//  use (the consumer's renderer owns them). Stored non-owning.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::SetPopupRenderDevice (ID3D11Device         * device,
                                           ID3D11DeviceContext  * context)
{
    DXUI_ASSERT_UI_THREAD();

    m_popupDevice  = device;
    m_popupContext = context;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializePooledPopup
//
//  Initializes one pooled popup for production rendering when a real
//  device/context/HINSTANCE are available, else falls back to headless
//  test mode. Full-ownership hosts use m_device/m_context; adopt-mode
//  hosts use the device set via SetPopupRenderDevice.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHwndSource::InitializePooledPopup (DxuiPopupHost * popup)
{
    ID3D11Device         *  device   = m_device  ? m_device.Get()  : m_popupDevice.Get();
    ID3D11DeviceContext  *  context  = m_context ? m_context.Get() : m_popupContext.Get();
    HRESULT                 hr       = S_OK;


    DXUI_ASSERT_UI_THREAD();

    if (device != nullptr && context != nullptr && m_hInstance != nullptr)
    {
        hr = popup->Initialize (m_hInstance, device, context);
        if (FAILED (hr))
        {
            popup->InitializeForTest();
        }
    }
    else
    {
        popup->InitializeForTest();
    }
}
