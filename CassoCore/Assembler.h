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

    static std::string FormatListingLine (const AssemblyLine & line, bool showCycleCounts = false);
    static std::string FormatListing     (const AssemblyResult & result,
                                          int pageHeight = 0,
                                          bool showCycleCounts = false);
    static std::string FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                          const std::unordered_map<std::string, SymbolKind> & symbolKinds);
    static std::string FormatDebugInfo   (const std::unordered_map<std::string, Word> & symbols);

private:
    void RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message);

    OpcodeTable      m_opcodeTable;
    AssemblerOptions m_options;
};
