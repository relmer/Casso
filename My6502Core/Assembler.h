#pragma once

#include "AssemblerTypes.h"
#include "OpcodeTable.h"

class Microcode;


////////////////////////////////////////////////////////////////////////////////

//
//  Assembler
//
////////////////////////////////////////////////////////////////////////////////

class Assembler
{
public:
    Assembler (const Microcode instructionSet[256], AssemblerOptions options = {});

    AssemblyResult Assemble (const std::string & sourceText);

    static std::string FormatListingLine (const AssemblyLine & line);

private:
    void RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message);

    OpcodeTable      m_opcodeTable;
    AssemblerOptions m_options;
};
