#include "Pch.h"

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/BreakpointImport.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/InstructionEffect.h"

#include "Debugger/DebugSession.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Debugger/Source/SourcePathList.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/EffectiveAddress.h"
#include "Debugger/SymbolDescriptions.h"
#include "Debugger/Handlers/MemoryHandlers.h"
#include "Debugger/Handlers/TraceHandlers.h"
#include "Debugger/TraceLookahead.h"





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
    static constexpr uint32_t  kPaneDumpBytes = 64;
    DebuggerViewSnapshot       snapshot;
    Reply                 registers   = session.ExecuteViewLine ("R",     CommandMode::AppleWin);
    Reply                 breakpoints = session.ExecuteViewLine ("BPL",   CommandMode::AppleWin);
    Reply                 stack       = session.ExecuteViewLine ("STACK", CommandMode::AppleWin);
    Reply                 watches     = session.ExecuteViewLine ("WL",    CommandMode::AppleWin);
    Reply                 calls       = session.ExecuteViewLine ("CALLS", CommandMode::AppleWin);
    MemoryData            memory;



    snapshot.isPaused       = isPaused;
    snapshot.isAssembling   = session.IsAssembling();
    snapshot.mode           = session.GetMode();
    snapshot.goTo           = m_goTo;
    snapshot.showPane       = m_showPane;
    snapshot.showPaneSerial = m_showPaneSerial;
    snapshot.suggestion       = m_suggestion;
    snapshot.suggestionSerial = m_suggestionSerial;
    snapshot.canUndoBreakpoints = m_breakpointHistory.CanUndo();
    snapshot.canRedoBreakpoints = m_breakpointHistory.CanRedo();
    snapshot.machine        = session.GetTarget().GetMachineInfo().name;

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
            SymbolTableId  table = SymbolTableId::Main;



            snapshot.breakpoints.push_back ({ info.id, info.address,
                                              std::format ("#{} ${:04X}{}", info.id, info.address, info.enabled ? "" : " (off)"),
                                              info.enabled, info });

            session.GetSymbols().TryFindName (info.address, snapshot.breakpoints.back().label, table);
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

    //  The rows D would show, read without running D, so a bare D typed later
    //  still continues from the user's own last dump.
    memory = MemoryHandlers::MakeRows (session.GetTarget(), m_memoryAddress, (Word) std::min<uint32_t> ((uint32_t) m_memoryAddress + kPaneDumpBytes - 1, 0xFFFF));

    for (const MemoryRow & row : memory.rows)
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
                                          entry.value.has_value() ? std::format ("{:04X}", *entry.value) : std::string ("--"),
                                          entry.enabled });
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
    Reply                     code   = session.ExecuteViewLine (std::format ("U {:04X}", now.pc), CommandMode::AppleWin);
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
//  DebuggerViewState::GetRegisterByte
//
//  The byte a register item stands for: A, X, Y, the status register P, or
//  the stack pointer S.
//
////////////////////////////////////////////////////////////////////////////////

Byte DebuggerViewState::GetRegisterByte (const std::string & name, const Cpu6502Registers & registers)
{
    if (name == "A") { return registers.a; }
    if (name == "X") { return registers.x; }
    if (name == "Y") { return registers.y; }
    if (name == "P") { return registers.p; }

    return registers.sp;
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
            Byte  held = GetRegisterByte (item.name, now);

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
    Reply     reply = session.ExecuteViewLine (GetHistoryLine (first, kTraceRows), CommandMode::AppleWin);



    if (TraceData * data = std::get_if<TraceData> (&reply.data))
    {
        snapshot.trace.isOn    = data->isOn;
        snapshot.trace.total   = data->total;
        snapshot.trace.first   = first;
        snapshot.trace.entries = std::move (data->entries);
    }

    if (snapshot.isPaused)
    {
        BuildTraceNext (session, snapshot);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildTraceNext
//
//  The instructions the stopped machine runs next, described as trace entries
//  are, so the pane shows them in the same columns.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildTraceNext (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    IDebugTarget                          & target    = session.GetTarget();
    Cpu6502Registers                        registers = target.GetRegisters();
    std::vector<DisassembledInstruction>    next;



    next = TraceLookahead::FindNext (target.GetInstructionSet(), registers.pc, registers.p,
                                     [&target] (Word address, Byte & value) { return target.TryPeek (address, value); },
                                     kTraceNextRows);

    for (const DisassembledInstruction & instruction : next)
    {
        TraceRecord  record;



        record.pc     = instruction.address;
        record.opcode = instruction.bytes.empty()    ? (Byte) 0 : instruction.bytes[0];
        record.op1    = instruction.bytes.size() > 1 ? instruction.bytes[1] : (Byte) 0;
        record.op2    = instruction.bytes.size() > 2 ? instruction.bytes[2] : (Byte) 0;
        snapshot.trace.next.push_back (record);
    }

    TraceHandlers::Describe (session, snapshot.trace.next);
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
//  addresses, which is rebuilt only when another debug file is loaded or the
//  same one is loaded at another offset.
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

    //  The same file loaded at another offset has the same text, so the key
    //  holds where each segment was placed too.
    key = session.GetDebugFileKey() + SourcePathList::WideToUtf8 (session.GetDebugFilePath());

    for (const DebugSegment & segment : file.segments)
    {
        key += std::format (":{:X}", segment.start);
    }

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

    //  Each line's bytes, from the first range it produced: up to eight, and
    //  none read from a soft switch, which a read would operate.
    if (snapshot.isPaused)
    {
        auto  bytes = std::make_shared<std::map<std::pair<int, int>, std::string>>();

        for (const auto & [place, first] : *m_lineAddresses)
        {
            std::string  text;
            Word         last  = first;

            for (const std::pair<Word, Word> & range : table.GetRanges (place.first, place.second))
            {
                last = (range.first == first) ? range.second : last;
            }

            for (int i = 0; i <= (int) (last - first) && i < kMaxLineBytes; i++)
            {
                Byte  value   = 0;
                Word  address = (Word) (first + i);

                if (session.GetTarget().GetRegion (address) == MemoryRegion::Io || !session.TryPeek (address, value))
                {
                    break;
                }

                text += std::format ("{}{:02X}", text.empty() ? "" : " ", value);
            }

            if (!text.empty() && (int) (last - first) >= kMaxLineBytes)
            {
                text += " ...";
            }

            if (!text.empty())
            {
                (*bytes)[place] = std::move (text);
            }
        }

        if (m_lineBytes == nullptr || *m_lineBytes != *bytes)
        {
            m_lineBytes = bytes;
        }
    }

    state.lineBytes = m_lineBytes;

    if (!atPc.empty())
    {
        state.fileId     = atPc.front().file;
        state.line       = atPc.front().line;
        state.bodyFileId = atPc.back().file;
        state.bodyLine   = atPc.back().line;
        state.depth      = atPc.back().depth;

        for (const SourcePosition & position : atPc)
        {
            state.places.push_back ({ position.file, position.line });
        }
    }

    //  Each line's operand in the files the PC is in, read as the disassembly
    //  pane reads its rows' operands, and what the line at PC leaves. Only
    //  those files, since each line runs its instruction through the core.
    if (snapshot.isPaused)
    {
        auto                            operands = std::make_shared<DebuggerViewSnapshot::LineOperands>();
        const Cpu6502Registers        & now      = session.GetTarget().GetRegisters();
        std::optional<DisassemblyLine>  atPcLine = GetInstructionAt (session, snapshot.pc);
        InstructionTouches::Result      touches;

        for (const auto & [place, first] : *m_lineAddresses)
        {
            std::optional<DisassemblyLine>  line;
            std::string                     effect;

            if (place.first != state.fileId && place.first != state.bodyFileId)
            {
                continue;
            }

            line = GetInstructionAt (session, first);

            if (!line.has_value())
            {
                continue;
            }

            touches = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(), now,
                                                first, (Word) line->instruction.bytes.size());

            if (first == snapshot.pc)
            {
                effect = GetEffect (session, now, touches, (Word) (first + line->instruction.bytes.size()));
            }

            (*operands)[place] = { GetAnnotation (session, *line, now, touches), effect };
        }

        if (m_lineOperands == nullptr || *m_lineOperands != *operands)
        {
            m_lineOperands = operands;
        }

        if (atPcLine.has_value() && atPcLine->instruction.hasTarget)
        {
            touches = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(), now,
                                                snapshot.pc, (Word) atPcLine->instruction.bytes.size());

            state.pcTarget        = atPcLine->instruction.target;
            state.isPcTargetTaken = !touches.isKnown || touches.after.pc == atPcLine->instruction.target;
        }
    }

    state.lineOperands = snapshot.isPaused ? m_lineOperands : nullptr;

    //  Every view's rows, as the window paints them, and the copy in `code`.
    for (int view = -1; view < kMaxCodeViews; view++)
    {
        for (DebuggerViewSnapshot::CodeLine & row : view < 0 ? snapshot.code : snapshot.codeViews[(size_t) view])
        {
            const std::vector<SourcePosition> & at = table.GetPositionsAt (row.address);

            if (!at.empty())
            {
                row.sourceFileId = at.front().file;
                row.sourceLine   = at.front().line;
            }
        }
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (bp.info.kind != BreakpointKind::Address)
        {
            continue;
        }

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
//  The rows D would show, read without running D, so the user's bare D still
//  continues from their own last dump; each row's region is given to every
//  byte in it, which is exact because rows start on eight-byte boundaries and
//  no region changes inside one.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot::MemoryWindow DebuggerViewState::ReadMemoryWindow (DebugSession & session, int id, Word address)
{
    DebuggerViewSnapshot::MemoryWindow  window;
    Word                                first = (Word) (address & ~(kMemoryRowBytes - 1));
    Word                                last  = (Word) std::min<uint32_t> ((uint32_t) first + kMemoryWindowBytes - 1, 0xFFFF);
    MemoryData                          data  = MemoryHandlers::MakeRows (session.GetTarget(), first, last);



    window.id    = id;
    window.first = first;

    for (const MemoryRow & row : data.rows)
    {
        window.bytes.insert (window.bytes.end(), row.bytes.begin(), row.bytes.end());
        window.regions.insert (window.regions.end(), row.bytes.size(), row.region);
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
//  own words for most of what they send. WinDbg has a word for each. Run to
//  cursor does not come through here, since it runs in Casso mode.
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

    //  The deposit takes hex bytes only; any other value stays an AppleWin
    //  MEB, which GSSquared mode runs too.
    if (name == "MEB" && split != std::string::npos && IsHexBytes (rest.substr (split)))
    {
        return rest.substr (0, split) + ":" + rest.substr (split);
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::IsHexBytes
//
//  True when text is one or more values of one or two hex digits each.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::IsHexBytes (const std::string & text)
{
    static constexpr size_t  kMaxDigits = 2;
    std::istringstream       values (text);
    std::string              value;
    bool                     any = false;



    while (values >> value)
    {
        if (value.size() > kMaxDigits || value.find_first_not_of ("0123456789ABCDEFabcdef") != std::string::npos)
        {
            return false;
        }

        any = true;
    }

    return any;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetSourceStepLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetSourceStepLine (bool isSource, CommandMode mode)
{
    return GetModeLine (isSource ? "SRC ON" : "SRC OFF", mode);
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
//  DebuggerViewState::DoesAssemblerKeepKey
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::DoesAssemblerKeepKey (const DebuggerViewSnapshot * snapshot, WPARAM vk, bool ctrl, bool alt)
{
    return snapshot != nullptr && snapshot->isAssembling && !ctrl && !alt && (vk == VK_RETURN || vk == VK_SPACE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::DoesConsoleKeepKey
//
//  The Monitor reads an empty line as a request for the next row of bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::DoesConsoleKeepKey (CommandMode mode, WPARAM vk, bool ctrl, bool alt)
{
    return mode == CommandMode::Monitor && vk == VK_RETURN && !ctrl && !alt;
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
        if (IsCodeBreakpointAt (bp, address))
        {
            return std::format ("BPC {}", bp.id);
        }
    }

    return std::format ("BP {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::IsCodeBreakpointAt
//
//  An execution breakpoint listed at the address. Watchpoints and the entries
//  that stop on an opcode, a register, BRK or an interrupt list an address
//  too, but none of them stops before the instruction there.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::IsCodeBreakpointAt (const DebuggerViewSnapshot::BreakpointLine & bp, Word address)
{
    return bp.info.kind == BreakpointKind::Address && bp.address == address;
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
//  DebuggerViewState::GetWatchEditActions
//
//  Each edit runs as the command anyone could have typed, and is echoed as
//  that command in the console:
//
//    manual watch, expression   the watch moved to the new address
//    manual watch, value        the word written to the watched address
//    automatic register         R <register> <value>
//    automatic flag             R P with that one bit set or cleared
//    automatic address          the byte written there
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerAction> DebuggerViewState::GetWatchEditActions (const DebuggerViewSnapshot & snapshot,
                                                                   std::optional<int> watchId, std::optional<int> autoIndex,
                                                                   int column, const std::string & typed, CommandMode mode)
{
    static constexpr std::pair<char, Byte>  kFlagBits[] =
    {
        { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
    };
    std::string  text    = typed;
    std::string  key;
    Word         address = 0;



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
                return (column == 0) ? std::vector<DebuggerAction> { DebuggerActions::GetClearWatch (watch.id, mode), DebuggerActions::GetAddWatch (text, mode) }
                                     : std::vector<DebuggerAction> { DebuggerActions::GetEnterWord (watch.address, text, mode) };
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
        return { DebuggerActions::GetSetRegister (key.substr (2), text, mode) };
    }

    if (key.starts_with ("M:") && TryParseHexWord (key.substr (2), address))
    {
        return { DebuggerActions::GetEnterByte (address, text, mode) };
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
                    return { DebuggerActions::GetSetRegister ("P", (Word) ((text == "1") ? (Byte) (p | bit) : (Byte) (p & ~bit)), mode) };
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
                                                                            std::optional<int> autoIndex, int column,
                                                                            CommandMode mode)
{
    WatchUndo  undo;
    Word       value   = 0;
    Word       address = 0;



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

            if (!TryParseHexWord (watch.value, value))
            {
                return std::nullopt;
            }

            undo.actions = { DebuggerActions::GetEnterWord (watch.address, value, mode) };
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

    if (line.key.starts_with ("F:"))
    {
        for (const DebuggerViewSnapshot::RegisterRow & reg : before.registers)
        {
            if (reg.name == "P" && TryParseHexWord (reg.value, value))
            {
                undo.actions = { DebuggerActions::GetSetRegister ("P", value, mode) };
                return undo;
            }
        }

        return std::nullopt;
    }

    if (!TryParseHexWord (line.value, value))
    {
        return std::nullopt;
    }

    if (line.key.starts_with ("R:"))
    {
        undo.actions = { DebuggerActions::GetSetRegister (line.key.substr (2), value, mode) };
    }
    else if (line.key.starts_with ("M:") && TryParseHexWord (line.key.substr (2), address))
    {
        undo.actions = { DebuggerActions::GetEnterByte (address, (Byte) value, mode) };
    }

    return undo.actions.empty() ? std::nullopt : std::optional<WatchUndo> (undo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::TryParseHexWord
//
//  A value as the panes show it: hex digits and nothing else. A value the
//  pane could not read, such as a soft switch's "--", is not one.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::TryParseHexWord (const std::string & text, Word & value)
{
    static constexpr unsigned  kMaxWord = 0xFFFF;
    unsigned                   parsed   = 0;
    auto                       result   = std::from_chars (text.data(), text.data() + text.size(), parsed, 16);



    if (text.empty() || result.ec != std::errc() || result.ptr != text.data() + text.size() || parsed > kMaxWord)
    {
        return false;
    }

    value = (Word) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::NoteMovedWatch
//
//  The engine numbers watches in order and never reuses a number, so the
//  watch a move made is the lowest id the snapshot holds that was not there
//  before. It is noted from the first snapshot that shows it, so a watch
//  removed and another added later is never taken for it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::NoteMovedWatch (const DebuggerViewSnapshot & now, WatchUndo & undo)
{
    if (!undo.restoreAddress.has_value() || undo.movedToId.has_value())
    {
        return;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : now.watches)
    {
        bool  existed = std::find (undo.movedFromIds.begin(), undo.movedFromIds.end(), watch.id) != undo.movedFromIds.end();

        if (!existed && (!undo.movedToId.has_value() || watch.id < *undo.movedToId))
        {
            undo.movedToId = watch.id;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetWatchUndoActions
//
//  Nothing yet when the snapshot does not show the watch a move made, so the
//  undo is kept for a later try. A moved watch that has since been removed
//  has nothing left to put back.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<DebuggerAction>> DebuggerViewState::GetWatchUndoActions (const DebuggerViewSnapshot & now, WatchUndo & undo, CommandMode mode)
{
    if (!undo.restoreAddress.has_value())
    {
        return undo.actions;
    }

    NoteMovedWatch (now, undo);

    if (!undo.movedToId.has_value())
    {
        return std::nullopt;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : now.watches)
    {
        if (watch.id == *undo.movedToId)
        {
            return std::vector<DebuggerAction> { DebuggerActions::GetClearWatch (watch.id, mode), DebuggerActions::GetAddWatch (*undo.restoreAddress, mode) };
        }
    }

    return std::vector<DebuggerAction> {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::FormatOpenViews
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::FormatOpenViews (const DebuggerViewSnapshot & snapshot)
{
    std::string  text;



    for (int view = 1; view < kMaxCodeViews; view++)
    {
        const std::vector<DebuggerViewSnapshot::CodeLine> & lines = snapshot.codeViews[(size_t) view];

        //  The TOP line, which the view reopens on exactly. The middle moves
        //  whenever the pane's height does, so saving it would drift.
        if (snapshot.codeOpen[(size_t) view] && !lines.empty())
        {
            text += std::format (" code{}={:04X}", view + 1, lines.front().address);
        }
    }

    if (snapshot.followView != 0)
    {
        text += std::format (" follow={}", snapshot.followView + 1);
    }

    for (const DebuggerViewSnapshot::MemoryWindow & window : snapshot.memoryWindows)
    {
        if (window.id >= 2)
        {
            text += std::format (" memory{}={:04X}", window.id, window.first);
        }
    }

    for (const DebuggerViewSnapshot::PanelInfo & panel : snapshot.panels)
    {
        if (panel.open)
        {
            text += " panel=" + panel.id;
        }
    }

    return text.empty() ? text : text.substr (1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ParseOpenViews
//
//  Anything this build does not recognize is passed over rather than
//  failing the rest, so a view a later build adds does not cost the others.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewState::OpenViews DebuggerViewState::ParseOpenViews (const std::string & text)
{
    OpenViews           views;
    std::istringstream  in (text);
    std::string         token;



    while (in >> token)
    {
        size_t       equals = token.find ('=');
        std::string  name   = token.substr (0, equals);
        std::string  value  = (equals == std::string::npos) ? std::string() : token.substr (equals + 1);
        unsigned     number = 0;
        bool         isHex  = !value.empty() &&
                              std::from_chars (value.data(), value.data() + value.size(), number, 16).ptr == value.data() + value.size();

        if (name == "panel" && !value.empty())
        {
            views.panels.push_back (value);
        }
        else if (name == "follow" && isHex && number >= 1 && number <= (unsigned) kMaxCodeViews)
        {
            views.follow = (int) number - 1;
        }
        else if (name.size() == 5 && name.starts_with ("code") && isHex && number <= 0xFFFF)
        {
            int  view = name[4] - '1';

            if (view >= 1 && view < kMaxCodeViews)
            {
                views.code[(size_t) view] = (Word) number;
            }
        }
        else if (name.size() == 7 && name.starts_with ("memory") && isHex && number <= 0xFFFF)
        {
            int  window = name[6] - '0';

            if (window >= 2 && window <= kMaxMemoryWindows)
            {
                views.memory[(size_t) (window - 1)] = (Word) number;
            }
        }
    }

    return views;
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
//  DebuggerViewState::ExecuteConsoleLine
//
//  The echo takes the prompt of the mode the line was typed in, as batch
//  writes it, not the mode a MODE line leaves in force. A line sent with its
//  own mode runs in that mode alone and leaves the session's as it was.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewState::ExecuteConsoleLine (
    DebugSession                & session,
    const std::string           & line,
    std::optional<CommandMode>    lineMode)
{
    CommandMode               mode  = lineMode.value_or (session.GetMode());
    std::vector<std::string>  lines;
    Reply                     reply;



    reply = ExecuteWindowLine (session, line, mode);

    m_suggestion = reply.suggestion;
    ++m_suggestionSerial;

    lines.push_back (DebugSession::GetPrompt (mode) + line);
    lines.insert (lines.end(), reply.text.begin(), reply.text.end());

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecuteAction
//
//  PANEL is the window's own, so it runs on the panes; anything else goes to
//  the session. A script the command starts runs its lines back through this
//  window, as one a typed line starts does.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewState::ExecuteAction (DebugSession & session, const DebuggerAction & action)
{
    CommandMode               mode = action.echoMode.value_or (session.GetMode());
    std::vector<std::string>  lines;
    Reply                     reply;



    if (action.breakpointStep.has_value())
    {
        return ExecuteBreakpointStep (session, *action.breakpointStep);
    }

    reply = ExecuteActionCommand (session, action);

    reply.mode = mode;
    session.FormatReply (reply, mode);

    lines.push_back (DebugSession::GetPrompt (mode) + action.echo);
    lines.insert (lines.end(), reply.text.begin(), reply.text.end());

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecuteActionCommand
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewState::ExecuteActionCommand (DebugSession & session, const DebuggerAction & action)
{
    DebugSession::ScriptLineRunner  previous = session.GetScriptLineRunner();
    DebugCommand                    command;
    std::string                     error;
    Reply                           reply;



    if (!TryResolveCommand (session, action, command, error))
    {
        reply.command = action.command.sourceName;
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return reply;
    }

    if (command.verb == DebugVerb::ListPanels || command.verb == DebugVerb::OpenPanel || command.verb == DebugVerb::ClosePanel)
    {
        reply.command = command.sourceName;
        RunPanelCommand (session, command, reply);
        return reply;
    }

    session.SetScriptLineRunner ([this, &session] (const std::string & scriptLine, CommandMode scriptMode)
    {
        return ExecuteWindowLine (session, scriptLine, scriptMode);
    });

    reply = session.Execute (command);
    session.SetScriptLineRunner (std::move (previous));
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::TryResolveCommand
//
//  A definition is parsed as the AppleWin line it is. What the user typed is
//  evaluated in the session's mode: one address or register value, or the
//  values to write, each split at spaces as a typed deposit's are, a byte
//  where it fits and a word, low byte first, where it does not or where the
//  command writes words.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::TryResolveCommand (DebugSession & session, const DebuggerAction & action, DebugCommand & command, std::string & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    AppleWinParseResult    parsed;
    std::istringstream     tokens (action.operand);
    std::string            token;
    Word                   value    = 0;
    bool                   isWord   = false;



    command = action.command;

    if (!action.definition.empty())
    {
        parsed = AppleWinParser::Parse (action.definition, session);

        if (parsed.status != ParseStatus::Ok)
        {
            error = parsed.error.empty() ? std::format ("{} is not a breakpoint definition.", action.definition) : parsed.error;
            return false;
        }

        command = parsed.command;
        return true;
    }

    if (action.operand.empty())
    {
        return true;
    }

    if (command.verb != DebugVerb::EnterBytes && command.verb != DebugVerb::EnterWords)
    {
        return AppleWinParser::TryEvaluate (action.operand, session, command.a1, error);
    }

    command.values.clear();
    command.mask.clear();

    while (tokens >> token)
    {
        if (!AppleWinParser::TryEvaluate (token, session, value, error))
        {
            return false;
        }

        isWord = command.verb == DebugVerb::EnterWords || value > kMaxByte;

        command.values.push_back ((Byte) value);
        command.mask.push_back ((Byte) kMaxByte);

        if (isWord)
        {
            command.values.push_back ((Byte) (value >> 8));
            command.mask.push_back ((Byte) kMaxByte);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecuteBreakpointStep
//
//  Each action runs directly and is echoed in its mode. An undo or redo runs
//  no command and gives one line. An import's lines are the file's text, so
//  they alone are parsed, in AppleWin's words.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewState::ExecuteBreakpointStep (DebugSession & session, const BreakpointStep & step)
{
    std::vector<std::string>         lines;
    BreakpointHistory::ActionRunner  act;
    BreakpointHistory::LineRunner    run;
    std::string                      summary;



    act = [this, &session, &lines] (const DebuggerAction & action)
    {
        CommandMode  mode  = action.echoMode.value_or (session.GetMode());
        Reply        reply = ExecuteActionCommand (session, action);

        reply.mode = mode;
        session.FormatReply (reply, mode);

        lines.push_back (DebugSession::GetPrompt (mode) + action.echo);
        lines.insert (lines.end(), reply.text.begin(), reply.text.end());
        return reply;
    };

    run = [this, &session, &lines] (const std::string & line)
    {
        Reply  reply = ExecuteSessionLine (session, line, CommandMode::AppleWin);

        lines.push_back (DebugSession::GetPrompt (CommandMode::AppleWin) + line);
        lines.insert (lines.end(), reply.text.begin(), reply.text.end());
        return reply;
    };

    switch (step.kind)
    {
    case BreakpointStep::Kind::Undo:
        if (m_breakpointHistory.TryUndo (session, summary))
        {
            lines.push_back (summary);
        }

        break;

    case BreakpointStep::Kind::Redo:
        if (m_breakpointHistory.TryRedo (session, summary))
        {
            lines.push_back (summary);
        }

        break;

    case BreakpointStep::Kind::Import:
        m_breakpointHistory.Record (session, [this, &session, &step, &run, &lines] { ImportBreakpoints (session, step.path, run, lines); });
        break;

    default:
        m_breakpointHistory.Record (session, [&step, &act]
        {
            for (const DebuggerAction & action : step.actions)
            {
                act (action);
            }
        });
        break;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ImportBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::ImportBreakpoints (
    DebugSession                         & session,
    const std::string                    & path,
    const BreakpointHistory::LineRunner  & run,
    std::vector<std::string>             & lines)
{
    IFileSystem  * files   = session.GetFileSystem();
    std::string    script;
    int            skipped = 0;
    HRESULT        hr      = S_OK;



    CBRF (files != nullptr, lines.push_back ("This session cannot read or write host files."));

    hr = files->ReadAllText (session.ResolvePath (path), script);
    CHRF (hr, lines.push_back (std::format ("{} could not be read.", path)));

    skipped = BreakpointImport::Run (session, script, run);
    lines.push_back (std::format ("Imported the breakpoints{}; skipped {} line{} that {} not breakpoints.",
                                  session.GetPathEcho (path, "from"), skipped, skipped == 1 ? "" : "s", skipped == 1 ? "is" : "are"));

Error:
    return;
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
    GSSquaredParseResult     parsed;
    DebugVerb                verb     = DebugVerb::None;



    //  GSSquared and WinDbg have no layout commands, and their words are not
    //  AppleWin's. PANEL, which they reach the way they reach any Casso
    //  command -- by its bare name, or after WinDbg's `!` -- is still the
    //  window's, or the panels saved with the window could not reopen in
    //  those modes.
    if (mode == CommandMode::GSSquared || mode == CommandMode::WinDbg)
    {
        //  debug and nodebug are GSSquared's PANEL, which the window runs.
        if (mode == CommandMode::GSSquared && !session.IsAssembling())
        {
            parsed = GSSquaredParser::Parse (line, session);
            verb   = parsed.commands.size() == 1 ? parsed.commands.front().verb : DebugVerb::None;

            if (verb == DebugVerb::ListPanels || verb == DebugVerb::OpenPanel || verb == DebugVerb::ClosePanel)
            {
                reply.command = line;
                RunPanelCommand (session, parsed.commands.front(), reply);
                session.FormatReply (reply, mode);
                return reply;
            }
        }

        if (mode == CommandMode::WinDbg)
        {
            if (first == std::string::npos || line[first] != '!')
            {
                return ExecuteSessionLine (session, line, mode);
            }

            text = line.substr (first + 1);
        }

        stream.str (text);
        stream >> name;

        entry = (name.empty() || session.IsAssembling()) ? nullptr : AppleWinCommandTable::Find (name);

        if (entry != nullptr && entry->verb == DebugVerb::ListPanels)
        {
            return ExecutePanelLine (session, text, line, mode);
        }

        return ExecuteSessionLine (session, line, mode);
    }

    //  In Monitor mode only a `/` line is an AppleWin line.
    if (mode == CommandMode::Monitor)
    {
        if (first == std::string::npos || line[first] != '/')
        {
            return ExecuteSessionLine (session, line, mode);
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
        return ExecuteSessionLine (session, line, mode);
    }

    reply.command = line;

    switch (entry->family)
    {
    case AppleWinCommandFamily::Cursor:
        MoveCodePane (session, entry->name, reply);
        break;

    case AppleWinCommandFamily::MiniMemory:
        MoveMemoryPane (session, entry->name, argument, reply);
        break;

    case AppleWinCommandFamily::Window:
        ShowWindowPane (session, entry->name, reply);
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
//  DebuggerViewState::ExecuteSessionLine
//
//  A line the session runs. A script it starts runs each of its lines back
//  through this window, in the mode its command gives, so a script opens
//  panels and moves panes as the same lines typed would.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewState::ExecuteSessionLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    DebugSession::ScriptLineRunner  previous = session.GetScriptLineRunner();
    Reply                           reply;



    session.SetScriptLineRunner ([this, &session] (const std::string & scriptLine, CommandMode scriptMode)
    {
        return ExecuteWindowLine (session, scriptLine, scriptMode);
    });

    reply = ExecuteLine (session, line, mode);
    session.SetScriptLineRunner (std::move (previous));
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ShowWindowPane
//
//  AppleWin's window commands switch its one screen between views. Here every
//  view is a pane, so they bring one forward: CODE and CODE1 the first
//  disassembly, CODE2 the second, DATA and DATA1 the first memory window,
//  DATA2 the second, CONSOLE the console. A second view that is not open
//  opens.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::ShowWindowPane (DebugSession & session, const std::string & name, Reply & reply)
{
    if (name.starts_with ("SOURCE"))
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available", "SOURCE needs a link to an assembler listing.");
        return;
    }

    if (name == "CONSOLE")
    {
        m_showPane = DebuggerLayout::kConsole;
    }
    else if (name == "CODE" || name == "CODE1")
    {
        m_showPane = DebuggerLayout::GetCodePaneId (0);
    }
    else if (name == "CODE2")
    {
        if (!IsCodeViewOpen (1))
        {
            OpenCodeView (1, session.GetTarget().GetRegisters().pc);
        }

        m_showPane = DebuggerLayout::GetCodePaneId (1);
    }
    else if (name == "DATA" || name == "DATA1")
    {
        m_showPane = DebuggerLayout::GetMemoryPaneId (1);
    }
    else if (name == "DATA2")
    {
        if (!GetMemoryWindowAddress (2).has_value())
        {
            GoToMemory (2, m_memoryAddress);
        }

        m_showPane = DebuggerLayout::GetMemoryPaneId (2);
    }
    else
    {
        reply.data = MessageData { { "This window shows every pane at once." } };
        return;
    }

    ++m_showPaneSerial;
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
                        std::format ("This machine has no {} panel. Use PANEL LIST to list the ones it has.", command.text));
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
    std::vector<DisassemblyLine>                   listed;
    Reply                                          code;
    uint32_t                                       next      = 0;
    Word                                           codeEnd   = 0;



    next = ChooseCodeStart (session, snapshot.pc, view);

    //  Ranges, so the count is ours rather than the command's default: the
    //  pane holds as many lines as it has room for. Three bytes a line covers
    //  the longest instruction, but a data block's line is longer, so the
    //  listing goes on from where the last range ended until the pane is
    //  full. A range stops at $FFFF: one that ran past it wrapped to below
    //  its own start and listed nothing, which emptied any view within a
    //  screenful of the vectors.
    while ((int) listed.size() < v.lines && next <= 0xFFFF)
    {
        const DisassemblyData * data = nullptr;



        codeEnd = (Word) (std::min) (0xFFFFu, next + (uint32_t) (v.lines - (int) listed.size()) * 3);
        code    = session.ExecuteViewLine (std::format ("U {:04X}:{:04X}", next, codeEnd), CommandMode::AppleWin);
        data    = std::get_if<DisassemblyData> (&code.data);

        if (data == nullptr || data->lines.empty())
        {
            break;
        }

        listed.insert (listed.end(), data->lines.begin(), data->lines.end());
        next = (uint32_t) data->lines.back().instruction.address + (uint32_t) (std::max) ((size_t) 1, data->lines.back().instruction.bytes.size());
    }

    v.shown.clear();

    for (const DisassemblyLine & line : listed)
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

            if (row.isCurrent && row.target.has_value() && touches.isKnown)
            {
                row.isTargetTaken = touches.after.pc == *row.target;
            }
        }

        if (line.instruction.hasOperandAddress || line.instruction.operand.starts_with ("("))
        {
            row.memoryOperand = line.instruction.operand;
            row.shownOperand  = line.GetShownOperand();
        }

        for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
        {
            if (IsCodeBreakpointAt (bp, row.address))
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
//  DebuggerViewState::OpenCodeViewAt
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::OpenCodeViewAt (int view, Word top)
{
    if (view < 1 || view >= kMaxCodeViews)
    {
        return;
    }

    m_code[(size_t) view].open = true;
    SetCodeAddress (top, view);
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

        v.address     = ScrollCodeTop (session, top, v.lines, v.scrollLines);
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
//  DebuggerViewState::ScrollCodeTop
//
//  The top line of a pane of `lines` lines moved `count` instructions from
//  `top`, down when positive. Down goes no further than the page whose last
//  line holds $FFFF, as up stops at $0000, so the pane always fills.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::ScrollCodeTop (DebugSession & session, Word top, int lines, int count)
{
    Word            from  = top;
    Word            last  = 0;
    int             shown = 1;
    DataBlockEntry  block;



    for (int i = 0; i < count && top < 0xFFFF; i++)
    {
        Word  next = (Word) (top + GetInstructionLength (session, top));

        top = (next > top) ? next : top;
    }

    if (count > 0)
    {
        last = top;

        while (shown < lines)
        {
            Word  next = (Word) (last + GetInstructionLength (session, last));

            if (next <= last)
            {
                break;
            }

            last = next;
            shown++;
        }

        if (shown < lines)
        {
            top = FindStartAbove (session, last, lines - 1);
        }
    }

    //  Up: a data block's lines are known, so those step back one line at a
    //  time. Above code, the alignment that lands on the top line, from as
    //  many lines above as can be found; a byte at a time where none can.
    if (count < 0)
    {
        while (count < 0 && top > 0 && session.GetDataBlocks().TryFindAt ((Word) (top - 1), block))
        {
            top = GetPreviousInstruction (session, top);
            count++;
        }

        from = top;

        for (int above = -count; above > 0 && top == from; above--)
        {
            top = FindStartAbove (session, from, above);
        }

        if (top == from && from > 0 && count < 0)
        {
            top = (Word) (from - (Word) (std::min) ((int) from, -count));
        }
    }

    return top;
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
    reply = session.ExecuteViewLine (std::format ("U {:04X}:{:04X}", first, pc), CommandMode::AppleWin);

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
//  RET and -> bring an address to its middle line as any navigation does,
//  and the rest move its top line. A following pane moves from the lines it
//  shows, which have the PC in their middle, not from the PC.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::MoveCodePane (DebugSession & session, const std::string & name, Reply & reply)
{
    CodeView             & v      = m_code[(size_t) m_follow];
    Word                   pc     = session.GetTarget().GetRegisters().pc;
    Word                   start  = v.address.value_or (v.shown.empty() ? pc : v.shown.front());
    std::optional<Word>    target = start;
    int                    count  = 0;



    if (name == ".")
    {
        //  Through SetCodeAddress, so a scroll or navigation queued ahead of
        //  the line is dropped and does not land on top of it, then centered
        //  on the PC as the Follow PC control does.
        SetCodeAddress (std::nullopt, m_follow);
        ShowPcIn (m_follow);
        reply.data = MessageData { { "The code pane follows the PC." } };
        return;
    }

    if (name == "RET" || name == "->")
    {
        //  -> reads the pinned top line's operand, or the PC's.
        start  = v.address.value_or (pc);
        target = (name == "RET") ? GetReturnAddress (session) : GetOperandAddress (session, start);

        if (!target.has_value())
        {
            reply.SetError (CommandStatus::Error, "no address",
                            (name == "RET") ? std::string ("The stack cannot be read, so there is no return address.")
                                            : std::format ("The instruction at ${:04X} has no address operand.", start));
            return;
        }

        CenterCodeOn (*target, m_follow);
        reply.data = MessageData { { std::format ("The code pane shows ${:04X}.", *target) } };
        return;
    }

    if (name == "^" || name == "V" || name == "PAGEUP" || name == "PAGEDN")
    {
        count   = (name == "^" || name == "V") ? 1 : v.lines;
        count   = (name == "^" || name == "PAGEUP") ? -count : count;
        *target = ScrollCodeTop (session, start, v.lines, count);
    }
    else if (name == "PAGEUP256")   { *target = (Word) (start - 0x0100); }
    else if (name == "PAGEUP4K")    { *target = (Word) (start - 0x1000); }
    else if (name == "PAGEDOWN256") { *target = (Word) (start + 0x0100); }
    else if (name == "PAGEDOWN4K")  { *target = (Word) (start + 0x1000); }

    SetCodeAddress (target, m_follow);
    reply.data    = MessageData { { std::format ("The code pane is at ${:04X}.", *target) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::MoveMemoryPane
//
//  Each memory window shows bytes and characters together, so MD, MA, MT and
//  M all move one: the 1 forms the first window, the 2 forms the second, as
//  DATA2 numbers it, opening it when it is not open.
//  The address is an expression, as every other AppleWin address is.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::MoveMemoryPane (DebugSession & session, const std::string & name, const std::string & argument, Reply & reply)
{
    int          id      = name.ends_with ('2') ? 2 : 1;
    Word         address = 0;
    std::string  error;



    if (argument.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} needs an address.", name));
        return;
    }

    if (!AppleWinParser::TryEvaluate (argument, session, address, error))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return;
    }

    GoToMemory (id, address);
    reply.data = MessageData { { std::format ("Memory window {} is at ${:04X}.", id, address) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GoToMemory
//
//  A memory pane places its view once and then follows its own scrolling, so
//  a new address alone would be scrolled straight back. It reaches the pane
//  as a Go to, as the Go to box's does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::GoToMemory (int window, Word address)
{
    DebuggerViewSnapshot::GoTo  goTo;



    OpenMemoryWindow (window, address);

    goTo.window  = window;
    goTo.text    = std::format ("{:04X}", address);
    goTo.address = address;
    goTo.serial  = m_goTo.has_value() ? m_goTo->serial + 1 : 1;

    m_goTo = goTo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetInstructionAt
//
//  The instruction that starts at an address, as the disassembly lists it,
//  or none for an I/O address, which a read would operate.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DisassemblyLine> DebuggerViewState::GetInstructionAt (DebugSession & session, Word address)
{
    Reply                   code;
    const DisassemblyData * data = nullptr;



    if (session.GetTarget().GetRegion (address) == MemoryRegion::Io)
    {
        return std::nullopt;
    }

    code = session.ExecuteViewLine (std::format ("U {0:04X}:{0:04X}", address), CommandMode::AppleWin);
    data = std::get_if<DisassemblyData> (&code.data);

    if (data == nullptr || data->lines.empty() || data->lines.front().instruction.address != address)
    {
        return std::nullopt;
    }

    return data->lines.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetInstructionLength
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::GetInstructionLength (DebugSession & session, Word address)
{
    Reply                   code = session.ExecuteViewLine (std::format ("U {:04X}", address), CommandMode::AppleWin);
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
//  byte back when none does. A data block's lines are known: they run from
//  the block's start, each as long as its first.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerViewState::GetPreviousInstruction (DebugSession & session, Word address)
{
    Word            before = (Word) (address - 1);
    DataBlockEntry  block;
    Word            length = 0;



    if (session.GetDataBlocks().TryFindAt (before, block))
    {
        length = GetInstructionLength (session, block.first);
        return (Word) (block.first + (before - block.first) / length * length);
    }

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
    std::string  about;
    bool         isIo  = false;
    Byte         value = 0;



    for (const InstructionTouches::Item & item : touches.items)
    {
        if (!item.isRead)
        {
            continue;
        }

        if (item.kind == InstructionTouches::Kind::Register)
        {
            Byte  held = GetRegisterByte (item.name, registers);


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

        isIo = session.GetTarget().GetRegion (item.address) == MemoryRegion::Io;

        if (isIo)
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

    //  A named location in memory says what it is after its byte, so
    //  INC RNDL reads $004E=96; random seed, low byte .... An I/O
    //  address already said it in place of the byte.
    about = isIo ? std::string() : SymbolDescriptions::GetAction (line.operandSymbol);

    if (!about.empty())
    {
        if (about.size() > 1 && isupper ((unsigned char) about[0]) && islower ((unsigned char) about[1]))
        {
            about[0] = (char) tolower ((unsigned char) about[0]);
        }

        text += (text.empty() ? "" : "; ") + about;
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
    Reply                   code    = session.ExecuteViewLine (std::format ("U {:04X}", address), CommandMode::AppleWin);
    const DisassemblyData * data    = std::get_if<DisassemblyData> (&code.data);
    size_t                  dollar  = std::string::npos;
    unsigned                operand = 0;



    if (data == nullptr || data->lines.empty())
    {
        return std::nullopt;
    }

    if (data->lines[0].instruction.hasTarget)
    {
        return data->lines[0].instruction.target;
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
//  have no one to ask, so there the handler reports the error instead. A line
//  typed while the session is assembling is an instruction, not a command.
//
//  The line is parsed against a scratch state: a range comes from the line
//  itself, and the session's own state must not move for a line that has not
//  run yet.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebugVerb> DebuggerViewState::GetMissingFileVerb (const std::string & line, CommandMode mode, bool isAssembling)
{
    MonitorState        scratch;
    MonitorParseResult  parsed;



    if (mode != CommandMode::Monitor || isAssembling)
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
