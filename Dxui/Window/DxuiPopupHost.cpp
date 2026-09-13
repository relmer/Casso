#include "Pch.h"

#include "DxuiPopupHost.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiTheme.h"
#include "Render/DxuiShadow.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "user32.lib")



static constexpr UINT     s_kDefaultDpi          = 96;

std::atomic<uint32_t>  s_classSerial { 0 };





////////////////////////////////////////////////////////////////////////////////
//
//  GetWorkAreaForRect
//
//
//   Returns the monitor work-area rect (excludes the taskbar) for
//   the monitor that contains the supplied rect. Falls back to a
//   giant synthetic work area if the multi-monitor lookup fails so
//   callers always get a usable rect.
//
//
////////////////////////////////////////////////////////////////////////////////

RECT  DxuiPopupHost::GetWorkAreaForRect (const RECT & rectScreenPx)
{
    RECT          work     = { 0, 0, 1920, 1080 };
    HMONITOR      monitor  = nullptr;
    MONITORINFO   info     = {};



    // Either lookup failing leaves the synthetic 1920x1080 fallback, so a
    // popup always gets a usable rect to clamp against.
    monitor = MonitorFromRect (&rectScreenPx, MONITOR_DEFAULTTONEAREST);

    if (monitor != nullptr)
    {
        info.cbSize = sizeof (info);

        if (GetMonitorInfoW (monitor, &info))
        {
            work = info.rcWork;
        }
    }

    return work;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHost::PlaceOnEdge
//
//  Position a rect of the supplied size on the chosen edge of an
//  anchor without any work-area clamping. Helper for placement.
//
////////////////////////////////////////////////////////////////////////////////

RECT  DxuiPopupHost::PlaceOnEdge (const RECT          & anchor,
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
//  Brings up the popup's own render facades on the OWNER's D3D device.
//
//  Sharing the device rather than creating one is the point: a popup is a
//  separate top-level HWND with its own swap chain, but a second D3D device
//  would mean a second driver context and no ability to share resources with
//  the window it belongs to.
//
//  The two facades take different inputs for a real reason -- DxuiPainter
//  needs the immediate CONTEXT, while DxuiTextRenderer derives its own D2D
//  device from the D3D DEVICE and manages its own context.
//
//  Neither is bound to a back buffer here; that happens in
//  CreateBackBufferRtv, once the popup HWND and its swap chain exist. So an
//  initialized host is ready to serve popups but owns no window yet.
//
//  InitializeForTest exists alongside this so popup PLACEMENT and dismissal
//  logic can be tested with no device, no window, and no GPU -- which is why
//  the render-ready flag is tracked separately from initialized.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::Initialize (HINSTANCE              hInstance,
                                  ID3D11Device         * device,
                                  ID3D11DeviceContext  * context)
{
    HRESULT  hr  = S_OK;



    DXUI_ASSERT_UI_THREAD();

    CBRA (!m_initialized);
    CBRAEx (hInstance != nullptr, E_INVALIDARG);
    CBRAEx (device    != nullptr, E_INVALIDARG);
    CBRAEx (context   != nullptr, E_INVALIDARG);

    m_hInstance   = hInstance;
    m_device      = device;
    m_context     = context;

    // Per-popup render facades on the shared device. DxuiPainter needs
    // the immediate context; DxuiTextRenderer derives its own D2D device
    // from the D3D device. Bound to the back buffer in CreateBackBufferRtv.
    hr = m_painter.Initialize (device, context);
    CHRA (hr);

    hr = m_textRenderer.Initialize (device);
    CHRA (hr);

    m_renderReady = true;
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

void DxuiPopupHost::InitializeForTest()
{
    DXUI_ASSERT_UI_THREAD();

    m_initialized = true;
    m_testMode    = true;
    m_renderReady = false;
    m_hInstance   = nullptr;
    m_device      = nullptr;
    m_context     = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
//  Closes any open popup, destroys the window and composition, and unregisters
//  the window class.
//
//  Closing FIRST matters: Close resolves the completion promise, and skipping
//  it would leave a caller waiting on a popup whose window is already gone.
//
//  The window class is unregistered because each host registers its own
//  uniquely-named class. Leaving it behind leaks a class registration per
//  host, which matters for a type that is created and destroyed with the
//  panels that use it rather than once per process.
//
//  Every pointer is cleared -- device, context, parent, active child -- since
//  all of them are BORROWED and the lenders can outlive or predecease this
//  object in either order.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::Shutdown()
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
    m_renderReady = false;
    m_hInstance   = nullptr;
    m_device      = nullptr;
    m_context     = nullptr;
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
    HRESULT        hr               = S_OK;
    RECT           workArea         = {};
    RECT           placedRect       = {};
    RECT           windowRect       = {};
    SIZE           sizePx           = {};
    UINT           dpi              = s_kDefaultDpi;



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

    workArea   = GetWorkAreaForRect (m_params.anchorRectScreen);
    placedRect = ComputePlacementForTest (m_params.anchorRectScreen,
                                          workArea,
                                          m_params.placement,
                                          sizePx,
                                          m_params.flipIfOffscreen);
    m_placedRectScreenPx = placedRect;

    // The window is the card plus a margin that holds the drawn shadow. The
    // placed rect stays the card, so a consumer measuring itself against it
    // and every placement decision above are unchanged.
    m_dpi                = dpi;
    m_shadowMarginPx     = m_params.shadow ? MulDiv ((int) DxuiShadow::kMarginDip, (int) dpi, (int) s_kDefaultDpi) : 0;
    windowRect.left      = placedRect.left   - m_shadowMarginPx;
    windowRect.top       = placedRect.top    - m_shadowMarginPx;
    windowRect.right     = placedRect.right  + m_shadowMarginPx;
    windowRect.bottom    = placedRect.bottom + m_shadowMarginPx;
    m_windowRectScreenPx = windowRect;

    // Reset the completion promise for this Show cycle.
    m_completionPromise  = std::promise<int>();
    m_completionPending  = true;
    m_open               = true;
    m_resultCode         = 0;

    // Test mode: no HWND, no swap chain. The state set above is the entire
    // deliverable; tests inspect GetParams(), GetPlacedRectScreenPx(), and
    // GetCompletion() directly.
    BAIL_OUT_IF (m_testMode, S_OK);

    hr = EnsureWindowClass();
    CHRA (hr);

    hr = CreateHwndAndComposition (windowRect);
    CHRA (hr);

    // No DwmExtendFrameIntoClientArea. A glass frame on a surface composited
    // with premultiplied alpha paints DWM's frame fill into the transparent
    // margin, which is exactly where the drawn shadow has to show through.
    //
    // DWM rounds the window only when the host is NOT drawing the rounded
    // card itself. With a shadow margin the window's corners are transparent
    // surround, and DWM's corner clip would cut the shadow off there.
    DxuiDwm::ApplyRoundedCorners (m_hwnd, m_shadowMarginPx == 0);

    // PAINT BEFORE SHOWING. These popups come from a pool and are handed
    // back most-recently-used first, so the window about to be shown is
    // usually the one the PREVIOUS menu was drawn into, and its swap chain
    // still holds that menu's pixels. Showing first and painting after put
    // the old menu on screen for a frame, which read as the new menu
    // flickering through the old one's rows on the way from one title to the
    // next. The HWND and swap chain are live from CreateHwndAndComposition;
    // visibility was never what they needed.
    RenderNow();

    // Arm the open animation and put the window into its FIRST frame before
    // it is ever shown. Starting the reveal after ShowWindow let the popup
    // appear full size for one frame and then jump back to the start of the
    // animation, which is exactly the blink the animation exists to avoid.
    if (m_params.revealMs > 0)
    {
        BeginReveal (m_params.revealMs, m_params.revealFade);
        ApplyReveal (0.0f);
    }

    // Show without activating (WS_EX_NOACTIVATE) so the owner keeps
    // keyboard focus / caption activation state.
    ShowWindow (m_hwnd, SW_SHOWNOACTIVATE);

    // Click-outside dismiss: capture so off-popup clicks route to
    // our WndProc as WM_CAPTURECHANGED / WM_LBUTTONDOWN-with-NCHITTEST.
    // Skipped when grabsCapture is false (owner-driven dismiss): the
    // popup then only receives events while the cursor is over it.
    if (m_params.grabsCapture &&
        (m_params.dismiss == DxuiPopupDismiss::OnClickOutside ||
         m_params.dismiss == DxuiPopupDismiss::OnClickAnywhere))
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
//  Hides the popup, unlinks it from the popup chain, resolves the completion
//  promise, and notifies the owner.
//
//  m_open is cleared FIRST, which makes this function re-entrant by
//  construction: the onClosed callback routinely calls ReleasePopup, which can
//  reach Close again, and the early exit turns that second entry into a no-op
//  instead of a recursion.
//
//  For the same reason onClosed is captured into a local before the content is
//  dropped -- the callback lives in the params being cleared, so calling it
//  through the member would invoke a destroyed std::function.
//
//  Chain bookkeeping is undone in both directions. A nested popup must clear
//  the PARENT's pointer to itself as well as its own, or the parent keeps
//  routing to a hidden child.
//
//  Capture is released before hiding, since a hidden window holding mouse
//  capture swallows input for the entire application.
//
//  The window is HIDDEN, not destroyed. These popups are pooled, so hiding is
//  what returns one to the pool with its swap chain intact and makes the next
//  Show cheap.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::Close (int resultCode)
{
    std::function<void()>  onClosed;



    DXUI_ASSERT_UI_THREAD();

    if (!m_open)
    {
        return;
    }

    m_resultCode = resultCode;
    m_open       = false;

    // These popups are pooled, so a reveal left running would be inherited by
    // whatever opens next in this window.
    m_revealing   = false;
    m_revealOut   = false;
    m_revealAlpha = 1.0f;

    if (m_compVisual)
    {
        m_compVisual->SetOffsetY (0.0f);

        if (m_compDevice)
        {
            m_compDevice->Commit();
        }
    }

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

    // Capture onClosed before dropping content so the owning widget
    // can clear its open/active state and return us to the pool. Close
    // early-exits on re-entry (m_open is already false), so a
    // ReleasePopup() from within onClosed does not recurse.
    onClosed = m_params.onClosed;
    m_params.content.reset();

    if (m_completionPending)
    {
        m_completionPromise.set_value (resultCode);
        m_completionPending = false;
    }

    if (onClosed)
    {
        onClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCompletion
//
////////////////////////////////////////////////////////////////////////////////

std::future<int> DxuiPopupHost::GetCompletion()
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
//  Called by DxuiHwndSource's WM_DPICHANGED_BEFOREPARENT handler so
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
    // An explicit Close() dismisses under every policy, including Manual --
    // that is what makes Manual "only Close() dismisses" rather than "nothing
    // dismisses".
    bool  dismisses = (reason == DxuiPopupDismissReason::Manual);



    switch (policy)
    {
        case DxuiPopupDismiss::OnClickOutside:
            // Clicks inside the popup or anywhere in its owner chain are NOT
            // dismiss-events -- that's the entire point of the chain
            // (cascading submenus).
            dismisses = dismisses
                        || reason == DxuiPopupDismissReason::ClickOutsideChain;
            break;

        case DxuiPopupDismiss::OnClickAnywhere:
            // Any click dismisses; pointer-leave does not.
            dismisses = dismisses
                        || reason == DxuiPopupDismissReason::ClickInsidePopup
                        || reason == DxuiPopupDismissReason::ClickInsideChainAncestor
                        || reason == DxuiPopupDismissReason::ClickOutsideChain;
            break;

        case DxuiPopupDismiss::OnPointerLeave:
            dismisses = dismisses
                        || reason == DxuiPopupDismissReason::PointerLeftPopup;
            break;

        case DxuiPopupDismiss::Manual:
            // Nothing to add: the Manual reason above is the only dismissal.
            break;
    }

    return dismisses;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProcThunk
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DxuiPopupHost::s_WndProcThunk (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DxuiPopupHost  * self   = nullptr;
    CREATESTRUCTW  * cs     = nullptr;
    LRESULT          result = 0;



    // WM_NCCREATE is where the instance pointer arrives, so it is handled
    // before (and never by) the instance WndProc. Messages that precede it,
    // or arrive after the pointer is cleared, also go straight to DefWindowProc.
    if (msg == WM_NCCREATE)
    {
        cs = reinterpret_cast<CREATESTRUCTW *> (lp);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) cs->lpCreateParams);
    }
    else
    {
        self = reinterpret_cast<DxuiPopupHost *> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
    }

    result = (self != nullptr) ? self->WndProc (msg, wp, lp)
                               : DefWindowProcW (hwnd, msg, wp, lp);

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetContentClientRect
//
//  The client rect less the shadow margin -- the card, in window-client
//  pixels. With no margin it is the whole client rect.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiPopupHost::GetContentClientRect() const
{
    RECT  rc = {};



    GetClientRect (m_hwnd, &rc);
    InflateRect   (&rc, -m_shadowMarginPx, -m_shadowMarginPx);

    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WndProc
//
//  The popup window's message handler: dismissal policy, hover routing, and
//  click classification.
//
//  Clicks are handled differently depending on whether the popup holds CAPTURE,
//  and both paths are needed. WITH capture the popup receives clicks anywhere
//  on screen, so it must classify inside versus outside and dismiss on the
//  latter. WITHOUT capture the OS only delivers clicks that land on the popup
//  itself, so they are inherently inside.
//
//  MA_NOACTIVATE is returned for WM_MOUSEACTIVATE so clicking the popup never
//  steals activation from its owner -- the window keeps its active caption and
//  its focus while the user drives the popup. The window is WS_EX_NOACTIVATE
//  as well; both are needed, since the style governs how the popup is shown
//  and this governs what a click does to it.
//
//  Dismissal is policy-driven rather than fixed, so a tooltip closing on
//  pointer-leave, a menu closing on click-outside, and a dialog that closes
//  only on command all share one implementation.
//
//  Losing capture or app activation dismisses under every non-Manual policy.
//  A popup that outlives the app losing focus reappears floating over another
//  application's window.
//
//  Hover is routed on every move without filtering. The consumer compares
//  against its own highlight and marks dirty only on a change, so the cost
//  stays low where the knowledge is.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiPopupHost::WndProc (UINT msg, WPARAM wp, LPARAM lp)
{
    POINT    pt          = { GET_X_LPARAM (lp), GET_Y_LPARAM (lp) };
    RECT     rc          = {};
    bool     haveCapture = false;
    bool     inside      = false;
    bool     claimed     = false;
    LRESULT  result      = 0;



    switch (msg)
    {
        case WM_CAPTURECHANGED:
        case WM_ACTIVATEAPP:
            if (m_open && m_params.dismiss != DxuiPopupDismiss::Manual)
            {
                Close (0);
            }

            claimed = true;
            break;

        case WM_MOUSEMOVE:
            // Hover routing (popup-local pixels). The consumer compares
            // against its current highlight and MarkDirty()s on change,
            // so this stays cheap despite firing on every move.
            if (m_open && m_params.onMoveInside)
            {
                rc = GetContentClientRect();

                if (PtInRect (&rc, pt))
                {
                    m_params.onMoveInside (POINT { pt.x - m_shadowMarginPx, pt.y - m_shadowMarginPx });
                }
            }

            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // A click reached us. With capture we must classify inside vs
            // outside (outside dismisses per policy). Without capture the
            // OS only delivers clicks that land ON the popup, so they are
            // inherently inside -- fire onClickInside directly.
            if (m_open)
            {
                haveCapture = (GetCapture() == m_hwnd);

                rc     = GetContentClientRect();
                inside = (PtInRect (&rc, pt) != FALSE);

                if (inside && msg == WM_LBUTTONDOWN && m_params.onClickInside)
                {
                    m_params.onClickInside (POINT { pt.x - m_shadowMarginPx, pt.y - m_shadowMarginPx });
                    claimed = true;
                }
                else if (!inside && haveCapture &&
                         (m_params.dismiss == DxuiPopupDismiss::OnClickOutside ||
                          m_params.dismiss == DxuiPopupDismiss::OnClickAnywhere))
                {
                    Close (0);
                    claimed = true;
                }
            }

            break;

        case WM_MOUSEACTIVATE:
            // Never steal activation from the owner when clicked (we are
            // also WS_EX_NOACTIVATE); the owner keeps focus / caption
            // activation while the popup is interacted with.
            result  = MA_NOACTIVATE;
            claimed = true;
            break;

        case WM_NCHITTEST:
            // The shadow margin is drawn, not hit: a press there belongs to
            // whatever is underneath, the same as a press anywhere else
            // outside the card. `pt` is in SCREEN coordinates for this message.
            if (m_shadowMarginPx > 0)
            {
                POINT  client = pt;

                ScreenToClient (m_hwnd, &client);
                rc = GetContentClientRect();

                if (!PtInRect (&rc, client))
                {
                    result  = HTTRANSPARENT;
                    claimed = true;
                }
            }

            break;

        case WM_MOUSELEAVE:
            if (m_open && m_params.dismiss == DxuiPopupDismiss::OnPointerLeave)
            {
                Close (0);
                claimed = true;
            }

            break;

        default:
            break;
    }

    if (!claimed)
    {
        result = DefWindowProcW (m_hwnd, msg, wp, lp);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureWindowClass
//
//  Registers this host's window class on first use.
//
//  The class name is made unique PER HOST, from an atomic serial plus the
//  object address, rather than being a single shared class. Hosts are created
//  and destroyed alongside the panels that use them, and a shared class would
//  have to be reference-counted across those lifetimes -- with a use-after-
//  unregister waiting for whoever got the counting wrong. A private class is
//  registered and unregistered by exactly one owner.
//
//  A null background brush is deliberate: the popup paints every pixel through
//  its own swap chain, and letting the class erase the background first causes
//  a visible flash on show.
//
//  CS_DBLCLKS is set so double-clicks reach the popup as double-clicks rather
//  than as two unrelated presses.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::EnsureWindowClass()
{
    HRESULT      hr               = S_OK;
    WNDCLASSEXW  wc               = {};
    wchar_t      classNameBuf[64] = {};
    uint32_t     serial           = 0;
    ATOM         classAtom        = 0;



    BAIL_OUT_IF (m_classRegistered, S_OK);

    serial = s_classSerial.fetch_add (1);
    (void) swprintf_s (classNameBuf, L"DxuiPopupHost_%u_%p", serial, (void *) this);
    m_className = classNameBuf;

    wc.cbSize        = sizeof (wc);
    // No CS_DROPSHADOW. DWM does not decorate a window whose content comes
    // from a DirectComposition target -- a plain popup with these same styles
    // gets the class shadow and this one does not -- so RenderNow draws the
    // shadow into the popup's own surface instead, the way a WinUI flyout
    // composites its ThemeShadow rather than asking the window manager.
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = &DxuiPopupHost::s_WndProcThunk;
    wc.hInstance     = m_hInstance;
    wc.hCursor       = LoadCursor (nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = m_className.c_str();

    classAtom = RegisterClassExW (&wc);
    CWRA (classAtom);

    m_classRegistered = true;

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateHwndAndComposition
//
//  Creates the WS_POPUP HWND (adds WS_EX_TRANSPARENT for pass-through
//  input popups) and a composition swap chain
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



    // WS_EX_TRANSPARENT alone is what makes the pointer pass through: it
    // takes the window out of hit testing, which is the whole requirement
    // for a tooltip. WS_EX_LAYERED used to ride along with it and bought
    // nothing -- the content is composited by DirectComposition over an
    // opaque clear, so there is no per-pixel alpha for layering to carry --
    // while costing the drop shadow, because a layered window does not get
    // CS_DROPSHADOW. Tooltips were the only popups that took it, and the
    // only ones without a shadow.
    if (m_params.input == DxuiPopupInput::PassThrough)
    {
        exStyle |= WS_EX_TRANSPARENT;
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

    // Bind (first create) or resize (pool reuse at a new size) the
    // back-buffer RTV + D2D text target to the current popup size.
    if (m_rtv == nullptr)
    {
        hr = CreateBackBufferRtv();
        CHRA (hr);
    }
    else
    {
        hr = ResizeSwapChain (widthPx, heightPx);
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

void DxuiPopupHost::DestroyHwndAndComposition()
{
    ReleaseBackBufferRtv();
    m_compVisual.Reset();
    m_compTarget.Reset();
    m_compDevice.Reset();
    m_swapChain.Reset();
    m_backBufferSizePx = {};

    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
        m_hwnd = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferRtv
//
//  Binds the popup swap chain's back buffer as both the D3D render
//  target (for DxuiPainter) and the D2D text target (for
//  DxuiTextRenderer). D2D is bound at the default DPI so D2D logical
//  units equal physical pixels — the popup renders and hit-tests in
//  physical pixels throughout (the consumer scales DIPs -> px itself).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::CreateBackBufferRtv()
{
    HRESULT                   hr          = S_OK;
    DXGI_SWAP_CHAIN_DESC1     scd         = {};
    D3D11_VIEWPORT            vp          = {};
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

    m_backBufferSizePx.cx = (LONG) scd.Width;
    m_backBufferSizePx.cy = (LONG) scd.Height;

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (float) scd.Width;
    vp.Height   = (float) scd.Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    hr = backBuffer.As (&backSurface);
    CHRA (hr);

    hr = m_textRenderer.BindBackBuffer (backSurface.Get(), s_kDefaultDpi, s_kDefaultDpi);
    CHRA (hr);

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseBackBufferRtv
//
//  Drops the back-buffer RTV + D2D target. Must run before
//  ResizeBuffers so the back-buffer reference count reaches zero.
//  Idempotent.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::ReleaseBackBufferRtv()
{
    if (m_renderReady)
    {
        m_textRenderer.UnbindBackBuffer();
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
//  ResizeSwapChain
//
//  Resizes the popup swap chain to a new pixel size on pool reuse.
//  Releases the RTV + D2D target first (strict order so ResizeBuffers
//  has no outstanding back-buffer references), then re-binds.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::ResizeSwapChain (int widthPx, int heightPx)
{
    HRESULT  hr  = S_OK;



    CBRA (m_swapChain);

    if (widthPx  < 1) { widthPx  = 1; }
    if (heightPx < 1) { heightPx = 1; }

    BAIL_OUT_IF (m_backBufferSizePx.cx == (LONG) widthPx &&
                 m_backBufferSizePx.cy == (LONG) heightPx &&
                 m_rtv != nullptr, S_OK);

    ReleaseBackBufferRtv();

    hr = m_swapChain->ResizeBuffers (0, (UINT) widthPx, (UINT) heightPx, DXGI_FORMAT_UNKNOWN, 0);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

Error:

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintShadowAndCard
//
//  The shadow, then the rounded card over it, drawn into the popup's own
//  surface. The shadow itself is DxuiShadow's; the flyouts that draw inside
//  the window share it, so every floating surface falls off the same way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::PaintShadowAndCard()
{
    float  scale  = (float) m_dpi / (float) s_kDefaultDpi;
    float  margin = (float) m_shadowMarginPx;
    float  cardW  = (float) m_backBufferSizePx.cx - margin * 2.0f;
    float  cardH  = (float) m_backBufferSizePx.cy - margin * 2.0f;
    float  radius = DxuiTheme::kOverlayCornerRadiusDip * scale;



    DxuiShadow::Paint (m_painter, margin, margin, cardW, cardH, radius, scale);
    m_painter.FillRoundedRect (margin, margin, cardW, cardH, radius, m_params.backgroundArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderNow
//
//  Clears the back buffer to the opaque background, invokes the
//  content render hook (popup-local pixels), and presents. The
//  premultiplied-alpha composition surface MUST be cleared fully
//  opaque or owner content shows through. Painter (D3D fills) flushes
//  before the text renderer (D2D glyphs) so foreground composites on
//  top. No-op outside production / closed / not ready.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::RenderNow()
{
    HRESULT   hr           = S_OK;
    uint32_t  argb         = 0u;
    float     clear[4]     = {};
    bool      painterBegun = false;
    bool      textBegun    = false;



    DXUI_ASSERT_UI_THREAD();

    // Nothing to draw into, or nothing that wants drawing. Both flags below
    // are still false here, so the Error: cleanup is a no-op.
    BAIL_OUT_IF (m_testMode || !m_open || !m_renderReady || !m_swapChain || m_rtv == nullptr, S_OK);

    argb     = (m_shadowMarginPx > 0) ? 0u : m_params.backgroundArgb;
    clear[0] = (float) ((argb >> 16) & 0xFFu) / 255.0f;
    clear[1] = (float) ((argb >>  8) & 0xFFu) / 255.0f;
    clear[2] = (float) ((argb      ) & 0xFFu) / 255.0f;
    clear[3] = (float) ((argb >> 24) & 0xFFu) / 255.0f;

    m_context->OMSetRenderTargets    (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clear);

    if (m_params.renderContent)
    {
        hr = m_painter.Begin ((int) m_backBufferSizePx.cx, (int) m_backBufferSizePx.cy);
        CHRA (hr);
        painterBegun = true;
        m_painter.SetGlobalAlpha (m_revealAlpha);

        hr = m_textRenderer.BeginDraw();
        CHRA (hr);
        textBegun = true;
        m_textRenderer.SetGlobalAlpha (m_revealAlpha);

        // Shadow and card first, at the window origin, then the content hook
        // shifted onto the card. Both go through the same global alpha, so a
        // fade takes the card and its shadow with it rather than leaving an
        // opaque rectangle behind the fading text.
        if (m_shadowMarginPx > 0)
        {
            PaintShadowAndCard();
            m_painter.SetOrigin      ((float) m_shadowMarginPx, (float) m_shadowMarginPx);
            m_textRenderer.SetOrigin ((float) m_shadowMarginPx, (float) m_shadowMarginPx);
        }

        m_params.renderContent (m_painter, m_textRenderer);

        m_painter.SetOrigin      (0.0f, 0.0f);
        m_textRenderer.SetOrigin (0.0f, 0.0f);

        hr = m_painter.End (m_rtv.Get());
        painterBegun = false;
        CHRA (hr);

        hr = m_textRenderer.EndDraw();
        textBegun = false;
        CHRA (hr);
    }

    hr = m_swapChain->Present (0, 0);
    CHRA (hr);

Error:

    if (textBegun)
    {
        (void) m_textRenderer.EndDraw();
    }

    if (painterBegun)
    {
        (void) m_painter.End (m_rtv.Get());
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginReveal
//
//  Starts the open animation. A slide leaves the content alone and shrinks
//  the WINDOW to nothing, so the first advance uncovers the first sliver; a
//  fade leaves the window alone and takes the content to transparent.
//
//  Whether the reveal grows down or up is decided here, once, by comparing
//  the placed rect against the anchor it was placed from: a popup that had to
//  flip above its anchor pins its BOTTOM edge, because one unfolding downward
//  from a flipped position would crawl away from the title that opened it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::BeginReveal (int durationMs, bool fade)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_testMode || m_hwnd == nullptr || durationMs <= 0)
    {
        return;
    }

    m_revealing        = true;
    m_revealFade       = fade;
    m_revealDurationMs = durationMs;
    m_revealStartMs    = (int64_t) GetTickCount64();
    m_revealUpward     = m_placedRectScreenPx.top < m_params.anchorRectScreen.top;
    m_revealOut        = false;
    m_revealAlpha      = fade ? 0.0f : 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginFadeOut
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::BeginFadeOut (int durationMs)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_testMode || m_hwnd == nullptr || durationMs <= 0)
    {
        return;
    }

    m_revealing        = true;
    m_revealFade       = true;
    m_revealOut        = true;
    m_revealDurationMs = durationMs;
    m_revealStartMs    = (int64_t) GetTickCount64();
    m_revealAlpha      = 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyReveal
//
//  One frame of the reveal at progress `t`, 0 to 1.
//
//  The slide is the WinUI one: the LAST row is on screen from the first
//  frame and the list slides DOWN out of the anchor, uncovering earlier rows
//  above it until the whole menu stands. Growing the window downward over
//  top-anchored content does the opposite -- first row first -- which is what
//  this replaces.
//
//  Two things move together to get it. The window grows downward from the
//  anchor, and the composition visual is offset UP by exactly the part not
//  yet uncovered, so the bottom of the fully-rendered menu is what shows
//  through the short window. A popup that flipped ABOVE its anchor mirrors
//  it: bottom edge pinned, no offset, so the list slides up instead.
//
//  Eased rather than linear. 150 ms of linear travel over ~9 frames reads as
//  stepping; easing out puts most of the motion in the first frames, where
//  the eye reads it as one movement.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::ApplyReveal (float t)
{
    float  eased  = 0.0f;
    int    fullW  = 0;
    int    fullH  = 0;
    int    shownH = 0;
    int    top    = 0;
    float  offset = 0.0f;



    DXUI_ASSERT_UI_THREAD();

    if (m_hwnd == nullptr)
    {
        return;
    }

    t     = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;
    eased = 1.0f - ((1.0f - t) * (1.0f - t));

    if (m_revealFade)
    {
        m_revealAlpha = m_revealOut ? (1.0f - eased) : eased;
        RenderNow();
        return;
    }

    fullW  = m_windowRectScreenPx.right  - m_windowRectScreenPx.left;
    fullH  = m_windowRectScreenPx.bottom - m_windowRectScreenPx.top;
    shownH = (int) ((float) fullH * eased);

    if (shownH < 1)
    {
        shownH = 1;
    }

    if (m_revealUpward)
    {
        top    = m_windowRectScreenPx.bottom - shownH;
        offset = 0.0f;
    }
    else
    {
        top    = m_windowRectScreenPx.top;
        offset = -(float) (fullH - shownH);
    }

    SetWindowPos (m_hwnd, nullptr,
                  m_windowRectScreenPx.left, top, fullW, shownH,
                  SWP_NOZORDER | SWP_NOACTIVATE);

    if (m_compVisual)
    {
        m_compVisual->SetOffsetY (offset);

        if (m_compDevice)
        {
            m_compDevice->Commit();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceReveal
//
//  One frame of the open animation, returning true while more are wanted.
//  The caller drives this from its frame loop: the pointer is not moving
//  during an open, so nothing else would wake that loop.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupHost::AdvanceReveal (int64_t nowMs)
{
    float  t = 0.0f;



    DXUI_ASSERT_UI_THREAD();

    if (!m_revealing)
    {
        return false;
    }

    t = (float) (nowMs - m_revealStartMs) / (float) m_revealDurationMs;

    if (t >= 1.0f || m_hwnd == nullptr)
    {
        t           = 1.0f;
        m_revealing = false;
    }

    ApplyReveal (t);

    return m_revealing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDirty
//
//  Public re-render trigger. Synchronous + UI-thread-only.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupHost::MarkDirty()
{
    DXUI_ASSERT_UI_THREAD();
    RenderNow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureText
//
//  Forwards to the popup's own text renderer. Only the DWrite factory
//  is required (created in Initialize), so this is valid for a pooled
//  popup before Show() has built the swap chain. Fails gracefully in
//  test mode where no factory exists.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::MeasureText (const wchar_t  * text,
                                    float            fontSizeDip,
                                    const wchar_t  * fontFamily,
                                    float          & outWidthDip,
                                    float          & outHeightDip)
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();

    hr = m_textRenderer.MeasureString (text, fontSizeDip, fontFamily, outWidthDip, outHeightDip);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureTextWrapped
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPopupHost::MeasureTextWrapped (const wchar_t  * text,
                                           float            fontSizeDip,
                                           const wchar_t  * fontFamily,
                                           float            maxWidthDip,
                                           float          & outWidthDip,
                                           float          & outHeightDip)
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();

    hr = m_textRenderer.MeasureStringWrapped (text, fontSizeDip, fontFamily, maxWidthDip,
                                              outWidthDip, outHeightDip);
    CHR (hr);

Error:
    return hr;
}

