#include "Pch.h"

#include "DxuiWindow.h"

#include "Core/DxuiEvents.h"
#include "Core/DxuiIconImage.h"
#include "Widgets/DxuiInfoTip.h"
#include "Widgets/DxuiPopupMenu.h"


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

    m_ownerHwnd = params.ownerHwnd;
    m_sizer.SetMinSizeDip (params.minSizeDip);

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
//  NotHandled, so DefWindowProc is called too.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiWindow::OnMouseLeave()
{
    m_mouse.OnLeave();

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
//  RouteOverlayMouse
//
//  A modal overlay captures all mouse input so the page beneath it stays
//  inert (clicks outside the overlay dialog are simply ignored).
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
//  A context menu open in a dialog is ticked from here; otherwise only an
//  application window ticks one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindow::TickDialogMenu()
{
    if (m_dialog.IsActive() && m_source != nullptr && m_source->GetContextMenu().IsVisible())
    {
        HRESULT  hr = m_source->SetTimer (s_kMenuTimerId, s_kMenuTickMs);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteOverlayKey
//
//  A modal overlay swallows every key, routing the ones it wants to its own
//  handler. The overlay's own return is discarded on purpose: handled or
//  not, the key stops here.
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
//  A modal overlay has text entry, as the color picker's hex field does.
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
    DxuiHwndSource  * host = GetPopupHost();



    return (host != nullptr && host->GetContextMenu().IsVisible()) ? &host->GetContextMenu() : nullptr;
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





