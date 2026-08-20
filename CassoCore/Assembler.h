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

    //  One listing row, wrapped to a column width: the row as FormatListingLine
    //  renders it, then as many continuation rows as its source text needs.
    //
    //  Continuations are indented to the SOURCE column, so wrapped text lines up
    //  under the text it came from rather than under the address and bytes. The
    //  fixed columns are never wrapped into -- a listing is read positionally,
    //  and a wrap that shifted them would break every reader of it.
    //
    //  A width of 0 or less means no wrapping, which is how a caller says "leave
    //  it alone" without the caller having to know a sentinel.
    static std::vector<std::string> FormatListingRows (const AssemblyLine & line,
                                                       bool showCycleCounts,
                                                       int  columnWidth);
    static std::string FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                          const std::unordered_map<std::string, SymbolKind> & symbolKinds);
    static std::string FormatDebugInfo   (const std::unordered_map<std::string, Word> & symbols);

private:
    void RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message);

    InstructionSetProvider m_instructionSets;
    AssemblerOptions       m_options;
};
