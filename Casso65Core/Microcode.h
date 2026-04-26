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
        Increment,
        Jump,
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
};
