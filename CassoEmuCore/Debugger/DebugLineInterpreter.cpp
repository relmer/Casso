#include "Pch.h"

#include "OpcodeTable.h"
#include "Disassembler.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/CommandSuggestion.h"
#include "Debugger/DebugLineInterpreter.h"
#include "Debugger/DebugSession.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/LineAssembler.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/WinDbgParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::DebugLineInterpreter
//
////////////////////////////////////////////////////////////////////////////////

DebugLineInterpreter::DebugLineInterpreter (DebugSession & session) :
    m_session (session)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::~DebugLineInterpreter
//
////////////////////////////////////////////////////////////////////////////////

DebugLineInterpreter::~DebugLineInterpreter() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecuteLine
//
//  The line goes to the parser for the mode. A line while the assembler is
//  active is source for it, whichever mode that is. Parse failures become
//  replies with the parser's message as the detail.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugLineInterpreter::ExecuteLine (const std::string & line, CommandMode mode)
{
    Reply                       reply;
    std::optional<CommandMode>  previous = m_lineMode;



    //  A handler that depends on the mode, such as HELP, reads the line's
    //  mode: a pipe request carries its own, whatever the session's is.
    if (m_assemblyAddress.has_value())
    {
        m_lineMode = mode;
        ExecuteAssemblyLine (Trim (line), reply);
        reply.command = line;
        m_lineMode    = previous;
        return reply;
    }

    return ExecutePaneLine (line, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecutePaneLine
//
//  One line as a command, whether or not an assembly is open.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugLineInterpreter::ExecutePaneLine (const std::string & line, CommandMode mode)
{
    Reply                       reply;
    std::string                 text          = Trim (line);
    std::optional<CommandMode>  outerLineMode = m_lineMode;



    //  A handler that answers in the line's dialect, as HELP does, reads it
    //  here; a script line run from inside this one sets its own.
    m_lineMode = mode;

    switch (mode)
    {
    case CommandMode::Monitor:    reply = ExecuteMonitorLine   (text, mode); break;
    case CommandMode::GSSquared:  reply = ExecuteGSSquaredLine (text, mode); break;
    case CommandMode::WinDbg:     reply = ExecuteWinDbgLine    (text); break;
    default:                      reply = ExecuteAppleWinLine  (text); break;
    }

    m_lineMode    = outerLineMode;
    reply.command = line;
    reply.mode    = mode;
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecuteAppleWinLine
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugLineInterpreter::ExecuteAppleWinLine (const std::string & text)
{
    AppleWinParseResult  parsed = AppleWinParser::Parse (text, m_session);
    Reply                reply;



    switch (parsed.status)
    {
    case ParseStatus::Ok:
        reply = m_session.Execute (parsed.command);
        break;

    case ParseStatus::Unknown:
        reply.SetError (CommandStatus::Unknown, "unknown command", parsed.error);
        CommandSuggestion::Annotate (reply, text, m_session.GetLineMode(), m_session);
        break;

    case ParseStatus::NotAvailable:
    case ParseStatus::WindowOnly:
        reply.SetError (CommandStatus::NotAvailable, "command not available", parsed.error);
        break;

    case ParseStatus::Invalid:
        reply.SetError (CommandStatus::Error, "invalid arguments", parsed.error);
        CommandSuggestion::Annotate (reply, text, m_session.GetLineMode(), m_session);
        break;

    default:
        break;
    }

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecuteWinDbgLine
//
//  A WinDbg command outside this machine's world is an error of its own:
//  it has no meaning here, which is not the same as being unavailable.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugLineInterpreter::ExecuteWinDbgLine (const std::string & text)
{
    WinDbgParseResult  parsed = WinDbgParser::Parse (text, m_session);
    Reply              reply;



    switch (parsed.status)
    {
    case ParseStatus::Ok:
        reply = m_session.Execute (parsed.command);
        break;

    case ParseStatus::Unknown:
        reply.SetError (CommandStatus::Unknown, "unknown command", parsed.error);
        CommandSuggestion::Annotate (reply, text, m_session.GetLineMode(), m_session);
        break;

    case ParseStatus::NotAvailable:
    case ParseStatus::WindowOnly:
        reply.SetError (CommandStatus::NotAvailable, parsed.label.empty() ? "command not available" : parsed.label, parsed.error);
        break;

    case ParseStatus::Invalid:
        reply.SetError (CommandStatus::Error, "invalid arguments", parsed.error);
        CommandSuggestion::Annotate (reply, text, m_session.GetLineMode(), m_session);
        break;

    default:
        break;
    }

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecuteMonitorLine
//
//  A Monitor line can hold several commands, so this is the one place a line
//  produces more than one reply and they have to become one.
//
//  ONE COMMAND KEEPS ITS REPLY WHOLE, data and all, because that is nearly
//  every line and a JSON reader should see the structure. Several commands
//  are run in order, each rendered in the line's output format as a
//  one-command line would be, and their text is concatenated, with the
//  first failure as the line's status; the merged reply carries no data of
//  its own, so formatting it again adds nothing. A JSON reader sees one record
//  with the whole line's text, which is the honest report of what a line
//  like `300.30F 400.40F` did.
//
//  A write turned away because the machine is running leaves the Monitor's
//  state as the line found it: the store address does not move and a
//  register edit stays armed, as though the line had not been typed.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugLineInterpreter::ExecuteMonitorLine (const std::string & text, CommandMode mode)
{
    MonitorState        before    = m_monitorState;
    MonitorParseResult  parsed    = MonitorParser::Parse (text, m_monitorState);
    Reply               merged;
    bool                rejected  = false;
    bool                isRunning = m_session.GetRunState() != RunState::Paused;



    //  A `/` line was never the Monitor's.
    if (!parsed.appleWinLine.empty())
    {
        return ExecuteAppleWinLine (parsed.appleWinLine);
    }

    if (parsed.status == ParseStatus::Invalid)
    {
        merged.SetError (CommandStatus::Error, "invalid arguments", parsed.error);
        return merged;
    }

    if (parsed.commands.size() == 1)
    {
        rejected = DebugSession::IsMachineWrite (parsed.commands.front().verb) && isRunning;
        merged   = m_session.Execute (parsed.commands.front());

        KeepRegisterEditArmed (parsed.commands.front(), merged);

        if (rejected)
        {
            m_monitorState = before;
        }

        return merged;
    }

    for (const DebugCommand & command : parsed.commands)
    {
        Reply  one;



        if (DebugSession::IsMachineWrite (command.verb) && m_session.GetRunState() != RunState::Paused)
        {
            rejected = true;
        }

        one = m_session.Execute (command);

        KeepRegisterEditArmed (command, one);
        MergeReply (merged, one, mode);
    }

    if (rejected)
    {
        m_monitorState = before;
    }

    return merged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::MergeReply
//
//  One of a line's several commands, rendered as a one-command line would
//  be and added to the line's reply; the first failure is the line's.
//
////////////////////////////////////////////////////////////////////////////////

void DebugLineInterpreter::MergeReply (Reply & merged, Reply & one, CommandMode mode) const
{
    m_session.FormatReply (one, mode);
    merged.text.insert (merged.text.end(), one.text.begin(), one.text.end());
    merged.files.insert (merged.files.end(), one.files.begin(), one.files.end());
    merged.isFormatted = true;

    if (merged.status == CommandStatus::Ok)
    {
        merged.status = one.status;
        merged.error  = one.error;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::KeepRegisterEditArmed
//
//  The parser spends the `^E` arming when it reads the colon, before the
//  session knows whether the edit can run. An edit that did not happen --
//  the machine was running -- leaves it armed, so the same colon after a
//  pause still sets the registers rather than storing into memory.
//
////////////////////////////////////////////////////////////////////////////////

void DebugLineInterpreter::KeepRegisterEditArmed (const DebugCommand & command, const Reply & reply)
{
    if (command.verb == DebugVerb::EditRegisters && reply.status != CommandStatus::Ok)
    {
        m_monitorState.registerEditPending = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecuteGSSquaredLine
//
//  A GSSquared line is one command, or one per address for `watch
//  first.last`; several are run in order and merged as a Monitor line's are.
//  A `nobp` number becomes an id or an address here, against the tables.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebugLineInterpreter::ExecuteGSSquaredLine (const std::string & text, CommandMode mode)
{
    GSSquaredParseResult  parsed = GSSquaredParser::Parse (text, m_session);
    Reply                 merged;



    switch (parsed.status)
    {
    case ParseStatus::Ok:
        break;

    case ParseStatus::Unknown:
        merged.SetError (CommandStatus::Unknown, "unknown command", parsed.error);
        CommandSuggestion::Annotate (merged, text, mode, m_session);
        return merged;

    case ParseStatus::NotAvailable:
    case ParseStatus::WindowOnly:
        merged.SetError (CommandStatus::NotAvailable, "command not available", parsed.error);
        return merged;

    case ParseStatus::Invalid:
        merged.SetError (CommandStatus::Error, "invalid arguments", parsed.error);
        CommandSuggestion::Annotate (merged, text, mode, m_session);
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
        return m_session.Execute (parsed.commands.front());
    }

    for (const DebugCommand & command : parsed.commands)
    {
        Reply  one = m_session.Execute (command);

        MergeReply (merged, one, mode);
    }

    return merged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::TryResolveIdOrAddress
//
//  GSSquared's `nobp N`: an id when an entry has that id, and otherwise the
//  address of an execution breakpoint, which is how GSSquared reads it. The
//  command becomes the BPC of the entry found.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugLineInterpreter::TryResolveIdOrAddress (DebugCommand & command, Reply & reply)
{
    BreakpointTable  & breakpoints = m_session.GetBreakpoints();
    WatchpointTable  & watchpoints = m_session.GetWatchpoints();
    bool               isId        = command.text.find_first_not_of ("0123456789") == std::string::npos;
    Breakpoint         breakpoint;
    Watchpoint         watchpoint;



    if (isId && (breakpoints.TryFind ((int) command.count, breakpoint) || watchpoints.TryFind ((int) command.count, watchpoint)))
    {
        return true;
    }

    for (const Breakpoint & entry : breakpoints.GetAll())
    {
        if (command.hasA1 && entry.kind == BreakpointKind::Address && entry.first == command.a1)
        {
            command.count = (uint32_t) entry.id;
            return true;
        }
    }

    reply.SetError (CommandStatus::Error, "unknown breakpoint",
                    std::format ("No breakpoint has the id or address {}.", command.text));
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::BeginAssembly
//
////////////////////////////////////////////////////////////////////////////////

void DebugLineInterpreter::BeginAssembly (Word address)
{
    m_assemblyAddress = address;
    m_assemblyOpcodes = std::make_unique<OpcodeTable> (m_session.GetTarget().GetInstructionSet());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::EndAssembly
//
////////////////////////////////////////////////////////////////////////////////

void DebugLineInterpreter::EndAssembly()
{
    m_assemblyAddress.reset();
    m_assemblyOpcodes.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter::ExecuteAssemblyLine
//
//  A blank line ends the mode. Anything else is one instruction, assembled at
//  the current address, or at the address before a colon, written, and shown
//  as the disassembler reads it back. A line starting with `$` is a Monitor
//  command. A line that does not assemble leaves the address and the mode as
//  they are.
//
////////////////////////////////////////////////////////////////////////////////

void DebugLineInterpreter::ExecuteAssemblyLine (const std::string & line, Reply & reply)
{
    IDebugTarget           & target    = m_session.GetTarget();
    LineAssembler            assembler (*m_assemblyOpcodes);
    Disassembler             disassembler (target.GetInstructionSet());
    DisassemblyData          data;
    DisassemblyLine          shown;
    std::vector<Byte>        bytes;
    std::vector<Byte>        originals;
    std::string              error;
    std::string              source    = line;
    LineAssemblyStatus       status    = LineAssemblyStatus::Ok;
    Word                     address   = *m_assemblyAddress;
    size_t                   prefixEnd = 0;
    HRESULT                  hr        = S_OK;



    if (line.empty())
    {
        EndAssembly();
        reply.data = MessageData { { "Assembly ended." } };
        return;
    }

    //  `$cmd` runs a Monitor command and leaves the assembler active.
    if (line[0] == '$')
    {
        reply = ExecuteMonitorLine (Trim (line.substr (1)), CommandMode::Monitor);
        return;
    }

    if (m_session.GetRunState() != RunState::Paused)
    {
        reply.SetError (CommandStatus::Error, "machine running", "Assembly changes memory. Pause the machine first.");
        return;
    }

    //  `addr:MNE operand` moves the assembly address first.
    while (prefixEnd < line.size() && prefixEnd < 4 && std::isxdigit ((unsigned char) line[prefixEnd]))
    {
        ++prefixEnd;
    }

    if (prefixEnd > 0 && prefixEnd < line.size() && line[prefixEnd] == ':')
    {
        address = (Word) std::stoul (line.substr (0, prefixEnd), nullptr, 16);
        source  = Trim (line.substr (prefixEnd + 1));
    }

    status = assembler.TryAssemble (address, source, bytes, error);

    if (status != LineAssemblyStatus::Ok)
    {
        reply.SetError (CommandStatus::Error, "assembly error", error);
        return;
    }

    //  All of the instruction is written or none of it: the old bytes are
    //  read first, and put back when a later byte cannot be written.
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        Byte  original = 0;

        target.TryPeek ((Word) (address + i), original);
        originals.push_back (original);

        if (!target.TryPoke ((Word) (address + i), bytes[i]))
        {
            for (size_t j = 0; j < i; ++j)
            {
                target.TryPoke ((Word) (address + j), originals[j]);
            }

            reply.SetError (CommandStatus::Error, "memory not writable",
                            std::format ("${:04X} cannot be written.", (Word) (address + i)));
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
//  DebugLineInterpreter::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugLineInterpreter::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r\n");
    size_t  last  = text.find_last_not_of  (" \t\r\n");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}
