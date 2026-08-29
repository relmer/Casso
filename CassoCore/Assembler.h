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

    // The provider already built, for a caller that decided which sets there
    // are somewhere else -- a subcommand, say -- and should not have to unpack
    // its decision into arrays for this constructor to pack again.
    Assembler (const InstructionSetProvider & instructionSets, AssemblerOptions options = {});

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

    //
    //  The whole listing: every line wrapped to the column width, broken into
    //  pages of the given height, under the title the source named.
    //
    //  IT COMPOSES THE TWO RULES RATHER THAN CHOOSING BETWEEN THEM. Wrapping is
    //  about how wide a row may be and pagination about how many rows fit, and
    //  they arrived from different directions -- one from the dialect work, one
    //  from the assembler's command line -- so a caller doing only one of them
    //  silently drops a flag the tool documents. `-w` and `-h` both land here.
    //
    //  PAGINATION COUNTS ROWS, NOT SOURCE LINES, which is the only reading that
    //  fills a page: a wrapped line occupies as many rows as it wrapped to, and
    //  counting it once would overrun the paper by however much wrapping added.
    //
    //  A height of 0 or less means one continuous page, which is the default and
    //  is what a listing to a screen wants.
    //
    static std::string FormatListing (const AssemblyResult & result,
                                      int  pageHeight     = 0,
                                      bool showCycleCounts = false,
                                      int  columnWidth     = 0);
    static std::string FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                          const std::unordered_map<std::string, SymbolKind> & symbolKinds);
    static std::string FormatDebugInfo   (const std::unordered_map<std::string, Word> & symbols);

private:
    void RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message);

    InstructionSetProvider m_instructionSets;
    AssemblerOptions       m_options;
};
