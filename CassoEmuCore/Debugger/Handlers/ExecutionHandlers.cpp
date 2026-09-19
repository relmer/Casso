#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/DebugSession.h"
#include "Disassembler.h"





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
    case DebugVerb::ShowVideoInfo:     ShowVideoInfo     (session, reply);          return true;
    case DebugVerb::ShowBranchRecord:  ShowBranchRecord  (reply);                   return true;
    case DebugVerb::TraceToFile:       ToggleTrace       (session, command, reply); return true;
    case DebugVerb::Profile:           Profile           (session, command, reply); return true;
    case DebugVerb::ShowCycles:        ShowCycles        (session, command, reply); return true;
    case DebugVerb::ResetCycles:       ResetCycles       (session, reply);          return true;
    case DebugVerb::Benchmark:
    case DebugVerb::ExitBenchmark:     Benchmark         (command, reply);          return true;
    default:                                                                        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnInstruction
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnInstruction (DebugSession & session, Word pc)
{
    FeedKeys      (session);
    RecordBranch  (session, pc);
    RecordProfile (session, pc);
    RecordTrace   (session, pc);
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
    BillProfile (session);

    m_lastRunCycles = stop.cycles;
    m_previousPc.reset();
    FlushTrace (session);
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
    Word              pc     = target.GetRegisters().pc;
    size_t            length = disassembler.GetLength (Peek (target, pc));
    DisassemblyData   data;
    HRESULT           hr     = S_OK;



    for (size_t i = 0; i < length; ++i)
    {
        if (!target.TryPoke ((Word) (pc + i), kNop))
        {
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
//  wait in the queue and are fed as the guest clears the strobe during a
//  debugger-driven run.
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
    reply.data = MessageData { { std::format ("Queued {} keys; {} waiting.", command.values.size(), m_keys.size()) } };
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
//  BPV vpos[,len] stops when the scanline enters the range, once.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BreakOnVideoLine (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word  last = command.hasA2 ? command.a2 : command.a1;



    session.SetVideoBreak (command.a1, last);
    reply.data = MessageData { { std::format ("Break when the video scanline is {}-{}; the break clears when it fires.", command.a1, last) } };
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

void ExecutionHandlers::ShowBranchRecord (Reply & reply) const
{
    reply.data = BranchRecordData { m_lastBranch };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::RecordBranch
//
//  When the instruction that just ran was a branch, jump, call, return or
//  BRK and the next instruction is not the one after it, that instruction
//  transferred control.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::RecordBranch (DebugSession & session, Word pc)
{
    IDebugTarget  & target      = session.GetTarget();
    Disassembler    disassembler (target.GetInstructionSet());
    bool            isCmos      = target.GetCpuKind() == DebugCpuKind::M65C02;
    Word            fallThrough = 0;



    if (target.GetInstructionSet() == nullptr)
    {
        return;
    }

    if (m_previousPc.has_value())
    {
        fallThrough = (Word) (*m_previousPc + disassembler.GetLength (m_previousOpcode));

        if (pc != fallThrough && IsControlTransfer (m_previousOpcode, isCmos))
        {
            m_lastBranch = m_previousPc;
        }
    }

    m_previousPc     = pc;
    m_previousOpcode = Peek (target, pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::IsControlTransfer
//
//  The eight conditional branches are xxx10000; BRA is $80 on the 65C02.
//
////////////////////////////////////////////////////////////////////////////////

bool ExecutionHandlers::IsControlTransfer (Byte opcode, bool isCmos)
{
    return (opcode & kBranchMask) == kBranchBits ||
           opcode == kJsr || opcode == kJmpAbsolute || opcode == kJmpIndirect ||
           opcode == kRts || opcode == kRti || opcode == kBrk ||
           (isCmos && opcode == kBraCmos);
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

    cycles    = target.GetCycleCount() - m_profilePending->startCycles;
    penalties = target.GetLastPenalties();

    m_profile.Record (m_profilePending->pc, m_profilePending->opcode, (Byte) std::min (cycles, kMaxBilledCycles), penalties);
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

void ExecutionHandlers::BuildProfile (DebugSession & session, bool isByAddress, ProfileData & data) const
{
    using ModeKey = std::pair<std::string, std::string>;

    const Microcode                   * set    = session.GetTarget().GetInstructionSet();
    std::map<ModeKey, ProfileEntry>     groups;
    ProfileAddressEntry                 entry;
    SymbolTableId                       table  = SymbolTableId::Main;



    data.isOn         = m_profile.IsOn();
    data.isByAddress  = isByAddress;
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

    if (!isByAddress)
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
//  ExecutionHandlers::SaveProfile
//
//  The same rows PROFILE LIST and PROFILE LIST ADDR print, one after the
//  other.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::SaveProfile (DebugSession & session, const std::string & name, Reply & reply) const
{
    IFileSystem  * files     = session.GetFileSystem();
    Reply          byOpcode;
    Reply          byAddress;
    ProfileData    opcodeData;
    ProfileData    addressData;
    std::string    text;
    HRESULT        hr        = S_OK;



    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    BuildProfile (session, false, opcodeData);
    BuildProfile (session, true,  addressData);

    byOpcode.data  = opcodeData;
    byAddress.data = addressData;
    AppleWinFormatter::Format (byOpcode);
    AppleWinFormatter::Format (byAddress);

    for (const std::string & line : byOpcode.text)  { text += line + "\n"; }
    for (const std::string & line : byAddress.text) { text += line + "\n"; }

    hr = files->WriteAllText (session.ResolvePath (name), text);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    reply.data = MessageData { { std::format ("Saved the profile to {}.", name) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Profile
//
//  PROFILE ON|OFF|RESET|LIST [ADDR]|SAVE [file]. A bare PROFILE lists.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Profile (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::istringstream  stream (command.text);
    std::string         verb;
    std::string         argument;
    std::string         extra;
    ProfileData         data;



    stream >> verb >> argument >> extra;
    verb = SymbolTable::ToUpper (verb);

    if (verb == "ON" && argument.empty())
    {
        m_profile.SetOn (true);
        reply.data = MessageData { { "Profiling on." } };
    }
    else if (verb == "OFF" && argument.empty())
    {
        m_profile.SetOn (false);
        m_profilePending.reset();
        reply.data = MessageData { { "Profiling off." } };
    }
    else if (verb == "RESET" && argument.empty())
    {
        m_profile.Reset();
        m_profilePending.reset();
        reply.data = MessageData { { "Profile reset." } };
    }
    else if (verb == "SAVE" && extra.empty())
    {
        SaveProfile (session, argument.empty() ? kDefaultProfile : argument, reply);
    }
    else if ((verb.empty() || verb == "LIST") && extra.empty() && (argument.empty() || SymbolTable::ToUpper (argument) == "ADDR"))
    {
        BuildProfile (session, !argument.empty(), data);
        reply.data = data;
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PROFILE takes ON, OFF, RESET, LIST [ADDR] or SAVE [file].");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ToggleTrace
//
//  TF ["file"] [v] turns the trace file on, or off if it is on. Each
//  instruction a debugger-driven run executes becomes one line. The file is
//  written at each stop and when tracing ends.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ToggleTrace (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::istringstream  stream (command.text);
    std::string         token;
    std::string         name;
    bool                withVideo = false;



    if (m_trace.isOn)
    {
        FlushTrace (session);
        reply.data = MessageData { { std::format ("Trace off: {}", m_trace.name) } };
        m_trace    = TraceState();
        return;
    }

    while (stream >> token)
    {
        if (token == "v" || token == "V") { withVideo = true; }
        else                              { name = token; }
    }

    if (session.GetFileSystem() == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    m_trace.isOn      = true;
    m_trace.withVideo = withVideo;
    m_trace.name      = name.empty() ? kDefaultTrace : name;
    m_trace.path      = session.ResolvePath (m_trace.name);
    reply.data        = MessageData { { std::format ("Trace on: {}", m_trace.name) } };
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
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::FlushTrace (DebugSession & session)
{
    HRESULT  hr = S_OK;



    if (m_trace.isOn && session.GetFileSystem() != nullptr)
    {
        hr = session.GetFileSystem()->WriteAllText (m_trace.path, m_trace.lines);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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
    else if (which == "PART")                 { reply.data = CyclesData { total - m_cycleMarker }; }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "CYCLES takes ABS, REL or PART.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::ResetCycles
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ResetCycles (DebugSession & session, Reply & reply)
{
    m_cycleMarker = session.GetTarget().GetCycleCount();
    reply.data    = MessageData { { "Cycle counter reset; CYCLES PART counts from here." } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Benchmark
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Benchmark (const DebugCommand & command, Reply & reply)
{
    reply.SetError (CommandStatus::NotAvailable, "command not available",
                    std::format ("{} needs a host clock, which this session does not have.", command.sourceName));
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
