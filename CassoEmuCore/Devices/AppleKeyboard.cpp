#include "Pch.h"

#include "AppleKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyboard
//
////////////////////////////////////////////////////////////////////////////////

AppleKeyboard::AppleKeyboard ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleKeyboard::Read (Word address)
{
    if (address >= 0xC000 && address <= 0xC00F)
    {
        // $C000-$C00F: Read keyboard data (bit 7 = strobe)
        return m_latchedKey.load (memory_order_acquire);
    }

    if (address >= 0xC010 && address <= 0xC01F)
    {
        // $C010-$C01F: Clear keyboard strobe (bit 7)
        Byte key = m_latchedKey.fetch_and (0x7F, memory_order_acq_rel);

        // Return the key with bit 7 reflecting any-key-down state
        if (m_anyKeyDown.load (memory_order_acquire))
        {
            return (key & 0x7F) | 0x80;
        }

        return key & 0x7F;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    // Writing to $C010 also clears strobe
    if (address >= 0xC010 && address <= 0xC01F)
    {
        m_latchedKey.fetch_and (0x7F, memory_order_release);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Reset ()
{
    m_latchedKey.store (0, memory_order_release);
    m_anyKeyDown.store (false, memory_order_release);
    m_repeatKey.store  (0, memory_order_release);
    m_repeatAccumCycles = 0;
    m_repeatStarted     = false;
    m_lastRepeatKey     = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4: clear the latched key and the any-key-down indicator so the
//  ROM's `]` prompt sees no pending strobe.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::SoftReset ()
{
    Reset ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyPress
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::KeyPress (Byte asciiChar)
{
    // Store key with bit 7 set (strobe) in a single atomic write
    m_latchedKey.store (
        TranslateToUppercase (asciiChar) | 0x80, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Regenerates the authentic //e keyboard auto-repeat. The shell suppresses
//  the host OS repeat and arms a single character via BeginKeyRepeat; this
//  drives the delay-then-rate cadence in emulated CPU time. Re-latches the
//  held key (re-arming the $C000 strobe) once the delay elapses and then at
//  the steady repeat interval -- but only while the key is still physically
//  down, which the shell signals through SetKeyDown / any-key-down.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Tick (uint32_t cpuCycles)
{
    Byte      key       = m_repeatKey.load (memory_order_acquire);
    bool      keyHeld   = m_anyKeyDown.load (memory_order_acquire);
    uint32_t  threshold = 0;

    // No armed key, or the physical key was released: stand down and reset
    // the cadence so the next press starts a fresh delay window.
    if (key == 0 || !keyHeld)
    {
        m_repeatAccumCycles = 0;
        m_repeatStarted     = false;
        m_lastRepeatKey     = 0;
        return;
    }

    // A newly-armed key. The shell already latched the first strobe on the
    // physical press, so begin timing the pre-repeat delay without latching
    // again here.
    if (key != m_lastRepeatKey)
    {
        m_lastRepeatKey     = key;
        m_repeatAccumCycles = 0;
        m_repeatStarted     = false;
        return;
    }

    m_repeatAccumCycles += cpuCycles;

    threshold = m_repeatStarted ? kKeyRepeatIntervalCycles
                                : kKeyRepeatDelayCycles;

    if (m_repeatAccumCycles >= threshold)
    {
        m_repeatAccumCycles -= threshold;
        m_repeatStarted      = true;
        KeyPress (key);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TranslateToUppercase
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleKeyboard::TranslateToUppercase (Byte ch) const
{
    // Apple II/II+ keyboard is uppercase only
    if (ch >= 'a' && ch <= 'z')
    {
        return ch - ('a' - 'A');
    }

    return ch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleKeyboard> ();
}
