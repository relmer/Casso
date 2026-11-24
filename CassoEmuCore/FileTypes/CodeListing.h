#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeListing
//
//  A code file's disassembly as text, for a copy whose options set a
//  processor: from the address the file is run from, with known addresses
//  written as symbols, as in the preview's disassembly. FileFormatTable
//  never identifies a file as this format; CopyOutConversion::Find returns
//  it for a code file when the options set a processor.
//
////////////////////////////////////////////////////////////////////////////////

class CodeListing
{
public:
    static HRESULT  Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte  kTypeRelocatable = 0xFE;
};
