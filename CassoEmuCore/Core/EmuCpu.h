#pragma once

#include "Pch.h"
#include "Cpu6502.h"
#include "ICpu.h"
#include "MemoryBus.h"
#include "MemoryBusCpu.h"

#include <memory>




////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu
//
//  Strategy-holder over an ICpu instance. Owns std::unique_ptr<ICpu>
//  (defaulting to a MemoryBusCpu wired to the supplied MemoryBus) and
//  exposes the legacy 6502-flavoured surface that EmulatorShell + tests
//  depend on as forwarders. Future CPU strategies (65C02, fake CPUs for
//  unit tests) plug in via the ICpu-accepting constructor without
//  changing any caller.
//
//  Per FR-030 (CPU strategy abstraction) and the Phase 1 design in
//  specs/004-apple-iie-fidelity.
//
////////////////////////////////////////////////////////////////////////////////

class EmuCpu
{
public:
    // Default: construct an internal MemoryBusCpu bound to `memoryBus`.
    explicit EmuCpu (MemoryBus & memoryBus);

    // Strategy-injection constructor. Caller supplies the CPU; EmuCpu does
    // not know how it was wired. The instance MUST also be a Cpu6502 so
    // the 6502-specific debug surface (PeekByte/PokeByte/GetMemory/...) can
    // continue to be served. Asserts on a non-6502 strategy.
    EmuCpu (MemoryBus & memoryBus, std::unique_ptr<ICpu> cpu);

    ~EmuCpu () = default;

    // Strategy access — for callers that want to talk to the CPU directly
    // through one of the contracts.
    ICpu *           GetCpu        () { return m_cpu.get (); }
    const ICpu *     GetCpu        () const { return m_cpu.get (); }
    Cpu6502 *        GetCpu6502    () { return m_cpu6502; }
    const Cpu6502 *  GetCpu6502    () const { return m_cpu6502; }

    // ICpu surface
    HRESULT          Reset            ()                 { return m_cpu->Reset (); }
    HRESULT          Step             (uint32_t & cyc)   { return m_cpu->Step (cyc); }
    void             SetInterruptLine (CpuInterruptKind k, bool a) { m_cpu->SetInterruptLine (k, a); }
    uint64_t         GetCycleCount    () const           { return m_cpu->GetCycleCount (); }

    // Cycle counter pass-through (Cpu6502 hosts the accumulator)
    uint64_t         GetTotalCycles      () const  { return m_cpu6502->GetCycleCount (); }
    void             ResetCycles         ()        { m_cpu6502->ResetCycles (); }
    uint64_t *       GetCycleCounterPtr  ()        { return m_cpu6502->GetCycleCounterPtr (); }
    void             AddCycles           (Byte n)  { m_cpu6502->AddCycles (n); }

    // 6502 execution surface
    void             StepOne                  ()                              { m_cpu6502->StepOne (); }
    Byte             GetLastInstructionCycles () const                        { return m_cpu6502->GetLastInstructionCycles (); }

    // 6502 register accessors
    Word             GetPC      () const  { return m_cpu6502->GetPC (); }
    void             SetPC      (Word pc) { m_cpu6502->SetPC (pc); }
    Byte             GetA       () const  { return m_cpu6502->GetA (); }
    Byte             GetX       () const  { return m_cpu6502->GetX (); }
    Byte             GetY       () const  { return m_cpu6502->GetY (); }
    Byte             GetSP      () const  { return m_cpu6502->GetSP (); }

    // 6502 memory surface
    Byte             PeekByte   (Word address) const            { return m_cpu6502->PeekByte (address); }
    void             PokeByte   (Word address, Byte value)      { m_cpu6502->PokeByte (address, value); }
    Word             PeekWord   (Word address) const            { return m_cpu6502->PeekWord (address); }
    const Byte *     GetMemory  () const                        { return m_cpu6502->GetMemory (); }

    // Bus-routed accessors. Forwarded directly to MemoryBus so callers
    // observe the same I/O dispatch the CPU sees during Step. (Cpu's own
    // ReadByte/WriteByte are protected and intentionally inaccessible.)
    Byte             ReadByte   (Word address)                  { return m_memoryBus.ReadByte (address); }
    void             WriteByte  (Word address, Byte value)      { m_memoryBus.WriteByte (address, value); }
    Word             ReadWord   (Word address);
    void             WriteWord  (Word address, Word value);

    // Power-on initialization (DRAM randomization + reset-vector fetch).
    // No-op if the strategy is not a MemoryBusCpu.
    void             InitForEmulation ();

private:
    MemoryBus &              m_memoryBus;
    std::unique_ptr<ICpu>    m_cpu;
    Cpu6502 *                m_cpu6502 = nullptr;
};
