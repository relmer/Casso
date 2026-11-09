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

    if (!line.comment.empty())
    {
        text += "  ; ";
        text += line.comment;
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
    std::vector<DisassembledLine>  & outLines,
    const DisassemblyOptions       & options)
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
        DisassembledLine                      line;
        Byte                                  opcode       = bytes[at];
        const Microcode                     & microcode    = instructionSet[opcode];
        const UndocumentedOpcode            * undocumented = (!microcode.isLegal && options.nmosUndocumented) ? FindUndocumented (opcode) : nullptr;
        GlobalAddressingMode::AddressingMode  mode         = (undocumented != nullptr) ? undocumented->mode : microcode.globalAddressingMode;
        size_t                                operandSize  = OpcodeTable::GetOperandSize (mode);
        size_t                                length       = 1 + operandSize;
        bool                                  fits         = (at + length) <= count;

        line.address = (Word) (origin + at);

        if (!microcode.isLegal && undocumented == nullptr)
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
        line.mnemonic  = (undocumented != nullptr) ? undocumented->mnemonic : microcode.instructionName;
        line.operand   = FormatOperand (mode, (operandSize > 0) ? &bytes[at + 1] : nullptr, line.address);
        line.isDefined = true;

        if (options.symbols != nullptr)
        {
            ApplySymbol (line, mode, *options.symbols);
        }

        outLines.push_back (line);
        at += length;

        //  The MLI returns past its call number and parameter list's address,
        //  so the three bytes after its JSR are data.
        if (opcode == s_kJsrOpcode && options.mliEntry.has_value() && at + 3 <= count &&
            (Word) (line.bytes[1] | (line.bytes[2] << 8)) == *options.mliEntry)
        {
            DisassembledLine  call;
            DisassembledLine  parameters;

            call.address   = (Word) (origin + at);
            call.bytes     = { bytes[at] };
            call.mnemonic  = kByteDirective;
            call.operand   = "$" + FormatHexByte (bytes[at]);
            call.isDefined = true;

            if (options.mliCalls != nullptr && options.mliCalls->contains (bytes[at]))
            {
                call.comment = options.mliCalls->at (bytes[at]);
            }

            parameters.address   = (Word) (origin + at + 1);
            parameters.bytes     = { bytes[at + 1], bytes[at + 2] };
            parameters.mnemonic  = kWordDirective;
            parameters.operand   = "$" + FormatHexWord ((Word) (bytes[at + 1] | (bytes[at + 2] << 8)));
            parameters.isDefined = true;

            outLines.push_back (call);
            outLines.push_back (parameters);
            at += 3;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::FindUndocumented
//
////////////////////////////////////////////////////////////////////////////////

const Disassembler::UndocumentedOpcode * Disassembler::FindUndocumented (Byte opcode)
{
    for (const UndocumentedOpcode & entry : s_kUndocumentedOpcodes)
    {
        if (entry.opcode == opcode)
        {
            return &entry;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::ApplySymbol
//
//  An absolute operand at a known address is written as the address's name,
//  with its index or indirection in place: `STA TXTCLR`, `JMP (SOFTEV)`,
//  `LDA KBD,X`.
//
////////////////////////////////////////////////////////////////////////////////

void Disassembler::ApplySymbol (DisassembledLine & line, GlobalAddressingMode::AddressingMode mode, const std::map<Word, std::string> & symbols)
{
    Word                                         target = 0;
    std::string                                  number;
    size_t                                       at     = 0;
    std::map<Word, std::string>::const_iterator  found;



    switch (mode)
    {
        case GlobalAddressingMode::Absolute:
        case GlobalAddressingMode::JumpAbsolute:
        case GlobalAddressingMode::AbsoluteX:
        case GlobalAddressingMode::AbsoluteY:
        case GlobalAddressingMode::AbsoluteXIndirect:
        case GlobalAddressingMode::JumpIndirect:
        case GlobalAddressingMode::JumpIndirectCmos:
            break;

        default:
            return;
    }

    target = (Word) (line.bytes[1] | (line.bytes[2] << 8));
    found  = symbols.find (target);

    if (found == symbols.end())
    {
        return;
    }

    number = "$" + FormatHexWord (target);
    at     = line.operand.find (number);

    if (at != std::string::npos)
    {
        line.operand.replace (at, number.size(), found->second);
    }
}
