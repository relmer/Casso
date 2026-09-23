#include "Pch.h"

#include "Debugger/DebugSession.h"

#include "OpcodeTable.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/AppleWinParser.h"
#include "Disassembler.h"
#include "Debugger/EffectiveAddress.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/IInstructionObserver.h"
#include "Debugger/LineAssembler.h"
#include "Debugger/MonitorFormatter.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/GSSquaredFormatter.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/WinDbgFormatter.h"
#include "Debugger/WinDbgParser.h"
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
    SetFilter (&m_hookFilter);

    m_target.SetStopConditions        (this);
    m_target.SetRunObserver           (this);
    m_target.SetWatchSink             (&m_watchpoints);
    m_watchpoints.SetContext          (this);
    m_watchpoints.SetValueBreakpoints (&m_breakpoints);
    m_watchpoints.SetTarget           (&m_target);
    m_callRecorder.SetPeek            ([this] (Word address) { return PeekByte (address); });
    m_callRecorder.SetWriterLocator   ([this] { return FindStoreInProgress(); });
    CallStackRecorder::MarkOpcodes    (m_callOpcodes.data());
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
//  DebugSession::SetDebugFile
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetDebugFile (DebugFile file, const std::wstring & path, const std::string & key)
{
    m_debugFile     = std::move (file);
    m_debugFilePath = path;
    m_debugFileKey  = key;
    m_lineTable.Build (m_debugFile);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::TryGetSourceLine
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryGetSourceLine (Word address, std::string & file, int & line) const
{
    const std::vector<SourcePosition>  & positions = m_lineTable.GetPositionsAt (address);
    int                                  fileId    = 0;



    if (positions.empty())
    {
        return false;
    }

    fileId = positions.back().file;

    for (const DebugSourceFile & each : m_debugFile.files)
    {
        if (each.id == fileId)
        {
            file = each.name;
            line = positions.back().line;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ClearDebugFile
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ClearDebugFile()
{
    m_debugFile = DebugFile();
    m_debugFilePath.clear();
    m_debugFileKey.clear();
    m_lineTable.Clear();
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
    Reply  reply;



    reply.command = command.sourceName;
    reply.verb    = command.verb;

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

    //  Registers and memory are changed only with the machine stopped
    //  (FR-103): a running program would overwrite the change, or be changed
    //  under the code using it.
    if (IsMachineWrite (command.verb) && (m_state == RunState::FreeRunning || m_state == RunState::DebugRun))
    {
        SetError (reply, CommandStatus::Error, "machine running",
                  std::format ("{} changes registers or memory; pause the machine first.", command.sourceName));
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
//  DebugSession::IsMachineWrite
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::IsMachineWrite (DebugVerb verb)
{
    switch (verb)
    {
    case DebugVerb::SetProgramCounter:
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
    case DebugVerb::WriteIo:
    case DebugVerb::Deposit:
    case DebugVerb::EditRegisters:
    case DebugVerb::ReadFile:
    case DebugVerb::EnterAssembler:
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
//  DebugSession::ExecuteLine
//
//  The line goes to the parser for the session's mode. A line while the
//  assembler is active is source for it, whichever mode that is. Parse
//  failures become replies with the parser's message as the detail.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteLine (const std::string & line)
{
    return ExecuteLine (line, m_mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::ExecuteLine
//
//  One line in a given mode, leaving the session's own mode alone.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteLine (const std::string & line, CommandMode mode)
{
    Reply        reply;
    std::string  text = Trim (line);



    if (m_assemblyAddress.has_value())
    {
        ExecuteAssemblyLine (text, reply);
        reply.command = line;
        return reply;
    }


    switch (mode)
    {
    case CommandMode::Monitor:    reply = ExecuteMonitorLine   (text); break;
    case CommandMode::GSSquared:  reply = ExecuteGSSquaredLine (text); break;
    case CommandMode::WinDbg:     reply = ExecuteWinDbgLine    (text); break;
    default:                      reply = ExecuteAppleWinLine  (text); break;
    }

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
//  DebugSession::ExecuteWinDbgLine
//
//  A WinDbg command outside this machine's world is an error of its own:
//  it has no meaning here, which is not the same as being unavailable.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteWinDbgLine (const std::string & text)
{
    WinDbgParseResult  parsed = WinDbgParser::Parse (text, *this);
    Reply              reply;



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
        SetError (reply, CommandStatus::NotAvailable, parsed.label.empty() ? "command not available" : parsed.label, parsed.error);
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
//  DebugSession::ExecuteGSSquaredLine
//
//  A GSSquared line is one command, or one per address for `watch
//  first.last`; several are run in order and merged as a Monitor line's are.
//  A `nobp` number becomes an id or an address here, against the tables.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugSession::ExecuteGSSquaredLine (const std::string & text)
{
    GSSquaredParseResult  parsed = GSSquaredParser::Parse (text, *this);
    Reply                 merged;



    switch (parsed.status)
    {
    case ParseStatus::Ok:
        break;

    case ParseStatus::Unknown:
        SetError (merged, CommandStatus::Unknown, "unknown command", parsed.error);
        return merged;

    case ParseStatus::NotAvailable:
    case ParseStatus::WindowOnly:
        SetError (merged, CommandStatus::NotAvailable, "command not available", parsed.error);
        return merged;

    case ParseStatus::Invalid:
        SetError (merged, CommandStatus::Error, "invalid arguments", parsed.error);
        return merged;

    default:
        return merged;
    }

    if (parsed.isIdOrAddress && !TryResolveIdOrAddress (parsed.commands.front(), merged))
    {
        return merged;
    }

    if (parsed.commands.size() == 1)
    {
        return Execute (parsed.commands.front());
    }

    for (const DebugCommand & command : parsed.commands)
    {
        Reply  one = Execute (command);

        FormatReply (one);
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
//  DebugSession::TryResolveIdOrAddress
//
//  GSSquared's `nobp N`: an id when an entry has that id, and otherwise the
//  address of an execution breakpoint, which is how GSSquared reads it. The
//  command becomes the BPC of the entry found.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugSession::TryResolveIdOrAddress (DebugCommand & command, Reply & reply)
{
    bool         isId     = command.text.find_first_not_of ("0123456789") == std::string::npos;
    Breakpoint   breakpoint;
    Watchpoint   watchpoint;



    if (isId && (m_breakpoints.TryFind ((int) command.count, breakpoint) || m_watchpoints.TryFind ((int) command.count, watchpoint)))
    {
        return true;
    }

    for (const Breakpoint & entry : m_breakpoints.GetAll())
    {
        if (command.hasA1 && entry.kind == BreakpointKind::Address && entry.first == command.a1)
        {
            command.count = (uint32_t) entry.id;
            return true;
        }
    }

    SetError (reply, CommandStatus::Error, "unknown breakpoint",
              std::format ("No breakpoint has the id or address {}.", command.text));
    return false;
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
    m_watchpoints.RefreshWatchedPages();
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
//  DebugSession::OnWatchedFetch
//
//  The CPU reports only the opcodes that can change the call record, the
//  instruction after each and every interrupt; any other instruction leaves
//  the record as it was, so the record is what it would be shown them all.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnWatchedFetch (Word pc, Byte sp, Byte opcode)
{
    m_callRecorder.OnInstruction (pc, sp, opcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::OnMachineChanged
//
//  A different machine makes every address meaningless, so breakpoints,
//  watchpoints and the trace go and the call record starts over. Watches and
//  bookmarks are only labels and stay.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::OnMachineChanged (const std::string & machineName)
{
    m_breakpoints.ClearAll();
    m_watchpoints.ClearAll();
    m_lastBreakpointId.reset();
    m_state = RunState::Paused;
    m_target.ClearTrace();

    if (m_callRecorder.IsActive())
    {
        m_callRecorder.End();
        SetCallRecording (true);
    }

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
    Word  pc = m_target.GetRegisters().pc;



    m_callRecorder.OnReset (pc, PeekByte (pc), isPowerCycle);
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
    SettleCallRecord();

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

    return m_watchpoints.HasEnabledBefore() && TryMatchBeforeWatchpoint (pc);
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

    AttachCondition (event);
    TryGetSourceLine (event.pc, event.sourceFile, event.sourceLine);

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

    SettleCallRecord();
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

    //  A value breakpoint stops as a watchpoint does, under its own id.
    if (stop.watch.has_value() && m_breakpoints.TryFind (stop.watch->id, breakpoint) && breakpoint.temporary)
    {
        m_breakpoints.TryClear (breakpoint.id);
        m_watchpoints.RefreshWatchedPages();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::AttachCondition
//
//  The IF expression of the entry that stopped the machine, and the value it
//  had at the hit, which the table recorded then. A register breakpoint's
//  condition is the breakpoint itself, not an IF, and is not repeated.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::AttachCondition (StopEvent & event) const
{
    Breakpoint  breakpoint;
    Watchpoint  watchpoint;



    if (event.breakpointId.has_value() && m_breakpoints.TryFind (*event.breakpointId, breakpoint) && breakpoint.kind != BreakpointKind::Register)
    {
        event.condition      = breakpoint.condition.text;
        event.conditionValue = m_breakpoints.GetLastConditionValue();
    }
    else if (event.watch.has_value() && m_watchpoints.TryFind (event.watch->id, watchpoint))
    {
        event.condition      = watchpoint.condition.text;
        event.conditionValue = event.watch->conditionValue;
    }
    else if (event.watch.has_value() && m_breakpoints.TryFind (event.watch->id, breakpoint))
    {
        event.condition      = breakpoint.condition.text;
        event.conditionValue = event.watch->conditionValue;
    }

    if (event.condition.empty())
    {
        event.conditionValue.reset();
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
        m_mode         = command.mode;
        m_outputFormat = CommandModeNames::GetOutputFormat (m_mode);
        reply.data     = ModeData { m_mode };
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

    case DebugVerb::ShowSource:
    case DebugVerb::SetSourceStepping:
        ExecuteSource (command, reply);
        return true;

    case DebugVerb::ListStepFilter:
    case DebugVerb::AddStepFilter:
    case DebugVerb::RemoveStepFilter:
    case DebugVerb::ClearStepFilter:
        ExecuteStepFilter (command, reply);
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
//  DebugSession::ExecuteSource
//
//  SRC gives the source line at PC and how steps go; SRC ON and SRC OFF
//  choose between source lines and instructions.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::ExecuteSource (const DebugCommand & command, Reply & reply)
{
    MessageData  message;
    std::string  file;
    int          line      = 0;
    Word         pc        = m_target.GetRegisters().pc;
    bool         hasSource = !m_lineTable.IsEmpty();



    if (command.verb == DebugVerb::SetSourceStepping)
    {
        m_stepBySource = command.count != 0;
    }

    if (!hasSource)
    {
        message.lines.push_back ("No debug file is loaded; SYM LOAD reads one.");
    }
    else if (TryGetSourceLine (pc, file, line))
    {
        message.lines.push_back (std::format ("${:04X} is {} line {}.", pc, file, line));
    }
    else
    {
        message.lines.push_back (std::format ("No source line produced ${:04X}.", pc));
    }

    message.lines.push_back ((m_stepBySource && hasSource) ? "Steps go by source line."
                             : m_stepBySource              ? "Steps go by instruction until a debug file is loaded."
                             :                               "Steps go by instruction.");
    reply.data = message;
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
                      std::format ("{} is not in the step filter. SKIP lists it.", command.text));
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
    request.lineTable  = (isStep && m_stepBySource && !m_lineTable.IsEmpty()) ? &m_lineTable : nullptr;
    request.stepFilter = (isStep && !m_stepFilter.IsEmpty()) ? &m_stepFilter : nullptr;

    if (request.kind == RunKind::StepOut)
    {
        SetStepOutFrame (request);
    }

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
//  DebugSession::SetStepOutFrame
//
//  A step out leaves the innermost recorded frame: a call by its return, an
//  interrupt by the RTI that goes back to the instruction it stopped. The
//  stack pointer alone cannot tell that when code between the handler and
//  the interrupted instruction keeps a return frame of its own, as the
//  enhanced //e's ROM does around an interrupt, or pulls bytes and jumps.
//
//  The record takes in each instruction at the next one's fetch, which is
//  after the stop hook asks about it, so it is settled first, from the
//  registers the next instruction will run with.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetStepOutFrame (RunRequest & request)
{
    CallStackFrame  frame;



    SettleCallRecord();

    if (!m_callRecorder.IsActive() || m_callRecorder.GetFrames().empty())
    {
        return;
    }

    frame = m_callRecorder.GetFrames().back();

    request.hasLeftFrame = [this, frame] (Word pc, Byte sp)
    {
        const std::vector<CallStackFrame> & frames = m_callRecorder.GetFrames();



        m_callRecorder.Settle (pc, sp);

        return std::none_of (frames.begin(), frames.end(), [&frame] (const CallStackFrame & each)
        {
            return each.callSite == frame.callSite && each.target == frame.target &&
                   each.kind == frame.kind && each.stackLevel == frame.stackLevel;
        });
    };
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
    RefreshHookFilter();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::FindStoreInProgress
//
//  Called from inside a bus write. The CPU has fetched the whole instruction
//  by the time it stores, so PC is already past it; a store that can reach
//  the stack page is three bytes (absolute, absolute indexed) or two
//  (indirect), and the one whose length ends at PC is the one running.
//
////////////////////////////////////////////////////////////////////////////////

Word DebugSession::FindStoreInProgress() const
{
    const Microcode  * set = m_target.GetInstructionSet();
    Word               pc  = m_target.GetRegisters().pc;
    Disassembler       disassembler (set);



    if (set == nullptr)
    {
        return pc;
    }

    for (Word length : { (Word) 3, (Word) 2 })
    {
        if (disassembler.GetLength (PeekByte ((Word) (pc - length))) == length)
        {
            return (Word) (pc - length);
        }
    }

    return pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SetCallRecording
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SetCallRecording (bool isOn)
{
    Word  pc = m_target.GetRegisters().pc;



    if (isOn == m_callRecorder.IsActive())
    {
        return;
    }

    if (isOn)
    {
        m_callRecorder.Begin (pc, PeekByte (pc), m_target.GetCycleCount() == 0);
        m_target.SetOpcodeWatch (m_callOpcodes.data(), this);
        m_watchpoints.SetStackWriteSink ([this] (Word address, Byte value, std::optional<Byte> previous)
        {
            m_callRecorder.OnStackWrite (address, value, previous);
        });
    }
    else
    {
        m_target.SetOpcodeWatch (nullptr, nullptr);
        m_watchpoints.SetStackWriteSink (nullptr);
        m_callRecorder.End();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::GetCallStack
//
//  With a debug file loaded, the walk keeps only calls to a routine entry,
//  which is any address a label in the file holds.
//
////////////////////////////////////////////////////////////////////////////////

CallStackData DebugSession::GetCallStack()
{
    std::set<Word>     entries;
    CallStackData      data;
    SymbolTableId      table = SymbolTableId::User;
    Cpu6502Registers   registers;



    SettleCallRecord();

    if (HasDebugFile())
    {
        for (const DebugSymbol & symbol : m_debugFile.symbols)
        {
            if (symbol.type == "lab")
            {
                entries.insert ((Word) symbol.value);
            }
        }
    }

    registers = m_target.GetRegisters();
    data      = CallStack::Build (m_callMechanism, m_callRecorder, registers.sp,
                                  [this] (Word address) { return PeekByte (address); },
                                  HasDebugFile() ? &entries : nullptr);

    for (CallStackRow & row : data.rows)
    {
        if (row.frame.has_value())
        {
            m_symbols.TryFindName (row.frame->target, row.frame->symbol, table);
        }
    }

    if (data.lastReturn.has_value())
    {
        m_symbols.TryFindName (data.lastReturn->target, data.lastReturn->symbol, table);
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::SettleCallRecord
//
//  The record takes in the last instruction executed, from the registers
//  after it; called only between instructions.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::SettleCallRecord()
{
    Cpu6502Registers  registers;



    if (!m_callRecorder.IsActive())
    {
        return;
    }

    registers = m_target.GetRegisters();
    m_callRecorder.Settle (registers.pc, registers.sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::PeekByte
//
//  A byte as the CPU sees it, or zero where the debugger cannot read one
//  without disturbing the machine.
//
////////////////////////////////////////////////////////////////////////////////

Byte DebugSession::PeekByte (Word address) const
{
    Byte  value = 0;



    m_target.TryPeek (address, value);
    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession::UpdateHookInstalled
//
//  The hook is installed while any enabled stop condition exists or a run is
//  active, and removed otherwise, so a machine with no debugger interest pays
//  only the null test. The call record does not need it: the CPU reports the
//  instructions it needs (OnWatchedFetch).
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::UpdateHookInstalled()
{
    bool  shouldInstall = HasStopConditions() || m_state == RunState::DebugRun || m_state == RunState::Stepping;



    RefreshHookFilter();

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
//  The instructions the session needs to see. Every one during a run the
//  debugger started, and while a watchpoint, a value breakpoint, a video
//  break or a breakpoint on anything but an address or an opcode is armed:
//  those are tested before or during any instruction, and a watchpoint hit
//  reports the address of the instruction that made it. Otherwise only the
//  pages holding an address breakpoint or the Monitor's return, and the
//  opcodes a breakpoint names.
//
////////////////////////////////////////////////////////////////////////////////

void DebugSession::RefreshHookFilter()
{
    bool  isEvery = m_state == RunState::DebugRun || m_state == RunState::Stepping || m_watchpoints.HasEnabled() || m_videoBreak.has_value();



    m_hookFilter       = DebugHookFilter();
    m_hookFilter.pages = {};

    for (const Breakpoint & entry : m_breakpoints.GetAll())
    {
        if (!entry.enabled)
        {
            continue;
        }

        switch (entry.kind)
        {
        case BreakpointKind::Address:
            for (int page = entry.first >> 8; page <= (entry.last >> 8); ++page)
            {
                m_hookFilter.pages[page] = true;
            }

            break;

        case BreakpointKind::Opcode:
        case BreakpointKind::Brk:
            m_hookFilter.opcodes[entry.opcode] = true;
            m_hookFilter.opcodesStop           = true;
            break;

        default:
            isEvery = true;
            break;
        }
    }

    if (m_monitorReturn.has_value())
    {
        m_hookFilter.pages[*m_monitorReturn >> 8] = true;
    }

    m_hookFilter.everyInstruction = isEvery;

    if (isEvery || m_hookFilter.opcodesStop)
    {
        m_hookFilter.pages = DebugHookFilter::s_kAllMarked;
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
