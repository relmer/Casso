#include "Pch.h"

#include "Window/DxuiMouseRouter.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiInfoTip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter::OnButton
//
//  A press or release at the message's client point.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiMouseRouter::OnButton (DxuiMouseEventKind kind, DxuiMouseButton button, LPARAM lParam)
{
    return Dispatch (kind, button, (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam), 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter::OnMove
//
//  WM_MOUSEMOVE has the held-button state in wParam (MK_LBUTTON). A move
//  with the button held is reported with Left, so a scrollbar thumb or
//  selection drag doesn't read its first move as a release.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiMouseRouter::OnMove (WPARAM wParam, LPARAM lParam)
{
    DxuiMouseButton  button = (wParam & MK_LBUTTON) ? DxuiMouseButton::Left : DxuiMouseButton::None;



    return OnButton (DxuiMouseEventKind::Move, button, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter::OnWheel
//
//  WM_MOUSEWHEEL has the point in screen coordinates; it is converted to the
//  client space the controls hit-test in.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiMouseRouter::OnWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    HWND               hwnd   = m_host.GetMouseHwnd();
    POINT              point  = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    float              notch  = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    if (hwnd != nullptr)
    {
        ScreenToClient (hwnd, &point);
    }

    //  The message's flags are the keys as they were when the wheel turned.
    m_wheelCtrl = (GET_KEYSTATE_WPARAM (wParam) & MK_CONTROL) != 0;

    result = Dispatch (DxuiMouseEventKind::Wheel, DxuiMouseButton::None, point.x, point.y, notch, horizontal);

    m_wheelCtrl = false;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter::OnLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMouseRouter::OnLeave()
{
    HWND  hwnd = m_host.GetMouseHwnd();



    if (hwnd != nullptr && GetCapture() != hwnd)
    {
        (void) Dispatch (DxuiMouseEventKind::Leave, DxuiMouseButton::None, SHRT_MIN, SHRT_MIN, 0.0f);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter::Dispatch
//
//  A client-pixel DxuiMouseEvent with the live modifier state, routed to the
//  modal overlay when one is up, and otherwise to the control tree.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiMouseRouter::Dispatch (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta, bool wheelHorizontal)
{
    DxuiMouseEvent     ev;
    DxuiMessageResult  result  = DxuiMessageResult::NotHandled;
    bool               isRight = false;



    ev.kind            = kind;
    ev.button          = button;
    ev.positionDip     = { x, y };
    ev.wheelDelta      = wheelDelta;
    ev.wheelHorizontal = wheelHorizontal;
    ev.shift           = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl            = m_wheelCtrl || (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt             = (GetKeyState (VK_MENU)    & 0x8000) != 0;
    isRight            = kind == DxuiMouseEventKind::Down && button == DxuiMouseButton::Right;

    m_host.BeginMouseBatch();

    // A modal overlay captures all mouse input so the page beneath it stays
    // inert (clicks outside the overlay dialog are simply ignored).
    if (m_host.RouteOverlayMouse (ev))
    {
        result = DxuiMessageResult::Handled;
    }
    else
    {
        //  A press anywhere but on a pinned info tip's "i" closes the tip. In
        //  a dialog a press focuses the control under it before the control
        //  sees the press, so a drop-down or slider already has focus when it
        //  handles the click, and Tab continues from it.
        if (kind == DxuiMouseEventKind::Down)
        {
            DxuiInfoTip::CloseOpenOutside (ev.positionDip);
            m_host.FocusPressedControl (POINT { x, y });
        }

        //  A move with no button down changes the controls under the pointer,
        //  and the pointer's exit from the window empties them. During a drag
        //  they aren't updated, so the control dragged from stays on the path.
        if ((kind == DxuiMouseEventKind::Move && button == DxuiMouseButton::None) || kind == DxuiMouseEventKind::Leave)
        {
            if (m_pointerPath.Update (m_host.GetMouseRoot(), POINT { x, y }, kind == DxuiMouseEventKind::Leave))
            {
                m_host.InvalidateForMouse();
            }
        }

        //  A right-press on a scrollbar opens the bar's own menu, as Windows'
        //  bars do, before the control or the window has the press.
        if (isRight && ShowScrollbarMenuIn (m_host.GetMouseRoot(), m_host.GetMouseHwnd(), POINT { x, y }))
        {
            result = DxuiMessageResult::Handled;
        }
        else if (m_host.GetMouseRoot().OnMouse (ev))
        {
            // A consumed event repaints at once, so drags and hover states
            // track the cursor every frame instead of only on the dialog
            // tick. Invalidation coalesces, so at most one paint lands per
            // frame whatever the mouse-move rate.
            result = DxuiMessageResult::Handled;
            m_host.InvalidateForMouse();
        }

        m_host.TickDialogMenu();
    }

    m_host.EndMouseBatch();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter::ShowScrollbarMenuIn
//
//  The control itself first, then the visible child under the point, down
//  to the control whose bar it is on. A container that clips its children
//  there, as a scrolled page does, is not searched below.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMouseRouter::ShowScrollbarMenuIn (IDxuiControl & control, HWND hwnd, POINT clientPx)
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
