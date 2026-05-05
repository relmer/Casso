#include "Pch.h"

#include "EmuCpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu
//
////////////////////////////////////////////////////////////////////////////////

EmuCpu::EmuCpu (MemoryBus & memoryBus)
    : Cpu(),
      m_memoryBus (memoryBus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte EmuCpu::ReadByte (Word address)
{
    // $C000+: route through MemoryBus for I/O, ROM, and Language Card bank switching
    if (address >= 0xC000)
    {
        return m_memoryBus.ReadByte (address);
    }

    return memory[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void EmuCpu::WriteByte (Word address, Byte value)
{
    // $C000+: route through MemoryBus only (devices handle I/O, LC RAM, etc.)
    // $0000-$BFFF: write to both bus (for video/device notifications) and memory[]
    m_memoryBus.WriteByte (address, value);

    if (address < 0xC000)
    {
        memory[address] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word EmuCpu::ReadWord (Word address)
{
    Byte lo = ReadByte (address);
    Byte hi = ReadByte (static_cast<Word> (address + 1));
    return static_cast<Word> (lo | (hi << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void EmuCpu::WriteWord (Word address, Word value)
{
    WriteByte (address, static_cast<Byte> (value & 0xFF));
    WriteByte (static_cast<Word> (address + 1), static_cast<Byte> (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitForEmulation
//
////////////////////////////////////////////////////////////////////////////////

void EmuCpu::InitForEmulation()
{
    // Randomize RAM ($0000-$BFFF) to simulate real DRAM power-on state.
    // Note: the //e ROM's 80STORE handling is incomplete (see issue), so
    // page 2 text memory ($0800-$0BFF) is zeroed to avoid visual artifacts
    // when 80STORE redirects $C054/$C055 to aux memory bank selection.
    mt19937                          rng (random_device{}());
    uniform_int_distribution<int>    dist (0, 255);

    for (size_t i = 0; i < 0xC000; i++)
    {
        memory[i] = static_cast<Byte> (dist (rng));
    }

    fill (memory.begin() + 0x0800, memory.begin() + 0x0C00, Byte (0));

    // Set initial CPU state
    SP = 0xFD;

    status.status          = 0;
    status.flags.alwaysOne = 1;
    status.flags.interruptDisable = 1;

    A = 0;
    X = 0;
    Y = 0;

    m_totalCycles = 0;

    // Fetch reset vector from ROM via bus
    PC = ReadWord (resVector);
}
