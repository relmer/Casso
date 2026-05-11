#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"

class AppleIIeMmu;





////////////////////////////////////////////////////////////////////////////////
//
//  CxxxRomRouter
//
//  Memory-bus device claiming $C100-$CFFF on the //e. Routes reads to one
//  of three sources based on the AppleIIeMmu's flag state:
//
//    - INTCXROM=1: all $C100-$CFFF reads come from the internal //e ROM
//      (audit §8 / Sather UTAIIe §5-25). Slot ROMs are shadowed.
//    - INTCXROM=0 + SLOTC3ROM=0: $C300-$C3FF reads come from internal ROM
//      (the 80-col firmware lives here). Reading $C3xx in this state also
//      latches INTC8ROM=1 so the $C800-$CFFF expansion-ROM window also
//      maps to internal.
//    - INTCXROM=0 + SLOTC3ROM=1: $C300-$C3FF reads come from slot 3.
//    - INTCXROM=0 (other slot pages): $CS00-$CSFF reads come from the
//      slot-S ROM (one page each, 256 bytes).
//    - $C800-$CFFF: routed to internal ROM when INTC8ROM=1, otherwise
//      floating bus (audit §8). A read of $CFFF clears INTC8ROM
//      (deactivates the expansion-ROM window).
//
//  Writes are ignored (ROM space) but still trigger the $CFFF
//  INTC8ROM-clear side effect for symmetry with reads.
//
////////////////////////////////////////////////////////////////////////////////

class CxxxRomRouter : public MemoryDevice
{
public:
    explicit CxxxRomRouter (AppleIIeMmu & mmu);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC100; }
    Word GetEnd   () const override { return 0xCFFF; }
    void Reset    () override;

    void SetInternalRom (vector<Byte> data);
    void SetSlotRom     (int slot, vector<Byte> data);
    bool HasSlotRom     (int slot) const;

private:
    Byte ResolveByte    (Word address);

    AppleIIeMmu &  m_mmu;
    vector<Byte>   m_internal;
    vector<Byte>   m_slotRom[8];
};
