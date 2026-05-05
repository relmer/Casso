#include "Pch.h"

#include "CpuOperations.h"
#include "Cpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AddWithCarry
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::AddWithCarry (Cpu & cpu, Byte operand)
{
    Byte carryIn   = cpu.status.flags.carry;
    Byte originalA = cpu.A;
    Word sum       = cpu.A + operand + carryIn;

    if (cpu.status.flags.decimal)
    {
        // BCD add: adjust low nibble then high nibble (NMOS 6502 algorithm).
        // Z flag is always from the binary result on NMOS 6502.
        cpu.status.flags.zero = (Byte) sum == 0;

        Word lo = (originalA & 0x0F) + (operand & 0x0F) + carryIn;

        if (lo > 0x09)
        {
            lo += 0x06;
        }

        Word hi = (originalA & 0xF0) + (operand & 0xF0) + (lo > 0x0F ? 0x10 : 0x00);

        // N and V flags are from the high nibble intermediate, before BCD correction.
        cpu.status.flags.negative = (bool) (hi & 0x80);
        cpu.status.flags.overflow = ((~(originalA ^ operand)) & (originalA ^ hi) & 0x80) != 0;

        if (hi > 0x90)
        {
            hi += 0x60;
        }

        cpu.A                  = (Byte) ((hi & 0xF0) | (lo & 0x0F));
        cpu.status.flags.carry = hi > 0xFF;
    }
    else
    {
        cpu.A                     = (Byte) sum;
        cpu.status.flags.carry    = sum > 0xFF;
        cpu.status.flags.zero     = cpu.A == 0;
        cpu.status.flags.negative = (bool) (cpu.A & 0x80);
        cpu.status.flags.overflow = ((~(originalA ^ operand)) & (originalA ^ cpu.A) & 0x80) != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  And
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::And (Cpu & cpu, Byte operand)
{
    cpu.A &= operand;

    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BitTest
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::BitTest (Cpu & cpu, Byte operand)
{
    Byte test = cpu.A & operand;

    cpu.status.flags.zero     = test == 0;
    cpu.status.flags.overflow = (bool) (operand & 0x40);
    cpu.status.flags.negative = (bool) (operand & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Branch
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  Break
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  Compare
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Compare (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    Word cmp = registerAffected - operand;

    cpu.status.flags.carry    = cmp < 0x100; // NB:  C functions as C' during subtraction (no borrow = carry set)
    cpu.status.flags.zero     = cmp == 0;
    cpu.status.flags.negative = (bool) (cmp & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decrement
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Decrement (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte value = pRegisterAffected ? *pRegisterAffected : cpu.ReadByte (effectiveAddress);

    value--;

    if (pRegisterAffected)
    {
        *pRegisterAffected = value;
    }
    else
    {
        cpu.WriteByte (effectiveAddress, value);
    }

    cpu.status.flags.zero     = value == 0;
    cpu.status.flags.negative = (bool) (value & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Increment
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Increment (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte value = pRegisterAffected ? *pRegisterAffected : cpu.ReadByte (effectiveAddress);

    value++;

    if (pRegisterAffected)
    {
        *pRegisterAffected = value;
    }
    else
    {
        cpu.WriteByte (effectiveAddress, value);
    }

    cpu.status.flags.zero     = value == 0;
    cpu.status.flags.negative = (bool) (value & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Jump
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Jump (Cpu & cpu, Instruction instruction, Word operand)
{
    cpu.PC = operand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JumpSubroutine
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::JumpSubroutine (Cpu & cpu, Word operand)
{
    // On the real 6502, JSR reads the low operand byte, pushes the return
    // address, then reads the high operand byte. If the stack overlaps the
    // operand, the push can overwrite the high byte before it's read.
    // We simulate this by re-reading the high byte after pushing.
    Word hiByteAddr = cpu.PC - 1;
    Byte lo         = (Byte) operand;

    cpu.PushWord (cpu.PC - 1);

    Byte hi = cpu.ReadByte (hiByteAddr);
    cpu.PC  = lo | ((Word) hi << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Load (Cpu & cpu, Byte & registerAffected, Byte operand)
{
    registerAffected = operand;

    cpu.status.flags.zero     = registerAffected == 0;
    cpu.status.flags.negative = (bool) (registerAffected & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Or
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Or (Cpu & cpu, Byte operand)
{
    cpu.A |= operand;

    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoOperation
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::NoOperation (Cpu & /*cpu*/)
{
    // Intentionally empty: NOP has no observable effect.
}





////////////////////////////////////////////////////////////////////////////////
//
//  Push
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  Pull
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  ReturnFromInterrupt
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::ReturnFromInterrupt (Cpu & cpu)
{
    Byte pulled      = cpu.PopByte ();
    Byte preserved   = cpu.status.status & 0x30;
    cpu.status.status = (pulled & ~0x30) | preserved;

    cpu.PC = cpu.PopWord ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReturnFromSubroutine
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::ReturnFromSubroutine (Cpu & cpu)
{
    cpu.PC = cpu.PopWord () + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFlag
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  Transfer
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  RotateLeft
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::RotateLeft (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte value         = pRegisterAffected ? *pRegisterAffected : cpu.ReadByte (effectiveAddress);
    Byte originalValue = value;

    value <<= 1;
    value  |= cpu.status.flags.carry;

    if (pRegisterAffected)
    {
        *pRegisterAffected = value;
    }
    else
    {
        cpu.WriteByte (effectiveAddress, value);
    }

    cpu.status.flags.carry    = originalValue >> 7;
    cpu.status.flags.zero     = value == 0;
    cpu.status.flags.negative = (bool) (value & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RotateRight
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::RotateRight (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    Byte value         = pRegisterAffected ? *pRegisterAffected : cpu.ReadByte (effectiveAddress);
    Byte originalValue = value;

    value >>= 1;
    value  |= cpu.status.flags.carry << 7;

    if (pRegisterAffected)
    {
        *pRegisterAffected = value;
    }
    else
    {
        cpu.WriteByte (effectiveAddress, value);
    }

    cpu.status.flags.carry    = originalValue & 1;
    cpu.status.flags.zero     = value == 0;
    cpu.status.flags.negative = (bool) (value & 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShiftLeft
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::ShiftLeft (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    // Rotate a 0 into bit 0
    cpu.status.flags.carry = 0;
    RotateLeft (cpu, pRegisterAffected, effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShiftRight
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::ShiftRight (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress)
{
    // Rotate a 0 into bit 7
    cpu.status.flags.carry = 0;
    RotateRight (cpu, pRegisterAffected, effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Store
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Store (Cpu & cpu, Byte & registerAffected, Word effectiveAddress)
{
    cpu.WriteByte (effectiveAddress, registerAffected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubtractWithCarry
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::SubtractWithCarry (Cpu & cpu, Byte operand)
{
    Byte borrowIn   = !cpu.status.flags.carry;
    Word difference = cpu.A - operand - borrowIn;

    // On NMOS 6502, V, N, Z, C are all from the binary subtraction,
    // even in decimal mode. Only the result (A) is BCD-adjusted.
    cpu.status.flags.overflow =
        ((cpu.A ^ difference) & 0x80) &&           // Result sign differs from A
        ((cpu.A ^ operand) & 0x80);                 // A and operand have different signs

    cpu.status.flags.carry    = !(difference & 0x8000);
    cpu.status.flags.zero     = (Byte) difference == 0;
    cpu.status.flags.negative = (bool) (difference & 0x80);

    if (cpu.status.flags.decimal)
    {
        // BCD subtract: adjust low nibble then high nibble (NMOS 6502 algorithm).
        int lo = (int) (cpu.A & 0x0F) - (int) (operand & 0x0F) - (int) borrowIn;

        if (lo < 0)
        {
            lo = ((lo - 0x06) & 0x0F) - 0x10;
        }

        int hi = (int) (cpu.A & 0xF0) - (int) (operand & 0xF0) + lo;

        if (hi < 0)
        {
            hi -= 0x60;
        }

        cpu.A = (Byte) (hi & 0xFF);
    }
    else
    {
        cpu.A = (Byte) difference;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Xor
//
////////////////////////////////////////////////////////////////////////////////

void CpuOperations::Xor (Cpu & cpu, Byte operand)
{
    cpu.A ^= operand;

    cpu.status.flags.zero     = cpu.A == 0;
    cpu.status.flags.negative = (bool) (cpu.A & 0x80);
}
