#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiWindowSizeMath.h"
#include "Widgets/DxuiInfoTip.h"
#include "Widgets/DxuiPopupMenu.h"


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

    hostParams.title                 = params.title;
    hostParams.hInstance             = params.hInstance;
    hostParams.ownerHwnd             = params.ownerHwnd;
    hostParams.borderless            = true;
    hostParams.resizable             = params.resizable;
    hostParams.roundedCorners        = true;
    hostParams.darkMode              = true;
    hostParams.backdrop              = !params.micaBackdrop ? DxuiHwndSourceBackdrop::None
                                     : params.micaAlt       ? DxuiHwndSourceBackdrop::MicaAlt
                                                            : DxuiHwndSourceBackdrop::Mica;
    hostParams.drawOverBackdrop      = params.micaBackdrop;
    hostParams.createSwapChain       = true;
    hostParams.captionStyle          = params.captionStyle;
    hostParams.composited            = params.composited;
    hostParams.classNameOverride     = params.classNameOverride;
    hostParams.initialSizeDip        = params.initialSizeDip;
    hostParams.insetRootBelowCaption = params.insetContentBelowCaption;
    hostParams.appIconBig            = params.appIconBig;
    hostParams.appIconSmall          = params.appIconSmall;
    hostParams.presentSyncInterval   = params.presentSyncInterval;
    hostParams.paceFrames            = params.paceFrames;
    hostParams.createNoActivate      = params.createNoActivate;
    hostParams.placement             = params.placement;
    hostParams.placementAnchorHwnd   = params.placementAnchorHwnd;
    hostParams.placementAnchorRectPx = params.placementAnchorRectPx;
    hostParams.fitToWorkArea         = params.fitToWorkArea;

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

    m_dialog.Begin (m_theme, defaultButtonId, true, m_initialFocus);

    m_source->SetTimer (s_kDialogTimerId, m_dialogTickMs);

    if (m_ownerHwnd != nullptr)
    {
        EnableWindow (m_ownerHwnd, FALSE);
        ownerDisabled = true;
    }

    OnDialogShowing();
    Show();

    while (!m_dialog.IsModalDone())
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

    result = m_dialog.GetModalResult();

Error:
    if (ownerDisabled)
    {
        EnableWindow (m_ownerHwnd, TRUE);

        //  When the active dialog is hidden, Windows chooses the next active
        //  window, which after an Alt+Tab away and back can be another
        //  program's. The owner is activated first, as DialogBox does.
        if (GetActiveWindow() == GetHwnd())
        {
            SetActiveWindow (m_ownerHwnd);
        }
    }

    if (m_source != nullptr)
    {
        m_source->KillTimer (s_kDialogTimerId);
    }

    m_dialog.Deactivate();
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

void DxuiWindow::ShowModelessDialog (int defaultButtonId, bool activate)
{
    if (m_source != nullptr)
    {
        m_dialog.Begin (m_theme, defaultButtonId, false, m_initialFocus);

        m_source->SetTimer (s_kDialogTimerId, m_dialogTickMs);

        OnDialogShowing();
        Show (activate);
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
    HWND  hwnd      = GetHwnd();



    // Only intercept the dialog-navigation keys while a dialog is active and
    // the message targets THIS window; otherwise fall through to normal
    // dispatch. A message aimed at a sibling window must not be swallowed.
    bool  isOurs = m_dialog.IsActive() && hwnd != nullptr && msg.hwnd == hwnd;

    if (isOurs)
    {
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
    HWND                 hwnd   = GetHwnd();
    DxuiDialogEndAction  action = m_dialog.End (result);



    if (action == DxuiDialogEndAction::ExitModalLoop)
    {
        if (hwnd != nullptr)
        {
            PostMessageW (hwnd, WM_NULL, 0, 0);
        }
    }
    else if (action == DxuiDialogEndAction::CloseModeless)
    {
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
//  Each paint waits for the next vertical blank to present, so a mouse event
//  whose controls invalidate three times would wait three frames. During a
//  paint batch, such as a mouse event, the window is only marked, and it is
//  painted once, when the batch ends. Windows sends WM_PAINT only once no input is queued, so a hover
//  drawn on a move would trail the pointer while it moves.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::Invalidate()
{
    HWND  hwnd = GetHwnd();



    if (hwnd != nullptr)
    {
        InvalidateRect (hwnd, nullptr, FALSE);

        if (m_batchDepth == 0)
        {
            UpdateWindow (hwnd);
        }
        else
        {
            m_paintAfterBatch = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginPaintBatch
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::BeginPaintBatch()
{
    m_batchDepth++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndPaintBatch
//
//  The window is painted once if it was invalidated in the batch.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::EndPaintBatch()
{
    m_batchDepth--;

    if (m_batchDepth == 0 && m_paintAfterBatch)
    {
        m_paintAfterBatch = false;

        if (GetHwnd() != nullptr)
        {
            UpdateWindow (GetHwnd());
        }
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
//  SetAppIcon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetAppIcon (HICON icon)
{
    DxuiIconImage  image;
    HRESULT        hr    = S_OK;



    if (icon == nullptr || GetHwnd() == nullptr)
    {
        return;
    }

    SendMessageW (GetHwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM> (icon));
    SendMessageW (GetHwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM> (icon));

    hr = DxuiIconImage::FromHicon (icon, kCaptionIconPx, image);

    if (SUCCEEDED (hr))
    {
        m_source->SetCaptionIcon (std::move (image.bgraPremul), image.width, image.height);
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
//  SetTitleChips
//
//  Chips drawn after the title in the caption, such as "Read as CP/M".
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetTitleChips (const std::vector<DxuiTitleChip> & chips)
{
    if (m_source != nullptr)
    {
        m_source->SetTitleChips (chips);
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
//  The pointer left the client area, off the window or onto its frame. The
//  panel tree is sent DxuiMouseEventKind::Leave, so hover states and tips
//  over the content end however fast the pointer went; the last move may
//  have been over a row. Not while the window has the capture, so a drag
//  continues until its release. NotHandled, so DefWindowProc is called too.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseLeave()
{
    HWND  hwnd = GetHwnd();



    if (hwnd != nullptr && GetCapture() != hwnd)
    {
        (void) DispatchMouse (DxuiMouseEventKind::Leave, DxuiMouseButton::None, SHRT_MIN, SHRT_MIN, 0.0f);
    }

    return DxuiMessageResult::NotHandled;
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

    BeginPaintBatch();

    // A modal overlay swallows every key (routing the ones it wants to its own
    // handler) so dialog navigation can't leak to the page behind it. The
    // overlay's own return is discarded on purpose: handled or not, the key
    // stops here.
    if (HasModalOverlay())
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
        result = m_dialog.IsActive() ? DispatchDialogKey (vk)
                                : DispatchKey (DxuiKeyEventKind::Down, vk);
    }

    EndPaintBatch();

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

    BeginPaintBatch();

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

    EndPaintBatch();

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
    HWND               hwnd       = GetHwnd();



    if (hitTest == HTCLIENT && hwnd != nullptr && GetCursorPos (&cursor) != FALSE)
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
//  reports, never below the minimum. Borderless, so client size and
//  window size coincide.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnGetMinMax (MINMAXINFO * info)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;
    SIZE               maxPx  = {};
    bool               hasMax = false;
    bool               isSet  = false;



    if (info == nullptr)
    {
        return result;
    }

    hasMax = TryGetMaxTrackSizePx (maxPx);
    isSet  = DxuiWindowSizeMath::TryApplyTrackLimits (m_minSizeDip, GetDpi(), hasMax, maxPx, *info);

    if (isSet)
    {
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResizeToMaxSize
//
//  Fit sizes the window to TryGetMaxClientSizePx within its monitor's work
//  area, and places it there by its creation-time placement. It is called
//  before the window is first shown, once its content has been laid out.
//
//  Grow grows the window toward TryGetMaxClientSizePx within the work area,
//  and leaves it alone when the content fits.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::ResizeToMaxSize (MaxSizeMode mode)
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

    done = GetWindowRect (hwnd, &rect);
    CWRA (done);

    monitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
    done    = GetMonitorInfoW (monitor, &info);
    CWRA (done);

    if (mode == MaxSizeMode::Grow)
    {
        grows = DxuiWindowSizeMath::TryGetGrownRect (rect, maxPx, info.rcWork, fitted);
        BAIL_OUT_IF (!grows, S_OK);
    }
    else
    {
        fitted = DxuiWindowSizeMath::FitRectToMaxSize (rect, maxPx, info.rcWork);

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
//  SetDialogTickIntervalMs
//
//  Re-arming a timer under the same id replaces it, so a dialog already
//  showing moves to the new cadence at once.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::SetDialogTickIntervalMs (UINT ms)
{
    HRESULT  hr = S_OK;



    m_dialogTickMs = ms;

    if (m_dialog.IsActive() && m_source != nullptr)
    {
        hr = m_source->SetTimer (s_kDialogTimerId, m_dialogTickMs);
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
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnClose()
{
    if (m_dialog.IsActive())
    {
        if (!m_dialog.TryClickButton (IDCANCEL))
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
    ev.shift       = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl        = m_wheelCtrl || (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt         = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    BeginPaintBatch();

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
        if ((kind == DxuiMouseEventKind::Move && button == DxuiMouseButton::None) || kind == DxuiMouseEventKind::Leave)
        {
            if (m_pointerPath.Update (*this, POINT { x, y }, kind == DxuiMouseEventKind::Leave))
            {
                Invalidate();
            }
        }

        //  A right-press on a scrollbar opens the bar's own menu, as Windows'
        //  bars do, before the control or the window has the press.
        if (kind == DxuiMouseEventKind::Down && button == DxuiMouseButton::Right && ShowScrollbarMenuIn (*this, GetHwnd(), POINT { x, y }))
        {
            result = DxuiMessageResult::Handled;
        }
        else if (OnMouse (ev))
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

    EndPaintBatch();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowScrollbarMenuIn
//
//  The control itself first, then the visible child under the point, down
//  to the control whose bar it is on. A container that clips its children
//  there, as a scrolled page does, is not searched below.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::ShowScrollbarMenuIn (IDxuiControl & control, HWND hwnd, POINT clientPx)
{
    bool    shown = control.ShowScrollbarMenu (hwnd, clientPx);
    size_t  count = control.GetChildCount();



    for (size_t i = 0; !shown && !control.IsPointClipped (clientPx) && i < count; i++)
    {
        IDxuiControl  * child = control.GetChild (i);
        RECT            rc    = (child != nullptr) ? child->GetBounds() : RECT {};

        if (child != nullptr && child->IsVisible() && PtInRect (&rc, clientPx))
        {
            shown = ShowScrollbarMenuIn (*child, hwnd, clientPx);
        }
    }

    return shown;
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
//  Dialog key routing (modal or modeless), after the modal overlay has had
//  the key: the dialog controller handles the rest.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::DispatchDialogKey (WPARAM vk)
{
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    bool               shift     = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    bool               ctrl      = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    bool               isHandled = false;



    // A modal overlay owns every key, Tab / Enter / Escape included. The
    // modeless pre-translate path (ProcessDialogMessage) arrives here without
    // passing through OnKeyDown, so the check has to live here too, or Tab
    // walks the focus of the page hidden behind the overlay.
    if (HasModalOverlay())
    {
        (void) OnOverlayKey (vk);
        return DxuiMessageResult::Handled;
    }

    isHandled = m_dialog.HandleKey (vk, shift, ctrl);
    result    = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshFocusOrder
//
//  Re-walks the visible tree for Tab traversal, and rescues focus if it was
//  left on something that has just become invisible.
//
//  Entering dialog mode builds the order once, which is right for a dialog whose
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





