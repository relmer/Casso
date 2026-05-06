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
    Byte * pBase = memory.data ();
    int    page  = 0;

    // Register this CPU's 64 KB memory[] as the default page-table backing
    // for $0000-$BFFF. The bus's fast-path read/write for RAM pages will
    // land in memory[], keeping PeekByte/PokeByte/GetMemory() coherent with
    // bus-routed reads and writes. EmulatorShell may later remap individual
    // pages (e.g. $0400-$07FF to aux RAM under 80STORE+PAGE2); those calls
    // override these defaults.
    for (page = 0x00; page <= 0xBF; page++)
    {
        m_memoryBus.SetReadPage  (page, pBase + (page * 0x100));
        m_memoryBus.SetWritePage (page, pBase + (page * 0x100));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte EmuCpu::ReadByte (Word address)
{
    // All addresses go through MemoryBus. The bus has a fast page table for
    // $0000-$BFFF that bypasses device dispatch. $C000+ uses device dispatch.
    return m_memoryBus.ReadByte (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void EmuCpu::WriteByte (Word address, Byte value)
{
    // All addresses go through MemoryBus. The bus's page table handles RAM
    // writes directly to the correct backing buffer (main, aux, or LC RAM).
    // I/O writes ($C000+) dispatch to devices.
    m_memoryBus.WriteByte (address, value);
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
