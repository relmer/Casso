#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource
//
//  Merlin assembler source: lines of label, opcode, operand and comment in
//  high ASCII, each column ended by a single $A0, a space inside a comment
//  or quoted operand stored as $20, and each line ended by $8D. A line
//  starting with * is a comment. On screen the columns start at 1, 10, 16
//  and 27.
//
////////////////////////////////////////////////////////////////////////////////

class MerlinSource
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     Check   (std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  Import  (std::span<const Byte> hostBytes, VolumeKind volume, FilePayload & outPayload, std::wstring & outError);

    //  Lines laid out in Merlin's columns back as Merlin's bytes.
    static HRESULT  Encode  (const std::vector<std::string> & lines, std::vector<Byte> & outBytes, std::wstring & outError);

private:
    static constexpr Byte    kColumnBreak  = 0xA0;
    static constexpr Byte    kLineEnd      = 0x8D;
    static constexpr Byte    kProDosText   = 0x04;
    static constexpr Byte    kDosBinary    = 0x04;
    static constexpr size_t  kMaxLine      = 255;
    static constexpr size_t  kColumns[]    = { 0, 9, 15, 26 };

    static std::string  SplitColumns       (const std::string & line, std::vector<std::string> & outColumns);
    static void         PadToColumn        (size_t column, std::string & inOutText);
    static size_t       CountCommentBreaks (const std::vector<std::string> & columns, size_t position);
};
