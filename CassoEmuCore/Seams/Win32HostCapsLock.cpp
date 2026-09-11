#include "Pch.h"

#include "Seams/Win32HostCapsLock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IsOn
//
//  The low bit of GetKeyState is the toggle. Read on the thread that owns the
//  window, it answers as of the message being processed, so inside a
//  VK_CAPITAL key-up it already reflects that press.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostCapsLock::IsOn() const
{
    return (GetKeyState (VK_CAPITAL) & 1) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Toggle
//
//  SetKeyboardState moves the table but not the LED, so the flip is a real
//  keystroke. Both halves go in one call so nothing can interleave between
//  them. A rejected injection is left alone: the host toggle simply stays
//  where it was, and the next real Caps Lock press puts it right.
//
////////////////////////////////////////////////////////////////////////////////

void Win32HostCapsLock::Toggle()
{
    INPUT  inputs[2] = {};
    UINT   sent      = 0;



    inputs[0].type           = INPUT_KEYBOARD;
    inputs[0].ki.wVk         = VK_CAPITAL;
    inputs[0].ki.dwExtraInfo = kSynthesizedMarker;

    inputs[1]                = inputs[0];
    inputs[1].ki.dwFlags     = KEYEVENTF_KEYUP;

    sent = SendInput (ARRAYSIZE (inputs), inputs, sizeof (INPUT));
    IGNORE_RETURN_VALUE (sent, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasLastKeySynthesized
//
//  GetMessageExtraInfo carries the injecting call's extra-info word for the
//  message this thread most recently retrieved, which inside a key handler is
//  the key being handled.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostCapsLock::WasLastKeySynthesized() const
{
    return (ULONG_PTR) GetMessageExtraInfo() == kSynthesizedMarker;
}
