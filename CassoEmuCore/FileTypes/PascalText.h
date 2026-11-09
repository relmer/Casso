#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PascalText
//
//  An Apple Pascal textfile: a 1 KB header the editor uses, then 1 KB pages
//  of lines, each ending in a carriage return, the rest of a page NULs. A
//  line may start with DLE ($10) and a count of spaces plus 32. A line is
//  never split across pages.
//
////////////////////////////////////////////////////////////////////////////////

class PascalText
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     Check   (std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  Import  (std::span<const Byte> hostBytes, VolumeKind volume, FilePayload & outPayload, std::wstring & outError);

    //  Lines of text as a textfile, the header zeroed.
    static HRESULT  Encode  (const std::vector<std::string> & lines, std::vector<Byte> & outBytes, std::wstring & outError);

private:
    static constexpr size_t  kPageBytes   = 1024;
    static constexpr Byte    kDle         = 0x10;
    static constexpr Byte    kIndentBase  = 32;
    static constexpr size_t  kMaxIndent   = 223;
    static constexpr Byte    kPascalText  = 3;
    static constexpr Byte    kProDosText  = 0x03;
};
