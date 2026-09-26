#include "Pch.h"

#include "Debugger/LineAssembler.h"

#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::LineAssembler
//
//  The opcode table decides which CPU is being assembled for: the base table
//  for a 6502, the extended one for a 65C02.
//
////////////////////////////////////////////////////////////////////////////////

LineAssembler::LineAssembler (const OpcodeTable & opcodes) :
    m_opcodes (opcodes)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::TryAssemble
//
//  Assembles one line in the Monitor mini-assembler's syntax: a mnemonic and
//  an operand in hex, with or without $. A branch operand is the absolute
//  destination. An address written with one or two digits selects a zero-page
//  mode when the instruction has one.
//
////////////////////////////////////////////////////////////////////////////////

LineAssemblyStatus LineAssembler::TryAssemble (
    Word                 address,
    const std::string  & line,
    std::vector<Byte>  & outBytes,
    std::string        & outError) const
{
    HRESULT                               hr            = S_OK;
    LineAssemblyStatus                    status        = LineAssemblyStatus::Ok;
    std::string                           mnemonic;
    std::string                           operandText;
    Operand                               operand;
    ModeList                              candidates;
    OpcodeEntry                           entry         = {};
    GlobalAddressingMode::AddressingMode  chosen        = GlobalAddressingMode::SingleByteNoOperand;
    bool                                  isInstruction = false;
    bool                                  isParsed      = false;
    bool                                  isFound       = false;
    bool                                  isInRange     = false;



    outBytes.clear();
    outError.clear();

    SplitLine (line, mnemonic, operandText);

    isInstruction = m_opcodes.NamesAnInstruction (mnemonic);
    CBRF (isInstruction, SetFailure (status, LineAssemblyStatus::UnknownMnemonic, outError,
                                     std::format ("{} is not an instruction on this CPU.", mnemonic)));

    isParsed = TryParseOperand (operandText, operand);
    CBRF (isParsed, SetFailure (status, LineAssemblyStatus::InvalidOperand, outError,
                                std::format ("{} is not a valid operand.", operandText)));

    GetCandidateModes (operand, candidates);

    for (GlobalAddressingMode::AddressingMode mode : candidates)
    {
        isFound = m_opcodes.TryLookup (mnemonic, mode, entry);

        if (isFound)
        {
            chosen = mode;
            break;
        }
    }

    CBRF (isFound, SetFailure (status, LineAssemblyStatus::ModeNotAvailable, outError,
                               std::format ("{} has no addressing mode for operand {}.", mnemonic, operandText)));

    outBytes.push_back (entry.opcode);

    isInRange = TryEncodeOperand (address, chosen, entry.operandSize, operand, outBytes);
    CBRF (isInRange, SetFailure (status, LineAssemblyStatus::BranchOutOfRange, outError,
                                 std::format ("The branch target {} is more than 127 bytes away.", operandText)));

Error:
    if (status != LineAssemblyStatus::Ok)
    {
        outBytes.clear();
    }

    return status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::SplitLine
//
//  The mnemonic is the first word, upper-cased. The operand is the rest with
//  spaces removed, so `LDA ( $3E ) , Y` reads the same as `LDA ($3E),Y`.
//
////////////////////////////////////////////////////////////////////////////////

void LineAssembler::SplitLine (
    const std::string  & line,
    std::string        & mnemonic,
    std::string        & operandText)
{
    std::istringstream  stream (line);
    std::string         word;



    mnemonic.clear();
    operandText.clear();

    stream >> mnemonic;

    while (stream >> word)
    {
        operandText += word;
    }

    for (char & ch : mnemonic)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    for (char & ch : operandText)
    {
        ch = (char) toupper ((unsigned char) ch);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::TryParseHex
//
//  One to four hex digits with an optional $. isShort reports two digits or
//  fewer, which is how a zero-page operand is written.
//
////////////////////////////////////////////////////////////////////////////////

bool LineAssembler::TryParseHex (
    const std::string  & text,
    uint32_t           & value,
    bool               & isShort)
{
    static constexpr size_t  kMaxDigits      = 4;
    static constexpr size_t  kMaxShortDigits = 2;
    size_t                   start           = (!text.empty() && text[0] == '$') ? 1 : 0;
    size_t                   digits          = text.size() - start;
    bool                     isHex           = digits > 0 && digits <= kMaxDigits;



    value = 0;

    for (size_t i = start; isHex && i < text.size(); ++i)
    {
        isHex = isxdigit ((unsigned char) text[i]) != 0;
    }

    if (isHex)
    {
        value   = (uint32_t) std::stoul (text.substr (start), nullptr, 16);
        isShort = digits <= kMaxShortDigits;
    }

    return isHex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::TryParseOperand
//
////////////////////////////////////////////////////////////////////////////////

bool LineAssembler::TryParseOperand (const std::string & text, Operand & operand)
{
    static constexpr uint32_t  kMaxByte = 0xFF;
    bool                       isParsed = false;
    size_t                     comma    = text.find (',');



    operand = Operand();

    if (text.empty())
    {
        isParsed = true;
    }
    else if (text == "A")
    {
        operand.shape = OperandShape::Accumulator;
        isParsed      = true;
    }
    else if (text[0] == '#')
    {
        operand.shape = OperandShape::Immediate;
        isParsed      = TryParseHex (text.substr (1), operand.value, operand.isShort) && operand.value <= kMaxByte;
    }
    else if (text[0] == '(')
    {
        isParsed = TryParseEnclosed (text, operand);
    }
    else if (text.ends_with (",X") || text.ends_with (",Y"))
    {
        operand.shape = text.ends_with (",X") ? OperandShape::IndexedX : OperandShape::IndexedY;
        isParsed      = TryParseHex (text.substr (0, comma), operand.value, operand.isShort);
    }
    else if (comma != std::string::npos)
    {
        operand.shape = OperandShape::BitBranch;
        isParsed      = TryParseHex (text.substr (0, comma), operand.value, operand.isShort) &&
                        operand.isShort                                                    &&
                        TryParseHex (text.substr (comma + 1), operand.target, operand.isShort);
    }
    else
    {
        operand.shape = OperandShape::Plain;
        isParsed      = TryParseHex (text, operand.value, operand.isShort);
    }

    return isParsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::TryParseEnclosed
//
//  The three parenthesized forms: (addr), (addr,X) and (zp),Y.
//
////////////////////////////////////////////////////////////////////////////////

bool LineAssembler::TryParseEnclosed (const std::string & text, Operand & operand)
{
    static constexpr uint32_t  kMaxByte = 0xFF;
    static constexpr size_t    kSuffix  = 3;
    bool                       isParsed = false;



    if (text.ends_with (",X)"))
    {
        operand.shape = OperandShape::IndirectX;
        isParsed      = TryParseHex (text.substr (1, text.size() - 1 - kSuffix), operand.value, operand.isShort);
    }
    else if (text.ends_with ("),Y"))
    {
        operand.shape = OperandShape::IndirectY;
        isParsed      = TryParseHex (text.substr (1, text.size() - 1 - kSuffix), operand.value, operand.isShort) &&
                        operand.value <= kMaxByte;
    }
    else if (text.ends_with (")"))
    {
        operand.shape = OperandShape::Indirect;
        isParsed      = TryParseHex (text.substr (1, text.size() - 2), operand.value, operand.isShort);
    }

    return isParsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::GetCandidateModes
//
//  The addressing modes an operand could mean, most specific first. A short
//  address tries the zero-page form before the absolute one, and a plain
//  address tries a branch first, so a mnemonic that has one mode for the
//  written form picks it.
//
////////////////////////////////////////////////////////////////////////////////

void LineAssembler::GetCandidateModes (const Operand & operand, ModeList & modes)
{
    using Mode = GlobalAddressingMode;



    modes.clear();

    switch (operand.shape)
    {
    case OperandShape::None:
        modes = { Mode::SingleByteNoOperand, Mode::Accumulator };
        break;

    case OperandShape::Accumulator: modes = { Mode::Accumulator };       break;
    case OperandShape::Immediate:   modes = { Mode::Immediate };         break;
    case OperandShape::IndirectY:   modes = { Mode::ZeroPageIndirectY }; break;
    case OperandShape::BitBranch:   modes = { Mode::ZeroPageRelative };  break;

    case OperandShape::Plain:
        modes = operand.isShort ? ModeList { Mode::Relative, Mode::ZeroPage, Mode::Absolute, Mode::JumpAbsolute }
                                : ModeList { Mode::Relative, Mode::Absolute, Mode::JumpAbsolute };
        break;

    case OperandShape::IndexedX:
        modes = operand.isShort ? ModeList { Mode::ZeroPageX, Mode::AbsoluteX } : ModeList { Mode::AbsoluteX };
        break;

    case OperandShape::IndexedY:
        modes = operand.isShort ? ModeList { Mode::ZeroPageY, Mode::AbsoluteY } : ModeList { Mode::AbsoluteY };
        break;

    case OperandShape::Indirect:
        modes = operand.isShort ? ModeList { Mode::ZeroPageIndirect, Mode::JumpIndirect, Mode::JumpIndirectCmos }
                                : ModeList { Mode::JumpIndirect, Mode::JumpIndirectCmos };
        break;

    case OperandShape::IndirectX:
        modes = operand.isShort ? ModeList { Mode::ZeroPageXIndirect, Mode::AbsoluteXIndirect } : ModeList { Mode::AbsoluteXIndirect };
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::TryEncodeOperand
//
//  Appends the operand bytes. Returns false only for a branch whose target is
//  out of reach.
//
////////////////////////////////////////////////////////////////////////////////

bool LineAssembler::TryEncodeOperand (
    Word                                   address,
    GlobalAddressingMode::AddressingMode   mode,
    Byte                                   operandSize,
    const Operand                        & operand,
    std::vector<Byte>                    & outBytes)
{
    static constexpr int  kMinBranchOffset = -128;
    static constexpr int  kMaxBranchOffset = 127;
    static constexpr int  kBranchLength    = 2;
    static constexpr int  kBitBranchLength = 3;
    int                   offset           = 0;
    bool                  isInRange        = true;



    if (mode == GlobalAddressingMode::Relative)
    {
        offset    = (int16_t) (Word) (operand.value - (address + kBranchLength));
        isInRange = offset >= kMinBranchOffset && offset <= kMaxBranchOffset;
        outBytes.push_back ((Byte) offset);
    }
    else if (mode == GlobalAddressingMode::ZeroPageRelative)
    {
        offset    = (int16_t) (Word) (operand.target - (address + kBitBranchLength));
        isInRange = offset >= kMinBranchOffset && offset <= kMaxBranchOffset;
        outBytes.push_back ((Byte) operand.value);
        outBytes.push_back ((Byte) offset);
    }
    else if (operandSize == 1)
    {
        outBytes.push_back ((Byte) operand.value);
    }
    else if (operandSize == 2)
    {
        outBytes.push_back ((Byte) operand.value);
        outBytes.push_back ((Byte) (operand.value >> 8));
    }

    return isInRange;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler::SetFailure
//
////////////////////////////////////////////////////////////////////////////////

void LineAssembler::SetFailure (
    LineAssemblyStatus  & status,
    LineAssemblyStatus    value,
    std::string         & outError,
    const std::string   & message)
{
    status   = value;
    outError = message;
}
