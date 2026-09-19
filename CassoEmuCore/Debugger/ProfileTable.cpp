#include "Pch.h"

#include "Debugger/ProfileTable.h"

#include "Cpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTable::Record
//
//  A cost smaller than its penalties cannot come from the CPU; the base is
//  held at zero rather than wrapping.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTable::Record (Word pc, Byte opcode, Byte cycles, Byte penalties)
{
    uint64_t  penaltyCycles = 0;
    uint64_t  baseCycles    = 0;



    if (!m_isOn)
    {
        return;
    }

    if (penalties & Cpu::kPenaltyPageCross)
    {
        ++m_penalties.pageCross;
        ++penaltyCycles;
    }

    if (penalties & Cpu::kPenaltyBranchTaken)
    {
        ++m_penalties.branchTaken;
        ++penaltyCycles;
    }

    if (penalties & Cpu::kPenaltyBranchCross)
    {
        ++m_penalties.branchCross;
        ++penaltyCycles;
    }

    baseCycles = cycles > penaltyCycles ? cycles - penaltyCycles : 0;

    ++m_byOpcode[opcode].count;
    m_byOpcode[opcode].cycles += baseCycles;
    m_byAddress[pc]           += cycles;
    m_totalCycles             += cycles;
    ++m_instructions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTable::Reset
//
//  Clears the counters and leaves the table on or off as it was.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTable::Reset()
{
    m_byOpcode.fill (OpcodeCounts());
    m_penalties    = PenaltyCycles();
    m_byAddress.clear();
    m_totalCycles  = 0;
    m_instructions = 0;
}
