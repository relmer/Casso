#pragma once





//
// This is the superset of all addressing modes across all groups
//

class GlobalAddressingMode
{
public:
    enum AddressingMode
    {
        __First     = 0,
        Immediate   = 0,     // #Immediate
        ZeroPage,            // Zero Page
        ZeroPageX,           // Zero Page, X
        ZeroPageY,           // Zero Page, Y
        Absolute,            // Absolute
        AbsoluteX,           // Absolute, X
        AbsoluteY,           // Absolute, Y
        ZeroPageXIndirect,   // (Zero Page, X) -> ($00LL + X)
        ZeroPageIndirectY,   // (Zero Page), Y
        Accumulator,         // Accumulator
        JumpAbsolute,        // Jump absolute (2 bytes)
        JumpIndirect,        // Jump indirect
        Relative,            // Relative offset (2 bytes)
        SingleByteNoOperand, // Single byte instruction (no operand)
        __Count
    };

    static constexpr const char * s_addressingModeName[] =
    {
        "#Immediate",
        "Zero Page",
        "Zero Page, X",
        "Zero Page, Y",
        "Absolute",
        "Absolute, X",
        "Absolute, Y",
        "(Zero Page, X)",
        "(Zero Page), Y",
        "Accumulator",
        "Jump Absolute",
        "Jump (Indirect)",
        "Relative",
        "[No Operand]",
    };
};
