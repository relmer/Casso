#include "Pch.h"

#include "AppleIIeKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeKeyboard
//
////////////////////////////////////////////////////////////////////////////////

AppleIIeKeyboard::AppleIIeKeyboard ()
    : AppleKeyboard ()
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

    // Default keyboard handling
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
//  On the //e, writes to $C000/$C001 toggle 80STORE mode.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeKeyboard::Write (Word address, Byte value)
{
    if (address == 0xC000)
    {
        m_80store = false;
    }
    else if (address == 0xC001)
    {
        m_80store = true;
    }
    else
    {
        AppleKeyboard::Write (address, value);
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
    m_openApple.store (false, memory_order_release);
    m_closedApple.store (false, memory_order_release);
    m_80store = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleIIeKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleIIeKeyboard> ();
}
