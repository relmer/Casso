#include "Pch.h"

#include "Debugger/DebugSession.h"

#include "Debugger/EffectiveAddress.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IDebugNotificationSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::DebugSession
//
//  A batch session starts paused; an emulator session starts in whichever
//  state the machine is in when the debugger opens. The session installs
//  itself as the target's stop conditions and run observer, and the
//  watchpoint table as the source of the target's watch mask.
//
////////////////////////////////////////////////////////////////////////////////

DebugSession::DebugSession (IDebugTarget & target, IDebugNotificationSink & sink, RunState initialState) :
    m_target (target),
    m_sink   (sink),
    m_state  (initialState)
{
    m_target.SetStopConditions (this);
    m_target.SetRunObserver    (this);
    m_watchpoints.SetTarget    (&m_target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::~DebugSession
//
////////////////////////////////////////////////////////////////////////////////

DebugSession::~DebugSession()
{
    m_watchpoints.SetTarget    (nullptr);
    m_target.SetStopConditions (nullptr);
    m_target.SetRunObserver    (nullptr);
    m_target.SetHookInstalled  (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::Execute
//
//  The session's own commands first, then each registered family in turn.
//  A verb nobody handles leaves the session unchanged and says so.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::Execute (const DebugCommand & command)
{
    Reply  reply;



    reply.command = command.sourceName;

    if (command.verb == DebugVerb::None)
    {
        SetError (reply, CommandStatus::Unknown, "unknown command",
                  std::format ("{} is not a command.", command.sourceName));
        return reply;
    }

    if (TryExecuteEngineCommand (command, reply))
    {
        return reply;
    }

    for (IDebugCommandHandler * handler : m_handlers)
    {
        if (handler->TryExecute (*this, command, reply))
        {
            return reply;
        }
    }

    SetError (reply, CommandStatus::NotAvailable, "command not available",
              std::format ("{} is not available yet.", command.sourceName));
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::AddHandler
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::AddHandler (IDebugCommandHandler * handler)
{
    m_handlers.push_back (handler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnStopConditionsChanged
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnStopConditionsChanged()
{
    UpdateHookInstalled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnMachineChanged
//
//  A different machine makes every address meaningless, so breakpoints and
//  watchpoints go. Watches and bookmarks are only labels and stay.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnMachineChanged (const std::string & machineName)
{
    m_breakpoints.ClearAll();
    m_watchpoints.ClearAll();
    m_lastBreakpointId.reset();
    m_state = RunState::Paused;

    UpdateHookInstalled();
    m_sink.OnMachineChanged (machineName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnReset
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnReset (bool isPowerCycle)
{
    m_sink.OnReset (isPowerCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnUserResumed
//
//  The user resumed the machine in Casso: it runs freely, and the stop
//  conditions stay armed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnUserResumed()
{
    m_state = RunState::FreeRunning;
    UpdateHookInstalled();
    m_sink.OnResumed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ShouldStopBefore
//
//  Records the instruction address for watchpoint hits during the
//  instruction, then checks the breakpoints, then the before-mode
//  watchpoints against what the instruction would touch.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::ShouldStopBefore (Word pc)
{
    Byte  opcode = 0;
    int   hitId  = -1;
    bool  isHit  = false;



    m_watchpoints.SetAccessPc (pc);
    m_target.TryPeek (pc, opcode);

    isHit = m_breakpoints.TryMatchBeforeInstruction (pc, opcode, *this, hitId);

    if (isHit)
    {
        m_lastBreakpointId = hitId;
        return true;
    }

    return TryMatchBeforeWatchpoint (pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryMatchBeforeWatchpoint
//
//  Prediction runs only while a before-mode watchpoint is enabled. A hit
//  stops before the instruction and arms the one-stop rule, so the accesses
//  that instruction makes when the run resumes are not reported again.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryMatchBeforeWatchpoint (Word pc)
{
    AccessPrediction  prediction;
    WatchHit          hit;
    HRESULT           hr = S_OK;



    if (!m_watchpoints.HasEnabledBefore())
    {
        return false;
    }

    hr = EffectiveAddress::Predict (m_target.GetInstructionSet(), pc, m_target.GetRegisters(), *this, prediction);

    if (FAILED (hr) || !m_watchpoints.TryMatchBefore (pc, prediction, hit))
    {
        return false;
    }

    for (const Watchpoint & entry : m_watchpoints.GetAll())
    {
        if (entry.id == hit.id)
        {
            m_watchpoints.SuppressAfterStopFor (pc, entry.first, entry.last);
        }
    }

    m_beforeHit = hit;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::HasPendingStop
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::HasPendingStop() const
{
    return m_watchpoints.HasPendingStop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnStopped
//
//  Every stop, whether it ends a debugger run or interrupts a free-running
//  machine, leaves the session paused and is announced. The breakpoint or
//  watchpoint that caused it is attached here, where it was recorded.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnStopped (const StopEvent & stop)
{
    StopEvent  event = stop;



    if (event.reason == StopReason::Breakpoint && m_beforeHit.has_value())
    {
        event.reason = StopReason::Watchpoint;
        event.watch  = m_beforeHit;
    }
    else if (event.reason == StopReason::Breakpoint)
    {
        event.breakpointId = m_lastBreakpointId;
    }
    else if (event.reason == StopReason::Watchpoint)
    {
        event.watch = m_watchpoints.GetPendingHit();
    }

    m_watchpoints.ClearPending();
    m_lastBreakpointId.reset();
    m_beforeHit.reset();
    m_state = RunState::Paused;

    UpdateHookInstalled();
    m_sink.OnStopped (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryGetRegister
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryGetRegister (const std::string & name, Word & value) const
{
    Cpu6502Registers  registers = m_target.GetRegisters();
    bool              isKnown   = true;



    if      (name == "A")  { value = registers.a;  }
    else if (name == "X")  { value = registers.x;  }
    else if (name == "Y")  { value = registers.y;  }
    else if (name == "P")  { value = registers.p;  }
    else if (name == "S")  { value = registers.sp; }
    else if (name == "PC") { value = registers.pc; }
    else                   { isKnown = false;     }

    return isKnown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryPeek
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryPeek (Word address, Byte & value) const
{
    return m_target.TryPeek (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryResolveSymbol
//
//  No symbol tables are loaded yet, so no name resolves.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryResolveSymbol (const std::string &, Word &) const
{
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryExecuteEngineCommand
//
//  The commands that change the session itself: mode, budget, pause, and
//  starting a run.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryExecuteEngineCommand (const DebugCommand & command, Reply & reply)
{
    RunKind  kind = RunKind::Go;



    switch (command.verb)
    {
    case DebugVerb::SetMode:
        m_mode     = command.mode;
        reply.data = ModeData { m_mode };
        m_sink.OnModeChanged (m_mode);
        return true;

    case DebugVerb::ShowMode:
        reply.data = ModeData { m_mode };
        return true;

    case DebugVerb::SetBudget:
        m_budget = (command.count == 0) ? std::nullopt : std::optional<uint64_t> (command.count);
        return true;

    case DebugVerb::Pause:
        m_target.RequestPause();
        return true;

    default:
        break;
    }

    if (TryGetRunKind (command.verb, kind))
    {
        ExecuteRun (command, reply);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ExecuteRun
//
//  A run while one is in progress is an error. A run on a free-running
//  machine adopts it. The state is set before the run starts because a
//  synchronous target delivers the stop before StartRun returns.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ExecuteRun (const DebugCommand & command, Reply & reply)
{
    RunRequest  request;
    HRESULT     hr      = S_OK;
    bool        isStep  = false;



    if (m_state == RunState::DebugRun || m_state == RunState::Stepping)
    {
        SetError (reply, CommandStatus::Error, "already running",
                  "A run is in progress. PAUSE stops it.");
        return;
    }

    TryGetRunKind (command.verb, request.kind);

    // G with a stop address runs to it.
    if (request.kind == RunKind::Go && command.hasA1)
    {
        request.kind = RunKind::RunTo;
    }

    isStep             = request.kind != RunKind::Go && request.kind != RunKind::RunTo;
    request.fullSpeed  = command.verb == DebugVerb::GoFullSpeed;
    request.hasUntilPc = command.hasA1 && request.kind == RunKind::RunTo;
    request.untilPc    = command.a1;
    request.count      = (command.count == 0) ? 1 : command.count;
    request.budget     = command.budget.has_value() ? command.budget : m_budget;

    m_state = isStep ? RunState::Stepping : RunState::DebugRun;
    UpdateHookInstalled();

    hr = m_target.StartRun (request);

    if (FAILED (hr))
    {
        m_state = RunState::Paused;
        UpdateHookInstalled();
        SetError (reply, CommandStatus::Error, "run failed", "The machine could not start the run.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::UpdateHookInstalled
//
//  The hook is installed while any enabled stop condition exists or a run is
//  active, and removed otherwise, so a machine with no debugger interest pays
//  only the null test.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::UpdateHookInstalled()
{
    bool  shouldInstall = HasStopConditions() || m_state == RunState::DebugRun || m_state == RunState::Stepping;



    if (shouldInstall != m_hookInstalled)
    {
        m_hookInstalled = shouldInstall;
        m_target.SetHookInstalled (shouldInstall);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::HasStopConditions
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::HasStopConditions() const
{
    return m_breakpoints.HasEnabledStopCondition() || m_watchpoints.HasEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryGetRunKind
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryGetRunKind (DebugVerb verb, RunKind & kind)
{
    bool  isRun = true;



    switch (verb)
    {
    case DebugVerb::Go:
    case DebugVerb::GoFullSpeed: kind = RunKind::Go;       break;
    case DebugVerb::StepInto:    kind = RunKind::StepInto; break;
    case DebugVerb::StepOver:    kind = RunKind::StepOver; break;
    case DebugVerb::StepOut:     kind = RunKind::StepOut;  break;
    case DebugVerb::Trace:       kind = RunKind::Trace;    break;
    default:                     isRun = false;            break;
    }

    return isRun;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetError
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetError (
    Reply              & reply,
    CommandStatus        status,
    const std::string  & label,
    const std::string  & detail)
{
    reply.status       = status;
    reply.error.label  = label;
    reply.error.detail = detail;
}
