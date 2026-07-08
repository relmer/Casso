#include "Pch.h"

#include "Cpu.h"
#include "CpuOperations.h"
#include "Ehm.h"
#include "Group00.h"
#include "Group01.h"
#include "Group10.h"
#include "GroupMisc.h"
#include "Utils.h"





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
    m_traceHead  = 0;
    m_traceCount = 0;

    if (capacity == 0)
    {
        m_traceEnabled  = false;
        m_traceCapacity = 0;
        m_trace.clear();
        m_trace.shrink_to_fit();
        return;
    }

    m_traceCapacity = capacity;
    m_trace.assign (capacity, TraceEntry {});
    m_traceEnabled  = true;
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
//  a fault trace most needs to be correct about. The operand bytes are
//  read from the raw RAM backing, which is exact for $0000-$BFFF code
//  (where bootloaders/decryptors live) and approximate elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::TracePush (Byte opcode)
{
    TraceEntry &  e = m_trace[m_traceHead];

    e.pc     = PC;
    e.opcode = opcode;
    e.op1    = memory[(Word) (PC + 1)];
    e.op2    = memory[(Word) (PC + 2)];
    e.a      = A;
    e.x      = X;
    e.y      = Y;
    e.sp     = SP;
    e.p      = status.status;

    m_traceHead = (m_traceHead + 1) % m_traceCapacity;
    m_traceCount++;
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
    // Short newest-first look-back to stderr -- enough to see the
    // JMP/JSR/RTS chain that landed PC on the bad byte. The full ring
    // (which may be millions of entries under --trace) is written
    // separately by DumpTraceToFile, so cap this quick dump.
    static constexpr size_t  kMaxLookback = 256;

    size_t  total   = (m_traceCount < (uint64_t) m_traceCapacity) ? (size_t) m_traceCount
                                                                  : m_traceCapacity;
    size_t  i       = 0;
    size_t  index   = 0;

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
        index = (m_traceHead + m_traceCapacity - 1 - i) % m_traceCapacity;

        const TraceEntry &  e        = m_trace[index];
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
//  `onProgress` (if set) is called every kProgressStride entries and once
//  at completion with (entriesWritten, totalEntries). Returns true on
//  success. CassoCore owns no UI; the caller drives any progress dialog.
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::DumpTraceToFile (const std::wstring & path,
                           const std::function<void (uint64_t, uint64_t)> & onProgress) const
{
    static constexpr uint64_t  kProgressStride = 100000;

    std::ofstream  out (path, std::ios::binary | std::ios::trunc);
    uint64_t       total = (m_traceCount < (uint64_t) m_traceCapacity) ? m_traceCount
                                                                       : (uint64_t) m_traceCapacity;
    size_t         start = 0;
    char           line[160];

    if (!out.is_open() || m_traceCapacity == 0)
    {
        return false;
    }

    // When the ring has wrapped, the oldest surviving entry sits at the
    // current head; otherwise it starts at index 0.
    start = (m_traceCount > (uint64_t) m_traceCapacity) ? m_traceHead : 0;

    out << "Casso CPU execution trace -- " << total << " instructions (oldest first)\n";

    for (uint64_t i = 0; i < total; i++)
    {
        size_t              index = (start + (size_t) i) % m_traceCapacity;
        const TraceEntry &  e     = m_trace[index];
        const char *        name  = instructionSet[e.opcode].instructionName != nullptr
                                    ? instructionSet[e.opcode].instructionName
                                    : "???";

        int  n = std::snprintf (line, sizeof (line),
                                "%08llu  PC=$%04X  op=$%02X OPS=$%02X $%02X (%s)  "
                                "A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X\n",
                                (unsigned long long) i,
                                (unsigned) e.pc, (unsigned) e.opcode,
                                (unsigned) e.op1, (unsigned) e.op2, name,
                                (unsigned) e.a, (unsigned) e.x, (unsigned) e.y,
                                (unsigned) e.sp, (unsigned) e.p);
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

    return out.good();
}




////////////////////////////////////////////////////////////////////////////////
//
//  StepOne
//
////////////////////////////////////////////////////////////////////////////////
void Cpu::StepOne()
{

    Byte        opcode      = ReadByte (PC);
    Microcode   microcode   = instructionSet[opcode];
    OperandInfo operandInfo = { 0 };



    if (m_traceEnabled)
    {
        TracePush (opcode);
    }

    if (!microcode.isLegal)
    {
        DEBUGMSG (L"Illegal opcode $%02X at PC=$%04X\n", opcode, PC);

        if (m_traceEnabled)
        {
            DumpInstructionTrace (opcode, PC);
        }

        ASSERT (false);

        m_lastCycles = 2;
        ++PC;
        
        return;
    }

    m_lastCycles = microcode.baseCycles;

    FetchOperand (microcode, operandInfo);
    ++PC;

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
        microcode.operation != Microcode::StoreZero          &&
        microcode.operation != Microcode::TestAndSetBits     &&
        microcode.operation != Microcode::TestAndResetBits   &&
        microcode.operation != Microcode::ResetMemoryBit     &&
        microcode.operation != Microcode::SetMemoryBit;

    if (isReadOp)
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
            }
        }
    }

    Word pcAfterFetch = PC;

    ExecuteInstruction (microcode, operandInfo);

    // Branch penalty: +1 when taken, +1 more when crossing a page. BRA is
    // unconditional so it always pays the taken penalty.
    if ((microcode.operation == Microcode::Branch ||
         microcode.operation == Microcode::BranchAlways) && PC != pcAfterFetch)
    {
        m_lastCycles++;

        if ((pcAfterFetch & 0xFF00) != (PC & 0xFF00))
        {
            m_lastCycles++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintSingleStepInfo
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
//  FetchOperand
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperand (Microcode microcode, OperandInfo & operandInfo)
{
    operandInfo.location         = 0;
    operandInfo.effectiveAddress = 0;
    operandInfo.operand          = 0;

    if (!microcode.isLegal)
    {
        return;
    }

    if (microcode.globalAddressingMode == GlobalAddressingMode::SingleByteNoOperand ||
        microcode.globalAddressingMode == GlobalAddressingMode::Accumulator)
    {
        return;
    }

    // Advance the program counter to the operand byte
    ++PC;

    switch (microcode.globalAddressingMode)
    {
    case GlobalAddressingMode::Absolute:          FetchOperandAbsolute          (operandInfo, microcode);    break;
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
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.location = ReadWord (PC++);

    // NMOS 6502 bug: JMP indirect wraps within the page.
    // If the pointer is at $xxFF, the high byte is read from $xx00.
    Word lo = ReadByte (operandInfo.location);
    Word hi = ReadByte ((operandInfo.location & 0xFF00) | ((operandInfo.location + 1) & 0x00FF));

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
    operandInfo.location = ReadWord (PC++);

    Word lo = ReadByte (operandInfo.location);
    Word hi = ReadByte (static_cast<Word> (operandInfo.location + 1));

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
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
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
    Word base = ReadWord (PC++);
    Word ptr  = static_cast<Word> (base + X);

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
    Byte zpAddr = ReadByte (PC++);
    Byte rel    = ReadByte (PC);

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

void Cpu::FetchOperandAbsolute (Cpu::OperandInfo & operandInfo, Microcode & microcode)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.operand           = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.operand           = ReadByte (operandInfo.effectiveAddress);
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
    operandInfo.operand           = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteInstruction
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::ExecuteInstruction (Microcode microcode, const OperandInfo & operandInfo)
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
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte Cpu::ReadByte (Word address)
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
    InitializeGroup00();
    InitializeGroup01();
    InitializeGroup10();
    InitializeMisc();
    InitializeUndocumented();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeGroup00
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeGroup00()
{
    using _00 = Group00;
    struct TableEntry
    {
        _00::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pSourceRegister;
        Byte                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { _00::BIT,          _00::AMF_ZeroPage  | _00::AMF_Absolute,                      Microcode::BitTest, nullptr, nullptr },
        { _00::JMP,          _00::AMF_Absolute,                                           Microcode::Jump,    nullptr, nullptr },
        { _00::JMP_indirect, _00::AMF_Absolute,                                           Microcode::Jump,    nullptr, nullptr },
        { _00::STY,          _00::AMF_ZeroPage  | _00::AMF_Absolute | _00::AMF_ZeroPageX, Microcode::Store,   &Y,      nullptr },
        { _00::LDY,          _00::__AMF_AllModes,                                         Microcode::Load,    nullptr, &Y      },
        { _00::CPY,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, &Y,      nullptr },
        { _00::CPX,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, &X,      nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_00::__AM_Count, _00::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group00, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeGroup01
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeGroup01()
{
    using _01 = Group01;
    struct TableEntry
    {
        _01::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pSourceRegister;
        Byte                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { _01::ORA, _01::__AMF_AllModes,                         Microcode::Or,                nullptr, nullptr },
        { _01::AND, _01::__AMF_AllModes,                         Microcode::And,               nullptr, nullptr },
        { _01::EOR, _01::__AMF_AllModes,                         Microcode::Xor,               nullptr, nullptr },
        { _01::ADC, _01::__AMF_AllModes,                         Microcode::AddWithCarry,      nullptr, nullptr },
        { _01::STA, _01::__AMF_AllModes & ~(_01::AMF_Immediate), Microcode::Store,             &A,      nullptr },
        { _01::LDA, _01::__AMF_AllModes,                         Microcode::Load,              nullptr, &A      },
        { _01::CMP, _01::__AMF_AllModes,                         Microcode::Compare,           &A,      nullptr },
        { _01::SBC, _01::__AMF_AllModes,                         Microcode::SubtractWithCarry, nullptr, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_01::__AM_Count, _01::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group01, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeGroup10
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeGroup10()
{
    using _10 = Group10;
    struct TableEntry
    {
        _10::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pSourceRegister;
        Byte                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { _10::ASL, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::ShiftLeft,   &A,      nullptr },
        { _10::ROL, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::RotateLeft,  &A,      nullptr },
        { _10::LSR, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::ShiftRight,  &A,      nullptr },
        { _10::ROR, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::RotateRight, &A,      nullptr },
        { _10::STX, _10::AMF_ZeroPage | _10::AMF_Absolute | _10::AMF_ZeroPageX,                     Microcode::Store,       &X,      nullptr },
        { _10::LDX, _10::__AMF_AllModes & ~(_10::AMF_Accumulator),                                  Microcode::Load,        nullptr, &X      },
        { _10::DEC, _10::__AMF_AllModes & ~(_10::AMF_Immediate | _10::AMF_Accumulator),             Microcode::Decrement,   nullptr, nullptr },
        { _10::INC, _10::__AMF_AllModes & ~(_10::AMF_Immediate | _10::AMF_Accumulator),             Microcode::Increment,   nullptr, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_10::__AM_Count, _10::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group10, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeMisc
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeMisc()
{
    struct TableEntry
    {
        GroupMisc::Opcode                      opcode;
        GlobalAddressingMode::AddressingMode   addressingMode;
        Microcode::Operation                   operation;
        Byte                                 * pSourceRegister;
        Byte                                 * pDestinationRegister;
        Byte                                   baseCycles;
    };

    TableEntry table[] =
    {
        { GroupMisc::BPL, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BMI, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BVC, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BVS, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BCC, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BCS, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BNE, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BEQ, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        
        { GroupMisc::BRK, GlobalAddressingMode::SingleByteNoOperand, Microcode::Break,                nullptr,        nullptr,        7 },
        { GroupMisc::JSR, GlobalAddressingMode::JumpAbsolute,        Microcode::JumpSubroutine,       nullptr,        nullptr,        6 },
        { GroupMisc::RTI, GlobalAddressingMode::SingleByteNoOperand, Microcode::ReturnFromInterrupt,  nullptr,        nullptr,        6 },
        { GroupMisc::RTS, GlobalAddressingMode::SingleByteNoOperand, Microcode::ReturnFromSubroutine, nullptr,        nullptr,        6 },
         
        { GroupMisc::PHP, GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                 &status.status, nullptr,        3 },
        { GroupMisc::PLP, GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                 nullptr,        &status.status, 4 },
        { GroupMisc::PHA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                 &A,             nullptr,        3 },
        { GroupMisc::PLA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                 nullptr,        &A,             4 },
        { GroupMisc::DEY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Decrement,            &Y,             nullptr,        2 },
        { GroupMisc::TAY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &A,             &Y,             2 },
        { GroupMisc::INY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Increment,            &Y,             nullptr,        2 },
        { GroupMisc::INX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Increment,            &X,             nullptr,        2 },
         
        { GroupMisc::CLC, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
        { GroupMisc::SEC, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
        { GroupMisc::CLI, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
        { GroupMisc::SEI, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
        { GroupMisc::TYA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &Y,             &A,             2 },
        { GroupMisc::CLV, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
        { GroupMisc::CLD, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
        { GroupMisc::SED, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr,        2 },
         
        { GroupMisc::TXA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &X,             &A,             2 },
        { GroupMisc::TXS, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &X,             &SP,            2 },
        { GroupMisc::TAX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &A,             &X,             2 },
        { GroupMisc::TSX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &SP,            &X,             2 },
        { GroupMisc::DEX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Decrement,            &X,             nullptr,        2 },
        { GroupMisc::NOP, GlobalAddressingMode::SingleByteNoOperand, Microcode::NoOperation,          nullptr,        nullptr,        2 },
    };


    for (TableEntry entry : table)
    {
        Instruction instruction            = Instruction (GroupMisc::instruction[entry.opcode].instruction);
        instructionSet[instruction.asByte] = Microcode (instruction, GroupMisc::instruction[entry.opcode].name, entry.operation, entry.addressingMode, entry.pSourceRegister, entry.pDestinationRegister);
        instructionSet[instruction.asByte].baseCycles = entry.baseCycles;
    }

}




////////////////////////////////////////////////////////////////////////////////
//
//  InitializeUndocumented
//
//  NMOS 6502 undocumented opcodes used by real Apple II software.
//  Only those encountered in the wild are listed here.
//
//  $04  DOP zp  — Double-NOP: reads a zero-page byte and discards it.
//                 2 bytes, 3 cycles.
//  $CF  DCP abs — Decrement memory at absolute address then CMP A with
//                 the decremented value.  3 bytes, 6 cycles.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeUndocumented()
{
    static constexpr Byte  s_kDopBaseCycles = 3;
    static constexpr Byte  s_kDcpBaseCycles = 6;

    instructionSet[0x04] = Microcode (Instruction (0x04), "NOP",
                                      Microcode::NoOperation,
                                      GlobalAddressingMode::ZeroPage,
                                      nullptr, nullptr);
    instructionSet[0x04].baseCycles = s_kDopBaseCycles;

    instructionSet[0xCF] = Microcode (Instruction (0xCF), "DCP",
                                      Microcode::DecrementAndCompare,
                                      GlobalAddressingMode::Absolute,
                                      nullptr, nullptr);
    instructionSet[0xCF].baseCycles = s_kDcpBaseCycles;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CreateInstruction
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::CreateInstruction (uint32_t                      addressingModeMax,
                             const char           * const  instructionName[],
                             Byte                          opcode,
                             Byte                          addressingModeFlags,
                             Byte                          group,
                             Microcode::Operation          operation,
                             Byte                 *        pSourceRegister,
                             Byte                 *        pDestinationRegister)
{
    Byte addressingMode = 0;
    Byte currentAddressingModeFlag = 1;

    while (addressingMode < addressingModeMax)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction instruction            = Instruction (opcode, addressingMode, group);
            instructionSet[instruction.asByte] = Microcode   (instruction, instructionName[opcode], operation, pSourceRegister, pDestinationRegister);

            // Compute base cycle count from addressing mode and operation type
            bool isStore     = (operation == Microcode::Store);
            bool isRmw       = (operation == Microcode::ShiftLeft  || operation == Microcode::ShiftRight  ||
                                operation == Microcode::RotateLeft || operation == Microcode::RotateRight ||
                                operation == Microcode::Decrement  || operation == Microcode::Increment);
            bool isMemoryRmw = isRmw && (instructionSet[instruction.asByte].globalAddressingMode != GlobalAddressingMode::Accumulator);

            Byte cycles = 2;

            switch (instructionSet[instruction.asByte].globalAddressingMode)
            {
            case GlobalAddressingMode::Immediate:           cycles = 2;                                        break;
            case GlobalAddressingMode::ZeroPage:            cycles = isMemoryRmw ? 5 : 3;                      break;
            case GlobalAddressingMode::ZeroPageX:           cycles = isMemoryRmw ? 6 : 4;                      break;
            case GlobalAddressingMode::ZeroPageY:           cycles = 4;                                        break;
            case GlobalAddressingMode::Absolute:            cycles = isMemoryRmw ? 6 : 4;                      break;
            case GlobalAddressingMode::AbsoluteX:           cycles = isMemoryRmw ? 7 : (isStore ? 5 : 4);     break;
            case GlobalAddressingMode::AbsoluteY:           cycles = isStore ? 5 : 4;                          break;
            case GlobalAddressingMode::ZeroPageXIndirect:   cycles = 6;                                        break;
            case GlobalAddressingMode::ZeroPageIndirectY:   cycles = isStore ? 6 : 5;                          break;
            case GlobalAddressingMode::Accumulator:         cycles = 2;                                        break;
            case GlobalAddressingMode::JumpAbsolute:        cycles = 3;                                        break;
            case GlobalAddressingMode::JumpIndirect:        cycles = 5;                                        break;
            case GlobalAddressingMode::Relative:            cycles = 2;                                        break;
            case GlobalAddressingMode::SingleByteNoOperand: cycles = 2;                                        break;
            default:                                                                                           break;
            }

            instructionSet[instruction.asByte].baseCycles = cycles;
        }

        ++addressingMode;
        currentAddressingModeFlag <<= 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinary
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::LoadBinary (const std::string & filename, Word address)
{
    HRESULT       hr      = S_OK;
    std::ifstream file      (filename, std::ios::binary);
    bool          fLoaded = false;

    CBRA (file.is_open());

    fLoaded = LoadBinary (file, address);
    CBR  (fLoaded);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinary
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::LoadBinary (std::istream & stream, Word address)
{
    HRESULT hr = S_OK;

    // Determine stream size
    stream.seekg (0, std::ios::end);
    auto size = stream.tellg();
    stream.seekg (0, std::ios::beg);

    CBRA (!stream.bad());
    CBR  (size >= 0 && (size_t) size <= memSize - address);

    // Read directly into CPU memory — no intermediate buffer
    stream.read (reinterpret_cast<char *>(memory.data() + address), size);

    CBRA (!stream.bad());

Error:
    return SUCCEEDED (hr);
}
