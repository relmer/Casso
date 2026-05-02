#include "Pch.h"

#include "AppleIIeSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeSoftSwitchBank
//
////////////////////////////////////////////////////////////////////////////////

AppleIIeSoftSwitchBank::AppleIIeSoftSwitchBank ()
    : AppleSoftSwitchBank ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleIIeSoftSwitchBank::Read (Word address)
{
    // IIe-specific switches
    switch (address)
    {
        case 0xC00C:
            m_80colMode = false;
            return 0;
        case 0xC00D:
            m_80colMode = true;
            return 0;
        case 0xC00E:
            m_altCharSet = false;
            return 0;
        case 0xC00F:
            m_altCharSet = true;
            return 0;
        case 0xC05E:
            m_doubleHiRes = true;
            return 0;
        case 0xC05F:
            m_doubleHiRes = false;
            return 0;
        default:
            break;
    }

    // Fall through to base class for $C050-$C057
    if (address >= 0xC050 && address <= 0xC057)
    {
        return AppleSoftSwitchBank::Read (address);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeSoftSwitchBank::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);
    Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeSoftSwitchBank::Reset ()
{
    AppleSoftSwitchBank::Reset ();
    m_80colMode   = false;
    m_doubleHiRes = false;
    m_altCharSet  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleIIeSoftSwitchBank::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleIIeSoftSwitchBank> ();
}
