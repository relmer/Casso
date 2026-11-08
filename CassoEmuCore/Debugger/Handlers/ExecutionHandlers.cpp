#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"

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
    case DebugVerb::ShowVideoInfo:     ShowVideoInfo     (session, reply);          return true;
    case DebugVerb::ShowBranchRecord:  ShowBranchRecord  (session, reply);          return true;
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
//  The instruction before this one is settled first, now that it has run or
//  an interrupt has been dispatched in its place. This one's state is taken
//  before it runs: its trace line, and the count the profile bills it from.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnInstruction (DebugSession & session, Word pc)
{
    IDebugTarget        & target  = session.GetTarget();
    PendingInstruction    pending;



    FeedKeys      (session);
    SettlePending (session);

    if (!m_profile.IsOn() && !m_trace.isOn)
    {
        return;
    }

    pending.pc          = pc;
    pending.opcode      = Peek (target, pc);
    pending.sp          = target.GetRegisters().sp;
    pending.startCycles = target.GetCycleCount();
    pending.isProfiled  = m_profile.IsOn();
    pending.isTraced    = m_trace.isOn;
    pending.traceStart  = m_trace.lines.size();

    if (m_trace.isOn)
    {
        m_trace.lines += FormatTraceLine (session, pc, m_trace.withVideo);
    }

    m_pending = pending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::OnRunStopped
//
//  The trace file is written at every stop, so a script that never turns
//  tracing off still leaves the file behind. The last instruction of the run
//  is settled here, since no next instruction will settle it.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnRunStopped (DebugSession & session, const StopEvent & stop)
{
    SettlePending (session);

    m_lastRunCycles = stop.cycles;
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
    reply.data = MessageData { { std::format ("Queued {} key{}. Keys waiting: {}.",
                                              command.values.size(), command.values.size() == 1 ? "" : "s", m_keys.size()) } };
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
//  BPV line[,len|:last] arms the session's one video break, which stops a
//  run once at the range. A range that ends before it begins, or runs past
//  the frame's last scanline, could never fire, and would keep every
//  instruction asked about for as long as it stayed armed.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BreakOnVideoLine (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    static constexpr uint32_t  kLastLine = VideoTiming::kScanlinesPerFrame - 1;
    Word                       last      = command.hasA2 ? command.a2 : command.a1;



    if (last < command.a1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("${:X}-${:X} ends before it begins.", command.a1, last));
        return;
    }

    if (last > kLastLine)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("A frame's scanlines are $0-${:X}, which is 0-{} in decimal.", kLastLine, kLastLine));
        return;
    }

    session.SetVideoBreak (command.a1, last);
    reply.data = MessageData { { std::format ("Breakpoint set on video scanlines {}-{}. It clears after it fires.", command.a1, last) } };
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
//  ExecutionHandlers::SettlePending
//
//  The hook reports an instruction before it runs, so what it did is known
//  at the next instruction, or at the stop for the last one. One that an
//  interrupt replaced did not run: it is neither billed nor traced, and is
//  reported again when it does run, after the handler returns. The dispatch
//  is no instruction, and is billed to none.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::SettlePending (DebugSession & session)
{
    if (!m_pending.has_value())
    {
        return;
    }

    if (!WasPreempted (session, *m_pending))
    {
        BillProfile (session, *m_pending);
    }
    else if (m_pending->isTraced && m_trace.isOn)
    {
        m_trace.lines.resize (m_pending->traceStart);
    }

    m_pending.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::WasPreempted
//
//  An interrupt dispatched in place of the instruction pushes three bytes,
//  the top two the instruction's own address, and goes to a vector. No
//  instruction does all three: BRK pushes the address two past itself.
//
////////////////////////////////////////////////////////////////////////////////

bool ExecutionHandlers::WasPreempted (DebugSession & session, const PendingInstruction & pending)
{
    static constexpr int  kByteBits = 8;
    IDebugTarget        & target    = session.GetTarget();
    Cpu6502Registers      registers = target.GetRegisters();
    Byte                  high      = Peek (target, (Word) (kStackPage + pending.sp));
    Byte                  low       = Peek (target, (Word) (kStackPage + (Byte) (pending.sp - 1)));
    Word                  pushed    = (Word) ((high << kByteBits) | low);
    bool                  isVector  = registers.pc == PeekWord (target, kIrqVector) || registers.pc == PeekWord (target, kNmiVector);



    return registers.sp == (Byte) (pending.sp - kInterruptPushes) && pushed == pending.pc && isVector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::BillProfile
//
//  The cycles the instruction took, from the machine's count, and the
//  penalties the CPU recorded for it. A count now below where it started was
//  reset under the instruction, which leaves its cost unknown, so it is not
//  billed.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::BillProfile (DebugSession & session, const PendingInstruction & pending)
{
    IDebugTarget  & target    = session.GetTarget();
    uint64_t        now       = target.GetCycleCount();
    Byte            penalties = target.GetLastPenalties();



    if (!pending.isProfiled || now < pending.startCycles)
    {
        return;
    }

    m_profile.Record (pending.pc, pending.opcode, (Byte) std::min (now - pending.startCycles, kMaxBilledCycles), penalties);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::SkipPendingBill
//
//  PROFILE OFF and PROFILE RESET: the instruction under way when either ran
//  is never billed.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::SkipPendingBill()
{
    if (m_pending.has_value())
    {
        m_pending->isProfiled = false;
    }
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
//  PROFILE ON|OFF|RESET|LIST [ADDR]|SAVE [file]. A bare PROFILE lists. A
//  file name with spaces is written in quotes.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Profile (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::vector<std::string>  words    = SplitWords (command.text);
    std::string               verb     = words.empty() ? std::string() : SymbolTable::ToUpper (words[0]);
    std::string               argument = words.size() > 1 ? words[1] : std::string();
    bool                      hasExtra = words.size() > 2;
    ProfileData               data;



    if (verb == "ON" && argument.empty())
    {
        m_profile.SetOn (true);
        reply.data = MessageData { { "Profiling on." } };
    }
    else if (verb == "OFF" && argument.empty())
    {
        m_profile.SetOn (false);
        SkipPendingBill();
        reply.data = MessageData { { "Profiling off." } };
    }
    else if (verb == "RESET" && argument.empty())
    {
        m_profile.Reset();
        SkipPendingBill();
        reply.data = MessageData { { "Profile reset." } };
    }
    else if (verb == "SAVE" && !hasExtra)
    {
        SaveProfile (session, argument.empty() ? kDefaultProfile : argument, reply);
    }
    else if ((verb.empty() || verb == "LIST") && !hasExtra && (argument.empty() || SymbolTable::ToUpper (argument) == "ADDR"))
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
//  written at each stop and when tracing ends; turned off between the slices
//  of a run, the instruction under way is settled first, so the file holds
//  it only if it ran.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ToggleTrace (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string  name;
    bool         withVideo = false;



    if (m_trace.isOn)
    {
        SettlePending (session);
        FlushTrace    (session);
        reply.data = MessageData { { std::format ("Trace off: {}", m_trace.name) } };
        m_trace    = TraceState();
        return;
    }

    for (const std::string & word : SplitWords (command.text))
    {
        if (word == "v" || word == "V") { withVideo = true; }
        else                            { name = word; }
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
//  ExecutionHandlers::FormatTraceLine
//
//  Cycles, the registers, the flags and the disassembly; with v, the video
//  scanline and horizontal position replace the cycle count. Built only
//  while tracing is on, since a run pays for it on every instruction.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExecutionHandlers::FormatTraceLine (DebugSession & session, Word pc, bool withVideo)
{
    IDebugTarget           & target    = session.GetTarget();
    Disassembler             disassembler (target.GetInstructionSet());
    Cpu6502Registers         registers = target.GetRegisters();
    VideoPosition            video     = target.GetVideoPosition();
    DisassembledInstruction  instruction;
    std::string              line;
    HRESULT                  hr        = S_OK;
    Byte                     bytes[Disassembler::kMaxInstructionBytes];



    for (size_t i = 0; i < std::size (bytes); ++i)
    {
        bytes[i] = Peek (target, (Word) (pc + i));
    }

    hr = disassembler.DisassembleOne (pc, bytes, instruction);
    IGNORE_RETURN_VALUE (hr, S_OK);

    line = withVideo
         ? std::format ("V{:03} H{:02} ", video.scanline, video.cycleInLine)
         : std::format ("{:08} ", target.GetCycleCount());

    line += std::format ("A={:02X} X={:02X} Y={:02X} SP={:02X} {}  {:04X}: {} {}\n",
                         registers.a, registers.x, registers.y, registers.sp,
                         AppleWinFormatter::FormatFlags (registers.p),
                         pc, instruction.mnemonic, instruction.operand);

    return line;
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
//  since RCC. A reset that took the machine's count below the RCC marker
//  started the count again, so PART is then the count since the reset.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::ShowCycles (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string  which = command.text;
    uint64_t     total = session.GetTarget().GetCycleCount();
    uint64_t     part  = (total >= m_cycleMarker) ? total - m_cycleMarker : total;



    for (char & ch : which)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    if      (which.empty() || which == "ABS") { reply.data = CyclesData { total }; }
    else if (which == "REL")                  { reply.data = CyclesData { m_lastRunCycles }; }
    else if (which == "PART")                 { reply.data = CyclesData { part }; }
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
    reply.data    = MessageData { { "Cycle counter reset. CYCLES PART now counts from here." } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Benchmark
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Benchmark (const DebugCommand & command, Reply & reply)
{
    reply.SetError (CommandStatus::NotAvailable, "command not available",
                    std::format ("{} is not available in this session.", command.sourceName));
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
//  ExecutionHandlers::PeekWord
//
////////////////////////////////////////////////////////////////////////////////

Word ExecutionHandlers::PeekWord (IDebugTarget & target, Word address)
{
    static constexpr int  kByteBits = 8;



    return (Word) (Peek (target, address) | (Peek (target, (Word) (address + 1)) << kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::SplitWords
//
//  Words split at whitespace, except that a word starting with a double
//  quote runs to the next one, spaces and all, and keeps both quotes. A
//  file name with spaces is written that way; ResolvePath drops the quotes.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ExecutionHandlers::SplitWords (const std::string & text)
{
    static constexpr const char * kSpace = " \t";
    std::vector<std::string>      words;
    size_t                        at     = text.find_first_not_of (kSpace);
    size_t                        end    = 0;



    while (at != std::string::npos)
    {
        end = (text[at] == '"') ? text.find ('"', at + 1) : text.find_first_of (kSpace, at);

        if (text[at] == '"' && end != std::string::npos)
        {
            ++end;
        }

        words.push_back (text.substr (at, (end == std::string::npos) ? std::string::npos : end - at));
        at = (end == std::string::npos) ? std::string::npos : text.find_first_not_of (kSpace, end);
    }

    return words;
}
