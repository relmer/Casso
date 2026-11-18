#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic
//
//  An Apple /// Business BASIC program, ProDOS type $09, as a listing:
//  lines of an offset to the next, a line number and the statements to a
//  $00, to a line whose offset is 0. A byte of $80 and above is a keyword,
//  and $FF is followed by a byte that picks one from a second table.
//  docs/file-formats/business-basic.md is the format.
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
    static bool          IsWordEnd   (char c);
};
