#include "Pch.h"

#include "DxuiInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshModifiers
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInput::RefreshModifiers()
{
    m_mods.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    m_mods.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    m_mods.alt   = (GetKeyState (VK_MENU)    & 0x8000) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Translate
//
//  Converts a single Win32 message into a typed `DxuiEvent` and queues it
//  for later pop by the shell's per-frame tick. Returns true when the
//  message produced an event; false otherwise (so callers can fall
//  through to their default Win32 handling).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInput::Translate (UINT msg, WPARAM wParam, LPARAM lParam, DxuiEvent & outEvent)
{
    DxuiEvent  ev = {};



    RefreshModifiers();
    ev.mods = m_mods;

    switch (msg)
    {
    case WM_MOUSEMOVE:
        m_mouseX  = (int) (short) LOWORD (lParam);
        m_mouseY  = (int) (short) HIWORD (lParam);
        ev.type   = DxuiEventType::MouseMove;
        ev.x      = m_mouseX;
        ev.y      = m_mouseY;
        break;

    case WM_LBUTTONDOWN:
        ev.type   = DxuiEventType::MouseDown;
        ev.button = DxuiMouseButton::Left;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        m_mouseX  = ev.x;
        m_mouseY  = ev.y;
        break;

    case WM_LBUTTONUP:
        ev.type   = DxuiEventType::MouseUp;
        ev.button = DxuiMouseButton::Left;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        m_mouseX  = ev.x;
        m_mouseY  = ev.y;
        break;

    case WM_RBUTTONDOWN:
        ev.type   = DxuiEventType::MouseDown;
        ev.button = DxuiMouseButton::Right;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_RBUTTONUP:
        ev.type   = DxuiEventType::MouseUp;
        ev.button = DxuiMouseButton::Right;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_MBUTTONDOWN:
        ev.type   = DxuiEventType::MouseDown;
        ev.button = DxuiMouseButton::Middle;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_MBUTTONUP:
        ev.type   = DxuiEventType::MouseUp;
        ev.button = DxuiMouseButton::Middle;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_MOUSEWHEEL:
        ev.type   = DxuiEventType::MouseWheel;
        ev.wheel  = GET_WHEEL_DELTA_WPARAM (wParam);
        ev.x      = m_mouseX;
        ev.y      = m_mouseY;
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        ev.type    = DxuiEventType::KeyDown;
        ev.keyCode = (UINT) wParam;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        ev.type    = DxuiEventType::KeyUp;
        ev.keyCode = (UINT) wParam;
        break;

    case WM_CHAR:
        ev.type    = DxuiEventType::Char;
        ev.ch      = (wchar_t) wParam;
        break;

    case WM_SETFOCUS:
        ev.type    = DxuiEventType::FocusGained;
        break;

    case WM_KILLFOCUS:
        ev.type    = DxuiEventType::FocusLost;
        break;

    default:
        return false;
    }

    outEvent = ev;
    m_queue.push_back (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushEvent
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInput::PushEvent (const DxuiEvent & ev)
{
    m_queue.push_back (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopEvent
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInput::PopEvent (DxuiEvent & outEvent)
{
    if (m_queue.empty())
    {
        return false;
    }

    outEvent = m_queue.front();
    m_queue.pop_front();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInput::Clear()
{
    m_queue.clear();
}
