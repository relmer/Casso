#include "Pch.h"

#include "Microcode.h"



Microcode::Microcode () :
    isLegal         (false),
    group           (Group::Invalid), 
    instructionName ("Illegal instruction")
{
}



Microcode::Microcode (Instruction    instruction,
                      const char   * instructionName, 
                      Operation      operation, 
                      Byte         * pSourceRegister, 
                      Byte         * pDestinationRegister) :

    isLegal              (true),
    group                ((Group) instruction.asBits.group),
    instruction          (instruction),
    instructionName      (instructionName),
    pSourceRegister      (pSourceRegister),
    pDestinationRegister (pDestinationRegister),
    operation            (operation)

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



Microcode::Microcode (Instruction                            instruction,
                      const char                           * instructionName, 
                      Operation                              operation, 
                      GlobalAddressingMode::AddressingMode   addressingMode, 
                      Byte                                 * pSourceRegister, 
                      Byte                                 * pDestinationRegister) :

    isLegal              (true),
    group                (Group::Misc),
    instruction          (instruction),
    instructionName      (instructionName),
    pSourceRegister      (pSourceRegister),
    pDestinationRegister (pDestinationRegister),
    operation            (operation)

{
    globalAddressingMode = addressingMode;
}
