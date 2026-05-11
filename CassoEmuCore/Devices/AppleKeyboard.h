#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple II key codes
//
//  The Apple II keyboard maps special keys to ASCII control characters.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte kAppleKeyLeft    = 0x08;   // Backspace / cursor left
static constexpr Byte kAppleKeyRight   = 0x15;   // NAK / cursor right
static constexpr Byte kAppleKeyUp      = 0x0B;   // VT / cursor up
static constexpr Byte kAppleKeyDown    = 0x0A;   // LF / cursor down
static constexpr Byte kAppleKeyEscape  = 0x1B;   // Escape
static constexpr Byte kAppleKeyDelete  = 0x7F;   // Delete





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyboard
//
//  Apple II/II+ uppercase-only keyboard mapped at $C000/$C010.
//  $C000: Read returns last key pressed with bit 7 as strobe.
//  $C010: Any read clears the strobe (bit 7 of $C000).
//
////////////////////////////////////////////////////////////////////////////////

class AppleKeyboard : public MemoryDevice
{
public:
    AppleKeyboard ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC000; }
    Word GetEnd   () const override { return 0xC01F; }
    void Reset    () override;
    void SoftReset () override;

    // Called from EmulatorShell when a key event arrives (UI thread)
    void KeyPress (Byte asciiChar);

    // Check if the strobe is clear (CPU has consumed the previous key)
    bool IsStrobeClear () const { return (m_latchedKey.load (memory_order_acquire) & 0x80) == 0; }

    // Read-only floating-bus accessor for the //e soft-switch bank
    // ($C011-$C01F status reads): returns the data bits 0-6 of the
    // latched key without clearing the strobe. (Phase 6 / FR-001 /
    // audit §1.2, §4.) Independent of the strobe-bit-7 state.
    Byte GetLatchedKeyDataBits () const
    {
        return static_cast<Byte> (m_latchedKey.load (memory_order_acquire) & 0x7F);
    }

    // Called from EmulatorShell for special keys (UI thread)
    void SetKeyDown (bool down) { m_anyKeyDown.store (down, memory_order_release); }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    Byte TranslateToUppercase (Byte ch) const;

    // m_latchedKey bit 7 = strobe (new key available).  Atomic because
    // KeyPress is called from the UI thread while Read is called from
    // the CPU thread.
    atomic<Byte>   m_latchedKey{0};
    atomic<bool>   m_anyKeyDown{false};
};
