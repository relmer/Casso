#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





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

    // Called from EmulatorShell when a key event arrives (UI thread)
    void KeyPress (Byte asciiChar);

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
