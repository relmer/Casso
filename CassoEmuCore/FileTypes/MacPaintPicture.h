#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MacPaintPicture
//
//  A MacPaint document, Finder type PNTG: a 512-byte header, its first four
//  bytes a version of 0 or 2, then 720 rows of 576 dots, 72 bytes a row,
//  packed with PackBits. A PackBits flag byte of 0 to 127 is that many
//  literal bytes plus one; -1 to -127 is the next byte repeated one more
//  time than the flag's magnitude; -128 is skipped. A set bit is black, and
//  a byte's high bit is its leftmost dot. A file in a MacBinary wrapper
//  begins with a 128-byte header whose Finder type is PNTG.
//
//  A file with no Finder type is a MacPaint document by its name, ending
//  .PNTG or .MAC, or by its MacBinary header, and then only when its rows
//  unpack whole.
//
////////////////////////////////////////////////////////////////////////////////

class MacPaintPicture
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static constexpr int     kWidth       = 576;
    static constexpr int     kHeight      = 720;
    static constexpr size_t  kHeaderBytes = 512;

private:
    static constexpr size_t  kRowBytes           = kWidth / 8;
    static constexpr size_t  kMacBinaryBytes     = 128;
    static constexpr size_t  kMacBinaryTypeAt    = 65;
    static constexpr size_t  kMacBinaryLengthAt  = 83;

    static bool  IsMacBinary (std::span<const Byte> bytes);
    static bool  GetDocument (std::span<const Byte> bytes, std::span<const Byte> & outDocument);
    static bool  Unpack      (std::span<const Byte> document, std::vector<Byte> & outDots);
};
