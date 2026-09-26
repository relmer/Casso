#include "Pch.h"

#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank
//
////////////////////////////////////////////////////////////////////////////////

AppleSoftSwitchBank::AppleSoftSwitchBank()
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
//  $C058-$C05D: AN0-AN2 off/on
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
        case 0xC058:
        case 0xC059:
        case 0xC05A:
        case 0xC05B:
        case 0xC05C:
        case 0xC05D:
            AccessAnnunciator (address);
            break;
        default:
            break;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessAnnunciator
//
//  $C058 + 2n turns annunciator n off and $C059 + 2n turns it on, on a read
//  or a write alike. The outputs are latches: nothing but another access to
//  the same pair, or power-on, changes them.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::AccessAnnunciator (Word address)
{
    int   offset = static_cast<int> (address - kwFirstAnnunciatorAddress);
    int   index  = offset / 2;
    Byte  mask   = static_cast<Byte> (1 << index);



    if (offset & 1)
    {
        m_annunciators.fetch_or (mask, memory_order_acq_rel);
    }
    else
    {
        m_annunciators.fetch_and (static_cast<Byte> (~mask), memory_order_acq_rel);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAnnunciatorOn
//
////////////////////////////////////////////////////////////////////////////////

bool AppleSoftSwitchBank::IsAnnunciatorOn (int index) const
{
    Byte  mask = static_cast<Byte> (1 << index);



    return (m_annunciators.load (memory_order_acquire) & mask) != 0;
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

void AppleSoftSwitchBank::Reset()
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

void AppleSoftSwitchBank::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  The annunciators come up off at power-on. A Ctrl-Reset leaves them as
//  they were, so Reset and SoftReset do not touch them; a program that
//  wants them in a known state after a reset writes them itself.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    m_annunciators.store (0, memory_order_release);

    SoftReset();
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
