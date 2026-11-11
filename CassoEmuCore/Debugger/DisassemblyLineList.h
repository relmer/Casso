#pragma once

#include "Debugger/DebugMemoryImage.h"
#include "Debugger/ProgramCheck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyLineList
//
//  A disassembly's lines for the whole address space, made in one forward
//  pass through the bytes the CPU reads: each line's address and length,
//  whether it is an instruction or data, and how it is known.
//
//  Inside data (a marked range, or a debug file's data that matches memory)
//  the pass makes the data's lines. At a known instruction start (a debug
//  file instruction whose bytes match memory, a byte with its fetch bit, or
//  a start the caller gives, as the PC) it takes that alignment. Elsewhere it
//  decodes the next instruction and moves on by its length; a decode that
//  would run over a known start is cut short there, as data, so the line
//  list has an instruction at the start. The PC is a line's start even when
//  another known instruction runs over it, as code that jumps into the
//  middle of an instruction does, so the views always have its row.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyLineList
{
public:
    enum class Source : uint8_t
    {
        DebugFile,
        Marked,
        Fetched,
        Decoded,
    };

    struct Line
    {
        Word     address = 0;
        uint8_t  length  = 1;
        bool     isData  = false;
        Source   source  = Source::Decoded;
    };

    //  The bytes a data line starting at an address takes, 0 where the
    //  address isn't in data; and whether data there is marked rather than
    //  the debug file's.
    using DataLineFn = std::function<int (Word address, bool & isMarked)>;

    struct Inputs
    {
        const DebugMemoryImage                * image      = nullptr;
        std::array<uint8_t, 256>                lengths    = {};
        std::vector<ProgramCheck::Entry>        debugLines;
        std::vector<Word>                       starts;      // other known starts, as the PC
        std::optional<Word>                     pc;          // starts a line even inside another known instruction
        DataLineFn                              dataLine;
    };

    void   Build    (const Inputs & inputs);
    void   Clear    ()       { m_lines.clear(); m_version++; }
    bool   IsBuilt  () const { return !m_lines.empty(); }

    const std::vector<Line> &  GetLines   () const { return m_lines; }
    uint64_t                   GetVersion () const { return m_version; }

    //  The index of the line an address is in, or of the line before it
    //  where none starts there; -1 with no lines.
    int    FindLine (Word address) const;

    //  The address `count` lines before or after the line an address is in,
    //  stopping at the first and last lines.
    Word   StepLines (Word address, int count) const;

private:
    std::vector<Line>  m_lines;
    uint64_t           m_version = 0;
};
