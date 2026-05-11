#include "Pch.h"

#include "MemoryBusCpu.h"
#include "Prng.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBusCpu
//
////////////////////////////////////////////////////////////////////////////////

MemoryBusCpu::MemoryBusCpu (MemoryBus & memoryBus)
    : Cpu6502 (),
      m_memoryBus (memoryBus)
{
    Byte * pBase = memory.data ();
    int    page  = 0;

    // Register this CPU's 64 KB memory[] as the default page-table backing
    // for $0000-$BFFF. The bus's fast-path read/write for RAM pages will
    // land in memory[], keeping PeekByte/PokeByte/GetMemory() coherent
    // with bus-routed reads and writes. EmulatorShell may later remap
    // individual pages (e.g. $0400-$07FF to aux RAM under 80STORE+PAGE2);
    // those calls override these defaults.
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

Byte MemoryBusCpu::ReadByte (Word address)
{
    return m_memoryBus.ReadByte (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBusCpu::WriteByte (Word address, Byte value)
{
    m_memoryBus.WriteByte (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word MemoryBusCpu::ReadWord (Word address)
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

void MemoryBusCpu::WriteWord (Word address, Word value)
{
    WriteByte (address, static_cast<Byte> (value & 0xFF));
    WriteByte (static_cast<Word> (address + 1), static_cast<Byte> (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitForEmulation
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBusCpu::InitForEmulation ()
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

    fill (memory.begin () + 0x0800, memory.begin () + 0x0C00, Byte (0));

    SP = 0xFD;

    status.status                 = 0;
    status.flags.alwaysOne        = 1;
    status.flags.interruptDisable = 1;

    A = 0;
    X = 0;
    Y = 0;

    m_totalCycles = 0;

    PC = ReadWord (resVector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4 / FR-034. Mirrors the 6502 /RESET sequence: forces I=1, sets
//  SP to the post-reset 0xFD value, and reloads PC from $FFFC. RAM, A/X/
//  Y and the cycle counter are preserved (audit §10).
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBusCpu::SoftReset ()
{
    SP = 0xFD;

    status.flags.interruptDisable = 1;
    status.flags.alwaysOne        = 1;

    m_irqLine    = false;
    m_nmiLine    = false;
    m_nmiPending = false;

    PC = ReadWord (resVector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Phase 4 / FR-035. Re-seeds Cpu::memory[$0000-$BFFF) from the shared
//  Prng, zeros page-2 text ($0800-$0BFF) per the InitForEmulation
//  comment, then runs the SoftReset sequence.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBusCpu::PowerCycle (Prng & prng)
{
    prng.Fill (memory.data (), 0xC000);

    fill (memory.begin () + 0x0800, memory.begin () + 0x0C00, Byte (0));

    A             = 0;
    X             = 0;
    Y             = 0;
    m_totalCycles = 0;

    status.status                 = 0;
    status.flags.alwaysOne        = 1;
    status.flags.interruptDisable = 1;

    SoftReset ();
}
