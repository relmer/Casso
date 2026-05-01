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
        return m_latchedKey.load (std::memory_order_acquire);
    }

    if (address >= 0xC010 && address <= 0xC01F)
    {
        // $C010-$C01F: Clear keyboard strobe (bit 7)
        Byte key = m_latchedKey.fetch_and (0x7F, std::memory_order_acq_rel);

        // Return the key with bit 7 reflecting any-key-down state
        if (m_anyKeyDown.load (std::memory_order_acquire))
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
        m_latchedKey.fetch_and (0x7F, std::memory_order_release);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Reset ()
{
    m_latchedKey.store (0, std::memory_order_release);
    m_anyKeyDown.store (false, std::memory_order_release);
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
        TranslateToUppercase (asciiChar) | 0x80, std::memory_order_release);
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

std::unique_ptr<MemoryDevice> AppleKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return std::make_unique<AppleKeyboard> ();
}
