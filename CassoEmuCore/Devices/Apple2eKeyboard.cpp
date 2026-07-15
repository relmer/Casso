#include "Pch.h"

#include "Apple2eKeyboard.h"
#include "Apple2eSoftSwitchBank.h"
#include "AppleMouse.h"
#include "AppleSpeaker.h"
#include "IInputEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eKeyboard
//
////////////////////////////////////////////////////////////////////////////////

Apple2eKeyboard::Apple2eKeyboard (MemoryBus * bus)
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

Byte Apple2eKeyboard::Read (Word address)
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

    // $C028: Apple //c ROM-bank flip-flop (ROMBANK) — soft-switch bank, which
    // toggles the visible firmware bank on any access. Unused on the //e (the
    // sibling no-ops when no ROM-bank switch is attached).
    if (address == 0xC028 && m_softSwitchSibling != nullptr)
    {
        return m_softSwitchSibling->Read (address);
    }

    // $C030-$C03F: speaker click — speaker device.
    if (address >= 0xC030 && address <= 0xC03F && m_speakerSibling != nullptr)
    {
        return m_speakerSibling->Read (address);
    }

    // $C048 (//c RSTXY): any access clears the mouse movement-interrupt
    // latches. No data behind the address — the read still returns 0.
    if (address == 0xC048 && m_mouse != nullptr)
    {
        m_mouse->AccessRstXY();
        return 0;
    }

    // $C050-$C05F: video display soft switches — soft-switch bank.
    if (address >= 0xC050 && address <= 0xC05F && m_softSwitchSibling != nullptr)
    {
        return m_softSwitchSibling->Read (address);
    }

    // $C061: Open Apple (bit 7).
    if (address == 0xC061)
    {
        Byte value = m_openApple.load (memory_order_acquire) ? 0x80 : 0x00;

        EmitButtonRead (address, value);

        return value;
    }

    // $C062: Closed Apple (bit 7).
    if (address == 0xC062)
    {
        Byte value = m_closedApple.load (memory_order_acquire) ? 0x80 : 0x00;

        EmitButtonRead (address, value);

        return value;
    }

    // $C063: mouse button on the //c (ACTIVE LOW; the //c wires the button
    // where the //e had its shift-key mod); Shift (bit 7) on the //e.
    if (address == 0xC063)
    {
        Byte value = (m_mouse != nullptr)
                         ? m_mouse->ReadButton()
                         : (m_shift.load (memory_order_acquire) ? 0x80 : 0x00);

        EmitButtonRead (address, value);

        return value;
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
//  EmitButtonRead
//
//  CPU thread. Coalesced emit for a guest read of $C061-$C063: fires only
//  when that button's returned byte (bit 7 = pressed) changed since the
//  last emit, so a tight button-poll loop yields one event per press /
//  release edge.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::EmitButtonRead (Word address, Byte value)
{
    IInputEventSink * sink = InputSink ();
    int               idx  = static_cast<int> (address - kFirstButtonAddress);

    if (sink == nullptr)
    {
        return;
    }

    if (m_lastEmittedButton[idx] == value)
    {
        return;
    }

    m_lastEmittedButton[idx] = value;
    sink->OnButtonRead (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOpenApple
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::SetOpenApple (bool pressed)
{
    m_openApple.store (pressed, memory_order_release);
    EmitHostButton (0, pressed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetClosedApple
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::SetClosedApple (bool pressed)
{
    m_closedApple.store (pressed, memory_order_release);
    EmitHostButton (1, pressed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitHostButton
//
//  Host UI thread. Coalesced emit for a host-set joystick button: fires
//  only when the staged button state changed since the last host-input emit.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::EmitHostButton (int index, bool pressed)
{
    IInputEventSink * sink  = InputSink ();
    int               value = pressed ? 1 : 0;



    if (sink == nullptr)
    {
        return;
    }

    if (m_lastEmittedHostButton[index] == value)
    {
        return;
    }

    m_lastEmittedHostButton[index] = value;
    sink->OnHostButton (index, pressed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyPressRaw
//
//  IIe keyboard supports lowercase — don't force uppercase.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::KeyPressRaw (Byte asciiChar)
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

void Apple2eKeyboard::Write (Word address, Byte value)
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

    // $C028: Apple //c ROM-bank flip-flop — forward to the soft-switch bank
    // (toggles on write too; no-op on the //e).
    if (address == 0xC028 && m_softSwitchSibling != nullptr)
    {
        m_softSwitchSibling->Write (address, value);
        return;
    }

    if (address >= 0xC030 && address <= 0xC03F && m_speakerSibling != nullptr)
    {
        m_speakerSibling->Write (address, value);
        return;
    }

    // $C048 (//c RSTXY): any access — the firmware acks with STA $C048.
    if (address == 0xC048 && m_mouse != nullptr)
    {
        m_mouse->AccessRstXY();
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

void Apple2eKeyboard::Reset ()
{
    AppleKeyboard::Reset ();

    m_openApple.store   (false, memory_order_release);
    m_closedApple.store (false, memory_order_release);
    m_shift.store       (false, memory_order_release);

    m_lastEmittedButton[0] = -1;
    m_lastEmittedButton[1] = -1;
    m_lastEmittedButton[2] = -1;

    m_lastEmittedHostButton[0] = -1;
    m_lastEmittedHostButton[1] = -1;
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

void Apple2eKeyboard::SoftReset ()
{
    // Bypass the virtual chain: AppleKeyboard::SoftReset -> Reset() would
    // dispatch to Apple2eKeyboard::Reset and clobber the modifiers we
    // need to preserve. Just clear the latched-character byte directly.
    AppleKeyboard::Reset ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> Apple2eKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return make_unique<Apple2eKeyboard> (&bus);
}
