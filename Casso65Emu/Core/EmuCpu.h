#pragma once

#include "Pch.h"
#include "Cpu.h"
#include "MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu
//
//  Subclass of Casso65Core Cpu that routes memory access through MemoryBus.
//
////////////////////////////////////////////////////////////////////////////////

class EmuCpu : public Cpu
{
public:
    explicit EmuCpu (MemoryBus & memoryBus);

    // Override memory operations to route through MemoryBus
    Byte ReadByte  (Word address) override;
    void WriteByte (Word address, Byte value) override;
    Word ReadWord  (Word address) override;
    void WriteWord (Word address, Word value) override;

    // Cycle tracking
    uint64_t GetTotalCycles () const { return m_totalCycles; }
    void     ResetCycles    ()      { m_totalCycles = 0; }

    // Initialize for emulation: set SP and fetch reset vector
    void InitForEmulation ();

private:
    MemoryBus & m_memoryBus;
    uint64_t    m_totalCycles;
};
