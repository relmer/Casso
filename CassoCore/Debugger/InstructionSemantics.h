#pragma once

#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSemantics
//
//  What an operation does to the accumulator and the status flags, keyed by
//  the microcode's own Operation -- the value the CPU dispatches on.
//
//  MOST OF AN INSTRUCTION'S INPUTS AND OUTPUTS ARE ALREADY IN THE MICROCODE.
//  Its addressing mode gives the addresses and the index register, and its
//  source and destination pointers give the registers it moves between. Two
//  things are missing, and they are the two here:
//
//    * the ACCUMULATOR an ALU operation works on without being told to. ADC's
//      row holds no source register, because adding to A is what the
//      operation means.
//    * the FLAGS, which the CPU applies in code with no table behind them.
//
//  This says what the operation OPERATES ON, not what changed this time. An
//  operation that writes Z writes it whether or not Z ends up different, and
//  the pane shows the value either way; only the highlight asks whether it
//  moved.
//
//  A table written beside a CPU can drift from it, so a test runs every
//  opcode through the core and fails if an instruction changes a flag or a
//  register this does not declare.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionSemantics
{
public:
    static constexpr Byte  kCarry    = 0x01;
    static constexpr Byte  kZero     = 0x02;
    static constexpr Byte  kIrqOff   = 0x04;
    static constexpr Byte  kDecimal  = 0x08;
    static constexpr Byte  kBreak    = 0x10;
    static constexpr Byte  kOverflow = 0x40;
    static constexpr Byte  kNegative = 0x80;

    struct Effect
    {
        Byte  readsFlags       = 0;
        Byte  writesFlags      = 0;

        //  The accumulator the operation uses of its own accord, over and
        //  above any register the microcode row points at.
        bool  readsAccumulator  = false;
        bool  writesAccumulator = false;
    };

    static Effect  Find (Microcode::Operation operation);

    //  Whether the operation works on the register its row points at rather
    //  than on memory -- INX and ASL A do, ASL $0400 does not, though both
    //  ASL rows carry the same pointer.
    static bool    IsRegisterOperation (Microcode::Operation operation);
};
