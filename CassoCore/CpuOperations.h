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
    static void DecrementAndCompare  (Cpu & cpu, Word effectiveAddress);
    static void Increment            (Cpu & cpu, Byte * pRegisterAffected, Word effectiveAddress);
    static void Load                 (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void Jump                 (Cpu & cpu, Instruction instruction, Word operand);
    static void JumpSubroutine       (Cpu & cpu, Word operand);
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

    // NMOS undocumented combined opcodes (DCP is DecrementAndCompare above).
    static void StoreAccumulatorAndX (Cpu & cpu, Word effectiveAddress);
    static void LoadAccumulatorAndX  (Cpu & cpu, Byte operand);
    static void ShiftLeftAndOr       (Cpu & cpu, Word effectiveAddress);
    static void RotateLeftAndAnd     (Cpu & cpu, Word effectiveAddress);
    static void ShiftRightAndXor     (Cpu & cpu, Word effectiveAddress);
    static void RotateRightAndAdd    (Cpu & cpu, Word effectiveAddress);
    static void IncrementAndSubtract (Cpu & cpu, Word effectiveAddress);

    // 65C02 (CMOS) operations.
    static void StoreZero             (Cpu & cpu, Word effectiveAddress);
    static void TestAndSetBits        (Cpu & cpu, Word effectiveAddress);
    static void TestAndResetBits      (Cpu & cpu, Word effectiveAddress);
    static void ResetMemoryBit        (Cpu & cpu, Instruction instruction, Word effectiveAddress);
    static void SetMemoryBit          (Cpu & cpu, Instruction instruction, Word effectiveAddress);
    static void BitBranchReset        (Cpu & cpu, Instruction instruction, Byte value, Word target);
    static void BitBranchSet          (Cpu & cpu, Instruction instruction, Byte value, Word target);
    static void BranchAlways          (Cpu & cpu, Word target);
    static void BitTestImmediate      (Cpu & cpu, Byte operand);
    static void AddWithCarryCmos      (Cpu & cpu, Byte operand);
    static void SubtractWithCarryCmos (Cpu & cpu, Byte operand);
    static void BreakCmos             (Cpu & cpu);
};
