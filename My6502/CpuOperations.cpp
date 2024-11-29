#include "Pch.h"

#include "CpuOperations.h"
#include "Cpu.h"



void CpuOperations::Or (Cpu & cpu, Byte operand)
{
    cpu.A |= operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = cpu.A & 0x80;
}



void CpuOperations::And (Cpu & cpu, Byte operand)
{
    cpu.A &= operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = cpu.A & 0x80;
}



void CpuOperations::Xor (Cpu & cpu, Byte operand)
{
    cpu.A ^= operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = cpu.A & 0x80;
}



void CpuOperations::AddWithCarry (Cpu & cpu, Byte operand)
{
    Word sum = cpu.A + operand + cpu.status.flags.c;

    cpu.status.flags.v = 
            !((cpu.A & 0x80) ^ (operand & 0x80))    // Both have the same sign
            && ((operand & 0x80) != (sum & 0x80));  // But that sign is not the same as the sum

    cpu.A = (Byte) sum;

    cpu.status.flags.c = sum > 0xFF;
    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = cpu.A & 0x80;
}



void CpuOperations::Store (Cpu & cpu, Byte & registerAffected, Word effectiveAddress)
{
    cpu.memory[effectiveAddress] = registerAffected;
}



void CpuOperations::Load (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected = operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = cpu.A & 0x80;
}



void CpuOperations::Compare (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    Word cmp = registerAffected - operand;

    cpu.status.flags.c = cmp < 0x80;  // NB:  C functions as C' during subtraction
    cpu.status.flags.z = cmp == 0;
    cpu.status.flags.n = cmp & 0x80;
}



void CpuOperations::SubtractWithCarry (Cpu & cpu, Byte operand)
{
    Word difference = cpu.A - operand - !cpu.status.flags.c;

    cpu.status.flags.v =
        !((cpu.A & 0x80) ^ (operand & 0x80))           // Both have the same sign
        && ((operand & 0x80) != (difference & 0x80));  // But that sign is not the same as the difference

    cpu.A = (Byte) difference;

    cpu.status.flags.c = !(difference & 0x8000);  // set to 0 if negative to indicate a borrow
    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = cpu.A & 0x80;

}
