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
    //  source, a BASIC program BASICLOAD reads (IsBasicProgramFile), or none
    //  of them.
    enum class Kind
    {
        Symbols,
        Source,
        Basic,
        Other,
    };

    //  How a file of neither kind opens: loaded as a BASIC program when it
    //  reads as an Applesoft listing, as symbols when it reads as a symbol
    //  file, otherwise opened as text, or as a hex dump when binary.
    enum class Opening
    {
        Symbols,
        Basic,
        Text,
        Hex,
    };

    static Kind         GetKind          (const std::wstring & path);
    static Opening      GetOpening       (const std::string & content);
    static bool         IsBinary         (const std::string & content);

    //  Lines that each start with a line number, the numbers rising, and at
    //  least one Applesoft statement among them: a listing such as
    //  "10 HOME" and "20 PRINT", which an AppleWin symbol file's "ADDR NAME"
    //  lines otherwise pass for.
    static bool         IsBasicListing   (const std::string & content);

    //  A file the emulator window takes as a BASIC program: a listing whose
    //  name ends in .bas or .bas.txt, an AppleSingle file (.as), or a file
    //  whose name has its ProDOS type as CiderPress writes it, "HELLO#fc0801".
    static bool         IsBasicProgramFile (const std::wstring & path);

    //  The tokenized program such a file has: an AppleSingle or #fc file's
    //  BAS data as it is, an AppleSingle TXT file's or a listing's text
    //  tokenized; false with the reason when there is no program.
    static bool         TryGetBasicProgram (const std::wstring & path, const std::vector<Byte> & bytes, std::vector<Byte> & tokenized, std::string & error);

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
