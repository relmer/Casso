#pragma once

#include "CpuStatus.h"
#include "Microcode.h"



class Cpu
{
    friend class CpuOperations;

public:
    Cpu ();
    void Reset ();
    void Run ();

protected:
    struct OperandInfo
    {
        Word location;
        Word effectiveAddress;
        Word operand;
    };

    void PrintSingleStepInfo           (Word initialPC, Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandAndComment        (Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandBytes             (Word initialPC, Byte opcode);
    void FetchOperand                  (Microcode microcode, OperandInfo & operandInfo);
    void FetchOperandAbsoluteX         (Cpu::OperandInfo & operandInfo);
    void FetchOperandAbsoluteY         (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageX         (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageY         (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageIndirectY (Cpu::OperandInfo & operandInfo);
    void FetchOperandAbsolute          (Cpu::OperandInfo & operandInfo, Microcode & microcode);
    void FetchOperandImmediate         (Cpu::OperandInfo & operandInfo);
    void FetchOperandJumpAbsolute      (Cpu::OperandInfo & operandInfo);
    void FetchOperandJumpIndirect      (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPage          (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageXIndirect (Cpu::OperandInfo & operandInfo);
    
    void ExecuteInstruction            (Microcode microcode, const OperandInfo & operandInfo);

    void InitializeInstructionSet ();

    void InitializeGroup00 ();
    void InitializeGroup01 ();
    void InitializeGroup10 ();

    void CreateInstruction (uint32_t addressingModeMax, const char * const instructionName[], Byte opcode, Byte addressingModeFlags, Byte group, Microcode::Operation operation, Byte * pRegisterAffected);
    
    void PrintInstructionSet (int group);
    


protected:
    static constexpr size_t memSize = 64 * 1024;
    Byte                    memory[memSize];

    Word                    SP;
    Word                    PC;
    Byte                    A;
    Byte                    X;
    Byte                    Y;

    CpuStatus               status;

protected:
    Microcode instructionSet[256];
};



