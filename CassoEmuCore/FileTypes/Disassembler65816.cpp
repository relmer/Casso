#include "Pch.h"

#include "FileTypes/Disassembler65816.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler65816::Disassemble
//
//  An instruction whose operand runs past the end of the bytes is listed as
//  its bytes, a DFB each.
//
////////////////////////////////////////////////////////////////////////////////

void Disassembler65816::Disassemble (
    std::span<const Byte>           bytes,
    Word                            origin,
    bool                            longRegisters,
    bool                            followFlags,
    std::vector<DisassembledLine> & outLines)
{
    size_t  at    = 0;
    bool    wideM = longRegisters;
    bool    wideX = longRegisters;



    outLines.clear();

    while (at < bytes.size())
    {
        DisassembledLine  line;
        Byte              opcode = bytes[at];
        char              mode   = s_kModes[opcode];
        size_t            size   = 1 + GetOperandSize (mode, wideM, wideX);

        line.address = (Word) (origin + at);

        if (at + size > bytes.size())
        {
            for (; at < bytes.size(); at++)
            {
                DisassembledLine  data;

                data.address  = (Word) (origin + at);
                data.mnemonic = Disassembler::kByteDirective;
                data.operand  = "$" + Disassembler::FormatHexByte (bytes[at]);
                data.bytes    = { bytes[at] };

                outLines.push_back (std::move (data));
            }

            break;
        }

        line.bytes.assign (bytes.begin() + (ptrdiff_t) at, bytes.begin() + (ptrdiff_t) (at + size));
        line.mnemonic  = std::string (s_kMnemonics + opcode * 3, 3);
        line.operand   = FormatOperand (mode, std::span<const Byte> (line.bytes).subspan (1), (Word) (origin + at + size));
        line.isDefined = true;

        if (followFlags && (opcode == kRep || opcode == kSep))
        {
            wideM = (bytes[at + 1] & kFlagM) ? (opcode == kRep) : wideM;
            wideX = (bytes[at + 1] & kFlagX) ? (opcode == kRep) : wideX;
        }

        outLines.push_back (std::move (line));
        at += size;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler65816::GetOperandSize
//
////////////////////////////////////////////////////////////////////////////////

size_t Disassembler65816::GetOperandSize (char mode, bool wideM, bool wideX)
{
    switch (mode)
    {
        case kImplied:
        case kAccumulator:      return 0;

        case kImmediateM:       return wideM ? 2 : 1;
        case kImmediateX:       return wideX ? 2 : 1;

        case kAbsolute:
        case kAbsoluteX:
        case kAbsoluteY:
        case kIndirect:
        case kXIndirect:
        case kIndirectLong:
        case kRelativeLong:
        case kBlockMove:        return 2;

        case kLong:
        case kLongX:            return 3;

        default:                return 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler65816::FormatOperand
//
//  A branch's operand is its target. A block move's bytes are the
//  destination bank then the source bank, written source first.
//
////////////////////////////////////////////////////////////////////////////////

std::string Disassembler65816::FormatOperand (char mode, std::span<const Byte> operand, Word next)
{
    std::string  byte0 = operand.empty() ? std::string() : "$" + Disassembler::FormatHexByte (operand[0]);
    std::string  word  = (operand.size() < 2) ? std::string() : "$" + Disassembler::FormatHexWord ((Word) (operand[0] | (operand[1] << 8)));
    std::string  lng   = (operand.size() < 3) ? std::string() : "$" + Disassembler::FormatHexByte (operand[2]) + word.substr (1);



    switch (mode)
    {
        case kAccumulator:      return "A";
        case kImmediateM:
        case kImmediateX:
        case kImmediate8:       return "#" + ((operand.size() == 2) ? word : byte0);
        case kDirect:           return byte0;
        case kDirectX:          return byte0 + ",X";
        case kDirectY:          return byte0 + ",Y";
        case kDirectIndirect:   return "(" + byte0 + ")";
        case kDirectXIndirect:  return "(" + byte0 + ",X)";
        case kDirectIndirectY:  return "(" + byte0 + "),Y";
        case kDirectLong:       return "[" + byte0 + "]";
        case kDirectLongY:      return "[" + byte0 + "],Y";
        case kAbsolute:         return word;
        case kAbsoluteX:        return word + ",X";
        case kAbsoluteY:        return word + ",Y";
        case kLong:             return lng;
        case kLongX:            return lng + ",X";
        case kIndirect:         return "(" + word + ")";
        case kXIndirect:        return "(" + word + ",X)";
        case kIndirectLong:     return "[" + word + "]";
        case kStack:            return byte0 + ",S";
        case kStackIndirectY:   return "(" + byte0 + ",S),Y";
        case kRelative:         return "$" + Disassembler::FormatHexWord ((Word) (next + (int8_t) operand[0]));
        case kRelativeLong:     return "$" + Disassembler::FormatHexWord ((Word) (next + (int16_t) (operand[0] | (operand[1] << 8))));
        case kBlockMove:        return "$" + Disassembler::FormatHexByte (operand[1]) + ",$" + Disassembler::FormatHexByte (operand[0]);
        default:                return std::string();
    }
}
