#include "Pch.h"

#include "CpuOperations.h"
#include "Cpu.h"



void CpuOperations::Or (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected |= operand;

    cpu.status.flags.z = registerAffected == 0;
    cpu.status.flags.n = registerAffected & 0x80;
}



void CpuOperations::And (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected &= operand;

    cpu.status.flags.z = registerAffected == 0;
    cpu.status.flags.n = registerAffected & 0x80;
}



void CpuOperations::Xor (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected ^= operand;

    cpu.status.flags.z = registerAffected == 0;
    cpu.status.flags.n = registerAffected & 0x80;
}



void CpuOperations::AddWithCarry (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    Word sum = registerAffected + operand + cpu.status.flags.c;

    cpu.status.flags.v = 
            !((registerAffected & 0x80) ^ (operand & 0x80))   // Both have the same sign
            && ((operand & 0x80) != (sum & 0x80));            // But that sign is not the same as the sum

    registerAffected = (Byte) sum;

    cpu.status.flags.c = sum > 0xFF;
    cpu.status.flags.z = registerAffected == 0;
    cpu.status.flags.n = registerAffected & 0x80;
}



