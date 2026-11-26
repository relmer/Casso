#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HexDump
//
//  A file's bytes as lines of text, 16 bytes a line: the offset of the
//  line's first byte, the bytes in hex, and the bytes as characters in the
//  options' character set. A control character is a period. The offset has
//  four digits for a file of 64 KB or less, and six for a larger one.
//
////////////////////////////////////////////////////////////////////////////////

class HexDump
{
public:
    static constexpr size_t  kBytesPerLine = 16;

    static HRESULT  Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static std::vector<std::wstring>  Format (std::span<const Byte> bytes, Charset charset);

private:
    static bool  IsControl (Byte b, Charset charset);
};
