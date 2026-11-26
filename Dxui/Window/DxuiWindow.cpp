#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiWindowFrame.h"
#include "Widgets/DxuiInfoTip.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiContextMenu.h"
#include "Widgets/DxuiTextInput.h"


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

    m_sizer.SetMinSizeDip (params.minSizeDip);
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

    m_sizer.SetMinSizeDip (params.minSizeDip);
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

    return m_mouse.OnButton (DxuiMouseEventKind::Down, DxuiMouseButton::Left, lParam);
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

    return m_mouse.OnButton (DxuiMouseEventKind::Up, DxuiMouseButton::Left, lParam);
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

    return m_mouse.OnButton (DxuiMouseEventKind::Down, DxuiMouseButton::Right, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryShowTextBoxMenu
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::TryShowTextBoxMenu (POINT clientPx)
{
    DxuiTextInput  * box = DxuiMouseRouter::FindTextBoxAt (*this, clientPx);



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
//  OnRButtonUp
//
//  The release too, for a context menu that opens as the button comes up, as
//  Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnRButtonUp (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);

    return m_mouse.OnButton (DxuiMouseEventKind::Up, DxuiMouseButton::Right, lParam);
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



    return m_mouse.OnButton (DxuiMouseEventKind::Up, button, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    return m_mouse.OnMove (wParam, lParam);
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
    m_mouse.SendMouseLeave();
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseWheel
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    return m_mouse.OnWheel (wParam, lParam, horizontal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    return m_keys.OnKeyDown (vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnChar (WPARAM ch, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    return m_keys.OnChar ((wchar_t) ch);
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
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnGetMinMax (MINMAXINFO * info)
{
    return (info != nullptr && m_sizer.GetMinMax (*info)) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
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



    if (m_dialogRunner.OnTimer (timerId))
    {
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
//  OnEnterSizeMove
//
//  A window is placed by more than a drag: the snap layouts, Win+arrow, a
//  monitor going away, an arrangement restored by the shell. Those arrive as
//  WM_SIZE and WM_MOVE outside the OS drag loop, and each is a placement;
//  inside the loop only its end is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::OnEnterSizeMove()
{
    m_sizer.BeginSizeMove();
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

    m_sizer.NoteGeometry();
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

    m_sizer.NoteGeometry();
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnExitSizeMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::OnExitSizeMove()
{
    m_sizer.EndSizeMove();
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





////////////////////////////////////////////////////////////////////////////////
//
//  RouteOverlayMouse
//
//  A modal overlay has all mouse input, so the page beneath it stays inert.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::RouteOverlayMouse (const DxuiMouseEvent & ev)
{
    if (!HasModalOverlay())
    {
        return false;
    }

    (void) OnOverlayMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusPressedControl
//
//  In a dialog a press focuses the control under it before the control sees
//  the press.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::FocusPressedControl (POINT clientPx)
{
    if (m_dialog.IsActive() && m_focus.FocusAtPoint (clientPx))
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickDialogMenu
//
//  A context menu open in a dialog is ticked by the window's menu timer;
//  otherwise only an application window ticks one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::TickDialogMenu()
{
    HRESULT  hr = S_OK;



    if (m_dialog.IsActive() && m_source != nullptr && m_source->GetContextMenu().IsVisible())
    {
        hr = m_source->SetTimer (s_kMenuTimerId, s_kMenuTickMs);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteOverlayKey
//
//  The overlay's own result is discarded: handled or not, the key stops at
//  the overlay.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::RouteOverlayKey (WPARAM vk)
{
    if (!HasModalOverlay())
    {
        return false;
    }

    (void) OnOverlayKey (vk);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteOverlayChar
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindow::RouteOverlayChar (wchar_t ch)
{
    if (!HasModalOverlay())
    {
        return false;
    }

    (void) OnOverlayChar (ch);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOpenContextMenu
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenu * DxuiWindow::GetOpenContextMenu()
{
    if (m_source == nullptr || !m_source->GetContextMenu().IsVisible())
    {
        return nullptr;
    }

    return &m_source->GetContextMenu();
}
