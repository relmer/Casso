#include "Pch.h"

#include "EmuCpu.h"
#include "MemoryBusCpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu
//
////////////////////////////////////////////////////////////////////////////////

EmuCpu::EmuCpu (MemoryBus & memoryBus)
    : m_memoryBus (memoryBus),
      m_cpu       (std::make_unique<MemoryBusCpu> (memoryBus))
{
    m_cpu6502 = static_cast<Cpu6502 *> (static_cast<MemoryBusCpu *> (m_cpu.get ()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu (strategy-injection)
//
////////////////////////////////////////////////////////////////////////////////

EmuCpu::EmuCpu (MemoryBus & memoryBus, std::unique_ptr<ICpu> cpu)
    : m_memoryBus (memoryBus),
      m_cpu       (std::move (cpu))
{
    // The 6502-flavoured pass-through surface (PeekByte/PokeByte/registers)
    // requires that the strategy is a Cpu6502. If it isn't, the typed
    // accessors are unavailable; downstream callers must use GetCpu()
    // instead. dynamic_cast is used so a future non-6502 ICpu surfaces
    // m_cpu6502 == nullptr rather than an unsafe reinterpretation.
    m_cpu6502 = dynamic_cast<Cpu6502 *> (m_cpu.get ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word EmuCpu::ReadWord (Word address)
{
    Byte lo = m_memoryBus.ReadByte (address);
    Byte hi = m_memoryBus.ReadByte (static_cast<Word> (address + 1));
    return static_cast<Word> (lo | (hi << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void EmuCpu::WriteWord (Word address, Word value)
{
    m_memoryBus.WriteByte (address, static_cast<Byte> (value & 0xFF));
    m_memoryBus.WriteByte (static_cast<Word> (address + 1), static_cast<Byte> (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitForEmulation
//
////////////////////////////////////////////////////////////////////////////////

void EmuCpu::InitForEmulation ()
{
    MemoryBusCpu * pBusCpu = dynamic_cast<MemoryBusCpu *> (m_cpu.get ());

    if (pBusCpu)
    {
        pBusCpu->InitForEmulation ();
    }
}
