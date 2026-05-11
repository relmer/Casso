#pragma once

#include "Pch.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryDevice
//
//  Abstract interface for all bus-attached components.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryDevice
{
public:
    virtual ~MemoryDevice () = default;

    virtual Byte Read     (Word address)              = 0;
    virtual void Write    (Word address, Byte value)  = 0;
    virtual Word GetStart () const                    = 0;
    virtual Word GetEnd   () const                    = 0;
    virtual void Reset    ()                          = 0;

    // Phase 4: split reset semantics. Default SoftReset is a no-op
    // (preserves all device state including any RAM contents). Default
    // PowerCycle forwards to SoftReset, ignoring the supplied Prng. Devices
    // that own DRAM-style storage override PowerCycle to re-seed via the
    // injected Prng (FR-035, audit §10).
    virtual void SoftReset    ()                {}
    virtual void PowerCycle   (Prng & prng)     { (void) prng; SoftReset (); }
};
