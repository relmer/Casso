#include "Pch.h"

#include "CpuOperations.h"
#include "Cpu.h"



void CpuOperations::AddWithCarry (Cpu & cpu, Byte operand)
{
    Word sum = cpu.A + operand + cpu.status.flags.c;

    cpu.status.flags.v =   !((cpu.A & 0x80) ^ (operand & 0x80))    // Both have the same sign
                         && ((operand & 0x80) != (sum & 0x80));  // But that sign is not the same as the sum

    cpu.A = (Byte) sum;

    cpu.status.flags.c = sum > 0xFF;
    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = (bool) (cpu.A & 0x80);
}



void CpuOperations::And (Cpu & cpu, Byte operand)
{
    cpu.A &= operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = (bool) (cpu.A & 0x80);
}



void CpuOperations::BitTest (Cpu & cpu, Byte operand)
{
    Byte test = cpu.A & operand;

    cpu.status.flags.z = test == 0;
    cpu.status.flags.v = (bool) (test & 0x40);
    cpu.status.flags.n = (bool) (test & 0x80);
}



void CpuOperations::Compare (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    Word cmp = registerAffected - operand;

    cpu.status.flags.c = cmp < 0x80;  // NB:  C functions as C' during subtraction
    cpu.status.flags.z = cmp == 0;
    cpu.status.flags.n = (bool) (cmp & 0x80);
}



void CpuOperations::Decrement (Cpu & cpu, Word effectiveAddress)
{
    cpu.memory[effectiveAddress]--;

    cpu.status.flags.z = cpu.memory[effectiveAddress] == 0;
    cpu.status.flags.n = (bool) (cpu.memory[effectiveAddress] & 0x80);
}



void CpuOperations::Increment (Cpu & cpu, Word effectiveAddress)
{
    cpu.memory[effectiveAddress]++;

    cpu.status.flags.z = cpu.memory[effectiveAddress] == 0;
    cpu.status.flags.n = (bool) (cpu.memory[effectiveAddress] & 0x80);
}



void CpuOperations::Jump (Cpu & cpu, Instruction instruction, Word operand)
{
    cpu.PC = operand;
}



void CpuOperations::Load (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected = operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = (bool) (cpu.A & 0x80);
}



void CpuOperations::Or (Cpu & cpu, Byte operand)
{
    cpu.A |= operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = (bool) (cpu.A & 0x80);
}



void CpuOperations::RotateLeft (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte * pByte = pRegisterAffected ? pRegisterAffected : &cpu.memory[effectiveAddress];
    Byte originalValue = *pByte;

    *pByte <<= 1;
    *pByte |= cpu.status.flags.c;

    cpu.status.flags.c = originalValue >> 7;
    cpu.status.flags.z = *pByte == 0;
    cpu.status.flags.n = (bool) (*pByte & 0x80);
}



void CpuOperations::RotateRight (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte * pByte         = pRegisterAffected ? pRegisterAffected : &cpu.memory[effectiveAddress];
    Byte   originalValue = *pByte;

    *pByte >>= 1;
    *pByte  |= cpu.status.flags.c << 7;

    cpu.status.flags.c = originalValue & 1;
    cpu.status.flags.z = *pByte == 0;
    cpu.status.flags.n = (bool) (*pByte & 0x80);
}



void CpuOperations::ShiftLeft (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    // Rotate a 0 into bit 0
    cpu.status.flags.c = 0;
    RotateLeft (cpu, pRegisterAffected, effectiveAddress);
}



void CpuOperations::ShiftRight (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    // Rotate a 0 into bit 7
    cpu.status.flags.c = 0;
    RotateRight (cpu, pRegisterAffected, effectiveAddress);
}



void CpuOperations::Store (Cpu & cpu, Byte & registerAffected, Word effectiveAddress)
{
    cpu.memory[effectiveAddress] = registerAffected;
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
    cpu.status.flags.n = (bool) (cpu.A & 0x80);

}



void CpuOperations::Xor (Cpu & cpu, Byte operand)
{
    cpu.A ^= operand;

    cpu.status.flags.z = cpu.A == 0;
    cpu.status.flags.n = (bool) (cpu.A & 0x80);
}



