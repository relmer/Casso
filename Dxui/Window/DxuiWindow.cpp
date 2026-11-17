#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"
#include "Core/DxuiWindowFrame.h"
#include "Widgets/DxuiInfoTip.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiContextMenu.h"
#include "Widgets/DxuiTextInput.h"
#include "Window/DxuiDialogMode.h"


static constexpr UINT_PTR  s_kDialogTimerId   = 1;      // dialog caret-blink / poll timer id
static constexpr UINT_PTR  s_kMenuTimerId     = 2;      // advances a dialog's context menu
static constexpr UINT      s_kMenuTickMs      = 16;





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

    hostParams.title                  = params.title;
    hostParams.hInstance              = params.hInstance;
    hostParams.ownerHwnd              = params.ownerHwnd;
    hostParams.borderless             = true;
    hostParams.resizable              = params.resizable;
    hostParams.frameless              = params.frameless;
    hostParams.roundedCorners         = true;
    hostParams.darkMode               = true;
    hostParams.backdrop               = !params.micaBackdrop ? DxuiHwndSourceBackdrop::None
                                      : params.micaAlt       ? DxuiHwndSourceBackdrop::MicaAlt
                                                             : DxuiHwndSourceBackdrop::Mica;
    hostParams.drawOverBackdrop       = params.micaBackdrop;
    hostParams.createSwapChain        = true;
    hostParams.captionStyle           = params.captionStyle;
    hostParams.composited             = params.composited;
    hostParams.classNameOverride      = params.classNameOverride;
    hostParams.initialSizeDip         = params.initialSizeDip;
    hostParams.useInitialWindowRectPx = params.useInitialWindowRectPx;
    hostParams.initialWindowRectPx    = params.initialWindowRectPx;
    hostParams.insetRootBelowCaption  = params.insetContentBelowCaption;
    hostParams.fitToWorkArea          = params.fitToWorkArea;
    hostParams.appIconBig             = params.appIconBig;
    hostParams.appIconSmall           = params.appIconSmall;
    hostParams.presentSyncInterval    = params.presentSyncInterval;
    hostParams.paceFrames             = params.paceFrames;
    hostParams.createNoActivate       = params.createNoActivate;
    hostParams.toolWindow             = params.toolWindow;
    hostParams.placement              = params.placement;
    hostParams.placementAnchorHwnd    = params.placementAnchorHwnd;
    hostParams.placementAnchorRectPx  = params.placementAnchorRectPx;

    m_source = std::make_unique<DxuiHwndSource>();
    m_source->SetClient (this);

    hr = m_source->Create (hostParams);
    CHRF (hr, m_source.reset());

    AttachSource();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindow::CreateHeadless
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiWindow::CreateHeadless (const CreateParams & params)
{
    constexpr float  kResizeBorderDip = 6.0f;
    HRESULT          hr               = S_OK;
    RECT             boundsDip        = { 0, 0, params.initialSizeDip.cx, params.initialSizeDip.cy };
    RECT             rectPx           = params.useInitialWindowRectPx ? params.initialWindowRectPx : boundsDip;



    BAIL_OUT_IF (m_source != nullptr, S_OK);

    m_minSizeDip = params.minSizeDip;
    m_ownerHwnd  = params.ownerHwnd;

    m_source = std::make_unique<DxuiHwndSource> (boundsDip, kResizeBorderDip, nullptr);
    m_source->SetClient       (this);
    m_source->SetWindowRectPx (rectPx);

    AttachSource();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindow::AttachSource
//
//  The host's hooks, the children OnCreate adds, and this panel as the root.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::AttachSource()
{
    // Paint a subclass modal overlay (e.g. the Settings color picker) as a top
    // layer while it is up. The predicate keeps idle dialogs off the extra flush.
    m_source->SetOverlayHooks (
        [this] () { return HasModalOverlay(); },
        [this] (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
        {
            PaintModalOverlay (painter, text, theme);
        });

    // Children painted above the page, a slid-out pane for one, get a flush
    // of their own so they cover the page's text.
    m_source->SetTopLayerHooks (
        [this] () { return HasTopLayer(); },
        [this] (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
        {
            PaintTopLayer (painter, text, theme);
        });

    // What a drag shows gets a flush above both, so it covers their pictures.
    m_source->SetDragLayerHooks (
        [this] () { return HasDragLayer(); },
        [this] (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
        {
            PaintDragLayer (painter, text, theme);
        });

    // A layer that moves alone paints last, over the frame saved under it.
    m_source->SetAnimationLayerHooks (
        [this] () { return HasAnimationLayer(); },
        [this] (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
        {
            PaintAnimationLayer (painter, text, theme);
        });

    // Populate children BEFORE installing the root so the first layout
    // pass (driven by SetContentRootRef) sees the fully-built tree.
    OnCreate();

    m_source->SetContentRootRef (this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiWindow
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindow::~DxuiWindow()
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

void DxuiWindow::DestroyBackend()
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
    HWND  hwnd = GetHwnd();



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
            // SW_SHOWNA, not SW_SHOWNOACTIVATE: NOACTIVATE shows the window at
            // its "most recent size and position", which RESTORES a maximized
            // window -- the printer preview's auto-open re-fires during a live
            // print and was quietly un-maximizing it. SHOWNA keeps the current
            // state (maximized stays maximized) and still never steals focus.
            ShowWindow (hwnd, SW_SHOWNA);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowMaximized
//
//  Windows has no show command that maximizes without activating: SW_MAXIMIZE
//  and SW_SHOWMAXIMIZED both take the foreground. A window that must not take
//  it is maximized by hand -- the maximized style, then the work area of its
//  monitor with its invisible border outside it -- and shown without
//  activation. The rect it had before stays its restored rect.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::ShowMaximized (bool activate)
{
    HRESULT      hr      = S_OK;
    HWND         hwnd    = GetHwnd();
    HMONITOR     monitor = nullptr;
    MONITORINFO  info    = { sizeof (info) };
    RECT         placed  = {};
    BOOL         gotInfo = FALSE;
    BOOL         moved   = FALSE;
    LONG_PTR     style   = 0;



    BAIL_OUT_IF (hwnd == nullptr, S_OK);

    if (activate)
    {
        ShowWindow (hwnd, SW_MAXIMIZE);
    }

    BAIL_OUT_IF (activate, S_OK);

    monitor = DxuiDisplay::Get().MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
    gotInfo = DxuiDisplay::Get().GetMonitorInfoW (monitor, &info);
    CWRA (gotInfo);

    style = GetWindowLongPtrW (hwnd, GWL_STYLE);
    SetWindowLongPtrW (hwnd, GWL_STYLE, style | WS_MAXIMIZE);

    placed = DxuiWindowFrame::ToWindowRect (hwnd, info.rcWork);
    moved  = SetWindowPos (hwnd, nullptr, placed.left, placed.top,
                           placed.right - placed.left, placed.bottom - placed.top,
                           SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    CWRA (moved);

    ShowWindow (hwnd, SW_SHOWNA);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::Hide()
{
    HWND  hwnd = GetHwnd();



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

void DxuiWindow::Close()
{
    DestroyBackend();
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

void DxuiWindow::Invalidate()
{
    HWND  hwnd = GetHwnd();



    if (hwnd != nullptr)
    {
        InvalidateRect (hwnd, nullptr, FALSE);
        UpdateWindow   (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InvalidateAnimationLayer
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::InvalidateAnimationLayer()
{
    HWND  hwnd    = GetHwnd();
    RECT  pending = {};



    if (hwnd == nullptr)
    {
        return;
    }

    //  A paint already asked for, by input or a change, is a whole frame.
    if (!GetUpdateRect (hwnd, &pending, FALSE))
    {
        m_source->RequestAnimationFrame();
    }

    Invalidate();
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
//  markAt is the position in the title of the unsaved-changes dot, or npos
//  for none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetTitle (const std::wstring & title, size_t markAt)
{
    if (m_source != nullptr)
    {
        m_source->SetTitle (title, markAt);
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
    HWND  hwnd = GetHwnd();



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
    HWND  hwnd = GetHwnd();



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
//  TryShowTextBoxMenu
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::TryShowTextBoxMenu (POINT clientPx)
{
    DxuiTextInput  * box = FindTextBoxAt (*this, clientPx);



    if (box == nullptr || GetPopupHost() == nullptr)
    {
        return false;
    }

    m_focus.SetFocused (box);
    DxuiContextMenu::Show (*GetPopupHost(), clientPx.x, clientPx.y, DxuiContextMenu::MakeEditItems (*box));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindTextBoxAt
//
//  The front-most visible text box under a point, among the controls under
//  `node`.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * DxuiWindow::FindTextBoxAt (const IDxuiControl & node, POINT clientPx)
{
    DxuiTextInput  * found = nullptr;



    for (size_t i = node.GetChildCount(); i > 0 && found == nullptr; i--)
    {
        IDxuiControl  * child = node.GetChild (i - 1);

        if (child == nullptr || !child->IsVisible())
        {
            continue;
        }

        found = dynamic_cast<DxuiTextInput *> (child);
        found = (found != nullptr && found->HitTest (clientPx.x, clientPx.y)) ? found : FindTextBoxAt (*child, clientPx);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonUp
//
//  The release too, for a context menu that opens as the button comes up, as
//  Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnRButtonUp (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);

    return DispatchMouse (DxuiMouseEventKind::Up,
                          DxuiMouseButton::Right,
                          (int) (short) LOWORD (lParam),
                          (int) (short) HIWORD (lParam),
                          0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnXButtonUp
//
//  A five-button mouse's back and forward buttons, delivered on release,
//  which is when Explorer acts on them.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnXButtonUp (WPARAM wParam, LPARAM lParam)
{
    DxuiMouseButton  button = (GET_XBUTTON_WPARAM (wParam) == XBUTTON1) ? DxuiMouseButton::Back : DxuiMouseButton::Forward;



    return DispatchMouse (DxuiMouseEventKind::Up,
                          button,
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
//  OnMouseLeave
//
//  WM_MOUSELEAVE. Without it a control's hover, a tip or a preview stayed up
//  when the pointer moved to another of the app's windows or onto a popup,
//  since this window then got no more moves.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseLeave()
{
    SendMouseLeave();
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SendMouseLeave()
{
    bool  isDragging = DxuiInputState::IsKeyDown (VK_LBUTTON);



    if (isDragging)
    {
        return;
    }

    (void) DispatchMouse (DxuiMouseEventKind::Leave, DxuiMouseButton::None, kNowherePx, kNowherePx, 0.0f);
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
    HWND               hwnd   = GetHwnd();
    POINT              point  = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    float              notch  = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    if (hwnd != nullptr)
    {
        ScreenToClient (hwnd, &point);
    }

    //  The message's flags are the keys as they were when the wheel turned.
    m_wheelCtrl = (GET_KEYSTATE_WPARAM (wParam) & MK_CONTROL) != 0;

    result = DispatchMouse (DxuiMouseEventKind::Wheel,
                            DxuiMouseButton::None,
                            point.x,
                            point.y,
                            notch,
                            horizontal);

    m_wheelCtrl = false;

    return result;
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
    // handler) so dialog navigation can't leak to the page behind it. The
    // overlay's own return is discarded on purpose: handled or not, the key
    // stops here. A filter that keeps a key, such as Escape canceling a drag
    // under way, comes before all of it.
    if (m_inputFilters.RunKey (vk))
    {
        result = DxuiMessageResult::Handled;
    }
    else if (HasModalOverlay())
    {
        (void) OnOverlayKey (vk);
        result = DxuiMessageResult::Handled;
    }
    else if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().IsVisible())
    {
        //  An open context menu is where the keyboard is, as a Windows menu's
        //  is: arrows, Enter, Escape and mnemonics act on it, not the page.
        (void) GetPopupHost()->GetContextMenu().OnKey (vk);
        result = DxuiMessageResult::Handled;
    }
    else
    {
        result = m_dialog.IsActive() ? m_dialog.DispatchKey (vk)
                                     : DispatchKey (DxuiKeyEventKind::Down, vk);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
//  Routes a typed character, in three tiers by what currently owns text entry.
//
//  A MODAL OVERLAY takes precedence over everything and always reports
//  handled, because it is modal: a character reaching anything behind it --
//  the color picker's hex field being the case in point -- would type into a
//  control the user cannot see.
//
//  A DIALOG routes to the focused control only, with no fan-out. There is no
//  sensible recipient for a character when nothing has focus, so an unfocused
//  dialog reports NotHandled and lets default processing run rather than
//  swallowing the keystroke.
//
//  Everything else goes through the normal key dispatch, which is the path a
//  plain window's content uses.
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
        result = DxuiMessageResult::Handled;
    }
    else if (m_dialog.IsActive())
    {
        // Dialog text entry goes to the focused control only -- there is no
        // fanout, so an unfocused dialog reports NotHandled.
        focused = m_focus.GetFocusedControl();

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
//  OnSetFocus
//
//  A text field's caret shows only while its window holds the focus, so a
//  window behind another app or another window has nothing blinking. Both
//  then run OnWindowFocusChanged, and return NotHandled so DefWindowProc
//  still runs.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnSetFocus()
{
    SetTextInputsWindowActive (this, true);
    Invalidate();
    OnWindowFocusChanged (true);
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKillFocus
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnKillFocus()
{
    SetTextInputsWindowActive (this, false);
    Invalidate();
    OnWindowFocusChanged (false);
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCancelMode
//
//  The system canceling whatever mode the window is in, as it does before a
//  dialog or a menu comes up. DefWindowProc still runs, and releases the
//  capture.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnCancelMode()
{
    m_inputFilters.RunMouseLost (WM_CANCELMODE);
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCaptureChanged
//
//  The window's own capture moving to itself is not a loss.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnCaptureChanged (HWND newCapture)
{
    if (newCapture != GetHwnd())
    {
        m_inputFilters.RunMouseLost (WM_CAPTURECHANGED);
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTextInputsWindowActive
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetTextInputsWindowActive (IDxuiControl * node, bool active)
{
    if (node == nullptr)
    {
        return;
    }

    node->OnWindowActiveChanged (active);

    for (size_t i = 0; i < node->GetChildCount(); ++i)
    {
        SetTextInputsWindowActive (node->GetChild (i), active);
    }
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
    HWND               hwnd       = GetHwnd();



    if (hitTest == HTCLIENT && hwnd != nullptr && DxuiInputState::GetCursorPos (&cursor) != FALSE)
    {
        ScreenToClient (hwnd, &cursor);
        cursorName = GetCursorForPoint (cursor);
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
//  size scaled to the current DPI, and the maximum to what the subclass
//  reports, never below the minimum. The maximum is a client size, so the
//  frame around a resizable window's client is added to it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnGetMinMax (MINMAXINFO * info)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;
    UINT               dpi    = GetDpi();
    SIZE               maxPx  = {};



    if (info == nullptr)
    {
        return result;
    }

    if (m_minSizeDip.cx > 0 || m_minSizeDip.cy > 0)
    {
        info->ptMinTrackSize.x = MulDiv (m_minSizeDip.cx, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        info->ptMinTrackSize.y = MulDiv (m_minSizeDip.cy, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        result = DxuiMessageResult::Handled;
    }

    if (TryGetMaxTrackSizePx (maxPx))
    {
        maxPx = AddFrameSizePx (maxPx);

        info->ptMaxTrackSize.x = std::max (maxPx.cx, info->ptMinTrackSize.x);
        info->ptMaxTrackSize.y = std::max (maxPx.cy, info->ptMinTrackSize.y);
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampSize
//
//  Each dimension held within [minSize, maxSize]. A maximum below the
//  minimum gives way to it.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiWindow::ClampSize (const SIZE & size, const SIZE & minSize, const SIZE & maxSize)
{
    return SIZE { std::clamp (size.cx, minSize.cx, std::max (maxSize.cx, minSize.cx)),
                  std::clamp (size.cy, minSize.cy, std::max (maxSize.cy, minSize.cy)) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitRectToMaxSize
//
//  The window at its maximum, cut to the work area, keeping its top left
//  corner unless that would carry it past the work area's right or bottom
//  edge. It grows as well as shrinks: a size remembered from before the
//  content grew, or a design size the content has since outgrown, would
//  otherwise open with a scroll bar on a screen with room to spare.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiWindow::FitRectToMaxSize (const RECT & windowPx, const SIZE & maxPx, const RECT & workPx)
{
    LONG  widthPx  = std::min (maxPx.cx, workPx.right  - workPx.left);
    LONG  heightPx = std::min (maxPx.cy, workPx.bottom - workPx.top);
    LONG  left     = std::max (workPx.left, std::min (windowPx.left, workPx.right  - widthPx));
    LONG  top      = std::max (workPx.top,  std::min (windowPx.top,  workPx.bottom - heightPx));



    return RECT { left, top, left + widthPx, top + heightPx };
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetGrownRect
//
//  FitRectToMaxSize with the window's own size as the floor, so a dimension
//  the content does not exceed keeps its size.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::TryGetGrownRect (const RECT & windowPx, const SIZE & maxPx, const RECT & workPx, RECT & outPx)
{
    SIZE  sizePx  = { windowPx.right - windowPx.left, windowPx.bottom - windowPx.top };
    SIZE  grownPx = { std::max (sizePx.cx, maxPx.cx), std::max (sizePx.cy, maxPx.cy) };



    outPx = FitRectToMaxSize (windowPx, grownPx, workPx);

    return outPx.right - outPx.left > sizePx.cx || outPx.bottom - outPx.top > sizePx.cy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitToMaxSize
//
//  Sizes the window to TryGetMaxClientSizePx within its monitor's work area,
//  and places it there by its creation-time placement. Called before the
//  window is first shown, once its content has been laid out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::FitToMaxSize()
{
    ResizeToMaxSize (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GrowToMaxSize
//
//  Grows the window toward TryGetMaxClientSizePx within its monitor's work
//  area, and leaves it alone when the content fits.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::GrowToMaxSize()
{
    ResizeToMaxSize (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResizeToMaxSize
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::ResizeToMaxSize (bool growOnly)
{
    HRESULT      hr      = S_OK;
    HWND         hwnd    = GetHwnd();
    HMONITOR     monitor = nullptr;
    MONITORINFO  info    = { sizeof (info) };
    RECT         rect    = {};
    RECT         fitted  = {};
    SIZE         maxPx   = {};
    SIZE         sizePx  = {};
    POINT        topLeft = {};
    bool         hasMax  = false;
    bool         isSame  = false;
    bool         grows   = false;
    BOOL         done    = FALSE;



    CBRA (hwnd != nullptr);

    hasMax = TryGetMaxClientSizePx (maxPx);
    BAIL_OUT_IF (!hasMax, S_OK);

    //  Windows computes a borderless window's resize borders only at a frame
    //  change, and a hidden window has had none since its creation: its
    //  client fills the window and the frame measures zero. A frame change is
    //  forced first.
    done = SetWindowPos (hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (done);

    maxPx = AddFrameSizePx (maxPx);

    done = GetWindowRect (hwnd, &rect);
    CWRA (done);

    monitor = DxuiDisplay::Get().MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
    done    = DxuiDisplay::Get().GetMonitorInfoW (monitor, &info);
    CWRA (done);

    if (growOnly)
    {
        grows = TryGetGrownRect (rect, maxPx, info.rcWork, fitted);
        BAIL_OUT_IF (!grows, S_OK);
    }
    else
    {
        fitted = FitRectToMaxSize (rect, maxPx, info.rcWork);

        // The fit runs while the window is still hidden, once its content can
        // be measured, so it is placed here at the size it really opens at:
        // the creation-time placement measured a size that was only a guess.
        sizePx = { fitted.right - fitted.left, fitted.bottom - fitted.top };

        if (m_source->TryGetPlacementForSize (sizePx, topLeft))
        {
            fitted = RECT { topLeft.x, topLeft.y, topLeft.x + sizePx.cx, topLeft.y + sizePx.cy };
        }
    }

    isSame = EqualRect (&fitted, &rect) != FALSE;
    BAIL_OUT_IF (isSame, S_OK);

    done = SetWindowPos (hwnd, nullptr, fitted.left, fitted.top,
                         fitted.right - fitted.left, fitted.bottom - fitted.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (done);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddFrameSizePx
//
//  A client size as the window size around it: a resizable window has
//  invisible resize borders outside its client, and a window size without
//  them makes the client that much smaller. Before the window has a client
//  to measure, the client size is returned as given.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiWindow::AddFrameSizePx (const SIZE & clientPx) const
{
    HWND  hwnd   = GetHwnd();
    RECT  window = {};
    RECT  client = {};
    SIZE  sizePx = clientPx;



    if (hwnd != nullptr && GetWindowRect (hwnd, &window) && GetClientRect (hwnd, &client) &&
        client.right > 0 && client.bottom > 0)
    {
        sizePx.cx += (window.right - window.left) - client.right;
        sizePx.cy += (window.bottom - window.top) - client.bottom;
    }

    return sizePx;
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



    if (m_dialog.IsActive() && timerId == s_kDialogTimerId)
    {
        OnDialogTick();
        Invalidate();
        result = DxuiMessageResult::Handled;
    }

    //  A dialog's context menu slides open, and opens its submenus, on these
    //  ticks while it is up; the dialog's own tick is far too slow for that.
    if (timerId == s_kMenuTimerId && m_source != nullptr)
    {
        DxuiPopupMenu  & menu = m_source->GetContextMenu();

        menu.Tick ((int64_t) GetTickCount64());

        if (!menu.IsVisible() && !menu.WantsTick())
        {
            HRESULT  hr = m_source->KillTimer (s_kMenuTimerId);

            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnEnterSizeMove / OnSize / OnMove
//
//  A window is placed by more than a drag: the snap layouts, Win+arrow, a
//  monitor going away, an arrangement restored by the shell. All of those
//  arrive as WM_SIZE / WM_MOVE and never enter the OS drag loop, so a hook
//  on the end of that loop alone never hears them.
//
//  Reporting every change is what this used to do, and it was wrong the
//  other way: inside the drag loop the geometry changes once per pixel.
//  So the loop is tracked, and a change outside it is a placement.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::OnEnterSizeMove()
{
    m_inSizeMove = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnSize (UINT widthPx, UINT heightPx)
{
    UNREFERENCED_PARAMETER (widthPx);
    UNREFERENCED_PARAMETER (heightPx);



    if (!m_inSizeMove)
    {
        OnWindowPlaced();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMove (int x, int y)
{
    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);



    if (!m_inSizeMove)
    {
        OnWindowPlaced();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnExitSizeMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::OnExitSizeMove()
{
    m_inSizeMove = false;
    OnWindowPlaced();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDialogTimer
//
//  The timer behind a dialog's tick. Re-arming a timer under the same id
//  replaces it, so a dialog already showing moves to a new cadence at once.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetDialogTimer (UINT ms)
{
    HRESULT  hr = S_OK;



    if (m_source != nullptr)
    {
        hr = m_source->SetTimer (s_kDialogTimerId, ms);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KillDialogTimer
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::KillDialogTimer()
{
    HRESULT  hr = S_OK;



    if (m_source != nullptr)
    {
        hr = m_source->KillTimer (s_kDialogTimerId);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnModalLoopTick
//
//  Fires while the OS runs its modal move / size loop for this window (the user
//  is dragging the caption or a border). The host owns the thread there, so no
//  ordinary frame runs; a panel that wires SetOnModalLoopTick keeps animating
//  by pumping a frame from here instead of freezing until the drag ends.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::OnModalLoopTick()
{
    if (m_onModalLoopTick)
    {
        m_onModalLoopTick();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAcceptsDroppedFiles
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetAcceptsDroppedFiles (bool accepts)
{
    constexpr UINT  kWmCopyGlobalData = 0x0049;
    HWND            hwnd              = GetHwnd();



    if (hwnd == nullptr)
    {
        return;
    }

    DragAcceptFiles (hwnd, accepts ? TRUE : FALSE);

    //  An elevated window would otherwise never see a drop from an ordinary
    //  Explorer: UIPI drops the messages that carry it, with no error.
    if (accepts)
    {
        (void) ChangeWindowMessageFilterEx (hwnd, WM_DROPFILES,      MSGFLT_ALLOW, nullptr);
        (void) ChangeWindowMessageFilterEx (hwnd, WM_COPYDATA,       MSGFLT_ALLOW, nullptr);
        (void) ChangeWindowMessageFilterEx (hwnd, kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDropFiles
//
//  Every path in the drop, then the handle released, whatever the subclass
//  does with them.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnDropFiles (HDROP drop)
{
    std::vector<std::wstring>  paths;
    UINT                       count = DragQueryFileW (drop, 0xFFFFFFFF, nullptr, 0);
    bool                       taken = false;



    for (UINT i = 0; i < count; i++)
    {
        UINT          length = DragQueryFileW (drop, i, nullptr, 0);
        std::wstring  path (length, L'\0');

        DragQueryFileW (drop, i, path.data(), length + 1);
        paths.push_back (std::move (path));
    }

    DragFinish (drop);
    taken = OnFilesDropped (paths);

    return taken ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnClose()
{
    if (m_dialog.IsActive())
    {
        if (!m_dialog.TryTriggerButton (IDCANCEL))
        {
            m_dialog.End (IDCANCEL);
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

void DxuiWindow::OnDestroy()
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
    DxuiMouseEvent     ev;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    ev.kind            = kind;
    ev.button          = button;
    ev.positionDip     = { x, y };
    ev.wheelDelta      = wheelDelta;
    ev.wheelHorizontal = wheelHorizontal;
    ev.shift       = DxuiInputState::IsKeyDown (VK_SHIFT);
    ev.ctrl        = m_wheelCtrl || DxuiInputState::IsKeyDown (VK_CONTROL);
    ev.alt         = DxuiInputState::IsKeyDown (VK_MENU);

    // A modal overlay captures all mouse input so the page beneath it stays
    // inert (clicks outside the overlay dialog are simply ignored).
    if (HasModalOverlay())
    {
        result = DxuiMessageResult::Handled;
        (void) OnOverlayMouse (ev);
    }
    else
    {
        // In a dialog a press focuses the control under it before the control
        // sees the press, so a drop-down or slider already has focus when it
        // handles the click, and Tab continues from it.
        //  A press anywhere but on a pinned info tip's "i" closes the tip.
        if (kind == DxuiMouseEventKind::Down)
        {
            DxuiInfoTip::CloseOpenOutside (ev.positionDip);
        }

        if (kind == DxuiMouseEventKind::Down && m_dialog.IsActive() && m_focus.FocusAtPoint (POINT { x, y }))
        {
            Invalidate();
        }

        //  A move with no button down changes the controls under the pointer,
        //  and the pointer's exit from the window empties them. During a drag
        //  they aren't updated, so the control dragged from stays on the path.
        if (kind == DxuiMouseEventKind::Move && button == DxuiMouseButton::None)
        {
            UpdatePointerPath (POINT { x, y }, false);
        }
        else if (kind == DxuiMouseEventKind::Leave)
        {
            UpdatePointerPath (POINT { x, y }, true);
        }

        //  A right-press on a scrollbar opens the bar's own menu, as Windows'
        //  bars do, before the control or the window has the press.
        if (kind == DxuiMouseEventKind::Down && button == DxuiMouseButton::Right && DxuiPointerTracker::ShowScrollbarMenuIn (*this, GetHwnd(), POINT { x, y }))
        {
            result = DxuiMessageResult::Handled;
        }
        else if ((kind == DxuiMouseEventKind::Down && button == DxuiMouseButton::Right && TryShowTextBoxMenu (ev.positionDip)) || m_inputFilters.RunMouse (ev) || OnMouse (ev))
        {
            // Repaint immediately when a control consumes the event so drags
            // (slider thumbs, scrubbing) and hover states track the cursor every
            // frame instead of only on the ~half-second dialog tick.
            // InvalidateRect coalesces, so at most one paint lands per frame
            // regardless of mouse-move rate.
            result = DxuiMessageResult::Handled;
            Invalidate();
        }

        //  A context menu open in a dialog is ticked from here; otherwise only
        //  an application window ticks one.
        if (m_dialog.IsActive() && m_source != nullptr && m_source->GetContextMenu().IsVisible())
        {
            HRESULT  hr = m_source->SetTimer (s_kMenuTimerId, s_kMenuTickMs);

            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchKey
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::DispatchKey (DxuiKeyEventKind kind, WPARAM code)
{
    DxuiKeyEvent  ev = MakeKeyEvent (kind, code);



    return (OnKey (ev) || RouteMappedKey (ev)) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeKeyEvent
//
//  A key event for the key, with the Shift, Ctrl and Alt keys as they are
//  now.
//
////////////////////////////////////////////////////////////////////////////////

DxuiKeyEvent DxuiWindow::MakeKeyEvent (DxuiKeyEventKind kind, WPARAM vk)
{
    DxuiKeyEvent  ev;



    ev.kind  = kind;
    ev.vk    = vk;
    ev.shift = DxuiInputState::IsKeyDown (VK_SHIFT);
    ev.ctrl  = DxuiInputState::IsKeyDown (VK_CONTROL);
    ev.alt   = DxuiInputState::IsKeyDown (VK_MENU);

    return ev;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteMappedKey
//
//  Key-downs only: a map binds presses, and a release of a mapped key must
//  not run its command a second time.
//
//  The first key of a two-key chord is kept until the next key-down. A
//  second key that completes no chord is consumed with it, as Visual Studio
//  consumes one, so it does not reach a control as a key of its own.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::RouteMappedKey (const DxuiKeyEvent & ev)
{
    int            commandId = 0;
    bool           isHandled = false;
    DxuiKeyMatch   match     = DxuiKeyMatch::None;
    DxuiKeyStroke  stroke    = { ev.vk, ev.ctrl, ev.alt, ev.shift };



    if (m_keyMap == nullptr || ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    match = m_keyMap->Match (stroke, m_keyPrefix, commandId);

    switch (match)
    {
    case DxuiKeyMatch::Command:
        m_keyPrefix.reset();
        isHandled = OnMappedCommand (commandId);
        break;

    case DxuiKeyMatch::Prefix:
        m_keyPrefix = stroke;
        isHandled   = true;
        break;

    case DxuiKeyMatch::Modifier:
        break;

    default:
        isHandled = m_keyPrefix.has_value();
        m_keyPrefix.reset();
        break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshFocusOrder
//
//  Re-walks the visible tree for Tab traversal, and rescues focus if it was
//  left on something that has just become invisible.
//
//  DxuiDialogMode::Begin builds the order once, which is right for a dialog whose
//  contents never change shape. It is wrong the moment part of the tree is
//  shown or hidden afterwards: the hidden controls stay in the order and the
//  newly shown ones are absent from it, so Tab reaches what cannot be seen
//  and misses what can.
//
//  Rebuilding on the visibility change rather than on each Tab keeps the walk
//  on the event that actually invalidates the answer -- page switches are
//  rare, keystrokes are not.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::RefreshFocusOrder (IDxuiControl * fallback)
{
    IDxuiControl *   focused = nullptr;



    m_focus.Rebuild();

    focused = m_focus.GetFocusedControl();

    if (focused != nullptr && !focused->IsVisible())
    {
        m_focus.SetFocused (fallback);
    }
}
