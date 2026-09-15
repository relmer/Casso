#pragma once

#include "Debugger/DebugHook.h"
#include "Debugger/Reply.h"

class DebugMemoryView;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  RunStopHook
//
//  The hook MachineHost consults while the debugger has a run in progress or
//  stop conditions set. It ends a run where the RunRequest says -- the run-to
//  address, the step count, the return from a stepped-over call, the RTS or
//  RTI that steps out -- and asks the session's conditions hook about
//  breakpoints and watchpoints.
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
    static constexpr Byte  kRts = 0x60;
    static constexpr Byte  kRti = 0x40;

    bool        IsRunComplete    (Word pc, Byte sp) const;
    Byte        PeekOpcode       (Word pc) const;

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
    Word                     m_returnPc     = 0;
};
