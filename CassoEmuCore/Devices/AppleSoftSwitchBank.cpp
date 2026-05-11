#include "Pch.h"

#include "AppleSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank
//
////////////////////////////////////////////////////////////////////////////////

AppleSoftSwitchBank::AppleSoftSwitchBank ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Read or write to $C050-$C057 toggles video mode flags.
//  Even addresses clear the flag, odd addresses set it.
//
//  $C050/$C051: Graphics/Text
//  $C052/$C053: Full/Mixed
//  $C054/$C055: Page1/Page2
//  $C056/$C057: LoRes/HiRes
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleSoftSwitchBank::Read (Word address)
{
    switch (address)
    {
        case 0xC050:
            m_graphicsMode = true;
            break;
        case 0xC051:
            m_graphicsMode = false;
            break;
        case 0xC052:
            m_mixedMode = false;
            break;
        case 0xC053:
            m_mixedMode = true;
            break;
        case 0xC054:
            m_page2 = false;
            break;
        case 0xC055:
            m_page2 = true;
            break;
        case 0xC056:
            m_hiresMode = false;
            break;
        case 0xC057:
            m_hiresMode = true;
            break;
        default:
            break;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    // Write has same effect as read for soft switches
    Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::Reset ()
{
    m_graphicsMode = false;
    m_mixedMode    = false;
    m_page2        = false;
    m_hiresMode    = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4 / FR-034: //e MMU asserts /RESET clearing the display soft
//  switches. PowerCycle inherits the same body via the default forwarder
//  on MemoryDevice (audit §10).
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::SoftReset ()
{
    Reset ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleSoftSwitchBank::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleSoftSwitchBank> ();
}
