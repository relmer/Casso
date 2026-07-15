#include "Pch.h"

#include "Cpu65C02Table.h"

#include "Cpu65C02.h"
#include "MemoryBus.h"




////////////////////////////////////////////////////////////////////////////////
//
//  GetCpu65C02InstructionSet
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * GetCpu65C02InstructionSet()
{
    // Build the authoritative CMOS table once, straight from a real Cpu65C02, so
    // the assembler and the emulator can never drift apart. The bus and CPU are
    // kept alive for the process lifetime: the table's per-entry register pointers
    // alias this CPU's registers, and while the assembler never dereferences them,
    // keeping them valid avoids returning a table full of dangling pointers.
    static MemoryBus bus;
    static Cpu65C02  cpu (bus);

    return cpu.GetInstructionSet();
}
