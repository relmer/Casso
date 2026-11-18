#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MagicWindowDocument
//
//  A Magic Window document: a DOS 3.3 binary file whose name ends .MW, a
//  256-byte header starting $8D with the page header's text from its third
//  byte, then the text, high or low ASCII, a carriage return ending each
//  line. The formatting commands, control characters in the text, are
//  removed.
//
////////////////////////////////////////////////////////////////////////////////

class MagicWindowDocument
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte    kDosBinary   = 0x04;
    static constexpr size_t  kHeaderBytes = 256;
    static constexpr size_t  kMaxBytes    = 49152;
    static constexpr Byte    kReturn      = 0x0D;
};
