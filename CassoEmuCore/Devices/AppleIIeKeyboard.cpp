#include "Pch.h"

#include "AppleIIeKeyboard.h"
#include "AppleIIeSoftSwitchBank.h"
#include "AppleSpeaker.h"





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
//  Phase 6 / T060 / T061. The bus range $C000-$C063 covers more than the
//  keyboard logically owns; addresses outside the keyboard's logical
//  scope are forwarded to the canonical sibling device. The bank's
//  read-only status path enforces strobe-clear isolation (audit §1.2):
//  ONLY $C010 clears the strobe.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleIIeKeyboard::Read (Word address)
{
    // $C00C-$C00F: 80COL/ALTCHARSET soft switches — soft-switch bank.
    if (address >= 0xC00C && address <= 0xC00F && m_softSwitchSibling != nullptr)
    {
        return m_softSwitchSibling->Read (address);
    }

    // $C011-$C01F: status reads — soft-switch bank (T061 ownership split).
    if (address >= 0xC011 && address <= 0xC01F && m_softSwitchSibling != nullptr)
    {
        return m_softSwitchSibling->Read (address);
    }

    // $C030-$C03F: speaker click — speaker device.
    if (address >= 0xC030 && address <= 0xC03F && m_speakerSibling != nullptr)
    {
        return m_speakerSibling->Read (address);
    }

    // $C050-$C05F: video display soft switches — soft-switch bank.
    if (address >= 0xC050 && address <= 0xC05F && m_softSwitchSibling != nullptr)
    {
        return m_softSwitchSibling->Read (address);
    }

    // $C061: Open Apple (bit 7).
    if (address == 0xC061)
    {
        return m_openApple.load (memory_order_acquire) ? 0x80 : 0x00;
    }

    // $C062: Closed Apple (bit 7).
    if (address == 0xC062)
    {
        return m_closedApple.load (memory_order_acquire) ? 0x80 : 0x00;
    }

    // $C063: Shift (bit 7).
    if (address == 0xC063)
    {
        return m_shift.load (memory_order_acquire) ? 0x80 : 0x00;
    }

    // $C000-$C00B (keyboard data) and $C010 (strobe-clear) fall through
    // to the base AppleKeyboard. Other unowned addresses ($C020-$C02F,
    // $C040-$C04F, $C060) return 0 — no device behind them on a //e.
    if (address <= 0xC010)
    {
        return AppleKeyboard::Read (address);
    }

    return 0;
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
    KeyPress (asciiChar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Phase 6 / T060 strobe-clear isolation: ONLY $C010 (read OR write)
//  clears the strobe. $C011-$C01F writes are routed to the soft-switch
//  bank (status-read mirrors; the bank's Write is a no-op) and MUST NOT
//  fall through to the base which would clear the strobe.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::Write (Word address, Byte value)
{
    if (address >= 0xC000 && address <= 0xC00F && m_softSwitchSibling != nullptr)
    {
        m_softSwitchSibling->Write (address, value);
        return;
    }

    if (address == 0xC010)
    {
        AppleKeyboard::Write (address, value);
        return;
    }

    if (address >= 0xC011 && address <= 0xC01F && m_softSwitchSibling != nullptr)
    {
        m_softSwitchSibling->Write (address, value);
        return;
    }

    if (address >= 0xC030 && address <= 0xC03F && m_speakerSibling != nullptr)
    {
        m_speakerSibling->Write (address, value);
        return;
    }

    if (address >= 0xC050 && address <= 0xC05F && m_softSwitchSibling != nullptr)
    {
        m_softSwitchSibling->Write (address, value);
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::Reset ()
{
    AppleKeyboard::Reset ();

    m_openApple.store   (false, memory_order_release);
    m_closedApple.store (false, memory_order_release);
    m_shift.store       (false, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Real //e behavior: a CPU /RESET pulse does NOT lift the user's finger
//  off Open Apple, Closed Apple, or Shift. The firmware reads $C061 at
//  reset specifically to decide warm-vs-autoboot, so clearing those
//  modifiers here would break Ctrl+Open-Apple+Reset autoboot. Only the
//  latched-character byte at $C000 needs to clear (handled by the base
//  class) so a stale typeahead doesn't survive the reset.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::SoftReset ()
{
    // Bypass the virtual chain: AppleKeyboard::SoftReset -> Reset() would
    // dispatch to AppleIIeKeyboard::Reset and clobber the modifiers we
    // need to preserve. Just clear the latched-character byte directly.
    AppleKeyboard::Reset ();
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
