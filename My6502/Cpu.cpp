#include "Pch.h"

#include "Cpu.h"
#include "CpuOperations.h"
#include "Utils.h"



Cpu::Cpu ()
{
    InitializeInstructionSet ();
}



void Cpu::Reset ()
{
    /*
    PC = 0xFFFC;
    SP = 0x0100;
    */

    status.status = 0;

    A = 0;
    X = 0;
    Y = 0;

    memset (memory, 0, sizeof (memory));




    // Test code
    PC = 0x8000;
    SP = 0x0100;

    Word addr = PC;


    // Immediate
    //memory[addr++] = 0x09;  // ORA
    //memory[addr++] = 0x0F;  // Immediate
    //memory[addr++] = 0x09;  // ORA
    //memory[addr++] = 0xF0;  // Immediate
    
    // Zero page + X, indirect
    //X = 0x11;
    //memory[addr++] = 0x01;  // ORA
    //memory[addr++] = 0x11;  // ($0011,X)
    //memory[memory[addr - 1] + X]     = 0x45;
    //memory[memory[addr - 1] + X + 1] = 0x23;
    //memory[0x2345] = 0x55;

    // Zero page indirect, Y
    //Y = 0x22;
    //memory[addr++] = 0x11;  // ORA
    //memory[addr++] = 0x33;  // (22),Y
    //memory[memory[addr - 1]] = 0x56;
    //memory[memory[addr - 1] + 1] = 0x34;
    //memory[0x3456 + Y] = 0x44;

    // Zero page
    //memory[addr++] = 0x05;  // ORA
    //memory[addr++] = 0x55;  // ($0055)
    //memory[memory[addr - 1]] = 0x44;

    // Zero page, X
    //X = 0x77;
    //memory[addr++] = 0x15;  // ORA
    //memory[addr++] = 0x66;  // ($0055)
    //memory[(memory[addr - 1] + X) & 0xFF] = 0xEE;

    // Absolute
    //memory[addr++] = 0x0D;  // ORA
    //memory[addr++] = 0x34;  // NB:  Little endian
    //memory[addr++] = 0x12;  // ($1234)
    //memory[0x1234] = 0x55;

    // Absolute, X
    X = 0x06;
    memory[addr++] = 0x1D;  // ORA
    memory[addr++] = 0x34;  // NB:  Little endian
    memory[addr++] = 0x12;  // ($1234)
    memory[0x123A] = 0x99;

    // Absolute, Y
    Y = 0x0C;
    memory[addr++] = 0x19;  // ORA
    memory[addr++] = 0x34;  // NB:  Little endian
    memory[addr++] = 0x12;  // ($1234)
    memory[0x1240] = 0xAA;
}



void Cpu::Run ()
{
    do
    {
        Word        initialPC   = PC;
        Byte        opcode      = memory[PC];
        Microcode   microcode   = instructionSet[opcode];
        OperandInfo operandInfo = { 0 };

        FetchOperand        (microcode, operandInfo);
        PrintSingleStepInfo (initialPC, opcode, operandInfo);

        if (!instructionSet[opcode].isLegal)
        {
            break;
        }

        ExecuteInstruction  (instructionSet[opcode], operandInfo);

        std::printf ("\n");

        ++PC;
    }
    while (1);
}



void Cpu::PrintSingleStepInfo (Word initialPC, Byte opcode, const OperandInfo & operandInfo)
{
    // Print the registers and the opcode byte
    std::printf ("SP: %04x    A: %04X    X: %04X    Y: %04X        [%04X] %02X ",
                  SP,
                  A,
                  X,
                  Y,
                  initialPC,
                  opcode);

    PrintOperandBytes (initialPC, opcode);

    // print the instruction name
    std::printf ("%s ", instructionSet[opcode].instructionName);

    PrintOperandAndComment (opcode, operandInfo);
}



void Cpu::PrintOperandBytes (Word initialPC, Byte opcode)
{
    // Print the operand bytes
    switch (instructionSet[opcode].instruction.asBits.addressingMode)
    {
    case Group01::AM_ZeroPageXIndirect:
    case Group01::AM_ZeroPage:
    case Group01::AM_Immediate:
    case Group01::AM_ZeroPageIndirectY:
    case Group01::AM_ZeroPageX:
        std::printf ("%02X           ", memory[initialPC + 1]);
        break;

    case Group01::AM_Absolute:
    case Group01::AM_AbsoluteY:
    case Group01::AM_AbsoluteX:
        std::printf ("%02X %02X        ", memory[initialPC + 1], memory[initialPC + 2]);
        break;
    }
}



void Cpu::PrintOperandAndComment (Byte opcode, const OperandInfo & operandInfo)
{
    if (!instructionSet[opcode].isLegal)
    {
        return;
    }

    // print the operand and comment if applicable
    switch (instructionSet[opcode].instruction.asBits.addressingMode)
    {
    case Group01::AM_ZeroPageXIndirect:
        printf ("($%02X,X) ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
        break;

    case Group01::AM_ZeroPage:
        printf ("$%02X     ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case Group01::AM_Immediate:
        printf ("#$%02X", operandInfo.operand);
        break;

    case Group01::AM_Absolute:
        printf ("$%04X   ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case Group01::AM_ZeroPageIndirectY:
        printf ("($%02X),Y ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
        break;

    case Group01::AM_ZeroPageX:
        printf ("$%02X,X   ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case Group01::AM_AbsoluteY:
        printf ("$%04X,Y ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case Group01::AM_AbsoluteX:
        printf ("$%04X,X ; $%02X", operandInfo.location, operandInfo.operand);
        break;
    }
}



void Cpu::FetchOperand (Microcode microcode, OperandInfo & operandInfo)
{
    operandInfo.location         = 0;
    operandInfo.effectiveAddress = 0;
    operandInfo.operand          = 0;

    if (microcode.isSingleByte || !microcode.isLegal)
    {
        return;
    }

    // Advance the program counter to the operand byte
    ++PC;

    switch (microcode.instruction.asBits.addressingMode)
    {
    case Group01::AM_ZeroPageXIndirect:
        operandInfo.location          = memory[PC];
        operandInfo.effectiveAddress  = memory[(operandInfo.location + X)     & 0xFF]
                                      | memory[(operandInfo.location + X + 1) & 0xFF] << 8;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    case Group01::AM_ZeroPage:
        operandInfo.location          = memory[PC];
        operandInfo.effectiveAddress  = operandInfo.location;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    case Group01::AM_Immediate:
        operandInfo.location          = memory[PC];
        operandInfo.operand           = operandInfo.location;
        break;

    case Group01::AM_Absolute:
        operandInfo.location          = memory[PC]
                                      | memory[++PC] << 8;
        operandInfo.effectiveAddress  = operandInfo.location;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    case Group01::AM_ZeroPageIndirectY:
        operandInfo.location          = memory[PC];
        operandInfo.effectiveAddress  = memory[operandInfo.location]
                                      | memory[operandInfo.location + 1] << 8;
        operandInfo.effectiveAddress += Y;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    case Group01::AM_ZeroPageX:
        operandInfo.location          = memory[PC];
        operandInfo.effectiveAddress  = operandInfo.location + X;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    case Group01::AM_AbsoluteY:
        operandInfo.location          = memory[PC];
        operandInfo.location         |= memory[++PC] << 8;
        operandInfo.effectiveAddress  = operandInfo.location;
        operandInfo.effectiveAddress += Y;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    case Group01::AM_AbsoluteX:
        operandInfo.location          = memory[PC];
        operandInfo.location         |= memory[++PC] << 8;
        operandInfo.effectiveAddress  = operandInfo.location;
        operandInfo.effectiveAddress += X;
        operandInfo.operand           = memory[operandInfo.effectiveAddress];
        break;

    default:
        std::printf ("Unhandled addressing mode %d\n", microcode.instruction.asBits.addressingMode);
        break;
    }

    return;
}



void Cpu::ExecuteInstruction (Microcode microcode, const OperandInfo & operandInfo)
{
    switch (microcode.operation)
    {
    case Microcode::Or:
        CpuOperations::Or (*this, *microcode.pRegisterAffected, (Byte) operandInfo.operand);
        break;

    case Microcode::And:
        CpuOperations::And (*this, *microcode.pRegisterAffected, (Byte) operandInfo.operand);
        break;

    case Microcode::Xor:
        CpuOperations::Xor (*this, *microcode.pRegisterAffected, (Byte) operandInfo.operand);
        break;

    case Microcode::AddWithCarry:
        CpuOperations::AddWithCarry (*this, *microcode.pRegisterAffected, (Byte) operandInfo.operand);
        break;

    default:
        std::printf ("Unimplemented instruction:  %s\n", microcode.instructionName);
        break;

    }
}



void Cpu::InitializeInstructionSet ()
{
    InitializeGroup01 ();

    PrintInstructionSet ();
}



void Cpu::InitializeGroup01 ()
{
    struct TableEntry
    {
        Group01::Opcode        opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pRegisterAffected;
    };

    TableEntry table[] =
    {
        { Group01::ORA, Group01::__AMF_AllModes,                             Microcode::Or,                &A      },
        { Group01::AND, Group01::__AMF_AllModes,                             Microcode::And,               &A      },
        { Group01::EOR, Group01::__AMF_AllModes,                             Microcode::Xor,               &A      },
        { Group01::ADC, Group01::__AMF_AllModes,                             Microcode::AddWithCarry,      &A      },
        { Group01::STA, Group01::__AMF_AllModes & ~(Group01::AMF_Immediate), Microcode::Store,             &A      },
        { Group01::LDA, Group01::__AMF_AllModes,                             Microcode::Load,              &A      },
        { Group01::CMP, Group01::__AMF_AllModes,                             Microcode::Compare,           nullptr },
        { Group01::SBC, Group01::__AMF_AllModes,                             Microcode::SubtractWithCarry, &A      },
    };


    for (TableEntry entry : table)
    {
        CreateGroup01Instruction (entry.opcode, entry.addressingModeFlags, entry.operation, entry.pRegisterAffected);
    }
}



void Cpu::CreateGroup01Instruction (Group01::Opcode        opcode,
                                    Byte                   addressingModeFlags,
                                    Microcode::Operation   operation,
                                    Byte                 * pRegisterAffected)
{
    Byte addressingMode = Group01::__AM_First;
    Byte currentAddressingModeFlag = 1;

    while (addressingMode < Group01::__AM_Count)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction instruction = Group01::CreateInstruction (opcode, (Group01::AddressingMode) addressingMode);
            instructionSet[instruction.asByte] = Microcode (instruction, Group01::instructionName[opcode], false, operation, pRegisterAffected);
        }

        ++addressingMode;
        currentAddressingModeFlag <<= 1;
    }
}



void Cpu::PrintInstructionSet ()
{
    for (size_t i = 0; i < ARRAYSIZE (instructionSet); i++)
    {
        if (instructionSet[i].isLegal)
        {
            Byte opcode = instructionSet[i].instruction.asBits.opcode;
            Byte addressingMode = instructionSet[i].instruction.asBits.addressingMode;

            std::printf ("Instruction %02X:  %s ($%02X) %s\n",
                         (unsigned int) i,
                         Group01::instructionName[opcode],
                         instructionSet[i].instruction.asByte,
                         Group01::addressingModeName[addressingMode]);
        }
    }
}



