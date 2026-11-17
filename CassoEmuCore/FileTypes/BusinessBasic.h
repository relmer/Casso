#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic
//
//  An Apple /// Business BASIC program, ProDOS type $09: the program's
//  length as a word, then lines, each its length in a byte, the line number
//  as a word, its bytes and a $00, to a length of 0. A byte past $7F is a
//  keyword, and $FF is followed by a byte that picks one from a second
//  table; the token tables are CiderPress II's, under its Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class BusinessBasic
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static std::wstring  ListLine (std::span<const Byte> content);

private:
    static constexpr Byte    kFileType  = 0x09;
    static constexpr size_t  kMaxBytes  = 48 * 1024;
    static constexpr Byte    kExtended  = 0xFF;
    static constexpr Byte    kRem       = 0xC0;

    static const char *  GetKeyword  (Byte token);
    static const char *  GetExtended (Byte token);
    static void          AppendWord  (std::string & ioLine, const char * keyword);
};
