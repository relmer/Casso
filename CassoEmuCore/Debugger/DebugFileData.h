#pragma once

#include "Pch.h"

#include "Debugger/DataBlockTable.h"
#include "Debugger/DebugFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileData
//
//  The bytes a debug file maps to data directives, so the disassembly reads
//  them as data rather than as instructions. A line counts when its source
//  file was read, it isn't a macro's, and its opcode is a directive rather
//  than a mnemonic or a macro's invocation: each of its spans is a block.
//
////////////////////////////////////////////////////////////////////////////////

class DebugFileData
{
public:
    //  The blocks, in the order of the file's lines. The sources are each
    //  file's lines by the file's id.
    static std::vector<DataBlockEntry>  GetBlocks (const DebugFile & file, const std::map<int, std::vector<std::string>> & sources);

    //  The kind of data a source line's directive lays down: text for a
    //  string directive or a byte directive with a string, words for a word
    //  directive, addresses for an address directive, and bytes for any
    //  other. False for a line whose opcode isn't a directive.
    static bool  TryGetKind (const std::string & line, DataBlockKind & kind);

private:
    static std::string  GetDirective (const std::wstring & line, bool & hasString);
};
