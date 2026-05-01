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
    AppleIIeKeyboard ();

    Byte Read  (Word address) override;
    void Reset () override;

    // Open/Closed Apple button state (set from UI thread)
    void SetOpenApple   (bool pressed) { m_openApple.store (pressed, std::memory_order_release); }
    void SetClosedApple (bool pressed) { m_closedApple.store (pressed, std::memory_order_release); }

    // Override key press to allow lowercase
    void KeyPressRaw (Byte asciiChar);

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    std::atomic<bool> m_openApple{false};
    std::atomic<bool> m_closedApple{false};
};
