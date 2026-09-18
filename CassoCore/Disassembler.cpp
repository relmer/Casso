#include "Pch.h"

#include "Disassembler.h"
#include "Microcode.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::Disassembler
//
//  The table is borrowed, not copied: it belongs to the CPU whose memory is
//  being disassembled, so a 6502 and a 65C02 each decode with their own.
//
////////////////////////////////////////////////////////////////////////////////

Disassembler::Disassembler (const Microcode * instructionSet) :
    m_instructionSet (instructionSet)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::GetLength
//
//  An opcode the table does not define is one byte long, which is how the
//  Monitor steps over it.
//
////////////////////////////////////////////////////////////////////////////////

size_t Disassembler::GetLength (Byte opcode) const
{
    const Microcode & microcode = m_instructionSet[opcode];
    size_t            length    = 1;



    if (microcode.isLegal)
    {
        length += OpcodeTable::GetOperandSize (microcode.globalAddressingMode);
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::DisassembleOne
//
//  Decodes the instruction at the front of bytes, which must hold at least as
//  many bytes as the instruction is long. Undocumented opcodes are the ones
//  the table hides from the assembler; an undefined opcode decodes as ???.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disassembler::DisassembleOne (
    Word                      address,
    std::span<const Byte>     bytes,
    DisassembledInstruction & instruction) const
{
    HRESULT   hr        = S_OK;
    size_t    available = bytes.size();
    size_t    length    = 0;
    bool      hasOpcode = available > 0;



    instruction = DisassembledInstruction();

    CBRAEx (m_instructionSet, E_UNEXPECTED);
    CBREx  (hasOpcode,        HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    length = GetLength (bytes[0]);
    CBREx (available >= length, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    Decode (address, bytes.first (length), instruction);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::Decode
//
//  The decode itself, once the caller has established that bytes holds the
//  whole instruction. It cannot fail, which is what lets the buffer walk use
//  it without an error path for a case it has already ruled out.
//
////////////////////////////////////////////////////////////////////////////////

void Disassembler::Decode (
    Word                      address,
    std::span<const Byte>     bytes,
    DisassembledInstruction & instruction) const
{
    const Microcode & microcode = m_instructionSet[bytes[0]];



    instruction            = DisassembledInstruction();
    instruction.address    = address;
    instruction.bytes.assign (bytes.begin(), bytes.end());
    instruction.isDefined  = microcode.isLegal;
    instruction.documented = microcode.isLegal && !microcode.assemblerHidden;
    instruction.mnemonic   = microcode.isLegal ? microcode.instructionName : kUndefinedMnemonic;

    if (microcode.isLegal)
    {
        FormatOperand (microcode.globalAddressingMode, address, bytes, instruction);
    }
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
    std::span<const Byte>                   bytes,
    Word                                    origin,
    const Microcode                       * instructionSet,
    std::vector<DisassembledInstruction>  & outLines)
{
    Disassembler  decoder (instructionSet);
    size_t        at    = 0;
    size_t        count = bytes.size();



    outLines.clear();

    if (instructionSet == nullptr)
    {
        return;
    }

    while (at < count)
    {
        DisassembledInstruction  line;
        size_t                   length = decoder.GetLength (bytes[at]);
        Word                     where  = (Word) (origin + at);

        if (at + length > count)
        {
            line.address  = where;
            line.bytes.assign (bytes.begin() + (ptrdiff_t) at, bytes.end());
            line.mnemonic = kUndefinedMnemonic;
            outLines.push_back (line);
            break;
        }

        decoder.Decode (where, bytes.subspan (at, length), line);
        outLines.push_back (line);
        at += length;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler::FormatLine
//
//  Address, up to three bytes padded to a fixed column, mnemonic, operand.
//
////////////////////////////////////////////////////////////////////////////////

std::string Disassembler::FormatLine (const DisassembledInstruction & line)
{
    constexpr size_t  kBytesColumnWidth = 9;   // "AA BB CC "



    std::string  text = std::format ("{:04X}  ", (unsigned) line.address);
    std::string  hex;



    for (Byte value : line.bytes)
    {
        hex += std::format ("{:02X} ", (unsigned) value);
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
//  Disassembler::FormatOperand
//
//  Zero-page operands print two digits and everything else four, as the
//  Monitor's own listing does. Branches print their destination, not the
//  displacement, because the displacement is the one number nobody reading a
//  listing wants to add up by hand.
//
////////////////////////////////////////////////////////////////////////////////

void Disassembler::FormatOperand (
    GlobalAddressingMode::AddressingMode   mode,
    Word                                   address,
    std::span<const Byte>                  bytes,
    DisassembledInstruction              & instruction)
{
    static constexpr Word  kBranchLength    = 2;
    static constexpr Word  kBitBranchLength = 3;
    Byte                   zp               = 0;
    Word                   absolute         = 0;
    Word                   target           = 0;



    if (bytes.size() > 1)
    {
        zp = bytes[1];
    }

    if (bytes.size() > 2)
    {
        absolute = (Word) (bytes[1] | (bytes[2] << 8));
    }

    switch (mode)
    {
    case GlobalAddressingMode::Immediate:         instruction.operand = std::format ("#${:02X}",     zp);       break;
    case GlobalAddressingMode::ZeroPage:          instruction.operand = std::format ("${:02X}",      zp);       break;
    case GlobalAddressingMode::ZeroPageX:         instruction.operand = std::format ("${:02X},X",    zp);       break;
    case GlobalAddressingMode::ZeroPageY:         instruction.operand = std::format ("${:02X},Y",    zp);       break;
    case GlobalAddressingMode::ZeroPageXIndirect: instruction.operand = std::format ("(${:02X},X)",  zp);       break;
    case GlobalAddressingMode::ZeroPageIndirectY: instruction.operand = std::format ("(${:02X}),Y",  zp);       break;
    case GlobalAddressingMode::ZeroPageIndirect:  instruction.operand = std::format ("(${:02X})",    zp);       break;
    case GlobalAddressingMode::Absolute:          instruction.operand = std::format ("${:04X}",      absolute); break;
    case GlobalAddressingMode::AbsoluteX:         instruction.operand = std::format ("${:04X},X",    absolute); break;
    case GlobalAddressingMode::AbsoluteY:         instruction.operand = std::format ("${:04X},Y",    absolute); break;
    case GlobalAddressingMode::JumpIndirect:
    case GlobalAddressingMode::JumpIndirectCmos:  instruction.operand = std::format ("(${:04X})",    absolute); break;
    case GlobalAddressingMode::AbsoluteXIndirect: instruction.operand = std::format ("(${:04X},X)",  absolute); break;

    case GlobalAddressingMode::JumpAbsolute:
        instruction.hasTarget = true;
        instruction.target    = absolute;
        instruction.operand   = std::format ("${:04X}", absolute);
        break;

    case GlobalAddressingMode::Relative:
        target                = (Word) (address + kBranchLength + (SByte) zp);
        instruction.hasTarget = true;
        instruction.target    = target;
        instruction.operand   = std::format ("${:04X}", target);
        break;

    case GlobalAddressingMode::ZeroPageRelative:
        target                = (Word) (address + kBitBranchLength + (SByte) bytes[2]);
        instruction.hasTarget = true;
        instruction.target    = target;
        instruction.operand   = std::format ("${:02X},${:04X}", zp, target);
        break;

    case GlobalAddressingMode::Accumulator:
        instruction.operand = "A";
        break;

    case GlobalAddressingMode::SingleByteNoOperand:
    default:
        break;
    }
}
