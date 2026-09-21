#include "Pch.h"

#include "Ui/Debugger/DebuggerViewState.h"

#include "Debugger/DebugSession.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Debugger/Source/SourcePathList.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/MonitorParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::IsBuildDue
//
//  A change between running and paused is due at once: a breakpoint that just
//  stopped the machine has to show where it stopped. A stopped machine changes
//  only when someone acts, which is what marks the view dirty, so time alone
//  never rebuilds it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::IsBuildDue (bool isDirty, bool isPaused, bool wasPaused, uint64_t nowMs, uint64_t builtAtMs)
{
    return isDirty                 ||
           isPaused != wasPaused   ||
           (!isPaused && nowMs - builtAtMs >= kBuildIntervalMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::Build
//
//  Each pane's command runs in AppleWin mode whatever mode the user is in, so
//  the panes read the same data in both; only the command box follows the
//  chosen mode.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot DebuggerViewState::Build (DebugSession & session) const
{
    DebuggerViewSnapshot  snapshot;
    Reply                 registers   = session.ExecuteLine ("R",     CommandMode::AppleWin);
    Reply                 breakpoints = session.ExecuteLine ("BPL",   CommandMode::AppleWin);
    Reply                 stack       = session.ExecuteLine ("STACK", CommandMode::AppleWin);
    Reply                 watches     = session.ExecuteLine ("WL",    CommandMode::AppleWin);
    Reply                 calls       = session.ExecuteLine ("CALLS", CommandMode::AppleWin);
    Reply                 code;
    Reply                 memory;
    Word                  codeStart   = 0;



    snapshot.mode    = session.GetMode();
    snapshot.machine = session.GetTarget().GetMachineInfo().name;

    if (const RegistersData * data = std::get_if<RegistersData> (&registers.data))
    {
        const Cpu6502Registers & r = data->registers;
        static const char        kFlagNames[] = "NV-BDIZC";

        snapshot.pc        = r.pc;
        snapshot.registers = { { "A",  std::format ("{:02X}", r.a) },
                               { "X",  std::format ("{:02X}", r.x) },
                               { "Y",  std::format ("{:02X}", r.y) },
                               { "P",  std::format ("{:02X}", r.p) },
                               { "S",  std::format ("{:02X}", r.sp) },
                               { "PC", std::format ("{:04X}", r.pc) } };

        for (int bit = 7; bit >= 0; bit--)
        {
            snapshot.flags += ((r.p >> bit) & 1) ? kFlagNames[7 - bit] : '.';
        }
    }

    if (const BreakpointListData * data = std::get_if<BreakpointListData> (&breakpoints.data))
    {
        for (const BreakpointInfo & info : data->breakpoints)
        {
            snapshot.breakpoints.push_back ({ info.id, info.address,
                                              std::format ("#{} ${:04X}{}", info.id, info.address, info.enabled ? "" : " (off)"),
                                              info.enabled });
        }
    }

    codeStart = ChooseCodeStart (session, snapshot.pc);
    //  A range, so the count is ours rather than the command's default: the
    //  pane holds as many lines as it has room for, and three bytes an
    //  instruction covers the longest the 6502 has.
    code = session.ExecuteLine (std::format ("U {:04X}:{:04X}", codeStart,
                                             (Word) (codeStart + (Word) (m_codeLines * 3))),
                                CommandMode::AppleWin);

    m_shownCode.clear();

    if (const DisassemblyData * data = std::get_if<DisassemblyData> (&code.data))
    {
        for (const DisassemblyLine & line : data->lines)
        {
            DebuggerViewSnapshot::CodeLine  row;
            std::string                     bytes;



            for (Byte b : line.instruction.bytes)
            {
                bytes += std::format ("{:02X} ", b);
            }

            row.address       = line.instruction.address;
            row.bytes         = bytes.empty() ? bytes : bytes.substr (0, bytes.size() - 1);
            row.instruction   = line.instruction.operand.empty() ? line.instruction.mnemonic
                                                                 : line.instruction.mnemonic + " " + line.GetShownOperand();
            row.label         = line.label;
            row.isCurrent     = row.address == snapshot.pc;
            row.hasBreakpoint = std::any_of (snapshot.breakpoints.begin(), snapshot.breakpoints.end(),
                                             [&] (const DebuggerViewSnapshot::BreakpointLine & bp) { return bp.address == row.address; });

            snapshot.code.push_back (row);

            m_shownCode.push_back (row.address);

            if ((int) snapshot.code.size() >= m_codeLines)
            {
                break;
            }
        }
    }

    memory = session.ExecuteLine (std::format ("D {:04X}", m_memoryAddress), CommandMode::AppleWin);

    if (const MemoryData * data = std::get_if<MemoryData> (&memory.data))
    {
        for (const MemoryRow & row : data->rows)
        {
            DebuggerViewSnapshot::MemoryLine  line;



            line.address = row.address;
            line.region  = GetRegionLabel (row.region);

            for (const std::optional<Byte> & b : row.bytes)
            {
                //  An I/O byte is not read for display, because reading one can
                //  change the machine; it shows as "--" rather than a guess.
                line.bytes      += b.has_value() ? std::format ("{:02X} ", *b) : std::string ("-- ");
                line.characters += (b.has_value() && (*b & 0x7F) >= 0x20 && (*b & 0x7F) < 0x7F) ? (char) (*b & 0x7F) : '.';
            }

            if (!line.bytes.empty())
            {
                line.bytes.pop_back();
            }

            snapshot.memory.push_back (std::move (line));

            if ((int) snapshot.memory.size() >= kMemoryRows)
            {
                break;
            }
        }
    }

    for (int id = 1; id <= kMaxMemoryWindows; id++)
    {
        std::optional<Word>  address = GetMemoryWindowAddress (id);

        if (address.has_value())
        {
            snapshot.memoryWindows.push_back (ReadMemoryWindow (session, id, *address));
        }
    }

    if (const StackData * data = std::get_if<StackData> (&stack.data))
    {
        for (const StackEntry & entry : data->entries)
        {
            snapshot.stack.push_back ({ entry.address, entry.value });
        }
    }

    if (const CallStackData * data = std::get_if<CallStackData> (&calls.data))
    {
        snapshot.callStack = *data;
    }

    if (const WatchListData * data = std::get_if<WatchListData> (&watches.data))
    {
        for (const WatchEntry & entry : data->entries)
        {
            snapshot.watches.push_back ({ entry.id, entry.address,
                                          entry.value.has_value() ? std::format ("{:04X}", *entry.value) : std::string ("--") });
        }
    }

    BuildSource (session, snapshot);
    BuildTrace  (session, snapshot);
    BuildPanels (session, snapshot);

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildTrace
//
//  The trace pane is HISTORY first count, for the window around where the
//  pane is scrolled; the trace size only says where that window starts.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildTrace (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    uint64_t  total = session.GetTarget().GetTraceSize();
    uint64_t  first = GetTraceWindowFirst (total, m_traceTop, kTraceRows);
    Reply     reply = session.ExecuteLine (GetHistoryLine (first, kTraceRows), CommandMode::AppleWin);



    if (TraceData * data = std::get_if<TraceData> (&reply.data))
    {
        snapshot.trace.isOn    = data->isOn;
        snapshot.trace.total   = data->total;
        snapshot.trace.first   = first;
        snapshot.trace.entries = std::move (data->entries);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetTraceWindowFirst
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DebuggerViewState::GetTraceWindowFirst (uint64_t total, std::optional<uint64_t> top, int rows)
{
    uint64_t  last = (total > (uint64_t) rows) ? total - (uint64_t) rows : 0;



    return top.has_value() ? std::min (*top, last) : last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetHistoryLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetHistoryLine (uint64_t first, int rows)
{
    return std::format ("HISTORY {} {}", first, rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::OpenMemoryWindow
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::OpenMemoryWindow (int id, Word address)
{
    if (id == 1)
    {
        m_memoryAddress = address;
    }
    else if (id >= 2 && id <= kMaxMemoryWindows)
    {
        m_extraWindows[(size_t) (id - 2)] = address;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::CloseMemoryWindow
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::CloseMemoryWindow (int id)
{
    if (id >= 2 && id <= kMaxMemoryWindows)
    {
        m_extraWindows[(size_t) (id - 2)].reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetMemoryWindowAddress
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> DebuggerViewState::GetMemoryWindowAddress (int id) const
{
    std::optional<Word>  address;



    if (id == 1)
    {
        address = m_memoryAddress;
    }
    else if (id >= 2 && id <= kMaxMemoryWindows)
    {
        address = m_extraWindows[(size_t) (id - 2)];
    }

    return address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildSource
//
//  The source pane's share of the snapshot, when a debug file is loaded: the
//  line at PC at both ends of any macro nesting, the source line of each code
//  row, the breakpoints that sit on lines, and the map from lines to
//  addresses, which is rebuilt only when another debug file is loaded.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildSource (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    const LineTable                       & table = session.GetLineTable();
    const DebugFile                       & file  = session.GetDebugFile();
    const std::vector<SourcePosition>     & atPc  = table.GetPositionsAt (snapshot.pc);
    DebuggerViewSnapshot::SourceState       state;
    std::string                             key;



    if (!session.HasDebugFile())
    {
        return;
    }

    key = session.GetDebugFileKey() + SourcePathList::WideToUtf8 (session.GetDebugFilePath());

    if (m_lineAddresses == nullptr || key != m_lineAddressesKey)
    {
        auto  addresses = std::make_shared<std::map<std::pair<int, int>, Word>>();

        for (const DebugLine & line : file.lines)
        {
            for (const std::pair<Word, Word> & range : table.GetRanges (line.file, line.line))
            {
                auto  found = addresses->find ({ line.file, line.line });

                if (found == addresses->end() || range.first < found->second)
                {
                    (*addresses)[{ line.file, line.line }] = range.first;
                }
            }
        }

        m_lineAddresses    = addresses;
        m_lineAddressesKey = key;
    }

    state.debugFilePath = session.GetDebugFilePath();
    state.programKey    = session.GetDebugFileKey();
    state.files         = file.files;
    state.stepBySource  = session.IsStepBySource();
    state.lineAddresses = m_lineAddresses;

    if (!atPc.empty())
    {
        state.fileId     = atPc.front().file;
        state.line       = atPc.front().line;
        state.bodyFileId = atPc.back().file;
        state.bodyLine   = atPc.back().line;
        state.depth      = atPc.back().depth;
    }

    for (DebuggerViewSnapshot::CodeLine & row : snapshot.code)
    {
        const std::vector<SourcePosition> & at = table.GetPositionsAt (row.address);

        if (!at.empty())
        {
            row.sourceFileId = at.front().file;
            row.sourceLine   = at.front().line;
        }
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        for (const SourcePosition & position : table.GetPositionsAt (bp.address))
        {
            state.breakpointLines.emplace_back (position.file, position.line, bp.id);
        }
    }

    snapshot.source = std::move (state);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ReadMemoryWindow
//
//  Through D, like the other panes, so a window shows exactly what the command
//  would; each row's region is given to every byte in it, which is exact
//  because rows start on eight-byte boundaries and no region changes inside
//  one.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot::MemoryWindow DebuggerViewState::ReadMemoryWindow (DebugSession & session, int id, Word address)
{
    DebuggerViewSnapshot::MemoryWindow  window;
    Word                                first = (Word) (address & ~(kMemoryRowBytes - 1));
    Word                                last  = (Word) std::min<uint32_t> ((uint32_t) first + kMemoryWindowBytes - 1, 0xFFFF);
    Reply                               reply = session.ExecuteLine (std::format ("D {:04X}:{:04X}", first, last), CommandMode::AppleWin);



    window.id    = id;
    window.first = first;

    if (const MemoryData * data = std::get_if<MemoryData> (&reply.data))
    {
        for (const MemoryRow & row : data->rows)
        {
            window.bytes.insert (window.bytes.end(), row.bytes.begin(), row.bytes.end());
            window.regions.insert (window.regions.end(), row.bytes.size(), row.region);
        }
    }

    return window;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetActionLine
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> DebuggerViewState::GetActionLine (
    DebuggerKeySchemes::Action   action,
    const DebuggerViewSnapshot * snapshot,
    int                          selectedRow)
{
    using Action = DebuggerKeySchemes::Action;

    std::optional<std::string>  line;
    std::optional<Word>         selected;
    std::optional<Word>         current;



    if (snapshot != nullptr && selectedRow >= 0 && selectedRow < (int) snapshot->code.size())
    {
        selected = snapshot->code[(size_t) selectedRow].address;
    }

    if (snapshot != nullptr)
    {
        current = selected.has_value() ? selected : std::optional<Word> (snapshot->pc);
    }

    switch (action)
    {
    case Action::Run:      line = GetRunLine();      break;
    case Action::StepInto: line = GetStepLine();     break;
    case Action::StepOver: line = GetStepOverLine(); break;
    case Action::StepOut:  line = GetStepOutLine();  break;

    case Action::RunToCursor:
        if (selected.has_value())
        {
            line = GetRunToCursorLine (*selected);
        }

        break;

    case Action::ToggleBreakpoint:
        if (snapshot != nullptr && current.has_value())
        {
            line = GetToggleBreakpointLine (*snapshot, *current);
        }

        break;

    case Action::Pause:
    default:
        break;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetModeLine
//
//  The controls build AppleWin lines and the session reads lines in its own
//  mode. Monitor mode reads an AppleWin line after its `/`; GSSquared has its
//  own words for most of what they send, and run to cursor, which has none,
//  stays an AppleWin line. WinDbg has a word for each.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetModeLine (const std::string & line, CommandMode mode)
{
    size_t       space = line.find (' ');
    std::string  name  = line.substr (0, space);
    std::string  rest  = (space == std::string::npos) ? std::string() : line.substr (space + 1);
    size_t       split = rest.find (' ');



    if (mode == CommandMode::Monitor)
    {
        return "/" + line;
    }

    if (mode == CommandMode::WinDbg)
    {
        return GetWinDbgLine (name, rest, line);
    }

    if (mode != CommandMode::GSSquared)
    {
        return line;
    }

    if (rest.empty())
    {
        if (name == "T")   { return "s"; }
        if (name == "P")   { return "o"; }
        if (name == "RTS") { return "r"; }
        if (name == "G")   { return "g"; }
    }

    if (name == "BP")  { return "bp " + rest; }
    if (name == "BPC") { return "nobp " + rest; }

    if (name == "MEB" && split != std::string::npos)
    {
        return rest.substr (0, split) + ":" + rest.substr (split);
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetWinDbgLine
//
//  A control's AppleWin line in WinDbg's words: t, p, gu, g, bp, bc and eb.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetWinDbgLine (const std::string & name, const std::string & rest, const std::string & line)
{
    static constexpr std::pair<const char *, const char *>  kWords[] =
    {
        { "T", "t" }, { "P", "p" }, { "RTS", "gu" }, { "G", "g" }, { "BP", "bp" }, { "BPC", "bc" }, { "MEB", "eb" },
    };



    for (const auto & [appleWin, windbg] : kWords)
    {
        if (name == appleWin)
        {
            return rest.empty() ? std::string (windbg) : std::string (windbg) + " " + rest;
        }
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetConsoleKeyAction
//
//  GSSquared steps and resumes by key, not by command, so a reader used to it
//  presses Space at an empty prompt. Once something is typed the keys type.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerKeySchemes::Action> DebuggerViewState::GetConsoleKeyAction (
    CommandMode  mode,
    WPARAM       vk,
    bool         ctrl,
    bool         alt,
    bool         shift,
    bool         isLineEmpty)
{
    if (mode != CommandMode::GSSquared || !isLineEmpty || ctrl || alt || shift)
    {
        return std::nullopt;
    }

    if (vk == VK_SPACE || vk == VK_F10)
    {
        return DebuggerKeySchemes::Action::StepInto;
    }

    if (vk == VK_RETURN)
    {
        return DebuggerKeySchemes::Action::Run;
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetToggleBreakpointLine
//
//  A click sets a breakpoint where there is none and clears the one that is
//  there. Clearing goes by id, so a click never clears a different breakpoint
//  that happens to cover the same address.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetToggleBreakpointLine (const DebuggerViewSnapshot & snapshot, Word address)
{
    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (bp.address == address)
        {
            return std::format ("BPC {}", bp.id);
        }
    }

    return std::format ("BP {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetPokeLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetPokeLine (Word address, Byte value)
{
    return std::format ("MEB {:04X} {:02X}", address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetRunToCursorLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetRunToCursorLine (Word address)
{
    return std::format ("G {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetPanelLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetPanelLine (const std::string & id, bool open, CommandMode mode)
{
    return std::format ("{}PANEL {}{}", mode == CommandMode::Monitor ? "/" : "", open ? "" : "CLOSE ", id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecuteLine
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewState::ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    Reply  reply = session.ExecuteLine (line, mode);



    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecuteWindowLine
//
//  THIS WINDOW SHOWS EVERY PANE AT ONCE, so AppleWin's commands that pick a
//  layout have nothing to change and say so. The screen views and appearance
//  commands are not available: the emulator window shows the screen, and the
//  theme sets the colors and font.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewState::ExecuteWindowLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    std::string              text     = line;
    size_t                   first    = line.find_first_not_of (" \t");
    std::istringstream       stream;
    std::string              name;
    std::string              argument;
    const AppleWinCommand  * entry    = nullptr;
    Reply                    reply;



    //  GSSquared and WinDbg have no layout commands, and their words are not
    //  AppleWin's.
    if (mode == CommandMode::GSSquared || mode == CommandMode::WinDbg)
    {
        return ExecuteLine (session, line, mode);
    }

    //  In Monitor mode only a `/` line is an AppleWin line.
    if (mode == CommandMode::Monitor)
    {
        if (first == std::string::npos || line[first] != '/')
        {
            return ExecuteLine (session, line, mode);
        }

        text = line.substr (first + 1);
    }

    stream.str (text);
    stream >> name >> argument;

    if (!name.empty() && !session.IsAssembling())
    {
        entry = AppleWinCommandTable::Find (name);
    }

    //  PANEL is the window's own: batch and the pipe report that it needs one.
    if (entry != nullptr && entry->verb == DebugVerb::ListPanels)
    {
        return ExecutePanelLine (session, text, line, mode);
    }

    if (entry == nullptr || entry->availability != CommandAvailability::WindowOnly)
    {
        return ExecuteLine (session, line, mode);
    }

    reply.command = line;

    switch (entry->family)
    {
    case AppleWinCommandFamily::Cursor:
        MoveCodePane (session, entry->name, reply);
        break;

    case AppleWinCommandFamily::MiniMemory:
        MoveMemoryPane (entry->name, argument, reply);
        break;

    case AppleWinCommandFamily::Window:
        if (std::string_view (entry->name).starts_with ("SOURCE"))
        {
            reply.SetError (CommandStatus::NotAvailable, "command not available", "SOURCE needs a link to an assembler listing.");
        }
        else
        {
            reply.data = MessageData { { "This window shows every pane at once." } };
        }

        break;

    case AppleWinCommandFamily::Views:
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("{} is not available here: the emulator window shows the screen.", entry->name));
        break;

    default:
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("{} is not available here: the Casso theme sets the window's colors and font.", entry->name));
        break;
    }

    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildPanels
//
//  Every provider the machine has is listed; only open panels are built, so a
//  device pays for its rows only while someone watches them. A panel whose
//  device is gone closes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildPanels (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    std::vector<const IDiagnosticsProvider *>  providers = session.GetTarget().GetDiagnosticsProviders();
    std::set<std::string>                      present;



    for (const IDiagnosticsProvider * provider : providers)
    {
        DebuggerViewSnapshot::PanelInfo  info  { provider->GetDiagnosticsId(), provider->GetDiagnosticsTitle(), false };
        DiagnosticsSnapshot              panel;

        present.insert (info.id);
        info.open = m_openPanels.contains (info.id);

        if (info.open)
        {
            panel.id     = info.id;
            panel.device = info.title;
            provider->GetDiagnostics (panel);
            snapshot.diagnostics.push_back (std::move (panel));
        }

        snapshot.panels.push_back (std::move (info));
    }

    std::erase_if (m_openPanels, [&present] (const std::string & id) { return !present.contains (id); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecutePanelLine
//
//  Parsed by the same parser batch uses, so the window and batch agree on
//  what a PANEL line means; only what it does differs.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewState::ExecutePanelLine (DebugSession & session, const std::string & text, const std::string & line, CommandMode mode)
{
    AppleWinParseResult  parsed = AppleWinParser::Parse (text, session);
    Reply                reply;



    reply.command = line;

    if (parsed.status != ParseStatus::Ok)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", parsed.error);
    }
    else
    {
        RunPanelCommand (session, parsed.command, reply);
    }

    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::RunPanelCommand
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::RunPanelCommand (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::vector<const IDiagnosticsProvider *>  providers = session.GetTarget().GetDiagnosticsProviders();
    constexpr int                              kIdWidth  = 14;
    const IDiagnosticsProvider               * provider  = nullptr;
    MessageData                                message;



    if (command.verb == DebugVerb::ListPanels)
    {
        for (const IDiagnosticsProvider * each : providers)
        {
            std::string  id = each->GetDiagnosticsId();

            message.lines.push_back (std::format ("{:<{}}{}{}", id, kIdWidth, each->GetDiagnosticsTitle(), m_openPanels.contains (id) ? " (open)" : ""));
        }

        if (providers.empty())
        {
            message.lines.push_back ("This machine has no device panels.");
        }

        reply.data = std::move (message);
        return;
    }

    provider = FindProvider (providers, command.text);

    if (provider == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no such panel",
                        std::format ("This machine has no {} panel. PANEL LIST lists the ones it has.", command.text));
        return;
    }

    if (command.verb == DebugVerb::ClosePanel)
    {
        ClosePanel (provider->GetDiagnosticsId());
        reply.data = MessageData { { std::format ("The {} panel is closed.", provider->GetDiagnosticsTitle()) } };
    }
    else
    {
        OpenPanel (provider->GetDiagnosticsId());
        reply.data = MessageData { { std::format ("The {} panel is open.", provider->GetDiagnosticsTitle()) } };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::FindProvider
//
//  By id or by title, either case.
//
////////////////////////////////////////////////////////////////////////////////

const IDiagnosticsProvider * DebuggerViewState::FindProvider (const std::vector<const IDiagnosticsProvider *> & providers, const std::string & name)
{
    auto  isSame = [] (const std::string & a, const std::string & b)
    {
        return a.size() == b.size() &&
               std::equal (a.begin(), a.end(), b.begin(), [] (char x, char y) { return tolower ((unsigned char) x) == tolower ((unsigned char) y); });
    };



    for (const IDiagnosticsProvider * provider : providers)
    {
        if (isSame (provider->GetDiagnosticsId(), name) || isSame (provider->GetDiagnosticsTitle(), name))
        {
            return provider;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::SetCodeLines
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::SetCodeLines (int lines)
{
    //  A pane too short to hold anything still gets a line; the ceiling keeps
    //  a dragged-tall pane from disassembling half the address space.
    m_codeLines = (std::clamp) (lines, 1, 200);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ChooseCodeStart
//
//  A pinned pane stays where it was put. A following pane MOVES ONLY WHEN IT
//  HAS TO: while the PC is among the lines already shown, the anchor is left
//  alone and the marker moves down the rows the user is reading. Anchoring on
//  the PC itself, which is what this replaces, re-disassembled from a new
//  address on every snapshot -- a window that never holds still, and one that
//  always showed the PC on its top line with nothing above it.
//
//  When the PC does leave, it comes back in the MIDDLE, so what led there is
//  on screen with it.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::ChooseCodeStart (DebugSession & session, Word pc) const
{
    bool  shown = std::find (m_shownCode.begin(), m_shownCode.end(), pc) != m_shownCode.end();



    if (m_codeAddress.has_value())
    {
        return *m_codeAddress;
    }

    if (!shown)
    {
        m_followAnchor = FindStartAbove (session, pc, m_codeLines / 2);
    }

    return m_followAnchor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::FindStartAbove
//
//  The 6502 cannot be disassembled backwards: an instruction begins where the
//  one before it ended, and reading the bytes above an address says nothing
//  about where they start. So this walks FORWARD from as far back as `before`
//  instructions could reach -- three bytes each -- and keeps the first
//  alignment that arrives exactly at `pc`, which is the one the machine was
//  executing. Where none does, the PC anchors the pane itself, which is what
//  the pane did before and is right for a PC in data.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::FindStartAbove (DebugSession & session, Word pc, int before)
{
    Word   first   = 0;
    Reply  reply;
    int    index   = 0;



    if (before <= 0 || pc < (Word) (before * 3))
    {
        return pc;
    }

    first = (Word) (pc - (Word) (before * 3));
    reply = session.ExecuteLine (std::format ("U {:04X}:{:04X}", first, pc), CommandMode::AppleWin);

    if (const DisassemblyData * data = std::get_if<DisassemblyData> (&reply.data))
    {
        for (index = 0; index < (int) data->lines.size(); index++)
        {
            if (data->lines[(size_t) index].instruction.address == pc)
            {
                //  `before` lines above the PC, or as many as were found.
                return data->lines[(size_t) (std::max) (0, index - before)].instruction.address;
            }
        }
    }

    return pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::MoveCodePane
//
//  Where the code pane is pinned, by name: "." lets it follow the PC again,
//  and the rest move it to an address the name stands for.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::MoveCodePane (DebugSession & session, const std::string & name, Reply & reply)
{
    Word                 start  = m_codeAddress.value_or (session.GetTarget().GetRegisters().pc);
    std::optional<Word>  target = start;



    if (name == ".")
    {
        m_codeAddress = std::nullopt;
        reply.data    = MessageData { { "The code pane follows the PC." } };
        return;
    }

    if (name == "RET")
    {
        target = GetReturnAddress (session);
    }
    else if (name == "->")
    {
        target = GetOperandAddress (session, start);

        if (!target.has_value())
        {
            reply.SetError (CommandStatus::Error, "no address", std::format ("The instruction at ${:04X} has no address operand.", start));
            return;
        }
    }
    else if (name == "^" || name == "V" || name == "PAGEUP" || name == "PAGEDN")
    {
        int  count = (name == "^" || name == "V") ? 1 : kCodeLines;

        for (int i = 0; i < count; i++)
        {
            *target = (name == "^" || name == "PAGEUP") ? GetPreviousInstruction (session, *target)
                                                        : (Word) (*target + GetInstructionLength (session, *target));
        }
    }
    else if (name == "PAGEUP256")   { *target = (Word) (start - 0x0100); }
    else if (name == "PAGEUP4K")    { *target = (Word) (start - 0x1000); }
    else if (name == "PAGEDOWN256") { *target = (Word) (start + 0x0100); }
    else if (name == "PAGEDOWN4K")  { *target = (Word) (start + 0x1000); }

    m_codeAddress = target;
    reply.data    = MessageData { { std::format ("The code pane is at ${:04X}.", *target) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::MoveMemoryPane
//
//  This window has one memory pane, which shows bytes and characters
//  together, so MD, MA and MT and both of their panes all move it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::MoveMemoryPane (const std::string & name, const std::string & argument, Reply & reply)
{
    std::string_view  digits  = argument;
    unsigned          address = 0;



    if (digits.starts_with ('$'))
    {
        digits.remove_prefix (1);
    }

    auto [end, error] = std::from_chars (digits.data(), digits.data() + digits.size(), address, 16);

    if (digits.empty() || error != std::errc() || end != digits.data() + digits.size() || address > 0xFFFF)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} needs a hex address.", name));
        return;
    }

    m_memoryAddress = (Word) address;
    reply.data      = MessageData { { std::format ("The memory pane is at ${:04X}.", address) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetInstructionLength
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::GetInstructionLength (DebugSession & session, Word address)
{
    Reply                   code = session.ExecuteLine (std::format ("U {:04X}", address), CommandMode::AppleWin);
    const DisassemblyData * data = std::get_if<DisassemblyData> (&code.data);



    if (data == nullptr || data->lines.empty() || data->lines[0].instruction.bytes.empty())
    {
        return 1;
    }

    return (Word) data->lines[0].instruction.bytes.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetPreviousInstruction
//
//  Code cannot be disassembled backward with certainty, so this takes the
//  longest instruction that ends exactly where the given one starts, and one
//  byte back when none does.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::GetPreviousInstruction (DebugSession & session, Word address)
{
    for (Word back = 3; back >= 1; back--)
    {
        if (GetInstructionLength (session, (Word) (address - back)) == back)
        {
            return (Word) (address - back);
        }
    }

    return (Word) (address - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetReturnAddress
//
//  JSR pushes the address of its own last byte, so RTS returns one past what
//  the stack holds.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> DebuggerViewState::GetReturnAddress (DebugSession & session)
{
    const IDebugTarget  & target = session.GetTarget();
    Byte                  s      = target.GetRegisters().sp;
    Byte                  low    = 0;
    Byte                  high   = 0;



    if (!target.TryPeek ((Word) (0x0100 + (Byte) (s + 1)), low) ||
        !target.TryPeek ((Word) (0x0100 + (Byte) (s + 2)), high))
    {
        return std::nullopt;
    }

    return (Word) (((high << 8) | low) + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetOperandAddress
//
//  A branch, jump or absolute operand is disassembled with four hex digits; a
//  zero-page or immediate operand has two, and is not taken as an address.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> DebuggerViewState::GetOperandAddress (DebugSession & session, Word address)
{
    Reply                   code    = session.ExecuteLine (std::format ("U {:04X}", address), CommandMode::AppleWin);
    const DisassemblyData * data    = std::get_if<DisassemblyData> (&code.data);
    size_t                  dollar  = std::string::npos;
    unsigned                operand = 0;



    if (data == nullptr || data->lines.empty())
    {
        return std::nullopt;
    }

    const std::string & text = data->lines[0].instruction.operand;

    dollar = text.find ('$');

    if (dollar == std::string::npos || text.size() < dollar + 5 ||
        std::from_chars (text.data() + dollar + 1, text.data() + dollar + 5, operand, 16).ptr != text.data() + dollar + 5)
    {
        return std::nullopt;
    }

    return (Word) operand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetMissingFileVerb
//
//  ReadFile or WriteFile when a Monitor line holds an R or W with no file
//  name, which the window asks for before the line runs. Batch and the pipe
//  have no one to ask, so there the handler reports the error instead.
//
//  The line is parsed against a scratch state: a range comes from the line
//  itself, and the session's own state must not move for a line that has not
//  run yet.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebugVerb> DebuggerViewState::GetMissingFileVerb (const std::string & line, CommandMode mode)
{
    MonitorState        scratch;
    MonitorParseResult  parsed;



    if (mode != CommandMode::Monitor)
    {
        return std::nullopt;
    }

    parsed = MonitorParser::Parse (line, scratch);

    for (const DebugCommand & command : parsed.commands)
    {
        if ((command.verb == DebugVerb::ReadFile || command.verb == DebugVerb::WriteFile) && command.text.empty())
        {
            return command.verb;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetLineWithFileName
//
//  Quoted, so a path with spaces stays one name; the Monitor parser takes the
//  quotes off again.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetLineWithFileName (const std::string & line, const std::string & path)
{
    return std::format ("{}\"{}\"", line, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetRegionLabel
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetRegionLabel (MemoryRegion region)
{
    switch (region)
    {
    case MemoryRegion::MainRam: return "RAM";
    case MemoryRegion::AuxRam:  return "AUX";
    case MemoryRegion::LcBank1: return "LC1";
    case MemoryRegion::LcBank2: return "LC2";
    case MemoryRegion::Rom:     return "ROM";
    case MemoryRegion::SlotRom: return "SLOT";
    case MemoryRegion::Io:      return "I/O";
    }

    return "?";
}
