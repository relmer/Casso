#include "Pch.h"

#include "CpuOperations.h"
#include "Cpu.h"



void CpuOperations::AddWithCarry (Cpu & cpu, Byte operand)
{
    Word sum = cpu.A + operand + cpu.status.flags.carry;

    cpu.status.flags.overflow =    !((cpu.A & 0x80) ^ (operand & 0x80))    // Both have the same sign
                                && ((operand & 0x80) != (sum & 0x80));  // But that sign is not the same as the sum

    cpu.A = (Byte) sum;

    cpu.status.flags.carry    = sum > 0xFF;
    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}



void CpuOperations::And (Cpu & cpu, Byte operand)
{
    cpu.A &= operand;

    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}



void CpuOperations::BitTest (Cpu & cpu, Byte operand)
{
    Byte test = cpu.A & operand;

    cpu.status.flags.zero     = test == 0;
    cpu.status.flags.overflow = (bool) (operand & 0x40);
    cpu.status.flags.negative = (bool) (operand & 0x80);
}



void CpuOperations::Branch (Cpu & cpu, Instruction instruction, Word operand)
{
    static constexpr Byte s_kflagMask[] = 
    { 
        0x80,   // Negative
        0x40,   // Overflow
        0x01,   // Carry
        0x02    // Zero
    };

    Byte flag = !!(s_kflagMask[instruction.asBranch.flag] & cpu.status.status);

    if (flag == instruction.asBranch.value)
    {
        cpu.PC = operand;
    }
}



void CpuOperations::Break (Cpu & cpu)
{
    cpu.status.flags.brk = 1;

    // BRK is a two-byte instruction though it ignores the second 
    // byte so we need to add one to the return address we push 
    cpu.PushWord (cpu.PC + 1);          
    cpu.PushByte (cpu.status.status);
    
    cpu.status.flags.interruptDisable = 1;
    cpu.PC                            = cpu.ReadWord (cpu.irqVector);
}



void CpuOperations::Compare (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    Word cmp = registerAffected - operand;

    cpu.status.flags.carry    = cmp < 0x100; // NB:  C functions as C' during subtraction (no borrow = carry set)
    cpu.status.flags.zero     = cmp == 0;
    cpu.status.flags.negative = (bool) (cmp & 0x80);
}



void CpuOperations::Decrement (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte * pByte = pRegisterAffected ? pRegisterAffected : &cpu.memory[effectiveAddress];

    (*pByte)--;

    cpu.status.flags.zero     = *pByte == 0;
    cpu.status.flags.negative = (bool) (*pByte & 0x80);
}



void CpuOperations::Increment (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte * pByte = pRegisterAffected ? pRegisterAffected : &cpu.memory[effectiveAddress];

    (*pByte)++;

    cpu.status.flags.zero = *pByte == 0;
    cpu.status.flags.negative = (bool) (*pByte & 0x80);
}



void CpuOperations::Jump (Cpu & cpu, Instruction instruction, Word operand)
{
    cpu.PC = operand;
}



void CpuOperations::Load (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected = operand;

    cpu.status.flags.zero     = registerAffected == 0;
    cpu.status.flags.negative = (bool) (registerAffected & 0x80);
}



void CpuOperations::Or (Cpu & cpu, Byte operand)
{
    cpu.A |= operand;

    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}



void CpuOperations::NoOperation (Cpu & /*cpu*/)
{
    // Intentionally empty: NOP has no observable effect.
}



void CpuOperations::Push (Cpu & cpu, Byte * pSourceRegister)
{
    Byte value = *pSourceRegister;

    // PHP pushes the status register with the Break and AlwaysOne bits set.
    if (pSourceRegister == &cpu.status.status)
    {
        value |= 0x30;
    }

    cpu.PushByte (value);
}



void CpuOperations::Pull (Cpu & cpu, Byte * pDestinationRegister)
{
    Byte value = cpu.PopByte ();

    if (pDestinationRegister == &cpu.status.status)
    {
        // PLP: pull processor status. Break and AlwaysOne bits are not affected
        // in the actual status register (per 6502 hardware behavior).
        Byte preserved = cpu.status.status & 0x30;
        cpu.status.status = (value & ~0x30) | preserved;
    }
    else
    {
        *pDestinationRegister     = value;
        cpu.status.flags.zero     = value == 0;
        cpu.status.flags.negative = (bool) (value & 0x80);
    }
}



void CpuOperations::ReturnFromInterrupt (Cpu & cpu)
{
    Byte pulled      = cpu.PopByte ();
    Byte preserved   = cpu.status.status & 0x30;
    cpu.status.status = (pulled & ~0x30) | preserved;

    cpu.PC = cpu.PopWord ();
}



void CpuOperations::ReturnFromSubroutine (Cpu & cpu)
{
    cpu.PC = cpu.PopWord () + 1;
}



void CpuOperations::SetFlag (Cpu & cpu, Instruction instruction)
{
    switch (instruction.asByte)
    {
    case 0x18: cpu.status.flags.carry            = 0; break;  // CLC
    case 0x38: cpu.status.flags.carry            = 1; break;  // SEC
    case 0x58: cpu.status.flags.interruptDisable = 0; break;  // CLI
    case 0x78: cpu.status.flags.interruptDisable = 1; break;  // SEI
    case 0xB8: cpu.status.flags.overflow         = 0; break;  // CLV
    case 0xD8: cpu.status.flags.decimal          = 0; break;  // CLD
    case 0xF8: cpu.status.flags.decimal          = 1; break;  // SED
    default:                                          break;
    }
}



void CpuOperations::Transfer (Cpu & cpu, Byte * pSourceRegister, Byte * pDestinationRegister)
{
    *pDestinationRegister = *pSourceRegister;

    // TXS (destination is SP) does not affect any flags.
    if (pDestinationRegister != &cpu.SP)
    {
        cpu.status.flags.zero     = *pDestinationRegister == 0;
        cpu.status.flags.negative = (bool) (*pDestinationRegister & 0x80);
    }
}



void CpuOperations::RotateLeft (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte * pByte = pRegisterAffected ? pRegisterAffected : &cpu.memory[effectiveAddress];
    Byte originalValue = *pByte;

    *pByte <<= 1;
    *pByte |= cpu.status.flags.carry;

    cpu.status.flags.carry    = originalValue >> 7;
    cpu.status.flags.zero     = *pByte == 0;
    cpu.status.flags.negative = (bool) (*pByte & 0x80);
}



void CpuOperations::RotateRight (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte * pByte         = pRegisterAffected ? pRegisterAffected : &cpu.memory[effectiveAddress];
    Byte   originalValue = *pByte;

    *pByte >>= 1;
    *pByte  |= cpu.status.flags.carry << 7;

    cpu.status.flags.carry    = originalValue & 1;
    cpu.status.flags.zero     = *pByte == 0;
    cpu.status.flags.negative = (bool) (*pByte & 0x80);
}



void CpuOperations::ShiftLeft (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    // Rotate a 0 into bit 0
    cpu.status.flags.carry = 0;
    RotateLeft (cpu, pRegisterAffected, effectiveAddress);
}



void CpuOperations::ShiftRight (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    // Rotate a 0 into bit 7
    cpu.status.flags.carry = 0;
    RotateRight (cpu, pRegisterAffected, effectiveAddress);
}



void CpuOperations::Store (Cpu & cpu, Byte & registerAffected, Word effectiveAddress)
{
    cpu.memory[effectiveAddress] = registerAffected;
}



void CpuOperations::SubtractWithCarry (Cpu & cpu, Byte operand)
{
    Word difference = cpu.A - operand - !cpu.status.flags.carry;

    cpu.status.flags.overflow =
        !((cpu.A & 0x80) ^ (operand & 0x80))           // Both have the same sign
        && ((operand & 0x80) != (difference & 0x80));  // But that sign is not the same as the difference

    cpu.A = (Byte) difference;

    cpu.status.flags.carry    = !(difference & 0x8000);  // set to 0 if negative to indicate a borrow
    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);

}



void CpuOperations::Xor (Cpu & cpu, Byte operand)
{
    cpu.A ^= operand;

    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}



