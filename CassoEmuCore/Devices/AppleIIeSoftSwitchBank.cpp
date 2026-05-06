#include "Pch.h"

#include "AppleIIeSoftSwitchBank.h"
#include "AppleIIeMmu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeSoftSwitchBank
//
////////////////////////////////////////////////////////////////////////////////

AppleIIeSoftSwitchBank::AppleIIeSoftSwitchBank (MemoryBus * bus)
    : AppleSoftSwitchBank (),
      m_bus               (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Is80Store
//
//  Delegates to the MMU which owns the canonical 80STORE flag.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleIIeSoftSwitchBank::Is80Store () const
{
    return m_mmu != nullptr && m_mmu->Get80Store ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  $C00C-$C00F (80COL/ALTCHARSET) toggle on read OR write per real //e.
//  $C054-$C057 (PAGE2/HIRES) trigger banking-changed so MMU can re-resolve.
//  $C05E/$C05F toggle DHIRES (display-only).
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleIIeSoftSwitchBank::Read (Word address)
{
    bool  bankingChange = false;

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
            bankingChange = true;
            break;
        case 0xC05F:
            m_doubleHiRes = false;
            bankingChange = true;
            break;
        default:
            break;
    }

    if (address >= 0xC054 && address <= 0xC057)
    {
        bankingChange = true;
    }

    Byte  result = 0;

    if (address >= 0xC050 && address <= 0xC057)
    {
        result = AppleSoftSwitchBank::Read (address);
    }

    if (bankingChange)
    {
        if (m_mmu != nullptr)
        {
            m_mmu->OnSoftSwitchChanged ();
        }

        if (m_bus != nullptr)
        {
            m_bus->NotifyBankingChanged ();
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Per Apple //e Tech Ref:
//    $C000 write -> 80STORE OFF      $C001 write -> 80STORE ON
//    $C002 write -> RAMRD   OFF      $C003 write -> RAMRD   ON
//    $C004 write -> RAMWRT  OFF      $C005 write -> RAMWRT  ON
//    $C006 write -> INTCXROM OFF     $C007 write -> INTCXROM ON
//    $C008 write -> ALTZP   OFF      $C009 write -> ALTZP   ON
//    $C00A write -> SLOTC3ROM OFF    $C00B write -> SLOTC3ROM ON
//    $C00C-$C00F writes  -> same as reads (80COL, ALTCHARSET)
//    Other writes        -> same as reads
//
//  All MMU-owned switches forward to the MMU (which owns the flag and
//  rebinds the page table). Audit §1.1 fix-by-relocation: this is the
//  correct addressing surface; the legacy AuxRamCard's $C003-$C006
//  was wrong and is deleted.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeSoftSwitchBank::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    if (address >= 0xC000 && address <= 0xC00B && m_mmu != nullptr)
    {
        switch (address)
        {
            case 0xC000:  m_mmu->Set80Store   (false); return;
            case 0xC001:  m_mmu->Set80Store   (true);  return;
            case 0xC002:  m_mmu->SetRamRd     (false); return;
            case 0xC003:  m_mmu->SetRamRd     (true);  return;
            case 0xC004:  m_mmu->SetRamWrt    (false); return;
            case 0xC005:  m_mmu->SetRamWrt    (true);  return;
            case 0xC006:  m_mmu->SetIntCxRom  (false); return;
            case 0xC007:  m_mmu->SetIntCxRom  (true);  return;
            case 0xC008:  m_mmu->SetAltZp     (false); return;
            case 0xC009:  m_mmu->SetAltZp     (true);  return;
            case 0xC00A:  m_mmu->SetSlotC3Rom (false); return;
            case 0xC00B:  m_mmu->SetSlotC3Rom (true);  return;
            default:      break;
        }
    }

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

    return make_unique<AppleIIeSoftSwitchBank> (&bus);
}
