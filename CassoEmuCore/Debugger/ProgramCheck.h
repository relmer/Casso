#pragma once

#include "Pch.h"

#include "Debugger/DebugFile.h"
#include "Disassembler.h"

class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck
//
//  Whether the program in memory is the one a debug or symbol file was made
//  for. An entry is what one address should contain: an instruction of a
//  mnemonic and size, from a source line, or bytes, from a Merlin listing or
//  from memory at a stop in the program. A reboot with another disk changes
//  nearly every entry; a variable or a patched instruction changes a few, so
//  a file matches while at least a quarter of its entries do.
//
////////////////////////////////////////////////////////////////////////////////

class ProgramCheck
{
public:
    static constexpr size_t  kMaxEntries = 512;
    static constexpr size_t  kLabelBytes = 4;
    static constexpr Word    kRamFirst   = 0x0200;
    static constexpr Word    kRamLast    = 0xBFFF;

    struct Entry
    {
        Word               address = 0;
        std::string        mnemonic;          // upper case; empty for bytes
        size_t             size    = 0;       // the instruction's bytes, with a mnemonic
        std::vector<Byte>  bytes;
        int                file    = -1;      // the source line it is from, for a debug file's
        int                line    = 0;

        bool operator== (const Entry & other) const = default;
    };

    using PeekFn = std::function<bool (Word address, Byte & value)>;

    //  A debug file's entries: each instruction line with one span, its
    //  mnemonic from the source text, and each span with the bytes a listing
    //  has for it. The sources are each file's lines by the file's id; a
    //  file that wasn't read doesn't have entries.
    static std::vector<Entry>  GetDebugFileEntries (const DebugFile & file, const std::map<int, std::vector<std::string>> & sources);

    //  The same entries, every one, unsampled.
    static std::vector<Entry>  GetDebugFileLineEntries (const DebugFile & file, const std::map<int, std::vector<std::string>> & sources);

    //  The source lines, by file and line, whose entries memory doesn't match.
    static std::set<std::pair<int, int>>  GetUnmatchedLines (const std::vector<Entry> & entries, const PeekFn & peek, const Microcode * instructionSet);

    //  The bytes at each label in RAM, past the stack and short of I/O.
    static std::vector<Entry>  GetLabelEntries     (const std::vector<Word> & labels, const PeekFn & peek);

    static bool                IsRamLabel          (Word address) { return address >= kRamFirst && address <= kRamLast; }

    //  A source line's mnemonic in upper case, or empty for a line without one.
    static std::string         GetMnemonic         (const std::string & line);

    //  Whether at least a quarter of the entries match memory; true with none.
    //  Without an instruction set, an entry with a mnemonic matches.
    static bool                IsMatch             (const std::vector<Entry> & entries, const PeekFn & peek, const Microcode * instructionSet);

private:
    static bool                IsEntryMatch        (const Entry & entry, const PeekFn & peek, const Disassembler * disassembler);
    static std::vector<Entry>  Sample              (std::vector<Entry> entries);
    static std::string         ToUpper             (std::string text);
};
