#include "Pch.h"

#include "Debugger/RunStopHook.h"

#include "Debugger/DebugMemoryView.h"
#include "Debugger/StepFilter.h"
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
    UseIdleFilter();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::SetConditions
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::SetConditions (DebugHook * conditions)
{
    m_conditions = conditions;

    if (!m_active)
    {
        UseIdleFilter();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::UseIdleFilter
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::UseIdleFilter()
{
    SetFilter ((m_conditions != nullptr) ? &m_conditions->GetFilter() : &s_kNoInstruction);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::Begin
//
//  A step over a JSR is over when the stack pointer is back at its level
//  before the call; a step over anything else is a single step.
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::Begin (const RunRequest & request)
{
    Word  pc = m_host.GetCpu()->GetPC();



    m_request      = request;
    m_active       = true;
    m_stopped      = false;
    m_reason       = StopReason::Pause;
    m_instructions = 0;
    m_startSp      = m_host.GetCpu()->GetSP();
    m_lastOpcode   = 0;
    m_overCall     = request.kind == RunKind::StepOver && PeekOpcode (pc) == kJsr;
    m_callSp.reset();
    m_startLine    = GetStepLine (pc);

    SetFilter (&s_kEveryInstruction);
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

    UseIdleFilter();
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

    if (m_active && !isFirst)
    {
        TrackCall (pc, sp);
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
        if (m_active)
        {
            m_lastOpcode = PeekOpcode (pc);
            ++m_instructions;
        }

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
    bool  isSource    = m_request.lineTable != nullptr && m_request.kind != RunKind::StepOut;



    if (isSource)
    {
        return IsSourceStepComplete (pc, sp);
    }

    switch (m_request.kind)
    {
    case RunKind::RunTo:
        isComplete = (m_request.hasUntilPc && pc == m_request.untilPc) || hasLeftSkip;
        break;

    case RunKind::StepInto:
    case RunKind::Trace:
        isComplete = m_instructions >= m_request.count && !m_callSp.has_value();
        break;

    case RunKind::StepOver:
        isComplete = !m_overCall || !m_callSp.has_value();
        break;

    case RunKind::StepOut:
        isComplete = m_request.hasLeftFrame ? m_request.hasLeftFrame (pc, sp)
                                            : sp > m_startSp && IsTransfer (m_lastOpcode);
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
//  RunStopHook::IsSourceStepComplete
//
//  Into: the first instruction of a line other than the one the step began
//  on. Over: the same, outside any call the step made, or any instruction
//  once the routine the step began in has returned.
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::IsSourceStepComplete (Word pc, Byte sp) const
{
    std::optional<std::pair<int, int>>  line       = GetStepLine (pc);
    bool                                isNewLine  = line.has_value() && line != m_startLine;
    bool                                isComplete = false;



    switch (m_request.kind)
    {
    case RunKind::StepInto:
    case RunKind::Trace:
        isComplete = isNewLine && !m_callSp.has_value();
        break;

    case RunKind::StepOver:
        isComplete = !m_callSp.has_value() && (isNewLine || sp > m_startSp);
        break;

    default:
        break;
    }

    return isComplete;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::TrackCall
//
//  After a JSR the stack pointer is two below its level before it. The call
//  is over once it is back there and control has left by a return or a jump.
//  The level alone is not enough: a routine that reads inline parameters or
//  discards its return address pulls that address itself, which brings the
//  stack back to the caller's level while it is still running.
//
//  A step over follows every call. A step into follows a call only when the
//  routine it reached, at pc, is in the step filter (FR-070): the step then
//  ends as a step over would, and a filtered routine that never returns
//  leaves the run going.
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::TrackCall (Word pc, Byte sp)
{
    static constexpr Byte  kReturnAddressBytes = 2;
    bool                   isInto              = m_request.kind == RunKind::StepInto || m_request.kind == RunKind::Trace;
    bool                   isFiltered          = isInto && m_request.stepFilter != nullptr && m_request.stepFilter->Contains (pc);



    if (m_callSp.has_value() && sp >= *m_callSp && IsTransfer (m_lastOpcode))
    {
        m_callSp.reset();
    }

    if (!m_callSp.has_value() && m_lastOpcode == kJsr && (m_request.kind == RunKind::StepOver || isFiltered))
    {
        m_callSp = (Byte) (sp + kReturnAddressBytes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::IsTransfer
//
//  A return, or a jump in any of its forms: the instructions that can take
//  control back out of a routine.
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::IsTransfer (Byte opcode)
{
    static constexpr Byte  kRts         = 0x60;
    static constexpr Byte  kRti         = 0x40;
    static constexpr Byte  kJmp         = 0x4C;
    static constexpr Byte  kJmpIndirect = 0x6C;
    static constexpr Byte  kJmpIndexed  = 0x7C;



    return opcode == kRts || opcode == kRti || opcode == kJmp || opcode == kJmpIndirect || opcode == kJmpIndexed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::GetStepLine
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<int, int>> RunStopHook::GetStepLine (Word pc) const
{
    const std::vector<SourcePosition>  * positions = nullptr;
    const SourcePosition               * chosen    = nullptr;



    if (m_request.lineTable == nullptr)
    {
        return std::nullopt;
    }

    positions = &m_request.lineTable->GetPositionsAt (pc);

    if (positions->empty())
    {
        return std::nullopt;
    }

    chosen = (m_request.kind == RunKind::StepOver) ? &positions->front() : &positions->back();

    return std::pair<int, int> (chosen->file, chosen->line);
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
