#include "Pch.h"

#include "Cpu.h"
#include "CpuOperations.h"
#include "Group00.h"
#include "Group01.h"
#include "Group10.h"
#include "GroupMisc.h"
#include "InstructionSetBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu
//
////////////////////////////////////////////////////////////////////////////////

Cpu::Cpu()
{
    InitializeInstructionSet();

#ifdef _DEBUG
    // Preserve the always-on illegal-opcode look-back in Debug builds even
    // when --trace is not passed. The --trace switch re-enables with a far
    // larger ring at runtime.
    EnableTrace (kTraceDefaultLookback);
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::Reset()
{
    status.status          = 0;
    status.flags.alwaysOne = 1;

    A  = 0;
    X  = 0;
    Y  = 0;
    SP = 0xFF;
    PC = 0;

    m_hasLastBranch   = false;
    m_hasLastBranchTo = false;

    std::fill (memory.begin(), memory.end(), Byte (0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnableTrace
//
//  Allocate (or free) the execution-trace ring. Capacity is in entries;
//  0 disables tracing and releases the buffer. Resets head/count.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::EnableTrace (size_t capacity)
{
    m_traceHead        = 0;
    m_traceCount       = 0;
    m_pendingTraceIntr = kTraceIntrNone;

    if (capacity == 0)
    {
        m_traceEnabled  = false;
        m_traceCapacity = 0;
        m_trace.clear();
        m_trace.shrink_to_fit();
        UpdateFetchObserved();
        return;
    }

    m_traceCapacity = capacity;
    m_trace.assign (capacity, TraceEntry {});
    m_traceEnabled  = true;
    UpdateFetchObserved();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOpcodeWatch
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher)
{
    bool  isOn = opcodes != nullptr && watcher != nullptr;



    m_watchOpcodes = isOn ? opcodes : nullptr;
    m_watcher      = isOn ? watcher : nullptr;
    m_watchFetch   = isOn ? opcodes : s_kNoOpcodes.data();
    UpdateFetchObserved();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportWatchedFetch
//
//  The fetch after a watched opcode is looked up in a table that holds every
//  opcode, so the watcher sees the registers that opcode left.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::ReportWatchedFetch (Byte opcode)
{
    m_watchFetch = m_watchOpcodes[opcode] ? s_kAllOpcodes.data() : m_watchOpcodes;
    m_watcher->OnWatchedFetch (PC, SP, opcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekForTrace
//
//  Read one byte for the trace: the byte the CPU would fetch, without
//  disturbing anything.
//
//  It goes through the SAME read-page table the instruction fetch uses,
//  because memory[] is the wrong buffer the moment an MMU re-points a page.
//  On the //e and //c the MMU rebinds all of $0000-$BFFF from RAMRD / RAMWRT /
//  ALTZP / 80STORE, so a trace that read memory[] printed operands from main
//  RAM while the CPU executed from aux -- and a disassembly whose operands
//  belong to a different bank is worse than none, because it reads as fact.
//  This was visible in a //e trace of a stack-page disk loader, where a
//  branch that plainly jumped backwards printed a forward operand.
//
//  A null page is I/O ($C000-$CFFF) or unmapped space, and those fall back to
//  memory[] rather than the bus ON PURPOSE: a bus read of $C0xx toggles soft
//  switches. Recording the machine must never change it, so the trace accepts
//  a stale byte there instead of a side effect.
//
////////////////////////////////////////////////////////////////////////////////

Byte Cpu::PeekForTrace (Word address) const
{
    Byte * const *  pages = (m_tracePeekPages != nullptr) ? m_tracePeekPages : m_readPages;



    if (pages != nullptr)
    {
        const Byte *  page = pages[address >> 8];

        if (page != nullptr)
        {
            return page[address & 0xFF];
        }
    }

    return memory[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePush
//
//  Snap the current pre-execution CPU state into the ring buffer.
//  Called at the top of StepOne so the entry captures the instruction
//  that is about to run -- the head-1 entry on dump is the faulting
//  instruction itself; head-2 is its predecessor; and so on.
//
//  `opcode` is the byte StepOne actually fetched via ReadByte (the
//  bus-routed value, which honors ROM/language-card/soft-switch
//  banking). It must NOT be re-read from the raw Cpu::memory[] array,
//  which only backs $00-$BF RAM and is stale for ROM/$Cxxx/$Dxxx-$FFFF
//  -- doing so makes the disassembly lie in exactly the banked regions
//  a fault trace most needs to be correct about. The operand bytes go
//  through PeekForTrace for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::TracePush (Byte opcode)
{
    TraceEntry &  e = m_trace[m_traceHead];



    e.cycles    = ((m_traceCycles != nullptr) ? *m_traceCycles : 0) + ((m_traceCycleBase != nullptr) ? *m_traceCycleBase : 0);
    e.pc        = PC;
    e.opcode    = opcode;
    e.op1       = PeekForTrace ((Word) (PC + 1));
    e.op2       = PeekForTrace ((Word) (PC + 2));
    e.a         = A;
    e.x         = X;
    e.y         = Y;
    e.sp        = SP;
    e.p         = status.status;
    e.hasAccess = false;

    // Move any pending interrupt-dispatch tag onto this entry: TracePush runs
    // for the handler's first instruction right after the vector was taken.
    e.intr             = m_pendingTraceIntr;
    m_pendingTraceIntr = kTraceIntrNone;

    // A compare rather than %: this runs for every instruction while tracing,
    // and a 64-bit divide was the costliest thing in it.
    m_traceHead++;

    if (m_traceHead == m_traceCapacity)
    {
        m_traceHead = 0;
    }

    m_traceCount++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTraceSize
//
//  The entries the ring holds: every one pushed until it wraps, then its
//  capacity.
//
////////////////////////////////////////////////////////////////////////////////

size_t Cpu::GetTraceSize() const
{
    return (m_traceCount < (uint64_t) m_traceCapacity) ? (size_t) m_traceCount : m_traceCapacity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetTraceEntry
//
//  The entry `index` places from the oldest the ring holds.
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::TryGetTraceEntry (size_t index, TraceEntry & entry) const
{
    size_t  size  = GetTraceSize();
    size_t  start = (m_traceCount > (uint64_t) m_traceCapacity) ? m_traceHead : 0;



    if (index >= size)
    {
        return false;
    }

    entry = m_trace[(start + index) % m_traceCapacity];
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordTraceAccess
//
//  A bus access, stored on the newest entry as its last access. Only the
//  instruction's own data accesses count, so three kinds are left out:
//
//    - the reads of the instruction's own bytes, at pc to pc+2;
//    - the next instruction's opcode fetch, which reads at PC before that
//      instruction's entry exists;
//    - an interrupt's stack writes and vector reads, which happen while a
//      tag is pending and belong to no instruction.
//
//  A data read that falls on those addresses is rare (code reading its own
//  bytes) and is left out with them.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::RecordTraceAccess (Word address, Byte data, bool isWrite)
{
    static constexpr Word  kInstructionSpan = 3;
    TraceEntry           * e                = nullptr;



    if (!m_traceEnabled || m_traceCount == 0 || m_pendingTraceIntr != kTraceIntrNone)
    {
        return;
    }

    e = &m_trace[(m_traceHead + m_traceCapacity - 1) % m_traceCapacity];

    if (!isWrite && (address == PC || (Word) (address - e->pc) < kInstructionSpan))
    {
        return;
    }

    e->hasAccess     = true;
    e->accessIsWrite = isWrite;
    e->accessData    = data;
    e->accessAddress = address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DumpInstructionTrace
//
//  Dumps the ring buffer newest-first via DEBUGMSG. Format mirrors
//  conventional 6502 disassemblers so the output is easy to skim:
//
//    [-N]  PC=$XXXX  op=$XX (NAME)   A=$XX X=$XX Y=$XX SP=$XX P=$XX
//
//  Pre-faces with a banner so the dump is easy to spot in DbgView.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::DumpInstructionTrace (Byte faultOpcode, Word faultPC) const
{
    size_t  i = 0;



    // Short newest-first look-back to stderr -- enough to see the
    // JMP/JSR/RTS chain that landed PC on the bad byte. The full ring
    // (which may be millions of entries under --trace) is written
    // separately by DumpTraceToFile, so cap this quick dump.
    static constexpr size_t  kMaxLookback = 256;

    size_t  total   = (m_traceCount < (uint64_t) m_traceCapacity) ? (size_t) m_traceCount
                                                                  : m_traceCapacity;

    if (total > kMaxLookback)
    {
        total = kMaxLookback;
    }

    DEBUGMSG (L"\n");
    DEBUGMSG (L"[Casso] === CPU illegal-opcode fault ===\n");
    DEBUGMSG (L"[Casso] Fault: opcode $%02X at PC=$%04X\n",
              faultOpcode, faultPC);
    DEBUGMSG (L"[Casso] Trace (newest-first, %zu entries):\n", total);

    for (i = 0; i < total; i++)
    {
        // head currently points one PAST the most recent push, so the
        // newest entry is (head - 1) mod size. Step backward from there.
        const TraceEntry &  e        = m_trace[(m_traceHead + m_traceCapacity - 1 - i) % m_traceCapacity];
        const char *        opName   = instructionSet[e.opcode].instructionName != nullptr
                                       ? instructionSet[e.opcode].instructionName
                                       : "???";

        DEBUGMSG (L"[Casso] [-%03zu] PC=$%04X  op=$%02X (%hs)  "
                  L"A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X\n",
                  i, (unsigned) e.pc, (unsigned) e.opcode, opName,
                  (unsigned) e.a, (unsigned) e.x, (unsigned) e.y,
                  (unsigned) e.sp, (unsigned) e.p);
    }

    DEBUGMSG (L"[Casso] === end trace ===\n");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DumpTraceToFile
//
//  Write the recorded ring to `path` as text, oldest-first (chronological).
//  One instruction per line:
//
//    00000001  PC=$XXXX  op=$XX OPS=$XX $XX (NAME)  A=$XX X=$XX Y=$XX SP=$XX P=$XX
//
//  `preamble` follows the title line verbatim; the emulator puts the power-on
//  seed there. `onProgress` (if set) is called every kProgressStride entries and once
//  at completion with (entriesWritten, totalEntries). CassoCore owns no UI;
//  the caller drives any progress dialog.
//
//  The user asks for this dump explicitly, so a failure has to say which one
//  it was: E_INVALIDARG for an unopenable path, E_UNEXPECTED for a trace ring
//  that was never allocated (--trace not on), E_FAIL for a write that started
//  and then failed. A bool collapsed all three into "nothing happened".
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Cpu::DumpTraceToFile (const std::wstring & path,
                              const std::string  & preamble,
                              const std::function<void (uint64_t, uint64_t)> & onProgress) const
{
    static constexpr uint64_t  kProgressStride = 100000;
    size_t                     start           = 0;
    bool                       isOpen          = false;
    bool                       wrote           = false;
    char                       line[160];



    HRESULT        hr    = S_OK;
    std::ofstream  out (path, std::ios::binary | std::ios::trunc);
    uint64_t       total = (m_traceCount < (uint64_t) m_traceCapacity) ? m_traceCount
                                                                       : (uint64_t) m_traceCapacity;

    isOpen = out.is_open();

    CBREx (isOpen,               E_INVALIDARG);
    CBREx (m_traceCapacity != 0, E_UNEXPECTED);

    {
        uint64_t  irqCount = 0;
        uint64_t  nmiCount = 0;

        // When the ring has wrapped, the oldest surviving entry sits at the
        // current head; otherwise it starts at index 0.
        start = (m_traceCount > (uint64_t) m_traceCapacity) ? m_traceHead : 0;

        out << "Casso CPU execution trace -- " << total << " instructions (oldest first)\n";
        out << preamble;

        // Pre-scan for the interrupt-rate summary. An interrupt storm (stuck
        // IRQ line, unacknowledged source, an ISR that never returns) is
        // otherwise invisible in a flat instruction trace, so surface the rate
        // up top. Cheap -- one pass reading a single byte per entry, dwarfed by
        // the write below.

        for (uint64_t i = 0; i < total; i++)
        {
            Byte  kind = m_trace[(start + (size_t) i) % m_traceCapacity].intr;

            if      (kind == kTraceIntrIrq) { ++irqCount; }
            else if (kind == kTraceIntrNmi) { ++nmiCount; }
        }

        {
            char  hdr[128];
            int   hn = (irqCount > 0)
                     ? std::snprintf (hdr, sizeof (hdr),
                           "interrupts: %llu IRQ, %llu NMI  (1 IRQ per %llu instrs, ~%.1f/frame)\n",
                           (unsigned long long) irqCount, (unsigned long long) nmiCount,
                           (unsigned long long) (total / irqCount),
                           (double) irqCount * 17030.0 / ((double) total * 3.0))
                     : std::snprintf (hdr, sizeof (hdr),
                           "interrupts: %llu IRQ, %llu NMI\n",
                           (unsigned long long) irqCount, (unsigned long long) nmiCount);

            if (hn > 0)
            {
                out.write (hdr, hn);
            }
        }

        for (uint64_t i = 0; i < total; i++)
        {
            size_t              index = (start + (size_t) i) % m_traceCapacity;
            const TraceEntry &  e     = m_trace[index];
            const char *        name  = instructionSet[e.opcode].instructionName != nullptr
                                        ? instructionSet[e.opcode].instructionName
                                        : "???";
            const char *        mark  = (e.intr == kTraceIntrIrq) ? "  [IRQ]"
                                      : (e.intr == kTraceIntrNmi) ? "  [NMI]"
                                      :                             "";

            int  n = std::snprintf (line, sizeof (line),
                                    "%08llu  PC=$%04X  op=$%02X OPS=$%02X $%02X (%s)  "
                                    "A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X%s\n",
                                    (unsigned long long) i,
                                    (unsigned) e.pc, (unsigned) e.opcode,
                                    (unsigned) e.op1, (unsigned) e.op2, name,
                                    (unsigned) e.a, (unsigned) e.x, (unsigned) e.y,
                                    (unsigned) e.sp, (unsigned) e.p, mark);
            if (n > 0)
            {
                out.write (line, n);
            }

            if (onProgress && (i % kProgressStride) == 0)
            {
                onProgress (i, total);
            }
        }

        out.flush();

        if (onProgress)
        {
            onProgress (total, total);
        }

        // Distinct from the open failure above: the file opened and we wrote
        // into it, then the stream went bad -- a full disk or a vanished share,
        // not a bad path. E_FAIL is the CBR default, so no -Ex here.
        wrote = out.good();

        CBR (wrote);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepOne
//
//  Executes one instruction and leaves its exact cycle cost in m_lastCycles,
//  which the host loop reads to keep emulated time honest. Timing is why this
//  is more than fetch-decode-execute: two penalties are not in the table.
//
//  Page-crossing (+1) applies to indexed READS only. Stores and read-modify-
//  write instructions always take the extra cycle -- the hardware cannot know
//  whether the page crossed until it has read, and it must write regardless --
//  so their cost is already baked into baseCycles and adding it here would
//  double-count. That is what the long isReadOp exclusion list is for.
//
//  The 65C02's abs,X shifts and rotates are the one exception, and they carry
//  it per instruction rather than by operation: the CMOS part made that write
//  conditional, so those four pay the cycle only on a real crossing while their
//  NMOS twins pay it always. Same opcode, same operation, different timing.
//
//  ZeroPageIndirectY needs its base recovered as effectiveAddress - Y, because
//  unlike AbsoluteX/Y the base was never a literal in the instruction; it came
//  from the zero-page pointer.
//
//  Branch penalties (+1 taken, +1 more crossing a page) are NOT charged here.
//  They belong to the branch operations, which are the only code that knows
//  whether the branch was taken: this used to infer it from PC having moved,
//  and a taken branch with a zero displacement moves PC nowhere.
//
//  An illegal opcode is a 2-cycle NOP that keeps running rather than an
//  assert: real software executes undocumented NMOS ops this table does not
//  carry yet (GH #52), and trapping would break titles that work on hardware.
//
////////////////////////////////////////////////////////////////////////////////
void Cpu::StepOne()
{
    Word               start       = PC;
    Byte               opcode      = ReadByte (PC);
    const Microcode  & microcode   = instructionSet[opcode];
    OperandInfo        operandInfo = { 0 };
    Word               next        = 0;



    m_instructionStart = start;

    if (m_fetchTable != nullptr && m_fetchTable[start >> 8] != nullptr)
    {
        m_fetchTable[start >> 8][(start & 0xFF) >> 3] |= (uint8_t) (1u << (start & 7));
    }

    if (m_isFetchObserved)
    {
        ObserveFetch (opcode);
    }

    if (!microcode.isLegal)
    {
        DEBUGMSG (L"Illegal opcode $%02X at PC=$%04X\n", opcode, PC);

        if (m_traceEnabled)
        {
            DumpInstructionTrace (opcode, PC);
        }

        // Do NOT break into the debugger on an illegal / not-yet-implemented
        // opcode: real software (e.g. Space Quarks) legitimately executes NMOS
        // undocumented ops we have not added yet (GH #52), and the DEBUGMSG above
        // already records the opcode + PC. Treat it as a 2-cycle single-byte NOP
        // and keep running. Uncomment the assert to break in while tracking down
        // a specific illegal-opcode fault.
        // ASSERT (false);

        m_lastCycles    = 2;
        m_lastPenalties = 0;
        ++PC;

        return;
    }

    m_lastCycles    = microcode.baseCycles;
    m_lastPenalties = 0;

    FetchOperand (microcode, operandInfo);
    ++PC;
    next = PC;

    // Page-crossing penalty for indexed reads (+1 cycle).
    // Stores and RMW always pay the penalty (baked into baseCycles).
    bool isReadOp =
        microcode.operation != Microcode::Store              &&
        microcode.operation != Microcode::ShiftLeft          &&
        microcode.operation != Microcode::ShiftRight         &&
        microcode.operation != Microcode::RotateLeft         &&
        microcode.operation != Microcode::RotateRight        &&
        microcode.operation != Microcode::Decrement          &&
        microcode.operation != Microcode::DecrementAndCompare &&
        microcode.operation != Microcode::Increment          &&
        microcode.operation != Microcode::StoreAccumulatorAndX &&
        microcode.operation != Microcode::ShiftLeftAndOr     &&
        microcode.operation != Microcode::RotateLeftAndAnd   &&
        microcode.operation != Microcode::ShiftRightAndXor   &&
        microcode.operation != Microcode::RotateRightAndAdd  &&
        microcode.operation != Microcode::IncrementAndSubtract &&
        microcode.operation != Microcode::StoreZero          &&
        microcode.operation != Microcode::TestAndSetBits     &&
        microcode.operation != Microcode::TestAndResetBits   &&
        microcode.operation != Microcode::ResetMemoryBit     &&
        microcode.operation != Microcode::SetMemoryBit;

    // The 65C02's abs,X shifts and rotates are read-modify-writes that pay the
    // crossing cycle only when the page actually crosses, so the operation-based
    // answer above is wrong for them and they say so per instruction.
    if (isReadOp || microcode.crossingAPageCostsACycle)
    {
        if (microcode.globalAddressingMode == GlobalAddressingMode::AbsoluteX  ||
            microcode.globalAddressingMode == GlobalAddressingMode::AbsoluteY  ||
            microcode.globalAddressingMode == GlobalAddressingMode::ZeroPageIndirectY)
        {
            Word baseAddr = operandInfo.location;

            if (microcode.globalAddressingMode == GlobalAddressingMode::ZeroPageIndirectY)
            {
                baseAddr = (Word) (operandInfo.effectiveAddress - Y);
            }

            if ((baseAddr & 0xFF00) != (operandInfo.effectiveAddress & 0xFF00))
            {
                m_lastCycles++;
                m_lastPenalties |= kPenaltyPageCross;
            }
        }
    }

    ExecuteInstruction (microcode, operandInfo);

    // A taken branch or a jump to the very next instruction leaves PC where it
    // would have been anyway, so PC alone does not tell a transfer apart.
    bool isJump =
        microcode.operation == Microcode::Jump                 ||
        microcode.operation == Microcode::JumpSubroutine       ||
        microcode.operation == Microcode::ReturnFromSubroutine ||
        microcode.operation == Microcode::ReturnFromInterrupt  ||
        microcode.operation == Microcode::Break                ||
        microcode.operation == Microcode::BreakCmos;

    if (PC != next || isJump || (m_lastPenalties & kPenaltyBranchTaken) != 0)
    {
        m_lastBranchFrom  = start;
        m_hasLastBranch   = true;
        m_lastBranchTo    = PC;
        m_hasLastBranchTo = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintSingleStepInfo
//
//  One trace line per instruction: registers, flags, address, opcode bytes and
//  disassembly, in fixed columns so a run diffs cleanly against a reference
//  trace -- which is how this gets used.
//
//  The flags[][] pair is a lookup rather than seven conditionals: index 0 is
//  all dots, index 1 the letters, so a bool selects the row and the flag's
//  position selects the column. A clear flag shows '.' in the same column its
//  letter would occupy, keeping the field a constant width.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PrintSingleStepInfo (Word initialPC, Byte opcode, const OperandInfo & operandInfo)
{
    static constexpr char flags[][8] =
    {
        ".......",
        "CZIDBVN"
    };



    // Print the registers and the opcode byte
    std::printf ("SP: %02x  A: %04X  X: %04X  Y: %04X  %c%c%c%c%c%c%c    [%04X] %02X ",
                 SP,
                 A,
                 X,
                 Y,
                 flags[status.flags.carry][0],
                 flags[status.flags.zero][1],
                 flags[status.flags.interruptDisable][2],
                 flags[status.flags.decimal][3],
                 flags[status.flags.brk][4],
                 flags[status.flags.overflow][5],
                 flags[status.flags.negative][6],
                 initialPC,
                 opcode);

    PrintOperandBytes (initialPC, opcode);

    std::printf ("%s ", instructionSet[opcode].instructionName);

    PrintOperandAndComment (opcode, operandInfo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintOperandBytes
//
//  The raw operand bytes of an instruction, blank-padded to a fixed width so
//  the disassembly after them stays in column no matter how many bytes the
//  addressing mode has.
//
//  Grouped by SIZE rather than listed per mode: every mode taking one operand
//  byte prints the same way, so the cases fall through to three bodies --
//  none, one byte, two bytes.
//
//  Reads go through ReadByte at PC+1 / PC+2 rather than through operandInfo,
//  so this shows what is actually in memory, including for an instruction
//  whose operand was never decoded.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PrintOperandBytes (Word initialPC, Byte opcode)
{
    switch (instructionSet[opcode].globalAddressingMode)
    {
        case GlobalAddressingMode::Accumulator:
        case GlobalAddressingMode::SingleByteNoOperand:
            std::printf ("             ");
            break;

        case GlobalAddressingMode::Immediate:
        case GlobalAddressingMode::ZeroPage:
        case GlobalAddressingMode::ZeroPageIndirectY:
        case GlobalAddressingMode::ZeroPageXIndirect:
        case GlobalAddressingMode::ZeroPageX:
        case GlobalAddressingMode::ZeroPageY:
            std::printf ("%02X           ", ReadByte (static_cast<Word> (initialPC + 1)));
            break;

        case GlobalAddressingMode::Absolute:
        case GlobalAddressingMode::AbsoluteX:
        case GlobalAddressingMode::AbsoluteY:
        case GlobalAddressingMode::JumpAbsolute:
        case GlobalAddressingMode::JumpIndirect:
        case GlobalAddressingMode::Relative:
            std::printf ("%02X %02X        ", ReadByte (static_cast<Word> (initialPC + 1)), ReadByte (static_cast<Word> (initialPC + 2)));
            break;

        default:
            ASSERT (false);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintOperandAndComment
//
//  Disassembly half of the single-step debug trace: renders the operand in the
//  syntax its addressing mode would be written in, plus the value actually
//  fetched.
//
//  The resolved value is the point -- `$1234,X ; $07` shows both what the
//  source said and what it came to at run time, which is the thing a stepping
//  session needs and static disassembly cannot give.
//
//  Illegal opcodes print nothing: their operand bytes were never decoded, so
//  any rendering would be invented.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PrintOperandAndComment (Byte opcode, const OperandInfo & operandInfo)
{
    if (!instructionSet[opcode].isLegal)
    {
        return;
    }

    // print the operand and comment if applicable
    switch (instructionSet[opcode].globalAddressingMode)
    {
        case GlobalAddressingMode::Absolute:
            printf ("$%04X   ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::AbsoluteX:
            printf ("$%04X,X ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::AbsoluteY:
            printf ("$%04X,Y ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::Immediate:
            printf ("#$%02X", operandInfo.operand);
            break;

        case GlobalAddressingMode::JumpAbsolute:
            printf ("$%04X", operandInfo.location);
            break;

        case GlobalAddressingMode::JumpIndirect:
            printf ("($%04X) ; $%04X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::Relative:
            printf ("$%02X     ; $%04X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPage:
            printf ("$%02X     ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageXIndirect:
            printf ("($%02X,X) ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageIndirectY:
            printf ("($%02X),Y ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageX:
            printf ("$%02X,X   ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageY:
            printf ("$%02X,Y   ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadsTarget
//
//  Whether an instruction reads the byte at its effective address before it
//  executes: every memory mode except a store's. A store only writes its
//  target, so a read there would trigger an extra access to an I/O location.
//  The //c's ROM-bank flip-flop at $C028 toggles on any access, and the
//  Uthernet II's data register advances its address pointer on a read; a
//  read before the store's write switches the bank back, or puts the byte at
//  the next address.
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::ReadsTarget (const Microcode & microcode)
{
    bool  isMemoryMode = false;
    bool  isStore      = false;



    switch (microcode.globalAddressingMode)
    {
    case GlobalAddressingMode::Absolute:
    case GlobalAddressingMode::AbsoluteX:
    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::ZeroPageX:
    case GlobalAddressingMode::ZeroPageY:
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::ZeroPageIndirect:
        isMemoryMode = true;
        break;

    default:
        break;
    }

    isStore = microcode.operation == Microcode::Store
           || microcode.operation == Microcode::StoreZero
           || microcode.operation == Microcode::StoreAccumulatorAndX;

    return isMemoryMode && !isStore;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperand
//
//  Resolves the addressing mode into operandInfo -- the effective address, and
//  the operand value where the mode implies one -- leaving PC on the LAST byte
//  of the instruction. StepOne's trailing ++PC is what finally advances past
//  it, so the two must be read together: the fetch helpers deliberately stop
//  one short.
//
//  A mode with no operand bytes skips the whole block, PC never moves here,
//  and that same trailing increment lands on the next instruction.
//
//  The default arm asserts rather than falling through quietly: an addressing
//  mode with no fetch would leave operandInfo zeroed, so the instruction would
//  execute against address $0000 instead of failing visibly.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperand (const Microcode & microcode, OperandInfo & operandInfo)
{
    // An illegal opcode has no operand to fetch, and the two implied modes
    // encode their operand in the opcode itself. Both leave PC on the opcode
    // byte -- only a real operand advances it.
    bool  hasOperand = microcode.isLegal
                       && microcode.globalAddressingMode != GlobalAddressingMode::SingleByteNoOperand
                       && microcode.globalAddressingMode != GlobalAddressingMode::Accumulator;



    operandInfo.location         = 0;
    operandInfo.effectiveAddress = 0;
    operandInfo.operand          = 0;

    if (hasOperand)
    {
        // Advance the program counter to the operand byte
        ++PC;

        switch (microcode.globalAddressingMode)
        {
        case GlobalAddressingMode::Absolute:          FetchOperandAbsolute          (operandInfo);               break;
        case GlobalAddressingMode::AbsoluteX:         FetchOperandAbsoluteX         (operandInfo);               break;
        case GlobalAddressingMode::AbsoluteY:         FetchOperandAbsoluteY         (operandInfo);               break;
        case GlobalAddressingMode::Immediate:         FetchOperandImmediate         (operandInfo);               break;
        case GlobalAddressingMode::JumpAbsolute:      FetchOperandJumpAbsolute      (operandInfo);               break;
        case GlobalAddressingMode::JumpIndirect:      FetchOperandJumpIndirect      (operandInfo);               break;
        case GlobalAddressingMode::Relative:          FetchOperandRelative          (operandInfo);               break;
        case GlobalAddressingMode::ZeroPage:          FetchOperandZeroPage          (operandInfo);               break;
        case GlobalAddressingMode::ZeroPageX:         FetchOperandZeroPageX         (operandInfo);               break;
        case GlobalAddressingMode::ZeroPageY:         FetchOperandZeroPageY         (operandInfo);               break;
        case GlobalAddressingMode::ZeroPageXIndirect: FetchOperandZeroPageXIndirect (operandInfo);               break;
        case GlobalAddressingMode::ZeroPageIndirectY: FetchOperandZeroPageIndirectY (operandInfo);               break;
        case GlobalAddressingMode::ZeroPageIndirect:  FetchOperandZeroPageIndirect  (operandInfo);               break;
        case GlobalAddressingMode::AbsoluteXIndirect: FetchOperandAbsoluteXIndirect (operandInfo);               break;
        case GlobalAddressingMode::ZeroPageRelative:  FetchOperandZeroPageRelative  (operandInfo);               break;
        case GlobalAddressingMode::JumpIndirectCmos:  FetchOperandJumpIndirectCmos  (operandInfo);               break;

        default:
            std::printf ("Unhandled addressing mode %d\n", microcode.instruction.asBits.addressingMode);
            ASSERT (false);
            break;
        }

        if (ReadsTarget (microcode))
        {
            operandInfo.operand = ReadByte (operandInfo.effectiveAddress);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageXIndirect
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageXIndirect (Cpu::OperandInfo & operandInfo)
{
    Byte zpBase = ReadByte (PC);
    Byte zpAddr = (zpBase + X) & 0xFF;



    // Zero page word read wraps within zero page
    operandInfo.location         = zpBase;
    operandInfo.effectiveAddress = ReadByte (zpAddr) | (ReadByte ((zpAddr + 1) & 0xFF) << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPage
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPage (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandImmediate
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandImmediate (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location = ReadByte (PC);
    operandInfo.operand  = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandJumpAbsolute
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandJumpAbsolute (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.operand          = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandJumpIndirect
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandJumpIndirect (Cpu::OperandInfo & operandInfo)
{
    Word  lo = 0;
    Word  hi = 0;



    operandInfo.location = ReadWord (PC++);

    // NMOS 6502 bug: JMP indirect wraps within the page.
    // If the pointer is at $xxFF, the high byte is read from $xx00.
    lo = ReadByte (operandInfo.location);
    hi = ReadByte ((operandInfo.location & 0xFF00) | ((operandInfo.location + 1) & 0x00FF));

    operandInfo.effectiveAddress = lo | (hi << 8);
    operandInfo.operand          = operandInfo.effectiveAddress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandJumpIndirectCmos
//
//  65C02 JMP (indirect). Reads the high byte from the next address, fixing the
//  NMOS $xxFF page-boundary bug.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandJumpIndirectCmos (Cpu::OperandInfo & operandInfo)
{
    Word  lo = 0;
    Word  hi = 0;



    operandInfo.location = ReadWord (PC++);

    lo = ReadByte (operandInfo.location);
    hi = ReadByte (static_cast<Word> (operandInfo.location + 1));

    operandInfo.effectiveAddress = lo | (hi << 8);
    operandInfo.operand          = operandInfo.effectiveAddress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageIndirect
//
//  65C02 (zp) mode: the effective address is the 16-bit pointer stored at the
//  zero-page location, wrapping within zero page.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageIndirect (Cpu::OperandInfo & operandInfo)
{
    Byte zpAddr = ReadByte (PC);



    operandInfo.location         = zpAddr;
    operandInfo.effectiveAddress = ReadByte (zpAddr) | (ReadByte ((zpAddr + 1) & 0xFF) << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsoluteXIndirect
//
//  65C02 JMP (abs,X): read the 16-bit vector at (absolute base + X).
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsoluteXIndirect (Cpu::OperandInfo & operandInfo)
{
    Word  ptr = 0;



    Word base = ReadWord (PC++);
    ptr = static_cast<Word> (base + X);

    operandInfo.location         = base;
    operandInfo.effectiveAddress = ReadByte (ptr) | (ReadByte (static_cast<Word> (ptr + 1)) << 8);
    operandInfo.operand          = operandInfo.effectiveAddress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageRelative
//
//  65C02 BBRx/BBSx: a zero-page byte to test followed by a signed branch
//  displacement. operand carries the tested byte; effectiveAddress the target.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageRelative (Cpu::OperandInfo & operandInfo)
{
    Byte  rel = 0;



    Byte zpAddr = ReadByte (PC++);
    rel = ReadByte (PC);

    operandInfo.location         = zpAddr;
    operandInfo.operand          = ReadByte (zpAddr);
    operandInfo.effectiveAddress = static_cast<Word> ((PC + 1) + (SByte) rel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandRelative
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandRelative (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = (PC + 1) + (SByte) operandInfo.location;
    operandInfo.operand          = operandInfo.effectiveAddress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsolute
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsolute (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageIndirectY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageIndirectY (Cpu::OperandInfo & operandInfo)
{
    Byte zpAddr = ReadByte (PC);



    // Zero page word read wraps within zero page
    operandInfo.location          = zpAddr;
    operandInfo.effectiveAddress  = ReadByte (zpAddr) | (ReadByte ((zpAddr + 1) & 0xFF) << 8);
    operandInfo.effectiveAddress += Y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageX
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageX (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = (operandInfo.location + X) & 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = (operandInfo.location + Y) & 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsoluteY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsoluteY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location          = ReadWord (PC++);
    operandInfo.effectiveAddress  = operandInfo.location;
    operandInfo.effectiveAddress += Y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsoluteX
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsoluteX (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location          = ReadWord (PC++);
    operandInfo.effectiveAddress  = operandInfo.location;
    operandInfo.effectiveAddress += X;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteInstruction
//
//  Dispatches an already-decoded instruction to its CpuOperations handler.
//  Pure routing: addressing has been resolved into operandInfo and the cycle
//  count settled by the caller, so nothing here reads memory or touches PC
//  except through the operations themselves.
//
//  Accumulator mode is turned into a POINTER rather than a flag, which is what
//  lets the shift and rotate operations take one code path for `ASL A` and
//  `ASL $1234` alike -- null means "operate on the effective address". Handing
//  them a mode to branch on would put the same test in six places.
//
//  Switching on operation rather than opcode is what collapses 256 opcodes to
//  this list: the addressing mode is already spent, so every LDA variant
//  arrives here as one Load.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::ExecuteInstruction (const Microcode & microcode, const OperandInfo & operandInfo)
{
    Byte * pAccumulator = nullptr;



    if (microcode.globalAddressingMode == GlobalAddressingMode::Accumulator)
    {
        pAccumulator = &A;
    }

    switch (microcode.operation)
    {
    case Microcode::AddWithCarry:         CpuOperations::AddWithCarry         (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::And:                  CpuOperations::And                  (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::BitTest:              CpuOperations::BitTest              (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Branch:               CpuOperations::Branch               (*this, microcode.instruction, operandInfo.operand);                   break;
    case Microcode::Break:                CpuOperations::Break                (*this);                                                               break;
    case Microcode::Compare:              CpuOperations::Compare              (*this, *microcode.pSourceRegister, (Byte) operandInfo.operand);       break;
    case Microcode::Decrement:            CpuOperations::Decrement            (*this, microcode.pSourceRegister, operandInfo.effectiveAddress);      break;
    case Microcode::DecrementAndCompare:  CpuOperations::DecrementAndCompare  (*this, operandInfo.effectiveAddress);                                   break;
    case Microcode::Increment:            CpuOperations::Increment            (*this, microcode.pSourceRegister, operandInfo.effectiveAddress);        break;
    case Microcode::Jump:                 CpuOperations::Jump                 (*this, microcode.instruction, operandInfo.operand);                   break;
    case Microcode::JumpSubroutine:       CpuOperations::JumpSubroutine       (*this, operandInfo.operand);                                          break;
    case Microcode::Load:                 CpuOperations::Load                 (*this, *microcode.pDestinationRegister, (Byte) operandInfo.operand);  break;
    case Microcode::NoOperation:          CpuOperations::NoOperation          (*this);                                                               break;
    case Microcode::Or:                   CpuOperations::Or                   (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Pull:                 CpuOperations::Pull                 (*this, microcode.pDestinationRegister);                               break;
    case Microcode::Push:                 CpuOperations::Push                 (*this, microcode.pSourceRegister);                                    break;
    case Microcode::ReturnFromInterrupt:  CpuOperations::ReturnFromInterrupt  (*this);                                                               break;
    case Microcode::ReturnFromSubroutine: CpuOperations::ReturnFromSubroutine (*this);                                                               break;
    case Microcode::RotateLeft:           CpuOperations::RotateLeft           (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::RotateRight:          CpuOperations::RotateRight          (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::SetFlag:              CpuOperations::SetFlag              (*this, microcode.instruction);                                        break;
    case Microcode::ShiftLeft:            CpuOperations::ShiftLeft            (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::ShiftRight:           CpuOperations::ShiftRight           (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::Store:                CpuOperations::Store                (*this, *microcode.pSourceRegister, operandInfo.effectiveAddress);     break;
    case Microcode::SubtractWithCarry:    CpuOperations::SubtractWithCarry    (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Transfer:             CpuOperations::Transfer             (*this, microcode.pSourceRegister, microcode.pDestinationRegister);    break;
    case Microcode::Xor:                  CpuOperations::Xor                  (*this, (Byte) operandInfo.operand);                                   break;

    case Microcode::StoreAccumulatorAndX: CpuOperations::StoreAccumulatorAndX (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::LoadAccumulatorAndX:  CpuOperations::LoadAccumulatorAndX  (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::ShiftLeftAndOr:       CpuOperations::ShiftLeftAndOr       (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::RotateLeftAndAnd:     CpuOperations::RotateLeftAndAnd     (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::ShiftRightAndXor:     CpuOperations::ShiftRightAndXor     (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::RotateRightAndAdd:    CpuOperations::RotateRightAndAdd    (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::IncrementAndSubtract: CpuOperations::IncrementAndSubtract (*this, operandInfo.effectiveAddress);                                 break;

    // 65C02 (CMOS) operations.
    case Microcode::StoreZero:            CpuOperations::StoreZero            (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::TestAndSetBits:       CpuOperations::TestAndSetBits       (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::TestAndResetBits:     CpuOperations::TestAndResetBits     (*this, operandInfo.effectiveAddress);                                 break;
    case Microcode::ResetMemoryBit:       CpuOperations::ResetMemoryBit       (*this, microcode.instruction, operandInfo.effectiveAddress);          break;
    case Microcode::SetMemoryBit:         CpuOperations::SetMemoryBit         (*this, microcode.instruction, operandInfo.effectiveAddress);          break;
    case Microcode::BitBranchReset:       CpuOperations::BitBranchReset       (*this, microcode.instruction, (Byte) operandInfo.operand, operandInfo.effectiveAddress);  break;
    case Microcode::BitBranchSet:         CpuOperations::BitBranchSet         (*this, microcode.instruction, (Byte) operandInfo.operand, operandInfo.effectiveAddress);  break;
    case Microcode::BranchAlways:         CpuOperations::BranchAlways         (*this, operandInfo.operand);                                          break;
    case Microcode::BitTestImmediate:     CpuOperations::BitTestImmediate     (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::AddWithCarryCmos:     CpuOperations::AddWithCarryCmos     (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::SubtractWithCarryCmos: CpuOperations::SubtractWithCarryCmos (*this, (Byte) operandInfo.operand);                                break;
    case Microcode::BreakCmos:            CpuOperations::BreakCmos            (*this);                                                               break;

    default:
        std::printf ("Unimplemented instruction:  %s\n", microcode.instructionName);                                
        ASSERT (false);
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushByte
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PushByte (Byte value)
{
    WriteByte (stackAddress + SP--, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushWord
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PushWord (Word value)
{
    PushByte (value >> 8);
    PushByte (value & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopByte
//
////////////////////////////////////////////////////////////////////////////////

Byte Cpu::PopByte()
{
    return ReadByte(stackAddress + ++SP);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopWord
//
////////////////////////////////////////////////////////////////////////////////

Word Cpu::PopWord()
{
    Byte lo = PopByte();
    Byte hi = PopByte();



    return lo | (hi << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::WriteByte (Word address, Byte value)
{
    memory[address] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::WriteWord (Word address, Word value)
{
    WriteByte (address, static_cast<Byte> (value & 0xFF));
    WriteByte (static_cast<Word> (address + 1), static_cast<Byte> (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByteSlow
//
//  Base (standalone) slow path: the built-in 64 KB memory[] array. A derived
//  strategy (MemoryBusCpu) overrides this to route I/O and unmapped accesses
//  through the emulator bus; RAM/ROM reads are served by the non-virtual
//  ReadByte fast path in the header and never reach here on that CPU.
//
////////////////////////////////////////////////////////////////////////////////

Byte Cpu::ReadByteSlow (Word address)
{
    return memory[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word Cpu::ReadWord (Word address)
{
    Byte lo = ReadByte (address);
    Byte hi = ReadByte (static_cast<Word> (address + 1));



    return static_cast<Word> (lo | (hi << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeInstructionSet
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeInstructionSet()
{
    InstructionSetBuilder::Registers  registers = { &A, &X, &Y, &SP, &status.status };



    InstructionSetBuilder (instructionSet, registers).Build();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinary
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Cpu::LoadBinary (const std::string & filename, Word address)
{
    HRESULT       hr     = S_OK;
    bool          isOpen = false;
    std::ifstream file     (filename, std::ios::binary);



    isOpen = file.is_open();
    CBRAEx (isOpen, E_INVALIDARG);

    hr = LoadBinary (file, address);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinary  (stream)
//
//  Loads a raw image from a stream directly into CPU memory at an address.
//
//  Read straight into the memory array with no intermediate buffer, since the
//  destination is a plain byte array of known size and copying through a
//  vector would double the work for a 64 KB image.
//
//  The two failure kinds are reported DIFFERENTLY on purpose. A stream fault
//  is the environment's problem and asserts; a negative size (tellg failed) or
//  an image that would run off the top of memory is the CALLER's problem and
//  returns E_INVALIDARG. Collapsing them would make a caller's bad address
//  look like a broken file.
//
//  The bounds test is written as size <= memSize - address rather than
//  address + size <= memSize, so it cannot overflow on a large address.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Cpu::LoadBinary (std::istream & stream, Word address)
{
    HRESULT         hr       = S_OK;
    bool            readWell = false;
    bool            fits     = false;
    std::streampos  size     = 0;



    // Determine stream size
    stream.seekg (0, std::ios::end);
    size = stream.tellg();
    stream.seekg (0, std::ios::beg);

    readWell = !stream.bad();
    CBRA (readWell);

    // A negative size means tellg failed; too large means the image would run
    // off the top of memory. Both are the caller's problem, not a stream fault,
    // so they read as E_INVALIDARG rather than E_FAIL.
    fits = (size >= 0 && (size_t) size <= memSize - address);
    CBREx (fits, E_INVALIDARG);

    // Read directly into CPU memory — no intermediate buffer
    stream.read (reinterpret_cast<char *>(memory.data() + address), size);

    readWell = !stream.bad();
    CBRA (readWell);

Error:
    return hr;
}
