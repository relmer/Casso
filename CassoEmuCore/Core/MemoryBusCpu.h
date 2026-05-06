#pragma once

#include "Pch.h"
#include "Cpu6502.h"
#include "MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBusCpu
//
//  Concrete Cpu6502 strategy that routes ReadByte/WriteByte through the
//  emulator MemoryBus instead of the inherited Cpu::memory[] array. The
//  Cpu::memory[] vector is registered as the default RAM backing for the
//  bus's $00-$BF page table at construction time, keeping PeekByte /
//  PokeByte / GetMemory() coherent with bus-routed accesses.
//
//  Used by EmuCpu via std::unique_ptr<ICpu>; EmulatorShell holds the
//  EmuCpu wrapper and never sees this type directly.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryBusCpu : public Cpu6502
{
public:
    explicit MemoryBusCpu (MemoryBus & memoryBus);

    Byte    ReadByte         (Word address) override;
    void    WriteByte        (Word address, Byte value) override;
    Word    ReadWord         (Word address) override;
    void    WriteWord        (Word address, Word value) override;

    // Randomize main RAM (DRAM power-on simulation), set initial register
    // state and load PC from the reset vector via the bus.
    void    InitForEmulation ();

private:
    MemoryBus &  m_memoryBus;
};
