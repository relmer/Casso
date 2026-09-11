#pragma once

#include "Pch.h"

#include "Seams/IHostCapsLock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostCapsLock
//
//  IHostCapsLock over GetKeyState and SendInput. A flip is a VK_CAPITAL press
//  and release injected into the input stream, stamped with a marker in the
//  extra-info word so the key-up it produces can be told from the user's.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostCapsLock : public IHostCapsLock
{
public:

    bool  IsOn                   () const override;
    void  Toggle                 ()       override;
    bool  WasLastKeySynthesized  () const override;

private:

    static constexpr ULONG_PTR  kSynthesizedMarker = 0x4361'7073;   // "Caps"
};
