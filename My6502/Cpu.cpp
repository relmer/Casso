#include "Pch.h"

#include "Cpu.h"
#include "Utils.h"



Cpu::Cpu ()
{
    InitializeInstructionSet ();
}



void Cpu::Reset ()
{
    PC = 0xFFFC;
    SP = 0x0100;

    status.status = 0;

    A = 0;
    X = 0;
    Y = 0;

    memset (memory, 0, sizeof (memory));
}



void Cpu::Run ()
{
    do
    {
        Byte opcode         = memory[PC];
        //Byte instruction    = Decode (opcode); 
    }
    while (0);
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
    Byte addressingMode            = Group01::__AM_First;
    Byte currentAddressingModeFlag = 1;

    while (addressingMode < Group01::__AM_Count)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction instruction = Group01::CreateInstruction (opcode, (Group01::AddressingMode) addressingMode);
            instructionSet[instruction.asByte] = Microcode (instruction, operation, pRegisterAffected);
        }

        ++addressingMode;
        currentAddressingModeFlag <<= 1;
    }
}



void Cpu::PrintInstructionSet ()
{
    for (size_t i = 0; i < ARRAYSIZE(instructionSet); i++)
    {
        if (instructionSet[i].isLegal)
        {
            Byte opcode = instructionSet[i].instruction.asBits.opcode;
            Byte addressingMode = instructionSet[i].instruction.asBits.addressingMode;

            std::printf ("Instruction %02X:  %s ($%02X) %s\n",
                         (unsigned int) i,
                         Group01::opcodeName[opcode],
                         instructionSet[i].instruction.asByte,
                         Group01::addressingModeName[addressingMode]);
        }
    }
}



Word Cpu::Decode (Byte opcode)
{
    return Word ();
}



Byte Cpu::FetchInstruction ()
{
    return Byte ();
}

