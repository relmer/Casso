#pragma once

#include "Cpu.h"
#include "CpuOperations.h"
#include "Assembler.h"



// TestCpu exposes Cpu's protected members for unit testing.
// No changes to production code required.
////////////////////////////////////////////////////////////////////////////////

//
//  TestCpu
//
////////////////////////////////////////////////////////////////////////////////

class TestCpu : public Cpu
{
public:
    enum class StopReason
    {
        ReachedTarget,
        CycleLimit,
        IllegalOpcode,
    };

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

        std::fill (memory.begin (), memory.end (), Byte (0));
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

    // Stack operation wrappers for testing (PushWord/PopWord are protected)
    void DoPushWord (Word value) { PushWord (value); }
    Word DoPopWord  ()           { return PopWord (); }

    // Access instruction set for verification
    const Microcode & GetMicrocode (Byte opcode) const { return instructionSet[opcode]; }

    // Access full instruction set array (for OpcodeTable construction)
    const Microcode * GetInstructionSet () const { return instructionSet.data (); }

    // Assemble source text into CPU memory; returns AssemblyResult
    AssemblyResult Assemble (const char * source, Word startAddress = 0x8000)
    {
        Assembler  asm6502 (instructionSet.data ());
        auto       result = asm6502.Assemble (source);

        if (result.success)
        {
            Word addr = startAddress;

            for (Byte b : result.bytes)
            {
                memory[addr++] = b;
            }

            PC = startAddress;

            // Fixup symbol addresses: the assembler uses 0x8000 as default origin,
            // but we may be loading at a different address
            if (startAddress != 0x8000)
            {
                Word offset = startAddress - 0x8000;

                for (auto & pair : result.symbols)
                {
                    pair.second += offset;
                }

                result.startAddress = startAddress;
                result.endAddress   = startAddress + (Word) result.bytes.size ();
            }
        }

        return result;
    }

    // Look up a label address from an AssemblyResult
    static Word LabelAddress (const AssemblyResult & result, const char * name)
    {
        auto it = result.symbols.find (name);

        if (it != result.symbols.end ())
        {
            return it->second;
        }

        return 0;
    }

    // Execute instructions until PC reaches targetAddress, or stop conditions met
    StopReason RunUntil (Word targetAddress, uint32_t maxCycles = 0)
    {
        uint32_t cycles = 0;

        while (PC != targetAddress)
        {
            if (maxCycles > 0 && cycles >= maxCycles)
            {
                return StopReason::CycleLimit;
            }

            Byte      opcode    = memory[PC];
            Microcode microcode = instructionSet[opcode];

            if (!microcode.isLegal)
            {
                return StopReason::IllegalOpcode;
            }

            Step ();
            cycles++;
        }

        return StopReason::ReachedTarget;
    }
};
