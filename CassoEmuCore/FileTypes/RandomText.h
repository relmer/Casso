#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RandomText
//
//  A random-access text file: entries of a fixed length, each fields that
//  end in carriage returns and then NULs to the record's end. ProDOS has
//  the length in the aux type. DOS 3.3 doesn't record it, so it's the
//  spacing the entries start at; a file with one entry is sequential text.
//
////////////////////////////////////////////////////////////////////////////////

class RandomText
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    //  The entry length of a DOS text file, 0 when it has only one entry.
    static size_t   FindRecordLength (std::span<const Byte> bytes);

private:
    static constexpr Byte  kProDosText = 0x04;
    static constexpr Byte  kDosText    = 0x00;
};
