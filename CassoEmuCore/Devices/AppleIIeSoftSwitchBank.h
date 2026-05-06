#pragma once

#include "Pch.h"
#include "Devices/AppleSoftSwitchBank.h"

class AppleIIeMmu;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeSoftSwitchBank
//
//  Extended IIe soft switches including 80-column and double hi-res.
//  $C000-$C00B write-switches are forwarded to the AppleIIeMmu (set via
//  SetMmu); the MMU owns the canonical RAMRD/RAMWRT/ALTZP/80STORE/INTCXROM/
//  SLOTC3ROM flag state. Display-only flags (80COL, ALTCHARSET, PAGE2,
//  HIRES, TEXT, MIXED, DHIRES) remain owned here.
//
////////////////////////////////////////////////////////////////////////////////

class AppleIIeSoftSwitchBank : public AppleSoftSwitchBank
{
public:
    AppleIIeSoftSwitchBank (MemoryBus * bus = nullptr);

    Byte Read  (Word address) override;
    void Write (Word address, Byte value) override;
    Word GetEnd () const override { return 0xC07F; }
    void Reset () override;

    bool Is80ColMode    () const { return m_80colMode; }
    bool IsDoubleHiRes  () const { return m_doubleHiRes; }
    bool IsAltCharSet   () const { return m_altCharSet; }
    bool Is80Store      () const;

    void SetMmu (AppleIIeMmu * mmu) { m_mmu = mmu; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    MemoryBus *     m_bus         = nullptr;
    AppleIIeMmu *   m_mmu         = nullptr;
    bool            m_80colMode   = false;
    bool            m_doubleHiRes = false;
    bool            m_altCharSet  = false;
};
