#include "Pch.h"

#include "Debugger/DebugSession.h"

#include "Disassembler.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DiskCallVector.h"
#include "Debugger/DiskValueSymbols.h"
#include "Debugger/GSSquaredFormatter.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/IInstructionObserver.h"
#include "Debugger/MonitorFormatter.h"
#include "Debugger/MonitorRun.h"
#include "Debugger/RomSymbols.h"
#include "Debugger/WinDbgFormatter.h"





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
    SetFilter (&m_hookFilter);

    m_target.SetStopConditions        (this);
    m_target.SetRunObserver           (this);
    m_target.SetWatchSink             (&m_watchpoints);
    m_watchpoints.SetContext          (this);
    m_watchpoints.SetValueBreakpoints (&m_breakpoints);
    m_watchpoints.SetTarget           (&m_target);
    m_diskMonitor.SetNotice           ([this] (const std::string & line) { m_sink.OnDiskNotice (line); });
    m_watchpoints.SetVectorWriteSink  ([this] { ResolveDiskCalls(); });
    RefreshHookFilter();
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
//  DebugSession::TryFindData
//
//  A data directive's block first, then the debug file's, which counts only
//  while the program in memory is the file's.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryFindData (Word address, DataBlockEntry & block, DataOrigin & origin) const
{
    origin = DataOrigin::None;

    if (m_dataBlocks.TryFindAt (address, block))
    {
        origin = DataOrigin::Marked;
    }
    else if (m_files.TryFindDebugFileData (address, block))
    {
        origin = DataOrigin::DebugFile;
    }

    return origin != DataOrigin::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::RemoveData
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::RemoveData (Word first, Word last)
{
    m_dataBlocks.Remove (first, last);
    m_files.RemoveDebugFileData (first, last);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetDebugFileCheck
//
//  The check runs at once while the machine is paused, and otherwise at the
//  next stop.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetDebugFileCheck (std::vector<ProgramCheck::Entry> entries)
{
    m_files.SetDebugFileCheck (std::move (entries));

    if (m_state == RunState::Paused)
    {
        m_files.CheckProgram();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetLabelCheck
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetLabelCheck (SymbolTableId table)
{
    m_files.SetLabelCheck (table);

    if (m_state == RunState::Paused)
    {
        m_files.CheckProgram();
    }
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
    m_target.SetOpcodeWatch    (nullptr, nullptr);
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
    Reply     reply;
    uint64_t  failuresBefore = m_hostFiles.GetHostAccessFailures();



    reply.command = command.sourceName;
    reply.verb    = command.verb;

    if (command.verb == DebugVerb::None)
    {
        SetError (reply, CommandStatus::Unknown, "unknown command",
                  std::format ("{} is not a command.", command.sourceName));
        return reply;
    }

    //  A step moves a stopped machine from where it stands, so a running one
    //  has nothing to step from. During a debugger run the run's own refusal
    //  applies, which says how to stop it.
    if (IsStepVerb (command.verb) && m_state == RunState::FreeRunning)
    {
        SetError (reply, CommandStatus::Error, "machine running",
                  std::format ("{} steps a paused machine. Pause the machine first.", command.sourceName));
        return reply;
    }

    if (TryExecuteEngineCommand (command, reply))
    {
        return reply;
    }

    //  Registers and memory are changed only with the machine stopped: a
    //  running program would overwrite the change, or be changed under the
    //  code using it. A step in the emulator is still running until it stops.
    //  A Monitor deposit's source is only a colon, which does not start a
    //  sentence, so the error says "This command" for it.
    if (IsMachineWrite (command.verb) && m_state != RunState::Paused)
    {
        SetError (reply, CommandStatus::Error, "machine running",
                  std::format ("{} changes registers or memory. Pause the machine first.",
                               command.sourceName == ":" ? std::string ("This command") : command.sourceName));
        return reply;
    }

    //  Behind live the change would discard recorded history: the host asks
    //  the user first, and runs the command again on a yes.
    if (IsMachineWrite (command.verb) && m_historyGuard && !m_historyGuard (command))
    {
        SetError (reply, CommandStatus::Error, "history kept",
                  "This change discards the history after this point. Answer the question in the Casso window to make the change.");
        return reply;
    }

    for (IDebugCommandHandler * handler : m_handlers)
    {
        if (handler->TryExecute (*this, command, reply))
        {
            m_hostFiles.ApplyHostAccessFailure (failuresBefore, reply);

            if (IsMachineWrite (command.verb))
            {
                ResolveDiskCalls();
                m_basicMonitor.TakeValues (*this);
            }

            return reply;
        }
    }

    SetError (reply, CommandStatus::NotAvailable, "command not available",
              std::format ("{} is not available yet.", command.sourceName));
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::IsMachineWrite
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::IsMachineWrite (DebugVerb verb)
{
    switch (verb)
    {
    case DebugVerb::SetProgramCounter:
    case DebugVerb::CallSubroutine:
    case DebugVerb::WriteNop:
    case DebugVerb::SetRegister:
    case DebugVerb::ClearFlag:
    case DebugVerb::SetFlag:
    case DebugVerb::PopStack:
    case DebugVerb::PopStackWord:
    case DebugVerb::PushStack:
    case DebugVerb::EnterBytes:
    case DebugVerb::EnterWords:
    case DebugVerb::PatchBytes:
    case DebugVerb::MoveMemory:
    case DebugVerb::FillMemory:
    case DebugVerb::LoadBinary:
    case DebugVerb::ReadIo:
    case DebugVerb::WriteIo:
    case DebugVerb::Deposit:
    case DebugVerb::EditRegisters:
    case DebugVerb::ReadFile:
    case DebugVerb::EnterAssembler:
    case DebugVerb::SetInverse:
    case DebugVerb::SetNormal:
    case DebugVerb::SetInputSlot:
    case DebugVerb::SetOutputSlot:
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::IsStepVerb
//
//  Run to cursor has no verb of its own: it is G with a stop address, a run,
//  and a run stays usable while the machine runs. The steps back are refused
//  by their own handler, which says why in history's terms.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::IsStepVerb (DebugVerb verb)
{
    switch (verb)
    {
    case DebugVerb::StepInto:
    case DebugVerb::StepOver:
    case DebugVerb::StepOut:
    case DebugVerb::RunFrame:
    case DebugVerb::BasicStep:
    case DebugVerb::BasicStepLine:
    case DebugVerb::BasicStepOut:
        return true;

    default:
        return false;
    }
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
//  DebugSession::GetPrompt
//
////////////////////////////////////////////////////////////////////////////////

const char * DebugSession::GetPrompt (CommandMode mode)
{
    return mode == CommandMode::Monitor ? "*"
         : mode == CommandMode::WinDbg  ? "0:000> "
         :                                ">";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::FormatReply
//
//  In the session's output format, which MODE sets and OUTPUT changes.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::FormatReply (Reply & reply) const
{
    RenderReply (reply, m_outputFormat);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::FormatReply
//
//  For a line run in a given mode. A line in the session's own mode renders
//  in the session's output format; a line a client ran in another mode, for
//  itself alone, renders in that mode's own format.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::FormatReply (Reply & reply, CommandMode mode) const
{
    RenderReply (reply, mode == m_mode ? m_outputFormat : CommandModeNames::GetOutputFormat (mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::RenderReply
//
//  Each format renders what it has a layout for and keeps the AppleWin text
//  for the rest.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::RenderReply (Reply & reply, OutputFormat format)
{
    if (reply.isFormatted)
    {
        return;
    }

    switch (format)
    {
    case OutputFormat::Monitor:    MonitorFormatter::Format   (reply); break;
    case OutputFormat::GSSquared:  GSSquaredFormatter::Format (reply); break;
    case OutputFormat::WinDbg:     WinDbgFormatter::Format    (reply); break;
    default:                       AppleWinFormatter::Format  (reply); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnStopConditionsChanged
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnStopConditionsChanged()
{
    m_watchpoints.RefreshWatchedPages();
    UpdateHookInstalled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ClearAllBreakpoints
//
//  With both tables empty no id is live, so numbering starts over, which
//  lets a saved breakpoint script address its entries by number. The video,
//  beam and read before written breaks go too.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ClearAllBreakpoints()
{
    m_breakpoints.ClearAll();
    m_watchpoints.ClearAll();
    m_nextId = 0;

    m_stops.ClearBreaks();
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
//  Recording runs only during a run the debugger started, and profiling in
//  any run.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnInstruction (Word pc)
{
    bool  isDebuggerRun = m_state == RunState::DebugRun || m_state == RunState::Stepping || IsProfiling();



    m_watchpoints.SetAccessPc (pc);

    if (m_watchpoints.HasEnabled())
    {
        m_stops.NoteCpuOwnReads (pc);
    }

    if (isDebuggerRun && m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnInstruction (*this, pc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnInterrupt
//
//  An interrupt dispatched in place of the instruction at pc. Its pushes are
//  reported from pc, as the instruction's own accesses would have been, but
//  it fetches none of the instruction's bytes, and the instruction itself
//  runs only after the return.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnInterrupt (Word pc)
{
    bool  isDebuggerRun = m_state == RunState::DebugRun || m_state == RunState::Stepping || IsProfiling();



    m_watchpoints.SetAccessPc (pc);

    if (m_watchpoints.HasEnabled())
    {
        m_watchpoints.SetCpuOwnReads (pc, 0, std::nullopt);
    }

    if (isDebuggerRun && m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnInterrupt (*this, pc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnMachineChanged
//
//  A different machine makes every address meaningless, so breakpoints,
//  watchpoints, the trace and the profile go and the call record starts
//  over. Watches and bookmarks are only labels and stay. With no breakpoint
//  id left live, numbering starts over as it does after BPC *. An A block
//  ends, since its opcodes were the old CPU's, and so does what is left of a
//  counted step. The new machine is paused or running as the caller says.
//
//  In a state loaded into the same kind of machine the addresses mean what
//  they did, so with isKeepingStops the breakpoints and watchpoints stay,
//  with their ids, and only what the old run built starts over.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnMachineChanged (const std::string & machineName, bool isPaused, bool isKeepingStops)
{
    if (!isKeepingStops)
    {
        m_breakpoints.ClearAll();
        m_watchpoints.ClearAll();
        m_nextId = 0;
    }

    m_stops.OnMachineChanged (isKeepingStops);
    m_interpreter.EndAssembly();
    m_countedSteps.Reset();
    m_state = isPaused ? RunState::Paused : RunState::FreeRunning;
    m_target.ClearTrace();
    m_target.ClearHeatMap();

    //  The new machine's bus has no watch sink.
    m_target.SetWatchSink (&m_watchpoints);

    m_callRecords.OnMachineChanged();

    if (m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnMachineChanged (*this);
    }

    UpdateHookInstalled();
    LoadRomSymbols();

    if (isKeepingStops)
    {
        OnStopConditionsChanged();
    }

    m_sink.OnMachineChanged (machineName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnReset
//
//  The call record starts over from the reset, and the stop at a Monitor
//  `G`'s return is disarmed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnReset (bool isPowerCycle)
{
    m_stops.OnReset();
    UpdateHookInstalled();
    m_callRecords.OnReset (isPowerCycle);
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
    m_callRecords.Settle();

    stop.reason    = StopReason::Pause;
    stop.registers = m_target.GetRegisters();
    stop.pc        = stop.registers.pc;

    m_files.TryGetSourceLine (stop.pc, stop.sourceFile, stop.sourceLine);
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
    if (m_state == RunState::FreeRunning)
    {
        return;
    }

    m_state = RunState::FreeRunning;
    UpdateHookInstalled();
    m_sink.OnResumed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetAttached
//
//  The bus reports to the watchpoints while the debugger is detached, so a
//  pending hit is dropped on either change.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetAttached (bool isAttached)
{
    m_isAttached = isAttached;
    m_watchpoints.ClearPending();
    UpdateHookInstalled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnFreeRunSlice
//
//  A slice of a free run has executed without stopping. What waits on the
//  guest, such as queued keys, is fed here, since no run reports its
//  instructions.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnFreeRunSlice()
{
    if (m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnFreeRunSlice (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnStopped
//
//  Every stop, whether it ends a debugger run or interrupts a free-running
//  machine, pauses the session and is announced, with the cause
//  DebugStopMonitor stored.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnStopped (const StopEvent & stop)
{
    StopEvent  event = stop;



    if (m_countedSteps.TryContinue (event))
    {
        return;
    }

    m_stops.ReportStop (event);
    m_files.CheckProgram();
    m_files.TryGetSourceLine (event.pc, event.sourceFile, event.sourceLine);

    m_lastStopReason     = event.reason;
    m_lastStopBreakpoint = event.breakpointId;
    m_callRecords.NoteStopPc (event.pc);

    m_stops.OnStopped();
    m_state = RunState::Paused;

    m_callRecords.OnStopped();
    m_stops.ClearTemporary (event);
    UpdateHookInstalled();

    if (m_instructionObserver != nullptr)
    {
        m_instructionObserver->OnRunStopped (*this, event);
    }

    m_sink.OnStopped (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::PauseForNextStep
//
//  ICountedStepOwner: between the steps of a count, the machine is paused,
//  so the next step's run starts as a run from a stop does.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::PauseForNextStep()
{
    m_state = RunState::Paused;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::StartStep
//
//  ICountedStepOwner: the count's next step, run as its command would be.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::StartStep (const DebugCommand & step)
{
    Reply  next;



    ExecuteRun (step, next);
    return next.status == CommandStatus::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::DeliverStop
//
//  ICountedStepOwner: the stop that ends a count whose next step didn't
//  start.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::DeliverStop (const StopEvent & stop)
{
    OnStopped (stop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ExecuteBasicStep
//
//  BASICSTEP, BASICLINE and BASICOUT run as G does, the step armed, and stop
//  where it ends unless something else stops the run first.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ExecuteBasicStep (const DebugCommand & command, Reply & reply)
{
    DebugCommand   run   = command;
    BasicStepKind  kind  = (command.verb == DebugVerb::BasicStepLine) ? BasicStepKind::Line
                         : (command.verb == DebugVerb::BasicStepOut)  ? BasicStepKind::Out
                         :                                              BasicStepKind::Statement;
    std::string    error;



    run.verb  = DebugVerb::Go;
    run.hasA1 = false;
    run.hasA2 = false;
    run.hasA3 = false;
    run.count = 0;

    //  During a run, G's error is returned, with how to stop the run.
    if (m_state != RunState::DebugRun && m_state != RunState::Stepping && !m_basicMonitor.TryBeginStep (kind, *this, error))
    {
        SetError (reply, CommandStatus::Error, "cannot step", error);
        return;
    }

    ExecuteRun (run, reply);

    if (reply.status != CommandStatus::Ok)
    {
        m_basicMonitor.EndStep();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ResolveDiskCalls
//
//  A machine with no Disk II controller resolves a driver breakpoint as
//  slot 6's.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ResolveDiskCalls()
{
    static constexpr int  kDefaultSlot = 6;
    DebugDiskState        state;
    std::vector<Word>     bytes;
    std::vector<int>      ids;
    int                   slot         = m_target.TryGetDiskState (state) ? state.slot : kDefaultSlot;



    for (const Breakpoint & entry : m_breakpoints.GetAll())
    {
        if (entry.IsDiskCall())
        {
            ids.push_back (entry.id);
            DiskCallVector::AddVectorBytes (*entry.disk, slot, bytes);
        }
    }

    for (int id : ids)
    {
        Breakpoint  entry;



        m_breakpoints.TryFind (id, entry);
        m_breakpoints.TrySetResolved (id, DiskCallVector::TryResolve (*entry.disk, slot, *this));
    }

    std::ranges::sort (bytes);
    bytes.erase (std::unique (bytes.begin(), bytes.end()), bytes.end());
    m_watchpoints.SetVectorBytes (bytes);
    RefreshHookFilter();
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
//  DebugSession::TryPeekIo
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryPeekIo (Word address, Byte & value) const
{
    return m_target.TryPeekIo (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryResolveSymbol
//
//  The disk values come first, ahead of any program symbol with the same
//  name, and are unknown on a machine with no Disk II. P.C, P.Z, P.I, P.D,
//  P.B, P.V and P.N are P's flags, 0 or 1. @n is the nth search result,
//  counted from 1; any other name is looked up in the enabled symbol
//  tables.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryResolveSymbol (const std::string & name, Word & address) const
{
    size_t         index = 0;
    SymbolTableId  table = SymbolTableId::Main;
    Byte           mask  = 0;
    Word           p     = 0;



    if (DiskValueSymbols::IsDiskValue (name))
    {
        return DiskValueSymbols::TryResolve (name, m_target, address);
    }

    if (TryGetFlagMask (name, mask))
    {
        if (!TryGetRegister ("P", p))
        {
            return false;
        }

        address = (p & mask) != 0 ? 1 : 0;
        return true;
    }

    if (name.size() < 2 || name[0] != '@' || name.find_first_not_of ("0123456789", 1) != std::string::npos)
    {
        return m_symbols.TryResolve (name, address, table);
    }

    auto [end, error] = std::from_chars (name.data() + 1, name.data() + name.size(), index);

    if (error != std::errc() || index == 0 || index > m_searchResults.size())
    {
        return false;
    }

    address = m_searchResults[index - 1];
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryGetFlagMask
//
//  P's bit for a flag's name, P.C through P.N, in any case.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryGetFlagMask (const std::string & name, Byte & mask)
{
    static constexpr std::pair<char, Byte>  kFlags[] =
    {
        { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'B', 0x10 }, { 'V', 0x40 }, { 'N', 0x80 },
    };



    if (name.size() != 3 || std::toupper ((unsigned char) name[0]) != 'P' || name[1] != '.')
    {
        return false;
    }

    for (const auto & [letter, bit] : kFlags)
    {
        if (std::toupper ((unsigned char) name[2]) == letter)
        {
            mask = bit;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::GetSymbolRevision
//
//  A disk value or a search result is worked out at each lookup; any other
//  name changes only with the symbol tables.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DebugSession::GetSymbolRevision (const std::string & name) const
{
    Byte  mask = 0;



    if (DiskValueSymbols::IsDiskValue (name) || name.starts_with ('@') || TryGetFlagMask (name, mask))
    {
        return 0;
    }

    return m_symbols.GetRevision();
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
        m_mode         = command.mode;
        m_outputFormat = CommandModeNames::GetOutputFormat (m_mode);
        reply.data     = ModeData { m_mode };
        m_sink.OnModeChanged (m_mode);
        return true;

    case DebugVerb::ShowMode:
        reply.data = ModeData { m_mode };
        return true;

    case DebugVerb::SetBudget:
        m_budget   = (command.count == 0) ? std::nullopt : std::optional<uint64_t> (command.count);
        reply.data = MessageData { { m_budget.has_value() ? std::format ("Budget set to {} cycles.", *m_budget) : std::string ("Budget removed.") } };
        return true;

    case DebugVerb::Pause:
        m_target.RequestPause();

        //  A run ends through its driver, which reports the stop. A machine
        //  running freely has no run to end: it is stopped where it is, and
        //  that is announced here, as a pause from the main window is. A
        //  pause that lands later in the frame is announced by the driver
        //  when it does.
        if (m_state == RunState::FreeRunning && !m_target.IsPausePending())
        {
            OnUserPaused();
        }

        return true;

    case DebugVerb::ShowSource:
    case DebugVerb::SetSourceStepping:
        m_files.ExecuteSource (command, m_target.GetRegisters().pc, reply);
        return true;

    case DebugVerb::ListStepFilter:
    case DebugVerb::AddStepFilter:
    case DebugVerb::RemoveStepFilter:
    case DebugVerb::ClearStepFilter:
        ExecuteStepFilter (command, reply);
        return true;

    case DebugVerb::BasicStep:
    case DebugVerb::BasicStepLine:
    case DebugVerb::BasicStepOut:
        ExecuteBasicStep (command, reply);
        return true;

    case DebugVerb::Project:
        if (!m_projectRequester)
        {
            SetError (reply, CommandStatus::NotAvailable, "no debug projects", "Only the emulator's debugger has debug projects.");
            return true;
        }

        m_projectRequester (command.text, reply);
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
//  DebugSession::ExecuteStepFilter
//
//  SKIP and its forms. Each replies with the list as it stands afterward.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ExecuteStepFilter (const DebugCommand & command, Reply & reply)
{
    std::optional<std::pair<Word, Word>>  range;



    switch (command.verb)
    {
    case DebugVerb::AddStepFilter:
        m_stepFilter.Add (command.text, command.a1, command.a2);
        break;

    case DebugVerb::RemoveStepFilter:
        if (command.hasA1)
        {
            range = std::pair<Word, Word> (command.a1, command.a2);
        }

        if (!m_stepFilter.TryRemove (command.text, range))
        {
            SetError (reply, CommandStatus::Error, "not in the step filter",
                      std::format ("{} is not in the step filter. Use SKIP to list the filter.", command.text));
            return;
        }

        break;

    case DebugVerb::ClearStepFilter:
        m_stepFilter.Clear();
        break;

    default:
        break;
    }

    reply.data = StepFilterData { m_stepFilter.GetEntries() };
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
    RunRequest          request;
    MonitorRun::Pushed  monitorReturn;
    HRESULT             hr     = S_OK;
    bool                isStep = false;



    if (!TryPrepareRun (command, request, isStep, reply))
    {
        return;
    }

    SetRunStart (command, request, isStep, monitorReturn);

    if (command.mode == CommandMode::Monitor && request.kind == RunKind::StepInto)
    {
        reply.data = MonitorRun::GetStepLine (m_target);
    }

    FillRunRequest (command, isStep, request, reply);

    m_stops.OnRunStarting();

    m_state = isStep ? RunState::Stepping : RunState::DebugRun;
    UpdateHookInstalled();

    hr = m_countedSteps.Begin (m_target, request);

    if (FAILED (hr))
    {
        MonitorRun::Undo (m_target, m_stops, monitorReturn);

        m_state = RunState::Paused;
        UpdateHookInstalled();
        SetError (reply, CommandStatus::Error, "run failed", "The machine could not start the run.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryPrepareRun
//
//  The run's kind from the command, and the errors that stop it before it
//  starts: a run while one is in progress, and a run from an address while
//  the machine runs, since setting the program counter, like any register
//  change, waits for the machine to stop. G with a stop address runs to it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryPrepareRun (
    const DebugCommand  & command,
    RunRequest          & request,
    bool                & outIsStep,
    Reply               & reply)
{
    if (m_state == RunState::DebugRun || m_state == RunState::Stepping)
    {
        //  PAUSE as the line's mode types it: /PAUSE in Monitor mode, !PAUSE in
        //  WinDbg mode.
        SetError (reply, CommandStatus::Error, "already running",
                  std::format ("A run is in progress. Use {} to stop it.", CommandModeHelp::GetTypedName (command.mode, "PAUSE")));
        return false;
    }

    TryGetRunKind (command.verb, request.kind);

    if (request.kind == RunKind::Go && command.hasA1)
    {
        request.kind = RunKind::RunTo;
    }

    outIsStep = request.kind != RunKind::Go && request.kind != RunKind::RunTo;

    if (m_state != RunState::Paused &&
        ((request.kind == RunKind::Go && command.hasA3 && !command.hasA2) || (outIsStep && command.hasA1)))
    {
        SetError (reply, CommandStatus::Error, "machine running",
                  std::format ("{} from an address sets the program counter. Pause the machine first.", command.sourceName));
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetRunStart
//
//  addrG sets the program counter first. A Monitor `G` leaves the Monitor's
//  return address on the stack before it, so a program ending in RTS comes
//  back rather than running on into whatever follows it; outReturn has what
//  the push replaced, for MonitorRun::Undo. The Monitor's `300S` and `300T`
//  step and trace FROM an address, where AppleWin's T and P take a count and
//  never an address. Keying on the address being present is what lets one
//  run path serve both.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetRunStart (
    const DebugCommand  & command,
    const RunRequest    & request,
    bool                  isStep,
    MonitorRun::Pushed  & outReturn)
{
    Cpu6502Registers  registers = {};



    if (request.kind == RunKind::Go && command.hasA3 && !command.hasA2)
    {
        if (command.mode == CommandMode::Monitor)
        {
            outReturn = MonitorRun::Push (m_target, m_stops);
            RefreshHookFilter();
        }

        registers    = m_target.GetRegisters();
        registers.pc = command.a3;
        m_target.SetRegisters (registers);
    }

    if (isStep && command.hasA1)
    {
        registers    = m_target.GetRegisters();
        registers.pc = command.a1;
        m_target.SetRegisters (registers);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::FillRunRequest
//
//  The run's stop address, skip range, count, budget, line table and step
//  filter. The Monitor's T traces until a stop or the budget: no count is no
//  end. A step out looks up its frame, and adds a notice when there is no
//  frame to step out to.
//  A count of steps CountedStepRunner runs one at a time goes to it.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::FillRunRequest (
    const DebugCommand  & command,
    bool                  isStep,
    RunRequest          & request,
    Reply               & reply)
{
    request.fullSpeed  = command.verb == DebugVerb::GoFullSpeed;
    request.hasUntilPc = command.hasA1 && request.kind == RunKind::RunTo;
    request.untilPc    = command.a1;
    request.hasSkip    = command.hasA2 && command.hasA3 && !isStep;
    request.skipFirst  = command.a2;
    request.skipLast   = command.a3;
    request.count      = (command.count != 0) ? command.count : (request.kind == RunKind::Trace) ? UINT32_MAX : 1;
    request.budget     = command.budget.has_value() ? command.budget : m_budget;
    request.lineTable  = isStep ? m_files.GetSteppingLineTable() : nullptr;
    request.stepFilter = (isStep && !m_stepFilter.IsEmpty()) ? &m_stepFilter : nullptr;

    m_runNotice.clear();

    if (request.kind == RunKind::StepOut)
    {
        m_callRecords.SetStepOutFrame (request, m_runNotice);

        if (!m_runNotice.empty())
        {
            reply.text.push_back (m_runNotice);
            reply.hasNotice = true;
        }
    }

    m_countedSteps.Arm (command, request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::GetCallStack
//
//  With a debug file loaded, the walk keeps only calls to a routine entry,
//  which is any address a label in the file has.
//
////////////////////////////////////////////////////////////////////////////////

CallStackData DebugSession::GetCallStack()
{
    std::set<Word>  entries;
    bool            hasFile = m_files.HasCurrentFile();



    if (hasFile)
    {
        m_files.GetRoutineEntries (entries);
    }

    return m_callRecords.GetCallStack (m_symbols, hasFile ? &entries : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TakePendingScript
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TakePendingScript (PendingScript & outScript)
{
    if (!m_pendingScript.has_value())
    {
        return false;
    }

    outScript = std::move (*m_pendingScript);
    m_pendingScript.reset();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TakeHitLogs
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebugSession::TakeHitLogs()
{
    std::vector<std::string>  logs    = m_breakpoints.TakeLogs();
    std::vector<std::string>  watches = m_watchpoints.TakeLogs();



    std::ranges::copy (watches, std::back_inserter (logs));

    for (const std::string & log : logs)
    {
        m_recentHitLogs.push_back (log);
    }

    while (m_recentHitLogs.size() > kRecentHitLogs)
    {
        m_recentHitLogs.pop_front();
    }

    return logs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::GetRecentHitLogs
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebugSession::GetRecentHitLogs (size_t count) const
{
    std::vector<std::string>  all (m_recentHitLogs.begin(), m_recentHitLogs.end());



    std::ranges::copy (m_breakpoints.GetLogs(), std::back_inserter (all));
    std::ranges::copy (m_watchpoints.GetLogs(), std::back_inserter (all));

    if (all.size() > count)
    {
        all.erase (all.begin(), all.end() - (ptrdiff_t) count);
    }

    return all;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TakeView
//
//  Copies only what is small or has changed: the symbols and the debug file
//  already in out are kept while their revisions match, so taking a view
//  copies them once per change rather than once per frame.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::TakeView (DebugSessionView & out)
{
    m_callRecords.Settle();

    out.breakpoints      = m_breakpoints.GetAll();
    out.watchpoints      = m_watchpoints.GetAll();
    out.watches          = m_watches;
    out.zeroPage         = m_zeroPage;
    out.bookmarks        = m_bookmarks;
    out.dataBlocks       = m_dataBlocks;
    out.mode             = m_mode;
    out.assemblyAddress  = m_interpreter.GetAssemblyAddress();
    out.cycleMarker      = m_cycleMarker;
    out.searchResults    = m_searchResults;
    out.searchSerial     = m_searchSerial;
    out.compareResults   = m_compareResults;
    out.compareSerial    = m_compareSerial;
    out.fileSystem       = m_hostFiles.GetFileSystem();
    out.currentDirectory = m_hostFiles.GetCurrentDirectory();
    out.nextId           = m_nextId;

    m_callRecords.TakeView (out);

    if (out.symbols == nullptr || out.symbols->GetRevision() != m_symbols.GetRevision())
    {
        out.symbols = std::make_shared<const SymbolTable> (m_symbols);
    }

    m_files.TakeView (out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::InstallView
//
//  For a session that only builds the panes: its tables become the copy's,
//  each entry keeping its id. The symbols and the debug file are copied in
//  only when their revisions differ from the ones this session already has.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::InstallView (const DebugSessionView & view)
{
    bool  isAdopted = false;



    m_breakpoints.ClearAll();

    for (const Breakpoint & entry : view.breakpoints)
    {
        isAdopted = m_breakpoints.TryAdopt (entry);
        IGNORE_RETURN_VALUE (isAdopted, false);
    }

    m_watchpoints.ClearAll();

    for (const Watchpoint & entry : view.watchpoints)
    {
        isAdopted = m_watchpoints.TryAdopt (entry);
        IGNORE_RETURN_VALUE (isAdopted, false);
    }

    m_watches          = view.watches;
    m_zeroPage         = view.zeroPage;
    m_bookmarks        = view.bookmarks;
    m_dataBlocks       = view.dataBlocks;
    m_mode             = view.mode;
    m_cycleMarker      = view.cycleMarker;
    m_searchResults    = view.searchResults;
    m_searchSerial     = view.searchSerial;
    m_compareResults   = view.compareResults;
    m_compareSerial    = view.compareSerial;
    m_nextId           = view.nextId;

    m_interpreter.SetAssemblyAddress  (view.assemblyAddress);
    m_hostFiles.SetFileSystem         (view.fileSystem);
    m_hostFiles.SetCurrentDirectory   (view.currentDirectory);
    m_callRecords.InstallView         (view);

    if (view.symbols != nullptr && view.symbols->GetRevision() != m_symbols.GetRevision())
    {
        m_symbols = *view.symbols;
    }

    m_files.InstallView (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::UpdateHookInstalled
//
//  The hook is installed while the debugger is attached and any enabled stop
//  condition exists, a run is active or profiling is on, and removed
//  otherwise, so a machine with no debugger interest pays only the null test.
//  The call record is fed without it: the CPU reports the instructions the
//  record uses (CallRecordKeeper::OnWatchedFetch).
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::UpdateHookInstalled()
{
    bool  shouldInstall = m_isAttached && (m_stops.HasStopConditions() || m_state == RunState::DebugRun || m_state == RunState::Stepping || IsProfiling());



    m_diskMonitor.Refresh();
    m_diskMonitor.SetPaused (m_state == RunState::Paused);
    ResolveDiskCalls();

    if (shouldInstall != m_hookInstalled)
    {
        m_hookInstalled = shouldInstall;
        m_target.SetHookInstalled (shouldInstall);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::RefreshHookFilter
//
//  Every instruction during a run the debugger started or while profiling
//  is on; otherwise the ones DebugStopMonitor::BuildHookFilter marks.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::RefreshHookFilter()
{
    m_stops.BuildHookFilter (m_hookFilter, m_state == RunState::DebugRun || m_state == RunState::Stepping || IsProfiling());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::IsProfiling
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::IsProfiling() const
{
    return m_instructionObserver != nullptr && m_instructionObserver->IsProfiling();
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
