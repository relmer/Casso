#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Reverse/HistoryStatus.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/DebugSession.h"
#include "Disassembler.h"
#include "Machines/Apple2/Common/VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool ExecutionHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::SetProgramCounter: SetProgramCounter (session, command, reply); return true;
    case DebugVerb::CallSubroutine:    CallSubroutine    (session, command, reply); return true;
    case DebugVerb::WriteNop:          WriteNop          (session, reply);          return true;
    case DebugVerb::InjectKey:         QueueKeys         (session, command, reply); return true;
    case DebugVerb::BreakOnVideoLine:  BreakOnVideoLine  (session, command, reply); return true;
    case DebugVerb::BreakOnBeam:       BreakOnBeam       (session, command, reply); return true;
    case DebugVerb::RunFrame:          RunFrame          (session, command, reply); return true;
    case DebugVerb::ShowVideoInfo:     ShowVideoInfo     (session, reply);          return true;
    case DebugVerb::ShowBranchRecord:  ShowBranchRecord  (session, reply);          return true;
    case DebugVerb::TraceToFile:       ToggleTrace       (session, command, reply); return true;
    case DebugVerb::Profile:           Profile           (session, command, reply); return true;
    case DebugVerb::Stopwatch:         Stopwatch         (session, command, reply); return true;
    case DebugVerb::ShowCycles:        ShowCycles        (session, command, reply); return true;
    case DebugVerb::ResetCycles:       ResetCycles       (session, reply);          return true;
    case DebugVerb::StepBack:
    case DebugVerb::StepBackOver:
    case DebugVerb::StepBackOut:
    case DebugVerb::ReverseGo:
    case DebugVerb::GoLive:            RunReverse        (session, command, reply); return true;
    default:                                                                        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnInstruction
//
//  Profiling is fed every instruction, a free run's too; the keys, the
//  stopwatch and the trace only a run the debugger started.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnInstruction (DebugSession & session, Word pc)
{
    bool  isDebuggerRun = session.GetRunState() == RunState::DebugRun || session.GetRunState() == RunState::Stepping;



    RecordProfile (session, pc);

    if (!isDebuggerRun)
    {
        return;
    }

    FeedKeys (session);

    if (m_stopwatch.IsArmed())
    {
        m_stopwatch.OnInstruction (pc, session.GetTarget().GetCycleCount());
    }

    // Tested here, not in RecordTrace, so a run with TF off does not pay for
    // the disassembler and the register reads RecordTrace sets up.
    if (m_trace.isOn)
    {
        RecordTrace (session, pc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnInterrupt
//
//  The instruction the interrupt was taken in place of has not run, so it
//  gets no trace line and no profile count; it is reported again when it
//  runs after the return. The instruction before it is billed now, before
//  the dispatch's cycles are added to the count.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnInterrupt (DebugSession & session)
{
    FeedKeys    (session);
    BillProfile (session);

    if (m_profile.IsOn())
    {
        m_routines.OnInterrupt();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnFreeRunSlice
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnFreeRunSlice (DebugSession & session)
{
    FeedKeys (session);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnRunStopped
//
//  The trace file is written at every stop, so a script that never turns
//  tracing off still leaves the file behind. The last instruction of the run
//  is billed to the profile here, since no next instruction will bill it.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnRunStopped (DebugSession & session, const StopEvent & stop)
{
    HRESULT  hr = S_OK;



    BillProfile (session);

    m_lastRunCycles = stop.cycles;
    hr              = FlushTrace (session);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnMachineChanged
//
//  The profile's opcodes were the old CPU's, which the new one may decode
//  differently, and its cycle count is not the new machine's, so the counts
//  and the instruction waiting to be billed go. Profiling stays on or off.
//  The stopwatch's laps, counted on the old machine's clock, go too; it
//  stays armed.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnMachineChanged (DebugSession & session)
{
    (void) session;

    m_profile.Reset();
    m_routines.Reset();
    m_profilePending.reset();
    m_stopwatch.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::SetProgramCounter
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::SetProgramCounter (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Cpu6502Registers  registers = session.GetTarget().GetRegisters();



    if (!command.hasA1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "= takes the new program counter.");
        return;
    }

    registers.pc = command.a1;
    session.GetTarget().SetRegisters (registers);
    reply.data = RegistersData { registers };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::CallSubroutine
//
//  JSR addr pushes the return address as the instruction would, so the
//  subroutine's RTS comes back to the current program counter, and runs
//  until it does.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::CallSubroutine (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    static constexpr int  kByteBits  = 8;
    IDebugTarget        & target     = session.GetTarget();
    Cpu6502Registers      registers  = target.GetRegisters();
    Word                  returnTo   = registers.pc;
    Word                  pushed     = (Word) (returnTo - 1);
    DebugCommand          run;



    if (!command.hasA1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "JSR takes the subroutine's address.");
        return;
    }

    target.TryPoke ((Word) (kStackPage + registers.sp), (Byte) (pushed >> kByteBits));
    registers.sp = (Byte) (registers.sp - 1);
    target.TryPoke ((Word) (kStackPage + registers.sp), (Byte) pushed);
    registers.sp = (Byte) (registers.sp - 1);
    registers.pc = command.a1;
    target.SetRegisters (registers);

    run.verb       = DebugVerb::Go;
    run.sourceName = command.sourceName;
    run.a1         = returnTo;
    run.hasA1      = true;
    run.budget     = command.budget;
    reply          = session.Execute (run);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::WriteNop
//
//  Every byte of the instruction at the program counter becomes a NOP, and
//  the reply shows what is there now.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::WriteNop (DebugSession & session, Reply & reply)
{
    IDebugTarget    & target = session.GetTarget();
    Disassembler      disassembler (target.GetInstructionSet());
    Word             pc                             = target.GetRegisters().pc;
    size_t           length                         = disassembler.GetLength (Peek (target, pc));
    DisassemblyData  data;
    HRESULT          hr                             = S_OK;
    Byte             original[kMaxInstructionBytes] = {};



    //  All of it or none of it: the bytes already written go back when one
    //  cannot be, so no instruction is left half replaced.
    for (size_t i = 0; i < length; ++i)
    {
        original[i] = Peek (target, (Word) (pc + i));

        if (!target.TryPoke ((Word) (pc + i), kNop))
        {
            for (size_t j = 0; j < i; ++j)
            {
                (void) target.TryPoke ((Word) (pc + j), original[j]);
            }

            reply.SetError (CommandStatus::Error, "memory not writable", std::format ("${:04X} cannot be written.", (Word) (pc + i)));
            return;
        }
    }

    for (size_t i = 0; i < length; ++i)
    {
        DisassemblyLine  line;
        Byte             nop = kNop;



        hr = disassembler.DisassembleOne ((Word) (pc + i), std::span<const Byte> (&nop, 1), line.instruction);
        IGNORE_RETURN_VALUE (hr, S_OK);
        data.lines.push_back (line);
    }

    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::QueueKeys
//
//  The first key goes to the keyboard at once if none is pending; the rest
//  wait in the queue and are fed as the guest clears the strobe, before each
//  instruction of a debugger-driven run and after each slice of a free run.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::QueueKeys (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    if (command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "KEY takes one or more key codes.");
        return;
    }

    m_keys.insert (m_keys.end(), command.values.begin(), command.values.end());
    FeedKeys (session);
    reply.data = MessageData { { std::format ("Queued {} key{}. Keys waiting: {}.", command.values.size(), command.values.size() == 1 ? "" : "s", m_keys.size()) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::FeedKeys
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::FeedKeys (DebugSession & session)
{
    if (!m_keys.empty() && !session.GetTarget().IsKeyPending())
    {
        session.GetTarget().InjectKey (m_keys.front());
        m_keys.pop_front();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BreakOnVideoLine
//
//  BPV vpos[,len] stops when the scanline enters the range, once. A range
//  that starts past the frame's last scanline could never stop. The
//  scanline is hex, like every number BPV and VIDEOINFO print. BPV OFF
//  clears it.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BreakOnVideoLine (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word  last = command.hasA2 ? command.a2 : command.a1;



    if (command.text == "OFF")
    {
        session.ClearVideoBreak();
        reply.data = MessageData { { "Video scanline breakpoint cleared." } };
        return;
    }

    if (command.a1 >= VideoTiming::kScanlinesPerFrame)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("Scanline {:X} is past the frame. Scanlines run from 0 to {:X}.", command.a1, VideoTiming::kScanlinesPerFrame - 1));
        return;
    }

    session.SetVideoBreak (command.a1, last);
    reply.data = MessageData { { std::format ("Breakpoint set on video scanlines ${:X}-${:X}. It clears after it fires.", command.a1, last) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BreakOnBeam
//
//  BPBEAM line cycle stops when the beam reaches that scanline and cycle;
//  BPBEAM VBL stops where vertical blank starts. Like BPV it fires once, and
//  the numbers are hex. BPBEAM OFF clears it.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BreakOnBeam (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    bool      isVbl    = command.text == "VBL";
    uint32_t  scanline = isVbl ? VideoTiming::kVblankStartScanline : command.a1;
    uint32_t  cycle    = isVbl ? 0 : command.a2;



    if (command.text == "OFF")
    {
        session.ClearBeamBreak();
        reply.data = MessageData { { "Beam position breakpoint cleared." } };
        return;
    }

    if (scanline >= VideoTiming::kScanlinesPerFrame)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("Scanline {:X} is past the frame. Scanlines run from 0 to {:X}.", scanline, VideoTiming::kScanlinesPerFrame - 1));
        return;
    }

    if (cycle >= VideoTiming::kCyclesPerScanline)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("Cycle {:X} is past the scanline. Cycles run from 0 to {:X}.", cycle, VideoTiming::kCyclesPerScanline - 1));
        return;
    }

    session.SetBeamBreak (scanline * VideoTiming::kCyclesPerScanline + cycle);

    reply.data = MessageData { { isVbl ? std::format ("Breakpoint set at the start of vertical blank, scanline ${:X}. It clears after it fires.", scanline)
                                       : std::format ("Breakpoint set at video scanline ${:X}, cycle ${:X}. It clears after it fires.", scanline, cycle) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::RunFrame
//
//  FRAME [count] runs until the beam comes back to where it is now, count
//  times: one whole frame each, 17,030 cycles. Any other stop condition met
//  on the way stops it first, and the frame break then goes with the run.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::RunFrame (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    VideoPosition  position = session.GetTarget().GetVideoPosition();
    DebugCommand   run;



    session.SetBeamBreak (position.scanline * VideoTiming::kCyclesPerScanline + position.cycleInLine, std::max (command.count, 1u), true);

    run.verb       = DebugVerb::Go;
    run.sourceName = command.sourceName;
    run.budget     = command.budget;
    reply          = session.Execute (run);

    if (reply.status != CommandStatus::Ok)
    {
        session.ClearBeamBreak();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ShowVideoInfo
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ShowVideoInfo (DebugSession & session, Reply & reply)
{
    VideoPosition  position = session.GetTarget().GetVideoPosition();



    reply.data = VideoInfoData { position.scanline, position.cycleInLine };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ShowBranchRecord
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ShowBranchRecord (DebugSession & session, Reply & reply)
{
    Word              from   = 0;
    BranchRecordData  record;



    if (session.GetTarget().TryGetLastBranch (from))
    {
        record.address = from;
    }

    reply.data = record;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::RecordProfile
//
//  The hook reports an instruction before it runs, so its cost is billed at
//  the next instruction, or at the stop for the last one. Nothing is kept
//  while profiling is off.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::RecordProfile (DebugSession & session, Word pc)
{
    IDebugTarget        & target  = session.GetTarget();
    PendingInstruction    pending;



    if (!m_profile.IsOn())
    {
        return;
    }

    BillProfile (session);

    pending.pc          = pc;
    pending.opcode      = Peek (target, pc);
    pending.startCycles = target.GetCycleCount();
    m_profilePending    = pending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BillProfile
//
//  The cycles the pending instruction took, from the machine's count, and the
//  penalties the CPU recorded for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BillProfile (DebugSession & session)
{
    IDebugTarget  & target    = session.GetTarget();
    uint64_t        cycles    = 0;
    Byte            penalties = 0;



    if (!m_profilePending.has_value())
    {
        return;
    }

    //  A power cycle restarts the count under the pending instruction, so
    //  what it cost is unknown and it is not billed.
    if (target.GetCycleCount() < m_profilePending->startCycles)
    {
        m_profilePending.reset();
        return;
    }

    cycles    = target.GetCycleCount() - m_profilePending->startCycles;
    penalties = target.GetLastPenalties();

    m_profile.Record  (m_profilePending->pc, m_profilePending->opcode, (Byte) std::min (cycles, kMaxBilledCycles), penalties);
    m_routines.Record (m_profilePending->pc, m_profilePending->opcode, std::min (cycles, kMaxBilledCycles), target.GetRegisters().sp,
                       m_profilePending->startCycles, RoutineProfile::kCyclesPerFrame);
    m_profilePending.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BuildProfile
//
//  The opcode rows are grouped by mnemonic and addressing mode, since several
//  opcodes can share both, and sorted by cycles. The address rows are the
//  hottest kHotAddresses, each with the first symbol that holds it.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BuildProfile (DebugSession & session, ProfileList list, ProfileData & data) const
{
    using ModeKey = std::pair<std::string, std::string>;

    const Microcode                   * set    = session.GetTarget().GetInstructionSet();
    std::map<ModeKey, ProfileEntry>     groups;
    ProfileAddressEntry                 entry;
    SymbolTableId                       table  = SymbolTableId::Main;



    data.isOn         = m_profile.IsOn();
    data.list         = list;
    data.instructions = m_profile.GetInstructionCount();
    data.cycles       = m_profile.GetTotalCycles();
    data.pageCross    = m_profile.GetPenalties().pageCross;
    data.branchTaken  = m_profile.GetPenalties().branchTaken;
    data.branchCross  = m_profile.GetPenalties().branchCross;

    for (size_t opcode = 0; opcode < ProfileTable::kOpcodeCount; ++opcode)
    {
        const ProfileTable::OpcodeCounts  & counts  = m_profile.GetOpcode ((Byte) opcode);
        bool                                isKnown = set != nullptr && set[opcode].isLegal;
        ModeKey                             key;



        if (counts.count == 0)
        {
            continue;
        }

        key.first  = isKnown ? set[opcode].instructionName : "???";
        key.second = isKnown ? GlobalAddressingMode::s_addressingModeName[set[opcode].globalAddressingMode] : "???";

        groups[key].mnemonic  = key.first;
        groups[key].mode      = key.second;
        groups[key].count    += counts.count;
        groups[key].cycles   += counts.cycles;
    }

    for (const auto & [key, group] : groups)
    {
        data.opcodes.push_back (group);
    }

    std::stable_sort (data.opcodes.begin(), data.opcodes.end(), [] (const ProfileEntry & a, const ProfileEntry & b) { return a.cycles > b.cycles; });

    BuildRoutines (session, data);

    if (list != ProfileList::Addresses)
    {
        return;
    }

    for (const auto & [address, cycles] : m_profile.GetByAddress())
    {
        entry.address = address;
        entry.cycles  = cycles;
        entry.symbol.clear();
        session.GetSymbols().TryFindName (address, entry.symbol, table);
        data.addresses.push_back (entry);
    }

    std::sort (data.addresses.begin(), data.addresses.end(), [] (const ProfileAddressEntry & a, const ProfileAddressEntry & b)
    {
        return a.cycles != b.cycles ? a.cycles > b.cycles : a.address < b.address;
    });

    if (data.addresses.size() > kHotAddresses)
    {
        data.addresses.resize (kHotAddresses);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BuildRoutines
//
//  The routines by their own cycles, each with its symbol; the call tree
//  depth first, each node's callees by their totals; and the frames, oldest
//  first. The tree's and the frames' routine indexes are places in the
//  sorted list.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BuildRoutines (DebugSession & session, ProfileData & data) const
{
    const std::vector<RoutineProfile::Routine>  & routines     = m_routines.GetRoutines();
    const std::vector<RoutineProfile::Node>     & nodes        = m_routines.GetNodes();
    std::vector<uint64_t>                         nodeTotals   = m_routines.GetNodeTotals();
    std::vector<uint64_t>                         routineTotal = m_routines.GetRoutineTotals();
    std::vector<size_t>                           order (routines.size());
    std::vector<size_t>                           place (routines.size());
    std::vector<std::vector<size_t>>              children (nodes.size());
    std::vector<std::pair<size_t, int>>           pending;
    std::vector<ProfileRoutineEntry>              sorted;
    SymbolTableId                                 table        = SymbolTableId::Main;



    data.routines.resize (routines.size());

    for (size_t i = 0; i < routines.size(); i++)
    {
        ProfileRoutineEntry  & entry = data.routines[i];

        entry.address = routines[i].address;
        entry.kind    = routines[i].kind;
        entry.total   = routineTotal[i];

        if (entry.kind != RoutineKind::Top)
        {
            session.GetSymbols().TryFindName (entry.address, entry.symbol, table);
        }
    }

    for (size_t i = 0; i < nodes.size(); i++)
    {
        data.routines[nodes[i].routine].self  += nodes[i].self;
        data.routines[nodes[i].routine].calls += nodes[i].calls;

        if (i > 0)
        {
            children[(size_t) nodes[i].parent].push_back (i);
        }
    }

    for (size_t i = 0; i < order.size(); i++)
    {
        order[i] = i;
    }

    std::stable_sort (order.begin(), order.end(), [&data] (size_t a, size_t b) { return data.routines[a].self > data.routines[b].self; });

    for (size_t i = 0; i < order.size(); i++)
    {
        place[order[i]] = i;
    }

    for (size_t index : order)
    {
        sorted.push_back (data.routines[index]);
    }

    data.routines = std::move (sorted);

    pending.push_back ({ (size_t) 0, 0 });

    while (!pending.empty())
    {
        auto [node, depth] = pending.back();
        auto &  callees    = children[node];

        pending.pop_back();
        data.tree.push_back (ProfileTreeEntry { place[nodes[node].routine], depth, nodes[node].self, nodeTotals[node], nodes[node].calls });

        //  The hottest callee goes on last, so it comes off first.
        std::stable_sort (callees.begin(), callees.end(), [&nodeTotals] (size_t a, size_t b) { return nodeTotals[a] < nodeTotals[b]; });

        for (size_t callee : callees)
        {
            pending.push_back ({ callee, depth + 1 });
        }
    }

    for (const RoutineProfile::Frame & frame : m_routines.GetFrames())
    {
        ProfileFrameEntry  entry;



        entry.index = frame.index;
        entry.other = frame.other;
        entry.count = frame.count;

        for (const RoutineProfile::Share & share : frame.shares)
        {
            entry.shares.push_back ({ place[share.routine], share.cycles });
        }

        data.frames.push_back (std::move (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::TryGetProfileList
//
////////////////////////////////////////////////////////////////////////////////

bool ExecutionHandlers::TryGetProfileList (const std::string & word, ProfileList & list)
{
    std::string  upper = SymbolTable::ToUpper (word);



    if      (upper.empty())       { list = ProfileList::Opcodes;   }
    else if (upper == "ADDR")     { list = ProfileList::Addresses; }
    else if (upper == "ROUTINES") { list = ProfileList::Routines;  }
    else if (upper == "TREE")     { list = ProfileList::Tree;      }
    else                          { return false;                  }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::SaveProfile
//
//  The same rows PROFILE LIST, PROFILE LIST ADDR, PROFILE LIST ROUTINES and
//  PROFILE LIST TREE print, one after the other.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::SaveProfile (DebugSession & session, const std::string & name, Reply & reply) const
{
    IFileSystem  * files     = session.GetFileSystem();
    std::string    text;
    HRESULT        hr        = S_OK;



    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    for (ProfileList list : { ProfileList::Opcodes, ProfileList::Addresses, ProfileList::Routines, ProfileList::Tree })
    {
        Reply        table;
        ProfileData  data;



        BuildProfile (session, list, data);
        table.data = data;
        AppleWinFormatter::Format (table);

        for (const std::string & line : table.text)
        {
            text += line + "\n";
        }
    }

    hr = files->WriteAllText (session.ResolvePath (name), text);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    reply.data = MessageData { { std::format ("Saved the profile{}.", session.GetPathEcho (name, "to")) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Profile
//
//  PROFILE ON|OFF|RESET|LIST [ADDR|ROUTINES|TREE]|SAVE [file]|HISTORY
//  [seconds|ALL]. A bare PROFILE lists. HISTORY builds the profile of the
//  last seconds of history, or of all of it, in the background.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Profile (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::istringstream  stream (command.text);
    std::string         verb;
    std::string         argument;
    std::string         extra;
    std::string         rest;
    ProfileData         data;
    ProfileList         list     = ProfileList::Opcodes;



    stream >> verb;
    std::getline (stream, rest);
    verb = SymbolTable::ToUpper (verb);

    //  The file name is the rest of the line, so a quoted name may hold
    //  spaces.
    if (verb == "SAVE")
    {
        rest = TrimSpaces (rest);
        SaveProfile (session, rest.empty() ? kDefaultProfile : rest, reply);
        return;
    }

    stream.clear();
    stream.str (rest);
    stream >> argument >> extra;

    if (verb == "ON" && argument.empty())
    {
        m_profile.SetOn (true);
        session.OnStopConditionsChanged();
        reply.data = MessageData { { "Profiling on." } };
    }
    else if (verb == "OFF" && argument.empty())
    {
        m_profile.SetOn (false);
        m_profilePending.reset();
        session.OnStopConditionsChanged();
        reply.data = MessageData { { "Profiling off." } };
    }
    else if (verb == "RESET" && argument.empty())
    {
        m_profile.Reset();
        m_routines.Reset();
        m_profilePending.reset();
        reply.data = MessageData { { "Profile reset." } };
    }
    else if (verb == "HISTORY" && extra.empty())
    {
        BuildProfileFromHistory (session, argument, reply);
    }
    else if ((verb.empty() || verb == "LIST") && extra.empty() && TryGetProfileList (argument, list))
    {
        BuildProfile (session, list, data);
        reply.data = data;
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PROFILE takes ON, OFF, RESET, LIST [ADDR|ROUTINES|TREE], SAVE [file] or HISTORY [seconds|ALL].");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BuildProfileFromHistory
//
//  The seconds are decimal, counted back from the machine's position at the
//  machine's own clock; ALL or none is all of history.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BuildProfileFromHistory (DebugSession & session, const std::string & argument, Reply & reply)
{
    uint64_t     now       = session.GetTarget().GetHistoryCycleCount();
    uint64_t     from      = 0;
    uint64_t     seconds   = 0;
    std::string  error;
    HRESULT      hr        = S_OK;
    bool         isAll     = argument.empty() || SymbolTable::ToUpper (argument) == "ALL";



    if (!m_historyProfiler)
    {
        reply.SetError (CommandStatus::NotAvailable, "no history", "This session has no history to profile.");
        return;
    }

    if (!isAll && (argument.find_first_not_of ("0123456789") != std::string::npos ||
                   std::from_chars (argument.data(), argument.data() + argument.size(), seconds).ec != std::errc()))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} is not a number of seconds.", argument));
        return;
    }

    from = isAll ? 0 : now - std::min (now, (uint64_t) ((double) seconds * HistoryStatus::kCyclesPerSecond));
    hr   = m_historyProfiler (from, error);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "no profile", error);
        return;
    }

    reply.data = MessageData { { isAll ? std::string ("Profiling all of history.") : std::format ("Profiling the last {} seconds of history.", seconds) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::InstallProfile
//
//  The counts as they came from history; whether profiling is on is the
//  profile's own.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::InstallProfile (const ProfileCounts & counts)
{
    bool  isOn = m_profile.IsOn();



    m_profile  = counts.table;
    m_routines = counts.routines;
    m_profile.SetOn (isOn);
    m_profilePending.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Stopwatch
//
//  STOPWATCH start [stop] arms the stopwatch, stop defaulting to start;
//  STOPWATCH OFF disarms it and RESET clears its laps. A bare STOPWATCH
//  shows it. Each address is an expression, as BP takes one.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Stopwatch (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    constexpr int32_t   kLastAddress = 0xFFFF;
    std::istringstream  stream (command.text);
    std::string         first;
    std::string         second;
    std::string         extra;
    std::string         keyword;
    std::string         error;
    int32_t             start        = 0;
    int32_t             stop         = 0;
    HRESULT             hr           = S_OK;



    stream >> first >> second >> extra;
    keyword = SymbolTable::ToUpper (first);

    if (first.empty())
    {
        ShowStopwatch (reply);
        return;
    }

    if ((keyword == "OFF" || keyword == "RESET") && second.empty())
    {
        if (keyword == "OFF")
        {
            m_stopwatch.Disarm();
        }
        else
        {
            m_stopwatch.Reset();
        }

        ShowStopwatch (reply);
        return;
    }

    hr = DebugExpressionEvaluator::ParseAndEvaluate (first, session, start, error);

    if (SUCCEEDED (hr) && !second.empty())
    {
        hr = DebugExpressionEvaluator::ParseAndEvaluate (second, session, stop, error);
    }
    else
    {
        stop = start;
    }

    if (FAILED (hr) || !extra.empty() || start < 0 || start > kLastAddress || stop < 0 || stop > kLastAddress)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error.empty() ? "STOPWATCH takes start [stop], OFF or RESET." : error);
        return;
    }

    m_stopwatch.Arm ((Word) start, (Word) stop);
    ShowStopwatch (reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ShowStopwatch
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ShowStopwatch (Reply & reply) const
{
    std::vector<std::string>  lines;
    uint64_t                  laps  = m_stopwatch.GetLapCount();



    if (m_stopwatch.IsArmed())
    {
        lines.push_back (std::format ("Stopwatch from ${:04X} to ${:04X}, in runs the debugger starts.", m_stopwatch.GetStart(), m_stopwatch.GetStop()));
    }
    else
    {
        lines.push_back ("Stopwatch off.");
    }

    if (laps == 0)
    {
        lines.push_back ("No laps.");
    }
    else
    {
        lines.push_back (std::format ("{} laps: last {} cycles, shortest {}, longest {}, average {}.",
                                      laps,
                                      m_stopwatch.GetLastLap(),
                                      m_stopwatch.GetShortest(),
                                      m_stopwatch.GetLongest(),
                                      m_stopwatch.GetTotal() / laps));
    }

    if (m_stopwatch.IsTiming())
    {
        lines.push_back ("A lap is being timed.");
    }

    reply.data = MessageData { lines };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ToggleTrace
//
//  TF ["file"] [v] turns the trace file on, or off if it is on. Each
//  instruction a debugger-driven run executes becomes one line. The file is
//  written at each stop and when tracing ends. TF ON ["file"] [v] and TF OFF
//  are not switches: ON with the file already on is nothing to do, and OFF
//  with it off is nothing to do, so a debug project can run either again.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ToggleTrace (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string  name      = TrimSpaces (command.text);
    std::string  first     = name.substr (0, name.find_first_of (" \t"));
    size_t       split     = 0;
    bool         withVideo = false;
    HRESULT      hr        = S_OK;



    std::ranges::transform (first, first.begin(), [] (char ch) { return (char) toupper ((unsigned char) ch); });

    if (first == "OFF" && !m_trace.isOn)
    {
        reply.data = MessageData { { "Trace off." } };
        return;
    }

    if (first == "ON")
    {
        name = TrimSpaces (name.substr (2));

        if (m_trace.isOn)
        {
            reply.data = MessageData { { std::format ("Trace on: {}", m_trace.name) } };
            return;
        }
    }

    if (m_trace.isOn)
    {
        hr = FlushTrace (session);

        if (FAILED (hr) || m_trace.hasFailed)
        {
            reply.SetError (CommandStatus::Error, "file not written", std::format ("Trace off. {} could not be written.", m_trace.name));
        }
        else
        {
            reply.data = MessageData { { std::format ("Trace off: {}", m_trace.name) } };
        }

        m_trace = TraceState();
        return;
    }

    //  v is a word of its own before or after the name; the rest of the line
    //  is the name, so a quoted name may hold spaces.
    split = name.find_last_of (" \t");

    if (name == "v" || name == "V")
    {
        withVideo = true;
        name.clear();
    }
    else if (split != std::string::npos && (name.substr (split + 1) == "v" || name.substr (split + 1) == "V"))
    {
        withVideo = true;
        name      = TrimSpaces (name.substr (0, split));
    }
    else if (name.starts_with ("v ") || name.starts_with ("V ") || name.starts_with ("v\t") || name.starts_with ("V\t"))
    {
        withVideo = true;
        name      = TrimSpaces (name.substr (1));
    }

    if (session.GetFileSystem() == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    name = name.empty() ? kDefaultTrace : name;

    //  A path that cannot be written is reported now rather than lost at the
    //  first stop, where no reply is sent.
    hr = ProbeWritable (*session.GetFileSystem(), session.ResolvePath (name));

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    m_trace.isOn      = true;
    m_trace.withVideo = withVideo;
    m_trace.name      = name;
    m_trace.path      = session.ResolvePath (m_trace.name);
    reply.data        = MessageData { { std::format ("Trace on: {}", m_trace.name) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ProbeWritable
//
//  Writes the file without changing what it holds: an existing file gets
//  its own content back, and a new one is created empty and removed again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExecutionHandlers::ProbeWritable (IFileSystem & files, const std::wstring & path)
{
    HRESULT      hr       = S_OK;
    bool         isThere  = files.Exists (path);
    std::string  content;



    if (isThere)
    {
        hr = files.ReadAllText (path, content);
        CHR (hr);
    }

    hr = files.WriteAllText (path, content);
    CHR (hr);

    if (!isThere)
    {
        hr = files.Delete (path);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::RecordTrace
//
//  Cycles, the registers, the flags and the disassembly; with v, the video
//  scanline and horizontal position replace the cycle count.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::RecordTrace (DebugSession & session, Word pc)
{
    IDebugTarget           & target    = session.GetTarget();
    Disassembler             disassembler (target.GetInstructionSet());
    Cpu6502Registers         registers = target.GetRegisters();
    VideoPosition            video     = target.GetVideoPosition();
    DisassembledInstruction  instruction;
    std::string              line;
    HRESULT                  hr        = S_OK;
    Byte                     bytes[Disassembler::kMaxInstructionBytes];



    if (!m_trace.isOn)
    {
        return;
    }

    for (size_t i = 0; i < std::size (bytes); ++i)
    {
        bytes[i] = Peek (target, (Word) (pc + i));
    }

    hr = disassembler.DisassembleOne (pc, bytes, instruction);
    IGNORE_RETURN_VALUE (hr, S_OK);

    line = m_trace.withVideo
         ? std::format ("V{:03} H{:02} ", video.scanline, video.cycleInLine)
         : std::format ("{:08} ", target.GetCycleCount());

    line += std::format ("A={:02X} X={:02X} Y={:02X} SP={:02X} {}  {:04X}: {} {}\n",
                         registers.a, registers.x, registers.y, registers.sp,
                         AppleWinFormatter::FormatFlags (registers.p),
                         pc, instruction.mnemonic, instruction.operand);

    m_trace.lines += line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::FlushTrace
//
//  A failed write is remembered, so turning the trace off reports it even
//  when the write that failed was made at a stop, where no reply is sent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExecutionHandlers::FlushTrace (DebugSession & session)
{
    HRESULT  hr = S_OK;



    if (m_trace.isOn && !m_trace.lines.empty() && session.GetFileSystem() != nullptr)
    {
        hr = session.GetFileSystem()->WriteAllText (m_trace.path, m_trace.lines);

        if (FAILED (hr))
        {
            m_trace.hasFailed = true;
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ShowCycles
//
//  CYCLES [ABS|REL|PART]: the machine's count, the last run's, or the count
//  since RCC.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ShowCycles (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string  which = command.text;
    uint64_t     total = session.GetTarget().GetCycleCount();



    for (char & ch : which)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    if      (which.empty() || which == "ABS") { reply.data = CyclesData { total }; }
    else if (which == "REL")                  { reply.data = CyclesData { m_lastRunCycles }; }
    else if (which == "PART")                 { reply.data = CyclesData { session.GetTripCycles (total) }; }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "CYCLES takes ABS, REL or PART.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::MakeScript
//
//  TF ON and TF OFF, so a second run of the script changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ExecutionHandlers::MakeScript() const
{
    std::vector<std::string>  lines;
    std::string               name;



    lines.push_back (m_stopwatch.IsArmed() ? std::format ("STOPWATCH {:04X} {:04X}", m_stopwatch.GetStart(), m_stopwatch.GetStop()) : std::string ("STOPWATCH OFF"));
    lines.push_back (m_profile.IsOn() ? "PROFILE ON" : "PROFILE OFF");

    //  The name as it was typed, which may have quotes of its own.
    name = m_trace.name.starts_with ('"') ? m_trace.name : "\"" + m_trace.name + "\"";

    lines.push_back (m_trace.isOn ? std::format ("TF ON {}{}", name, m_trace.withVideo ? " v" : "") : std::string ("TF OFF"));

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ResetCycles
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ResetCycles (DebugSession & session, Reply & reply)
{
    session.SetCycleMarker (session.GetTarget().GetCycleCount());
    reply.data    = MessageData { { "Cycle counter reset. CYCLES PART now counts from here." } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::RunReverse
//
//  T-, P-, GU-, G- and LIVE: the machine moves through its recorded history
//  on the machine's own thread, and the landing is reported as a step's stop.
//  The machine must be paused, and the host must be recording.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::RunReverse (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    const DebugSession::ReverseRequester  & requester = session.GetReverseRequester();
    ReverseCommand                          reverse   = GetReverseCommand (command.verb);
    bool                                    isTaken   = false;



    if (session.GetRunState() != RunState::Paused)
    {
        reply.SetError (CommandStatus::Error, "machine running",
                        std::format ("{} moves the machine through its recorded history. Pause the machine first.", command.sourceName));
        return;
    }

    if (requester != nullptr)
    {
        isTaken = requester (reverse);
    }

    if (!isTaken)
    {
        reply.SetError (CommandStatus::NotAvailable, "no recorded history",
                        "Reverse execution is off. Turn recording on in the debugger's options.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::GetReverseCommand
//
////////////////////////////////////////////////////////////////////////////////

ReverseCommand ExecutionHandlers::GetReverseCommand (DebugVerb verb)
{
    switch (verb)
    {
    case DebugVerb::StepBackOver: return ReverseCommand::StepBackOver;
    case DebugVerb::StepBackOut:  return ReverseCommand::StepBackOut;
    case DebugVerb::ReverseGo:    return ReverseCommand::ReverseContinue;
    case DebugVerb::GoLive:       return ReverseCommand::GoLive;
    default:                      return ReverseCommand::StepBack;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Peek
//
////////////////////////////////////////////////////////////////////////////////

Byte ExecutionHandlers::Peek (IDebugTarget & target, Word address)
{
    Byte  value = 0;



    target.TryPeek (address, value);
    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::TrimSpaces
//
////////////////////////////////////////////////////////////////////////////////

std::string ExecutionHandlers::TrimSpaces (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t");
    size_t  last  = text.find_last_not_of (" \t");



    if (first == std::string::npos)
    {
        return std::string();
    }

    return text.substr (first, last - first + 1);
}
