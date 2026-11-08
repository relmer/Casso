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
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::Begin (const RunRequest & request)
{
    Word  pc = m_host.GetCpu()->GetPC();



    m_request      = request;
    m_active       = true;
    m_stopped      = false;
    m_reason       = StopReason::Pause;
    m_instructions = 0;
    m_steps        = 0;
    m_startSp      = m_host.GetCpu()->GetSP();
    m_lastOpcode   = 0;
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
    if (!CheckStopBefore (pc))
    {
        RecordInstruction (pc);
    }

    return m_stopped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::CheckStopBefore
//
//  Whether the machine stops before the instruction at pc, decided as
//  ShouldStopBefore decides it, without taking the instruction as run. A run
//  driver asks this when a run's budget is spent, so a run that has just
//  reached its end, or a stop condition, is reported rather than the budget.
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::CheckStopBefore (Word pc)
{
    Byte  sp      = m_host.GetCpu()->GetSP();
    bool  isFirst = m_active && m_instructions == 0;



    if (m_stopped)
    {
        return true;
    }

    if (m_active && !isFirst)
    {
        TrackCall     (pc, sp);
        CountProgress (pc, sp);
    }

    if (m_active && !isFirst && IsRunComplete (pc))
    {
        m_stopped = true;
        m_reason  = (m_request.kind == RunKind::RunTo || m_request.kind == RunKind::Go) ? StopReason::RunTo : StopReason::Step;
    }
    else if (m_conditions != nullptr && !isFirst && m_conditions->ShouldStopBefore (pc))
    {
        m_stopped = true;
        m_reason  = StopReason::Breakpoint;
    }

    return m_stopped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook::RecordInstruction
//
//  The instruction at pc is about to run.
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::RecordInstruction (Word pc)
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
//  Called before every instruction after the first. A step is complete when
//  its count of steps is done; a trace runs until a stop condition or the
//  budget ends it.
//
////////////////////////////////////////////////////////////////////////////////

bool RunStopHook::IsRunComplete (Word pc) const
{
    bool  isComplete  = false;
    bool  hasLeftSkip = m_request.hasSkip && (pc < m_request.skipFirst || pc > m_request.skipLast);



    switch (m_request.kind)
    {
    case RunKind::RunTo:
        isComplete = (m_request.hasUntilPc && pc == m_request.untilPc) || hasLeftSkip;
        break;

    case RunKind::StepInto:
    case RunKind::StepOver:
    case RunKind::StepOut:
        isComplete = m_steps >= m_request.count;
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
//  RunStopHook::CountProgress
//
//  The steps finished by the time the instruction at pc is reached.
//
//  By instruction, into or over: each instruction that ran outside a call the
//  step follows is a step, so a stepped-over or filtered call is one.
//
//  By source line: into, the first instruction of a line other than the one
//  the last step ended on; over, the same, outside any call the step made,
//  or any instruction once the routine the step began in has returned.
//
//  Out: the recorded frame the request chose has ended, which covers every
//  level asked for; with no frame, the stack pointer has risen above its
//  level at the last step and a return or a jump has just run.
//
////////////////////////////////////////////////////////////////////////////////

void RunStopHook::CountProgress (Word pc, Byte sp)
{
    std::optional<std::pair<int, int>>  line;
    bool                                isOutside = !m_callSp.has_value();
    bool                                isSource  = m_request.lineTable != nullptr;
    bool                                isDone    = false;



    switch (m_request.kind)
    {
    case RunKind::StepInto:
    case RunKind::StepOver:
        line   = GetStepLine (pc);
        isDone = !isSource ? isOutside
               : isOutside && ((line.has_value() && line != m_startLine) || (m_request.kind == RunKind::StepOver && sp > m_startSp));
        break;

    case RunKind::StepOut:
        if (m_request.hasLeftFrame && m_request.hasLeftFrame (pc, sp))
        {
            m_steps = m_request.count;
            return;
        }

        isDone = !m_request.hasLeftFrame && sp > m_startSp && IsTransfer (m_lastOpcode);
        break;

    default:
        break;
    }

    if (!isDone)
    {
        return;
    }

    ++m_steps;
    m_startLine = line;
    m_startSp   = std::max (m_startSp, sp);
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
