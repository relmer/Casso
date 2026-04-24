#pragma once

#include "Cpu.h"
#include "CpuOperations.h"



// TestCpu exposes Cpu's protected members for unit testing.
// No changes to production code required.
class TestCpu : public Cpu
{
public:
    // Register access
    Byte & RegA  () { return A; }
    Byte & RegX  () { return X; }
    Byte & RegY  () { return Y; }
    Byte & RegSP () { return SP; }
    Word & RegPC () { return PC; }

    CpuStatus & Status () { return status; }

    // Memory access
    void Poke     (Word address, Byte value) { memory[address] = value; }
    Byte Peek     (Word address) const       { return memory[address]; }
    void PokeWord (Word address, Word value)
    {
        memory[address]     = value & 0xFF;
        memory[address + 1] = value >> 8;
    }

    // Write a sequence of bytes starting at startAddress; returns next free address
    Word WriteBytes (Word startAddress, std::initializer_list<Byte> bytes)
    {
        Word addr = startAddress;

        for (Byte b : bytes)
        {
            memory[addr++] = b;
        }

        return addr;
    }

    // Initialize CPU for a test: clean state, no hardcoded test code
    void InitForTest (Word startPC = 0x8000)
    {
        status.status          = 0;
        status.flags.alwaysOne = 1;

        A  = 0;
        X  = 0;
        Y  = 0;
        SP = 0xFF;
        PC = startPC;

        memset (memory, 0, sizeof (memory));
    }

    // Execute one instruction at the current PC
    void Step ()
    {
        Byte        opcode      = memory[PC];
        Microcode   microcode   = instructionSet[opcode];
        OperandInfo operandInfo = { 0 };

        FetchOperand (microcode, operandInfo);
        ++PC;
        ExecuteInstruction (microcode, operandInfo);
    }

    // Execute N instructions
    void StepN (int n)
    {
        for (int i = 0; i < n; i++)
        {
            Step ();
        }
    }

    // Access instruction set for verification
    const Microcode & GetMicrocode (Byte opcode) const { return instructionSet[opcode]; }
};
