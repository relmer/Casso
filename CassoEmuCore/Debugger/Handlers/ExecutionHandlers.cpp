#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Disassembler.h"





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
//  tracing off still leaves the file behind.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::OnRunStopped (DebugSession & session, const StopEvent & stop)
{
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
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::RecordProfile (DebugSession & session, Word pc)
{
    IDebugTarget     & target    = session.GetTarget();
    const Microcode  * set       = target.GetInstructionSet();
    Byte               opcode    = Peek (target, pc);



    if (set == nullptr)
    {
        return;
    }

    if (!m_profile.hasStart)
    {
        m_profile.startCycles = target.GetCycleCount();
        m_profile.hasStart    = true;
    }

    ++m_profile.instructions;
    ++m_profile.opcodes[set[opcode].isLegal ? set[opcode].instructionName : "???"];
    ++m_profile.modes[set[opcode].isLegal ? GlobalAddressingMode::s_addressingModeName[set[opcode].globalAddressingMode] : "???"];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers::Profile
//
//  PROFILE [LIST|RESET|SAVE]: the counts sorted by count, cleared, or written
//  tab-separated to Profile.txt.
//
////////////////////////////////////////////////////////////////////////////////

void ExecutionHandlers::Profile (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string   verb  = command.text;
    ProfileData   data;
    std::string   text;
    IFileSystem * files = session.GetFileSystem();
    HRESULT       hr    = S_OK;



    for (char & ch : verb)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    if (verb == "RESET")
    {
        m_profile = ProfileState();
        reply.data = MessageData { { "Profile reset." } };
        return;
    }

    data.instructions = m_profile.instructions;
    data.cycles       = m_profile.hasStart ? session.GetTarget().GetCycleCount() - m_profile.startCycles : 0;

    for (const auto & [name, count] : m_profile.opcodes) { data.opcodes.push_back ({ name, count }); }
    for (const auto & [name, count] : m_profile.modes)   { data.modes.push_back   ({ name, count }); }

    std::stable_sort (data.opcodes.begin(), data.opcodes.end(), [] (const ProfileEntry & a, const ProfileEntry & b) { return a.count > b.count; });
    std::stable_sort (data.modes.begin(),   data.modes.end(),   [] (const ProfileEntry & a, const ProfileEntry & b) { return a.count > b.count; });

    if (verb == "SAVE")
    {
        if (files == nullptr)
        {
            reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
            return;
        }

        text = std::format ("Instructions\t{}\nCycles\t{}\n", data.instructions, data.cycles);

        for (const ProfileEntry & entry : data.opcodes) { text += std::format ("{}\t{}\n", entry.name, entry.count); }
        for (const ProfileEntry & entry : data.modes)   { text += std::format ("{}\t{}\n", entry.name, entry.count); }

        hr = files->WriteAllText (session.ResolvePath (kDefaultProfile), text);

        if (FAILED (hr))
        {
            reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", kDefaultProfile));
            return;
        }

        reply.data = MessageData { { std::format ("Saved the profile to {}.", kDefaultProfile) } };
        return;
    }

    if (!verb.empty() && verb != "LIST")
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PROFILE takes LIST, RESET or SAVE.");
        return;
    }

    reply.data = data;
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
