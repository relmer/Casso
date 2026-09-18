#pragma once

#include "Debugger/DebugHook.h"
#include "Debugger/LineTable.h"
#include "Debugger/Reply.h"

class DebugMemoryView;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook
//
//  The hook MachineHost consults while the debugger has a run in progress or
//  stop conditions set. It ends a run where the RunRequest says -- the run-to
//  address, the step count, the return from a stepped-over call, the return
//  that steps out -- and asks the session's conditions hook about
//  breakpoints and watchpoints.
//
//  CALLS ARE FOLLOWED BY THE STACK POINTER, NOT THE RETURN ADDRESS (R-033). A
//  call is over once the stack pointer is back at its level before the JSR
//  and a return or a jump has just executed, so a routine that returns past
//  inline parameters, a recursive routine and one that discards its return
//  address and jumps away all end the step. Stepping out ends the same way,
//  once the stack pointer is above its level when the step began.
//
//  A request with a line table steps by source line: into stops at the first
//  instruction of another line, the innermost one when macros nest; over
//  runs whole calls and stops when the outermost line changes; out is the
//  instruction step out. An instruction with no line never ends a source
//  step, so code without source runs through.
//
//  The first instruction of a run always executes, so resuming from a
//  breakpoint does not stop on the same breakpoint again.
//
////////////////////////////////////////////////////////////////////////////////

class RunStopHook : public DebugHook
{
public:
    RunStopHook (MachineHost & host, const DebugMemoryView & view);

    void        SetConditions    (DebugHook * conditions) { m_conditions = conditions; }
    void        Begin            (const RunRequest & request);
    void        End              ();
    bool        IsActive         () const { return m_active; }
    bool        HasStopped       () const;
    StopReason  GetReason        () const;

    bool        ShouldStopBefore (Word pc) override;
    bool        HasPendingStop   () const override;

private:
    static constexpr Byte  kJsr = 0x20;

    bool        IsRunComplete    (Word pc, Byte sp) const;
    bool        IsSourceStepComplete (Word pc, Byte sp) const;
    void        TrackCall        (Byte sp);
    static bool IsTransfer       (Byte opcode);
    Byte        PeekOpcode       (Word pc) const;

    //  The line at an address that a source step compares: the innermost for
    //  a step into, the outermost otherwise. Absent where no line produced
    //  the address.
    std::optional<std::pair<int, int>>  GetStepLine (Word pc) const;

    MachineHost            & m_host;
    const DebugMemoryView  & m_view;
    DebugHook              * m_conditions   = nullptr;

    RunRequest               m_request;
    bool                     m_active       = false;
    bool                     m_stopped      = false;
    StopReason               m_reason       = StopReason::Pause;
    uint64_t                 m_instructions = 0;
    Byte                     m_startSp      = 0;
    Byte                     m_lastOpcode   = 0;
    bool                     m_overCall     = false;

    //  The stack pointer before the outermost call the step is inside, while
    //  it is inside one, and the line the step began on.
    std::optional<Byte>                  m_callSp;
    std::optional<std::pair<int, int>>   m_startLine;
};
