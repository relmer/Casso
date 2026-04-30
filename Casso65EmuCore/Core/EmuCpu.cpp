#include "Pch.h"

#include "EmuCpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu
//
////////////////////////////////////////////////////////////////////////////////

EmuCpu::EmuCpu (MemoryBus & memoryBus)
    : Cpu (),
      m_memoryBus   (memoryBus),
      m_totalCycles (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte EmuCpu::ReadByte (Word address)
{
    // For I/O range ($C000-$CFFF), always read from bus (soft switches, keyboard, etc.)
    // For everything else, internal memory[] is authoritative (synced by WriteByte)
    if (address >= 0xC000 && address <= 0xCFFF)
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
    // Write to both the bus (for devices/video) and internal memory[]
    // (for CpuOperations which read from memory[] directly)
    m_memoryBus.WriteByte (address, value);
    memory[address] = value;
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

void EmuCpu::InitForEmulation ()
{
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
