#pragma once

#include "Debugger/DebugFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePosition
//
//  One source line an address belongs to, and how deep inside macros it is:
//  an assembler line is depth 0, a macro body line one level in is 1.
//
////////////////////////////////////////////////////////////////////////////////

struct SourcePosition
{
    int            file  = 0;
    int            line  = 0;
    DebugLineType  type  = DebugLineType::Asm;
    int            depth = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LineTable
//
//  A debug file's lines, indexed both ways: every address to the source lines
//  that produced it, and every source line to the addresses it produced.
//
//  AN ADDRESS LOOKUP IS AN INDEX, NOT A SEARCH. A source-level step asks for
//  the line of every instruction it runs, so the table keeps a slot for each of
//  the 64K addresses.
//
//  An address inside a macro expansion has more than one line: the invocation
//  and each body line down to the innermost, outermost first.
//
////////////////////////////////////////////////////////////////////////////////

class LineTable
{
public:
    void  Build (const DebugFile & file);
    void  Clear ();
    bool  IsEmpty () const { return m_byLine.empty(); }

    const std::vector<SourcePosition> &  GetPositionsAt (Word address) const;

    //  The address ranges, inclusive, that a file's line produced.
    std::vector<std::pair<Word, Word>>  GetRanges (int file, int line) const;

    //  The first line at or after `line` in the file that produced code, for a
    //  breakpoint set on a comment or a directive.
    std::optional<int>  GetNextLineWithCode (int file, int line) const;

private:
    static constexpr size_t  kAddressSpace = 0x10000;

    std::vector<std::vector<SourcePosition>>                  m_byAddress;
    std::map<std::pair<int, int>, std::vector<std::pair<Word, Word>>>  m_byLine;
};
