#pragma once

#include "Cpu.h"


////////////////////////////////////////////////////////////////////////////////
//
//  CpuOperations
//
////////////////////////////////////////////////////////////////////////////////

class CpuOperations
{
public:
    CpuOperations () = delete;

    static void AddWithCarry         (Cpu & cpu, Byte operand);
    static void And                  (Cpu & cpu, Byte operand);
    static void BitTest              (Cpu & cpu, Byte operand);
    static void Branch               (Cpu & cpu, Instruction instruction, Word operand);
    static void Break                (Cpu & cpu);
    static void Compare              (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void Decrement            (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress);
    static void Increment            (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress);
    static void Load                 (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void Jump                 (Cpu & cpu, Instruction instruction, Word operand);
    static void NoOperation          (Cpu & cpu);
    static void Or                   (Cpu & cpu, Byte operand);
    static void Pull                 (Cpu & cpu, Byte * pDestinationRegister);
    static void Push                 (Cpu & cpu, Byte * pSourceRegister);
    static void ReturnFromInterrupt  (Cpu & cpu);
    static void ReturnFromSubroutine (Cpu & cpu);
    static void RotateLeft           (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void RotateRight          (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void SetFlag              (Cpu & cpu, Instruction instruction);
    static void ShiftLeft            (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void ShiftRight           (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void Store                (Cpu & cpu, Byte & registerAffected, Word effectiveAddress);
    static void SubtractWithCarry    (Cpu & cpu, Byte operand);
    static void Transfer             (Cpu & cpu, Byte * pSourceRegister, Byte * pDestinationRegister);
    static void Xor                  (Cpu & cpu, Byte operand);
};
