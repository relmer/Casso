#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"
#include "Widgets/DxuiButton.h"


static constexpr UINT_PTR  s_kModalTimerId    = 1;      // modal caret-blink / poll timer id




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
    m_ownerHwnd  = params.ownerHwnd;

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
//  ShowDialog
//
//  Modal show: emphasize + auto-wire the command buttons, attach the
//  focus manager, disable the owner, and run a private message pump
//  until EndDialog() resolves a result (a command button click, Enter
//  on the default button, or Escape / close-box on IDCANCEL). Re-enables
//  the owner and returns the result. Mirrors Win32 DialogBox / EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiWindow::ShowDialog (int defaultButtonId)
{
    HRESULT  hr            = S_OK;
    MSG      msg           = {};
    BOOL     gotMessage    = FALSE;
    bool     ownerDisabled = false;
    int      result        = IDCANCEL;



    CBRA (m_source != nullptr);

    m_defaultButtonId = defaultButtonId;
    m_modalResult     = IDCANCEL;
    m_modalDone       = false;
    m_modalActive     = true;

    WireDialogButtons();

    m_focus.SetTheme (m_theme);
    m_focus.Attach   (this);
    m_focus.Rebuild();

    if (m_initialFocus != nullptr)
    {
        m_focus.SetFocused (m_initialFocus);
    }

    m_source->SetTimer (s_kModalTimerId, m_modalTickMs);

    if (m_ownerHwnd != nullptr)
    {
        EnableWindow (m_ownerHwnd, FALSE);
        ownerDisabled = true;
    }

    Show();

    while (!m_modalDone)
    {
        gotMessage = GetMessageW (&msg, nullptr, 0, 0);

        if (gotMessage == 0)
        {
            PostQuitMessage ((int) msg.wParam);
            break;
        }

        CWRA (gotMessage != -1);

        TranslateMessage (&msg);
        DispatchMessageW  (&msg);
    }

    result = m_modalResult;

Error:
    if (ownerDisabled)
    {
        EnableWindow (m_ownerHwnd, TRUE);
    }

    if (m_source != nullptr)
    {
        m_source->KillTimer (s_kModalTimerId);
    }

    m_modalActive = false;
    Hide();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDialog
//
//  Records the modal result once and flags the pump to exit; posts a
//  no-op message so a blocked GetMessage re-checks the done flag. A
//  command button, Enter / Escape, or content code calls this. Mirrors
//  Win32 EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::EndDialog (int result)
{
    HWND  hwnd = Hwnd();



    if (m_modalActive && !m_modalDone)
    {
        m_modalResult = result;
        m_modalDone   = true;

        if (hwnd != nullptr)
        {
            PostMessageW (hwnd, WM_NULL, 0, 0);
        }
    }
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
    m_theme = theme;

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
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;


    UNREFERENCED_PARAMETER (lParam);

    result = m_modalActive ? DispatchModalKey (vk)
                           : DispatchKey (DxuiKeyEventKind::Down, vk);

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnChar (WPARAM ch, LPARAM lParam)
{
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    IDxuiControl *     focused   = nullptr;
    bool               isHandled = false;


    UNREFERENCED_PARAMETER (lParam);

    if (m_modalActive)
    {
        focused = m_focus.Focused();

        if (focused != nullptr)
        {
            isHandled = focused->OnChar ((wchar_t) ch);
        }

        result = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
    }
    else
    {
        result = DispatchKey (DxuiKeyEventKind::Char, ch);
    }

    return result;
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
//  OnTimer
//
//  Modal caret-blink / poll tick: drives the subclass poller and repaints
//  so a focused caret blinks and any per-tick progress shows. Only active
//  during ShowDialog().
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnTimer (UINT_PTR timerId)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;


    if (m_modalActive && timerId == s_kModalTimerId)
    {
        OnDialogTick();
        Invalidate();
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
    if (m_modalActive)
    {
        if (!TriggerButtonById (IDCANCEL))
        {
            EndDialog (IDCANCEL);
        }
    }
    else
    {
        OnWindowClose();
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchModalKey
//
//  Modal key routing: Tab / Shift+Tab move focus; Escape fires the
//  IDCANCEL button (or resolves IDCANCEL directly); Enter lets the
//  focused control claim it first, else fires the default button; any
//  other key routes to the focused control. Mirrors the old modal client.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::DispatchModalKey (WPARAM vk)
{
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    bool               shift     = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    bool               isHandled = false;



    switch (vk)
    {
        case VK_TAB:
            isHandled = m_focus.HandleKey (shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
            Invalidate();
            break;

        case VK_ESCAPE:
            if (!TriggerButtonById (IDCANCEL))
            {
                EndDialog (IDCANCEL);
            }

            isHandled = true;
            break;

        case VK_RETURN:
            isHandled = RouteKeyToFocused (vk, shift) || TriggerButtonById (m_defaultButtonId);
            break;

        default:
            isHandled = RouteKeyToFocused (vk, shift);
            break;
    }

    result = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireDialogButtons
//
//  Walks the panel tree: emphasizes the default-id command button and,
//  for every command button (nonzero id) lacking a custom click handler,
//  installs a click that calls EndDialog(its id). Buttons that must stay
//  open (Download, Apply) carry their own SetOnClick and are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::WireDialogButtons ()
{
    int  defaultId = m_defaultButtonId;



    ForEachButton (this, [this, defaultId] (DxuiButton * button)
    {
        int  id = button->CommandId();


        button->SetEmphasis (id != 0 && id == defaultId);

        if (id != 0 && !button->HasClickHandler())
        {
            button->SetOnClick ([this, id] () { EndDialog (id); });
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteKeyToFocused
//
//  Forwards a key-down to the focused control as a DxuiKeyEvent. Returns
//  true iff a control has focus and consumed the key.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::RouteKeyToFocused (WPARAM vk, bool shift)
{
    HRESULT         hr        = S_OK;
    bool            isHandled = false;
    IDxuiControl *  focused   = m_focus.Focused();
    DxuiKeyEvent    ke;



    BAIL_OUT_IF (focused == nullptr, S_OK);

    ke.kind  = DxuiKeyEventKind::Down;
    ke.vk    = vk;
    ke.shift = shift;

    isHandled = focused->OnKey (ke);

Error:
    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TriggerButtonById
//
//  Finds and clicks the enabled / visible button with the given command
//  id. Returns true iff such a button exists and was fired.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::TriggerButtonById (int commandId)
{
    bool          fired  = false;
    DxuiButton *  button = FindButtonById (this, commandId);



    if (button != nullptr && button->Enabled() && button->Visible())
    {
        button->Click();
        fired = true;
    }

    return fired;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindButtonById
//
//  Recursively returns the first button in the tree whose command id
//  matches, or null. A zero commandId never matches (0 = not a command
//  button).
//
////////////////////////////////////////////////////////////////////////////////

DxuiButton * DxuiWindow::FindButtonById (IDxuiControl * node, int commandId)
{
    HRESULT       hr    = S_OK;
    DxuiButton *  found = nullptr;
    DxuiButton *  self  = nullptr;
    size_t        count = 0;
    size_t        i     = 0;



    BAIL_OUT_IF (node == nullptr || commandId == 0, S_OK);

    self = dynamic_cast<DxuiButton *> (node);

    if (self != nullptr && self->CommandId() == commandId)
    {
        found = self;
        BAIL_OUT_IF (true, S_OK);
    }

    count = node->ChildCount();

    for (i = 0; i < count; ++i)
    {
        found = FindButtonById (node->Child (i), commandId);

        if (found != nullptr)
        {
            break;
        }
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForEachButton
//
//  Recursively invokes fn for every DxuiButton in the tree rooted at node.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::ForEachButton (IDxuiControl * node, const std::function<void (DxuiButton *)> & fn)
{
    HRESULT       hr    = S_OK;
    DxuiButton *  self  = nullptr;
    size_t        count = 0;
    size_t        i     = 0;



    BAIL_OUT_IF (node == nullptr, S_OK);

    self = dynamic_cast<DxuiButton *> (node);

    if (self != nullptr)
    {
        fn (self);
    }

    count = node->ChildCount();

    for (i = 0; i < count; ++i)
    {
        ForEachButton (node->Child (i), fn);
    }

Error:
    return;
}
