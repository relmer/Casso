#include "Pch.h"

#include "Window/DxuiKeyRouter.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiInputState.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Window/DxuiDialogController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::OnKeyDown
//
//  A filter that keeps a key, such as Escape canceling a drag under way,
//  comes first. A modal overlay swallows every key, handled or not, so
//  dialog navigation can't reach the page behind it. An open context menu
//  is where the keyboard is, as a Windows menu's is.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiKeyRouter::OnKeyDown (WPARAM vk)
{
    DxuiMessageResult  result = DxuiMessageResult::Handled;
    DxuiPopupMenu    * menu   = nullptr;



    m_host.BeginKeyBatch();

    if (!m_host.RouteFilteredKey (vk) && !m_host.RouteOverlayKey (vk))
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
    DxuiKeyEvent  ev = DxuiInputState::MakeKeyEvent (kind, code);



    return (m_host.RouteKeyToControls (ev) || RouteMappedKey (ev)) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
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
    bool  shift = DxuiInputState::IsKeyDown (VK_SHIFT);
    bool  ctrl  = DxuiInputState::IsKeyDown (VK_CONTROL);



    if (m_host.RouteOverlayKey (vk))
    {
        return DxuiMessageResult::Handled;
    }

    return m_dialog.HandleKey (vk, shift, ctrl) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter::RouteMappedKey
//
//  Key-downs only: a map binds presses, and a release of a mapped key must
//  not run its command a second time.
//
//  The first key of a two-key chord is kept until the next key-down. A
//  second key that completes no chord is consumed with it, as Visual Studio
//  consumes one, so it does not reach a control as a key of its own.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiKeyRouter::RouteMappedKey (const DxuiKeyEvent & ev)
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
        isHandled = m_host.RunMappedCommand (commandId);
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
