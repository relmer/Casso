#include "Pch.h"

#include "DxuiPopupHost.h"
#include "Theme/DxuiDwm.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "user32.lib")



namespace
{
    constexpr UINT     s_kDefaultDpi          = 96;
    constexpr LONG     s_kShadowInsetPx       = 1;

    std::atomic<uint32_t>  s_classSerial { 0 };


    //
    //  Returns the monitor work-area rect (excludes the taskbar) for
    //  the monitor that contains the supplied rect. Falls back to a
    //  giant synthetic work area if the multi-monitor lookup fails so
    //  callers always get a usable rect.
    //
    RECT  WorkAreaForRect (const RECT & rectScreenPx)
    {
        RECT          work     = { 0, 0, 1920, 1080 };
        HMONITOR      monitor  = nullptr;
        MONITORINFO   info     = {};


        monitor = MonitorFromRect (&rectScreenPx, MONITOR_DEFAULTTONEAREST);
        if (monitor == nullptr)
        {
            return work;
        }

        info.cbSize = sizeof (info);
        if (GetMonitorInfoW (monitor, &info))
        {
            work = info.rcWork;
        }
        return work;
    }


    //
    //  Position a rect of the supplied size on the chosen edge of an
    //  anchor without any work-area clamping. Helper for placement.
    //
    RECT  PlaceOnEdge (const RECT          & anchor,
                       DxuiPopupPlacement    edge,
                       SIZE                  popupSizePx)
    {
        RECT  out = {};


        switch (edge)
        {
            case DxuiPopupPlacement::Below:
                out.left   = anchor.left;
                out.top    = anchor.bottom;
                break;

            case DxuiPopupPlacement::Above:
                out.left   = anchor.left;
                out.top    = anchor.top - popupSizePx.cy;
                break;

            case DxuiPopupPlacement::Right:
                out.left   = anchor.right;
                out.top    = anchor.top;
                break;

            case DxuiPopupPlacement::Left:
                out.left   = anchor.left - popupSizePx.cx;
                out.top    = anchor.top;
                break;

            case DxuiPopupPlacement::AtCursor:
                // Anchor's (left, top) is treated as the cursor point.
                out.left   = anchor.left;
                out.top    = anchor.top;
                break;
        }

        out.right  = out.left + popupSizePx.cx;
        out.bottom = out.top  + popupSizePx.cy;
        return out;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHost
//
//  Default constructor — does not allocate OS resources. Caller must
//  follow with Initialize() (production) or InitializeForTest()
//  before invoking Show().
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupHost::DxuiPopupHost()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiPopupHost
//
//  Idempotent teardown via Shutdown().
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupHost::~DxuiPopupHost()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::Initialize (HINSTANCE hInstance, ID3D11Device * device)
{
    HRESULT  hr  = S_OK;


    DXUI_ASSERT_UI_THREAD();

    CBRA (!m_initialized);
    CBRAEx (hInstance != nullptr, E_INVALIDARG);
    CBRAEx (device    != nullptr, E_INVALIDARG);

    m_hInstance   = hInstance;
    m_device      = device;
    m_initialized = true;
    m_testMode    = false;

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeForTest
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::InitializeForTest ()
{
    DXUI_ASSERT_UI_THREAD();

    m_initialized = true;
    m_testMode    = true;
    m_hInstance   = nullptr;
    m_device      = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::Shutdown ()
{
    DXUI_ASSERT_UI_THREAD();

    if (m_open)
    {
        Close (0);
    }

    DestroyHwndAndComposition();

    if (m_classRegistered && m_hInstance != nullptr)
    {
        UnregisterClassW (m_className.c_str(), m_hInstance);
        m_classRegistered = false;
    }

    m_className.clear();
    m_initialized = false;
    m_testMode    = false;
    m_hInstance   = nullptr;
    m_device      = nullptr;
    m_parent      = nullptr;
    m_activeChild = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Computes the final placement rect, ensures the popup HWND /
//  composition swap chain exist (production mode only), repositions
//  + shows the HWND, and arms the completion promise.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::Show (ShowParams params)
{
    HRESULT  hr             = S_OK;
    RECT     workArea       = {};
    RECT     placedRect     = {};
    SIZE     sizePx         = {};
    UINT     dpi            = s_kDefaultDpi;


    DXUI_ASSERT_UI_THREAD();

    CBRA (m_initialized);

    // Already open? Re-show with new params (close prior completion).
    if (m_open)
    {
        Close (0);
    }

    m_params = std::move (params);

    // Convert DIPs -> Px at the owner's DPI (fall back to 96 if no
    // owner HWND was supplied — typical in tests).
    if (m_params.ownerHwnd != nullptr)
    {
        dpi = GetDpiForWindow (m_params.ownerHwnd);
        if (dpi == 0) { dpi = s_kDefaultDpi; }
    }
    sizePx.cx = MulDiv (m_params.sizeDip.cx, (int) dpi, (int) s_kDefaultDpi);
    sizePx.cy = MulDiv (m_params.sizeDip.cy, (int) dpi, (int) s_kDefaultDpi);

    workArea   = WorkAreaForRect (m_params.anchorRectScreen);
    placedRect = ComputePlacementForTest (m_params.anchorRectScreen,
                                          workArea,
                                          m_params.placement,
                                          sizePx,
                                          m_params.flipIfOffscreen);
    m_placedRectScreenPx = placedRect;

    // Reset the completion promise for this Show cycle.
    m_completionPromise  = std::promise<int>();
    m_completionPending  = true;
    m_open               = true;
    m_resultCode         = 0;

    if (m_testMode)
    {
        // Test mode: no HWND, no swap chain. State above is the
        // entire deliverable; tests inspect Params(),
        // PlacedRectScreenPx(), and Completion() directly.
        goto Error;
    }

    hr = EnsureWindowClass();
    CHRA (hr);

    hr = CreateHwndAndComposition (placedRect);
    CHRA (hr);

    if (m_params.shadow)
    {
        DxuiDwm::ExtendFrameIntoClientArea (m_hwnd, (int) s_kShadowInsetPx);
    }

    // Show without activating (WS_EX_NOACTIVATE) so the owner keeps
    // keyboard focus / caption activation state.
    ShowWindow (m_hwnd, SW_SHOWNOACTIVATE);

    // Click-outside dismiss: capture so off-popup clicks route to
    // our WndProc as WM_CAPTURECHANGED / WM_LBUTTONDOWN-with-NCHITTEST.
    if (m_params.dismiss == DxuiPopupDismiss::OnClickOutside ||
        m_params.dismiss == DxuiPopupDismiss::OnClickAnywhere)
    {
        SetCapture (m_hwnd);
    }

Error:

    if (FAILED (hr))
    {
        // Show failed — fulfil completion immediately so callers
        // awaiting the future don't deadlock.
        if (m_completionPending)
        {
            m_completionPromise.set_value (-1);
            m_completionPending = false;
        }
        m_open = false;
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::Close (int resultCode)
{
    DXUI_ASSERT_UI_THREAD();

    if (!m_open)
    {
        return;
    }

    m_resultCode = resultCode;
    m_open       = false;

    // Detach from chain bookkeeping.
    if (m_parent != nullptr && m_parent->m_activeChild == this)
    {
        m_parent->m_activeChild = nullptr;
    }
    m_parent      = nullptr;
    m_activeChild = nullptr;

    if (!m_testMode && m_hwnd != nullptr)
    {
        if (GetCapture() == m_hwnd)
        {
            ReleaseCapture();
        }
        ShowWindow (m_hwnd, SW_HIDE);
    }

    // Drop the content panel reference (releases caller-owned data).
    m_params.content.reset();

    if (m_completionPending)
    {
        m_completionPromise.set_value (resultCode);
        m_completionPending = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Completion
//
////////////////////////////////////////////////////////////////////////////////

std::future<int> DxuiPopupHost::Completion ()
{
    DXUI_ASSERT_UI_THREAD();
    return m_completionPromise.get_future();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetParentPopup
//
//  Establishes a cascading-submenu link. A click inside any popup
//  in the chain (this popup, its parent, grandparent, …) counts as
//  inside-chain rather than outside, so the chain does not dismiss
//  when the user mouses up into an ancestor.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::SetParentPopup (DxuiPopupHost * parent)
{
    DXUI_ASSERT_UI_THREAD();

    // Detach from prior parent's active-child slot.
    if (m_parent != nullptr && m_parent->m_activeChild == this)
    {
        m_parent->m_activeChild = nullptr;
    }

    m_parent = parent;

    if (m_parent != nullptr)
    {
        m_parent->m_activeChild = this;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDpiChanged
//
//  Called by DxuiHostWindow's WM_DPICHANGED_BEFOREPARENT handler so
//  cross-monitor popups re-DPI before the owner repaints.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::HandleDpiChanged (UINT newDpi)
{
    DXUI_ASSERT_UI_THREAD();

    // Currently the popup host stashes only the size-in-DIPs; the
    // next Show() recomputes pixel sizes against the new DPI. If a
    // popup is mid-flight when DPI changes we'd want to re-place it
    // here — left as a follow-up since the dismiss-on-WM_MOVE
    // policy normally closes the popup before this fires.
    (void) newDpi;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputePlacementForTest
//
//  Pure-function placement helper. Tries the preferred edge first;
//  if flipIfOffscreen is set and the placed rect would extend past
//  the work area on that edge, flips to the opposite edge and
//  re-checks. After edge selection the rect is finally clamped to
//  the work area on the orthogonal axis so it never paints off
//  the monitor.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiPopupHost::ComputePlacementForTest (
    RECT                anchorScreenPx,
    RECT                monitorWorkAreaPx,
    DxuiPopupPlacement  preferred,
    SIZE                popupSizePx,
    bool                flipIfOffscreen)
{
    DxuiPopupPlacement  chosen     = preferred;
    RECT                placed     = {};
    bool                outBelow   = false;
    bool                outAbove   = false;
    bool                outRight   = false;
    bool                outLeft    = false;


    placed = PlaceOnEdge (anchorScreenPx, preferred, popupSizePx);

    if (flipIfOffscreen)
    {
        switch (preferred)
        {
            case DxuiPopupPlacement::Below:
                outBelow = placed.bottom > monitorWorkAreaPx.bottom;
                if (outBelow)
                {
                    chosen = DxuiPopupPlacement::Above;
                    placed = PlaceOnEdge (anchorScreenPx, chosen, popupSizePx);
                }
                break;

            case DxuiPopupPlacement::Above:
                outAbove = placed.top < monitorWorkAreaPx.top;
                if (outAbove)
                {
                    chosen = DxuiPopupPlacement::Below;
                    placed = PlaceOnEdge (anchorScreenPx, chosen, popupSizePx);
                }
                break;

            case DxuiPopupPlacement::Right:
                outRight = placed.right > monitorWorkAreaPx.right;
                if (outRight)
                {
                    chosen = DxuiPopupPlacement::Left;
                    placed = PlaceOnEdge (anchorScreenPx, chosen, popupSizePx);
                }
                break;

            case DxuiPopupPlacement::Left:
                outLeft = placed.left < monitorWorkAreaPx.left;
                if (outLeft)
                {
                    chosen = DxuiPopupPlacement::Right;
                    placed = PlaceOnEdge (anchorScreenPx, chosen, popupSizePx);
                }
                break;

            case DxuiPopupPlacement::AtCursor:
                // Cursor-placed popups don't flip — they slide.
                break;
        }
    }

    // Orthogonal clamp to keep the popup inside the work area on the
    // axis we did NOT just choose.
    if (chosen == DxuiPopupPlacement::Below ||
        chosen == DxuiPopupPlacement::Above ||
        chosen == DxuiPopupPlacement::AtCursor)
    {
        if (placed.right > monitorWorkAreaPx.right)
        {
            LONG  shift = placed.right - monitorWorkAreaPx.right;
            placed.left  -= shift;
            placed.right -= shift;
        }
        if (placed.left < monitorWorkAreaPx.left)
        {
            LONG  shift = monitorWorkAreaPx.left - placed.left;
            placed.left  += shift;
            placed.right += shift;
        }
    }

    if (chosen == DxuiPopupPlacement::Right ||
        chosen == DxuiPopupPlacement::Left ||
        chosen == DxuiPopupPlacement::AtCursor)
    {
        if (placed.bottom > monitorWorkAreaPx.bottom)
        {
            LONG  shift = placed.bottom - monitorWorkAreaPx.bottom;
            placed.top    -= shift;
            placed.bottom -= shift;
        }
        if (placed.top < monitorWorkAreaPx.top)
        {
            LONG  shift = monitorWorkAreaPx.top - placed.top;
            placed.top    += shift;
            placed.bottom += shift;
        }
    }

    return placed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldDismissForTest
//
//  Per-policy classification: should an event of type `reason`
//  dismiss a popup configured with `policy`?
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss        policy,
                                          DxuiPopupDismissReason  reason)
{
    if (reason == DxuiPopupDismissReason::Manual)
    {
        return true;
    }

    switch (policy)
    {
        case DxuiPopupDismiss::OnClickOutside:
            // Clicks inside the popup or anywhere in its owner chain
            // are NOT dismiss-events — that's the entire point of the
            // chain (cascading submenus).
            return reason == DxuiPopupDismissReason::ClickOutsideChain;

        case DxuiPopupDismiss::OnClickAnywhere:
            // Any click dismisses; pointer-leave does not.
            return reason == DxuiPopupDismissReason::ClickInsidePopup        ||
                   reason == DxuiPopupDismissReason::ClickInsideChainAncestor ||
                   reason == DxuiPopupDismissReason::ClickOutsideChain;

        case DxuiPopupDismiss::OnPointerLeave:
            return reason == DxuiPopupDismissReason::PointerLeftPopup;

        case DxuiPopupDismiss::Manual:
            // Only the explicit Close() call dismisses; reached above.
            return false;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProcThunk
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DxuiPopupHost::s_WndProcThunk (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DxuiPopupHost *  self  = nullptr;


    if (msg == WM_NCCREATE)
    {
        CREATESTRUCTW *  cs = reinterpret_cast<CREATESTRUCTW *> (lp);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) cs->lpCreateParams);
        return DefWindowProcW (hwnd, msg, wp, lp);
    }

    self = reinterpret_cast<DxuiPopupHost *> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
    if (self == nullptr)
    {
        return DefWindowProcW (hwnd, msg, wp, lp);
    }

    return self->WndProc (msg, wp, lp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiPopupHost::WndProc (UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CAPTURECHANGED:
        case WM_ACTIVATEAPP:
            if (m_open && m_params.dismiss != DxuiPopupDismiss::Manual)
            {
                Close (0);
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Click landed on us while we had capture — work out if
            // it was inside our client rect. If not, dismiss per
            // policy.
            if (m_open && GetCapture() == m_hwnd)
            {
                POINT  pt = { GET_X_LPARAM (lp), GET_Y_LPARAM (lp) };
                RECT   rc = {};
                GetClientRect (m_hwnd, &rc);

                if (!PtInRect (&rc, pt))
                {
                    if (m_params.dismiss == DxuiPopupDismiss::OnClickOutside ||
                        m_params.dismiss == DxuiPopupDismiss::OnClickAnywhere)
                    {
                        Close (0);
                        return 0;
                    }
                }
            }
            break;

        case WM_MOUSELEAVE:
            if (m_open && m_params.dismiss == DxuiPopupDismiss::OnPointerLeave)
            {
                Close (0);
                return 0;
            }
            break;
    }

    return DefWindowProcW (m_hwnd, msg, wp, lp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureWindowClass
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::EnsureWindowClass ()
{
    HRESULT      hr               = S_OK;
    WNDCLASSEXW  wc               = {};
    wchar_t      classNameBuf[64] = {};
    uint32_t     serial           = 0;


    if (m_classRegistered)
    {
        goto Error;
    }

    serial = s_classSerial.fetch_add (1);
    (void) swprintf_s (classNameBuf, L"DxuiPopupHost_%u_%p", serial, (void *) this);
    m_className = classNameBuf;

    wc.cbSize        = sizeof (wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = &DxuiPopupHost::s_WndProcThunk;
    wc.hInstance     = m_hInstance;
    wc.hCursor       = LoadCursor (nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = m_className.c_str();

    CWRA (RegisterClassExW (&wc));
    m_classRegistered = true;

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateHwndAndComposition
//
//  Creates the WS_POPUP HWND (adds WS_EX_TRANSPARENT|WS_EX_LAYERED
//  for pass-through input popups) and a composition swap chain
//  bound to a DirectComposition visual rooted on the HWND. WS_POPUP
//  HWNDs need DComp for proper z-order, transparency, and shadow
//  (CreateSwapChainForHwnd would paint at the wrong z-layer and
//  block the DWM shadow).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::CreateHwndAndComposition (const RECT & placedRectScreenPx)
{
    HRESULT                hr            = S_OK;
    DWORD                  style         = WS_POPUP;
    DWORD                  exStyle       = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    int                    widthPx       = placedRectScreenPx.right  - placedRectScreenPx.left;
    int                    heightPx      = placedRectScreenPx.bottom - placedRectScreenPx.top;
    ComPtr<IDXGIDevice>    dxgiDevice;
    ComPtr<IDXGIAdapter>   dxgiAdapter;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1  scd           = {};


    if (m_params.input == DxuiPopupInput::PassThrough)
    {
        exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    }

    if (m_hwnd == nullptr)
    {
        m_hwnd = CreateWindowExW (exStyle,
                                  m_className.c_str(),
                                  L"",
                                  style,
                                  placedRectScreenPx.left,
                                  placedRectScreenPx.top,
                                  widthPx,
                                  heightPx,
                                  m_params.ownerHwnd,
                                  nullptr,
                                  m_hInstance,
                                  this);
        CWRA (m_hwnd);
    }
    else
    {
        // Reuse: re-position and (if needed) flip ex-style bits.
        SetWindowLongPtrW (m_hwnd, GWL_EXSTYLE, (LONG_PTR) exStyle);
        SetWindowPos (m_hwnd,
                      HWND_TOPMOST,
                      placedRectScreenPx.left,
                      placedRectScreenPx.top,
                      widthPx,
                      heightPx,
                      SWP_NOACTIVATE | SWP_NOZORDER);
    }

    if (m_swapChain == nullptr)
    {
        hr = m_device->QueryInterface (IID_PPV_ARGS (dxgiDevice.GetAddressOf()));
        CHRA (hr);

        hr = dxgiDevice->GetAdapter (dxgiAdapter.GetAddressOf());
        CHRA (hr);

        hr = dxgiAdapter->GetParent (IID_PPV_ARGS (dxgiFactory.GetAddressOf()));
        CHRA (hr);

        scd.Width            = (UINT) (widthPx  > 0 ? widthPx  : 1);
        scd.Height           = (UINT) (heightPx > 0 ? heightPx : 1);
        scd.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        scd.SampleDesc.Count = 1;
        scd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount      = 2;
        scd.Scaling          = DXGI_SCALING_STRETCH;
        scd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;

        hr = dxgiFactory->CreateSwapChainForComposition (m_device,
                                                         &scd,
                                                         nullptr,
                                                         m_swapChain.GetAddressOf());
        CHRA (hr);

        hr = DCompositionCreateDevice (dxgiDevice.Get(),
                                       IID_PPV_ARGS (m_compDevice.GetAddressOf()));
        CHRA (hr);

        hr = m_compDevice->CreateTargetForHwnd (m_hwnd,
                                                TRUE,
                                                m_compTarget.GetAddressOf());
        CHRA (hr);

        hr = m_compDevice->CreateVisual (m_compVisual.GetAddressOf());
        CHRA (hr);

        hr = m_compVisual->SetContent (m_swapChain.Get());
        CHRA (hr);

        hr = m_compTarget->SetRoot (m_compVisual.Get());
        CHRA (hr);

        hr = m_compDevice->Commit();
        CHRA (hr);
    }

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DestroyHwndAndComposition
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::DestroyHwndAndComposition ()
{
    m_compVisual.Reset();
    m_compTarget.Reset();
    m_compDevice.Reset();
    m_swapChain.Reset();

    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
        m_hwnd = nullptr;
    }
}
