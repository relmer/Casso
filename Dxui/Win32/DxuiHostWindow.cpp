#include "Pch.h"

#include "DxuiHostWindow.h"
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

    serial = s_classSerial.fetch_add (1);
    (void) swprintf_s (classNameBuf, L"DxuiHostWindow_%u_%p", serial, (void *) this);
    m_className = classNameBuf;

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

    // Initial size is given in DIPs; convert at the current desktop DPI
    // before CreateWindowEx. WM_DPICHANGED will rescale later if the
    // window straddles or moves between monitors.
    dpiAtCreate = GetDpiForSystem();
    if (dpiAtCreate == 0)
    {
        dpiAtCreate = s_kDefaultDpi;
    }
    m_scaler.SetDpi (dpiAtCreate);

    widthPx  = MulDiv (params.initialSizeDip.cx, (int) dpiAtCreate, (int) s_kDefaultDpi);
    heightPx = MulDiv (params.initialSizeDip.cy, (int) dpiAtCreate, (int) s_kDefaultDpi);

    m_hwnd = CreateWindowExW (exStyle,
                              m_className.c_str(),
                              params.title.c_str(),
                              style,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
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

    hr = CreateDeviceAndSwapChain();
    CHRA (hr);

    hr = CreateRenderResources();
    CHRA (hr);

    ApplyDwmConfiguration();

    m_focusManager.Attach (m_root.get());

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

    if (m_hwnd != nullptr && m_ownsHwnd)
    {
        DestroyWindow (m_hwnd);
    }
    m_hwnd     = nullptr;
    m_ownsHwnd = false;

    if (m_classRegistered && m_hInstance != nullptr)
    {
        UnregisterClassW (m_className.c_str(), m_hInstance);
        m_classRegistered = false;
    }
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
            return HandleNcHitTest (lp);

        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCMOUSEMOVE:
        case WM_NCMOUSELEAVE:
            return HandleNcMouse (msg, wp, lp);

        case WM_DPICHANGED:
            HandleDpiChanged (wp, lp);
            return 0;

        case WM_DPICHANGED_BEFOREPARENT:
            // TODO(Phase 9): forward to every active pooled DxuiPopupHost
            // so popups straddling a monitor boundary re-DPI correctly.
            // Popup hosting is not implemented in Phase 7; this message
            // currently has nothing to forward.
            return 0;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            HandleThemeChange();
            break;

        case WM_SIZE:
            HandleSize (lp);
            break;

        case WM_DESTROY:
            // Bookkeeping only; the actual teardown happens in
            // Destroy()/~DxuiHostWindow().
            break;
    }

    return DefWindowProc (m_hwnd, msg, wp, lp);
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
        return DefWindowProc (m_hwnd, WM_NCCALCSIZE, wp, lp);
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
    defResult   = DefWindowProc (m_hwnd, WM_NCCALCSIZE, wp, lp);
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



    ptScreen.x = GET_X_LPARAM (lp);
    ptScreen.y = GET_Y_LPARAM (lp);
    ptClient   = ptScreen;
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
//  Stub for Phase 7 — the framework owns these messages so the OS
//  doesn't draw the default chrome button overlays, but actual mouse
//  routing into the panel tree wires up in Phase 8 once a consumer
//  is driving the host. Returning 0 suppresses DefWindowProc's
//  built-in chrome behavior.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiHostWindow::HandleNcMouse (UINT msg, WPARAM wp, LPARAM lp)
{
    (void) msg;
    (void) wp;
    (void) lp;

    return 0;
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
        rcClient.right  = MulDiv (rcClient.right,  (int) s_kDefaultDpi, (int) newDpi);
        rcClient.bottom = MulDiv (rcClient.bottom, (int) s_kDefaultDpi, (int) newDpi);
        m_root->Layout (rcClient, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSize
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHostWindow::HandleSize (LPARAM lp)
{
    RECT  rcClient  = {};
    UINT  widthPx   = LOWORD (lp);
    UINT  heightPx  = HIWORD (lp);



    (void) widthPx;
    (void) heightPx;

    if (m_swapChain)
    {
        m_rtv.Reset();
        (void) m_swapChain->ResizeBuffers (0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    }

    if (m_root != nullptr && m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        rcClient.right  = MulDiv (rcClient.right,  (int) s_kDefaultDpi, (int) m_scaler.Dpi());
        rcClient.bottom = MulDiv (rcClient.bottom, (int) s_kDefaultDpi, (int) m_scaler.Dpi());
        m_root->Layout (rcClient, m_scaler);
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
