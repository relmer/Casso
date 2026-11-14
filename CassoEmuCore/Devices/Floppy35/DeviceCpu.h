#pragma once

#include "Pch.h"

class MemoryBus;
class MemoryBusCpu;
class MemoryDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceCpu
//
//  The 65C02 inside a card or a drive, on a separate bus: the card's
//  memory map is one memory device over the whole 64K. The owner runs it to
//  a cycle count, which follows the Apple's clock.
//
////////////////////////////////////////////////////////////////////////////////

class DeviceCpu
{
public:
    DeviceCpu();
    ~DeviceCpu();

    HRESULT   Initialize (MemoryDevice & memory);

    //  Serves pages straight from `read` and `write`, page `firstPage` from
    //  their first byte. A null pointer sends the pages' accesses to the
    //  memory device instead.
    void      MapPages   (int firstPage, int pageCount, Byte * read, Byte * write);

    //  The 65C02's RESET: the PC from $FFFC.
    void      Reset      ();

    //  Runs instructions until the cycle count is at least `targetCycle`.
    void      RunUntil   (uint64_t targetCycle);

    //  Moves the cycle count to `cycle` without running instructions, for a
    //  device in reset, or idle for long enough that it doesn't matter.
    void      SkipTo     (uint64_t cycle) { m_cycles = std::max (m_cycles, cycle); }

    uint64_t  GetCycles  () const { return m_cycles; }
    Word      GetPC      () const;

    const uint64_t * GetCyclePtr () const { return &m_cycles; }

private:
    std::unique_ptr<MemoryBus>     m_bus;
    std::unique_ptr<MemoryBusCpu>  m_cpu;
    uint64_t                       m_cycles = 0;
};
