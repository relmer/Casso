#include "Pch.h"

#include "CpuStepPrinter.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuStepPrinter::PrintSingleStepInfo
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

void CpuStepPrinter::PrintSingleStepInfo (
    const Registers         & regs,
    Word                      initialPC,
    Byte                      opcode,
    const CpuOperandInfo    & operandInfo,
    const Microcode         * instructionSet,
    const ByteReader        & read)
{
    static constexpr char flags[][8] =
    {
        ".......",
        "CZIDBVN"
    };



    // Print the registers and the opcode byte
    std::printf ("SP: %02x  A: %04X  X: %04X  Y: %04X  %c%c%c%c%c%c%c    [%04X] %02X ",
                 regs.sp,
                 regs.a,
                 regs.x,
                 regs.y,
                 flags[regs.status.flags.carry][0],
                 flags[regs.status.flags.zero][1],
                 flags[regs.status.flags.interruptDisable][2],
                 flags[regs.status.flags.decimal][3],
                 flags[regs.status.flags.brk][4],
                 flags[regs.status.flags.overflow][5],
                 flags[regs.status.flags.negative][6],
                 initialPC,
                 opcode);

    PrintOperandBytes (initialPC, opcode, instructionSet, read);

    std::printf ("%s ", instructionSet[opcode].instructionName);

    PrintOperandAndComment (opcode, operandInfo, instructionSet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuStepPrinter::PrintOperandBytes
//
//  The raw operand bytes of an instruction, blank-padded to a fixed width so
//  the disassembly after them stays in column no matter how many bytes the
//  addressing mode has.
//
//  Grouped by SIZE rather than listed per mode: every mode taking one operand
//  byte prints the same way, so the cases fall through to three bodies --
//  none, one byte, two bytes.
//
//  Reads go through the reader at PC+1 / PC+2 rather than through operandInfo,
//  so this shows what is actually in memory, including for an instruction
//  whose operand was never decoded.
//
////////////////////////////////////////////////////////////////////////////////

void CpuStepPrinter::PrintOperandBytes (
    Word                  initialPC,
    Byte                  opcode,
    const Microcode     * instructionSet,
    const ByteReader    & read)
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
            std::printf ("%02X           ", read (static_cast<Word> (initialPC + 1)));
            break;

        case GlobalAddressingMode::Absolute:
        case GlobalAddressingMode::AbsoluteX:
        case GlobalAddressingMode::AbsoluteY:
        case GlobalAddressingMode::JumpAbsolute:
        case GlobalAddressingMode::JumpIndirect:
        case GlobalAddressingMode::Relative:
            std::printf ("%02X %02X        ", read (static_cast<Word> (initialPC + 1)), read (static_cast<Word> (initialPC + 2)));
            break;

        default:
            ASSERT (false);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuStepPrinter::PrintOperandAndComment
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

void CpuStepPrinter::PrintOperandAndComment (
    Byte                      opcode,
    const CpuOperandInfo    & operandInfo,
    const Microcode         * instructionSet)
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
