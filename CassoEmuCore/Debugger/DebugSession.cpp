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
#include "Debugger/MonitorFormatter.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/RomSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::DebugSession
//
//  A batch session starts paused; an emulator session starts in whichever
//  state the machine is in when the debugger opens. The session installs
//  itself as the target's stop conditions and run observer, and the
//  watchpoint table as both the source of the target's watch mask and the
//  sink every access to a watched page is reported to.
//
////////////////////////////////////////////////////////////////////////////////

DebugSession::DebugSession (IDebugTarget & target, IDebugNotificationSink & sink, RunState initialState) :
    m_target (target),
    m_sink   (sink),
    m_state  (initialState)
{
    m_target.SetStopConditions (this);
    m_target.SetRunObserver    (this);
    m_target.SetWatchSink      (&m_watchpoints);
    m_watchpoints.SetTarget    (&m_target);
    LoadRomSymbols();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::LoadRomSymbols
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::LoadRomSymbols()
{
    std::string  error;
    size_t       loaded = 0;
    HRESULT      hr     = S_OK;



    hr = m_symbols.LoadFrom (SymbolTableId::Main, RomSymbols::GetMain (m_target.GetMachineInfo().name), 0, loaded, error);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = m_symbols.LoadFrom (SymbolTableId::Basic, RomSymbols::GetBasic(), 0, loaded, error);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = m_symbols.LoadFrom (SymbolTableId::Dos33, RomSymbols::GetDos33(), 0, loaded, error);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = m_symbols.LoadFrom (SymbolTableId::ProDos, RomSymbols::GetProDos(), 0, loaded, error);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::~DebugSession
//
////////////////////////////////////////////////////////////////////////////////

DebugSession::~DebugSession()
{
    m_watchpoints.SetTarget    (nullptr);
    m_target.SetWatchSink      (nullptr);
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
//  The line goes to the parser for the session's mode. A line while the
//  assembler is active is source for it, whichever mode that is. Parse
//  failures become replies with the parser's message as the detail.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteLine (const std::string & line)
{
    Reply        reply;
    std::string  text = Trim (line);



    if (m_assemblyAddress.has_value())
    {
        ExecuteAssemblyLine (text, reply);
        reply.command = line;
        return reply;
    }

    reply         = (m_mode == CommandMode::Monitor) ? ExecuteMonitorLine (text) : ExecuteAppleWinLine (text);
    reply.command = line;
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ExecuteAppleWinLine
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteAppleWinLine (const std::string & text)
{
    AppleWinParseResult  parsed = AppleWinParser::Parse (text, *this);
    Reply                reply;



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

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ExecuteMonitorLine
//
//  A Monitor line can hold several commands, so this is the one place a line
//  produces more than one reply and they have to become one.
//
//  ONE COMMAND KEEPS ITS REPLY WHOLE, data and all, because that is nearly
//  every line and a JSON reader should see the structure. Several commands
//  are run in order and their rendered text is concatenated, with the first
//  failure as the line's status; the merged reply carries no data of its
//  own, so formatting it again adds nothing. A JSON reader sees one record
//  with the whole line's text, which is the honest report of what a line
//  like `300.30F 400.40F` did.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteMonitorLine (const std::string & text)
{
    MonitorParseResult  parsed = MonitorParser::Parse (text, m_monitorState);
    Reply               merged;



    //  A `/` line was never the Monitor's.
    if (!parsed.appleWinLine.empty())
    {
        return ExecuteAppleWinLine (parsed.appleWinLine);
    }

    if (parsed.status == ParseStatus::Invalid)
    {
        SetError (merged, CommandStatus::Error, "invalid arguments", parsed.error);
        return merged;
    }

    if (parsed.commands.size() == 1)
    {
        return Execute (parsed.commands.front());
    }

    for (const DebugCommand & command : parsed.commands)
    {
        Reply  one = Execute (command);

        MonitorFormatter::Format (one);
        merged.text.insert (merged.text.end(), one.text.begin(), one.text.end());

        if (merged.status == CommandStatus::Ok)
        {
            merged.status = one.status;
            merged.error  = one.error;
        }
    }

    return merged;
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
    if (m_mode == CommandMode::Monitor)
    {
        MonitorFormatter::Format (reply);
        return;
    }

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
//  Every instruction about to execute. The address is what a watchpoint hit
//  during the instruction reports as its access PC: recorded here rather
//  than only in ShouldStopBefore, because the first instruction of a run is
//  never asked whether to stop, and a read it makes was reported from $0000.
//  Recording runs only during a run the debugger started.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnInstruction (Word pc)
{
    bool  isDebuggerRun = m_state == RunState::DebugRun || m_state == RunState::Stepping;



    m_watchpoints.SetAccessPc (pc);

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
    LoadRomSymbols();
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
//  DebugSession::OnUserPaused
//
//  The user paused the machine in Casso. A client is told the same way it is
//  told about any other stop, so it can read where the machine is without
//  knowing who stopped it.
//
//  A machine that was already paused announces nothing: the stop a client last
//  heard about is still the true one.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnUserPaused()
{
    StopEvent  stop;



    if (m_state == RunState::Paused)
    {
        return;
    }

    m_state = RunState::Paused;
    UpdateHookInstalled();

    stop.reason    = StopReason::Pause;
    stop.registers = m_target.GetRegisters();
    stop.pc        = stop.registers.pc;

    m_sink.OnStopped (stop);
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

    if (IsVideoBreakHit())
    {
        m_videoBreakHit = true;
        return true;
    }

    //  A Monitor `G` returning through its pushed address. It takes no id and
    //  is absent from BPL, because it is not the reader's breakpoint: it is
    //  how the Monitor gets control back from a program that ends in RTS.
    if (m_monitorReturn.has_value() && pc == *m_monitorReturn)
    {
        m_monitorReturn.reset();
        return true;
    }

    return TryMatchBeforeWatchpoint (pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetVideoBreak
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetVideoBreak (uint32_t first, uint32_t last)
{
    m_videoBreak = VideoBreak { first, last };
    UpdateHookInstalled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ClearVideoBreak
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ClearVideoBreak()
{
    m_videoBreak.reset();
    UpdateHookInstalled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::IsVideoBreakHit
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::IsVideoBreakHit() const
{
    uint32_t  scanline = 0;



    if (!m_videoBreak.has_value())
    {
        return false;
    }

    scanline = m_target.GetVideoPosition().scanline;
    return scanline >= m_videoBreak->first && scanline <= m_videoBreak->last;
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

    if (m_videoBreakHit)
    {
        m_videoBreak.reset();
        m_videoBreakHit = false;
    }

    m_watchpoints.ClearPending();
    m_lastBreakpointId.reset();
    m_beforeHit.reset();
    m_monitorReturn.reset();
    m_state = RunState::Paused;

    ClearTemporary (event);
    UpdateHookInstalled();

    if (m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnRunStopped (*this, event);
    }

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
//  @n is the nth search result, counted from 1; any other name is looked up
//  in the enabled symbol tables.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryResolveSymbol (const std::string & name, Word & address) const
{
    size_t         index = 0;
    SymbolTableId  table = SymbolTableId::Main;



    if (name.size() < 2 || name[0] != '@' || name.find_first_not_of ("0123456789", 1) != std::string::npos)
    {
        return m_symbols.TryResolve (name, address, table);
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
    RunRequest        request;
    Cpu6502Registers  registers = {};
    HRESULT           hr        = S_OK;
    bool              isStep    = false;



    if (m_state == RunState::DebugRun || m_state == RunState::Stepping)
    {
        SetError (reply, CommandStatus::Error, "already running",
                  "A run is in progress. PAUSE stops it.");
        return;
    }

    TryGetRunKind (command.verb, request.kind);

    // G with a stop address runs to it; addrG sets the program counter
    // first; a skip range ends the run when PC leaves it.
    if (request.kind == RunKind::Go && command.hasA1)
    {
        request.kind = RunKind::RunTo;
    }

    if (request.kind == RunKind::Go && command.hasA3 && !command.hasA2)
    {
        //  A Monitor `G` leaves the Monitor's own return address on the
        //  stack first, so a program ending in RTS comes back rather than
        //  running on into whatever follows it.
        if (command.mode == CommandMode::Monitor)
        {
            PushMonitorReturn();
        }

        registers    = m_target.GetRegisters();
        registers.pc = command.a3;
        m_target.SetRegisters (registers);
    }

    isStep = request.kind != RunKind::Go && request.kind != RunKind::RunTo;

    //  The Monitor's `300S` and `300T` step and trace FROM an address, where
    //  AppleWin's T and P take a count and never an address. Keying on the
    //  address being present is what lets one run path serve both.
    if (isStep && command.hasA1)
    {
        registers    = m_target.GetRegisters();
        registers.pc = command.a1;
        m_target.SetRegisters (registers);
    }

    request.fullSpeed  = command.verb == DebugVerb::GoFullSpeed;
    request.hasUntilPc = command.hasA1 && request.kind == RunKind::RunTo;
    request.untilPc    = command.a1;
    request.hasSkip    = command.hasA2 && command.hasA3 && !isStep;
    request.skipFirst  = command.a2;
    request.skipLast   = command.a3;
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
//  DebugSession::PushMonitorReturn
//
//  The address an RTS returns to, pushed as the 6502 pushes one: high byte
//  first, and one less than the address itself, because RTS adds one.
//
//  REGISTERS ARE NOT RELOADED FROM $45-$49 the way the ROM's G does. The CPU
//  is the single truth here, so a register set in AppleWin mode survives a
//  Monitor G rather than being overwritten by five bytes of zero page.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::PushMonitorReturn()
{
    static constexpr Word  kStackPage = 0x0100;
    Cpu6502Registers       registers  = m_target.GetRegisters();
    Word                   pushed     = (Word) (kMonitorReentry - 1);



    m_target.TryPoke ((Word) (kStackPage + registers.sp), (Byte) (pushed >> 8));
    registers.sp = (Byte) (registers.sp - 1);

    m_target.TryPoke ((Word) (kStackPage + registers.sp), (Byte) (pushed & 0xFF));
    registers.sp = (Byte) (registers.sp - 1);

    m_target.SetRegisters (registers);
    m_monitorReturn = kMonitorReentry;
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
    return m_breakpoints.HasEnabledStopCondition() || m_watchpoints.HasEnabled() || m_videoBreak.has_value();
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
