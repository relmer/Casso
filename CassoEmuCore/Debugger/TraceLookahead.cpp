#include "Pch.h"

#include "Debugger/TraceLookahead.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TraceLookahead::FindNext
//
//  A byte that cannot be read ends the walk, as does an opcode the table has
//  no instruction for.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DisassembledInstruction> TraceLookahead::FindNext (
    const Microcode * instructionSet,
    Word              pc,
    Byte              p,
    const PeekFn    & peek,
    size_t            count)
{
    HRESULT                               hr          = S_OK;
    Disassembler                          disassembler (instructionSet);
    std::vector<DisassembledInstruction>  next;
    Word                                  address     = pc;
    bool                                  flagsKnown  = true;



    while (next.size() < count)
    {
        DisassembledInstruction  instruction;
        Byte                     bytes[Disassembler::kMaxInstructionBytes] = {};
        std::optional<bool>      taken;
        bool                     isRead                                    = false;



        isRead = peek (address, bytes[0]);

        if (!isRead)
        {
            break;
        }

        for (size_t i = 1; i < std::size (bytes); i++)
        {
            (void) peek ((Word) (address + i), bytes[i]);
        }

        hr = disassembler.DisassembleOne (address, bytes, instruction);

        if (FAILED (hr) || !instruction.isDefined)
        {
            break;
        }

        next.push_back (instruction);
        taken = IsBranchTaken (instruction.mnemonic, p);

        if (taken.has_value())
        {
            if (!flagsKnown)
            {
                break;
            }

            address = *taken ? instruction.target : (Word) (address + instruction.bytes.size());
            continue;
        }

        if (IsEnd (instruction.mnemonic))
        {
            break;
        }

        if (instruction.mnemonic == "JMP" || instruction.mnemonic == "JSR" || instruction.mnemonic == "BRA")
        {
            if (!instruction.hasTarget && !(instruction.mnemonic == "JSR" && instruction.hasOperandAddress))
            {
                break;
            }

            address = instruction.hasTarget ? instruction.target : instruction.operandAddress;
            continue;
        }

        flagsKnown = flagsKnown && DoesKeepFlags (instruction.mnemonic, p);
        address    = (Word) (address + instruction.bytes.size());
    }

    return next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceLookahead::IsBranchTaken
//
////////////////////////////////////////////////////////////////////////////////

std::optional<bool> TraceLookahead::IsBranchTaken (const std::string & mnemonic, Byte p)
{
    static constexpr Byte  kCarry    = 0x01;
    static constexpr Byte  kZero     = 0x02;
    static constexpr Byte  kOverflow = 0x40;
    static constexpr Byte  kNegative = 0x80;



    if (mnemonic == "BPL") { return (p & kNegative) == 0; }
    if (mnemonic == "BMI") { return (p & kNegative) != 0; }
    if (mnemonic == "BVC") { return (p & kOverflow) == 0; }
    if (mnemonic == "BVS") { return (p & kOverflow) != 0; }
    if (mnemonic == "BCC") { return (p & kCarry)    == 0; }
    if (mnemonic == "BCS") { return (p & kCarry)    != 0; }
    if (mnemonic == "BNE") { return (p & kZero)     == 0; }
    if (mnemonic == "BEQ") { return (p & kZero)     != 0; }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceLookahead::DoesKeepFlags
//
//  Only instructions that leave the flags alone, or set one to a known value,
//  keep them known; anything else may change them.
//
////////////////////////////////////////////////////////////////////////////////

bool TraceLookahead::DoesKeepFlags (const std::string & mnemonic, Byte & p)
{
    static constexpr Byte  kCarry     = 0x01;
    static constexpr Byte  kInterrupt = 0x04;
    static constexpr Byte  kDecimal   = 0x08;
    static constexpr Byte  kOverflow  = 0x40;
    static constexpr std::string_view  kKeepers[] = { "STA", "STX", "STY", "STZ", "NOP", "PHA", "PHP", "PHX", "PHY", "TXS" };



    if (mnemonic == "CLC") { p &= (Byte) ~kCarry;     return true; }
    if (mnemonic == "SEC") { p |= kCarry;             return true; }
    if (mnemonic == "CLI") { p &= (Byte) ~kInterrupt; return true; }
    if (mnemonic == "SEI") { p |= kInterrupt;         return true; }
    if (mnemonic == "CLD") { p &= (Byte) ~kDecimal;   return true; }
    if (mnemonic == "SED") { p |= kDecimal;           return true; }
    if (mnemonic == "CLV") { p &= (Byte) ~kOverflow;  return true; }

    return std::find (std::begin (kKeepers), std::end (kKeepers), mnemonic) != std::end (kKeepers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceLookahead::IsEnd
//
//  A return goes where the stack says, a bit branch on a byte of memory, and
//  the rest stop or interrupt the machine.
//
////////////////////////////////////////////////////////////////////////////////

bool TraceLookahead::IsEnd (const std::string & mnemonic)
{
    static constexpr std::string_view  kEnds[] = { "RTS", "RTI", "BRK", "STP", "WAI" };



    if (mnemonic.starts_with ("BBR") || mnemonic.starts_with ("BBS"))
    {
        return true;
    }

    return std::find (std::begin (kEnds), std::end (kEnds), mnemonic) != std::end (kEnds);
}
