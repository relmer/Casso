#include "Pch.h"

#include "Debugger/RunStopHook.h"

#include "Debugger/DebugMemoryView.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::RunStopHook
//
////////////////////////////////////////////////////////////////////////////////

RunStopHook::RunStopHook (MachineHost & host, const DebugMemoryView & view) :
    m_host (host),
    m_view (view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::Begin
//
//  A step over a JSR runs until PC is at the instruction after it with the
//  stack pointer back where it was, so a recursive call is one step. A step
//  over anything else is a single step.
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::Begin (const RunRequest & request)
{
    static constexpr Word  kJsrLength = 3;
    Word                   pc         = m_host.GetCpu()->GetPC();



    m_request      = request;
    m_active       = true;
    m_stopped      = false;
    m_reason       = StopReason::Pause;
    m_instructions = 0;
    m_startSp      = m_host.GetCpu()->GetSP();
    m_lastOpcode   = 0;
    m_overCall     = request.kind == RunKind::StepOver && PeekOpcode (pc) == kJsr;
    m_returnPc     = (Word) (pc + kJsrLength);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::End
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::End()
{
    m_active  = false;
    m_stopped = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::ShouldStopBefore
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::ShouldStopBefore (Word pc)
{
    Byte  sp            = m_host.GetCpu()->GetSP();
    bool  isFirst       = m_active && m_instructions == 0;



    if (m_stopped)
    {
        return true;
    }

    if (m_active && !isFirst && IsRunComplete (pc, sp))
    {
        m_stopped = true;
        m_reason  = (m_request.kind == RunKind::RunTo || m_request.kind == RunKind::Go) ? StopReason::RunTo : StopReason::Step;
    }
    else if (m_conditions != nullptr && !isFirst && m_conditions->ShouldStopBefore (pc))
    {
        m_stopped = true;
        m_reason  = StopReason::Breakpoint;
    }

    if (!m_stopped)
    {
        m_lastOpcode = PeekOpcode (pc);
        ++m_instructions;

        if (m_conditions != nullptr)
        {
            m_conditions->OnInstruction (pc);
        }
    }

    return m_stopped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::HasPendingStop
//
//  A watchpoint hit raised by the conditions hook during the last instruction.
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::HasPendingStop() const
{
    return m_stopped || (m_conditions != nullptr && m_conditions->HasPendingStop());
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::HasStopped
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::HasStopped() const
{
    return HasPendingStop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::GetReason
//
////////////////////////////////////////////////////////////////////////////////

StopReason RunStopHook::GetReason() const
{
    if (m_stopped)
    {
        return m_reason;
    }

    return (m_conditions != nullptr && m_conditions->HasPendingStop()) ? StopReason::Watchpoint : StopReason::Pause;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::IsRunComplete
//
//  Called before every instruction after the first.
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::IsRunComplete (Word pc, Byte sp) const
{
    bool  isComplete  = false;
    bool  hasLeftSkip = m_request.hasSkip && (pc < m_request.skipFirst || pc > m_request.skipLast);



    switch (m_request.kind)
    {
    case RunKind::RunTo:
        isComplete = (m_request.hasUntilPc && pc == m_request.untilPc) || hasLeftSkip;
        break;

    case RunKind::StepInto:
    case RunKind::Trace:
        isComplete = m_instructions >= m_request.count;
        break;

    case RunKind::StepOver:
        isComplete = m_overCall ? (pc == m_returnPc && sp == m_startSp) : m_instructions >= 1;
        break;

    case RunKind::StepOut:
        isComplete = (m_lastOpcode == kRts || m_lastOpcode == kRti) && sp > m_startSp;
        break;

    case RunKind::Go:
        isComplete = hasLeftSkip;
        break;

    default:
        break;
    }

    return isComplete;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::PeekOpcode
//
////////////////////////////////////////////////////////////////////////////////

Byte RunStopHook::PeekOpcode (Word pc) const
{
    Byte  opcode = 0;



    m_view.TryPeek (pc, opcode);
    return opcode;
}
