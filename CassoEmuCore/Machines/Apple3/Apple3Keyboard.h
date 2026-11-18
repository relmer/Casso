#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/AppleKeyboard.h"

class Apple3SystemVias;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Keyboard
//
//  The Apple /// keyboard. $C000-$C007 read the KA port: the encoder's
//  code in bits 0-6 and the data ready flag in bit 7. $C008-$C00F read the
//  KB port: any key down in bit 0, then Shift, Control, Alpha Lock, Apple 1
//  and Apple 2 in bits 1-5, each 0 while down or set, and bit 7 of the
//  encoder's code, which is set for the special keys (Escape, Tab, Space,
//  the arrows and the keypad). $C010-$C01F clear the data ready flag.
//
//  The encoder sends the same letter code with or without Shift; SOS turns
//  it into lower or upper case from the KB port. So a typed character is
//  turned back into the key and the Shift and Control that produce it, as
//  the table on SVC page 8.6 lists them. The data ready flag drives the E
//  VIA's CA2 line low, the keyboard interrupt.
//
//  A key repeats after half a second at 10 per second, or 30 per second
//  with Apple 2 down.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Keyboard : public AppleKeyboard
{
public:
    static constexpr Word      kFirstKbPort   = 0xC008;
    static constexpr Word      kLastKbPort    = 0xC00F;

    static constexpr Byte      kKbAnyKey      = 0x01;
    static constexpr Byte      kKbShift       = 0x02;
    static constexpr Byte      kKbControl     = 0x04;
    static constexpr Byte      kKbAlphaLock   = 0x08;
    static constexpr Byte      kKbApple1      = 0x10;
    static constexpr Byte      kKbApple2      = 0x20;
    static constexpr Byte      kKbSpecial     = 0x80;

    static constexpr uint32_t  kRepeatRateHz     = 10;
    static constexpr uint32_t  kFastRepeatRateHz = 30;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('K', 'B', 'D', '3');
    static constexpr uint16_t  kStateVersion = 1;

    //  How the encoder produces a code: the key's code, bit 7 set for a
    //  special key, and whether Shift and Control are down.
    struct Stroke
    {
        Byte  code      = 0;
        bool  isShift   = false;
        bool  isControl = false;
        bool  isValid   = false;
    };

    Byte     Read           (Word address) override;
    bool     TryPeekIo      (Word address, Byte & value) override;
    void     Reset          () override;

    void     AttachVias     (Apple3SystemVias * vias) { m_vias = vias; }

    //  Modifier keys from the host, UI thread.
    void     SetShift       (bool down)  { m_shift.store     (down, memory_order_release); }
    void     SetControl     (bool down)  { m_control.store   (down, memory_order_release); }
    void     SetAlphaLock   (bool on)    { m_alphaLock.store (on,   memory_order_release); }
    void     SetApple1      (bool down)  { m_apple1.store    (down, memory_order_release); }
    void     SetApple2      (bool down)  { m_apple2.store    (down, memory_order_release); }

    //  A key with no typed character, such as a keypad key: its code from
    //  the table, bit 7 included. UI thread.
    void     PressStroke    (const Stroke & stroke);

    //  CPU thread: drives the E VIA's CA2 line from the data ready flag.
    void     UpdateInterruptLine ();

    Byte     GetKbPort      () const;
    Byte     MapSpecialKey  (AppleSpecialKey key) const override;

    static Stroke  GetStrokeForChar (Byte asciiChar);

    HRESULT  SaveState      (StateWriter & writer) const override;
    HRESULT  LoadState      (StateReader & reader) override;

protected:
    Byte      TranslateTypedChar  (Byte ch) const override;
    void      OnKeyLatched        (Byte asciiChar, bool isSpecialKey) override;
    uint32_t  GetRepeatIntervalUs () const override;

private:
    Apple3SystemVias  * m_vias           = nullptr;
    atomic<bool>        m_shift          {false};
    atomic<bool>        m_control        {false};
    atomic<bool>        m_alphaLock      {false};
    atomic<bool>        m_apple1         {false};
    atomic<bool>        m_apple2         {false};

    //  The last key's Shift, Control and bit 7, from the stroke that
    //  produced it.
    atomic<bool>        m_strokeShift    {false};
    atomic<bool>        m_strokeControl  {false};
    atomic<bool>        m_strokeSpecial  {false};
};
