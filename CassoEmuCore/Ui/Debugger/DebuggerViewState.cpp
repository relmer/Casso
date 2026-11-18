#include "Pch.h"

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/CodeAnnotations.h"
#include "Ui/Debugger/InstructionLookup.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Source/SourcePathList.h"
#include "Debugger/ReplyJson.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/RegisterHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"
#include "Debugger/Handlers/TraceHandlers.h"
#include "Debugger/TraceLookahead.h"
#include "Core/IParallelRunner.h"
#include "Machines/Apple2/Common/VideoTiming.h"





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
    return IsBuildDue (isDirty, isPaused, wasPaused, true, nowMs, builtAtMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::IsBuildDue
//
//  A snapshot the window has yet to take would only be replaced unseen, so
//  time alone builds no other until it is taken; the window takes one each
//  time it paints, so the machine builds no more than the window shows.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewState::IsBuildDue (bool isDirty, bool isPaused, bool wasPaused, bool isTaken, uint64_t nowMs, uint64_t builtAtMs)
{
    return isDirty                 ||
           isPaused != wasPaused   ||
           (!isPaused && isTaken && nowMs - builtAtMs >= kBuildIntervalMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::Build
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot DebuggerViewState::Build (DebugSession & session, bool isPaused) const
{
    DebuggerViewSnapshot  snapshot = BuildCaptured (session, isPaused);



    BuildLive (session, snapshot, isPaused);
    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildCaptured
//
//  Every read is a direct call on the session and its target, never a
//  command, so the session may be one over a capture on another thread. The
//  panes that only read are built at once on the runner given, or one after
//  another without one.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot DebuggerViewState::BuildCaptured (DebugSession & session, bool isPaused, IParallelRunner * runner) const
{
    static constexpr char         kFlagNames[]   = "NV-BDIZC";
    DebuggerViewSnapshot      snapshot;
    const Cpu6502Registers    r            = session.GetTarget().GetRegisters();
    const VideoPosition       video        = session.GetTarget().GetVideoPosition();
    const DebugMachineInfo    machineInfo  = session.GetTarget().GetMachineInfo();
    const StackData           stack        = RegisterHandlers::MakeStackData (session.GetTarget());
    BreakpointListData        breakpoints;
    DebugDiskState            diskState;
    InlineParallelRunner      inlineRunner;
    IParallelRunner         & run          = (runner != nullptr) ? *runner : inlineRunner;
    const IParallelRunner::Job    jobs[]         = { [this, &session, &snapshot] { m_codeViews.BuildCodeViews       (session, snapshot); },
                                                     [this, &session, &snapshot] { m_memoryWindows.BuildMemoryPanes (session, snapshot); },
                                                     [this, &session, &snapshot] { BuildTrace                       (session, snapshot); } };



    snapshot.isPaused       = isPaused;
    snapshot.isAssembling   = session.IsAssembling();
    snapshot.stopReason     = session.GetLastStopReason();
    snapshot.stopBreakpoint = session.GetLastStopBreakpoint();
    snapshot.runNotice      = session.GetRunNotice();
    snapshot.mode           = session.GetMode();
    m_commands.AddRequests (snapshot);

    snapshot.machine        = machineInfo.name;
    snapshot.ioRegisters    = machineInfo.ioRegisters;
    snapshot.slotCards      = machineInfo.slotCards;
    snapshot.machineId      = machineInfo.machineId;
    snapshot.symbolSources  = DescribeSymbolSources (session.GetSymbols());
    snapshot.heatMapSymbols = m_heatMap.GetHeatMapSymbols (session.GetSymbols());

    snapshot.pc        = r.pc;
    snapshot.cycles    = session.GetTarget().GetCycleCount();
    snapshot.registers = { { "A",  std::format ("{:02X}", r.a) },
                           { "X",  std::format ("{:02X}", r.x) },
                           { "Y",  std::format ("{:02X}", r.y) },
                           { "P",  std::format ("{:02X}", r.p) },
                           { "S",  std::format ("{:02X}", r.sp) },
                           { "PC", std::format ("{:04X}", r.pc) } };

    //  The CPU cycles since power-on, as other emulators' register views
    //  show them; a replay or a seek restores the count with the machine.
    snapshot.registers.push_back ({ std::string (kCyclesRegister), std::format ("{}", session.GetTarget().GetCycleCount()) });
    snapshot.registers.push_back ({ std::string (kTripRegister),   std::format ("{}", session.GetTripCycles (session.GetTarget().GetCycleCount())) });
    snapshot.registers.push_back ({ std::string (kLastBranchRegister), std::string() });

    {
        Word  from = 0;
        Word  to   = 0;

        snapshot.lastBranchFrom = session.GetTarget().TryGetLastBranch (from)     ? std::optional<Word> (from) : std::nullopt;
        snapshot.lastBranchTo   = session.GetTarget().TryGetLastBranchTarget (to) ? std::optional<Word> (to)   : std::nullopt;
    }

    for (int bit = 7; bit >= 0; bit--)
    {
        snapshot.flags += ((r.p >> bit) & 1) ? kFlagNames[7 - bit] : '.';
    }

    snapshot.beam = DebuggerViewSnapshot::BeamState { video.scanline, video.cycleInLine };

    BreakpointHandlers::ListAll (session, breakpoints);
    snapshot.nextBreakpointId = session.GetNextId();

    //  An RWTS or driver breakpoint is listed at the routine it resolved to.
    for (const BreakpointInfo & info : breakpoints.breakpoints)
    {
        SymbolTableId  table   = SymbolTableId::Main;
        Word           address = info.resolved.value_or (info.address);



        snapshot.breakpoints.push_back ({ info.id, address,
                                          std::format ("#{} ${:04X}{}", info.id, address, info.enabled ? "" : " (off)"),
                                          info.enabled, info });

        session.GetSymbols().TryFindName (address, snapshot.breakpoints.back().label, table);
    }

    AddSessionStops (session, snapshot);

    if (session.GetTarget().TryGetDiskState (diskState))
    {
        snapshot.diskSlot = diskState.slot;
    }

    //  Before the code views, which mark the line a step just ran.
    if (isPaused)
    {
        m_autoWatches.UpdateLastStop (session, snapshot);
    }

    snapshot.callStack      = session.GetCallStack();

    //  The line list takes the call stack's JSRs as known starts, so it is
    //  built once the call stack is read and before the code views are.
    m_codeViews.RefreshLineList (session, isPaused, snapshot.callStack);
    snapshot.codeFind = m_codeViews.GetFindLines (isPaused);

    //  The code views, the memory panes and the trace each read the machine
    //  and write only their own part of the snapshot, so they are built at
    //  once. What follows reads the code views and the call record.
    run.RunAll (jobs);

    snapshot.code       = snapshot.codeViews[0];
    snapshot.followView = m_codeViews.GetFollowView();

    for (const StackEntry & entry : stack.entries)
    {
        snapshot.stack.push_back ({ entry.address, entry.value });
    }

    snapshot.instructionSet   = session.GetTarget().GetInstructionSet();
    snapshot.instructionTable = session.GetTarget().GetInstructionTable();

    snapshot.searchHits    = session.GetSearchResults();
    snapshot.searchSerial  = session.GetSearchSerial();
    snapshot.comparison    = session.GetCompareResults();
    snapshot.compareSerial = session.GetCompareSerial();

    BuildWatches (session, snapshot);
    BuildSymbols (session, snapshot);

    if (isPaused)
    {
        m_autoWatches.BuildAutoWatches (session, snapshot);
    }

    BuildSource (session, snapshot);
    snapshot.staleFileNames = session.GetFiles().GetStaleFileNames();

    //  A stopped machine's program is read whatever is on screen: its
    //  variables are in the Watches pane and its frames in the Call stack.
    if (m_isBasicShown || isPaused)
    {
        snapshot.basic = ReadBasic (session, isPaused);
        MarkAbsentBasicLines (snapshot);
    }

    //  A BASIC watch's value is read with the program's, once the machine is
    //  stopped in it.
    for (DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        if (!watch.basicName.empty())
        {
            watch.value = (snapshot.basic.has_value() && snapshot.basic->hasValues) ? WatchHandlers::ReadBasicValue (session, watch.basicName).value_or ("--")
                                                                                    : std::string ("--");
        }
    }

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildWatches
//
//  The manual watches, the zero-page pointers with the bytes each points to,
//  and the bookmarks.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildWatches (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    const WatchListData  watches   = WatchHandlers::MakeList (session, WatchListKind::Watch, false);
    const WatchListData  pointers  = WatchHandlers::MakeList (session, WatchListKind::ZeroPage);
    const WatchListData  bookmarks = WatchHandlers::MakeList (session, WatchListKind::Bookmark);



    for (const WatchEntry & entry : watches.entries)
    {
        std::string  value = entry.valueText.has_value() ? *entry.valueText
                           : entry.value.has_value()     ? std::format ("{:04X}", *entry.value)
                           :                               std::string ("--");

        snapshot.watches.push_back ({ entry.id, entry.address, value, entry.enabled, entry.basicName, entry.expression,
                                      entry.isValueEditable, entry.error, entry.isRegister });
    }

    for (const WatchEntry & entry : pointers.entries)
    {
        constexpr int  kPointedBytes = 4;
        std::string    text          = "--";
        Byte           value         = 0;

        if (entry.value.has_value())
        {
            text = std::format ("${:04X}:", *entry.value);

            for (int i = 0; i < kPointedBytes; i++)
            {
                text += session.GetTarget().TryPeek ((Word) (*entry.value + i), value) ? std::format (" {:02X}", value) : std::string (" ??");
            }
        }

        snapshot.zeroPagePointers.push_back ({ entry.id, entry.address, text, entry.enabled });
    }

    for (const WatchEntry & entry : bookmarks.entries)
    {
        snapshot.bookmarks.push_back ({ entry.id, entry.address, std::string(), entry.enabled });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildSymbols
//
//  The symbol tables and their symbols, read again only when they have
//  changed, then the bytes under the symbols.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildSymbols (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    //  The symbols are read again only when they have changed.
    if (m_symbols == nullptr || m_symbolsRevision != session.GetSymbols().GetRevision())
    {
        auto  state = std::make_shared<DebuggerViewSnapshot::SymbolsState>();



        for (int table = 0; table < SymbolTable::kTableCount; table++)
        {
            auto  load = std::ranges::find_if (session.GetFiles().GetSymbolLoads(), [table] (const DebugSession::SymbolLoad & each) { return each.table == (SymbolTableId) table; });



            state->tables.push_back ({ ReplyJson::GetSymbolTableName ((SymbolTableId) table), session.GetSymbols().GetCount ((SymbolTableId) table),
                                       session.GetSymbols().IsEnabled ((SymbolTableId) table), (SymbolTableId) table,
                                       session.GetSymbols().GetOrigins ((SymbolTableId) table),
                                       (load != session.GetFiles().GetSymbolLoads().end()) ? load->path : std::wstring() });
            session.GetSymbols().GetAll ((SymbolTableId) table, state->symbols);
        }

        m_symbols         = state;
        m_symbolsRevision = session.GetSymbols().GetRevision();
    }

    snapshot.symbols = m_symbols;
    BuildSymbolBytes (session, snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::MarkAbsentBasicLines
//
//  Only once a program is read: with no Applesoft, or no program, the lines
//  there will be are not known.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::MarkAbsentBasicLines (DebuggerViewSnapshot & snapshot)
{
    if (!snapshot.basic.has_value() || !snapshot.basic->isApplesoft || !snapshot.basic->hasProgram)
    {
        return;
    }

    for (DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (bp.info.kind == BreakpointKind::BasicLine)
        {
            bp.isAbsent = std::ranges::none_of (snapshot.basic->lines, [&bp] (const BasicLine & line) { return line.number == bp.info.address; });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ReadBasic
//
//  Every read is a peek. A real is written by the ROM's own FOUT, run on an
//  overlay, which costs a few hundred instructions a value, so the values
//  are read only while the machine is stopped or at the prompt, where they
//  do not change, and only with a program, since with none the variable
//  table's pointers can span most of memory. After a run ends the values
//  stay in memory until RUN, NEW or CLEAR, so they are read at the prompt.
//  While the language card banks RAM over Applesoft, as during ProDOS's
//  calls, Applesoft is found in the ROM image and the program is listed,
//  but no values are read, since FOUT would run whatever the RAM contains.
//  The text pointer is the statement's first token: at a statement's start
//  the loop has moved TXTPTR onto it, and elsewhere TXTPTR is on the byte
//  before it.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot::BasicState DebuggerViewState::ReadBasic (DebugSession & session, bool isPaused)
{
    DebuggerViewSnapshot::BasicState  state;
    ApplesoftProgram                  program ([&session] (Word address) { Byte value = 0; session.TryPeek (address, value); return value; });
    Byte                              low       = 0;
    Byte                              high      = 0;
    bool                              isVisible = false;



    isVisible         = BasicStopMonitor::IsApplesoftVisible (session);
    state.isApplesoft = isVisible || BasicStopMonitor::IsApplesoftInRom (session.GetTarget());

    if (!state.isApplesoft)
    {
        return state;
    }

    session.TryPeek (ApplesoftProgram::kTxtPtr,              low);
    session.TryPeek ((Word) (ApplesoftProgram::kTxtPtr + 1), high);

    state.hasProgram  = program.TryGetListing (state.lines);
    state.line        = program.IsAtPrompt (session.GetTarget().GetRegisters().sp) ? std::nullopt : program.GetCurrentLine();
    state.textPointer = (Word) ((low | (high << 8)) + (session.GetTarget().GetRegisters().pc == BasicStopMonitor::kStatement ? 0 : 1));
    state.hasValues   = (isPaused || !state.line.has_value()) && isVisible && state.hasProgram;

    if (state.hasValues)
    {
        program.GetVariables (state.variables);
        program.GetArrays    (state.arrays);
        program.GetFrames    (state.frames);
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildLive
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildLive (DebugSession & session, DebuggerViewSnapshot & snapshot, bool isPaused) const
{
    m_panels.BuildPanels   (session, snapshot);
    m_heatMap.BuildHeatMap (session, snapshot, isPaused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildSymbolBytes
//
//  The bytes at each symbol's address, read without side effects; an I/O
//  switch is skipped, since reading one would change the machine.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildSymbolBytes (DebugSession & session, DebuggerViewSnapshot & snapshot)
{
    if (snapshot.symbols == nullptr)
    {
        return;
    }

    snapshot.symbolBytes.reserve (snapshot.symbols->symbols.size());

    for (const SymbolInfo & symbol : snapshot.symbols->symbols)
    {
        DebuggerViewSnapshot::SymbolBytes  read;
        size_t                             wanted = std::clamp<size_t> (symbol.size, 1, DebuggerViewSnapshot::kMaxSymbolBytes);



        if (symbol.isConstant || session.GetTarget().GetRegion (symbol.address) == MemoryRegion::Io)
        {
            continue;
        }

        read.address = symbol.address;

        while (read.count < wanted && session.TryPeek ((Word) (symbol.address + read.count), read.bytes[read.count]))
        {
            read.count++;
        }

        if (read.count > 0)
        {
            snapshot.symbolBytes.push_back (read);
        }
    }

    //  One entry for an address, the one with the most bytes.
    std::ranges::sort (snapshot.symbolBytes, [] (const auto & left, const auto & right)
    {
        return (left.address != right.address) ? left.address < right.address : left.count > right.count;
    });

    snapshot.symbolBytes.erase (std::unique (snapshot.symbolBytes.begin(), snapshot.symbolBytes.end(),
                                             [] (const auto & left, const auto & right) { return left.address == right.address; }),
                                snapshot.symbolBytes.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::BuildTrace
//
//  The trace pane is the window HISTORY first count would report, around
//  where the pane is scrolled; the trace size only says where it starts.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::BuildTrace (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    uint64_t   total = session.GetTarget().GetTraceSize();
    uint64_t   first = GetTraceWindowFirst (total, m_traceTop, kTraceRows);
    TraceData  data  = TraceHandlers::MakeWindow (session, (size_t) first, kTraceRows);



    snapshot.trace.isOn    = data.isOn;
    snapshot.trace.total   = data.total;
    snapshot.trace.first   = first;
    snapshot.trace.entries = std::move (data.entries);

    if (snapshot.isPaused && (data.isOn || data.total > 0))
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
    const LineTable                       & table = session.GetFiles().GetLineTable();
    const DebugFile                       & file  = session.GetFiles().GetDebugFile();
    const std::vector<SourcePosition>     & atPc  = table.GetPositionsAt (snapshot.pc);
    DebuggerViewSnapshot::SourceState       state;
    std::string                             key;



    if (!session.GetFiles().HasDebugFile())
    {
        return;
    }

    //  The same file loaded at another offset has the same text, so the key
    //  holds where each segment was placed too.
    key = session.GetFiles().GetDebugFileKey() + SourcePathList::WideToUtf8 (session.GetFiles().GetDebugFilePath());

    for (const DebugSegment & segment : file.segments)
    {
        key += std::format (":{:X}", segment.start);
    }

    if (m_lineAddresses == nullptr || key != m_lineAddressesKey)
    {
        auto  addresses = std::make_shared<std::map<std::pair<int, int>, Word>>();
        auto  ranges    = std::make_shared<std::map<std::pair<int, int>, std::pair<Word, Word>>>();

        for (const DebugLine & line : file.lines)
        {
            for (const std::pair<Word, Word> & range : table.GetRanges (line.file, line.line))
            {
                auto  found = addresses->find ({ line.file, line.line });

                if (found == addresses->end() || range.first < found->second)
                {
                    (*addresses)[{ line.file, line.line }] = range.first;
                    (*ranges)[{ line.file, line.line }]    = range;
                }
            }
        }

        m_lineAddresses    = addresses;
        m_lineRanges       = ranges;
        m_lineAddressesKey = key;
    }

    state.debugFilePath = session.GetFiles().GetDebugFilePath();
    state.programKey    = session.GetFiles().GetDebugFileKey();
    state.files         = file.files;
    state.stepBySource  = session.GetFiles().IsStepBySource();
    state.lineAddresses = m_lineAddresses;
    state.lineRanges    = m_lineRanges;
    state.isStale       = session.GetFiles().IsDebugFileStale();

    if (m_heatMap.IsHeatColumnsShown())
    {
        state.unmatchedLines = std::make_shared<const std::set<std::pair<int, int>>> (session.GetFiles().GetUnmatchedSourceLines());
    }

    //  Over a program that isn't the file's, the documents are open but
    //  none of their lines maps to an address.
    if (state.isStale)
    {
        static const auto  s_kNoAddresses = std::make_shared<const std::map<std::pair<int, int>, Word>>();
        static const auto  s_kNoRanges    = std::make_shared<const std::map<std::pair<int, int>, std::pair<Word, Word>>>();

        state.lineAddresses = s_kNoAddresses;
        state.lineRanges    = s_kNoRanges;
        snapshot.source     = std::move (state);
        return;
    }

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
        std::optional<DisassemblyLine>  atPcLine = InstructionLookup::GetInstructionAt (session, snapshot.pc);
        InstructionTouches::Result      touches;

        for (const auto & [place, first] : *m_lineAddresses)
        {
            std::optional<DisassemblyLine>  line;
            std::string                     effect;

            if (place.first != state.fileId && place.first != state.bodyFileId)
            {
                continue;
            }

            line = InstructionLookup::GetInstructionAt (session, first);

            if (!line.has_value())
            {
                continue;
            }

            touches = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(), now,
                                                first, (Word) line->instruction.bytes.size());

            if (first == snapshot.pc)
            {
                effect = CodeAnnotations::GetEffect (session, now, touches, (Word) (first + line->instruction.bytes.size()));
            }

            (*operands)[place] = { CodeAnnotations::GetAnnotation (session, *line, now, touches), effect };
        }

        if (m_lineOperands == nullptr || *m_lineOperands != *operands)
        {
            m_lineOperands = operands;
        }

        //  The instructions in every range each of those lines produced.
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
//  DebuggerViewState::DescribeSymbolSources
//
//  Main, Basic, DOS 3.3 and ProDOS come filled with the ROM's and the
//  systems' entry points; any other table is filled from a file or by hand.
//  A table turned off says so.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewState::DescribeSymbolSources (const SymbolTable & symbols)
{
    std::vector<std::string>  lines;



    for (int i = 0; i < SymbolTable::kTableCount; i++)
    {
        SymbolTableId                     table   = (SymbolTableId) i;
        size_t                            count   = symbols.GetCount (table);
        const std::vector<std::string>  & origins = symbols.GetOrigins (table);
        std::string                       name    = SymbolTable::ToUpper (ReplyJson::GetSymbolTableName (table));
        std::string                       off     = symbols.IsEnabled (table) ? "" : ", turned off";

        if (count == 0)
        {
            continue;
        }

        if (origins.empty())
        {
            lines.push_back (std::format ("Built-in {} table, {} symbols{}", name, count, off));
            continue;
        }

        for (const std::string & origin : origins)
        {
            lines.push_back (std::format ("{}, in the {} table{}", origin, name, off));
        }
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::AddSessionStops
//
//  The session's own stops after the table's breakpoints: a read before
//  written, which counts only once the heat map is tracking writes, a video
//  scanline range, and a beam position, each with no id.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewState::AddSessionStops (DebugSession & session, DebuggerViewSnapshot & snapshot)
{
    std::optional<std::pair<uint32_t, uint32_t>>  video = session.GetVideoBreak();
    std::optional<uint32_t>                       beam  = session.GetBeamBreak();
    BreakpointInfo                                info;



    info.id      = -1;
    info.enabled = true;

    if (session.HasUnwrittenBreak())
    {
        info.kind       = BreakpointKind::UnwrittenRead;
        info.isTracking = session.GetTarget().GetUnwrittenStatus().isTracking;
        snapshot.breakpoints.push_back ({ -1, 0, std::string(), true, info });
    }

    if (video.has_value())
    {
        info.kind    = BreakpointKind::VideoLine;
        info.address = (Word) video->first;
        info.last    = (Word) video->second;
        snapshot.breakpoints.push_back ({ -1, 0, std::string(), true, info });
    }

    if (beam.has_value())
    {
        info.kind    = BreakpointKind::Beam;
        info.address = (Word) (*beam / VideoTiming::kCyclesPerScanline);
        info.last    = (Word) (*beam % VideoTiming::kCyclesPerScanline);
        snapshot.breakpoints.push_back ({ -1, 0, std::string(), true, info });
    }
}
