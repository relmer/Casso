#pragma once

#include "Pch.h"

#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder
//
//  Fills a 6502's table of 256 opcodes, the documented ones and the
//  undocumented ones a real NMOS part runs, each with its addressing mode,
//  its operation, the registers it reads and writes, and its base cycle
//  count. Registers has the address of each of the CPU's registers, which
//  the operations act on.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionSetBuilder
{
public:
    struct Registers
    {
        Byte  * a      = nullptr;
        Byte  * x      = nullptr;
        Byte  * y      = nullptr;
        Byte  * sp     = nullptr;
        Byte  * status = nullptr;
    };

    InstructionSetBuilder (std::vector<Microcode> & instructionSet, const Registers & registers);

    void  Build ();

private:
    void  InitializeGroup00      ();
    void  InitializeGroup01      ();
    void  InitializeGroup10      ();
    void  InitializeMisc         ();
    void  InitializeUndocumented ();

    void  CreateInstruction (uint32_t                        addressingModeMax,
                             const char              * const instructionName[],
                             Byte                            opcode,
                             Byte                            addressingModeFlags,
                             Byte                            group,
                             Microcode::Operation            operation,
                             Byte                    *       pSourceRegister,
                             Byte                    *       pDestinationRegister);

    std::vector<Microcode>  & m_instructionSet;
    Registers                 m_registers;
};
