#pragma once

#include "AssemblerTypes.h"
#include "OpcodeTable.h"

class Microcode;



class Assembler
{
public:
    Assembler (const Microcode instructionSet[256], AssemblerOptions options = {});

    AssemblyResult Assemble (const std::string & sourceText);

private:
    OpcodeTable      m_opcodeTable;
    AssemblerOptions m_options;
};
