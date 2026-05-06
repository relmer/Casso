#include "Pch.h"

#include "AppleIIeKeyboard.h"
#include "AppleIIeSoftSwitchBank.h"
#include "AppleIIeMmu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeKeyboard
//
////////////////////////////////////////////////////////////////////////////////

AppleIIeKeyboard::AppleIIeKeyboard (MemoryBus * bus)
    : AppleKeyboard (),
      m_bus         (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleIIeKeyboard::Read (Word address)
{
    // $C061: Open Apple button (bit 7)
    if (address == 0xC061)
    {
        return m_openApple.load (memory_order_acquire) ? 0x80 : 0x00;
    }

    // $C062: Closed Apple button (bit 7)
    if (address == 0xC062)
    {
        return m_closedApple.load (memory_order_acquire) ? 0x80 : 0x00;
    }

    // $C00C-$C00F: 80COL / ALTCHARSET soft switches (//e). Forward to the
    // softswitch sibling since these overlap the keyboard's address range.
    if (address >= 0xC00C && address <= 0xC00F && m_softSwitchSibling != nullptr)
    {
        return m_softSwitchSibling->Read (address);
    }

    // $C011-$C01F on //e: status flag reads. Bit 7 reflects state of the
    // corresponding soft switch. Low 7 bits return the latched keyboard data
    // (floating-bus behavior; most software only checks bit 7).
    // Note: $C010 still clears the keyboard strobe (handled by base).
    if (address >= 0xC011 && address <= 0xC01F)
    {
        Byte kbdByte = AppleKeyboard::Read (0xC000) & 0x7F;
        bool flag    = false;

        switch (address)
        {
            case 0xC013: // RDRAMRD
                flag = m_mmu != nullptr && m_mmu->GetRamRd ();
                break;
            case 0xC014: // RDRAMWRT
                flag = m_mmu != nullptr && m_mmu->GetRamWrt ();
                break;
            case 0xC015: // RDINTCXROM
                flag = m_mmu != nullptr && m_mmu->GetIntCxRom ();
                break;
            case 0xC016: // RDALTZP
                flag = m_mmu != nullptr && m_mmu->GetAltZp ();
                break;
            case 0xC017: // RDSLOTC3ROM
                flag = m_mmu != nullptr && m_mmu->GetSlotC3Rom ();
                break;
            case 0xC018: // RD80STORE
                flag = m_mmu != nullptr && m_mmu->Get80Store ();
                break;
            case 0xC01C: // RDPAGE2
                flag = m_softSwitchSibling != nullptr && m_softSwitchSibling->IsPage2 ();
                break;
            case 0xC01D: // RDHIRES
                flag = m_softSwitchSibling != nullptr && m_softSwitchSibling->IsHiresMode ();
                break;
            case 0xC01E: // RDALTCHAR
                flag = m_softSwitchSibling != nullptr && m_softSwitchSibling->IsAltCharSet ();
                break;
            case 0xC01F: // RD80VID
                flag = m_softSwitchSibling != nullptr && m_softSwitchSibling->Is80ColMode ();
                break;
            case 0xC01A: // RDTEXT
                flag = m_softSwitchSibling != nullptr && !m_softSwitchSibling->IsGraphicsMode ();
                break;
            case 0xC01B: // RDMIXED
                flag = m_softSwitchSibling != nullptr && m_softSwitchSibling->IsMixedMode ();
                break;
            // BSRBANK2 / BSRREADRAM (LC) is Phase 3; VBL ($C019) is Phase 5.
            default:
                break;
        }

        return static_cast<Byte> (kbdByte | (flag ? 0x80 : 0x00));
    }

    // Default keyboard handling ($C000-$C00B reads kbd, $C010 clears strobe)
    return AppleKeyboard::Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyPressRaw
//
//  IIe keyboard supports lowercase — don't force uppercase.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::KeyPressRaw (Byte asciiChar)
{
    // Directly set the latched key without uppercase translation
    KeyPress (asciiChar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Per Apple //e Tech Ref, the keyboard's address range $C000-$C0FF is
//  shared with soft switches. Writes in $C000-$C00F change paging/video state
//  rather than affecting the keyboard. We forward them to the softswitch
//  sibling. Writes in $C010-$C01F clear the keyboard strobe.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::Write (Word address, Byte value)
{
    if (address >= 0xC000 && address <= 0xC00F && m_softSwitchSibling != nullptr)
    {
        m_softSwitchSibling->Write (address, value);
        return;
    }

    AppleKeyboard::Write (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::Reset ()
{
    AppleKeyboard::Reset ();
    m_openApple.store (false, memory_order_release);
    m_closedApple.store (false, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleIIeKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return make_unique<AppleIIeKeyboard> (&bus);
}
