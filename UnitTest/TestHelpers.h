#pragma once

#include "Cpu6502.h"
#include "CpuOperations.h"
#include "Assembler.h"





// TestCpu exposes Cpu6502's protected members for unit testing.
// No changes to production code required.





////////////////////////////////////////////////////////////////////////////////
//
//  TestCpu
//
//  A Cpu subclass that exposes what tests need and nothing more.
//
//  It exists because the CPU's instruction table and internals are protected --
//  correctly, since nothing in the product should reach them -- while tests
//  legitimately must. Subclassing keeps that access in ONE place rather than
//  loosening the real class's encapsulation for every test that needs a peek.
//
//  InitForTest builds the instruction set without the rest of a machine, which
//  is what lets assembler tests obtain a valid table with no bus, no devices,
//  and no config.
//
//  It adds no BEHAVIOR. Anything that changed how the CPU executes would make
//  every test that uses it prove something about the double rather than about
//  the product.
//
////////////////////////////////////////////////////////////////////////////////

class TestCpu : public Cpu6502
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
        // By reference, as Cpu::StepOne does. Taking the row by value copied
        // the whole Microcode on every emulated instruction, and because
        // Microcode has a default member initializer the copy dragged
        // __autoclassinit2 in to zero the object first. A CPU profile of the
        // Dormann run put __autoclassinit2 at 5.4% and this operator[] at
        // 4.9% -- about a tenth of the run spent copying a table row that
        // never changes.
        Byte                opcode      = memory[PC];
        const Microcode  &  microcode   = instructionSet[opcode];
        OperandInfo         operandInfo = { 0 };

        FetchOperand (microcode, operandInfo);
        ++PC;
        ExecuteInstruction (microcode, operandInfo);
    }

    // Execute one instruction through the production StepOne dispatch and
    // report what it cost.
    //
    // Step() above is the lean path the bulk of the suite uses and computes no
    // cost at all. The cycle count only exists on StepOne, and the whole value
    // of checking it against the Harte vectors is that it is the number the
    // emulator really bills -- a count recomputed here would restate the test's
    // own assumptions and prove nothing. Mirrors TestCpu65C02.
    Byte StepAndCountCycles ()
    {
        StepOne ();

        return GetLastInstructionCycles ();
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

            // Fixup symbol addresses: the assembler origin may differ from
            // the address we're loading at in memory
            if (startAddress != result.startAddress)
            {
                Word offset = startAddress - result.startAddress;

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
