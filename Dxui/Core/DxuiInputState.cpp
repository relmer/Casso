#include "Pch.h"

#include "Core/DxuiInputState.h"
#include "Core/DxuiEvents.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputState readers
//
//  Empty until a test sets one; each call is then the Win32 call.
//
////////////////////////////////////////////////////////////////////////////////

DxuiInputState::KeyReader     DxuiInputState::s_readKey;
DxuiInputState::KeyReader     DxuiInputState::s_readAsyncKey;
DxuiInputState::CursorReader  DxuiInputState::s_readCursor;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputState::MakeKeyEvent
//
////////////////////////////////////////////////////////////////////////////////

DxuiKeyEvent DxuiInputState::MakeKeyEvent (DxuiKeyEventKind kind, WPARAM vk)
{
    DxuiKeyEvent  ev;



    ev.kind  = kind;
    ev.vk    = vk;
    ev.shift = IsKeyDown (VK_SHIFT);
    ev.ctrl  = IsKeyDown (VK_CONTROL);
    ev.alt   = IsKeyDown (VK_MENU);

    return ev;
}