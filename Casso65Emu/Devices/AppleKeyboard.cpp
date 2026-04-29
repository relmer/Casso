#include "Pch.h"

#include "AppleKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyboard
//
////////////////////////////////////////////////////////////////////////////////

AppleKeyboard::AppleKeyboard ()
    : m_latchedKey (0),
      m_strobe     (false),
      m_anyKeyDown (false)
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
        // $C000-$C00F: Read keyboard data
        // Bit 7 = strobe (1 = key available)
        if (m_strobe)
        {
            return m_latchedKey | 0x80;
        }

        return m_latchedKey & 0x7F;
    }

    if (address >= 0xC010 && address <= 0xC01F)
    {
        // $C010-$C01F: Clear keyboard strobe
        m_strobe = false;

        // Return the key with strobe reflecting any-key-down state
        if (m_anyKeyDown)
        {
            return m_latchedKey | 0x80;
        }

        return m_latchedKey & 0x7F;
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
        m_strobe = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Reset ()
{
    m_latchedKey = 0;
    m_strobe     = false;
    m_anyKeyDown = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyPress
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::KeyPress (Byte asciiChar)
{
    m_latchedKey = TranslateToUppercase (asciiChar) & 0x7F;
    m_strobe     = true;
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
