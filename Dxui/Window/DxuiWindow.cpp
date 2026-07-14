#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"
#include "Widgets/DxuiButton.h"


static constexpr UINT_PTR  s_kDialogTimerId   = 1;      // dialog caret-blink / poll timer id




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
    hostParams.composited            = params.composited;
    hostParams.classNameOverride     = params.classNameOverride;
    hostParams.initialSizeDip        = params.initialSizeDip;
    hostParams.insetRootBelowCaption = params.insetContentBelowCaption;
    hostParams.appIconBig            = params.appIconBig;
    hostParams.appIconSmall          = params.appIconSmall;
    hostParams.presentSyncInterval   = params.presentSyncInterval;

    m_source = std::make_unique<DxuiHwndSource>();
    m_source->SetClient (this);

    hr = m_source->Create (hostParams);
    CHRF (hr, m_source.reset());

    // Paint a subclass modal overlay (e.g. the Settings color picker) as a top
    // layer while it is up. The predicate keeps idle dialogs off the extra flush.
    m_source->SetOverlayHooks (
        [this] () { return HasModalOverlay(); },
        [this] (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
        {
            PaintModalOverlay (painter, text, theme);
        });

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

void DxuiWindow::Show (bool activate)
{
    HWND  hwnd = Hwnd();



    if (hwnd != nullptr)
    {
        if (activate)
        {
            ShowWindow (hwnd, IsIconic (hwnd) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow (hwnd);
        }
        else
        {
            // Bring it on-screen at the top of the z-order but leave focus where
            // it is, so an auto-opened window never eats the guest's keystrokes.
            ShowWindow (hwnd, SW_SHOWNOACTIVATE);
        }
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
//  ShowModalDialog
//
//  Modal show: emphasize + auto-wire the command buttons, attach the
//  focus manager, disable the owner, and run a private message pump
//  until EndDialog() resolves a result (a command button click, Enter
//  on the default button, or Escape / close-box on IDCANCEL). Re-enables
//  the owner and returns the result. Mirrors Win32 DialogBox / EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiWindow::ShowModalDialog (int defaultButtonId)
{
    HRESULT  hr            = S_OK;
    MSG      msg           = {};
    BOOL     gotMessage    = FALSE;
    bool     ownerDisabled = false;
    int      result        = IDCANCEL;



    CBRA (m_source != nullptr);

    BeginDialogMode (defaultButtonId, true);

    m_source->SetTimer (s_kDialogTimerId, m_dialogTickMs);

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

        if (!ProcessDialogMessage (msg))
        {
            TranslateMessage (&msg);
            DispatchMessageW  (&msg);
        }
    }

    result = m_modalResult;

Error:
    if (ownerDisabled)
    {
        EnableWindow (m_ownerHwnd, TRUE);
    }

    if (m_source != nullptr)
    {
        m_source->KillTimer (s_kDialogTimerId);
    }

    m_dialogActive = false;
    m_modal        = false;
    Hide();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowModelessDialog
//
//  Modeless show: applies the same dialog behaviors as ShowModalDialog
//  (button wiring, focus-manager Tab traversal, default-button emphasis,
//  periodic tick) but does NOT disable the owner and does NOT run a
//  private loop -- it shows the window and returns immediately. The
//  host's own message loop drives it; dialog-key nav flows through this
//  window's WndProc (all controls live in this one HWND, so no
//  IsDialogMessage pump is needed). Close via EndDialog(), which hides
//  the window and fires the SetOnDialogEnd callback. Mirrors Win32
//  CreateDialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::ShowModelessDialog (int defaultButtonId)
{
    if (m_source != nullptr)
    {
        BeginDialogMode (defaultButtonId, false);

        m_source->SetTimer (s_kDialogTimerId, m_dialogTickMs);

        Show();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProcessDialogMessage
//
//  Dxui's IsDialogMessage analog for modeless dialogs. Intercepts only
//  the dialog-navigation keys for this window (Tab/Shift+Tab, Enter,
//  Escape) before TranslateMessage can manufacture a WM_CHAR or the
//  normal dispatch path can bypass the focus manager. Everything else
//  falls through to the host's standard Translate/Dispatch path.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::ProcessDialogMessage (const MSG & msg)
{
    bool  isHandled = false;
    HWND  hwnd      = Hwnd();

    // Only intercept the dialog-navigation keys while a dialog is active and the
    // message targets this window; otherwise fall through to normal dispatch.
    // Plain early returns (not BAIL_OUT_IF): this returns bool, not HRESULT.
    if (!m_dialogActive || hwnd == nullptr) { return false; }
    if (msg.hwnd != hwnd)                   { return false; }

    switch (msg.message)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (msg.wParam == VK_TAB || msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)
            {
                isHandled = (DispatchDialogKey (msg.wParam) == DxuiMessageResult::Handled);
            }
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDialog
//
//  Resolves an open dialog. Modal: records the result once and flags the
//  private pump to exit (posts a no-op so a blocked GetMessage re-checks
//  the done flag) -- ShowModalDialog returns the result. Modeless: no
//  loop to break, so it deactivates the dialog behaviors, hides the
//  window, and fires the SetOnDialogEnd callback with the result. A
//  command button, Enter / Escape, or content code calls this. Mirrors
//  Win32 EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::EndDialog (int result)
{
    HWND  hwnd = Hwnd();



    if (m_dialogActive)
    {
        if (m_modal)
        {
            if (!m_modalDone)
            {
                m_modalResult = result;
                m_modalDone   = true;

                if (hwnd != nullptr)
                {
                    PostMessageW (hwnd, WM_NULL, 0, 0);
                }
            }
        }
        else
        {
            m_dialogActive = false;

            if (m_source != nullptr)
            {
                m_source->KillTimer (s_kDialogTimerId);
            }

            Hide();

            if (m_onDialogEnd)
            {
                m_onDialogEnd (result);
            }
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetComposedOpacity
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetComposedOpacity (float opacity)
{
    if (m_source != nullptr)
    {
        m_source->SetComposedOpacity (opacity);
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
    // WM_MOUSEMOVE carries the held-button state in wParam (MK_LBUTTON).
    // Forward it so controls can distinguish a drag (button held) from a
    // hover: a scrollbar-thumb / selection drag relies on Move events still
    // reporting Left, otherwise the control reads the first drag-move as a
    // button release and aborts the drag.
    DxuiMouseButton  button = (wParam & MK_LBUTTON) ? DxuiMouseButton::Left
                                                    : DxuiMouseButton::None;

    return DispatchMouse (DxuiMouseEventKind::Move,
                          button,
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



    if (hwnd != nullptr)
    {
        ScreenToClient (hwnd, &point);
    }

    return DispatchMouse (DxuiMouseEventKind::Wheel,
                          DxuiMouseButton::None,
                          point.x,
                          point.y,
                          notch,
                          horizontal);
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

    // A modal overlay swallows every key (routing the ones it wants to its own
    // handler) so dialog navigation can't leak to the page behind it.
    if (HasModalOverlay())
    {
        (void) OnOverlayKey (vk);
        return DxuiMessageResult::Handled;
    }

    result = m_dialogActive ? DispatchDialogKey (vk)
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

    // A modal overlay owns text entry (e.g. the color picker's hex field).
    if (HasModalOverlay())
    {
        (void) OnOverlayChar ((wchar_t) ch);
        return DxuiMessageResult::Handled;
    }

    if (m_dialogActive)
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
//  during ShowModalDialog().
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnTimer (UINT_PTR timerId)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;


    if (m_dialogActive && timerId == s_kDialogTimerId)
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
    if (m_dialogActive)
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
                                             float              wheelDelta,
                                             bool               wheelHorizontal)
{
    DxuiMouseEvent  ev;



    ev.kind            = kind;
    ev.button          = button;
    ev.positionDip     = { x, y };
    ev.wheelDelta      = wheelDelta;
    ev.wheelHorizontal = wheelHorizontal;
    ev.shift       = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl        = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt         = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    // A modal overlay captures all mouse input so the page beneath it stays
    // inert (clicks outside the overlay dialog are simply ignored).
    if (HasModalOverlay())
    {
        (void) OnOverlayMouse (ev);
        return DxuiMessageResult::Handled;
    }

    // Repaint immediately when a control consumes the event so drags (slider
    // thumbs, scrubbing) and hover states track the cursor every frame instead
    // of only on the ~half-second dialog tick. InvalidateRect coalesces, so at
    // most one paint lands per frame regardless of mouse-move rate.
    if (OnMouse (ev))
    {
        Invalidate();
        return DxuiMessageResult::Handled;
    }
    return DxuiMessageResult::NotHandled;
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
//  DispatchDialogKey
//
//  Dialog key routing (modal or modeless): Tab / Shift+Tab move focus;
//  Escape fires the IDCANCEL button (or resolves IDCANCEL directly);
//  Enter lets the focused control claim it first, else fires the default
//  button; any other key routes to the focused control.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::DispatchDialogKey (WPARAM vk)
{
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    bool               shift     = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    bool               isHandled = false;



    switch (vk)
    {
        case VK_TAB:
            // Ctrl(+Shift)+Tab cycles a tabbed window's pages (e.g.
            // DxuiPropertySheet); plain Tab traverses focus within the page.
            if ((GetKeyState (VK_CONTROL) & 0x8000) != 0 && OnDialogTabSwitch (shift))
            {
                isHandled = true;
                break;
            }
            isHandled = m_focus.HandleKey (shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
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

    // Any consumed dialog key (Tab focus move, arrow-key slider nudge, etc.)
    // repaints so the change shows at once rather than on the next tick.
    if (isHandled)
    {
        Invalidate();
    }

    result = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDialogMode
//
//  Shared setup for modal and modeless dialogs: remember the default
//  button, clear any stale modal result, wire the buttons, and attach /
//  rebuild the focus manager so Tab traversal, Enter, and Escape all
//  run through the dialog contract.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::BeginDialogMode (int defaultButtonId, bool modal)
{
    m_defaultButtonId = defaultButtonId;
    m_modalResult     = IDCANCEL;
    m_modalDone       = false;
    m_modal           = modal;
    m_dialogActive    = true;

    WireDialogButtons();

    m_focus.SetTheme (m_theme);
    m_focus.Attach   (this);
    m_focus.Rebuild();

    if (m_initialFocus != nullptr)
    {
        m_focus.SetFocused (m_initialFocus);
    }
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
