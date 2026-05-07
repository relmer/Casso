#include "Pch.h"

#include "Cpu6502.h"
#include "Ehm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu6502
//
////////////////////////////////////////////////////////////////////////////////

Cpu6502::Cpu6502 ()
    : Cpu ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  ICpu Reset entry point. Delegates to Cpu::Reset for the existing
//  byte-exact reset semantics, then clears interrupt-line state and the
//  total cycle counter.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Cpu6502::Reset ()
{
    HRESULT     hr = S_OK;



    Cpu::Reset ();

    m_irqLine     = false;
    m_nmiLine     = false;
    m_nmiPending  = false;
    m_totalCycles = 0;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Step
//
//  Executes exactly one instruction. If a pending NMI edge or an unmasked
//  IRQ is asserted, dispatches via the corresponding vector instead of
//  fetching the next opcode. Reports cycles consumed via outCycles.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Cpu6502::Step (uint32_t & outCycles)
{
    HRESULT     hr         = S_OK;
    bool        dispatched = false;



    outCycles = 0;

    dispatched = TryDispatchInterrupt (outCycles);

    if (!dispatched)
    {
        StepOne ();

        outCycles      = static_cast<uint32_t> (m_lastCycles);
        m_totalCycles += outCycles;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInterruptLine
//
//  IRQ (kMaskable) is level-sensitive — the line state simply tracks the
//  caller's last assertion. NMI (kNonMaskable) is edge-triggered — a
//  false->true transition latches m_nmiPending; the dispatch is consumed
//  by the next Step.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu6502::SetInterruptLine (CpuInterruptKind kind, bool asserted)
{
    if (kind == CpuInterruptKind::kMaskable)
    {
        m_irqLine = asserted;
        return;
    }

    if (asserted && !m_nmiLine)
    {
        m_nmiPending = true;
    }

    m_nmiLine = asserted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRegisters
//
////////////////////////////////////////////////////////////////////////////////

Cpu6502Registers Cpu6502::GetRegisters () const
{
    Cpu6502Registers    regs = {};



    regs.pc = PC;
    regs.a  = A;
    regs.x  = X;
    regs.y  = Y;
    regs.sp = SP;
    regs.p  = status.status;

    return regs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRegisters
//
////////////////////////////////////////////////////////////////////////////////

void Cpu6502::SetRegisters (const Cpu6502Registers & regs)
{
    PC            = regs.pc;
    A             = regs.a;
    X             = regs.x;
    Y             = regs.y;
    SP            = regs.sp;
    status.status = regs.p;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryDispatchInterrupt
//
//  Checked at the instruction-loop boundary in Step. NMI edge takes
//  priority and is dispatched regardless of the I flag; IRQ is dispatched
//  only when (P & I) == 0. On dispatch, both consume 7 cycles.
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu6502::TryDispatchInterrupt (uint32_t & outCycles)
{
    bool        dispatched = false;



    if (m_nmiPending)
    {
        m_nmiPending = false;

        DispatchVector (nmiVector, false);

        outCycles      = 7;
        m_totalCycles += outCycles;
        dispatched     = true;
    }
    else if (m_irqLine && status.flags.interruptDisable == 0)
    {
        DispatchVector (irqVector, false);

        outCycles      = 7;
        m_totalCycles += outCycles;
        dispatched     = true;
    }

    return dispatched;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchVector
//
//  Common 6502 interrupt prologue: push PCH, PCL, then status (with B
//  cleared for hardware IRQ/NMI, set for software BRK; U is always
//  set on the pushed copy). Sets I=1, then loads PC from the vector.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu6502::DispatchVector (Word vector, bool fromBrk)
{
    Byte    pStatus = 0;



    PushByte (static_cast<Byte> ((PC >> 8) & 0xFF));
    PushByte (static_cast<Byte> (PC & 0xFF));

    pStatus  = status.status;
    pStatus |= kStatusAlwaysOneBit;

    if (fromBrk)
    {
        pStatus |= kStatusBreakBit;
    }
    else
    {
        pStatus &= static_cast<Byte> (~kStatusBreakBit);
    }

    PushByte (pStatus);

    status.flags.interruptDisable = 1;

    PC = ReadWord (vector);
}
