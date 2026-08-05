#pragma once

#include "Cpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuOperations
//
//  The CPU's ALU and control primitives, one static function per Microcode
//  operation.
//
//  A non-instantiable class of statics rather than free functions, so the set
//  is named and can be a friend of Cpu -- these need direct access to
//  registers and flags, and are the CPU's own internals split out for
//  readability rather than an external collaborator.
//
//  The name of each function matches its Microcode::Operation enumerator
//  exactly, so dispatch reads as a direct correspondence and adding an
//  operation is a matching pair rather than a lookup to maintain.
//
//  Operand-taking and address-taking signatures differ deliberately. An
//  operation that only READS its operand takes a value; one that reads and
//  writes back -- the shifts, rotates, and increments -- takes an effective
//  address and a register pointer, since the same operation targets memory or
//  the accumulator depending on the addressing mode. A null register pointer
//  is what selects the memory form.
//
//  The undocumented NMOS combined opcodes are implemented here too, composed
//  from the same primitives, so their flag behavior follows from the parts
//  rather than being re-derived.
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
