#pragma once

#include "GlobalAddressingModes.h"
#include "Group00.h"
#include "Group01.h"
#include "Group10.h"
#include "Instruction.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Microcode
//
////////////////////////////////////////////////////////////////////////////////

class Microcode
{
public:
    enum Operation
    {
        AddWithCarry,
        And,
        BitTest,
        Branch,
        Break,
        Compare,
        Decrement,
        DecrementAndCompare,
        Increment,
        Jump,
        JumpSubroutine,
        Load,
        NoOperation,
        Or,
        Pull,
        Push,
        ReturnFromInterrupt,
        ReturnFromSubroutine,
        RotateLeft,
        RotateRight,
        SetFlag,
        ShiftLeft,
        ShiftRight,
        Store,
        SubtractWithCarry,
        Transfer,
        Xor,

        // 65C02 (CMOS) additions. Never referenced by the NMOS instruction
        // table, so NMOS dispatch is unaffected.
        StoreZero,          // STZ
        TestAndSetBits,     // TSB
        TestAndResetBits,   // TRB
        ResetMemoryBit,     // RMB0-7
        SetMemoryBit,       // SMB0-7
        BitBranchReset,     // BBR0-7
        BitBranchSet,       // BBS0-7
        BranchAlways,       // BRA
        BitTestImmediate,   // BIT #imm (affects Z only)

        // CMOS variants of NMOS operations that differ only in decimal mode /
        // interrupt entry. The 65C02 table points the affected opcodes here;
        // the NMOS ops (AddWithCarry / SubtractWithCarry / Break) are untouched.
        AddWithCarryCmos,       // ADC (decimal N/Z/V correct, +1 cycle)
        SubtractWithCarryCmos,  // SBC (decimal N/Z/V correct, +1 cycle)
        BreakCmos,              // BRK (clears decimal flag on entry)
    };

    enum Group
    {
        Group00 = 0b00,
        Group01 = 0b01,
        Group10 = 0b10,

        Misc    = 0x80,
        Invalid = 0xFF,
    };

public:
    Microcode ();

    Microcode (Instruction    instruction, 
               const char   * instructionName, 
               Operation      operation, 
               Byte         * pSourceRegister, 
               Byte         * pDestinationRegister);

    Microcode (Instruction                            instruction, 
               const char                           * instructionName, 
               Operation                              operation, 
               GlobalAddressingMode::AddressingMode   addressingMode, 
               Byte                                 * pSourceRegister, 
               Byte                                 * pDestinationRegister);


public:
    bool                                   isLegal;
    Instruction                            instruction;
    Group                                  group;
    const char                           * instructionName;
    Byte                                 * pSourceRegister;
    Byte                                 * pDestinationRegister;
    Operation                              operation;
    GlobalAddressingMode::AddressingMode   globalAddressingMode;
    Byte                                   baseCycles;

    // Legal to execute and disassemble, but hidden from the assembler's opcode
    // table. Used for the 65C02 reserved/undefined-opcode NOP fill: those slots
    // really do execute as NOPs on the CMOS part (and must, for conformance), but
    // the only NOP the assembler should ever emit is the canonical $EA -- so these
    // filler entries must not shadow it. Defaults false; set only by the fill.
    bool                                   assemblerHidden = false;
};
