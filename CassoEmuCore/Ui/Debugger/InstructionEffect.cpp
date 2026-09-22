#include "Pch.h"

#include "Ui/Debugger/InstructionEffect.h"





static constexpr Byte  kCarry    = 0x01;
static constexpr Byte  kZero     = 0x02;
static constexpr Byte  kDecimal  = 0x08;
static constexpr Byte  kOverflow = 0x40;
static constexpr Byte  kNegative = 0x80;





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect::GetFlags
//
//  The two flags every load, transfer, logic and counting instruction sets
//  from its result.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstructionEffect::GetFlags (Byte value)
{
    return std::format (" N={} Z={}", (value & kNegative) ? 1 : 0, (value == 0) ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect::GetCompare
//
//  A compare subtracts without keeping the difference: carry says the
//  register was at least the operand.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstructionEffect::GetCompare (Byte left, Byte right)
{
    Byte  difference = (Byte) (left - right);



    return std::format ("N={} Z={} C={}", (difference & kNegative) ? 1 : 0, (left == right) ? 1 : 0, (left >= right) ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect::Describe
//
//  Grouped as the 6502's own documentation groups them: what the instruction
//  leaves in a register, what it leaves in memory, or where it leaves the PC.
//
//  DECIMAL MODE IS NOT PREDICTED. ADC and SBC with D set produce a result
//  this would have to model a second way, and an answer that is wrong on a
//  machine in decimal mode is worse than none.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstructionEffect::Describe (const Input & input)
{
    const std::string       & name   = input.mnemonic;
    const Cpu6502Registers  & r      = input.registers;
    Byte                      value  = input.value.value_or (0);
    Byte                      result = 0;
    unsigned                  wide   = 0;



    //  Loads and transfers: the byte moved, and the two flags it sets.
    if (name == "LDA" && input.value.has_value()) { return std::format ("A={:02X}{}", value, GetFlags (value)); }
    if (name == "LDX" && input.value.has_value()) { return std::format ("X={:02X}{}", value, GetFlags (value)); }
    if (name == "LDY" && input.value.has_value()) { return std::format ("Y={:02X}{}", value, GetFlags (value)); }

    if (name == "TAX") { return std::format ("X={:02X}{}", r.a,  GetFlags (r.a));  }
    if (name == "TAY") { return std::format ("Y={:02X}{}", r.a,  GetFlags (r.a));  }
    if (name == "TXA") { return std::format ("A={:02X}{}", r.x,  GetFlags (r.x));  }
    if (name == "TYA") { return std::format ("A={:02X}{}", r.y,  GetFlags (r.y));  }
    if (name == "TSX") { return std::format ("X={:02X}{}", r.sp, GetFlags (r.sp)); }
    if (name == "TXS") { return std::format ("S={:02X}", r.x); }

    //  Counting, in a register or in memory.
    if (name == "INX") { result = (Byte) (r.x + 1); return std::format ("X={:02X}{}", result, GetFlags (result)); }
    if (name == "DEX") { result = (Byte) (r.x - 1); return std::format ("X={:02X}{}", result, GetFlags (result)); }
    if (name == "INY") { result = (Byte) (r.y + 1); return std::format ("Y={:02X}{}", result, GetFlags (result)); }
    if (name == "DEY") { result = (Byte) (r.y - 1); return std::format ("Y={:02X}{}", result, GetFlags (result)); }

    if ((name == "INC" || name == "DEC") && input.value.has_value())
    {
        result = (Byte) (name == "INC" ? value + 1 : value - 1);

        return input.address.has_value() ? std::format ("${:04X}={:02X}{}", *input.address, result, GetFlags (result))
                                         : std::format ("A={:02X}{}", result, GetFlags (result));
    }

    //  Stores: the byte the instruction puts there.
    if (name == "STA" && input.address.has_value()) { return std::format ("${:04X}={:02X}", *input.address, r.a); }
    if (name == "STX" && input.address.has_value()) { return std::format ("${:04X}={:02X}", *input.address, r.x); }
    if (name == "STY" && input.address.has_value()) { return std::format ("${:04X}={:02X}", *input.address, r.y); }
    if (name == "STZ" && input.address.has_value()) { return std::format ("${:04X}=00", *input.address); }

    //  Compares, which keep no result.
    if (name == "CMP" && input.value.has_value()) { return GetCompare (r.a, value); }
    if (name == "CPX" && input.value.has_value()) { return GetCompare (r.x, value); }
    if (name == "CPY" && input.value.has_value()) { return GetCompare (r.y, value); }

    //  Logic.
    if (name == "AND" && input.value.has_value()) { result = (Byte) (r.a & value); return std::format ("A={:02X}{}", result, GetFlags (result)); }
    if (name == "ORA" && input.value.has_value()) { result = (Byte) (r.a | value); return std::format ("A={:02X}{}", result, GetFlags (result)); }
    if (name == "EOR" && input.value.has_value()) { result = (Byte) (r.a ^ value); return std::format ("A={:02X}{}", result, GetFlags (result)); }

    //  BIT keeps nothing either: Z comes from the AND, N and V from the byte.
    if (name == "BIT" && input.value.has_value())
    {
        return std::format ("N={} V={} Z={}", (value & kNegative) ? 1 : 0, (value & kOverflow) ? 1 : 0,
                            ((r.a & value) == 0) ? 1 : 0);
    }

    //  Arithmetic, binary mode only.
    if ((name == "ADC" || name == "SBC") && input.value.has_value() && (r.p & kDecimal) == 0)
    {
        Byte  operand = (name == "ADC") ? value : (Byte) ~value;

        wide   = (unsigned) r.a + operand + ((r.p & kCarry) ? 1u : 0u);
        result = (Byte) wide;

        return std::format ("A={:02X}{} C={} V={}", result, GetFlags (result), (wide > 0xFF) ? 1 : 0,
                            (((r.a ^ result) & (operand ^ result) & kNegative) != 0) ? 1 : 0);
    }

    //  Shifts and rotates, on the accumulator or on memory.
    if ((name == "ASL" || name == "LSR" || name == "ROL" || name == "ROR") && input.value.has_value())
    {
        Byte  carryIn  = (Byte) ((r.p & kCarry) ? 1 : 0);
        Byte  carryOut = 0;

        if (name == "ASL") { carryOut = (Byte) ((value & kNegative) ? 1 : 0); result = (Byte) (value << 1); }
        if (name == "LSR") { carryOut = (Byte) (value & kCarry);              result = (Byte) (value >> 1); }
        if (name == "ROL") { carryOut = (Byte) ((value & kNegative) ? 1 : 0); result = (Byte) ((value << 1) | carryIn); }
        if (name == "ROR") { carryOut = (Byte) (value & kCarry);              result = (Byte) ((value >> 1) | (Byte) (carryIn << 7)); }

        return input.address.has_value() ? std::format ("${:04X}={:02X}{} C={}", *input.address, result, GetFlags (result), carryOut)
                                         : std::format ("A={:02X}{} C={}", result, GetFlags (result), carryOut);
    }

    //  The flag instructions say what they set in their own names.
    if (name == "SEC") { return "C=1"; }
    if (name == "CLC") { return "C=0"; }
    if (name == "SEI") { return "I=1"; }
    if (name == "CLI") { return "I=0"; }
    if (name == "SED") { return "D=1"; }
    if (name == "CLD") { return "D=0"; }
    if (name == "CLV") { return "V=0"; }

    //  The stack.
    if (name == "PHA") { return std::format ("${:04X}={:02X} S={:02X}", 0x0100 + r.sp, r.a, (Byte) (r.sp - 1)); }
    if (name == "PHP") { return std::format ("${:04X}={:02X} S={:02X}", 0x0100 + r.sp, r.p, (Byte) (r.sp - 1)); }
    if (name == "PLA" && input.value.has_value()) { return std::format ("A={:02X}{} S={:02X}", value, GetFlags (value), (Byte) (r.sp + 1)); }
    if (name == "PLP") { return std::format ("S={:02X}", (Byte) (r.sp + 1)); }

    //  Where the PC lands. A branch says whether it is taken, since that is
    //  the question being asked of it.
    if (name == "JMP" && input.target.has_value()) { return std::format ("PC=${:04X}", *input.target); }

    if (name == "JSR" && input.target.has_value())
    {
        return std::format ("PC=${:04X} S={:02X}", *input.target, (Byte) (r.sp - 2));
    }

    if (name.size() == 3 && name[0] == 'B' && input.target.has_value())
    {
        static const std::pair<const char *, std::pair<Byte, bool>>  kBranches[] =
        {
            { "BCC", { kCarry,    false } }, { "BCS", { kCarry,    true } },
            { "BNE", { kZero,     false } }, { "BEQ", { kZero,     true } },
            { "BVC", { kOverflow, false } }, { "BVS", { kOverflow, true } },
            { "BPL", { kNegative, false } }, { "BMI", { kNegative, true } },
        };

        for (const auto & [mnemonic, test] : kBranches)
        {
            if (name == mnemonic)
            {
                bool  taken = ((r.p & test.first) != 0) == test.second;

                return std::format ("PC=${:04X} {}", taken ? *input.target : input.next, taken ? "taken" : "not taken");
            }
        }
    }

    return {};
}
