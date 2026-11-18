#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/Cpu65C02.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryBusCpu.h"
#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/DeviceCpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceCpu
//
////////////////////////////////////////////////////////////////////////////////

DeviceCpu::DeviceCpu() :
    m_bus (std::make_unique<MemoryBus>())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DeviceCpu
//
////////////////////////////////////////////////////////////////////////////////

DeviceCpu::~DeviceCpu() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeviceCpu::Initialize (MemoryDevice & memory)
{
    constexpr int  kPages = 0x100;



    m_bus->AddDevice (&memory);
    m_cpu = std::make_unique<Cpu65C02> (*m_bus);

    //  The CPU maps its built-in RAM below $C000. The device serves every
    //  page until the owner maps pages to memory directly.
    MapPages (0, kPages, nullptr, nullptr);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapPages
//
////////////////////////////////////////////////////////////////////////////////

void DeviceCpu::MapPages (int firstPage, int pageCount, Byte * read, Byte * write)
{
    constexpr size_t  kPageBytes = 0x100;



    for (int i = 0; i < pageCount; i++)
    {
        m_bus->SetReadPage  (firstPage + i, read  != nullptr ? read  + (size_t) i * kPageBytes : nullptr);
        m_bus->SetWritePage (firstPage + i, write != nullptr ? write + (size_t) i * kPageBytes : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void DeviceCpu::Reset()
{
    if (m_cpu != nullptr)
    {
        m_cpu->SoftReset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunUntil
//
////////////////////////////////////////////////////////////////////////////////

void DeviceCpu::RunUntil (uint64_t targetCycle, CatchUpKind kind)
{
    uint32_t  cycles  = 0;
    uint64_t  skipped = 0;
    HRESULT   hr      = S_OK;



    while (m_cpu != nullptr && m_cycles < targetCycle)
    {
        if (m_idleLoop && m_cpu->GetPC() == m_idlePc)
        {
            skipped   = m_idleLoop (targetCycle - m_cycles);
            m_cycles += skipped;

            if (kind == CatchUpKind::BetweenFrames && skipped > 0 && targetCycle - m_cycles < m_idlePass)
            {
                break;
            }
        }

        hr = m_cpu->Step (cycles);

        if (FAILED (hr) || cycles == 0)
        {
            m_cycles = targetCycle;
            break;
        }

        m_cycles += cycles;
        m_instructions++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetIdleLoop
//
////////////////////////////////////////////////////////////////////////////////

void DeviceCpu::SetIdleLoop (Word pc, uint64_t passCycles, IdleLoopFn skip)
{
    m_idlePc   = pc;
    m_idlePass = passCycles;
    m_idleLoop = std::move (skip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPC
//
////////////////////////////////////////////////////////////////////////////////

Word DeviceCpu::GetPC() const
{
    return (m_cpu != nullptr) ? m_cpu->GetPC() : (Word) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRegisters
//
////////////////////////////////////////////////////////////////////////////////

Cpu6502Registers DeviceCpu::GetRegisters() const
{
    return (m_cpu != nullptr) ? m_cpu->GetRegisters() : Cpu6502Registers {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRegisters
//
////////////////////////////////////////////////////////////////////////////////

void DeviceCpu::SetRegisters (const Cpu6502Registers & regs)
{
    if (m_cpu != nullptr)
    {
        m_cpu->SetRegisters (regs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceCpu::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeviceCpu::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    m_cpu->SaveRegisters (writer);

    writer.WriteUInt64 (m_cycles);
    writer.WriteUInt64 (m_instructions);
    writer.WriteUInt64 (m_idlePass);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceCpu::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeviceCpu::LoadState (StateReader & reader)
{
    HRESULT  hr = S_OK;



    m_cpu->LoadRegisters (reader, MemoryBusCpu::kStateVersion);

    reader.ReadUInt64 (m_cycles);
    reader.ReadUInt64 (m_instructions);
    reader.ReadUInt64 (m_idlePass);

    hr = reader.GetResult();

    return hr;
}
