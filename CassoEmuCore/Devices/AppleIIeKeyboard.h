#pragma once

#include "Pch.h"
#include "Devices/AppleKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeKeyboard
//
//  Full keyboard with lowercase, modifier keys, Open/Closed Apple.
//
////////////////////////////////////////////////////////////////////////////////

class AppleIIeKeyboard : public AppleKeyboard
{
public:
    AppleIIeKeyboard (MemoryBus * bus = nullptr);

    Byte Read  (Word address) override;
    void Write (Word address, Byte value) override;
    void Reset () override;

    // Open/Closed Apple button state (set from UI thread)
    void SetOpenApple   (bool pressed) { m_openApple.store (pressed, memory_order_release); }
    void SetClosedApple (bool pressed) { m_closedApple.store (pressed, memory_order_release); }

    // Sibling softswitch device that owns soft switches in $C000-$C00F
    // (80STORE, RAMRD/WRT, ALTZP, 80COL, ALTCHARSET, etc.).
    // The keyboard's address range covers these, so we forward to the
    // softswitch instead of handling them here.
    void SetSoftSwitchSibling (class AppleIIeSoftSwitchBank * sibling) { m_softSwitchSibling = sibling; }

    // Sibling MMU that owns RAMRD/RAMWRT/ALTZP/INTCXROM/SLOTC3ROM state.
    // Used for $C013-$C017 status bit-7 reads.
    void SetMmu (class AppleIIeMmu * mmu) { m_mmu = mmu; }

    // Override key press to allow lowercase
    void KeyPressRaw (Byte asciiChar);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    MemoryBus *                    m_bus = nullptr;
    class AppleIIeSoftSwitchBank * m_softSwitchSibling = nullptr;
    class AppleIIeMmu *            m_mmu               = nullptr;
    atomic<bool>                   m_openApple{false};
    atomic<bool>                   m_closedApple{false};
};
