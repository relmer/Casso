#include "Pch.h"

#include "Disassembler.h"
#include "Microcode.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::FormatHexByte
//
////////////////////////////////////////////////////////////////////////////////

std::string Disassembler::FormatHexByte (Byte value)
{
    return std::format ("{:02X}", (unsigned) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::FormatHexWord
//
////////////////////////////////////////////////////////////////////////////////

std::string Disassembler::FormatHexWord (Word value)
{
    return std::format ("{:04X}", (unsigned) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::FormatOperand
//
//  The text form each addressing mode takes. A branch shows its TARGET rather
//  than its displacement, because the displacement is the one number nobody
//  reading a listing wants to add up by hand.
//
////////////////////////////////////////////////////////////////////////////////

std::string Disassembler::FormatOperand (
    GlobalAddressingMode::AddressingMode    mode,
    const Byte                            * operandBytes,
    Word                                    instructionAddress)
{
    constexpr size_t  kBranchLength    = 2;
    constexpr size_t  kBitBranchLength = 3;



    Byte  low  = (operandBytes != nullptr) ? operandBytes[0] : 0;
    Byte  high = (operandBytes != nullptr) ? operandBytes[1] : 0;
    Word  word = (Word) (low | (high << 8));



    switch (mode)
    {
        case GlobalAddressingMode::Immediate:          return "#$" + FormatHexByte (low);
        case GlobalAddressingMode::ZeroPage:           return "$" + FormatHexByte (low);
        case GlobalAddressingMode::ZeroPageX:          return "$" + FormatHexByte (low) + ",X";
        case GlobalAddressingMode::ZeroPageY:          return "$" + FormatHexByte (low) + ",Y";
        case GlobalAddressingMode::Absolute:           return "$" + FormatHexWord (word);
        case GlobalAddressingMode::JumpAbsolute:       return "$" + FormatHexWord (word);
        case GlobalAddressingMode::AbsoluteX:          return "$" + FormatHexWord (word) + ",X";
        case GlobalAddressingMode::AbsoluteY:          return "$" + FormatHexWord (word) + ",Y";
        case GlobalAddressingMode::ZeroPageXIndirect:  return "($" + FormatHexByte (low) + ",X)";
        case GlobalAddressingMode::ZeroPageIndirectY:  return "($" + FormatHexByte (low) + "),Y";
        case GlobalAddressingMode::ZeroPageIndirect:   return "($" + FormatHexByte (low) + ")";
        case GlobalAddressingMode::AbsoluteXIndirect:  return "($" + FormatHexWord (word) + ",X)";
        case GlobalAddressingMode::JumpIndirect:       return "($" + FormatHexWord (word) + ")";
        case GlobalAddressingMode::JumpIndirectCmos:   return "($" + FormatHexWord (word) + ")";
        case GlobalAddressingMode::Accumulator:        return "A";
        case GlobalAddressingMode::SingleByteNoOperand: return "";

        case GlobalAddressingMode::Relative:
            return "$" + FormatHexWord ((Word) (instructionAddress + kBranchLength + (SByte) low));

        case GlobalAddressingMode::ZeroPageRelative:
            return "$" + FormatHexByte (low) + ",$"
                 + FormatHexWord ((Word) (instructionAddress + kBitBranchLength + (SByte) high));

        default:
            return "";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::FormatLine
//
//  Address, up to three bytes padded to a fixed column, mnemonic, operand.
//
////////////////////////////////////////////////////////////////////////////////

std::string Disassembler::FormatLine (const DisassembledLine & line)
{
    constexpr size_t  kBytesColumnWidth = 9;   // "AA BB CC "



    std::string  text = FormatHexWord (line.address) + "  ";
    std::string  hex;



    for (Byte value : line.bytes)
    {
        hex += FormatHexByte (value);
        hex += ' ';
    }

    while (hex.size() < kBytesColumnWidth)
    {
        hex += ' ';
    }

    text += hex;
    text += ' ';
    text += line.mnemonic;

    if (!line.operand.empty())
    {
        text += ' ';
        text += line.operand;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::Disassemble
//
//  One line per instruction, or per byte the table cannot place. An operand
//  the buffer ends inside is not guessed at: the bytes that remain become one
//  undefined line, so the tail of the file is visible without being described
//  as something it is not.
//
////////////////////////////////////////////////////////////////////////////////

void Disassembler::Disassemble (
    std::span<const Byte>            bytes,
    Word                             origin,
    const Microcode                * instructionSet,
    std::vector<DisassembledLine>  & outLines)
{
    size_t  at    = 0;
    size_t  count = bytes.size();



    outLines.clear();

    if (instructionSet == nullptr)
    {
        return;
    }

    while (at < count)
    {
        DisassembledLine    line;
        Byte                opcode      = bytes[at];
        const Microcode   & microcode   = instructionSet[opcode];
        size_t              operandSize = OpcodeTable::GetOperandSize (microcode.globalAddressingMode);
        size_t              length      = 1 + operandSize;
        bool                fits        = (at + length) <= count;

        line.address = (Word) (origin + at);

        if (!microcode.isLegal)
        {
            line.bytes.push_back (opcode);
            line.mnemonic  = kUndefinedMnemonic;
            line.isDefined = false;
            outLines.push_back (line);
            at++;
            continue;
        }

        if (!fits)
        {
            line.bytes.assign (bytes.begin() + (ptrdiff_t) at, bytes.end());
            line.mnemonic  = kUndefinedMnemonic;
            line.isDefined = false;
            outLines.push_back (line);
            break;
        }

        line.bytes.assign (bytes.begin() + (ptrdiff_t) at, bytes.begin() + (ptrdiff_t) (at + length));
        line.mnemonic  = microcode.instructionName;
        line.operand   = FormatOperand (microcode.globalAddressingMode,
                                        (operandSize > 0) ? &bytes[at + 1] : nullptr,
                                        line.address);
        line.isDefined = true;

        outLines.push_back (line);
        at += length;
    }
}
