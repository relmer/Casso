#pragma once

#include "CpuStatus.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu
//
////////////////////////////////////////////////////////////////////////////////

class Cpu
{
    friend class CpuOperations;

public:
    Cpu ();
    void Reset ();
    void Run ();

    // Public accessors for CLI and external use
    Word                GetPC             () const  { return PC; }
    void                SetPC             (Word pc) { PC = pc; }
    Byte                GetA              () const  { return A; }
    Byte                GetX              () const  { return X; }
    Byte                GetY              () const  { return Y; }
    Byte                GetSP             () const  { return SP; }
    const Microcode &   GetMicrocode      (Byte opcode) const { return instructionSet[opcode]; }
    const Microcode *   GetInstructionSet () const  { return instructionSet.data (); }

    void StepOne ();
    Byte PeekByte  (Word address) const { return memory[address]; }
    void PokeByte  (Word address, Byte value) { memory[address] = value; }
    Word PeekWord  (Word address) const { return memory[address] | (memory[address + 1] << 8); }

    // Load a raw binary file into memory at the specified address.
    // Returns true on success; false if the file cannot be opened or does not
    // fit within the 64 KB address space starting at `address`.
    // On failure, memory contents are left unchanged.
    bool LoadBinary (const std::string & filename, Word address);

    // Stream-based overload. Reads all remaining bytes from `stream` into
    // memory starting at `address`. Used directly by unit tests to avoid
    // touching the filesystem; the filename overload is a thin wrapper.
    bool LoadBinary (std::istream & stream, Word address);

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
    void FetchOperandRelative          (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPage          (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageXIndirect (Cpu::OperandInfo & operandInfo);
    
    void ExecuteInstruction            (Microcode microcode, const OperandInfo & operandInfo);

    // Stack operations
    void PushByte (Byte value);
    void PushWord (Word value);
    Byte PopByte  ();
    Word PopWord  ();

    // Memory operations
    void WriteByte (Word address, Byte value);
    void WriteWord (Word address, Word value);
    Byte ReadByte  (Word address);
    Word ReadWord  (Word address);

    void InitializeInstructionSet ();

    void InitializeGroup00 ();
    void InitializeGroup01 ();
    void InitializeGroup10 ();
    void InitializeMisc ();

    void CreateInstruction (uint32_t                        addressingModeMax, 
                            const char              * const instructionName[], 
                            Byte                            opcode, 
                            Byte                            addressingModeFlags, 
                            Byte                            group, 
                            Microcode::Operation            operation, 
                            Byte                    *       pSourceRegister,
                            Byte                    *       pDestinationRegister);
    


protected:
    static constexpr size_t memSize      = 64 * 1024;
    static constexpr size_t stackAddress = 0x0100;
    static constexpr Word   nmiVector    = 0xFFFA;
    static constexpr Word   resVector    = 0xFFFC;
    static constexpr Word   irqVector    = 0xFFFE;

    // Heap-allocated to keep the 64 KB 6502 address space off the stack
    // (otherwise every function that stack-allocates a Cpu blows past C6262's
    // 16 KB frame-size threshold during code analysis).
    std::vector<Byte>       memory;

    Byte                    SP;
    Word                    PC;
    Byte                    A;
    Byte                    X;
    Byte                    Y;

    CpuStatus               status;

protected:
    // Heap-allocated for the same reason as `memory`: keeps the ~10 KB
    // instruction table off the stack of any function holding a Cpu.
    std::vector<Microcode> instructionSet;
};
