#pragma once

#include "Debugger/BreakpointTable.h"
#include "Debugger/DebugHook.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/IRunObserver.h"
#include "Debugger/WatchTable.h"
#include "Debugger/WatchpointTable.h"

class IDebugCommandHandler;
class IDebugNotificationSink;





////////////////////////////////////////////////////////////////////////////////
//
//  RunState
//
////////////////////////////////////////////////////////////////////////////////

enum class RunState
{
    FreeRunning,
    Paused,
    DebugRun,
    Stepping,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession
//
//  The debugger's attachment to one machine. It executes commands against
//  the target, owns the breakpoint, watchpoint and watch tables, and tracks
//  whether the machine is running freely, paused, or in a run the debugger
//  started.
//
//  The session is also the target's stop-conditions hook, its run observer,
//  and the context expressions are evaluated in.
//
////////////////////////////////////////////////////////////////////////////////

class DebugSession : public DebugHook,
                     public IRunObserver,
                     public IDebugExpressionContext
{
public:
    DebugSession (IDebugTarget & target, IDebugNotificationSink & sink, RunState initialState);
    ~DebugSession() override;

    DebugSession             (const DebugSession &) = delete;
    DebugSession & operator= (const DebugSession &) = delete;

    Reply  Execute               (const DebugCommand & command);
    void   AddHandler            (IDebugCommandHandler * handler);

    // Called by command handlers after they change a table.
    void   OnStopConditionsChanged ();

    // Machine events the host reports.
    void   OnMachineChanged      (const std::string & machineName);
    void   OnReset               (bool isPowerCycle);
    void   OnUserResumed         ();

    RunState                GetRunState    () const { return m_state; }
    CommandMode             GetMode        () const { return m_mode; }
    std::optional<uint64_t> GetBudget      () const { return m_budget; }
    IDebugTarget          & GetTarget      ()       { return m_target; }
    BreakpointTable       & GetBreakpoints ()       { return m_breakpoints; }
    WatchpointTable       & GetWatchpoints ()       { return m_watchpoints; }
    WatchTable            & GetWatches     ()       { return m_watches; }
    WatchTable            & GetZeroPage    ()       { return m_zeroPage; }
    WatchTable            & GetBookmarks   ()       { return m_bookmarks; }

    // DebugHook: the stop conditions consulted before each instruction.
    bool   ShouldStopBefore      (Word pc) override;
    bool   HasPendingStop        () const override;

    // IRunObserver
    void   OnStopped             (const StopEvent & stop) override;

    // IDebugExpressionContext
    bool   TryGetRegister        (const std::string & name, Word & value) const override;
    bool   TryPeek               (Word address, Byte & value) const override;
    bool   TryResolveSymbol      (const std::string & name, Word & address) const override;

private:
    bool   TryExecuteEngineCommand (const DebugCommand & command, Reply & reply);
    void   ExecuteRun            (const DebugCommand & command, Reply & reply);
    void   UpdateHookInstalled   ();
    bool   HasStopConditions     () const;

    static bool   TryGetRunKind  (DebugVerb verb, RunKind & kind);
    static void   SetError       (Reply & reply, CommandStatus status, const std::string & label, const std::string & detail);

    IDebugTarget                        & m_target;
    IDebugNotificationSink              & m_sink;
    std::vector<IDebugCommandHandler *>   m_handlers;

    int                                   m_nextId        = 0;
    BreakpointTable                       m_breakpoints   { m_nextId };
    WatchpointTable                       m_watchpoints   { m_nextId };
    WatchTable                            m_watches;
    WatchTable                            m_zeroPage;
    WatchTable                            m_bookmarks;

    RunState                              m_state         = RunState::Paused;
    CommandMode                           m_mode          = CommandMode::AppleWin;
    std::optional<uint64_t>               m_budget;
    bool                                  m_hookInstalled = false;
    std::optional<int>                    m_lastBreakpointId;
    std::optional<WatchHit>               m_beforeHit;

    bool   TryMatchBeforeWatchpoint (Word pc);
};
