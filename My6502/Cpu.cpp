#include "Pch.h"

#include "Cpu.h"
#include "Utils.h"



Cpu::Cpu ()
{
    InitializeInstructionSet ();
}



void Cpu::Reset ()
{
    pc = 0xFFFC;
    sp = 0x0100;

    status.status = 0;

    a = 0;
    x = 0;
    y = 0;

    memset (memory, 0, sizeof (memory));
}



void Cpu::Run ()
{
    do
    {
        byte opcode         = memory[pc];
        //byte instruction    = Decode (opcode); 
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
    Group01 group01;
    Instruction instruction;


    instruction = group01.CreateInstruction (Group01::ORA, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ORA, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::AND, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::AND, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::EOR, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::EOR, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::ADC, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::ADC, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::STA, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::STA, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::STA, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::STA, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::STA, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::STA, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::STA, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::LDA, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::LDA, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::CMP, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::CMP, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);

    instruction = group01.CreateInstruction (Group01::SBC, Group01::Immediate);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::ZeroPage);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::ZeroPageX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::Absolute);          instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::AbsoluteX);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::AbsoluteY);         instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::ZeroPageXIndirect); instructionSet[instruction.asByte] = Microcode (instruction);
    instruction = group01.CreateInstruction (Group01::SBC, Group01::ZeroPageIndirectY); instructionSet[instruction.asByte] = Microcode (instruction);
}



void Cpu::PrintInstructionSet ()
{
    for (size_t i = 0; i < ARRAYSIZE(instructionSet); i++)
    {
        if (instructionSet[i].isLegal)
        {
            byte opcode = instructionSet[i].instruction.asBits.opcode;
            byte addressingMode = instructionSet[i].instruction.asBits.addressingMode;

            std::printf ("Instruction %02X:  %s ($%02X) %s\n",
                         (unsigned int) i,
                         Group01::opcodeName[opcode],
                         instructionSet[i].instruction.asByte,
                         Group01::addressingModeName[addressingMode]);
        }
    }
}



word Cpu::Decode (byte opcode)
{
    return word ();
}



byte Cpu::FetchInstruction ()
{
    return byte ();
}

