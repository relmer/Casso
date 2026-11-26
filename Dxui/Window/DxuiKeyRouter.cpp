#include "Pch.h"

#include "Window/DxuiKeyRouter.h"
#include "Core/DxuiFocusManager.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Window/DxuiDialogController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::OnKeyDown
//
//  A modal overlay swallows every key, handled or not, so dialog navigation
//  can't reach the page behind it. An open context menu is where the
//  keyboard is, as a Windows menu's is: arrows, Enter, Escape and mnemonics
//  act on it, not the page.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiKeyRouter::OnKeyDown (WPARAM vk)
{
    DxuiMessageResult  result = DxuiMessageResult::Handled;
    DxuiPopupMenu    * menu   = nullptr;



    m_host.BeginKeyBatch();

    if (!m_host.RouteOverlayKey (vk))
    {
        menu = m_host.GetOpenContextMenu();

        if (menu != nullptr)
        {
            (void) menu->OnKey (vk);
        }
        else
        {
            result = m_dialog.IsActive() ? DispatchDialogKey (vk) : DispatchKey (DxuiKeyEventKind::Down, vk);
        }
    }

    m_host.EndKeyBatch();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::OnChar
//
//  A modal overlay takes every character, so none types into a control the
//  user can't see. A dialog's characters go to the focused control only,
//  and with nothing focused the character is left to default processing.
//  Everything else goes through the normal key dispatch.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiKeyRouter::OnChar (wchar_t ch)
{
    DxuiMessageResult  result  = DxuiMessageResult::Handled;
    IDxuiControl     * focused = nullptr;



    m_host.BeginKeyBatch();

    if (m_host.RouteOverlayChar (ch))
    {
        result = DxuiMessageResult::Handled;
    }
    else if (m_dialog.IsActive())
    {
        focused = m_focus.GetFocusedControl();
        result  = (focused != nullptr && focused->OnChar (ch)) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
    }
    else
    {
        result = DispatchKey (DxuiKeyEventKind::Char, ch);
    }

    m_host.EndKeyBatch();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::ProcessDialogMessage
//
//  A message aimed at another window is never taken.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiKeyRouter::ProcessDialogMessage (const MSG & msg)
{
    HWND  hwnd   = m_host.GetKeyHwnd();
    bool  isOurs = m_dialog.IsActive() && hwnd != nullptr && msg.hwnd == hwnd;
    bool  isNav  = msg.wParam == VK_TAB || msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE;



    if (!isOurs || !isNav || (msg.message != WM_KEYDOWN && msg.message != WM_SYSKEYDOWN))
    {
        return false;
    }

    return DispatchDialogKey (msg.wParam) == DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::DispatchKey
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiKeyRouter::DispatchKey (DxuiKeyEventKind kind, WPARAM code)
{
    DxuiKeyEvent  ev;



    ev.kind  = kind;
    ev.vk    = code;
    ev.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt   = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    return m_host.RouteKeyToControls (ev) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::DispatchDialogKey
//
//  A modal overlay has every key, Tab, Enter and Escape included. The
//  modeless path through ProcessDialogMessage arrives here without passing
//  OnKeyDown, so the overlay is checked here too, or Tab walks the focus of
//  the page hidden behind it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiKeyRouter::DispatchDialogKey (WPARAM vk)
{
    bool  shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    bool  ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;



    if (m_host.RouteOverlayKey (vk))
    {
        return DxuiMessageResult::Handled;
    }

    return m_dialog.HandleKey (vk, shift, ctrl) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}
