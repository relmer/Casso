#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScAssemblerSource
//
//  S-C Assembler source, a DOS 3.3 I file: lines of a length byte, a line
//  number and bytes, ending in $00. $20 to $7F are characters, $80 to $BF
//  that count of spaces less $80, and $C0 a count and a byte to repeat.
//
////////////////////////////////////////////////////////////////////////////////

class ScAssemblerSource
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     Check   (std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte  kDosInteger = 0x01;
    static constexpr Byte  kRepeat     = 0xC0;
};
