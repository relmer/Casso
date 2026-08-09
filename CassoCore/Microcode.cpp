#include "Pch.h"

#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Microcode
//
////////////////////////////////////////////////////////////////////////////////

Microcode::Microcode() :
    isLegal              (false),
    group                (Group::Invalid), 
    instructionName      ("Illegal instruction"),
    pSourceRegister      (nullptr),
    pDestinationRegister (nullptr),
    operation            (Operation::NoOperation),
    globalAddressingMode (GlobalAddressingMode::SingleByteNoOperand),
    baseCycles           (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Microcode
//
//  Derives an instruction's addressing mode from its OPCODE BIT PATTERN, then
//  patches the handful that lie about themselves.
//
//  The 6502 encodes opcodes as aaabbbcc -- an operation, an addressing mode,
//  and a group -- so most of the addressing mode falls out of the bits via a
//  per-group lookup table rather than a 256-entry hand-written map. That is
//  the whole reason this constructor is short.
//
//  The exception list exists because the encoding is not perfectly regular.
//  A few opcodes are encoded as one addressing mode and actually use another
//  -- JMP absolute being the classic case -- so they are corrected by opcode
//  value after the bit-pattern pass. Trying to express those in the group
//  tables would break the regularity that makes the tables work.
//
//  Order matters: the general derivation runs first and the exceptions
//  overwrite it, so an exception need only state its own answer.
//
////////////////////////////////////////////////////////////////////////////////

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
    operation            (operation),
    globalAddressingMode (GlobalAddressingMode::SingleByteNoOperand),
    baseCycles           (0)

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





////////////////////////////////////////////////////////////////////////////////
//
//  Microcode
//
////////////////////////////////////////////////////////////////////////////////

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
    operation            (operation),
    globalAddressingMode (addressingMode),
    baseCycles           (0)

{
}
