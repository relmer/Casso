#include "Pch.h"

#include "Debugger/InstructionSemantics.h"





#define F_C  InstructionSemantics::kCarry
#define F_Z  InstructionSemantics::kZero
#define F_I  InstructionSemantics::kIrqOff
#define F_D  InstructionSemantics::kDecimal
#define F_B  InstructionSemantics::kBreak
#define F_V  InstructionSemantics::kOverflow
#define F_N  InstructionSemantics::kNegative
#define F_NZ (F_N | F_Z)
#define F_ALL (F_C | F_Z | F_I | F_D | F_B | F_V | F_N)



//  One row per operation: the flags it reads, the flags it writes, and
//  whether it uses the accumulator without the microcode row saying so.
//
//  Read as: adding takes the carry in and decimal mode decides how, and it
//  leaves N, V, Z and C behind, working on A at both ends.
static constexpr struct
{
    Microcode::Operation  operation;
    Byte                  reads;
    Byte                  writes;
    bool                  readsA;
    bool                  writesA;
}  s_kEffects[] =
{
    { Microcode::AddWithCarry,          F_C | F_D, F_N | F_V | F_Z | F_C, true,  true  },
    { Microcode::SubtractWithCarry,     F_C | F_D, F_N | F_V | F_Z | F_C, true,  true  },
    { Microcode::AddWithCarryCmos,      F_C | F_D, F_N | F_V | F_Z | F_C, true,  true  },
    { Microcode::SubtractWithCarryCmos, F_C | F_D, F_N | F_V | F_Z | F_C, true,  true  },

    { Microcode::And,                   0,         F_NZ,                  true,  true  },
    { Microcode::Or,                    0,         F_NZ,                  true,  true  },
    { Microcode::Xor,                   0,         F_NZ,                  true,  true  },

    //  A compare works on the register its row points at, so no accumulator
    //  of its own.
    { Microcode::Compare,               0,         F_N | F_Z | F_C,       false, false },

    { Microcode::Load,                  0,         F_NZ,                  false, false },
    { Microcode::Store,                 0,         0,                     false, false },
    { Microcode::StoreZero,             0,         0,                     false, false },
    { Microcode::Transfer,              0,         F_NZ,                  false, false },

    { Microcode::Increment,             0,         F_NZ,                  false, false },
    { Microcode::Decrement,             0,         F_NZ,                  false, false },

    { Microcode::ShiftLeft,             0,         F_NZ | F_C,            false, false },
    { Microcode::ShiftRight,            0,         F_NZ | F_C,            false, false },
    { Microcode::RotateLeft,            F_C,       F_NZ | F_C,            false, false },
    { Microcode::RotateRight,           F_C,       F_NZ | F_C,            false, false },

    { Microcode::BitTest,               0,         F_N | F_V | F_Z,       true,  false },
    { Microcode::BitTestImmediate,      0,         F_Z,                   true,  false },

    //  A branch reads the flag it tests. WHICH flag is in the opcode itself,
    //  not here: the caller reads it off the instruction.
    { Microcode::Branch,                0,         0,                     false, false },
    { Microcode::BranchAlways,          0,         0,                     false, false },
    { Microcode::BitBranchReset,        0,         0,                     false, false },
    { Microcode::BitBranchSet,          0,         0,                     false, false },

    //  SetFlag covers CLC, SEC, CLI, SEI, CLD, SED and CLV. WHICH flag is in
    //  the opcode, so the caller works it out the same way.
    { Microcode::SetFlag,               0,         0,                     false, false },

    { Microcode::Jump,                  0,         0,                     false, false },
    { Microcode::JumpSubroutine,        0,         0,                     false, false },
    { Microcode::ReturnFromSubroutine,  0,         0,                     false, false },
    { Microcode::ReturnFromInterrupt,   0,         F_ALL,                 false, false },
    { Microcode::Break,                 0,         F_I | F_B,             false, false },
    { Microcode::BreakCmos,             0,         F_I | F_B | F_D,       false, false },
    { Microcode::NoOperation,           0,         0,                     false, false },

    //  A push or pull moves the register its row points at. Pulling A sets
    //  the two flags; pulling the status register sets them all, and that
    //  row's destination IS the status register, so it is left to the caller.
    { Microcode::Push,                  0,         0,                     false, false },
    { Microcode::Pull,                  0,         F_NZ,                  false, false },

    { Microcode::TestAndSetBits,        0,         F_Z,                   true,  false },
    { Microcode::TestAndResetBits,      0,         F_Z,                   true,  false },
    { Microcode::ResetMemoryBit,        0,         0,                     false, false },
    { Microcode::SetMemoryBit,          0,         0,                     false, false },

    //  The undocumented instructions, each the sum of the two steps it
    //  fuses: a memory step and an ALU step that owns the flags.
    { Microcode::StoreAccumulatorAndX,  0,         0,                     true,  false },
    { Microcode::LoadAccumulatorAndX,   0,         F_NZ,                  false, true  },
    { Microcode::DecrementAndCompare,   0,         F_N | F_Z | F_C,       true,  false },
    { Microcode::ShiftLeftAndOr,        0,         F_NZ | F_C,            true,  true  },
    { Microcode::RotateLeftAndAnd,      F_C,       F_NZ | F_C,            true,  true  },
    { Microcode::ShiftRightAndXor,      0,         F_NZ | F_C,            true,  true  },
    { Microcode::RotateRightAndAdd,     F_C | F_D, F_N | F_V | F_Z | F_C, true,  true  },
    { Microcode::IncrementAndSubtract,  F_C | F_D, F_N | F_V | F_Z | F_C, true,  true  },
};



//  The operations that work on the register their row points at when the
//  addressing mode gives them no memory to work on.
static constexpr Microcode::Operation  s_kRegisterOperations[] =
{
    Microcode::Increment, Microcode::Decrement,
    Microcode::ShiftLeft, Microcode::ShiftRight, Microcode::RotateLeft, Microcode::RotateRight,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSemantics::Find
//
////////////////////////////////////////////////////////////////////////////////

InstructionSemantics::Effect InstructionSemantics::Find (Microcode::Operation operation)
{
    for (const auto & entry : s_kEffects)
    {
        if (entry.operation == operation)
        {
            return Effect { entry.reads, entry.writes, entry.readsA, entry.writesA };
        }
    }

    return Effect {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSemantics::IsRegisterOperation
//
////////////////////////////////////////////////////////////////////////////////

bool InstructionSemantics::IsRegisterOperation (Microcode::Operation operation)
{
    return std::find (std::begin (s_kRegisterOperations), std::end (s_kRegisterOperations), operation) != std::end (s_kRegisterOperations);
}
