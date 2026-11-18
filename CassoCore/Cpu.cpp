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

Cpu::Cpu() :
    m_trace (m_fetchObservers, instructionSet.data())
{
    InitializeInstructionSet();

#ifdef _DEBUG
    // Preserve the always-on illegal-opcode look-back in Debug builds even
    // when --trace is not passed. The --trace switch re-enables with a far
    // larger ring at runtime.
    m_trace.Enable (CpuTrace::kDefaultLookback);
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
//  Snaps the CPU state before the instruction into the trace. Called at the
//  top of StepOne, so the newest entry on a dump is the faulting instruction
//  itself and the one before is its predecessor.
//
//  `opcode` is the byte StepOne fetched through ReadByte (the bus-routed
//  value, which honors ROM, language-card and soft-switch banking). It must
//  NOT be read again from the raw Cpu::memory[] array, which only backs RAM
//  at $00-$BF and is stale for ROM, $Cxxx and $Dxxx-$FFFF; that would make
//  the disassembly wrong in exactly the banked regions a fault trace most
//  needs. The operand bytes go through PeekForTrace for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::TracePush (Byte opcode)
{
    CpuTrace::Entry  & e = m_trace.PushNext();



    e.pc     = PC;
    e.opcode = opcode;
    e.op1    = PeekForTrace ((Word) (PC + 1));
    e.op2    = PeekForTrace ((Word) (PC + 2));
    e.a      = A;
    e.x      = X;
    e.y      = Y;
    e.sp     = SP;
    e.p      = status.status;
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
    CpuOperandInfo     operandInfo = { 0 };
    Word               next        = 0;



    m_instructionStart = start;

    if (m_fetchTable != nullptr && m_fetchTable[start >> 8] != nullptr)
    {
        m_fetchTable[start >> 8][(start & 0xFF) >> 3] |= (uint8_t) (1u << (start & 7));
    }

    if (m_fetchObservers.IsObserved())
    {
        ObserveFetch (opcode);
    }

    if (!microcode.isLegal)
    {
        DEBUGMSG (L"Illegal opcode $%02X at PC=$%04X\n", opcode, PC);

        if (m_trace.IsEnabled())
        {
            m_trace.DumpRecent (opcode, PC);
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

void Cpu::FetchOperand (const Microcode & microcode, CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandZeroPageXIndirect (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandZeroPage (CpuOperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandImmediate
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandImmediate (CpuOperandInfo & operandInfo)
{
    operandInfo.location = ReadByte (PC);
    operandInfo.operand  = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandJumpAbsolute
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandJumpAbsolute (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandJumpIndirect (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandJumpIndirectCmos (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandZeroPageIndirect (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandAbsoluteXIndirect (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandZeroPageRelative (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandRelative (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandAbsolute (CpuOperandInfo & operandInfo)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageIndirectY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageIndirectY (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandZeroPageX (CpuOperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = (operandInfo.location + X) & 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageY (CpuOperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = (operandInfo.location + Y) & 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsoluteY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsoluteY (CpuOperandInfo & operandInfo)
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

void Cpu::FetchOperandAbsoluteX (CpuOperandInfo & operandInfo)
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

void Cpu::ExecuteInstruction (const Microcode & microcode, const CpuOperandInfo & operandInfo)
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
