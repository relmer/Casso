#include "Pch.h"

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/InstructionEffect.h"

#include "Debugger/DebugSession.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Debugger/Source/SourcePathList.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/EffectiveAddress.h"
#include "Debugger/SymbolDescriptions.h"





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

DebuggerViewSnapshot DebuggerViewState::Build (DebugSession & session, bool isPaused) const
{
    DebuggerViewSnapshot  snapshot;
    Reply                 registers   = session.ExecuteLine ("R",     CommandMode::AppleWin);
    Reply                 breakpoints = session.ExecuteLine ("BPL",   CommandMode::AppleWin);
    Reply                 stack       = session.ExecuteLine ("STACK", CommandMode::AppleWin);
    Reply                 watches     = session.ExecuteLine ("WL",    CommandMode::AppleWin);
    Reply                 calls       = session.ExecuteLine ("CALLS", CommandMode::AppleWin);
    Reply                 memory;



    snapshot.isPaused = isPaused;
    snapshot.mode     = session.GetMode();
    snapshot.goTo     = m_goTo;
    snapshot.machine  = session.GetTarget().GetMachineInfo().name;

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
                                              info.enabled, info });
        }
    }

    //  Each disassembly view open, the first always; one of them follows the
    //  PC and the rest stay where they were put.
    for (int view = 0; view < kMaxCodeViews; view++)
    {
        if (view == 0 || m_code[(size_t) view].open)
        {
            snapshot.codeViews[(size_t) view] = BuildCode (session, snapshot, view);
            snapshot.codeOpen[(size_t) view]  = true;
        }
    }

    snapshot.code       = snapshot.codeViews[0];
    snapshot.followView = m_follow;

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

    if (isPaused)
    {
        BuildAutoWatches (session, snapshot);
    }

    BuildSource (session, snapshot);
    BuildTrace  (session, snapshot);
    BuildPanels (session, snapshot);

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildAutoWatches
//
//  What the instruction at the PC touches, then what the one just executed
//  touched that this one does not (FR-095). Both come from InstructionTouches,
//  the same account the code pane annotates from.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildAutoWatches (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    const Cpu6502Registers  & now    = session.GetTarget().GetRegisters();
    Reply                     code   = session.ExecuteLine (std::format ("U {:04X}", now.pc), CommandMode::AppleWin);
    Word                        length  = 1;
    InstructionTouches::Result  current;



    if (const DisassemblyData * data = std::get_if<DisassemblyData> (&code.data); data != nullptr && !data->lines.empty())
    {
        length = (Word) data->lines[0].instruction.bytes.size();
    }

    current = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(), now, now.pc, length);

    AddAutoWatches (session, current, now, false, snapshot.autoWatches);

    //  A new stop. The instruction that just ran is known only when this stop
    //  is exactly where the last one's instruction would have left the
    //  machine, which is what a step looks like; after a free run it is not
    //  known, and is left out rather than shown for the wrong instruction.
    if (!m_lastStop.isValid || !IsSameRegisters (m_lastStop.at, now))
    {
        bool  isStep = m_lastStop.isValid && m_lastStop.here.isKnown && IsSameRegisters (m_lastStop.here.after, now);

        m_lastStop.previous = isStep ? std::optional<InstructionTouches::Result> (m_lastStop.here) : std::nullopt;
        m_lastStop.here     = current;
        m_lastStop.at       = now;
        m_lastStop.isValid  = true;
    }

    if (m_lastStop.previous.has_value())
    {
        AddAutoWatches (session, *m_lastStop.previous, now, true, snapshot.autoWatches);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::IsSameRegisters
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::IsSameRegisters (const Cpu6502Registers & left, const Cpu6502Registers & right)
{
    return left.pc == right.pc && left.a == right.a && left.x == right.x &&
           left.y  == right.y  && left.sp == right.sp && left.p == right.p;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::AddAutoWatches
//
//  Each register, flag and address the instruction touches, with what it holds
//  NOW. Something already listed for the current instruction is not listed
//  again for the previous one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::AddAutoWatches (DebugSession & session, const InstructionTouches::Result & touches,
                                        const Cpu6502Registers & now, bool isPrevious,
                                        std::vector<DebuggerViewSnapshot::AutoWatchLine> & lines)
{
    static constexpr std::pair<char, Byte>  kFlagBits[] =
    {
        { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
    };



    for (const InstructionTouches::Item & item : touches.items)
    {
        DebuggerViewSnapshot::AutoWatchLine  line;
        std::string                          symbol;
        SymbolTableId                        table = SymbolTableId::Main;
        Byte                                 value = 0;



        line.isRead     = item.isRead;
        line.isWrite    = item.isWrite;
        line.isPrevious = isPrevious;

        if (item.kind == InstructionTouches::Kind::Register)
        {
            Byte  held = (item.name == "A") ? now.a : (item.name == "X") ? now.x : (item.name == "Y") ? now.y : now.sp;

            line.key   = "R:" + item.name;
            line.label = item.name;
            line.value = std::format ("{:02X}", held);
        }
        else if (item.kind == InstructionTouches::Kind::Flag)
        {
            line.key   = "F:" + item.name;
            line.label = item.name;

            for (const auto & [letter, bit] : kFlagBits)
            {
                if (item.name[0] == letter)
                {
                    line.value = (now.p & bit) ? "1" : "0";
                }
            }
        }
        else
        {
            session.GetSymbols().TryFindName (item.address, symbol, table);

            line.key   = std::format ("M:{:04X}", item.address);
            line.label = symbol.empty() ? std::format ("${:04X}", item.address) : std::format ("{} ${:04X}", symbol, item.address);

            //  A soft switch has no value to read without operating it.
            line.value = (session.GetTarget().GetRegion (item.address) == MemoryRegion::Io || !session.TryPeek (item.address, value))
                         ? std::string ("--") : std::format ("{:02X}", value);
        }

        if (std::none_of (lines.begin(), lines.end(),
                          [&line] (const DebuggerViewSnapshot::AutoWatchLine & other) { return other.key == line.key; }))
        {
            lines.push_back (std::move (line));
        }
    }
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
//  Any other goes through WinDbg mode's engine marker, `!`, which is how
//  that mode reaches Casso's own commands (FR-014) -- MODE among them.
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

    return "!" + line;
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
//  DebuggerViewState::GetWatchEditLines
//
//  Each edit becomes the command anyone could have typed, so it lands in the
//  console and the session's history like one:
//
//    manual watch, expression   the watch moved to the new address
//    manual watch, value        the word written to the watched address
//    automatic register         R <register> <value>
//    automatic flag             R P with that one bit set or cleared
//    automatic address          the byte written there
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewState::GetWatchEditLines (const DebuggerViewSnapshot & snapshot,
                                                              std::optional<int> watchId, std::optional<int> autoIndex,
                                                              int column, const std::string & typed)
{
    static constexpr std::pair<char, Byte>  kFlagBits[] =
    {
        { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
    };
    std::string  text = typed;
    std::string  key;



    while (!text.empty() && isspace ((unsigned char) text.back()))  { text.pop_back(); }
    while (!text.empty() && isspace ((unsigned char) text.front())) { text.erase (0, 1); }

    if (text.empty())
    {
        return {};
    }

    if (watchId.has_value())
    {
        for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
        {
            if (watch.id == *watchId)
            {
                return (column == 0) ? std::vector<std::string> { std::format ("WC {}", watch.id), std::format ("W {}", text) }
                                     : std::vector<std::string> { std::format ("MEW {:04X} {}", watch.address, text) };
            }
        }

        return {};
    }

    if (!autoIndex.has_value() || *autoIndex < 0 || *autoIndex >= (int) snapshot.autoWatches.size() || column == 0)
    {
        return {};
    }

    key = snapshot.autoWatches[(size_t) *autoIndex].key;

    if (key.starts_with ("R:"))
    {
        return { std::format ("R {} {}", key.substr (2), text) };
    }

    if (key.starts_with ("M:"))
    {
        return { std::format ("MEB {} {}", key.substr (2), text) };
    }

    //  A flag is 0 or 1 and nothing else; it is written as the whole status
    //  register with that one bit changed.
    if (key.starts_with ("F:") && (text == "0" || text == "1"))
    {
        for (const DebuggerViewSnapshot::RegisterRow & reg : snapshot.registers)
        {
            unsigned  p = 0;

            if (reg.name != "P" || std::from_chars (reg.value.data(), reg.value.data() + reg.value.size(), p, 16).ec != std::errc())
            {
                continue;
            }

            for (const auto & [letter, bit] : kFlagBits)
            {
                if (key[2] == letter)
                {
                    return { std::format ("R P {:02X}", (text == "1") ? (Byte) (p | bit) : (Byte) (p & ~bit)) };
                }
            }
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetWatchUndo
//
//  The value each kind of edit overwrote, written back the way the edit wrote
//  it. A value the pane could not read -- a soft switch shows "--" -- has
//  nothing to put back, and the edit is then not undoable.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerViewState::WatchUndo> DebuggerViewState::GetWatchUndo (const DebuggerViewSnapshot & before,
                                                                            std::optional<int> watchId,
                                                                            std::optional<int> autoIndex, int column)
{
    WatchUndo  undo;



    if (watchId.has_value())
    {
        for (const DebuggerViewSnapshot::WatchLine & watch : before.watches)
        {
            if (watch.id != *watchId)
            {
                continue;
            }

            if (column == 0)
            {
                undo.restoreAddress = watch.address;

                for (const DebuggerViewSnapshot::WatchLine & other : before.watches)
                {
                    undo.movedFromIds.push_back (other.id);
                }

                return undo;
            }

            if (watch.value == "--")
            {
                return std::nullopt;
            }

            undo.lines = { std::format ("MEW {:04X} {}", watch.address, watch.value) };
            return undo;
        }

        return std::nullopt;
    }

    if (!autoIndex.has_value() || *autoIndex < 0 || *autoIndex >= (int) before.autoWatches.size() || column == 0)
    {
        return std::nullopt;
    }

    //  An automatic watch's edit and its undo are the same command with the
    //  old value in it -- a flag's, the whole status register as it was.
    const DebuggerViewSnapshot::AutoWatchLine & line = before.autoWatches[(size_t) *autoIndex];

    if (line.value == "--")
    {
        return std::nullopt;
    }

    if (line.key.starts_with ("F:"))
    {
        for (const DebuggerViewSnapshot::RegisterRow & reg : before.registers)
        {
            if (reg.name == "P")
            {
                undo.lines = { "R P " + reg.value };
                return undo;
            }
        }

        return std::nullopt;
    }

    undo.lines = GetWatchEditLines (before, std::nullopt, autoIndex, 1, line.value);

    return undo.lines.empty() ? std::nullopt : std::optional<WatchUndo> (undo);
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

std::string DebuggerViewState::GetPanelLine (const std::string & id, bool open)
{
    return std::format ("PANEL {}{}", open ? "" : "CLOSE ", id);
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
//  DebuggerViewState::BuildCode
//
//  One disassembly view's lines, from where ChooseCodeStart puts it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerViewSnapshot::CodeLine> DebuggerViewState::BuildCode (DebugSession & session, const DebuggerViewSnapshot & snapshot, int view) const
{
    CodeView                                     & v         = m_code[(size_t) view];
    std::vector<DebuggerViewSnapshot::CodeLine>    lines;
    Reply                                          code;
    Word                                           codeStart = 0;



    codeStart = ChooseCodeStart (session, snapshot.pc, view);
    //  A range, so the count is ours rather than the command's default: the
    //  pane holds as many lines as it has room for, and three bytes an
    //  instruction covers the longest the 6502 has.
    code = session.ExecuteLine (std::format ("U {:04X}:{:04X}", codeStart,
                                             (Word) (codeStart + (Word) (v.lines * 3))),
                                CommandMode::AppleWin);

    v.shown.clear();

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
            row.target        = line.instruction.hasTarget ? std::optional<Word> (line.instruction.target) : std::nullopt;
            //  Only while the machine is paused (FR-110): a running machine
            //  is somewhere else by the time these are drawn.
            //
            //  ONE RUN ANSWERS BOTH COLUMNS. What the instruction reads is
            //  the left annotation and what it leaves is the right one, and
            //  both come from the same trip through the core.
            if (snapshot.isPaused)
            {
                const Cpu6502Registers    & now     = session.GetTarget().GetRegisters();
                InstructionTouches::Result  touches = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(),
                                                                                now, line.instruction.address,
                                                                                (Word) line.instruction.bytes.size());

                row.annotation = GetAnnotation (session, line, now, touches);
                row.effect     = row.isCurrent ? GetEffect (session, now, touches,
                                                            (Word) (line.instruction.address + line.instruction.bytes.size()))
                                               : std::string();
            }

            if (line.instruction.hasOperandAddress || line.instruction.operand.starts_with ("("))
            {
                row.memoryOperand = line.instruction.operand;
                row.shownOperand  = line.GetShownOperand();
            }

            for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
            {
                if (bp.address == row.address)
                {
                    row.isEnabled     = row.hasBreakpoint ? (row.isEnabled || bp.enabled) : bp.enabled;
                    row.hasBreakpoint = true;
                }
            }

            lines.push_back (row);

            v.shown.push_back (row.address);

            if ((int) lines.size() >= v.lines)
            {
                break;
            }
        }
    }


    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::OpenCodeView
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::OpenCodeView (int view, Word address)
{
    if (view < 1 || view >= kMaxCodeViews)
    {
        return;
    }

    m_code[(size_t) view].open = true;
    SetCodeAddress (std::nullopt, view);
    CenterCodeOn   (address, view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::CloseCodeView
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::CloseCodeView (int view)
{
    if (view < 1 || view >= kMaxCodeViews)
    {
        return;
    }

    m_code[(size_t) view].open = false;

    if (m_follow == view)
    {
        SetFollowView (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::SetFollowView
//
//  The view giving up the PC is pinned where it stands, so it does not jump;
//  the one taking it follows from where it is, re-anchoring only once the PC
//  is off its lines.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::SetFollowView (int view)
{
    CodeView  & from = m_code[(size_t) m_follow];



    if (view < 0 || view >= kMaxCodeViews || !IsCodeViewOpen (view) || view == m_follow)
    {
        return;
    }

    if (!from.address.has_value())
    {
        from.address = from.shown.empty() ? from.followAnchor : from.shown.front();
    }

    m_follow = view;
    SetCodeAddress (std::nullopt, view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::SetCodeLines
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::SetCodeLines (int lines, int view)
{
    //  A pane too short to hold anything still gets a line; the ceiling keeps
    //  a dragged-tall pane from disassembling half the address space.
    m_code[(size_t) view].lines = (std::clamp) (lines, 1, 200);
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

Word DebuggerViewState::ChooseCodeStart (DebugSession & session, Word pc, int view) const
{
    CodeView  & v     = m_code[(size_t) view];
    bool        shown = std::find (v.shown.begin(), v.shown.end(), pc) != v.shown.end();
    Word        top   = 0;



    if (v.centerOnPc)
    {
        v.centerOn   = pc;
        v.centerOnPc = false;
    }

    //  The following view put somewhere -- scrolled, or navigated to -- holds
    //  there only until the PC moves. It then follows again from where it
    //  stands, so a PC still on its lines moves the marker, not the view.
    if (view == m_follow && v.address.has_value() && v.pinnedAtPc.has_value() && *v.pinnedAtPc != pc)
    {
        v.followAnchor = *v.address;
        v.address.reset();
        v.pinnedAtPc.reset();
    }

    //  A view that does not follow the PC and has not been put anywhere
    //  starts with the PC in its middle, and stays there after.
    if (view != m_follow && !v.address.has_value() && !v.centerOn.has_value())
    {
        v.centerOn = pc;
    }

    //  A navigation: the address in the middle, or as far down as there are
    //  instructions above it to fill the lines over it.
    if (v.centerOn.has_value())
    {
        top = *v.centerOn;

        for (int above = v.lines / 2; above > 0 && top == *v.centerOn; above--)
        {
            top = FindStartAbove (session, *v.centerOn, above);
        }

        v.address = top;
        v.centerOn.reset();
    }

    if (v.scrollLines != 0)
    {
        top = v.address.value_or (shown ? v.followAnchor : FindStartAbove (session, pc, v.lines / 2));

        for (int i = 0; i < v.scrollLines && top < 0xFFFF; i++)
        {
            Word  next = (Word) (top + GetInstructionLength (session, top));

            top = (next > top) ? next : top;
        }

        //  Up: the alignment that lands on the top line, from as many lines
        //  above as can be found; a byte at a time where none can.
        if (v.scrollLines < 0)
        {
            Word  from = top;

            for (int above = -v.scrollLines; above > 0 && top == from; above--)
            {
                top = FindStartAbove (session, from, above);
            }

            if (top == from && from > 0)
            {
                top = (Word) (from - (Word) (std::min) ((int) from, -v.scrollLines));
            }
        }

        v.address = top;
        v.scrollLines = 0;
    }

    if (v.address.has_value())
    {
        if (!v.pinnedAtPc.has_value())
        {
            v.pinnedAtPc = pc;
        }

        return *v.address;
    }

    v.pinnedAtPc.reset();

    if (!shown)
    {
        v.followAnchor = FindStartAbove (session, pc, v.lines / 2);
    }

    return v.followAnchor;
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
    Word                 start  = m_code[(size_t) m_follow].address.value_or (session.GetTarget().GetRegisters().pc);
    std::optional<Word>  target = start;



    if (name == ".")
    {
        m_code[(size_t) m_follow].address = std::nullopt;
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

    m_code[(size_t) m_follow].address = target;
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
//  DebuggerViewState::GetAnnotation
//
//  What the instruction acts on, in the manner of AppleWin's and VICE's
//  monitors: the registers and flags it reads, the address it would touch
//  with the registers as they stand, and the byte there -- `X=02 $067B=A0
//  C=1`.
//
//  EVERY PART OF THAT COMES FROM InstructionTouches, which asks the core
//  itself. A branch shows the flag it tests, and a compare the register it
//  compares, because running the instruction with that input changed lands
//  it somewhere else -- not because a table here says so.
//
//  An I/O address shows what the switch does instead of a byte: the byte
//  does not exist until a read happens, and that read operates the machine
//  (FR-112).
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetAnnotation (DebugSession & session, const DisassemblyLine & line,
                                              const Cpu6502Registers & registers, const InstructionTouches::Result & touches)
{
    std::string  registerText;
    std::string  addressText;
    std::string  flagText;
    Byte         value = 0;



    for (const InstructionTouches::Item & item : touches.items)
    {
        if (!item.isRead)
        {
            continue;
        }

        if (item.kind == InstructionTouches::Kind::Register)
        {
            Byte  held = (item.name == "A") ? registers.a : (item.name == "X") ? registers.x
                       : (item.name == "Y") ? registers.y : registers.sp;

            registerText += std::format ("{}={:02X} ", item.name, held);
        }
        else if (item.kind == InstructionTouches::Kind::Flag)
        {
            static constexpr std::pair<char, Byte>  kBits[] =
            {
                { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
            };

            for (const auto & [letter, bit] : kBits)
            {
                if (item.name[0] == letter)
                {
                    flagText += std::format ("{}={} ", letter, (registers.p & bit) ? 1 : 0);
                }
            }
        }
    }

    //  The address the instruction works on is the last one it touched: an
    //  indirect mode reads its pointer first, and the pointer is not what the
    //  line is about.
    for (const InstructionTouches::Item & item : touches.items)
    {
        if (item.kind != InstructionTouches::Kind::Address)
        {
            continue;
        }

        if (session.GetTarget().GetRegion (item.address) == MemoryRegion::Io)
        {
            std::string  action = SymbolDescriptions::GetAction (line.operandSymbol);

            addressText = action.empty() ? std::format ("${:04X}", item.address) : action;
        }
        else if (session.TryPeek (item.address, value))
        {
            addressText = std::format ("${:04X}={:02X}", item.address, value);
        }
        else
        {
            addressText = std::format ("${:04X}", item.address);
        }
    }

    std::string  text = registerText + addressText + (addressText.empty() ? "" : " ") + flagText;

    while (!text.empty() && text.back() == ' ')
    {
        text.pop_back();
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetEffect
//
//  What running the PC's instruction would leave behind (FR-107), written
//  from the run InstructionTouches already made (FR-111).
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetEffect (DebugSession & session, const Cpu6502Registers & registers,
                                          const InstructionTouches::Result & touches, Word next)
{
    //  A write to a soft switch stores no byte: it operates the machine, and
    //  what it operates is what the line should say (FR-112).
    auto  describeWrite = [&session] (Word address) -> std::string
    {
        std::vector<std::string>  names;

        if (session.GetTarget().GetRegion (address) != MemoryRegion::Io)
        {
            return {};
        }

        session.GetSymbols().FindNames (address, names);

        return SymbolDescriptions::GetAction (SymbolDescriptions::ChooseByDirection (names, true));
    };



    if (!touches.isKnown)
    {
        return {};
    }

    return InstructionEffect::Format (registers, touches.after, touches.writes, next, describeWrite);
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
//  DebuggerViewState::ResolveGoTo
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> DebuggerViewState::ResolveGoTo (const std::string & text, const Cpu6502Registers & registers, const GoToPeek & peek)
{
    std::string          s;
    std::string          core;
    std::string          index;
    bool                 indirect    = false;
    bool                 indexInside = false;
    unsigned long        value       = 0;
    bool                 isZeroPage  = false;
    std::optional<Byte>  lo;
    std::optional<Byte>  hi;
    Word                 pointer     = 0;



    for (char ch : text)
    {
        if (!std::isspace ((unsigned char) ch))
        {
            s += (char) std::toupper ((unsigned char) ch);
        }
    }

    if (s == "PC")              { return registers.pc; }
    if (s == "A")               { return (Word) registers.a; }
    if (s == "X")               { return (Word) registers.x; }
    if (s == "Y")               { return (Word) registers.y; }
    if (s == "S" || s == "SP")  { return (Word) (0x0100 + registers.sp); }

    //  (zp,X), (zp),Y and (abs): the parentheses, and where the index sits.
    if (s.starts_with ("("))
    {
        indirect = true;

        if (s.ends_with (",X)"))
        {
            indexInside = true;
            index       = "X";
            core        = s.substr (1, s.size() - 4);
        }
        else if (s.ends_with ("),Y"))
        {
            index = "Y";
            core  = s.substr (1, s.size() - 4);
        }
        else if (s.ends_with (")"))
        {
            core = s.substr (1, s.size() - 2);
        }
        else
        {
            return std::nullopt;
        }
    }
    else if (s.ends_with (",X") || s.ends_with (",Y"))
    {
        index = s.substr (s.size() - 1);
        core  = s.substr (0, s.size() - 2);
    }
    else
    {
        core = s;
    }

    if (core.starts_with ("$"))
    {
        core = core.substr (1);
    }

    if (core.empty() || core.size() > 4 || core.find_first_not_of ("0123456789ABCDEF") != std::string::npos)
    {
        return std::nullopt;
    }

    value      = std::stoul (core, nullptr, 16);
    isZeroPage = core.size() <= 2;

    if (!indirect)
    {
        if (index.empty())
        {
            return (Word) value;
        }

        value += (index == "X") ? registers.x : registers.y;
        return isZeroPage ? (Word) (value & 0xFF) : (Word) value;
    }

    //  (abs) takes any address; the indexed forms, a zero-page pointer.
    if (!index.empty() && !isZeroPage)
    {
        return std::nullopt;
    }

    pointer = indexInside ? (Word) ((value + registers.x) & 0xFF) : (Word) value;
    lo      = peek (pointer);
    hi      = peek (isZeroPage ? (Word) ((pointer + 1) & 0xFF) : (Word) (pointer + 1));

    if (!lo.has_value() || !hi.has_value())
    {
        return std::nullopt;
    }

    value = (unsigned long) (*lo | (*hi << 8));

    if (index == "Y")
    {
        value += registers.y;
    }

    return (Word) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::RequestGoTo
//
//  Resolved here, on the CPU thread, where the registers and memory are; a
//  pointer in I/O is not read, since reading one changes the machine.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::RequestGoTo (DebugSession & session, int window, const std::string & text)
{
    DebuggerViewSnapshot::GoTo  goTo;
    IDebugTarget              & target = session.GetTarget();



    goTo.window  = window;
    goTo.text    = text;
    goTo.serial  = m_goTo.has_value() ? m_goTo->serial + 1 : 1;
    goTo.address = ResolveGoTo (text, target.GetRegisters(), [&session, &target] (Word address) -> std::optional<Byte>
    {
        Byte  value = 0;

        if (target.GetRegion (address) == MemoryRegion::Io || !session.TryPeek (address, value))
        {
            return std::nullopt;
        }

        return value;
    });

    m_goTo = goTo;
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
