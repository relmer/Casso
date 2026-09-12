#include "Pch.h"

#include "Shell/Input/CapsLockTracker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OnCapsLockPressed
//
//  Every press after the first changes nothing here: the tracker is already
//  following the host, and the host has already moved.
//
////////////////////////////////////////////////////////////////////////////////

bool CapsLockTracker::OnCapsLockPressed (bool hostCapsLockOn)
{
    bool  isFirstPress = !m_isFollowingHost;



    m_isFollowingHost = true;

    return isFirstPress && hostCapsLockOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveHostCapsLock
//
//  Case inversion rather than a fold to lower case, because Shift still
//  counts: under the host's Caps Lock, Shift+A arrives as 'a', and the letter
//  the user meant is 'A'.
//
////////////////////////////////////////////////////////////////////////////////

Byte CapsLockTracker::RemoveHostCapsLock (Byte ch, bool hostCapsLockOn)
{
    constexpr Byte  kCaseBit = 'a' - 'A';
    bool            isUpper  = ch >= 'A' && ch <= 'Z';
    bool            isLower  = ch >= 'a' && ch <= 'z';



    if (!hostCapsLockOn || !(isUpper || isLower))
    {
        return ch;
    }

    return (Byte) (ch ^ kCaseBit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyCapsLock
//
////////////////////////////////////////////////////////////////////////////////

Byte CapsLockTracker::ApplyCapsLock (Byte ch, bool capsLockOn)
{
    constexpr Byte  kCaseBit = 'a' - 'A';
    bool            isLower  = ch >= 'a' && ch <= 'z';



    if (!capsLockOn || !isLower)
    {
        return ch;
    }

    return (Byte) (ch & ~kCaseBit);
}
