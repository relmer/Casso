#pragma once

#include "Pch.h"
#include "Devices/AppleSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeSoftSwitchBank
//
//  Extended IIe soft switches including 80-column and double hi-res.
//
////////////////////////////////////////////////////////////////////////////////

class AppleIIeSoftSwitchBank : public AppleSoftSwitchBank
{
public:
    AppleIIeSoftSwitchBank ();

    Byte Read  (Word address) override;
    void Write (Word address, Byte value) override;
    Word GetEnd () const override { return 0xC07F; }
    void Reset () override;

    bool Is80ColMode    () const { return m_80colMode; }
    bool IsDoubleHiRes  () const { return m_doubleHiRes; }
    bool IsAltCharSet   () const { return m_altCharSet; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    bool m_80colMode   = false;
    bool m_doubleHiRes = false;
    bool m_altCharSet  = false;
};
