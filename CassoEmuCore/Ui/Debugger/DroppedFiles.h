#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles
//
//  What the debugger does with a file dropped on it or opened from the File
//  menu, decided from its extension and, when that says nothing, its content.
//  No window and no file system: the window reads the file through its host
//  and acts on the answer.
//
////////////////////////////////////////////////////////////////////////////////

class DroppedFiles
{
public:
    //  By extension: a debug or symbol file SYM LOAD reads, an assembler
    //  source, or neither.
    enum class Kind
    {
        Symbols,
        Source,
        Other,
    };

    //  How a file of neither kind opens: loaded as symbols when it reads as a
    //  symbol file, otherwise shown as text, or as a hex dump when binary.
    enum class Opening
    {
        Symbols,
        Text,
        Hex,
    };

    static Kind         GetKind    (const std::wstring & path);
    static Opening      GetOpening (const std::string & content);
    static bool         IsBinary   (const std::string & content);

    //  Sixteen bytes a line: the offset, the bytes in hex, and the printable
    //  ones as characters.
    static std::string  FormatHexDump (const std::string & content);

    //  The debug and symbol files a source's own assembly would have written
    //  beside it, in the order to try them.
    static std::vector<std::wstring>  GetSymbolFilesBeside (const std::wstring & sourcePath);

private:
    static constexpr size_t  kHexBytesPerLine = 16;
    static constexpr size_t  kBinarySample    = 4096;

    static bool  HasExtension (const std::wstring & path, std::initializer_list<const wchar_t *> extensions);
};
