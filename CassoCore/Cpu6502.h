#pragma once

#include "Cpu.h"
#include "I6502DebugInfo.h"
#include "ICpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu6502
//
//  Concrete 6502-family CPU implementation that exposes the family-agnostic
//  ICpu strategy interface and the family-specific I6502DebugInfo
//  register-inspection interface. Inherits the existing Cpu execution core
//  unchanged — Cpu6502 is a thin adapter that adds:
//    - HRESULT-returning Reset/Step over Cpu's void surface
//    - SetInterruptLine + IRQ/NMI vectored dispatch state
//    - GetCycleCount accumulator (m_totalCycles)
//    - GetRegisters / SetRegisters debug accessors
//
////////////////////////////////////////////////////////////////////////////////

class Cpu6502 : public Cpu, public ICpu, public I6502DebugInfo
{
public:
                                  Cpu6502 ();
                                  ~Cpu6502 () override = default;

    // ICpu
    HRESULT                       Reset            () override;
    HRESULT                       Step             (uint32_t & outCycles) override;
    void                          SetInterruptLine (CpuInterruptKind kind, bool asserted) override;
    uint64_t                      GetCycleCount    () const override { return m_totalCycles; }

    // Cycle-counter mutators used by host wiring (e.g. EmuCpu, speaker
    // sample timing). The accumulator is shared with ICpu::GetCycleCount
    // and advances once per instruction at the post-StepOne rollup.
    void                          AddCycles          (Byte n) { m_totalCycles += n; }
    void                          ResetCycles        ()       { m_totalCycles = 0; m_busCycle = 0; }
    uint64_t *                    GetCycleCounterPtr ()       { return &m_totalCycles; }

    // Issue #67: sub-instruction bus-cycle estimate. Unlike m_totalCycles
    // (which only advances at the post-StepOne rollup), this is current to
    // the in-flight memory access, so a $C0Ex disk read sees rotational
    // position at the access cycle rather than the instruction boundary.
    // Matches AppleWin attributing the disk update to the exact bus cycle.
    uint64_t *                    GetBusCyclePtr     ()       { return &m_busCycle; }

    // I6502DebugInfo
    Cpu6502Registers              GetRegisters     () const override;
    void                          SetRegisters     (const Cpu6502Registers & regs) override;

    // Test/debug helpers — not part of ICpu, used by CpuIrqTests to inspect
    // the latched interrupt line state independent of dispatch.
    bool                          IsIrqLineAsserted () const { return m_irqLine; }
    bool                          IsNmiLineAsserted () const { return m_nmiLine; }
    bool                          IsNmiPending      () const { return m_nmiPending; }

    // Host-driven interrupt dispatch for run loops that own their own cycle
    // accounting (StepOne + AddCycles) rather than the self-accounting Step().
    // Dispatches a pending NMI edge or unmasked IRQ via DispatchVector, records
    // the 7-cycle cost in m_lastCycles (so GetLastInstructionCycles reports it),
    // and leaves m_totalCycles to the caller's AddCycles -- so an interrupt step
    // and a normal instruction step advance the counters through one identical
    // path. Returns true if an interrupt was taken, in which case the caller
    // skips StepOne for this step. The shell run loop MUST call this each step
    // or maskable IRQs are never serviced (the failure mode that hid until the
    // Mockingboard became the first device to assert one).
    bool                          TryStepInterrupt ();

protected:
    // Returns true if a pending NMI or unmasked IRQ was dispatched. On
    // dispatch, sets outCycles = 7 and accumulates m_totalCycles.
    bool                          TryDispatchInterrupt (uint32_t & outCycles);

    // Pushes PC + status (with B/U bits set per `fromBrk`), sets I=1, and
    // loads PC from the indicated vector. Used by IRQ/NMI dispatch.
    void                          DispatchVector (Word vector, bool fromBrk);

    // Issue #67: refresh m_busCycle to the cycle of the in-flight memory
    // access. Called from the MemoryBusCpu read/write override so the disk
    // catch-up sees sub-instruction rotational position.
    void                          UpdateBusCycle ();

protected:
    static constexpr Byte         kStatusBreakBit     = 0x10;
    static constexpr Byte         kStatusAlwaysOneBit = 0x20;

    bool                          m_irqLine    = false;
    bool                          m_nmiLine    = false;
    bool                          m_nmiPending = false;
    uint64_t                      m_totalCycles = 0;
    uint64_t                      m_busCycle    = 0;
};
