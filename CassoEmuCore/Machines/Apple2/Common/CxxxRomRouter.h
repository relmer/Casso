#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"

class Apple2eMmu;





////////////////////////////////////////////////////////////////////////////////
//
//  CxxxRomRouter
//
//  Memory-bus device claiming $C100-$CFFF on the //e. Routes reads to one
//  of three sources based on the Apple2eMmu's flag state:
//
//    - INTCXROM=1: all $C100-$CFFF reads come from the internal //e ROM
//      (audit §8 / Sather UTAIIe §5-25). Slot ROMs are shadowed.
//    - INTCXROM=0 + SLOTC3ROM=0: $C300-$C3FF reads come from internal ROM
//      (the 80-col firmware lives here).
//    - INTCXROM=0 + SLOTC3ROM=1: $C300-$C3FF reads come from slot 3.
//    - INTCXROM=0 (other slot pages): $CS00-$CSFF reads come from the
//      slot-S ROM (one page each, 256 bytes).
//    - $C800-$CFFF: routed to internal ROM when INTC8ROM=1, otherwise
//      floating bus (audit §8).
//
//  Any access to $C3xx while SLOTC3ROM=0 sets INTC8ROM, whatever INTCXROM
//  holds, so the $C800-$CFFF expansion-ROM window maps to internal. Any
//  access to $CFFF clears it. Reads and writes alike apply both; writes
//  are otherwise ignored (ROM space).
//
////////////////////////////////////////////////////////////////////////////////

class CxxxRomRouter : public MemoryDevice
{
public:
    explicit CxxxRomRouter (Apple2eMmu & mmu);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC100; }
    Word GetEnd   () const override { return 0xCFFF; }
    void Reset    () override;

    void SetInternalRom (vector<Byte> data);
    void SetSlotRom     (int slot, vector<Byte> data);
    bool HasSlotRom     (int slot) const;

    // The images as loaded, for the machine's ROM identity.
    const vector<Byte> & GetInternalRom () const         { return m_internal; }
    const vector<Byte> & GetSlotRom     (int slot) const { return m_slotRom[slot]; }

    // Page-table read pointer for a $C100-$CFFF page ($C1-$CF), or null if the
    // page must stay on the Read handler. Only the //c (no external slots)
    // serves the whole window as static internal ROM, so only there is a
    // pointer returned -- and even then the reactive pages ($C3xx latches
    // INTC8ROM, $CFFF clears it) return null so their read side effects still
    // run, and a slot page with a registered I/O device returns null so its
    // reads reach the device as its writes do. On the //e the window is
    // arbitrated per access, so every page is null (handler). The pointer is
    // into m_internal, which SetInternalRom move-reassigns on a //c $C028
    // bank flip, so the MMU must re-query after any AttachInternalCxxxRom.
    Byte * GetFastMapReadPtr (int page);

    // Apple //c: there are no external card slots, so the whole $C100-$CFFF
    // window (including the $C800 expansion space) is always the internal
    // firmware regardless of the INTCXROM/SLOTC3ROM/INTC8ROM switches -- the
    // //c ROM jumps straight into $C800 with those switches clear. On the //e
    // this stays false and the full slot/expansion arbitration applies.
    void SetNoExternalSlots (bool v) { m_noExternalSlots = v; }

    // Register an active I/O device (e.g. a Mockingboard) that owns a
    // slot's $Cn00 page. When slot cards are visible (INTCXROM=0) reads
    // and writes to that page are delegated to the device instead of
    // resolving to slot ROM. Caller-owned; pass nullptr to detach.
    void SetSlotIoDevice (int slot, MemoryDevice * device);

    // Side-effect-free read for the debugger: the byte a CPU read would
    // return, without latching or clearing INTC8ROM. A slot page owned by an
    // I/O device is not readable this way, since reading a device has effects.
    bool TryPeek (Word address, Byte & value) const;

    // The debugger's ROM patch: one byte of whichever image the switches
    // select for address, internal or slot, so TryPeek and the CPU then read
    // it. False where no image answers (a device page, the floating bus).
    bool TryPatch (Word address, Byte value);

    // True when address resolves to the internal ROM image rather than a slot
    // ROM or the floating bus, under the current MMU switches.
    bool IsInternalRomSelected (Word address) const;

private:
    Byte           ResolveByte            (Word address) const;
    MemoryDevice * GetSlotIoDevice        (Word address) const;
    void           ApplyAccessSideEffects (Word address);

    Apple2eMmu &   m_mmu;
    vector<Byte>   m_internal;
    vector<Byte>   m_slotRom[8];
    bool           m_noExternalSlots = false;
    MemoryDevice * m_slotIoDevice[8] = {};

    // True while any slot has an I/O device registered (see SetSlotIoDevice).
    // Lets the //c read fast path assert "no slot delegation possible" with a
    // single bool test instead of scanning m_slotIoDevice on every access.
    bool           m_hasSlotIoDevice = false;
};
