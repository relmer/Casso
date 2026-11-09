#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TeachDocument
//
//  A Teach file, $50/$5445: the text in the data fork, carriage returns
//  ending its paragraphs, and its styles in the resource fork's
//  rStyleBlock 1, a TextEdit TEFormat as the TextEdit chapter of the Apple
//  IIgs Toolbox Reference sets it out: the rulers, the list of styles, and
//  the runs of text each in one of them.
//
////////////////////////////////////////////////////////////////////////////////

class TeachDocument
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte    kFileType    = 0x50;
    static constexpr Word    kAuxType     = 0x5445;
    static constexpr Byte    kReturn      = 0x0D;
    static constexpr size_t  kStyleBytes  = 12;
    static constexpr size_t  kItemBytes   = 8;

    struct StyleRun
    {
        size_t  length = 0;
        Byte    style  = 0;
    };

    static uint32_t   ReadLong     (std::span<const Byte> bytes, size_t at);
    static Alignment  GetAlignment (Word justification);
    static bool       ReadFormat   (std::span<const Byte> format, Alignment & outAlignment, std::vector<StyleRun> & outRuns);
};
