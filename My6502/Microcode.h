#pragma once



#include "GlobalAddressingModes.h"
#include "Group00.h"
#include "Group01.h"
#include "Group10.h"
#include "Instruction.h"



class Microcode
{
public:
    enum Operation
    {
        AddWithCarry,
        And,
        BitTest,
        Compare,
        Decrement,
        Increment,
        Jump,
        Load,
        Or,
        RotateLeft,
        RotateRight,
        ShiftLeft,
        ShiftRight,
        Store,
        SubtractWithCarry,
        Xor,
    };

public:
    Microcode () :
        isLegal (false),
        instructionName ("Illegal instruction")
    {
    }

    Microcode (Instruction instruction, const char * instructionName, bool isSingleByte, Operation operation, Byte * pRegisterAffected) :
        isLegal           (true),
        instruction       (instruction),
        instructionName   (instructionName),
        isSingleByte      (isSingleByte),
        pRegisterAffected (pRegisterAffected),
        operation         (operation)
    {
        switch (instruction.asBits.group)
        {
        case 0b00:
            globalAddressingMode = (GlobalAddressingMode::AddressingMode) Group00::s_addressingModeMap[instruction.asBits.addressingMode];
            break;

        case 0b01:
            globalAddressingMode = (GlobalAddressingMode::AddressingMode) Group01::s_addressingModeMap[instruction.asBits.addressingMode];
            break;

        case 0b10:
            globalAddressingMode = (GlobalAddressingMode::AddressingMode) Group10::s_addressingModeMap[instruction.asBits.addressingMode];
            break;
        }

        // There are a few instructions that are encoded
        // as one addressing mode but actually use another.
        switch (instruction.asByte)
        {
        case 0x4C:  // JMP Absolute
            globalAddressingMode = GlobalAddressingMode::JumpAbsolute;
            break;

        case 0x6C:  // JMP (Indirect)
            globalAddressingMode = GlobalAddressingMode::JumpIndirect;
            break;

        case 0x96:  // STX ZeroPage, X
        case 0xB6:  // LDX ZeroPage, X
            globalAddressingMode = GlobalAddressingMode::ZeroPageY;
            break;

        case 0xBE:  // LDX Absolute, X
            globalAddressingMode = GlobalAddressingMode::AbsoluteY;
            break;
        }
    }

public:
    bool                                   isLegal;
    Instruction                            instruction;
    const char                           * instructionName;
    bool                                   isSingleByte;
    Byte                                 * pRegisterAffected;
    Operation                              operation;
    GlobalAddressingMode::AddressingMode   globalAddressingMode;
};
