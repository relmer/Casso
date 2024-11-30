#pragma once



class Instruction
{
public:
    Instruction ()
    {
        asByte = 0;
    }


    Instruction (Byte opcode, Byte addressingMode, Byte group)
    {
        asBits.group = group;
        asBits.addressingMode = addressingMode;
        asBits.opcode = opcode;
    }

public:
    union
    {
        Byte asByte;

        // Bitfields must be in reverse order (low to high)
        struct InstructionBits
        {
            Byte group          : 2;
            Byte addressingMode : 3;
            Byte opcode         : 3;
        } asBits;
    };
};
