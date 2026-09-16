#include "Pch.h"

#include "Debugger/DebugSession.h"

#include "OpcodeTable.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/Disassembler.h"
#include "Debugger/EffectiveAddress.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/IInstructionObserver.h"
#include "Debugger/LineAssembler.h"





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
//  DebugSession::ExecuteLine
//
//  In AppleWin mode the line is an AppleWin command. In Monitor mode only the
//  / prefix reaches one until the Monitor parser exists. A line while the
//  assembler is active is source for it. Parse failures become replies with
//  the parser's message as the detail.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteLine (const std::string & line)
{
    AppleWinParseResult  parsed;
    Reply                reply;
    std::string          text = Trim (line);



    if (m_assemblyAddress.has_value())
    {
        reply.command = line;
        ExecuteAssemblyLine (text, reply);
        return reply;
    }

    if (m_mode == CommandMode::Monitor && !text.starts_with ('/'))
    {
        reply.command = line;
        SetError (reply, CommandStatus::NotAvailable, "command not available", "Monitor mode is not available yet.");
        return reply;
    }

    if (text.starts_with ('/'))
    {
        text = Trim (text.substr (1));
    }

    parsed = AppleWinParser::Parse (text, *this);

    switch (parsed.status)
    {
    case ParseStatus::Ok:
        reply = Execute (parsed.command);
        break;

    case ParseStatus::Unknown:
        SetError (reply, CommandStatus::Unknown, "unknown command", parsed.error);
        break;

    case ParseStatus::NotAvailable:
    case ParseStatus::WindowOnly:
        SetError (reply, CommandStatus::NotAvailable, "command not available", parsed.error);
        break;

    case ParseStatus::Invalid:
        SetError (reply, CommandStatus::Error, "invalid arguments", parsed.error);
        break;

    default:
        break;
    }

    reply.command = line;
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::FormatReply
//
//  Monitor mode renders as AppleWin mode until its formatter exists.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::FormatReply (Reply & reply) const
{
    AppleWinFormatter::Format (reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ResolvePath
//
//  Quotes around the name are dropped. A rooted path is used as given; any
//  other is taken from the current directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebugSession::ResolvePath (const std::string & path) const
{
    std::string   bare     = Trim (path);
    std::wstring  wide;
    bool          isRooted = false;



    if (bare.size() >= 2 && (bare.front() == '"' || bare.front() == '\'') && bare.back() == bare.front())
    {
        bare = bare.substr (1, bare.size() - 2);
    }

    wide.assign (bare.begin(), bare.end());
    isRooted = wide.starts_with (L'\\') || wide.starts_with (L'/') || (wide.size() > 1 && wide[1] == L':');

    if (isRooted || m_currentDirectory.empty() || wide.empty())
    {
        return wide;
    }

    return m_currentDirectory + (m_currentDirectory.ends_with (L'\\') ? L"" : L"\\") + wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::BeginAssembly
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::BeginAssembly (Word address)
{
    m_assemblyAddress = address;
    m_assemblyOpcodes = std::make_unique<OpcodeTable> (m_target.GetInstructionSet());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ExecuteAssemblyLine
//
//  A blank line ends the mode. Anything else is one instruction, assembled at
//  the current address, written, and shown as the disassembler reads it back.
//  A line that does not assemble leaves the address and the mode as they are.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ExecuteAssemblyLine (const std::string & line, Reply & reply)
{
    LineAssembler            assembler (*m_assemblyOpcodes);
    Disassembler             disassembler (m_target.GetInstructionSet());
    DisassemblyData          data;
    DisassemblyLine          shown;
    std::vector<Byte>        bytes;
    std::string              error;
    LineAssemblyStatus       status  = LineAssemblyStatus::Ok;
    Word                     address = *m_assemblyAddress;
    HRESULT                  hr      = S_OK;



    if (line.empty())
    {
        m_assemblyAddress.reset();
        m_assemblyOpcodes.reset();
        reply.data = MessageData { { "Assembly ended." } };
        return;
    }

    status = assembler.TryAssemble (address, line, bytes, error);

    if (status != LineAssemblyStatus::Ok)
    {
        SetError (reply, CommandStatus::Error, "assembly error", error);
        return;
    }

    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (!m_target.TryPoke ((Word) (address + i), bytes[i]))
        {
            SetError (reply, CommandStatus::Error, "memory not writable", std::format ("${:04X} cannot be written.", address));
            return;
        }
    }

    hr = disassembler.DisassembleOne (address, bytes, shown.instruction);
    IGNORE_RETURN_VALUE (hr, S_OK);

    data.lines.push_back (shown);
    reply.data        = data;
    m_assemblyAddress = (Word) (address + bytes.size());
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
//  DebugSession::ClearAllBreakpoints
//
//  With both tables empty no id is live, so numbering starts over, which
//  lets a saved breakpoint script address its entries by number.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ClearAllBreakpoints()
{
    m_breakpoints.ClearAll();
    m_watchpoints.ClearAll();
    m_nextId = 0;
    UpdateHookInstalled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnInstruction
//
//  Recording runs only during a run the debugger started.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnInstruction (Word pc)
{
    bool  isDebuggerRun = m_state == RunState::DebugRun || m_state == RunState::Stepping;



    if (isDebuggerRun && m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnInstruction (*this, pc);
    }
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

    ClearTemporary (event);
    UpdateHookInstalled();
    m_sink.OnStopped (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ClearTemporary
//
//  A temporary breakpoint or watchpoint goes once it has caused a stop.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ClearTemporary (const StopEvent & stop)
{
    Breakpoint  breakpoint;
    Watchpoint  watchpoint;



    if (stop.breakpointId.has_value() && m_breakpoints.TryFind (*stop.breakpointId, breakpoint) && breakpoint.temporary)
    {
        m_breakpoints.TryClear (breakpoint.id);
    }

    if (stop.watch.has_value() && m_watchpoints.TryFind (stop.watch->id, watchpoint) && watchpoint.temporary)
    {
        m_watchpoints.TryClear (watchpoint.id);
    }
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
//  @n is the nth search result, counted from 1. No symbol tables are loaded
//  yet, so no other name resolves.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryResolveSymbol (const std::string & name, Word & address) const
{
    size_t  index = 0;



    if (name.size() < 2 || name[0] != '@' || name.find_first_not_of ("0123456789", 1) != std::string::npos)
    {
        return false;
    }

    index = (size_t) std::stoul (name.substr (1));

    if (index == 0 || index > m_searchResults.size())
    {
        return false;
    }

    address = m_searchResults[index - 1];
    return true;
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





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugSession::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r\n");
    size_t  last  = text.find_last_not_of  (" \t\r\n");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}
