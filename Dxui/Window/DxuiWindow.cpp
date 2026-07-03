#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Conjures the OS window (hidden) via the internal DxuiHwndSource
//  backend, installs this panel as the backend's non-owning content
//  root, then invokes OnCreate() so the subclass can populate its
//  children before the first layout / paint pass. The consumer calls
//  Show() afterward to display the window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiWindow::Create (const CreateParams & params)
{
    HRESULT                       hr         = S_OK;
    DxuiHwndSource::CreateParams  hostParams;



    BAIL_OUT_IF (m_source != nullptr, S_OK);
    CBRAEx (params.hInstance, E_INVALIDARG);

    m_minSizeDip = params.minSizeDip;

    hostParams.title                 = params.title;
    hostParams.hInstance             = params.hInstance;
    hostParams.ownerHwnd             = params.ownerHwnd;
    hostParams.borderless            = true;
    hostParams.resizable             = params.resizable;
    hostParams.roundedCorners        = true;
    hostParams.darkMode              = true;
    hostParams.backdrop              = DxuiHwndSourceBackdrop::None;
    hostParams.createSwapChain       = true;
    hostParams.captionStyle          = params.captionStyle;
    hostParams.classNameOverride     = params.classNameOverride;
    hostParams.initialSizeDip        = params.initialSizeDip;
    hostParams.insetRootBelowCaption = params.insetContentBelowCaption;
    hostParams.appIconBig            = params.appIconBig;
    hostParams.appIconSmall          = params.appIconSmall;

    m_source = std::make_unique<DxuiHwndSource>();
    m_source->SetClient (this);

    hr = m_source->Create (hostParams);
    CHRF (hr, m_source.reset());

    // Populate children BEFORE installing the root so the first layout
    // pass (driven by SetContentRootRef) sees the fully-built tree.
    OnCreate();

    m_source->SetContentRootRef (this);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiWindow
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindow::~DxuiWindow ()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DestroyBackend
//
//  Detaches this panel from the backend (so a WM_DESTROY / paint during
//  teardown can never reach a partially-destroyed subclass) and releases
//  the HWND + swap chain. Idempotent.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::DestroyBackend ()
{
    if (m_source != nullptr)
    {
        m_source->SetClient (nullptr);
        m_source->SetContentRootRef (nullptr);
        m_source.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::Show ()
{
    HWND  hwnd = Hwnd();



    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, IsIconic (hwnd) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::Hide ()
{
    HWND  hwnd = Hwnd();



    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_HIDE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::Close ()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Invalidate
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::Invalidate ()
{
    HWND  hwnd = Hwnd();



    if (hwnd != nullptr)
    {
        InvalidateRect (hwnd, nullptr, FALSE);
        UpdateWindow   (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetTheme (const IDxuiTheme * theme)
{
    if (m_source != nullptr)
    {
        m_source->SetTheme (theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetTitle (const std::wstring & title)
{
    if (m_source != nullptr)
    {
        m_source->SetTitle (title);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
//  Captures the mouse + takes focus so a drag begun on a scrollbar thumb
//  or column-resize handle keeps receiving moves after the cursor leaves
//  the client, then routes the press into the panel tree.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    HWND  hwnd = Hwnd();



    UNREFERENCED_PARAMETER (wParam);

    if (hwnd != nullptr)
    {
        SetCapture (hwnd);
        SetFocus   (hwnd);
    }

    return DispatchMouse (DxuiMouseEventKind::Down,
                          DxuiMouseButton::Left,
                          (int) (short) LOWORD (lParam),
                          (int) (short) HIWORD (lParam),
                          0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);

    ReleaseCapture();

    return DispatchMouse (DxuiMouseEventKind::Up,
                          DxuiMouseButton::Left,
                          (int) (short) LOWORD (lParam),
                          (int) (short) HIWORD (lParam),
                          0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnRButtonDown (WPARAM wParam, LPARAM lParam)
{
    HWND  hwnd = Hwnd();



    UNREFERENCED_PARAMETER (wParam);

    if (hwnd != nullptr)
    {
        SetFocus (hwnd);
    }

    return DispatchMouse (DxuiMouseEventKind::Down,
                          DxuiMouseButton::Right,
                          (int) (short) LOWORD (lParam),
                          (int) (short) HIWORD (lParam),
                          0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);

    return DispatchMouse (DxuiMouseEventKind::Move,
                          DxuiMouseButton::None,
                          (int) (short) LOWORD (lParam),
                          (int) (short) HIWORD (lParam),
                          0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseWheel
//
//  WM_MOUSEWHEEL packs the point in SCREEN coordinates; convert to the
//  client space the panel tree hit-tests in before dispatching.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    HWND   hwnd  = Hwnd();
    POINT  point = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    float  notch = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;



    UNREFERENCED_PARAMETER (horizontal);

    if (hwnd != nullptr)
    {
        ScreenToClient (hwnd, &point);
    }

    return DispatchMouse (DxuiMouseEventKind::Wheel,
                          DxuiMouseButton::None,
                          point.x,
                          point.y,
                          notch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    return DispatchKey (DxuiKeyEventKind::Down, vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnChar (WPARAM ch, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    return DispatchKey (DxuiKeyEventKind::Char, ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSetCursor
//
//  Delegates to the panel tree's CursorForPoint so a content control
//  (e.g. a DxuiListView column-resize handle) can request a non-default
//  cursor. Returns NotHandled for the plain arrow so DefWindowProc runs.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnSetCursor (WORD hitTest)
{
    DxuiMessageResult  result     = DxuiMessageResult::NotHandled;
    POINT              cursor     = {};
    LPCWSTR            cursorName = nullptr;
    HWND               hwnd       = Hwnd();



    if (hitTest == HTCLIENT && hwnd != nullptr && GetCursorPos (&cursor) != FALSE)
    {
        ScreenToClient (hwnd, &cursor);
        cursorName = CursorForPoint (cursor);
        if (cursorName != nullptr)
        {
            SetCursor (LoadCursorW (nullptr, cursorName));
            result = DxuiMessageResult::Handled;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnGetMinMax
//
//  Clamps the OS minimum track size to the configured minimum client
//  size scaled to the current DPI. Borderless, so client size and
//  window size coincide.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnGetMinMax (MINMAXINFO * info)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;
    UINT               dpi    = Dpi();



    if (info != nullptr && (m_minSizeDip.cx > 0 || m_minSizeDip.cy > 0))
    {
        info->ptMinTrackSize.x = MulDiv (m_minSizeDip.cx, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        info->ptMinTrackSize.y = MulDiv (m_minSizeDip.cy, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnClose ()
{
    OnWindowClose();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::OnDestroy ()
{
    OnWindowDestroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchMouse
//
//  Builds a client-pixel DxuiMouseEvent (plus live modifier state) and
//  routes it through this panel's OnMouse, mapping the bool result onto
//  the host client Handled / NotHandled contract.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::DispatchMouse (DxuiMouseEventKind kind,
                                             DxuiMouseButton    button,
                                             int                x,
                                             int                y,
                                             float              wheelDelta)
{
    DxuiMouseEvent  ev;



    ev.kind        = kind;
    ev.button      = button;
    ev.positionDip = { x, y };
    ev.wheelDelta  = wheelDelta;
    ev.shift       = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl        = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt         = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    return OnMouse (ev) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchKey
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::DispatchKey (DxuiKeyEventKind kind, WPARAM code)
{
    DxuiKeyEvent  ev;



    ev.kind  = kind;
    ev.vk    = code;
    ev.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt   = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    return OnKey (ev) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}
