#pragma once

#include "AssemblerTypes.h"
#include "OpcodeTable.h"
#include "InstructionSetProvider.h"





class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  Assembler
//
////////////////////////////////////////////////////////////////////////////////

class Assembler
{
public:
    // One instruction set, with nothing to switch to. Unchanged, so every
    // existing caller behaves exactly as it did.
    Assembler (const Microcode instructionSet[256], AssemblerOptions options = {});

    // Base and extended sets, for a dialect whose source can select the wider
    // one. The extended table is injected because it lives in the emulator
    // library, which this one must not reach into.
    Assembler (const Microcode baseSet[256], const Microcode extendedSet[256], AssemblerOptions options = {});

    AssemblyResult Assemble (const std::string & sourceText);

    static std::string FormatListingLine (const AssemblyLine & line, bool showCycleCounts = false);
    static std::string FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                          const std::unordered_map<std::string, SymbolKind> & symbolKinds);
    static std::string FormatDebugInfo   (const std::unordered_map<std::string, Word> & symbols);

private:
    void RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message);

    InstructionSetProvider m_instructionSets;
    AssemblerOptions       m_options;
};
