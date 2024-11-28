#pragma once

#include "CpuStatus.h"



class Instruction
{
public:
    Instruction () 
    {
        asByte = 0;
    }


    Instruction (Byte opcode, Byte addressingMode, Byte group) 
    {
        asBits.group          = group;   
        asBits.addressingMode = addressingMode; 
        asBits.opcode         = opcode;
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



class Group01
{
protected:
    static constexpr Byte group = 0b01;

public:
    enum Opcode
    {
        ORA,    // 000 = OR
        AND,    // 001 = AND
        EOR,    // 010 = XOR
        ADC,    // 011 = ADD with Carry
        STA,    // 100 = Store
        LDA,    // 101 = Load
        CMP,    // 110 = Compare
        SBC     // 111 = Subtract with Carry
    };

    static constexpr const char * instructionName[] =
    { 
        "ORA", 
        "AND", 
        "EOR", 
        "ADC", 
        "STA", 
        "LDA", 
        "CMP", 
        "SBC" 
    };

    enum AddressingMode
    {
        __AM_First           = 0,
        AM_ZeroPageXIndirect = 0b000,   // 000 (Zero Page, X) -> ($00LL + X)
        AM_ZeroPage          = 0b001,   // 001 Zero Page
        AM_Immediate         = 0b010,   // 010 #Immediate
        AM_Absolute          = 0b011,   // 011 Absolute
        AM_ZeroPageIndirectY = 0b100,   // 100 (Zero Page), Y
        AM_ZeroPageX         = 0b101,   // 101 Zero Page, X
        AM_AbsoluteY         = 0b110,   // 110 Absolute, Y
        AM_AbsoluteX         = 0b111,   // 111 Absolute, X
        __AM_Count
    };

    enum AddressingModeFlag
    {
        AMF_ZeroPageXIndirect   = 0b00000001,
        AMF_ZeroPage            = 0b00000010,
        AMF_Immediate           = 0b00000100,
        AMF_Absolute            = 0b00001000,
        AMF_ZeroPageIndirectY   = 0b00010000,
        AMF_ZeroPageX           = 0b00100000,
        AMF_AbsoluteY           = 0b01000000,
        AMF_AbsoluteX           = 0b10000000,
        __AMF_AllModes          = 0b11111111
    };

    static constexpr const char * addressingModeName[] =
    {
        "(Zero Page, X)",
        "Zero Page",
        "#Immediate",
        "Absolute",
        "(Zero Page), Y",
        "Zero Page, X",
        "Absolute, Y",
        "Absolute, X"
    };

public:
    static Instruction CreateInstruction (Opcode opcode, AddressingMode addressingMode)
    {
        return Instruction (opcode, addressingMode, group);
    }
};



class Microcode
{
public:
    enum Operation
    {
        Or,
        And,
        Xor,
        AddWithCarry,
        Store,
        Load,
        Compare,
        SubtractWithCarry,
    };

public:
    Microcode () : 
        isLegal         (false),
        instructionName ("Illegal instruction")
    {
    }

    Microcode (Instruction instruction, const char * instructionName, bool isSingleByte, Operation operation, Byte * pRegisterAffected) :
        isLegal             (true),
        instruction         (instruction),
        instructionName     (instructionName),
        isSingleByte        (isSingleByte),
        pRegisterAffected    (pRegisterAffected),
        operation           (operation)
    {
    }

public:
    bool          isLegal;
    Instruction   instruction;
    const char  * instructionName;
    bool          isSingleByte;
    Byte        * pRegisterAffected;
    Operation     operation;
};



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

    void PrintSingleStepInfo    (Word initialPC, Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandAndComment (Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandBytes      (Word initialPC, Byte opcode);
    void FetchOperand           (Microcode microcode, OperandInfo & operandInfo);
    void ExecuteInstruction     (Microcode microcode, const OperandInfo & operandInfo);

    void InitializeInstructionSet ();
    void InitializeGroup01 ();
    void CreateGroup01Instruction (Group01::Opcode opcode, Byte addressingModeFlags, Microcode::Operation operation, Byte * pRegisterAffected);
    
    void PrintInstructionSet ();
    


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



