#include "Pch.h"

#include "Apple3Keyboard.h"
#include "Apple3SystemVias.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr Byte  s_kStrobe       = 0x80;
static constexpr Byte  s_kCodeMask     = 0x7F;
static constexpr Byte  s_kSpecialBit   = 0x80;
static constexpr Byte  s_kCaseBit      = 0x20;
static constexpr Byte  s_kControlBase  = 0x40;
static constexpr Byte  s_kLastControl  = 0x1A;
static constexpr Byte  s_kReturn       = 0x0D;
static constexpr Byte  s_kSpace        = 0x20;
static constexpr Word  s_kFirstClear   = 0xC010;

//  The keys other than the letters, Return and the special keys, from SVC
//  page 8.6: the code alone, with Shift, with Control, and with both. The
//  page prints Shift-Control-6 as $53; every other key's Shift-Control code
//  is its Shift code's low five bits or its Shift code, so it is $1E here.
static constexpr int  s_kAlone        = 0;
static constexpr int  s_kWithShift    = 1;
static constexpr int  s_kWithControl  = 2;
static constexpr int  s_kWithBoth     = 3;
static constexpr int  s_kCombinations = 4;

static constexpr std::array<std::array<Byte, s_kCombinations>, 21>  s_kSymbolKeys =
{ {
    { 0x31, 0x21, 0x31, 0x31 },     // 1
    { 0x32, 0x40, 0x32, 0x00 },     // 2
    { 0x33, 0x23, 0x33, 0x23 },     // 3
    { 0x34, 0x24, 0x34, 0x24 },     // 4
    { 0x35, 0x25, 0x35, 0x25 },     // 5
    { 0x36, 0x5E, 0x36, 0x1E },     // 6
    { 0x37, 0x26, 0x37, 0x26 },     // 7
    { 0x38, 0x2A, 0x38, 0x2A },     // 8
    { 0x39, 0x28, 0x39, 0x28 },     // 9
    { 0x30, 0x29, 0x30, 0x29 },     // 0
    { 0x2D, 0x5F, 0x2D, 0x1F },     // -
    { 0x3D, 0x2B, 0x3D, 0x2B },     // =
    { 0x5C, 0x7C, 0x7F, 0x1C },     // backslash
    { 0x5B, 0x7B, 0x1B, 0x1B },     // [
    { 0x5D, 0x7D, 0x1D, 0x1D },     // ]
    { 0x60, 0x7E, 0x60, 0x7E },     // `
    { 0x3B, 0x3A, 0x3B, 0x3A },     // ;
    { 0x27, 0x22, 0x27, 0x22 },     // '
    { 0x2C, 0x3C, 0x2C, 0x3C },     // ,
    { 0x2E, 0x3E, 0x2E, 0x3E },     // .
    { 0x2F, 0x3F, 0x2F, 0x3F },     // /
} };





////////////////////////////////////////////////////////////////////////////////
//
//  GetStrokeForChar
//
//  The key, Shift and Control that send a typed character. A lowercase
//  letter is its key alone and an uppercase one its key with Shift, since
//  the encoder sends the uppercase code for both. Control with a letter
//  sends the letter's low five bits. Return, not Control-M, sends $0D, and
//  Space, a special key, sends $A0. The rest come from the table, taking
//  the first combination that sends the character.
//
////////////////////////////////////////////////////////////////////////////////

Apple3Keyboard::Stroke Apple3Keyboard::GetStrokeForChar (Byte asciiChar)
{
    Byte    c      = (Byte) (asciiChar & s_kCodeMask);
    Stroke  stroke;



    if (c >= 'a' && c <= 'z')
    {
        stroke = { (Byte) (c - s_kCaseBit), false, false, true };
    }
    else if (c >= 'A' && c <= 'Z')
    {
        stroke = { c, true, false, true };
    }
    else if (c == s_kReturn)
    {
        stroke = { c, false, false, true };
    }
    else if (c == s_kSpace)
    {
        stroke = { (Byte) (c | s_kSpecialBit), false, false, true };
    }
    else if (c >= 1 && c <= s_kLastControl)
    {
        stroke = { (Byte) (c + s_kControlBase), false, true, true };
    }

    for (int combination = 0; !stroke.isValid && combination < s_kCombinations; combination++)
    {
        for (const auto & key : s_kSymbolKeys)
        {
            if (!stroke.isValid && key[(size_t) combination] == c)
            {
                bool  isShift   = combination == s_kWithShift   || combination == s_kWithBoth;
                bool  isControl = combination == s_kWithControl || combination == s_kWithBoth;



                stroke = { key[s_kAlone], isShift, isControl, true };
            }
        }
    }

    return stroke;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TranslateTypedChar
//
//  The code the encoder sends for a typed character: the uppercase letter
//  whatever the case, and anything else as typed.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Keyboard::TranslateTypedChar (Byte ch) const
{
    return (ch >= 'a' && ch <= 'z') ? (Byte) (ch - s_kCaseBit) : (Byte) (ch & s_kCodeMask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyLatched
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Keyboard::OnKeyLatched (Byte asciiChar, bool isSpecialKey)
{
    Stroke  stroke = GetStrokeForChar (asciiChar);



    if (isSpecialKey)
    {
        stroke = { (Byte) (asciiChar | s_kSpecialBit), false, false, true };
    }

    m_strokeShift.store   (stroke.isShift,                       memory_order_release);
    m_strokeControl.store (stroke.isControl,                     memory_order_release);
    m_strokeSpecial.store ((stroke.code & s_kSpecialBit) != 0,   memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PressStroke
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Keyboard::PressStroke (const Stroke & stroke)
{
    m_latchedKey.store ((Byte) ((stroke.code & s_kCodeMask) | s_kStrobe), memory_order_release);

    m_strokeShift.store   (stroke.isShift,                       memory_order_release);
    m_strokeControl.store (stroke.isControl,                     memory_order_release);
    m_strokeSpecial.store ((stroke.code & s_kSpecialBit) != 0,   memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapSpecialKey
//
//  The arrows, Tab and Escape. The Apple /// has no Delete key.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Keyboard::MapSpecialKey (AppleSpecialKey key) const
{
    Byte  code = 0;



    switch (key)
    {
    case AppleSpecialKey::Left:     code = kAppleKeyLeft;   break;
    case AppleSpecialKey::Right:    code = kAppleKeyRight;  break;
    case AppleSpecialKey::Up:       code = kAppleKeyUp;     break;
    case AppleSpecialKey::Down:     code = kAppleKeyDown;   break;
    case AppleSpecialKey::Tab:      code = kAppleKeyTab;    break;
    case AppleSpecialKey::Escape:   code = kAppleKeyEscape; break;
    default:                                                break;
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetKbPort
//
//  Shift and Control read as down while the host has them down or the last
//  key's stroke used them, so a pasted uppercase letter reads as shifted.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Keyboard::GetKbPort() const
{
    bool  isShift   = m_shift.load   (memory_order_acquire) || m_strokeShift.load   (memory_order_acquire);
    bool  isControl = m_control.load (memory_order_acquire) || m_strokeControl.load (memory_order_acquire);
    Byte  value     = 0;



    value |= m_anyKeyDown.load (memory_order_acquire)    ? kKbAnyKey  : 0;
    value |= isShift                                     ? 0 : kKbShift;
    value |= isControl                                   ? 0 : kKbControl;
    value |= m_alphaLock.load (memory_order_acquire)     ? 0 : kKbAlphaLock;
    value |= m_apple1.load (memory_order_acquire)        ? 0 : kKbApple1;
    value |= m_apple2.load (memory_order_acquire)        ? 0 : kKbApple2;
    value |= m_strokeSpecial.load (memory_order_acquire) ? kKbSpecial : 0;

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Keyboard::Read (Word address)
{
    Byte  value = 0;



    if (address >= kFirstKbPort && address <= kLastKbPort)
    {
        return GetKbPort();
    }

    value = AppleKeyboard::Read (address);

    if (address >= s_kFirstClear)
    {
        UpdateInterruptLine();
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Keyboard::TryPeekIo (Word address, Byte & value)
{
    if (address >= kFirstKbPort && address <= kLastKbPort)
    {
        value = GetKbPort();
        return true;
    }

    return AppleKeyboard::TryPeekIo (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Alpha Lock is a latching key, so a reset doesn't change it.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Keyboard::Reset()
{
    AppleKeyboard::Reset();

    m_strokeShift.store   (false, memory_order_release);
    m_strokeControl.store (false, memory_order_release);
    m_strokeSpecial.store (false, memory_order_release);

    UpdateInterruptLine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateInterruptLine
//
//  CA2 is low while the data ready flag is set; the VIA flags the falling
//  edge.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Keyboard::UpdateInterruptLine()
{
    bool  isReady = (m_latchedKey.load (memory_order_acquire) & s_kStrobe) != 0;



    if (m_vias != nullptr)
    {
        m_vias->GetE().SetCa2 (!isReady);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRepeatIntervalUs
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple3Keyboard::GetRepeatIntervalUs() const
{
    uint32_t  rate = m_apple2.load (memory_order_acquire) ? kFastRepeatRateHz : kRepeatRateHz;



    return kMicrosecondsPerSecond / rate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  The base keyboard's section, then the modifier keys and the last
//  stroke's Shift, Control and bit 7.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Keyboard::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    hr = AppleKeyboard::SaveState (writer);
    CHR (hr);

    writer.WriteBool (m_shift.load         (memory_order_acquire));
    writer.WriteBool (m_control.load       (memory_order_acquire));
    writer.WriteBool (m_alphaLock.load     (memory_order_acquire));
    writer.WriteBool (m_apple1.load        (memory_order_acquire));
    writer.WriteBool (m_apple2.load        (memory_order_acquire));
    writer.WriteBool (m_strokeShift.load   (memory_order_acquire));
    writer.WriteBool (m_strokeControl.load (memory_order_acquire));
    writer.WriteBool (m_strokeSpecial.load (memory_order_acquire));

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Keyboard::LoadState (StateReader & reader)
{
    HRESULT              hr      = S_OK;
    uint16_t             version = 0;
    std::array<bool, 8>  flags   = {};



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    hr = AppleKeyboard::LoadState (reader);
    CHR (hr);

    for (bool & flag : flags)
    {
        reader.ReadBool (flag);
    }

    hr = reader.EndSection();
    CHR (hr);

    m_shift.store         (flags[0], memory_order_release);
    m_control.store       (flags[1], memory_order_release);
    m_alphaLock.store     (flags[2], memory_order_release);
    m_apple1.store        (flags[3], memory_order_release);
    m_apple2.store        (flags[4], memory_order_release);
    m_strokeShift.store   (flags[5], memory_order_release);
    m_strokeControl.store (flags[6], memory_order_release);
    m_strokeSpecial.store (flags[7], memory_order_release);

Error:
    return hr;
}
