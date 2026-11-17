#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergDocument
//
//  A document on a Gutenberg or Gutenberg, Jr. disk: a file whose catalog
//  type is a document or a locked document. The text ends at the first $00.
//  A character of the standard set has its high bit set, and $8D ends a
//  line. The other bytes from $80 to $9F are formatting commands and are
//  removed. A byte with its high bit clear is a character of the alternate
//  set, whose glyph depends on the font the document was written for, so it
//  is removed and a note is added.
//
//  The character ranges are from CiderPress II's Gutenberg notes, under its
//  Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class GutenbergDocument
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte  kEnd          = 0x00;
    static constexpr Byte  kReturn       = 0x8D;
    static constexpr Byte  kHighBit      = 0x80;
    static constexpr Byte  kFirstPrinted = 0xA0;
    static constexpr Byte  kRubout       = 0xFF;
};
