#pragma once

#include "Core/Cpu65C02.h"
#include "Core/MemoryBus.h"




////////////////////////////////////////////////////////////////////////////////
//
//  TestCpu65C02Bus
//
//  Holds the MemoryBus that Cpu65C02 requires at construction. Listed first in
//  TestCpu65C02's base list so it is constructed before the Cpu65C02 base, which
//  makes the reference valid; the bus is never consulted for reads/writes since
//  TestCpu65C02 overrides the memory accessors to bypass it.
//
////////////////////////////////////////////////////////////////////////////////

class TestCpu65C02Bus
{
protected:
    MemoryBus    m_testBus;
};




////////////////////////////////////////////////////////////////////////////////
//
//  TestCpu65C02
//
//  A flat-memory 65C02 for conformance / unit testing. It reuses the real
//  Cpu65C02 -- its CMOS instruction table, shared dispatch, and DispatchVector
//  override -- but re-overrides the four memory accessors back to the base
//  Cpu::memory[] array, so the full 64K (including $C000-$FFFF and the
//  reset/IRQ vectors) behaves as flat RAM with no bus/device wiring. This is the
//  65C02 analogue of TestCpu and mirrors its interface, so the Harte/Dormann
//  harnesses can drive it the same way.
//
////////////////////////////////////////////////////////////////////////////////

class TestCpu65C02 : private TestCpu65C02Bus, public Cpu65C02
{
public:
    TestCpu65C02() : TestCpu65C02Bus(), Cpu65C02 (m_testBus) {}

    // Flat-memory overrides: the whole 64K address space is Cpu::memory[].
    Byte    ReadByte  (Word address) override             { return memory[address]; }
    void    WriteByte (Word address, Byte value) override { memory[address] = value; }
    Word    ReadWord  (Word address) override             { return (Word) (memory[address] | (memory[(Word) (address + 1)] << 8)); }
    void    WriteWord (Word address, Word value) override { memory[address] = value & 0xFF; memory[(Word) (address + 1)] = (Byte) (value >> 8); }

    // Register / status access (mirrors TestCpu).
    Byte &      RegA  () { return A; }
    Byte &      RegX  () { return X; }
    Byte &      RegY  () { return Y; }
    Byte &      RegSP() { return SP; }
    Word &      RegPC() { return PC; }
    CpuStatus & Status() { return status; }

    // Execute one instruction via the real dispatch (no async interrupts).
    void    Step  () { StepOne(); }
    Byte    Cycles() { return GetLastInstructionCycles(); }

    // The 65C02 instruction table, for building a 65C02-aware Assembler.
    const Microcode * GetInstructionSet() const { return instructionSet.data(); }

    // Memory helpers.
    void    Poke     (Word address, Byte value) { memory[address] = value; }
    Byte    Peek     (Word address) const       { return memory[address]; }
    void    PokeWord (Word address, Word value)
    {
        memory[address]              = value & 0xFF;
        memory[(Word) (address + 1)] = (Byte) (value >> 8);
    }

    // Clean state for a test (mirrors TestCpu::InitForTest).
    void    InitForTest (Word startPC = 0x8000)
    {
        status.status          = 0;
        status.flags.alwaysOne = 1;

        A  = 0;
        X  = 0;
        Y  = 0;
        SP = 0xFF;
        PC = startPC;

        std::fill (memory.begin(), memory.end(), Byte (0));
    }
};
