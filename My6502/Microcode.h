#pragma once



#include "GlobalAddressingModes.h"
#include "Group00.h"
#include "Group01.h"
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
        Jump,
        Load,
        Or,
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
