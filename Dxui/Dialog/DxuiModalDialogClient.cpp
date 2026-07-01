#include "Pch.h"

#include "Dialog/DxuiModalDialogClient.h"
#include "Dialog/DxuiDialog.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::Bind
//
//  Binds the client to the hosted dialog and seeds the result with the
//  cancel code, so an unresolved close (owner-forced teardown) still
//  returns a sane value.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiModalDialogClient::Bind (DxuiDialog * dialog, int cancelResult)
{
    m_dialog       = dialog;
    m_cancelResult = cancelResult;
    m_result       = cancelResult;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::SetupFocus
//
//  Attaches the focus manager to the (built, hosted) dialog and puts
//  initial focus on the dialog's requested control (e.g. a picker's
//  search box) so typing / Tab work from the moment it opens.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiModalDialogClient::SetupFocus (DxuiDialog * dialog, const IDxuiTheme * theme)
{
    m_focus.SetTheme (theme);
    m_focus.Attach   (dialog);
    m_focus.Rebuild  ();

    if (dialog != nullptr && dialog->InitialFocus() != nullptr)
    {
        m_focus.SetFocused (dialog->InitialFocus());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::Resolve
//
//  Records the chosen return code once; the modal pump polls Done() and
//  exits with Result().
//
////////////////////////////////////////////////////////////////////////////////

void DxuiModalDialogClient::Resolve (int returnCode)
{
    if (!m_done)
    {
        m_done   = true;
        m_result = returnCode;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnKeyDown
//
//  Tab / Shift+Tab move focus; Escape activates the cancel button (or
//  resolves the cancel result directly when none is marked); other keys
//  route to the focused control, with Enter falling back to the default
//  button.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    bool  shift   = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    bool  handled = false;


    UNREFERENCED_PARAMETER (lParam);

    if (m_dialog != nullptr)
    {
        if (vk == VK_TAB)
        {
            handled = m_focus.HandleKey (shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
        }
        else if (vk == VK_ESCAPE)
        {
            std::optional<int>  rc = m_dialog->ActivateCancel();

            handled = rc.has_value();

            if (!handled)
            {
                Resolve (m_cancelResult);
                handled = true;
            }
        }
        else
        {
            handled = RouteKeyToFocused (vk, shift);

            if (!handled && vk == VK_RETURN)
            {
                std::optional<int>  rc = m_dialog->ActivateDefault();

                handled = rc.has_value();
            }
        }
    }

    return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnChar
//
//  Forwards typed characters to the focused control (e.g. a search box).
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnChar (WPARAM ch, LPARAM lParam)
{
    IDxuiControl *  focused = m_focus.Focused();
    bool            handled = false;


    UNREFERENCED_PARAMETER (lParam);

    if (focused != nullptr)
    {
        handled = focused->OnChar ((wchar_t) ch);
    }

    return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnClose
//
//  A window-close gesture (caption X / Alt+F4) activates the cancel
//  button, or resolves the cancel result directly when none is marked.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnClose ()
{
    std::optional<int>  rc = std::nullopt;


    if (m_dialog != nullptr)
    {
        rc = m_dialog->ActivateCancel();
    }

    if (!rc.has_value())
    {
        Resolve (m_cancelResult);
    }

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnGetMinMax
//
//  For resizable dialogs, clamps the minimum track size to the DPI-scaled
//  minimum client size plus the live non-client overhead. No-op (and thus
//  NotHandled) when no minimum was set, leaving fixed-size dialogs alone.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnGetMinMax (MINMAXINFO * info)
{
    HRESULT            hr       = S_OK;
    DxuiMessageResult  result   = DxuiMessageResult::NotHandled;
    RECT               rcClient = {};
    RECT               rcWindow = {};
    UINT               dpi      = USER_DEFAULT_SCREEN_DPI;
    int                minCW    = 0;
    int                minCH    = 0;
    int                ncW      = 0;
    int                ncH      = 0;


    BAIL_OUT_IF (info == nullptr || m_hwnd == nullptr, S_OK);
    BAIL_OUT_IF (m_minClientSizeDip.cx <= 0 && m_minClientSizeDip.cy <= 0, S_OK);

    dpi   = GetDpiForWindow (m_hwnd);
    minCW = MulDiv (m_minClientSizeDip.cx, (int) dpi, USER_DEFAULT_SCREEN_DPI);
    minCH = MulDiv (m_minClientSizeDip.cy, (int) dpi, USER_DEFAULT_SCREEN_DPI);

    // Translate the client floor to a window floor via the live NC
    // overhead (non-fatal if either query fails -- overhead stays 0).
    if (GetClientRect (m_hwnd, &rcClient) && GetWindowRect (m_hwnd, &rcWindow))
    {
        ncW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
        ncH = (rcWindow.bottom - rcWindow.top)  - (rcClient.bottom - rcClient.top);
    }

    info->ptMinTrackSize.x = minCW + ncW;
    info->ptMinTrackSize.y = minCH + ncH;
    result                 = DxuiMessageResult::Handled;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    DxuiMouseButton  button = ((wParam & MK_LBUTTON) != 0) ? DxuiMouseButton::Left
                                                           : DxuiMouseButton::None;

    return RouteMouse (DxuiMouseEventKind::Move, button, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnLButtonDown
//
//  Routes the press into the control tree and, if consumed, captures the
//  mouse so a drag (scrollbar thumb, column divider, text selection) keeps
//  tracking outside the client rect.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  routed = RouteMouse (DxuiMouseEventKind::Down, DxuiMouseButton::Left, lParam);


    UNREFERENCED_PARAMETER (wParam);

    if (routed == DxuiMessageResult::Handled && m_hwnd != nullptr)
    {
        SetCapture (m_hwnd);
    }

    return routed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  routed = RouteMouse (DxuiMouseEventKind::Up, DxuiMouseButton::Left, lParam);


    UNREFERENCED_PARAMETER (wParam);

    if (m_hwnd != nullptr && GetCapture() == m_hwnd)
    {
        ReleaseCapture();
    }

    return routed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnMouseWheel
//
//  Converts the screen point to client, maps a horizontal wheel / swipe to
//  the list's Shift+wheel horizontal-scroll model (with the sign flipped:
//  Win32 +right vs the list's +delta = scroll-left), and repaints on a
//  handled notch.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    HRESULT            hr      = S_OK;
    DxuiMessageResult  result  = DxuiMessageResult::NotHandled;
    DxuiMouseEvent     ev;
    POINT              pt      = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
    float              delta   = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;
    bool               handled = false;


    BAIL_OUT_IF (m_dialog == nullptr, S_OK);

    if (m_hwnd != nullptr)
    {
        ScreenToClient (m_hwnd, &pt);
    }

    ev.kind        = DxuiMouseEventKind::Wheel;
    ev.positionDip = pt;
    ev.wheelDelta  = horizontal ? -delta : delta;
    ev.shift       = horizontal || ((GetKeyState (VK_SHIFT) & 0x8000) != 0);
    handled        = m_dialog->OnMouse (ev);

    if (handled && m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }

    result = handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::OnTimer
//
//  Drives the dialog's periodic tick (download poller, caret-blink
//  repaint). The host repaints after this returns Handled.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::OnTimer (UINT_PTR timerId)
{
    UNREFERENCED_PARAMETER (timerId);

    if (m_dialog != nullptr)
    {
        m_dialog->Tick();
    }

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::RouteKeyToFocused
//
//  Forwards a key-down to the focused control as a DxuiKeyEvent.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiModalDialogClient::RouteKeyToFocused (WPARAM vk, bool shift)
{
    HRESULT         hr      = S_OK;
    bool            result  = false;
    IDxuiControl *  focused = m_focus.Focused();
    DxuiKeyEvent    ke;


    BAIL_OUT_IF (focused == nullptr, S_OK);

    ke.kind  = DxuiKeyEventKind::Down;
    ke.vk    = vk;
    ke.shift = shift;
    result   = focused->OnKey (ke);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient::RouteMouse
//
//  Builds a DxuiMouseEvent from the client-coordinate lParam and dispatches
//  it into the dialog's control tree. Interactive content (list selection /
//  hover, column-resize + scrollbar drags, search-box caret) mutates on
//  mouse input but the host only repaints after keyboard / timer messages,
//  so a handled event invalidates the window here to show the change.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiModalDialogClient::RouteMouse (DxuiMouseEventKind kind, DxuiMouseButton button, LPARAM lParam)
{
    DxuiMouseEvent  ev;
    bool            handled = false;


    if (m_dialog != nullptr)
    {
        ev.kind        = kind;
        ev.button      = button;
        ev.positionDip = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
        handled        = m_dialog->OnMouse (ev);
    }

    if (handled && m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }

    return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}
