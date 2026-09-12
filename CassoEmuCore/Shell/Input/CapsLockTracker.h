#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CapsLockTracker
//
//  Whether the //e and //c Caps Lock key is down, decided without ever
//  changing the host keyboard's Caps Lock.
//
//  A real //e and //c ship with the key down, and most software expects upper
//  case, but the host's Caps Lock is usually off. So until the user presses
//  Caps Lock in the emulator window, the emulated key reads as down whatever
//  the host's is. The first press there switches it to following the host's
//  Caps Lock for the rest of the session, and from then on the keyboard LED
//  tells the truth.
//
//  That first press is invisible when it turns the host's Caps Lock ON: the
//  emulated key was already down and stays down. It is the one press that
//  needs explaining, so OnCapsLockPressed reports it.
//
////////////////////////////////////////////////////////////////////////////////

class CapsLockTracker
{
public:

    static constexpr const wchar_t *  kpszNowFollowingHostNotice =
        L"Casso is now tracking the Caps Lock state. Toggle again to turn off Caps Lock.";

    static constexpr const wchar_t *  kpszPasteRaisedNotice =
        L"Pasted in upper case because Caps Lock is on. Turn off Caps Lock to paste lower case.";

    bool  IsFollowingHost () const                     { return m_isFollowingHost; }
    bool  IsOn            (bool hostCapsLockOn) const  { return m_isFollowingHost ? hostCapsLockOn : true; }

    // A Caps Lock press the emulator window received, with the host's Caps
    // Lock as that press left it. True only for the first press, and only when
    // it turned the host's Caps Lock on.
    bool  OnCapsLockPressed (bool hostCapsLockOn);

    // The case a letter would have had with the host's Caps Lock off: Windows
    // inverts a letter's case under Caps Lock, so inverting it back leaves
    // Shift alone deciding. Anything but an ASCII letter passes through.
    static Byte  RemoveHostCapsLock (Byte ch, bool hostCapsLockOn);

    // The encoder's Caps Lock: a letter comes out upper case while the key is
    // down, whatever Shift says. Digits and punctuation are unaffected.
    static Byte  ApplyCapsLock      (Byte ch, bool capsLockOn);

private:

    bool  m_isFollowingHost = false;
};
