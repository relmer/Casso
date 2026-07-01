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
    m_focus.Rebuild();

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
    HRESULT            hr        = S_OK;
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    bool               shift     = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    bool               isHandled = false;


    UNREFERENCED_PARAMETER (lParam);

    BAIL_OUT_IF (m_dialog == nullptr, S_OK);

    switch (vk)
    {
        case VK_TAB:
            isHandled = m_focus.HandleKey (shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
            break;

        case VK_ESCAPE:
            //  Escape is always consumed: fire the cancel button if one
            //  exists, else resolve the window-level cancel result directly.
            if (!m_dialog->TriggerCancel())
            {
                Resolve (m_cancelResult);
            }
            isHandled = true;
            break;

        case VK_RETURN:
            //  Let the focused control claim Enter first (e.g. a text field);
            //  only fire the default button when it declines.
            isHandled = RouteKeyToFocused (vk, shift) || m_dialog->TriggerDefault();
            break;

        default:
            isHandled = RouteKeyToFocused (vk, shift);
            break;
    }

    result = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

Error:
    return result;
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
    HRESULT            hr        = S_OK;
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    IDxuiControl *     focused   = m_focus.Focused();
    bool               isHandled = false;


    UNREFERENCED_PARAMETER (lParam);

    BAIL_OUT_IF (focused == nullptr, S_OK);

    isHandled = focused->OnChar ((wchar_t) ch);
    result    = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

Error:
    return result;
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
    bool  isHandled = false;



    if (m_dialog != nullptr)
    {
        isHandled = m_dialog->TriggerCancel();
    }

    if (!isHandled)
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
    HRESULT            hr        = S_OK;
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    DxuiMouseEvent     ev;
    POINT              pt        = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
    float              delta     = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;
    bool               isHandled = false;



    BAIL_OUT_IF (m_dialog == nullptr, S_OK);

    if (m_hwnd != nullptr)
    {
        ScreenToClient (m_hwnd, &pt);
    }

    ev.kind        = DxuiMouseEventKind::Wheel;
    ev.positionDip = pt;
    ev.wheelDelta  = horizontal ? -delta : delta;
    ev.shift       = horizontal || ((GetKeyState (VK_SHIFT) & 0x8000) != 0);
    isHandled      = m_dialog->OnMouse (ev);

    if (isHandled && m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }

    result = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

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
//  Forwards a key-down to the focused control as a DxuiKeyEvent. Returns
//  true iff a control has focus and consumed the key; false when nothing
//  is focused or the focused control declined it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiModalDialogClient::RouteKeyToFocused (WPARAM vk, bool shift)
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
    bool            isHandled = false;



    if (m_dialog != nullptr)
    {
        ev.kind        = kind;
        ev.button      = button;
        ev.positionDip = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
        isHandled      = m_dialog->OnMouse (ev);
    }

    if (isHandled && m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }

    return isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}
