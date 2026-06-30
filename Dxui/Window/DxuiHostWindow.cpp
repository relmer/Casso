#include "Pch.h"

#include "DxuiHostWindow.h"
#include "DxuiCaptionBar.h"
#include "DxuiPopupHost.h"
#include "DxuiSystemButton.h"
#include "IDxuiHostClient.h"
#include "Theme/DxuiDwm.h"
#include "Theme/IDxuiTheme.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")



namespace
{
    constexpr int    s_kMinResizeBorderPx     = 4;
    constexpr UINT   s_kDefaultDpi            = 96;
    constexpr LONG   s_kExtendFrameInsetPx    = 1;

    // Distinct per-instance window class names — every Create()
    // generates a fresh class so multiple host windows in one
    // process don't share registration state.
    std::atomic<uint32_t>  s_classSerial { 0 };


    void NotifySystemButtonsMaximizedInTree (IDxuiControl * control, bool maximized)
    {
        size_t              n      = 0;
        size_t              i      = 0;
        IDxuiControl      * child  = nullptr;
        DxuiSystemButton  * button = nullptr;



        if (control == nullptr)
        {
            return;
        }

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
    }


    IDxuiControl *  FindNcSystemControlInTree (IDxuiControl * control, POINT clientDip)
    {
        size_t           n     = 0;
        size_t           i     = 0;
        IDxuiControl   * child = nullptr;
        IDxuiControl   * found = nullptr;
        RECT             rc    = {};
        DxuiHitTestKind  kind  = DxuiHitTestKind::None;



        if (control == nullptr)
        {
            return nullptr;
        }

        n = control->ChildCount();
        for (i = n; i > 0; --i)
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHostWindow
//
//  Default constructor — full-ownership mode. Caller must drive
//  Create() before the host is usable.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHostWindow::DxuiHostWindow()
{
    m_root = std::make_unique<DxuiPanel>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHostWindow (synthetic-root ctor)
//
//  Test-only mode. No HWND, no device, no swap chain. The caller
//  supplies a pre-built root panel; tests then drive
//  ClassifyHitForTest() directly.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHostWindow::DxuiHostWindow (RECT                       clientBoundsDip,
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
//  ~DxuiHostWindow
//
//  Idempotent teardown. Safe to call when the host was never Created.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHostWindow::~DxuiHostWindow()
{
    Destroy();
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

HRESULT DxuiHostWindow::Create (const CreateParams & params)
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
        (void) swprintf_s (classNameBuf, L"DxuiHostWindow_%u_%p", serial, (void *) this);
        m_className = classNameBuf;
    }

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &DxuiHostWindow::s_WndProcThunk;
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

    // Re-seed scaler from the per-window DPI now that the HWND knows
    // which monitor it landed on.
    m_scaler.SetDpi (GetDpiForWindow (m_hwnd));

    // Apply optional app icons. Win32 MessageBox dialogs + the
    // taskbar pick the icon up via WM_GETICON, NOT WNDCLASS::hIcon,
    // so the explicit WM_SETICON pair is required even when the
    // class was registered with icons.
    if (params.appIconBig != nullptr)
    {
        SendMessageW (m_hwnd, WM_SETICON, ICON_BIG, (LPARAM) params.appIconBig);
    }
    if (params.appIconSmall != nullptr)
    {
        SendMessageW (m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM) params.appIconSmall);
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

    m_focusManager.Attach (m_root.get());
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
//  Destroy
//
//  Idempotent. Releases render resources first (they reference the
//  swap chain), then drops the device / swap chain, destroys the
//  HWND if owned, and unregisters the per-instance class.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHostWindow::Destroy ()
{
    DXUI_ASSERT_UI_THREAD();

    ReleaseRenderResources();

    m_rtv.Reset();
    m_swapChain.Reset();
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

HRESULT DxuiHostWindow::CreateInAdoptMode (
    HWND                              existingHwnd,
    const CreateParams              & params,
    std::unique_ptr<DxuiHostWindow> & outHost)
{
    HRESULT                          hr   = S_OK;
    std::unique_ptr<DxuiHostWindow>  host;
    UINT                             dpi  = 0;



    DXUI_ASSERT_UI_THREAD();

    host = std::unique_ptr<DxuiHostWindow> (new DxuiHostWindow());
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

void DxuiHostWindow::SetHitTestDelegate (std::function<LRESULT (POINT)> delegate)
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

void DxuiHostWindow::SetBeforePresentHook (std::function<void()> hook)
{
    DXUI_ASSERT_UI_THREAD();

    m_beforePresentHook = std::move (hook);
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

bool DxuiHostWindow::HandleMessage (UINT msg, WPARAM wp, LPARAM lp, LRESULT & outResult)
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

void DxuiHostWindow::SetTheme (const IDxuiTheme * theme)
{
    DXUI_ASSERT_UI_THREAD();

    m_theme = theme;
    m_focusManager.SetTheme (theme);
    if (m_root != nullptr)
    {
        m_root->OnThemeChanged();
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

void DxuiHostWindow::SetClient (IDxuiHostClient * client)
{
    DXUI_ASSERT_UI_THREAD();

    m_client = client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDefaultProcForTest
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHostWindow::SetDefaultProcForTest (std::function<LRESULT (HWND, UINT, WPARAM, LPARAM)> defaultProc)
{
    DXUI_ASSERT_UI_THREAD();

    m_defaultProcForTest = std::move (defaultProc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTrackMouseEventForTest
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHostWindow::SetTrackMouseEventForTest (std::function<BOOL (TRACKMOUSEEVENT *)> trackMouseEvent)
{
    DXUI_ASSERT_UI_THREAD();

    m_trackMouseEventForTest = std::move (trackMouseEvent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetContentPanel
//
//  Replace the root panel with a caller-supplied tree. Lets a
//  consumer install a fully-assembled content panel (DxuiDialog,
//  SettingsWindow content, ...) as the host's paint / hit-test /
//  focus / accessibility root in one shot. The previous root is
//  destroyed. When the host already owns a real HWND, the new
//  panel's bounds are set from the current client rect so it lays
//  out immediately; in synthetic mode the panel inherits the
//  previous root's bounds.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHostWindow::SetContentPanel (std::unique_ptr<DxuiPanel> panel)
{
    RECT  bounds        = {};
    bool  liveLayout    = false;


    DXUI_ASSERT_UI_THREAD();
    assert (panel != nullptr && "DxuiHostWindow::SetContentPanel requires a non-null panel");

    if (panel == nullptr)
    {
        return;
    }

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
            bounds.left   = 0;
            bounds.top    = 0;
            bounds.right  = MulDiv (clientRectPx.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
            bounds.bottom = MulDiv (clientRectPx.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
            liveLayout    = true;
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
                rootBounds.top += m_caption->PreferredHeightDip();
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTimer
//
//  Thin convenience wrapper around `::SetTimer` so consumers don't
//  have to reach for the global symbol. WM_TIMER dispatches to
//  `IDxuiHostClient::OnTimer` (DxuiHostWindow's WndProc already
//  forwards the message). Returns true iff the timer was scheduled;
//  no-ops in release when the host has no HWND.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHostWindow::SetTimer (UINT_PTR timerId, UINT intervalMs)
{
    UINT_PTR  result = 0;


    DXUI_ASSERT_UI_THREAD();
    assert (m_hwnd != nullptr && "DxuiHostWindow::SetTimer requires an HWND");

    if (m_hwnd == nullptr)
    {
        return false;
    }

    result = ::SetTimer (m_hwnd, timerId, intervalMs, nullptr);

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

bool DxuiHostWindow::KillTimer (UINT_PTR timerId)
{
    BOOL  result = FALSE;


    DXUI_ASSERT_UI_THREAD();
    assert (m_hwnd != nullptr && "DxuiHostWindow::KillTimer requires an HWND");

    if (m_hwnd == nullptr)
    {
        return false;
    }

    result = ::KillTimer (m_hwnd, timerId);

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

HRESULT DxuiHostWindow::CreateDeviceAndSwapChain ()
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
    scd.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;

    hr = dxgiFactory->CreateSwapChainForHwnd (m_device.Get(),
                                              m_hwnd,
                                              &scd,
                                              nullptr,
                                              nullptr,
                                              m_swapChain.GetAddressOf());
    CHRA (hr);

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateRenderResources
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiHostWindow::CreateRenderResources ()
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

void DxuiHostWindow::ReleaseRenderResources ()
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

HRESULT DxuiHostWindow::CreateBackBufferRtv ()
{
    HRESULT                   hr           = S_OK;
    DXGI_SWAP_CHAIN_DESC1     scd          = {};
    D3D11_VIEWPORT            vp           = {};
    ComPtr<ID3D11Texture2D>   backBuffer;
    ComPtr<IDXGISurface>      backSurface;



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

    // Rebind the Direct2D target bitmap (used by DxuiTextRenderer)
    // to the new back-buffer surface. Skipped when the text
    // renderer hasn't been Initialized yet (e.g. mid-tear-down).
    if (m_textRenderer != nullptr)
    {
        hr = backBuffer.As (&backSurface);
        CHRA (hr);

        hr = m_textRenderer->BindBackBuffer (backSurface.Get(), m_scaler.Dpi(), m_scaler.Dpi());
        CHRA (hr);
    }

Error:

    return hr;
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

void DxuiHostWindow::ReleaseBackBufferRtv ()
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
//  PaintPump
//
//  WM_PAINT body for full-ownership mode. Binds the back-buffer RTV,
//  clears to the theme background, lets the registered before-present
//  hook composite its full-buffer content first (e.g. the Apple ][
//  framebuffer via D3DRenderer::UploadAndComposite), then walks the
//  root panel tree invoking IDxuiControl::Paint on every visible child
//  so the chrome composites on top, then presents the swap chain.
//
//  Bails cleanly if the painter / text renderer / RTV / swap chain
//  are missing — partially-initialized states still get a Present
//  attempt so DWM doesn't latch a stale frame.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHostWindow::PaintPump ()
{
    HRESULT                hr             = S_OK;
    DXGI_SWAP_CHAIN_DESC1  scd            = {};
    float                  clearColor[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
    uint32_t               bgArgb         = 0xFF000000u;
    bool                   painterBegun   = false;
    bool                   textBegun      = false;



    DXUI_ASSERT_UI_THREAD();

    if (!m_swapChain || !m_rtv || m_painter == nullptr || m_textRenderer == nullptr)
    {
        return;
    }

    hr = m_swapChain->GetDesc1 (&scd);
    CHRA (hr);

    // Theme background clear. Without a theme, the host still clears
    // to opaque black so a partially-themed startup frame doesn't
    // present garbage from the back buffer.
    if (m_theme != nullptr)
    {
        bgArgb = m_theme->Background();
    }
    clearColor[0] = (float) ((bgArgb >> 16) & 0xFFu) / 255.0f;
    clearColor[1] = (float) ((bgArgb >>  8) & 0xFFu) / 255.0f;
    clearColor[2] = (float) ((bgArgb      ) & 0xFFu) / 255.0f;
    clearColor[3] = (float) ((bgArgb >> 24) & 0xFFu) / 255.0f;

    m_context->OMSetRenderTargets    (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    // Composite the consumer's content (e.g. the Apple ][ framebuffer
    // via D3DRenderer::UploadAndComposite) into the back buffer FIRST.
    // The hook owns a full-buffer write (emulator frame plus black
    // letterbox bars); the panel-tree painter / text passes below are
    // purely additive (neither clears the RTV), so the chrome composites
    // on top of the hook's result -- matching the legacy emulator-frame
    // plus chrome-overlay order.
    if (m_beforePresentHook)
    {
        m_beforePresentHook();
    }

    // Walk the panel tree. Painter buffers geometry between Begin /
    // End; the text renderer composites Direct2D over the same back
    // buffer between BeginDraw / EndDraw. The D2D bitmap is bound
    // once per back-buffer lifetime by CreateBackBufferRtv.
    if (m_root != nullptr && m_theme != nullptr)
    {
        hr = m_painter->Begin ((int) scd.Width, (int) scd.Height);
        CHRA (hr);
        painterBegun = true;

        hr = m_textRenderer->BeginDraw();
        CHRA (hr);
        textBegun = true;

        m_root->Paint (*m_painter, *m_textRenderer, *m_theme);

        // Host-owned caption paints last so it overlays the top strip
        // (the before-present hook fills the whole back buffer, the
        // chrome bands paint over it, and the caption sits on top).
        if (m_caption)
        {
            m_caption->Paint (*m_painter, *m_textRenderer, *m_theme);
        }

        // Flush the painter (D3D control fills) FIRST, then the text
        // (D2D glyphs / labels) so the foreground composites on top of
        // the fills -- matching the proven UiShell::Render order.
        // Flushing text first lets the opaque fills paint over it, which
        // makes menu labels and min/max/close glyphs vanish.
        hr = m_painter->End (m_rtv.Get());
        painterBegun = false;
        CHRA (hr);

        hr = m_textRenderer->EndDraw();
        textBegun = false;
        CHRA (hr);
    }

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:

    // Make sure the painter / text renderer don't stay mid-frame
    // after an early CHRA bail-out; the next paint pass would
    // otherwise see them in an inconsistent state.
    if (textBegun)
    {
        (void) m_textRenderer->EndDraw();
    }
    if (painterBegun)
    {
        (void) m_painter->End (m_rtv.Get());
    }
    return;
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

void DxuiHostWindow::ApplyDwmConfiguration ()
{
    bool  wantMica = false;



    if (m_hwnd == nullptr)
    {
        return;
    }

    wantMica = (m_params.backdrop == DxuiHostWindowBackdrop::Mica);

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

LRESULT CALLBACK DxuiHostWindow::s_WndProcThunk (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DxuiHostWindow *  pThis = nullptr;
    CREATESTRUCTW  *  pcs   = nullptr;



    if (msg == WM_NCCREATE)
    {
        pcs   = reinterpret_cast<CREATESTRUCTW *> (lp);
        pThis = static_cast<DxuiHostWindow *> (pcs->lpCreateParams);
        SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (pThis));
        if (pThis != nullptr)
        {
            pThis->m_hwnd = hwnd;
        }
    }
    else
    {
        pThis = reinterpret_cast<DxuiHostWindow *> (GetWindowLongPtr (hwnd, GWLP_USERDATA));
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

LRESULT DxuiHostWindow::WndProc (UINT msg, WPARAM wp, LPARAM lp)
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
            // Destroy()/~DxuiHostWindow(). Notify the client so it
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

        case WM_TIMER:
            if (m_client != nullptr && m_client->OnTimer (static_cast<UINT_PTR> (wp)) == DxuiMessageResult::Handled)
            {
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

LRESULT DxuiHostWindow::DefaultProc (UINT msg, WPARAM wp, LPARAM lp)
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

BOOL DxuiHostWindow::TrackMouseEventHost (TRACKMOUSEEVENT * pEvent)
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

void DxuiHostWindow::TrackClientMouseLeave()
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

LRESULT DxuiHostWindow::HandleNcCalcSize (WPARAM wp, LPARAM lp)
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

LRESULT DxuiHostWindow::HandleNcHitTest (LPARAM lp)
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

LRESULT DxuiHostWindow::HandleNcMouse (UINT msg, WPARAM wp, LPARAM lp)
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

void DxuiHostWindow::DispatchNcUpToTrackedButton (LPARAM lp)
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

void DxuiHostWindow::HandleDpiChanged (WPARAM wp, LPARAM lp)
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

void DxuiHostWindow::HandleSize (WPARAM wp, LPARAM lp)
{
    HRESULT  hr        = S_OK;
    RECT     rcClient  = {};
    UINT     widthPx   = LOWORD (lp);
    UINT     heightPx  = HIWORD (lp);



    (void) widthPx;
    (void) heightPx;

    if (wp == SIZE_MAXIMIZED)
    {
        NotifySystemButtonsMaximized (true);
    }
    else if (wp == SIZE_RESTORED)
    {
        NotifySystemButtonsMaximized (false);
    }

    if (m_swapChain)
    {
        ReleaseBackBufferRtv();
        (void) m_swapChain->ResizeBuffers (0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

        hr = CreateBackBufferRtv();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_root != nullptr && m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        MaybeRelayoutRoot (rcClient);
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

void DxuiHostWindow::HandleThemeChange ()
{
    ApplyDwmConfiguration();

    if (m_root != nullptr)
    {
        m_root->OnThemeChanged();
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

void DxuiHostWindow::MaybeRelayoutRoot (const RECT & clientPx)
{
    RECT  boundsDip = clientPx;



    if (!m_ownsPaintPump || m_root == nullptr)
    {
        return;
    }

    boundsDip.right  = MulDiv (clientPx.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
    boundsDip.bottom = MulDiv (clientPx.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());

    {
        RECT  rootDip = boundsDip;

        if (m_params.insetRootBelowCaption && m_caption)
        {
            rootDip.top += m_caption->PreferredHeightDip();
        }

        m_root->Layout (rootDip, m_scaler);
    }

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

void DxuiHostWindow::BuildCaption ()
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

void DxuiHostWindow::LayoutCaption (const RECT & clientDip)
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

void DxuiHostWindow::SetTitle (const std::wstring & title)
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

void DxuiHostWindow::SetCaptionIcon (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx)
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

int DxuiHostWindow::CaptionHeightPx () const
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

void DxuiHostWindow::RenderCaption (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
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

void DxuiHostWindow::LayoutCaptionForClient (const RECT & clientPx)
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

bool DxuiHostWindow::RouteCaptionNcMouse (UINT msg, WPARAM wp, LPARAM lp)
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

DxuiHitTestKind DxuiHostWindow::ClassifyHitForTest (POINT clientDip) const
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

IDxuiControl * DxuiHostWindow::FindNcSystemControlAt (POINT clientDip) const
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

    if (m_root == nullptr)
    {
        return nullptr;
    }

    n = m_root->ChildCount();
    for (i = n; i > 0; --i)
    {
        child = m_root->Child (i - 1);
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

void DxuiHostWindow::NotifySystemButtonsMaximized (bool maximized)
{
    NotifySystemButtonsMaximizedInTree (m_root.get(), maximized);
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

DxuiHitTestKind DxuiHostWindow::ClassifyHitInternal (POINT clientDip, RECT clientBoundsDip) const
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

    if (m_root == nullptr)
    {
        return DxuiHitTestKind::Client;
    }

    // Reverse z-order so visually-topmost children win.
    n = m_root->ChildCount();
    for (i = n; i > 0; --i)
    {
        child = m_root->Child (i - 1);
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

DxuiHitTestKind DxuiHostWindow::ClassifyResizeEdge (POINT clientDip,
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

LRESULT DxuiHostWindow::KindToHt (DxuiHitTestKind kind)
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






namespace
{
    constexpr size_t  s_kPopupPoolInitialSize = 3;
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

DxuiPopupHost * DxuiHostWindow::AcquirePopup ()
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

void DxuiHostWindow::ReleasePopup (DxuiPopupHost * popup)
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

void DxuiHostWindow::SetPopupRenderDevice (ID3D11Device         * device,
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

void DxuiHostWindow::InitializePooledPopup (DxuiPopupHost * popup)
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
