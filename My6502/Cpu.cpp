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
    //memory[addr++] = 0x11;  // ($0011 + X)
    //memory[memory[addr - 1] + X] = 0x33;
    //memory[addr++] = 0x01;  // ORA

    // Zero page
    //memory[addr++] = 0x05;  // ORA
    //memory[addr++] = 0x55;  // ($0055)
    //memory[memory[addr - 1]] = 0x44;
    //memory[addr++] = 0x09;  // ORA
    //memory[addr++] = 0x10;  // #$10

    // Absolute
    memory[addr++] = 0x0D;  // ORA
    memory[addr++] = 0x34;  // NB:  Little endian
    memory[addr++] = 0x12;  // ($1234)
    memory[0x1234] = 0x55;
    memory[addr++] = 0x09;  // ORA
    memory[addr++] = 0x10;  // #$10
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

        ExecuteInstruction  (instructionSet[opcode], operandInfo.operand);

        std::printf ("\n");

        ++PC;
    }
    while (1);
}



void Cpu::PrintSingleStepInfo (Word initialPC, Byte opcode, OperandInfo & operandInfo)
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



void Cpu::PrintOperandAndComment (Byte opcode, Cpu::OperandInfo & operandInfo)
{
    if (!instructionSet[opcode].isLegal)
    {
        return;
    }

    // print the operand and comment if applicable
    switch (instructionSet[opcode].instruction.asBits.addressingMode)
    {
    case Group01::AM_ZeroPageXIndirect:
        printf ("($%02X,X)  ; $%02X", operandInfo.offset, operandInfo.operand);
        break;

    case Group01::AM_ZeroPage:
        printf ("$%02X      ; $%02X", operandInfo.offset, operandInfo.operand);
        break;

    case Group01::AM_Immediate:
        printf ("#$%02X", operandInfo.operand);
        break;

    case Group01::AM_Absolute:
        printf ("$%04X      ; $%02X", operandInfo.offset, operandInfo.operand);
        break;

        /*
        case Group01::AM_ZeroPageIndirectY:
        return memory[(memory[PC] + Y) & 0xFF];

        case Group01::AM_ZeroPageX:
        return memory[(memory[PC] + Y) & 0xFF];

        case Group01::AM_AbsoluteY:
        return memory[ReadWord (PC) + Y];

        case Group01::AM_AbsoluteX:
        return memory[ReadWord (PC) + X];
        */
    }
}



void Cpu::FetchOperand (Microcode microcode, OperandInfo & operandInfo)
{
    operandInfo.offset  = 0;
    operandInfo.operand = 0;

    if (microcode.isSingleByte || !microcode.isLegal)
    {
        return;
    }

    // Advance the program counter to the operand byte
    ++PC;

    switch (microcode.instruction.asBits.addressingMode)
    {
        case Group01::AM_ZeroPageXIndirect:
            operandInfo.offset  = memory[PC];
            operandInfo.operand = memory[operandInfo.offset + X];
            break;

        case Group01::AM_ZeroPage:
            operandInfo.offset  = memory[PC];
            operandInfo.operand = memory[operandInfo.offset];
            break;

        case Group01::AM_Immediate:
            operandInfo.operand = memory[PC];
            break;

        case Group01::AM_Absolute:
            operandInfo.offset  = memory[PC];
            operandInfo.offset |= memory[++PC] << 8;
            operandInfo.operand = memory[operandInfo.offset];
            break;


        /*
            case Group01::AM_ZeroPageIndirectY:
                return memory[(memory[PC] + Y) & 0xFF];

            case Group01::AM_ZeroPageX:
                return memory[(memory[PC] + Y) & 0xFF];

            case Group01::AM_AbsoluteY:
                return memory[ReadWord (PC) + Y];

            case Group01::AM_AbsoluteX:
                return memory[ReadWord (PC) + X];

            }
        */
    default:
        std::printf ("Unhandled addressing mode %d\n", microcode.instruction.asBits.addressingMode);
        break;
    }

    return;
}



void Cpu::ExecuteInstruction (Microcode microcode, Word operand)
{
    switch (microcode.operation)
    {
    case Microcode::Or:
        CpuOperations::Or (microcode.pRegisterAffected, (Byte) operand);
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
        Byte * pRegisterAffected;
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
                                    Byte * pRegisterAffected)
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



