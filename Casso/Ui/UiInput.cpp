#include "Pch.h"

#include "UiInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshModifiers
//
////////////////////////////////////////////////////////////////////////////////

void UiInput::RefreshModifiers ()
{
    m_mods.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    m_mods.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    m_mods.alt   = (GetKeyState (VK_MENU)    & 0x8000) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Translate
//
//  Converts a single Win32 message into a typed `UiEvent` and queues it
//  for later pop by the shell's per-frame tick. Returns true when the
//  message produced an event; false otherwise (so callers can fall
//  through to their default Win32 handling).
//
////////////////////////////////////////////////////////////////////////////////

bool UiInput::Translate (UINT msg, WPARAM wParam, LPARAM lParam, UiEvent & outEvent)
{
    UiEvent  ev = {};



    RefreshModifiers();
    ev.mods = m_mods;

    switch (msg)
    {
    case WM_MOUSEMOVE:
        m_mouseX  = (int) (short) LOWORD (lParam);
        m_mouseY  = (int) (short) HIWORD (lParam);
        ev.type   = UiEventType::MouseMove;
        ev.x      = m_mouseX;
        ev.y      = m_mouseY;
        break;

    case WM_LBUTTONDOWN:
        ev.type   = UiEventType::MouseDown;
        ev.button = UiMouseButton::Left;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        m_mouseX  = ev.x;
        m_mouseY  = ev.y;
        break;

    case WM_LBUTTONUP:
        ev.type   = UiEventType::MouseUp;
        ev.button = UiMouseButton::Left;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        m_mouseX  = ev.x;
        m_mouseY  = ev.y;
        break;

    case WM_RBUTTONDOWN:
        ev.type   = UiEventType::MouseDown;
        ev.button = UiMouseButton::Right;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_RBUTTONUP:
        ev.type   = UiEventType::MouseUp;
        ev.button = UiMouseButton::Right;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_MBUTTONDOWN:
        ev.type   = UiEventType::MouseDown;
        ev.button = UiMouseButton::Middle;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_MBUTTONUP:
        ev.type   = UiEventType::MouseUp;
        ev.button = UiMouseButton::Middle;
        ev.x      = (int) (short) LOWORD (lParam);
        ev.y      = (int) (short) HIWORD (lParam);
        break;

    case WM_MOUSEWHEEL:
        ev.type   = UiEventType::MouseWheel;
        ev.wheel  = GET_WHEEL_DELTA_WPARAM (wParam);
        ev.x      = m_mouseX;
        ev.y      = m_mouseY;
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        ev.type    = UiEventType::KeyDown;
        ev.keyCode = (UINT) wParam;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        ev.type    = UiEventType::KeyUp;
        ev.keyCode = (UINT) wParam;
        break;

    case WM_CHAR:
        ev.type    = UiEventType::Char;
        ev.ch      = (wchar_t) wParam;
        break;

    case WM_SETFOCUS:
        ev.type    = UiEventType::FocusGained;
        break;

    case WM_KILLFOCUS:
        ev.type    = UiEventType::FocusLost;
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

void UiInput::PushEvent (const UiEvent & ev)
{
    m_queue.push_back (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopEvent
//
////////////////////////////////////////////////////////////////////////////////

bool UiInput::PopEvent (UiEvent & outEvent)
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

void UiInput::Clear ()
{
    m_queue.clear();
}
